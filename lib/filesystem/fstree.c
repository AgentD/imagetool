/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void free_recursive(tree_node_t *n)
{
	tree_node_t *it;

	if (n->type == TREE_NODE_FILE)
		free(n->data.file.blocks);

	if (n->type == TREE_NODE_DIR) {
		while (n->data.dir.children != NULL) {
			it = n->data.dir.children;
			n->data.dir.children = it->next;

			free_recursive(it);
		}
	}

	free(n);
}

static void destroy(object_t *base)
{
	fstree_t *fs = (fstree_t *)base;

	object_drop(fs->volume);
	free_recursive(fs->root);
	free(fs->inode_table);
	free(fs);
}

fstree_t *fstree_create(volume_t *volume, uint64_t data_lead_in)
{
	fstree_t *fs = calloc(1, sizeof(*fs));

	if (fs == NULL)
		return NULL;

	fs->root = calloc(1, sizeof(*fs->root) + 1);
	if (fs->root == NULL) {
		free(fs);
		errno = ENOMEM;
		return NULL;
	}

	fs->default_permissions = 0755;
	fs->root->type = TREE_NODE_DIR;
	fs->root->permissions = 0755;
	fs->root->name = (char *)fs->root->payload;
	fs->root->data.dir.created_implicitly = true;

	strcpy((char *)fs->root->payload, "");

	((object_t *)fs)->refcount = 1;
	((object_t *)fs)->destroy = destroy;

	fs->volume = object_grab(volume);
	fs->data_lead_in = data_lead_in;
	fs->data_offset = data_lead_in;

	fs->nodes_by_type[TREE_NODE_DIR] = fs->root;
	return fs;
}
