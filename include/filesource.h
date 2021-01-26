/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filesource.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FILESOURCE_H
#define FILESOURCE_H

#include "predef.h"
#include "fstream.h"

typedef enum {
	FILE_SOURCE_DIR = 0,
	FILE_SOURCE_FILE,
	FILE_SOURCE_FIFO,
	FILE_SOURCE_SOCKET,
	FILE_SOURCE_CHAR_DEV,
	FILE_SOURCE_BLOCK_DEV,
	FILE_SOURCE_SYMLINK,
	FILE_SOURCE_HARD_LINK,
} FILE_SOURCE_TYPE;

typedef struct {
	uint16_t type;
	uint16_t permissions;
	uint32_t uid;
	uint32_t gid;
	uint32_t devno;

	uint64_t ctime;
	uint64_t mtime;
	uint64_t size;

	char *full_path;
	char *link_target;
} file_source_record_t;

typedef struct file_source_t {
	object_t base;

	int (*get_next_record)(struct file_source_t *fs,
			       file_source_record_t **out,
			       istream_t **stream_out);
} file_source_t;

#ifdef __cplusplus
extern "C" {
#endif

file_source_t *file_source_directory_create(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* FILESOURCE_H */
