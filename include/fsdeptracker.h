/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fsdeptracker.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FSDEPTRACKER_H
#define FSDEPTRACKER_H

#include "predef.h"

struct fs_dep_tracker_t {
	object_t base;

	/*
	  Add a volume and the parent that it is derived from (or NULL if it is
	  at the bottom of the stacking hierarchy).
	*/
	int (*add_volume)(fs_dep_tracker_t *dep, volume_t *volume,
			  volume_t *parent);

	/*
	  Add a volume that represents a partition and the partition manager
	  that it is derived from.
	*/
	int (*add_partition)(fs_dep_tracker_t *dep, partition_t *part,
			     partition_mgr_t *parent);

	/*
	  Add a partition manager and the volume that it is based on. If the
	  partition manager is based on multiple volumes, it can be addded
	  multiple times to create several depenceny links.
	*/
	int (*add_partition_mgr)(fs_dep_tracker_t *dep, partition_mgr_t *mgr,
				 volume_t *parent);

	/*
	  Add a volume that is internally based on a file from an
	  existing filesystem.
	*/
	int (*add_volume_file)(fs_dep_tracker_t *dep, volume_t *volume,
			       filesystem_t *parent);

	/*
	  Add a named filesystem and remember the volume
	  that it is based on.
	*/
	int (*add_fs)(fs_dep_tracker_t *dep, filesystem_t *fs,
		      volume_t *parent, const char *name);

	filesystem_t *(*get_fs_by_name)(fs_dep_tracker_t *dep,
					const char *name);

	/*
	  Call build_format() on all filesystems and commit() on
	  all volumes, in the correct dependency order.
	*/
	int (*commit)(fs_dep_tracker_t *tracker);
};

#ifdef __cplusplus
extern "C" {
#endif

fs_dep_tracker_t *fs_dep_tracker_create(void);

#ifdef __cplusplus
}
#endif

#endif /* FSDEPTRACKER_H */
