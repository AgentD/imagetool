/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * nullstream.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"

#include <stdlib.h>
#include <stdio.h>

static int null_append(ostream_t *strm, const void *data, size_t size)
{
	null_ostream_t *null = (null_ostream_t *)strm;
	(void)data;

	null->bytes_written += size;
	return 0;
}

static int null_append_sparse(ostream_t *strm, size_t size)
{
	return null_append(strm, NULL, size);
}

static int null_flush(ostream_t *strm)
{
	(void)strm;
	/* stock sound of a toilet flushing */
	return 0;
}

static const char *null_get_filename(ostream_t *strm)
{
	(void)strm;
	return "null sink";
}

static void null_destroy(object_t *obj)
{
	free(obj);
}

null_ostream_t *null_ostream_create(void)
{
	null_ostream_t *null = calloc(1, sizeof(*null));
	ostream_t *ostr = (ostream_t *)null;
	object_t *obj = (object_t *)ostr;

	if (null == NULL) {
		perror("Creating null-stream");
		return NULL;
	}

	ostr->append = null_append;
	ostr->append_sparse = null_append_sparse;
	ostr->flush = null_flush;
	ostr->get_filename = null_get_filename;
	obj->refcount = 1;
	obj->destroy = null_destroy;
	return null;
}
