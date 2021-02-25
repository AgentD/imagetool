/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filesystem.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "predef.h"

struct filesystem_t {
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
	int (*build_format)(filesystem_t *fs);
};

#ifdef __cplusplus
extern "C" {
#endif

filesystem_t *filesystem_tar_create(volume_t *volume);

filesystem_t *filesystem_cpio_create(volume_t *volume);

filesystem_t *filesystem_fatfs_create(volume_t *volume);

#ifdef __cplusplus
}
#endif

#endif /* FILESYSTEM_H */
