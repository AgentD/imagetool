/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * directory.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "filesource.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>


typedef struct dir_level_t {
	struct dir_level_t *next;
	DIR *dirrd;
	char name[];
} dir_level_t;

typedef struct {
	file_source_t base;

	dir_level_t *stack_top;
} file_source_dir_t;

static char *full_path(file_source_dir_t *dir, const char *fname)
{
	size_t sublen, len;
	dir_level_t *it;
	char *out;

	len = strlen(fname);
	for (it = dir->stack_top; it != NULL; it = it->next)
		len += strlen(it->name);

	out = calloc(1, len + 1);
	if (out == NULL)
		return NULL;

	len -= strlen(fname);
	strcpy(out + len, fname);

	for (it = dir->stack_top; it != NULL; it = it->next) {
		sublen = strlen(it->name);

		if (sublen > 0) {
			len -= sublen;
			memcpy(out + len, it->name, sublen);
		}
	}

	return out;
}

static char *readlink_full(file_source_dir_t *dir, const char *name)
{
	char *buffer, *new;
	size_t size = 32;
	ssize_t ret;

	buffer = malloc(size);
	if (buffer == NULL)
		goto fail;

	for (;;) {
		ret = readlinkat(dirfd(dir->stack_top->dirrd), name,
				 buffer, size - 1);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			goto fail;
		}

		if ((size_t)ret >= (size - 1)) {
			new = realloc(buffer, size * 2);
			if (new == NULL)
				goto fail;

			buffer = new;
			size *= 2;
			continue;
		}

		buffer[ret] = '\0';
		break;
	}

	return buffer;
fail:
	perror("reading symlink target");
	free(buffer);
	return NULL;
}

static file_source_record_t *create_entry(file_source_dir_t *dir,
					  const char *name,
					  const struct stat *sb)
{
	file_source_record_t *out;
	char *path;

	path = full_path(dir, name);
	if (path == NULL) {
		perror("reading from directory");
		return NULL;
	}

	out = calloc(1, sizeof(*out));
	if (out == NULL) {
		perror(path);
		free(path);
		return NULL;
	}

	out->permissions = sb->st_mode & (~S_IFMT);
	out->uid = sb->st_uid;
	out->gid = sb->st_gid;
	out->devno = sb->st_rdev;
	out->ctime = sb->st_ctime;
	out->mtime = sb->st_mtime;
	out->full_path = path;

	switch (sb->st_mode & S_IFMT) {
	case S_IFBLK:
		out->type = FILE_SOURCE_BLOCK_DEV;
		out->devno = sb->st_rdev;
		break;
	case S_IFCHR:
		out->type = FILE_SOURCE_CHAR_DEV;
		out->devno = sb->st_rdev;
		break;
	case S_IFREG:
		out->type = FILE_SOURCE_FILE;
		out->size = sb->st_size;
		break;
	case S_IFLNK:
		out->type = FILE_SOURCE_SYMLINK;
		out->link_target = readlink_full(dir, name);
		if (out->link_target == NULL) {
			free(path);
			free(out);
			return NULL;
		}
		break;
	case S_IFDIR:
		out->type = FILE_SOURCE_DIR;
		break;
	case S_IFIFO:
		out->type = FILE_SOURCE_FIFO;
		break;
	case S_IFSOCK:
		out->type = FILE_SOURCE_SOCKET;
		break;
	default:
		fprintf(stderr, "%s: unknown file type.\n", path);
		free(path);
		free(out);
		return NULL;
	}

	return out;
}

static void pop_dir(file_source_dir_t *dir)
{
	dir_level_t *old;

	if (dir->stack_top != NULL) {
		old = dir->stack_top;
		dir->stack_top = old->next;

		closedir(old->dirrd);
		free(old);
	}
}

