/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * create_inode_table.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <stdlib.h>
#include <stdio.h>

static size_t count_inodes_dfs(tree_node_t *n)
{
	if (n->type == TREE_NODE_HARD_LINK)
		return 0;

	if (n->type == TREE_NODE_DIR) {
		size_t sum = 0;

		n = n->data.dir.children;

		while (n != NULL) {
			sum += count_inodes_dfs(n);
			n = n->next;
		}

		return sum + 1;
	}

	return 1;
}

static void map_inodes_dfs(fstree_t *fs, tree_node_t *n, size_t *index)
{
	tree_node_t *it;

	for (it = n->data.dir.children; it != NULL; it = it->next) {
		if (it->type != TREE_NODE_HARD_LINK) {
			it->inode_num = (*index)++;
			fs->inode_table[it->inode_num] = it;
		}
	}

	for (it = n->data.dir.children; it != NULL; it = it->next) {
		if (it->type == TREE_NODE_DIR)
			map_inodes_dfs(fs, it, index);
	}
}

int fstree_create_inode_table(fstree_t *fs)
{
	size_t index;

	free(fs->inode_table);
	fs->inode_table = NULL;

	fs->num_inodes = count_inodes_dfs(fs->root);
	fs->inode_table = calloc(fs->num_inodes, sizeof(fs->inode_table[0]));

	if (fs->inode_table == NULL) {
		perror("allocating inode table");
		return -1;
	}

	fs->root->inode_num = 0;
	fs->inode_table[0] = fs->root;

	index = 1;
	map_inodes_dfs(fs, fs->root, &index);
	return 0;
}
