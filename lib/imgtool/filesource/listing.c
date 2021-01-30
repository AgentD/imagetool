/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * listing.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "filesource.h"
#include "fstree.h"

#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

typedef struct pool_t {
	struct pool_t *next;
	uint16_t used;
	uint8_t buffer[1024];
} pool_t;

struct file_source_listing_t {
	file_source_t base;

	size_t offset;
	pool_t *pool_list;

	int dirfd;
};

static int get_file_size(int fd, const char *path, uint64_t *out)
{
	struct stat sb;

	if (fstat(fd, &sb) != 0) {
		perror(path);
		return -1;
	}

	*out = sb.st_size;
	return 0;
}

static pool_t *get_pool(file_source_listing_t *lst, size_t size)
{
	pool_t *p, *new;

	for (p = lst->pool_list; p != NULL; p = p->next) {
		if ((sizeof(p->buffer) - p->used) >= size)
			return p;

		if (p->next == NULL)
			break;
	}

	new = calloc(1, sizeof(*new));
	if (new != NULL) {
		if (p == NULL) {
			lst->pool_list = new;
		} else {
			p->next = new;
		}
	}
	return new;
}

static uint8_t *store_u32(uint8_t *out, uint32_t value, int len)
{
	switch (len) {
	case 3:
		*(out++) = (value >> 24) & 0xFF;
		*(out++) = (value >> 16) & 0xFF;
		/* fall-through */
	case 2:
		*(out++) = (value >> 8) & 0xFF;
		/* fall-through */
	default:
		*(out++) = value & 0xFF;
		break;
	}
	return out;

}

static size_t load_u32(uint8_t *in, uint32_t *value, int len)
{
	*value = 0;

	switch (len) {
	case 3:
		(*value) |= *(in++) << 24;
		(*value) |= *(in++) << 16;
		/* fall-through */
	case 2:
		(*value) |= *(in++) << 8;
		/* fall-through */
	default:
		(*value) |= *(in++);
		break;
	}

	return len == 3 ? 4 : (len == 2 ? 2 : 1);
}

static size_t get_encoded_size(int type, uint32_t uid, uint32_t gid,
			       const char *name, const char *target)
{
	size_t namelen = strlen(name);
	size_t len = 2 + ((namelen > 31) ? 1 : 0) + namelen;

	if (type != FILE_SOURCE_HARD_LINK && type != FILE_SOURCE_SYMLINK)
		len += 1;

	if (type != FILE_SOURCE_HARD_LINK) {
		if (uid <= 0x0F && gid <= 0x0F) {
			len += 1;
		} else {
			len += (uid <= 0xFF ? 1 : (uid <= 0xFFFF ? 2 : 4));
			len += (gid <= 0xFF ? 1 : (gid <= 0xFFFF ? 2 : 4));
		}
	}

	switch (type) {
	case FILE_SOURCE_CHAR_DEV:
	case FILE_SOURCE_BLOCK_DEV:
		len += 4;
		break;
	case FILE_SOURCE_SYMLINK:
	case FILE_SOURCE_HARD_LINK:
		len += 1 + strlen(target);
		break;
	case FILE_SOURCE_FILE:
		if (target != NULL)
			len += strlen(target);
		len += 1;
		break;
	default:
		break;
	}

	return len;
}

static void encode_entry(uint8_t *out, int type, uint32_t uid, uint32_t gid,
			 uint16_t mode, const char *name, const char *target,
			 uint32_t devno)
{
	size_t namelen = strlen(name);
	size_t targetlen = target == NULL ? 0 : strlen(target);
	int ul = uid <= 0x0F ? 0 : (uid <= 0xFF ? 1 : (uid <= 0xFFFF ? 2 : 3));
	int gl = gid <= 0x0F ? 0 : (gid <= 0xFF ? 1 : (gid <= 0xFFFF ? 2 : 3));

	*(out++) = (type & 0x07) | ((namelen <= 31) ? (namelen << 3) : 0);
	*(out++) = (ul << 6) | (gl << 4) | ((mode >> 8) & 0x0F);

	if (type != FILE_SOURCE_HARD_LINK && type != FILE_SOURCE_SYMLINK)
		*(out++) = mode & 0xFF;

	if (type != FILE_SOURCE_HARD_LINK) {
		if (ul == 0 && gl == 0) {
			*(out++) = (uid << 4) | gid;
		} else {
			out = store_u32(out, uid, ul);
			out = store_u32(out, gid, gl);
		}
	}

	if (namelen > 31)
		*(out++) = namelen - 32;

	memcpy(out, name, namelen);
	out += namelen;

	switch (type) {
	case FILE_SOURCE_CHAR_DEV:
	case FILE_SOURCE_BLOCK_DEV:
		store_u32(out, devno, 3);
		break;
	case FILE_SOURCE_SYMLINK:
	case FILE_SOURCE_HARD_LINK:
		*(out++) = targetlen - 1;
		memcpy(out, target, targetlen);
		break;
	case FILE_SOURCE_FILE:
		if (target == NULL) {
			*(out++) = 0;
		} else {
			*(out++) = targetlen;
			memcpy(out, target, targetlen);
		}
		break;
	default:
		break;
	}
}

