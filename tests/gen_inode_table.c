/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gen_inode_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "volume.h"
#include "test.h"

static void check_children_after_parent(tree_node_t *root)
{
	tree_node_t *n;

	for (n = root->data.dir.children; n != NULL; n = n->next) {
		if (n->type == TREE_NODE_HARD_LINK)
			continue;

		TEST_ASSERT(n->inode_num > root->inode_num);

		if (n->type == TREE_NODE_DIR)
			check_children_after_parent(n);
	}
}

static void check_children_continuous(tree_node_t *root)
{
	tree_node_t *n, *last;

	n = root->data.dir.children;
	last = NULL;

	while (n != NULL) {
		if (n->type != TREE_NODE_HARD_LINK) {
			if (last != NULL) {
				TEST_EQUAL_UI(n->inode_num, last->inode_num + 1);
			}

			last = n;
		}

		if (n->type == TREE_NODE_DIR)
			check_children_continuous(n);

		n = n->next;
	}
}

static void check_mapped(fstree_t *fs, tree_node_t *root)
{
	tree_node_t *it;

	if (root->type == TREE_NODE_HARD_LINK) {
		TEST_EQUAL_UI(root->inode_num, 0);
		TEST_ASSERT(fs->inode_table[0] != root);
	} else {
		TEST_ASSERT(root->inode_num < fs->num_inodes);
		TEST_ASSERT(fs->inode_table[root->inode_num] == root);
	}

	if (root->type == TREE_NODE_DIR) {
		it = root->data.dir.children;

		while (it != NULL) {
			check_mapped(fs, it);
			it = it->next;
		}
	}
}

static volume_t dummy_vol = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 1337,

	.min_block_count = 0,
	.max_block_count = 10,

	.read_block = NULL,
	.write_block = NULL,
	.move_block = NULL,
	.discard_blocks = NULL,
	.commit = NULL,
};

int main(void)
{
	fstree_t *fs;
	int ret;

	// inode table for the empty tree
	fs = fstree_create(&dummy_vol);
	TEST_NOT_NULL(fs);
	fstree_sort(fs);

	ret = fstree_resolve_hard_links(fs);
	TEST_EQUAL_UI(ret, 0);

	ret = fstree_create_inode_table(fs);
	TEST_EQUAL_UI(ret, 0);

	TEST_EQUAL_UI(fs->num_inodes, 1);
	TEST_ASSERT(fs->inode_table[0] == fs->root);

	TEST_EQUAL_UI(fs->root->inode_num, 0);

	check_mapped(fs, fs->root);
	check_children_continuous(fs->root);
	check_children_after_parent(fs->root);
	object_drop(fs);

	// tree with 3 levels under root, fan out 3
	fs = fstree_create(&dummy_vol);
	TEST_NOT_NULL(fs);

	TEST_NOT_NULL(fstree_add_fifo(fs, "a/a_a/fifo0"));
	TEST_NOT_NULL(fstree_add_fifo(fs, "a/a_a/fifo1"));
	TEST_NOT_NULL(fstree_add_fifo(fs, "a/a_a/fifo2"));
	TEST_NOT_NULL(fstree_add_directory(fs, "a/a_b"));
	TEST_NOT_NULL(fstree_add_directory(fs, "a/a_c"));

	TEST_NOT_NULL(fstree_add_hard_link(fs, "b/b_a/link0", "a/a_a/fifo0"));
	TEST_NOT_NULL(fstree_add_symlink(fs, "b/b_a/link1", "a/a_a/fifo1"));
	TEST_NOT_NULL(fstree_add_hard_link(fs, "b/b_a/link2", "a/a_a/fifo2"));
	TEST_NOT_NULL(fstree_add_directory(fs, "b/b_b"));
	TEST_NOT_NULL(fstree_add_directory(fs, "b/b_c"));

	TEST_NOT_NULL(fstree_add_symlink(fs, "c/c_a/link0", "b/b_a/link0"));
	TEST_NOT_NULL(fstree_add_hard_link(fs, "c/c_a/link1", "b/b_a/link1"));
	TEST_NOT_NULL(fstree_add_symlink(fs, "c/c_a/link2", "b/b_a/link2"));
	TEST_NOT_NULL(fstree_add_directory(fs, "c/c_b"));
	TEST_NOT_NULL(fstree_add_directory(fs, "c/c_c"));

	fstree_sort(fs);

	ret = fstree_resolve_hard_links(fs);
	TEST_EQUAL_UI(ret, 0);

	ret = fstree_create_inode_table(fs);
	TEST_EQUAL_UI(ret, 0);

	TEST_EQUAL_UI(fs->num_inodes, 19);
	check_mapped(fs, fs->root);
	check_children_continuous(fs->root);
	check_children_after_parent(fs->root);

	object_drop(fs);
	return EXIT_SUCCESS;
}
