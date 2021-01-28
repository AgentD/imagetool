/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "filesource.h"
#include "fstree.h"
#include "tar.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

typedef struct {
	file_source_t base;

	istream_t *tar_stream;
	tar_header_decoded_t hdr;

	uint64_t apparent_file_size;
	uint64_t file_bytes_left;
	bool locked_by_file;
} file_source_tar_t;


typedef struct {
	istream_t base;

	uint8_t buffer[1024];
	sparse_map_t *sparse;
	uint64_t offset;

	file_source_tar_t *tar;
	char filename[];
} tar_istream_t;

/*****************************************************************************/

static int tar_istream_precache(istream_t *strm)
{
	tar_istream_t *ts = (tar_istream_t *)strm;
	uint64_t available;
	sparse_map_t *old;
	int32_t ret;
	size_t diff;

	strm->buffer_offset = 0;
	strm->buffer_used = 0;

	while (strm->buffer_used < sizeof(ts->buffer)) {
		diff = sizeof(ts->buffer) - strm->buffer_used;
		if (diff > ts->tar->apparent_file_size)
			diff = ts->tar->apparent_file_size;
		if (diff == 0) {
			strm->eof = true;
			break;
		}

		if (ts->sparse != NULL) {
			if (ts->sparse->offset > ts->offset) {
				ret = ts->sparse->offset - ts->offset;
				if ((size_t)ret > diff)
					ret = diff;

				memset(ts->buffer + strm->buffer_used, 0, ret);
				ts->tar->apparent_file_size -= ret;
				strm->buffer_used += ret;
				ts->offset += ret;
				continue;
			}

			available = ts->offset - ts->sparse->offset;

			if (ts->sparse->count > available) {
				available = ts->sparse->count - available;
			} else {
				old = ts->sparse;
				ts->sparse = old->next;
				free(old);
				continue;
			}

			if (available < diff)
				diff = available;
		}

		ret = istream_read(ts->tar->tar_stream,
				   ts->buffer + strm->buffer_used, diff);
		if (ret == 0) {
			strm->eof = true;
			break;
		}
		if (ret < 0)
			return -1;

		strm->buffer_used += ret;
		ts->tar->apparent_file_size -= ret;
		ts->tar->file_bytes_left -= ret;
		ts->offset += ret;
	}

	return 0;
}

static const char *tar_istream_get_filename(istream_t *strm)
{
	return ((tar_istream_t *)strm)->filename;
}

static void tar_istream_destroy(object_t *obj)
{
	tar_istream_t *ts = (tar_istream_t *)obj;

	while (ts->sparse != NULL) {
		sparse_map_t *old = ts->sparse;
		ts->sparse = old->next;
		free(old);
	}

	ts->tar->locked_by_file = false;
	object_drop(ts->tar);
	free(ts);
}

static istream_t *tar_istream_create(file_source_tar_t *tar,
				     sparse_map_t *sparse,
				     const char *filename)
{
	tar_istream_t *ts = calloc(1, sizeof(*ts) + strlen(filename) + 1);
	istream_t *strm = (istream_t *)ts;
	object_t *obj = (object_t *)strm;

	if (ts == NULL) {
		fprintf(stderr, "%s: %s: %s\n",
			tar->tar_stream->get_filename(tar->tar_stream),
			filename, strerror(errno));
		return NULL;
	}

	strcpy(ts->filename, filename);

	ts->sparse = sparse;
	ts->tar = object_grab(tar);
	strm->buffer = ts->buffer;
	strm->precache = tar_istream_precache;
	strm->get_filename = tar_istream_get_filename;
	obj->destroy = tar_istream_destroy;
	obj->refcount = 1;
	return strm;
}

/*****************************************************************************/

