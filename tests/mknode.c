/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "volume.h"
#include "test.h"

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
	tree_node_t *a, *b;
	fstree_t *fs;

	/* setup fstree */
	fs = fstree_create(&dummy_vol, 42);
	TEST_NOT_NULL(fs);

	fs->default_ctime = 4;
	fs->default_mtime = 8;
	fs->default_uid = 1000;
	fs->default_gid = 100;
	fs->default_permissions = 0750;

	/* add a directory */
	a = fstree_add_directory(fs, "dir");
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "dir");
	TEST_EQUAL_UI(a->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(a->permissions, fs->default_permissions);
	TEST_EQUAL_UI(a->uid, fs->default_uid);
	TEST_EQUAL_UI(a->gid, fs->default_gid);
	TEST_EQUAL_UI(a->link_count, 0);
	TEST_EQUAL_UI(a->ctime, fs->default_ctime);
	TEST_EQUAL_UI(a->mtime, fs->default_mtime);

	TEST_ASSERT(!a->data.dir.created_implicitly);
	TEST_ASSERT(a->data.dir.children == NULL);

	TEST_ASSERT(fs->root->data.dir.children == a);
	TEST_ASSERT(a->parent == fs->root);
	TEST_NULL(a->next);

	/* add a block device */
	b = fstree_add_block_device(fs, "blkdev", 1234);
	TEST_NOT_NULL(b);
	TEST_ASSERT(b != a);
	TEST_STR_EQUAL(b->name, "blkdev");
	TEST_EQUAL_UI(b->type, TREE_NODE_BLOCK_DEV);
	TEST_EQUAL_UI(b->permissions, (fs->default_permissions & 0666));
	TEST_EQUAL_UI(b->uid, fs->default_uid);
	TEST_EQUAL_UI(b->gid, fs->default_gid);
	TEST_EQUAL_UI(b->link_count, 0);
	TEST_EQUAL_UI(b->ctime, fs->default_ctime);
	TEST_EQUAL_UI(b->mtime, fs->default_mtime);
	TEST_EQUAL_UI(b->data.device_number, 1234);

	TEST_ASSERT(b->parent == fs->root);
	TEST_ASSERT(b->next == a);
	TEST_ASSERT(fs->root->data.dir.children == b);

	/* cannot create anything underneath the blockdevice */
	TEST_NULL(fstree_add_fifo(fs, "blkdev/foo"));
	TEST_EQUAL_UI(errno, ENOTDIR);

	/* cannot create something that already exists */
	TEST_NULL(fstree_add_fifo(fs, "dir"));
	TEST_EQUAL_UI(errno, EEXIST);

	/* character device inside the first directory */
	b = fstree_add_character_device(fs, "dir/chrdev", 5678);
	TEST_NOT_NULL(b);
	TEST_STR_EQUAL(b->name, "chrdev");
	TEST_EQUAL_UI(b->type, TREE_NODE_CHAR_DEV);
	TEST_EQUAL_UI(b->permissions, (fs->default_permissions & 0666));
	TEST_EQUAL_UI(b->uid, fs->default_uid);
	TEST_EQUAL_UI(b->gid, fs->default_gid);
	TEST_EQUAL_UI(b->link_count, 0);
	TEST_EQUAL_UI(b->ctime, fs->default_ctime);
	TEST_EQUAL_UI(b->mtime, fs->default_mtime);
	TEST_EQUAL_UI(b->data.device_number, 5678);

	TEST_ASSERT(b->parent == a);
	TEST_NULL(b->next);
	TEST_ASSERT(a->data.dir.children == b);

	/* implicitly created directory */
	b = fstree_add_character_device(fs, "dir/foo/chrdev2", 42);
	TEST_NOT_NULL(b);
	TEST_STR_EQUAL(b->name, "chrdev2");
	TEST_EQUAL_UI(b->type, TREE_NODE_CHAR_DEV);
	TEST_EQUAL_UI(b->permissions, (fs->default_permissions & 0666));
	TEST_EQUAL_UI(b->uid, fs->default_uid);
	TEST_EQUAL_UI(b->gid, fs->default_gid);
	TEST_EQUAL_UI(b->link_count, 0);
	TEST_EQUAL_UI(b->ctime, fs->default_ctime);
	TEST_EQUAL_UI(b->mtime, fs->default_mtime);
	TEST_EQUAL_UI(b->data.device_number, 42);

	TEST_ASSERT(b->parent != a);
	TEST_ASSERT(b->parent->parent == a);
	TEST_NULL(b->next);

	TEST_ASSERT(a->data.dir.children != b);

	b = b->parent;
	TEST_EQUAL_UI(b->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(b->permissions, fs->default_permissions);
	TEST_EQUAL_UI(b->uid, fs->default_uid);
	TEST_EQUAL_UI(b->gid, fs->default_gid);
	TEST_EQUAL_UI(b->link_count, 0);
	TEST_EQUAL_UI(b->ctime, fs->default_ctime);
	TEST_EQUAL_UI(b->mtime, fs->default_mtime);

	TEST_ASSERT(b->data.dir.created_implicitly);
	TEST_STR_EQUAL(b->name, "foo");

	/* create it explicitly a second time */
	a = fstree_add_directory(fs, "dir/foo");
	TEST_NOT_NULL(a);
	TEST_ASSERT(a == b);
	TEST_ASSERT(!a->data.dir.created_implicitly);
	TEST_EQUAL_UI(a->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(a->permissions, fs->default_permissions);
	TEST_EQUAL_UI(a->uid, fs->default_uid);
	TEST_EQUAL_UI(a->gid, fs->default_gid);
	TEST_EQUAL_UI(a->link_count, 0);
	TEST_EQUAL_UI(a->ctime, fs->default_ctime);
	TEST_EQUAL_UI(a->mtime, fs->default_mtime);

	/* must fail a third time */
	TEST_NULL(fstree_add_directory(fs, "dir/foo"));
	TEST_EQUAL_UI(errno, EEXIST);

	/* special case: links */
	b = fstree_add_symlink(fs, "dir/foo/alink", "first_target");
	TEST_NOT_NULL(b);
	TEST_EQUAL_UI(b->type, TREE_NODE_SYMLINK);
	TEST_EQUAL_UI(b->permissions, 0777);
	TEST_EQUAL_UI(b->uid, fs->default_uid);
	TEST_EQUAL_UI(b->gid, fs->default_gid);
	TEST_EQUAL_UI(b->link_count, 0);
	TEST_EQUAL_UI(b->ctime, fs->default_ctime);
	TEST_EQUAL_UI(b->mtime, fs->default_mtime);
	TEST_STR_EQUAL(b->name, "alink");
	TEST_STR_EQUAL(b->data.link.target, "first_target");
	TEST_ASSERT(b->parent == a);

	b = fstree_add_hard_link(fs, "dir/foo/blink", "dir/foo/chrdev");
	TEST_NOT_NULL(b);
	TEST_EQUAL_UI(b->type, TREE_NODE_HARD_LINK);
	TEST_STR_EQUAL(b->name, "blink");
	TEST_STR_EQUAL(b->data.link.target, "dir/foo/chrdev");
	TEST_ASSERT(b->parent == a);

	/* make sure we have the global lists of everything */
	a = fs->nodes_by_type[TREE_NODE_DIR];
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "foo");

	a = a->next_by_type;
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "dir");

	a = a->next_by_type;
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "");
	TEST_ASSERT(a == fs->root);
	TEST_NULL(a->next_by_type);

	a = fs->nodes_by_type[TREE_NODE_FILE];
	TEST_NULL(a);

	a = fs->nodes_by_type[TREE_NODE_FIFO];
	TEST_NULL(a);

	a = fs->nodes_by_type[TREE_NODE_SOCKET];
	TEST_NULL(a);

	a = fs->nodes_by_type[TREE_NODE_CHAR_DEV];
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "chrdev2");

	a = a->next_by_type;
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "chrdev");
	TEST_NULL(a->next_by_type);

	a = fs->nodes_by_type[TREE_NODE_BLOCK_DEV];
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "blkdev");
	TEST_NULL(a->next_by_type);

	a = fs->nodes_by_type[TREE_NODE_SYMLINK];
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "alink");
	TEST_NULL(a->next_by_type);

	a = fs->nodes_by_type[TREE_NODE_HARD_LINK];
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "blink");
	TEST_NULL(a->next_by_type);

	/* cleanup */
	object_drop(fs);
	return EXIT_SUCCESS;
}
