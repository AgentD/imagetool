/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filesystem.h
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "predef.h"
#include "volume.h"

typedef struct filesystem_t filesystem_t;
typedef struct file_meta_t file_meta_t;

struct file_meta_t {
	uint64_t mtime;
	uint64_t ctime;

	uint32_t owner;
	uint32_t group;
	uint16_t permissions;
};

struct filesystem_t {
	object_t base;

	volume_t *volume;

	int (*add_directory)(filesystem_t *fs, const char *path,
			     const file_meta_t *meta);

	int (*add_character_device)(filesystem_t *fs, const char *path,
				    const file_meta_t *meta, uint32_t devno);

	int (*add_block_device)(filesystem_t *fs, const char *path,
				const file_meta_t *meta, uint32_t devno);

	int (*add_fifo)(filesystem_t *fs, const char *path,
			const file_meta_t *meta);

	int (*add_socket)(filesystem_t *fs, const char *path,
			  const file_meta_t *meta);

	int (*add_symlink)(filesystem_t *fs, const char *path,
			   const file_meta_t *meta, const char *target);

	int (*add_link)(filesystem_t *fs, const char *path, const char *target);

	volume_t *(*add_file)(filesystem_t *fs, const char *path,
			      const file_meta_t *meta,
			      uint64_t min_size, uint64_t max_size);

	int (*commit)(filesystem_t *fs);
};

#endif /* FILESYSTEM_H */