static int get_next_record(file_source_t *fs, file_source_record_t **out,
			   istream_t **stream_out)
{
	file_source_listing_t *listing = (file_source_listing_t *)fs;
	size_t namelen, targetlen, offset_old, offset = listing->offset;
	uint32_t uid = 0, gid = 0, devno = 0;
	int ul, gl, fd = -1;
	pool_t *p;

	*out = NULL;
	if (stream_out != NULL)
		*stream_out = NULL;

	for (p = listing->pool_list; p != NULL; p = p->next) {
		if (offset < (size_t)p->used)
			break;

		offset -= p->used;
	}

	if (p == NULL)
		return 1;

	offset_old = offset;

	*out = calloc(1, sizeof(**out));
	if (*out == NULL)
		goto fail;

	(*out)->type = p->buffer[offset] & 0x07;
	namelen = (p->buffer[offset] >> 3) & 0x1F;

	ul = (p->buffer[offset + 1] >> 6) & 0x03;
	gl = (p->buffer[offset + 1] >> 4) & 0x03;

	if ((*out)->type == FILE_SOURCE_HARD_LINK) {
		(*out)->permissions = 0777;
		offset += 2;
	} else if ((*out)->type == FILE_SOURCE_SYMLINK) {
		(*out)->permissions = 0777;
		(*out)->permissions |= (p->buffer[offset + 1] & 0x0E) << 8;
		offset += 2;
	} else {
		(*out)->permissions = (p->buffer[offset + 1] & 0x0F) << 8;
		(*out)->permissions |= p->buffer[offset + 2];
		offset += 3;
	}

	if ((*out)->type != FILE_SOURCE_HARD_LINK) {
		if (ul == 0 && gl == 0) {
			uid = (p->buffer[offset] >> 4) & 0x0F;
			gid =  p->buffer[offset]       & 0x0F;
			offset += 1;
		} else {
			offset += load_u32(p->buffer + offset, &uid, ul);
			offset += load_u32(p->buffer + offset, &gid, gl);
		}
	}

	if (namelen > 31)
		namelen += 32 + p->buffer[offset++];

	(*out)->full_path = strndup((const char *)(p->buffer + offset),
				    namelen);
	offset += namelen;

	if ((*out)->full_path == NULL)
		goto fail;

	switch ((*out)->type) {
	case FILE_SOURCE_CHAR_DEV:
	case FILE_SOURCE_BLOCK_DEV:
		offset += load_u32(p->buffer + offset, &devno, 3);
		break;
	case FILE_SOURCE_SYMLINK:
	case FILE_SOURCE_HARD_LINK:
		targetlen = p->buffer[offset++] + 1;
		(*out)->link_target =
			strndup((const char *)(p->buffer + offset),
				targetlen);
		offset += targetlen;
		if ((*out)->link_target == NULL)
			goto fail;
		break;
	case FILE_SOURCE_FILE:
		targetlen = p->buffer[offset++];

		if (targetlen > 0 && stream_out != NULL) {
			(*out)->link_target =
				strndup((const char *)(p->buffer + offset),
					targetlen);
			if ((*out)->link_target == NULL)
				goto fail;

			fd = openat(listing->dirfd, (*out)->link_target,
				    O_RDONLY);
			if (fd < 0) {
				perror((*out)->link_target);
				goto fail_quiet;
			}

			if (get_file_size(fd, (*out)->link_target,
					  &(*out)->size)) {
				goto fail_quiet;
			}

			*stream_out = istream_open_fd((*out)->link_target, fd);
			if (*stream_out == NULL)
				goto fail_quiet;
		} else if (stream_out != NULL) {
			fd = openat(listing->dirfd, (*out)->full_path,
				    O_RDONLY);
			if (fd < 0) {
				perror((*out)->full_path);
				goto fail_quiet;
			}

			if (get_file_size(fd, (*out)->full_path,
					  &(*out)->size)) {
				goto fail_quiet;
			}

			*stream_out = istream_open_fd((*out)->full_path, fd);
			if (*stream_out == NULL)
				goto fail_quiet;
		}

		offset += targetlen;
		break;
	default:
		break;
	}

	(*out)->devno = devno;
	(*out)->uid = uid;
	(*out)->gid = gid;

	listing->offset += offset - offset_old;
	return 0;
fail:
	perror("reading entry from file listing");
fail_quiet:
	if (fd >= 0)
		close(fd);
	if (*out != NULL) {
		free((*out)->full_path);
		free((*out)->link_target);
		free(*out);
		*out = NULL;
	}
	return -1;
}