static int get_next_record(file_source_t *fs, file_source_record_t **out,
			   istream_t **stream_out)
{
	file_source_tar_t *tar = (file_source_tar_t *)fs;
	bool skip;
	int ret;

	*out = NULL;
	if (stream_out != NULL)
		*stream_out = NULL;

	if (tar->locked_by_file) {
		fprintf(stderr, "[BUG] %s: tried to get next tar "
			"record while still reading from file!\n",
			tar->tar_stream->get_filename(tar->tar_stream));
		return -1;
	}

	if (tar->file_bytes_left > 0) {
		if (istream_skip(tar->tar_stream, tar->file_bytes_left))
			return -1;

		tar->file_bytes_left = 0;
	}

retry:
	ret = read_header(tar->tar_stream, &tar->hdr);
	if (ret != 0)
		return ret;

	/* TODO: we need to deal with those eventually */
	if (tar->hdr.xattr != NULL) {
		free_xattr_list(tar->hdr.xattr);
		tar->hdr.xattr = NULL;
	}

	skip = false;

	if (tar->hdr.name != NULL)
		fstree_canonicalize_path(tar->hdr.name);

	if (tar->hdr.name == NULL || tar->hdr.name[0] == '\0' ||
	    tar->hdr.unknown_record) {
		skip = true;
	}

	if (!skip && tar->hdr.sparse != NULL) {
		uint64_t offset = tar->hdr.sparse->offset;
		uint64_t count = 0;
		sparse_map_t *it;

		for (it = tar->hdr.sparse; it != NULL; it = it->next) {
			if (it->offset < offset) {
				skip = true;
				break;
			}
			offset = it->offset + it->count;
			count += it->count;
		}

		if (count != tar->hdr.record_size)
			skip = true;
	}

	if (skip) {
		if (skip_entry(tar->tar_stream, tar->hdr.record_size)) {
			clear_header(&tar->hdr);
			return -1;
		}

		clear_header(&tar->hdr);
		goto retry;
	}

	*out = calloc(1, sizeof(**out));
	if (*out == NULL) {
		perror(tar->hdr.name);
		clear_header(&tar->hdr);
		return -1;
	}

	if (tar->hdr.is_hard_link) {
		(*out)->type = FILE_SOURCE_HARD_LINK;
	} else {
		switch (tar->hdr.sb.st_mode & S_IFMT) {
		case S_IFBLK:  (*out)->type = FILE_SOURCE_BLOCK_DEV; break;
		case S_IFCHR:  (*out)->type = FILE_SOURCE_CHAR_DEV;  break;
		case S_IFREG:  (*out)->type = FILE_SOURCE_FILE;      break;
		case S_IFLNK:  (*out)->type = FILE_SOURCE_SYMLINK;   break;
		case S_IFDIR:  (*out)->type = FILE_SOURCE_DIR;       break;
		case S_IFIFO:  (*out)->type = FILE_SOURCE_FIFO;      break;
		case S_IFSOCK: (*out)->type = FILE_SOURCE_SOCKET;    break;
		default:
			free(*out);
			*out = NULL;
			clear_header(&tar->hdr);
			goto retry;
		}
	}

	(*out)->permissions = tar->hdr.sb.st_mode & (~S_IFMT);
	(*out)->uid = tar->hdr.sb.st_uid;
	(*out)->gid = tar->hdr.sb.st_gid;
	(*out)->devno = tar->hdr.sb.st_rdev;
	(*out)->ctime = tar->hdr.mtime;
	(*out)->mtime = tar->hdr.mtime;
	(*out)->size = tar->hdr.actual_size;
	(*out)->full_path = tar->hdr.name;
	(*out)->link_target = tar->hdr.link_target;

	tar->hdr.name = NULL;
	tar->hdr.link_target = NULL;

	tar->apparent_file_size = tar->hdr.actual_size;
	tar->file_bytes_left = tar->hdr.record_size;
	if (tar->file_bytes_left % 512) {
		tar->file_bytes_left += 512 - (tar->file_bytes_left % 512);
	}

	if (stream_out != NULL && S_ISREG(tar->hdr.sb.st_mode)) {
		tar->locked_by_file = true;

		*stream_out = tar_istream_create(tar, tar->hdr.sparse,
						 (*out)->full_path);
		if (*stream_out == NULL) {
			clear_header(&tar->hdr);
			free((*out)->full_path);
			free((*out)->link_target);
			free((*out));
			(*out) = NULL;
			return -1;
		}

		tar->hdr.sparse = NULL;
	}

	clear_header(&tar->hdr);
	return 0;
}

static void destroy(object_t *obj)
{
	file_source_tar_t *tar = (file_source_tar_t *)obj;

	object_drop(tar->tar_stream);
	free(tar);
}

file_source_t *file_source_tar_create(const char *path)
{
	file_source_tar_t *tar = calloc(1, sizeof(*tar));
	file_source_t *fs = (file_source_t *)tar;
	object_t *obj = (object_t *)fs;

	if (tar == NULL) {
		perror(path);
		return NULL;
	}

	tar->tar_stream = istream_open_file(path);
	if (tar->tar_stream == NULL) {
		free(tar);
		return NULL;
	}

	fs->get_next_record = get_next_record;
	obj->destroy = destroy;
	obj->refcount = 1;
	return fs;
}
