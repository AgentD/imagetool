/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filesource.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FILESOURCE_H
#define FILESOURCE_H

#include "predef.h"

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

struct file_source_t {
	object_t base;

	int (*get_next_record)(file_source_t *fs, file_source_record_t **out,
			       istream_t **stream_out);
};

struct file_source_stackable_t {
	file_source_t base;

	int (*add_nested)(file_source_stackable_t *source,
			  file_source_t *nested);
};

typedef enum {
	FILE_SOURCE_FILTER_ALLOW = 0,
	FILE_SOURCE_FILTER_DISCARD,
} FILE_SOURCE_FILTER;

struct file_source_filter_t {
	file_source_stackable_t base;

	int (*add_glob_rule)(file_source_filter_t *filter,
			     const char *pattern, int target);
};

struct file_source_listing_t {
	file_source_t base;

	int (*add_line)(file_source_listing_t *listing,
			const char *line, gcfg_file_t *file);
};

#ifdef __cplusplus
extern "C" {
#endif

file_source_t *file_source_directory_create(const char *path);

file_source_t *file_source_tar_create(const char *path);

file_source_listing_t *file_source_listing_create(const char *sourcedir);

file_source_stackable_t *file_source_aggregate_create(void);

file_source_filter_t *file_source_filter_create(void);

#ifdef __cplusplus
}
#endif

#endif /* FILESOURCE_H */