static void destroy(object_t *obj)
{
	file_source_listing_t *listing = (file_source_listing_t *)obj;

	while (listing->pool_list != NULL) {
		pool_t *pool = listing->pool_list;
		listing->pool_list = pool->next;
		free(pool);
	}

	close(listing->dirfd);
	free(listing);
}

file_source_listing_t *file_source_listing_create(const char *sourcedir)
{
	file_source_listing_t *listing = calloc(1, sizeof(*listing));
	file_source_t *fs = (file_source_t *)listing;
	object_t *obj = (object_t *)fs;

	if (listing == NULL) {
		perror(sourcedir);
		return NULL;
	}

	listing->dirfd = open(sourcedir, O_RDONLY | O_DIRECTORY);
	if (listing->dirfd < 0) {
		perror(sourcedir);
		free(listing);
		return NULL;
	}

	fs->get_next_record = get_next_record;
	obj->destroy = destroy;
	obj->refcount = 1;
	return listing;
}

/*****************************************************************************/

static const struct {
	const char *str;
	int type;
} keywords[] = {
	{ "dir", FILE_SOURCE_DIR },
	{ "file", FILE_SOURCE_FILE },
	{ "pipe", FILE_SOURCE_FIFO },
	{ "sock", FILE_SOURCE_SOCKET },
	{ "nod", FILE_SOURCE_CHAR_DEV },
	{ "slink", FILE_SOURCE_SYMLINK },
	{ "link", FILE_SOURCE_HARD_LINK },
};

static const char *skip_space(const char *line)
{
	if (!isspace(*line))
		return NULL;
	while (isspace(*line))
		++line;
	return line;
}

static const char *read_number(const char *line, uint32_t *out,
			       uint32_t limit, int base)
{
	*out = 0;

	if (!isdigit(*line))
		return NULL;

	while (isdigit(*line)) {
		int x = *(line++) - '0';
		if (x >= base)
			return NULL;

		if ((*out) > (0xFFFFFFFF - x) / base)
			return NULL;

		*out = (*out) * base + x;
		if ((*out) > limit)
			return NULL;
	}

	return line;
}

static void unescape(char *str)
{
	char *src = str, *dst = str;

	if (*(src++) != '"')
		return;

	while (*src != '"' && *src != '\0') {
		if (*src == '\\') {
			++src;
			if (*src == '\\' || *src == '"')
				*(dst++) = *(src++);
		} else {
			*(dst++) = *(src++);
		}
	}

	*dst = '\0';
}

