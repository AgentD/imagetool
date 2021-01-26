/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fsdeptracker.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fsdeptracker.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void fs_dep_tracker_destroy(object_t *obj)
{
	fs_dep_tracker_t *dep = (fs_dep_tracker_t *)obj;
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

static fs_dependency_node_t *get_vol_by_ptr(fs_dep_tracker_t *dep,
					    volume_t *volume)
{
	fs_dependency_node_t *it;

	for (it = dep->nodes; it != NULL; it = it->next) {
		if (it->type == FS_DEPENDENCY_VOLUME &&
		    it->data.volume == volume) {
			break;
		}
	}

	if (it == NULL) {
		it = calloc(1, sizeof(*it) + 1);
		if (it == NULL) {
			perror("Creating volume dependency entry");
			return NULL;
		}

		it->type = FS_DEPENDENCY_VOLUME;
		it->data.volume = object_grab(volume);
		it->name[0] = '\0';
		it->next = dep->nodes;
		dep->nodes = it;
	}

	return it;
}

static fs_dependency_node_t *get_fs_by_ptr(fs_dep_tracker_t *dep,
					   filesystem_t *fs,
					   const char *name)
{
	fs_dependency_node_t *it;

	for (it = dep->nodes; it != NULL; it = it->next) {
		if (it->type == FS_DEPENDENCY_FILESYSTEM &&
		    it->data.filesystem == fs) {
			break;
		}
	}

	if (it == NULL) {
		it = calloc(1, sizeof(*it) + strlen(name) + 1);
		if (it == NULL) {
			perror("Creating filesystem dependency entry");
			return NULL;
		}

		it->type = FS_DEPENDENCY_FILESYSTEM;
		it->data.filesystem = object_grab(fs);
		strcpy(it->name, name);
		it->next = dep->nodes;
		dep->nodes = it;
	}

	return it;
}

static fs_dependency_edge_t *find_edge(fs_dep_tracker_t *dep,
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

fs_dep_tracker_t *fs_dep_tracker_create(void)
{
	fs_dep_tracker_t *dep = calloc(1, sizeof(*dep));

	if (dep == NULL) {
		perror("Creating filesystem dependency tracker");
		return NULL;
	}

	((object_t *)dep)->refcount = 1;
	((object_t *)dep)->destroy = fs_dep_tracker_destroy;
	return dep;
}

int fs_dep_tracker_add_volume(fs_dep_tracker_t *dep,
			      volume_t *volume, volume_t *parent)
{
	fs_dependency_node_t *pit, *vit;
	fs_dependency_edge_t *edge;

	vit = get_vol_by_ptr(dep, volume);
	if (vit == NULL)
		return -1;

	if (parent != NULL) {
		pit = get_vol_by_ptr(dep, parent);
		if (pit == NULL)
			return -1;

		edge = find_edge(dep, vit, pit);
		if (edge == NULL)
			return -1;
	}
	return 0;
}

int fs_dep_tracker_add_volume_file(fs_dep_tracker_t *dep,
				   volume_t *volume, filesystem_t *parent)
{
	fs_dependency_node_t *pit, *vit;
	fs_dependency_edge_t *edge;

	vit = get_vol_by_ptr(dep, volume);
	if (vit == NULL)
		return -1;

	pit = get_fs_by_ptr(dep, parent, "");
	if (pit == NULL)
		return -1;

	edge = find_edge(dep, vit, pit);
	if (edge == NULL)
		return -1;

	return 0;
}

int fs_dep_tracker_add_fs(fs_dep_tracker_t *dep,
			  filesystem_t *fs, volume_t *parent,
			  const char *name)
{
	fs_dependency_node_t *pit, *cit;
	fs_dependency_edge_t *edge;

	cit = get_fs_by_ptr(dep, fs, name);
	if (cit == NULL)
		return -1;

	pit = get_vol_by_ptr(dep, parent);
	if (pit == NULL)
		return -1;

	edge = find_edge(dep, cit, pit);
	if (edge == NULL)
		return -1;

	return 0;
}

int fs_dep_tracker_commit(fs_dep_tracker_t *tracker)
{
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
