/* SPDX-License-Identifier: ISC */
/*
 * dummy_file.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "test.h"

static void dummy_report_error(gcfg_file_t *f, const char *msg, ...)
{
	(void)f; (void)msg;
}

void dummy_file_init(gcfg_file_t *f, const char *line)
{
	f->report_error = dummy_report_error;
	f->buffer = strdup(line);
	assert(f->buffer != NULL);
}

void dummy_file_cleanup(gcfg_file_t *f)
{
	free(f->buffer);
}
