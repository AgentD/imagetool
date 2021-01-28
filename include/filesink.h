/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filesink.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FILESINK_H
#define FILESINK_H

#include "predef.h"
#include "filesource.h"
#include "filesystem.h"

typedef struct file_sink_bind_t {
	struct file_sink_bind_t *next;
	filesystem_t *target;
	char prefix[];
} file_sink_bind_t;

typedef struct {
	object_t base;

	file_sink_bind_t *binds;
} file_sink_t;

#ifdef __cplusplus
extern "C" {
#endif

file_sink_t *file_sink_create(void);

int file_sink_bind(file_sink_t *fs, const char *path, filesystem_t *target);

int file_sink_add_data(file_sink_t *dst, file_source_t *src);

#ifdef __cplusplus
}
#endif

#endif /* FILESINK_H */
