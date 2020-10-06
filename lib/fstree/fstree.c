/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

int fstree_init(fstree_t *fs)
{
	memset(fs, 0, sizeof(*fs));
	fs->default_permissions = 0755;

	fs->root = calloc(1, sizeof(*fs->root) + 1);
	if (fs->root == NULL)
		return -1;

	fs->root->type = TREE_NODE_DIR;
	fs->root->permissions = 0755;
	fs->root->name = (char *)fs->root->payload;
	fs->root->data.dir.created_implicitly = true;

	strcpy((char *)fs->root->payload, "");

	return 0;
}

void fstree_cleanup(fstree_t *fs)
{
	free_recursive(fs->root);
	free(fs->inode_table);
	memset(fs, 0, sizeof(*fs));
}
