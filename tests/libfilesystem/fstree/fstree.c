/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "../../test.h"
#include "fstree.h"
#include "volume.h"

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
	int i;

	fs = fstree_create(&dummy_vol);
	TEST_EQUAL_UI(dummy_vol.base.refcount, 2);
	TEST_NOT_NULL(fs);

	TEST_EQUAL_UI(fs->default_ctime, 0);
	TEST_EQUAL_UI(fs->default_mtime, 0);
	TEST_EQUAL_UI(fs->default_uid, 0);
	TEST_EQUAL_UI(fs->default_gid, 0);
	TEST_EQUAL_UI(fs->default_permissions, 0755);
	TEST_EQUAL_UI(fs->num_inodes, 0);
	TEST_NULL(fs->inode_table);
	TEST_EQUAL_UI(fs->data_offset, 0);
	TEST_ASSERT(fs->volume == &dummy_vol);

	TEST_EQUAL_UI(fs->root->ctime, 0);
	TEST_EQUAL_UI(fs->root->mtime, 0);
	TEST_EQUAL_UI(fs->root->uid, 0);
	TEST_EQUAL_UI(fs->root->gid, 0);
	TEST_EQUAL_UI(fs->root->inode_num, 0);
	TEST_EQUAL_UI(fs->root->link_count, 0);
	TEST_EQUAL_UI(fs->root->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(fs->root->permissions, 0755);
	TEST_STR_EQUAL(fs->root->name, "");
	TEST_NULL(fs->root->next_by_type);
	TEST_NULL(fs->root->next);
	TEST_NULL(fs->root->parent);

	TEST_ASSERT(fs->root->data.dir.created_implicitly);

	for (i = 0; i < TREE_NODE_TYPE_COUNT; ++i) {
		if (i == TREE_NODE_DIR)
			continue;

		TEST_NULL(fs->nodes_by_type[i]);
	}

	TEST_ASSERT(fs->nodes_by_type[TREE_NODE_DIR] == fs->root);

	object_drop(fs);
	TEST_EQUAL_UI(dummy_vol.base.refcount, 1);
	return EXIT_SUCCESS;
}
