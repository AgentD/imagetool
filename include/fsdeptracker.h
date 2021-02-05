/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fsdeptracker.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FSDEPTRACKER_H
#define FSDEPTRACKER_H

#include "predef.h"
#include "volume.h"
#include "filesystem.h"

typedef enum {
	FS_DEPENDENCY_VOLUME = 1,
	FS_DEPENDENCY_FILESYSTEM,
} FS_DEP_NODE_TYPE;

typedef struct fs_dependency_node_t {
	struct fs_dependency_node_t *next;
	FS_DEP_NODE_TYPE type;

	union {
		volume_t *volume;
		filesystem_t *filesystem;
		object_t *obj;
	} data;

	/* how many other nodes depend on this one? */
	size_t dep_count;

	char name[];
} fs_dependency_node_t;

typedef struct fs_dependency_edge_t {
	struct fs_dependency_edge_t *next;
	fs_dependency_node_t *node;
	fs_dependency_node_t *depends_on;
} fs_dependency_edge_t;

typedef struct {
	object_t base;

	fs_dependency_edge_t *edges;
	fs_dependency_node_t *nodes;
} fs_dep_tracker_t;

#ifdef __cplusplus
extern "C" {
#endif

fs_dep_tracker_t *fs_dep_tracker_create(void);

/*
  Add a volume and the parent that it is derived from (or NULL if it is at the
  bottom of the stacking hierarchy).
 */
int fs_dep_tracker_add_volume(fs_dep_tracker_t *dep,
			      volume_t *volume, volume_t *parent);

/*
  Add a volume that is internally based on a file from an existing filesystem.
 */
int fs_dep_tracker_add_volume_file(fs_dep_tracker_t *dep,
				   volume_t *volume, filesystem_t *parent);

/*
  Add a named filesystem and remember the volume that it is based on.
 */
int fs_dep_tracker_add_fs(fs_dep_tracker_t *dep,
			  filesystem_t *fs, volume_t *parent,
			  const char *name);

filesystem_t *fs_dep_tracker_get_fs_by_name(fs_dep_tracker_t *dep,
					    const char *name);

/*
  Call build_format() on all filesystems and commit() on all volumes, in the
  correct dependency order.
 */
int fs_dep_tracker_commit(fs_dep_tracker_t *tracker);

#ifdef __cplusplus
}
#endif

#endif /* FSDEPTRACKER_H */
