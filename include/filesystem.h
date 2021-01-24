/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filesystem.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "predef.h"
#include "fstree.h"
#include "volume.h"

typedef struct filesystem_t {
	object_t base;

	/*
	  Use this object to create entries in the filesystem and to add file
	  data before finally calling the build_format function.
	 */
	fstree_t *fstree;

	/*
	  After everything is done, create meta data structures from the fstree
	  and build the actual filesystem on the underlying volume.

	  The filesystem may re-arange data in ways that the fstree_t data model
	  cannot express, so after calling this, the unerlying files may no
	  longer be accessible (including read access).
	 */
	int (*build_format)(struct filesystem_t *fs);
} filesystem_t;

typedef struct filesystem_factory_t {
	object_t base;

	/*
	  The name of this particular filesystem (e.g. "FAT32" or "SquashFS").
	 */
	const char *name;

	/*
	  Create an instance of a filesystem. The given volume is used to store
	  data. Be aware that the filesystem may choose to create an internal
	  wrapper volume on top of this one.
	 */
	filesystem_t *(*create_instance)(volume_t *volume);
} filesystem_factory_t;

#ifdef __cplusplus
extern "C" {
#endif

filesystem_factory_t *filesystem_factory_tar_create(void);

#ifdef __cplusplus
}
#endif

#endif /* FILESYSTEM_H */
