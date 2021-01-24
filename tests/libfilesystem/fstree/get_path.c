/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_path.c
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

static tree_node_t *mknode(const char *name, int type)
{
	tree_node_t *node = calloc(1, sizeof(*node) + strlen(name) + 1);

	TEST_NOT_NULL(node);

	node->type = type;
	node->name = (const char *)node->payload;
	strcpy((char *)node->payload, name);
	return node;
}

int main(void)
{
	tree_node_t *dev, *etc, *usr, *bin, *lib, *sda0, *tty0, *foo;
	fstree_t *fs;
	char *path;

	fs = fstree_create(&dummy_vol);
	TEST_NOT_NULL(fs);

	/* create a happy little tree */
	dev = mknode("dev", TREE_NODE_DIR);
	etc = mknode("etc", TREE_NODE_DIR);
	usr = mknode("usr", TREE_NODE_DIR);
	sda0 = mknode("sda0", TREE_NODE_BLOCK_DEV);
	tty0 = mknode("tty0", TREE_NODE_CHAR_DEV);
	bin = mknode("bin", TREE_NODE_DIR);
	lib = mknode("lib", TREE_NODE_DIR);
	foo = mknode("foo", TREE_NODE_FILE);

	dev->parent = fs->root;
	etc->parent = fs->root;
	usr->parent = fs->root;
	sda0->parent = dev;
	tty0->parent = dev;
	bin->parent = usr;
	lib->parent = usr;
	foo->parent = bin;

	fs->root->data.dir.children = dev;
	dev->next = etc;
	etc->next = usr;
	usr->next = NULL;

	dev->data.dir.children = sda0;
	sda0->next = tty0;
	tty0->next = NULL;

	usr->data.dir.children = bin;
	bin->next = lib;
	lib->next = NULL;

	bin->data.dir.children = foo;
	foo->next = NULL;

	/* test paths */
	path = fstree_get_path(fs->root);
	TEST_NOT_NULL(path);
	TEST_STR_EQUAL(path, "/");
	free(path);

	path = fstree_get_path(dev);
	TEST_NOT_NULL(path);
	TEST_STR_EQUAL(path, "/dev");
	free(path);

	path = fstree_get_path(sda0);
	TEST_NOT_NULL(path);
	TEST_STR_EQUAL(path, "/dev/sda0");
	free(path);

	path = fstree_get_path(tty0);
	TEST_NOT_NULL(path);
	TEST_STR_EQUAL(path, "/dev/tty0");
	free(path);

	path = fstree_get_path(etc);
	TEST_NOT_NULL(path);
	TEST_STR_EQUAL(path, "/etc");
	free(path);

	path = fstree_get_path(usr);
	TEST_NOT_NULL(path);
	TEST_STR_EQUAL(path, "/usr");
	free(path);

	path = fstree_get_path(bin);
	TEST_NOT_NULL(path);
	TEST_STR_EQUAL(path, "/usr/bin");
	free(path);

	path = fstree_get_path(foo);
	TEST_NOT_NULL(path);
	TEST_STR_EQUAL(path, "/usr/bin/foo");
	free(path);

	path = fstree_get_path(lib);
	TEST_NOT_NULL(path);
	TEST_STR_EQUAL(path, "/usr/lib");
	free(path);

	/* cleanup */
	object_drop(fs);
	return EXIT_SUCCESS;
}
