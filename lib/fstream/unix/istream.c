/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

typedef struct {
	istream_t base;
	char *path;
	int fd;
	bool eof;

	uint8_t buffer[BUFSZ];
} file_istream_t;

static int file_precache(istream_t *strm)
{
	file_istream_t *file = (file_istream_t *)strm;
	ssize_t ret;
	size_t diff;

	while (strm->buffer_used < sizeof(file->buffer)) {
		diff = sizeof(file->buffer) - strm->buffer_used;

		ret = read(file->fd, strm->buffer + strm->buffer_used, diff);

		if (ret == 0) {
			file->eof = true;
			break;
		}

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			perror(file->path);
			return -1;
		}

		strm->buffer_used += ret;
	}

	return 0;
}

static const char *file_get_filename(istream_t *strm)
{
	file_istream_t *file = (file_istream_t *)strm;

	return file->path;
}

static void file_destroy(object_t *obj)
{
	file_istream_t *file = (file_istream_t *)obj;

	if (file->fd != STDIN_FILENO)
		close(file->fd);

	free(file->path);
	free(file);
}

istream_t *istream_open_fd(const char *path, int fd)
{
	file_istream_t *file = calloc(1, sizeof(*file));
	object_t *obj = (object_t *)file;
	istream_t *strm = (istream_t *)file;

	if (file == NULL) {
		perror(path);
		return NULL;
	}

	file->path = strdup(path);
	if (file->path == NULL) {
		perror(path);
		goto fail_free;
	}

	file->fd = fd;
	strm->buffer = file->buffer;
	strm->precache = file_precache;
	strm->get_filename = file_get_filename;
	obj->refcount = 1;
	obj->destroy = file_destroy;
	return strm;
fail_free:
	free(file);
	return NULL;
}

istream_t *istream_open_file(const char *path)
{
	istream_t *out;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		return NULL;
	}

	out = istream_open_fd(path, fd);
	if (out == NULL) {
		close(fd);
		return NULL;
	}

	return out;
}
