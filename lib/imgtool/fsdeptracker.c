/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fsdeptracker.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fsdeptracker.h"
#include "filesystem.h"
#include "volume.h"
#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef enum {
	FS_DEPENDENCY_VOLUME = 1,
	FS_DEPENDENCY_FILESYSTEM,
	FS_DEPENDENCY_PARTITION,
	FS_DEPENDENCY_PART_MGR,
} FS_DEP_NODE_TYPE;

typedef struct fs_dependency_node_t {
	struct fs_dependency_node_t *next;
	FS_DEP_NODE_TYPE type;

	union {
		volume_t *volume;
		partition_t *partition;
		filesystem_t *filesystem;
		partition_mgr_t *partmgr;
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
	fs_dep_tracker_t base;

	fs_dependency_edge_t *edges;
	fs_dependency_node_t *nodes;
} dep_tracker_private_t;

static void fs_dep_tracker_destroy(object_t *obj)
{
	dep_tracker_private_t *dep = (dep_tracker_private_t *)obj;
	fs_dependency_node_t *nit;
	fs_dependency_edge_t *eit;

	while (dep->edges != NULL) {
		eit = dep->edges;
		dep->edges = eit->next;
		free(eit);
	}

	while (dep->nodes != NULL) {
		nit = dep->nodes;
		dep->nodes = nit->next;

		object_drop(nit->data.obj);
		free(nit);
	}

	free(dep);
}

static fs_dependency_node_t *get_by_ptr(dep_tracker_private_t *dep,
					object_t *obj, FS_DEP_NODE_TYPE type,
					const char *name)
{
	fs_dependency_node_t *it;

	for (it = dep->nodes; it != NULL; it = it->next) {
		if (it->type == type && it->data.obj == obj)
			break;
	}

	if (it == NULL) {
		it = calloc(1, sizeof(*it) + strlen(name) + 1);
		if (it == NULL) {
			perror("Creating dependency entry");
			return NULL;
		}

		it->type = type;
		it->data.obj = object_grab(obj);
		strcpy(it->name, name);
		it->next = dep->nodes;
		dep->nodes = it;
	}

	return it;
}

static fs_dependency_edge_t *find_edge(dep_tracker_private_t *dep,
				       fs_dependency_node_t *node,
				       fs_dependency_node_t *depends_on)
{
	fs_dependency_edge_t *it;

	for (it = dep->edges; it != NULL; it = it->next) {
		if (it->node == node && it->depends_on == depends_on)
			return it;
	}

	it = calloc(1, sizeof(*it));
	if (it == NULL) {
		perror("Creating dependency relation");
		return NULL;
	}

	it->node = node;
	it->depends_on = depends_on;
	it->next = dep->edges;
	dep->edges = it;
	return it;
}

static int add_dependency(fs_dep_tracker_t *interface, object_t *child,
			  FS_DEP_NODE_TYPE child_type, const char *child_name,
			  object_t *parent, FS_DEP_NODE_TYPE parent_type,
			  const char *parent_name)
{
	dep_tracker_private_t *dep = (dep_tracker_private_t *)interface;
	fs_dependency_node_t *pit, *cit;
	fs_dependency_edge_t *edge;

	cit = get_by_ptr(dep, child, child_type, child_name);
	if (cit == NULL)
		return -1;

	if (parent != NULL) {
		pit = get_by_ptr(dep, parent, parent_type, parent_name);
		if (pit == NULL)
			return -1;

		edge = find_edge(dep, cit, pit);
		if (edge == NULL)
			return -1;
	}
	return 0;
}

static int dep_tracker_add_volume(fs_dep_tracker_t *interface,
				  volume_t *volume, volume_t *parent)
{
	return add_dependency(interface,
			      (object_t *)volume, FS_DEPENDENCY_VOLUME, "",
			      (object_t *)parent, FS_DEPENDENCY_VOLUME, "");
}

static int add_partition(fs_dep_tracker_t *interface, partition_t *volume,
			 partition_mgr_t *parent)
{
	return add_dependency(interface,
			      (object_t *)volume, FS_DEPENDENCY_PARTITION, "",
			      (object_t *)parent, FS_DEPENDENCY_PART_MGR, "");
}

static int add_partition_mgr(fs_dep_tracker_t *interface, partition_mgr_t *mgr,
			     volume_t *parent)
{
	return add_dependency(interface,
			      (object_t *)mgr, FS_DEPENDENCY_PART_MGR, "",
			      (object_t *)parent, FS_DEPENDENCY_VOLUME, "");
}

static int dep_tracker_add_volume_file(fs_dep_tracker_t *interface,
				       volume_t *volume, filesystem_t *parent)
{
	return add_dependency(interface,
			      (object_t *)volume, FS_DEPENDENCY_VOLUME, "",
			      (object_t *)parent, FS_DEPENDENCY_FILESYSTEM, "");
}

static int dep_tracker_add_fs(fs_dep_tracker_t *interface,
			      filesystem_t *fs, volume_t *parent,
			      const char *name)
{
	return add_dependency(interface,
			      (object_t *)fs, FS_DEPENDENCY_FILESYSTEM, name,
			      (object_t *)parent, FS_DEPENDENCY_VOLUME, "");
}

static int dep_tracker_commit(fs_dep_tracker_t *interface)
{
	dep_tracker_private_t *tracker = (dep_tracker_private_t *)interface;
	fs_dependency_node_t *nit, *nprev;
	fs_dependency_edge_t *eit, *eprev;
	int ret;

	for (nit = tracker->nodes; nit != NULL; nit = nit->next) {
		nit->dep_count = 0;
	}

	for (eit = tracker->edges; eit != NULL; eit = eit->next) {
		eit->depends_on->dep_count += 1;
	}

	while (tracker->nodes != NULL) {
		/* find a node that nobody else depends on */
		nprev = NULL;
		nit = tracker->nodes;

		while (nit != NULL && nit->dep_count != 0) {
			nprev = nit;
			nit = nit->next;
		}

		if (nit == NULL) {
			/* TODO: better error message! */
			fputs("dependency cycle detected!\n", stderr);
			return -1;
		}

		/* resolve */
		switch (nit->type) {
		case FS_DEPENDENCY_VOLUME:
		case FS_DEPENDENCY_PARTITION:
			ret = nit->data.volume->commit(nit->data.volume);
			if (ret)
				return -1;
			break;
		case FS_DEPENDENCY_FILESYSTEM:
			ret = nit->data.filesystem->
				build_format(nit->data.filesystem);
			if (ret)
				return -1;

			ret = nit->data.filesystem->fstree->volume->
				commit(nit->data.filesystem->fstree->volume);
			if (ret)
				return -1;
			break;
		case FS_DEPENDENCY_PART_MGR:
			ret = nit->data.partmgr->commit(nit->data.partmgr);
			if (ret)
				return -1;
			break;
		default:
			break;
		}

		/* remove all outgoing edges */
		eprev = NULL;
		eit = tracker->edges;

		while (eit != NULL) {
			if (eit->node == nit) {
				eit->depends_on->dep_count -= 1;

				if (eprev == NULL) {
					tracker->edges = eit->next;
					free(eit);
					eit = tracker->edges;
				} else {
					eprev->next = eit->next;
					free(eit);
					eit = eprev->next;
				}
			} else {
				eprev = eit;
				eit = eit->next;
			}
		}

		/* remove the node from the list & destroy it */
		if (nprev == NULL) {
			tracker->nodes = nit->next;
		} else {
			nprev->next = nit->next;
		}

		object_drop(nit->data.obj);
		free(nit);
	}

	return 0;
}

static filesystem_t *dep_tracker_get_fs_by_name(fs_dep_tracker_t *interface,
						const char *name)
{
	dep_tracker_private_t *dep = (dep_tracker_private_t *)interface;
	fs_dependency_node_t *it;

	for (it = dep->nodes; it != NULL; it = it->next) {
		if (it->type == FS_DEPENDENCY_FILESYSTEM &&
		    strcmp(it->name, name) == 0) {
			break;
		}
	}

	return it == NULL ? NULL : object_grab(it->data.filesystem);
}

fs_dep_tracker_t *fs_dep_tracker_create(void)
{
	dep_tracker_private_t *dep = calloc(1, sizeof(*dep));
	fs_dep_tracker_t *public = (fs_dep_tracker_t *)dep;

	if (dep == NULL) {
		perror("Creating filesystem dependency tracker");
		return NULL;
	}

	public->add_volume = dep_tracker_add_volume;
	public->add_partition = add_partition;
	public->add_partition_mgr = add_partition_mgr;
	public->add_volume_file = dep_tracker_add_volume_file;
	public->add_fs = dep_tracker_add_fs;
	public->get_fs_by_name = dep_tracker_get_fs_by_name;
	public->commit = dep_tracker_commit;
	((object_t *)dep)->refcount = 1;
	((object_t *)dep)->destroy = fs_dep_tracker_destroy;
	return public;
}