static int push_dir(file_source_dir_t *dir, const char *name)
{
	dir_level_t *child = NULL;
	DIR *dirrd = NULL;
	int childfd = -1;
	char *fullpath;
	int error;

	childfd = openat(dirfd(dir->stack_top->dirrd), name,
			 O_RDONLY | O_DIRECTORY);
	if (childfd < 0)
		goto fail;

	dirrd = fdopendir(childfd);
	if (dirrd == NULL)
		goto fail;
	childfd = -1;

	child = calloc(1, sizeof(*child) + strlen(name) + 2);
	if (child == NULL)
		goto fail;

	strcpy(child->name, name);
	strcat(child->name, "/");

	child->dirrd = dirrd;
	child->next = dir->stack_top;
	dir->stack_top = child;
	return 0;
fail:
	error = errno;
	fullpath = full_path(dir, name);

	fprintf(stderr, "%s: %s\n", fullpath == NULL ? name : fullpath,
		strerror(error));
	free(fullpath);
	free(child);
	if (dirrd != NULL)
		closedir(dirrd);
	if (childfd >= 0)
		close(childfd);
	return -1;
}

/*****************************************************************************/

static int get_next_record(file_source_t *fs, file_source_record_t **out,
			   istream_t **stream_out)
{
	file_source_dir_t *dir = (file_source_dir_t *)fs;
	struct dirent *ent = NULL;
	char *path = NULL;
	struct stat sb;
	int ret, fd;

	if (out != NULL)
		*out = NULL;

	if (stream_out != NULL)
		*stream_out = NULL;

	do {
		if (dir->stack_top == NULL)
			return 1;

		errno = 0;
		ent = readdir(dir->stack_top->dirrd);

		if (ent == NULL) {
			if (errno != 0)
				goto fail;
			pop_dir(dir);
		} else if (!strcmp(ent->d_name, ".") ||
			   !strcmp(ent->d_name, "..")) {
			ent = NULL;
		}
	} while (ent == NULL);

	ret = fstatat(dirfd(dir->stack_top->dirrd), ent->d_name, &sb,
		      AT_SYMLINK_NOFOLLOW);
	if (ret != 0)
		goto fail;

	if (out != NULL) {
		*out = create_entry(dir, ent->d_name, &sb);
		if ((*out) == NULL)
			return -1;
	}

	if (stream_out != NULL && S_ISREG(sb.st_mode)) {
		fd = openat(dirfd(dir->stack_top->dirrd),
			    ent->d_name, O_RDONLY);
		if (fd < 0)
			goto fail;

		*stream_out = istream_open_fd((*out)->full_path, fd);
		if (*stream_out == NULL) {
			close(fd);
			goto fail_cleanup;
		}
	} else if (S_ISDIR(sb.st_mode)) {
		if (push_dir(dir, ent->d_name))
			goto fail;
	}

	return 0;
fail:
	ret = errno;
	path = full_path(dir, "");

	fprintf(stderr, "%s: %s\n",
		path == NULL ? "reading from directory" : path, strerror(ret));
fail_cleanup:
	if (*out != NULL) {
		free((*out)->full_path);
		free((*out)->link_target);
		free(*out);
	}
	free(path);
	return -1;
}

static void destroy(object_t *obj)
{
	file_source_dir_t *dir = (file_source_dir_t *)obj;

	while (dir->stack_top != NULL)
		pop_dir(dir);

	free(dir);
}

file_source_t *file_source_directory_create(const char *path)
{
	file_source_dir_t *dir;
	file_source_t *fs;
	dir_level_t *l0;
	object_t *obj;

	l0 = calloc(1, sizeof(*l0) + 1);
	if (l0 == NULL) {
		perror(path);
		goto fail_l0;
	}

	l0->name[0] = '\0';
	l0->dirrd = opendir(path);
	if (l0->dirrd == NULL) {
		perror(path);
		goto fail_l0;
	}

	dir = calloc(1, sizeof(*dir));
	fs = (file_source_t *)dir;
	obj = (object_t *)fs;

	if (dir == NULL) {
		perror(path);
		goto fail_l0_close;
	}

	dir->stack_top = l0;
	fs->get_next_record = get_next_record;
	obj->destroy = destroy;
	obj->refcount = 1;
	return fs;
fail_l0_close:
	closedir(l0->dirrd);
fail_l0:
	free(l0);
	return NULL;
}