int file_source_listing_add_line(file_source_listing_t *listing,
				 const char *line, gcfg_file_t *file)
{
	uint32_t mode = 0, uid = 0, gid = 0, dev_major = 0, dev_minor = 0;
	size_t i, len, name_len = 0, target_len = 0;
	const char *name = NULL, *target = NULL;
	char *name_str = NULL, *target_str = NULL;
	int type = 0;
	pool_t *pool;

	/* extract the keyword */
	for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
		len = strlen(keywords[i].str);
		if (strncmp(line, keywords[i].str, len) != 0)
			continue;

		if (isspace(line[len])) {
			type = keywords[i].type;
			line += len;
			while (isspace(*line))
				++line;
			break;
		}
	}

	if (i >= (sizeof(keywords) / sizeof(keywords[0])))
		goto fail_kw;

	/* extract the entry filename */
	name = line;

	if (*line == '"') {
		++line;
		while (*line != '"' && *line != '\0') {
			if (line[0] == '\\' && line[1] != '\0') {
				line += 2;
				name_len += 2;
			} else {
				++line;
				++name_len;
			}
		}
		if (*line != '"')
			goto fail_quote;
		++line;
		++name_len;
	} else {
		while (!isspace(*line) && *line != '\0') {
			++line;
			++name_len;
		}
	}

	if (name_len == 0)
		goto fail_name;

	line = skip_space(line);
	if (line == NULL)
		goto fail_mode;

	/* permissions, UID, GID */
	line = read_number(line, &mode, 07777, 8);
	if (line == NULL)
		goto fail_mode;
	line = skip_space(line);
	if (line == NULL)
		goto fail_uidgid;
	line = read_number(line, &uid, 0xFFFFFFFF, 10);
	if (line == NULL)
		goto fail_uidgid;
	line = skip_space(line);
	if (line == NULL)
		goto fail_uidgid;
	line = read_number(line, &gid, 0xFFFFFFFF, 10);
	if (line == NULL)
		goto fail_uidgid;
	line = skip_space(line);

	/* extra */
	switch (type) {
	case FILE_SOURCE_CHAR_DEV:
		if (line == NULL)
			goto fail_devno;
		if (*line == 'b' || *line == 'B') {
			type = FILE_SOURCE_BLOCK_DEV;
		} else if (*line != 'c' && *line != 'C') {
			goto fail_devtype;
		}
		line = skip_space(line + 1);
		if (line == NULL)
			goto fail_devno;
		line = read_number(line, &dev_major, 0xFFFFFFFF, 10);
		if (line == NULL)
			goto fail_devno;
		line = skip_space(line);
		if (line == NULL)
			goto fail_devno;
		line = read_number(line, &dev_minor, 0xFFFFFFFF, 10);
		if (line == NULL)
			goto fail_devno;
		line = skip_space(line);
		break;
	case FILE_SOURCE_FILE:
		if (line == NULL || *line == '\0')
			break;
		/* fall-through */
	case FILE_SOURCE_SYMLINK:
	case FILE_SOURCE_HARD_LINK:
		if (line == NULL || *line == '\0')
			goto fail_target;
		target = line;
		while (!isspace(*line) && *line != '\0') {
			++line;
			++target_len;
		}
		if (target_len == 0)
			goto fail_target;
		line = skip_space(line);
		break;
	default:
		break;
	}

	if (line != NULL && *line != '\0')
		goto fail_extra;

	name_str = strndup(name, name_len);
	if (name_str == NULL)
		goto fail_errno;

	unescape(name_str);
	fstree_canonicalize_path(name_str);

	if (target != NULL) {
		target_str = strndup(target, target_len);
		if (target_str == NULL)
			goto fail_errno;

		if (type == FILE_SOURCE_HARD_LINK ||
		    type == FILE_SOURCE_FILE) {
			fstree_canonicalize_path(target_str);
			if (*target_str == '\0')
				goto fail_target;
		}
	}

	name = name_str;
	target = target_str;
	len = get_encoded_size(type, uid, gid, name, target);

	pool = get_pool(listing, len);
	if (pool == NULL)
		goto fail_errno;

	encode_entry(pool->buffer + pool->used, type, uid, gid, mode, name,
		     target, makedev(dev_major, dev_minor));
	pool->used += len;

	free(name_str);
	free(target_str);
	return 0;
fail_kw:
	file->report_error(file,
			   "expected `dir`, `file`, `pipe`, `sock`, "
			   "`nod`, `slink` or `link`");
	goto fail;
fail_name:
	file->report_error(file, "expected filename after `%s`",
			   keywords[i].str);
	goto fail;
fail_mode:
	file->report_error(file, "expected octal file permissions "
			   "after filename");
	goto fail;
fail_uidgid:
	file->report_error(file, "expected numeric 32 bit user & group ID "
			   "after filename");
	goto fail;
fail_extra:
	file->report_error(file, "unexpected extra arguments for `%s`",
			   keywords[i].str);
	goto fail;
fail_devtype:
	file->report_error(file, "unknown device type `%c`",
			   isprint(*line) ? *line : '\0');
	goto fail;
fail_devno:
	file->report_error(file, "expected major & minor device "
			   "number after device type");
	goto fail;
fail_target:
	file->report_error(file, "missing target path for `%s`",
			   keywords[i].str);
	goto fail;
fail_quote:
	file->report_error(file, "missing '\"'");
	goto fail;
fail_errno:
	file->report_error(file, strerror(errno));
	goto fail;
fail:
	free(name_str);
	free(target_str);
	return -1;
}
