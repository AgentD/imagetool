/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gcfg_file.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "libimgtool.h"
#include "fstream.h"
#include "gcfg.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
	gcfg_file_t base;

	istream_t *istrm;
	size_t line_num;
} gcfg_stream_file_t;

static GCFG_PRINTF_FUN(2, 3) void report_error(gcfg_file_t *base,
					       const char *msg, ...)
{
	gcfg_stream_file_t *file = (gcfg_stream_file_t *)base;
	va_list ap;

	fprintf(stderr, "%s: %zu: ", istream_get_filename(file->istrm),
		file->line_num);

	va_start(ap, msg);
	fprintf(stderr, msg, ap);
	fputc('\n', stderr);
	va_end(ap);
}

static int fetch_line(gcfg_file_t *base)
{
	gcfg_stream_file_t *file = (gcfg_stream_file_t *)base;
	int ret, flags;

	free(base->buffer);
	base->buffer = NULL;

	flags = ISTREAM_LINE_LTRIM | ISTREAM_LINE_RTRIM;
	flags |= ISTREAM_LINE_SKIP_EMPTY;

	ret = istream_get_line(file->istrm, &base->buffer,
			       &file->line_num, flags);
	if (ret != 0)
		return ret;

	file->line_num += 1;
	return 0;
}

static void destroy(object_t *obj)
{
	gcfg_stream_file_t *file = (gcfg_stream_file_t *)obj;

	object_drop(file->istrm);
	free(file);
}

gcfg_file_t *open_gcfg_file(const char *path)
{
	gcfg_stream_file_t *file = calloc(1, sizeof(*file));
	gcfg_file_t *gcfg = (gcfg_file_t *)file;
	object_t *obj = (object_t *)gcfg;

	if (file == NULL) {
		perror(path);
		return NULL;
	}

	file->istrm = istream_open_file(path);
	if (file->istrm == NULL) {
		free(file);
		return NULL;
	}

	gcfg->report_error = report_error;
	gcfg->fetch_line = fetch_line;
	obj->refcount = 1;
	obj->destroy = destroy;
	return gcfg;
}
