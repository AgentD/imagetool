/* SPDX-License-Identifier: ISC */
/*
 * file.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#define BUFFER_SIZE (256)

typedef struct {
	gcfg_file_t base;

	const char *name;
	unsigned long linenum;
	int fd;

	size_t buffer_used;
	size_t line_len;
	bool eof;

	char *namestr;
	char buffer[BUFFER_SIZE];
} gcfg_stdio_file_t;

static GCFG_PRINTF_FUN(2, 3) void report_error(gcfg_file_t *base,
					       const char *msg, ...)
{
	gcfg_stdio_file_t *file = (gcfg_stdio_file_t *)base;
	ssize_t ret;
	va_list ap;

	dprintf(STDERR_FILENO, "%s: %lu: ", file->name, file->linenum);

	va_start(ap, msg);
	vdprintf(STDERR_FILENO, msg, ap);
	va_end(ap);

	do {
		ret = write(STDERR_FILENO, "\n", 1);
	} while (ret < 0 && (errno == EINTR));
}

static int fetch_line(gcfg_file_t *base)
{
	gcfg_stdio_file_t *file = (gcfg_stdio_file_t *)base;
	ssize_t ret;
	size_t diff;

	/* remove current line */
	if (file->line_len > 0) {
		if (file->line_len < file->buffer_used) {
			diff = file->buffer_used - file->line_len;

			memmove(file->buffer,
				file->buffer + file->line_len,
				diff);

			file->buffer_used -= file->line_len;
		} else {
			file->buffer_used = 0;
		}

		file->line_len = 0;
	}

	/* fetch more data */
	while (!file->eof && file->buffer_used < BUFFER_SIZE) {
		diff = BUFFER_SIZE - file->buffer_used;

		ret = read(file->fd, file->buffer + file->buffer_used, diff);

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			report_error(base, "%s", strerror(errno));
			return -1;
		}

		if (ret == 0)
			file->eof = true;

		file->buffer_used += (size_t)ret;
	}

	if (file->buffer_used == 0)
		return 1;

	/* isolate line */
	for (;;) {
		if (file->buffer[file->line_len] == '\n') {
			file->buffer[file->line_len] = '\0';

			if (file->buffer[file->line_len - 1] == '\r')
				file->buffer[file->line_len - 1] = '\0';

			file->line_len += 1;
			break;
		}

		if (file->line_len == file->buffer_used) {
			if (file->buffer_used == BUFFER_SIZE) {
				report_error(base, "line too long");
				return -1;
			}

			file->buffer[file->line_len] = '\0';
			break;
		}

		file->line_len += 1;
	}

	file->linenum += 1;
	return 0;
}

#ifdef GCFG_DISABLE_ALLOC
static gcfg_stdio_file_t static_file;
static bool have_file = false;
#endif

gcfg_file_t *gcfg_file_open(const char *path)
{
	gcfg_stdio_file_t *file;
	gcfg_file_t *base;

#ifdef GCFG_DISABLE_ALLOC
	if (have_file) {
		dprintf(STDERR_FILENO, "%s: too many open gcfg files.\n",
			path);
		return NULL;
	}

	file = &static_file;
	have_file = true;
	memset(file, 0, sizeof(*file));

	file->name = path;
#else
	file = calloc(1, sizeof(*file));
	if (file == NULL)
		goto fail;

	file->namestr = strdup(path);
	if (file->namestr == NULL)
		goto fail;

	file->name = file->namestr;
#endif

	file->fd = open(path, O_RDONLY);
	if (file->fd < 0)
		goto fail;

	base = (gcfg_file_t *)file;
	base->report_error = report_error;
	base->fetch_line = fetch_line;
	base->buffer = file->buffer;
	return base;
fail:
	dprintf(STDERR_FILENO, "%s: %s\n", path, strerror(errno));
#ifndef GCFG_DISABLE_ALLOC
	if (file != NULL) {
		free(file->namestr);
		free(file);
	}
#endif
	return NULL;
}

void gcfg_file_close(gcfg_file_t *base)
{
	gcfg_stdio_file_t *file = (gcfg_stdio_file_t *)base;

	close(file->fd);

#ifdef GCFG_DISABLE_ALLOC
	have_file = false;
#else
	free(file->namestr);
	free(file);
#endif
}
