/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * node_from_path.c
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
	tree_node_t *a, *b, *c, *d, *e;
	tree_node_t *n;
	fstree_t *fs;

	/* setup fstree */
	fs = fstree_create(&dummy_vol, 42);
	TEST_EQUAL_UI(dummy_vol.base.refcount, 2);
	TEST_NOT_NULL(fs);

	TEST_EQUAL_UI(fs->root->link_count, 0);
	TEST_EQUAL_UI(fs->root->type, TREE_NODE_DIR);
	TEST_EQUAL_UI(fs->root->permissions, 0755);
	TEST_STR_EQUAL(fs->root->name, "");

	/* create a happy little tree */
	a = mknode("dev", TREE_NODE_DIR);
	b = mknode("etc", TREE_NODE_DIR);
	c = mknode("usr", TREE_NODE_DIR);
	d = mknode("sda0", TREE_NODE_BLOCK_DEV);
	e = mknode("tty0", TREE_NODE_CHAR_DEV);

	a->parent = fs->root;
	b->parent = fs->root;
	c->parent = fs->root;
	d->parent = a;
	e->parent = a;

	fs->root->data.dir.children = a;
	a->next = b;
	b->next = c;
	c->next = NULL;

	a->data.dir.children = d;
	d->next = e;
	e->next = NULL;

	/* query some existing nodes */
	n = fstree_node_from_path(fs, "dev", -1, false);
	TEST_ASSERT(n == a);

	n = fstree_node_from_path(fs, "dev/sda0", -1, false);
	TEST_ASSERT(n == d);

	n = fstree_node_from_path(fs, "/dev", -1, false);
	TEST_ASSERT(n == a);

	n = fstree_node_from_path(fs, "/dev/sda0", -1, false);
	TEST_ASSERT(n == d);

	n = fstree_node_from_path(fs, "/dev/./sda0", -1, false);
	TEST_ASSERT(n == d);

	n = fstree_node_from_path(fs, "/etc/./", -1, false);
	TEST_ASSERT(n == b);

	n = fstree_node_from_path(fs, "/etc/../dev/tty0", -1, false);
	TEST_ASSERT(n == e);

	/* query some non existing nodes */
	n = fstree_node_from_path(fs, "/dev/foo", -1, false);
	TEST_NULL(n);
	TEST_EQUAL_UI(errno, ENOENT);

	n = fstree_node_from_path(fs, "/dev/sda0/foo", -1, false);
	TEST_NULL(n);
	TEST_EQUAL_UI(errno, ENOTDIR);

	/* implicitly create directories */
	n = fstree_node_from_path(fs, "/etc/foo/bar", -1, false);
	TEST_NULL(n);
	TEST_EQUAL_UI(errno, ENOENT);

	n = fstree_node_from_path(fs, "/etc/foo/bar", -1, true);
	TEST_NOT_NULL(n);
	TEST_EQUAL_UI(n->type, TREE_NODE_DIR);
	TEST_STR_EQUAL(n->name, "bar");
	TEST_ASSERT(n->data.dir.created_implicitly);

	n = n->parent;
	TEST_NOT_NULL(n);
	TEST_EQUAL_UI(n->type, TREE_NODE_DIR);
	TEST_STR_EQUAL(n->name, "foo");
	TEST_ASSERT(n->data.dir.created_implicitly);

	n = n->parent;
	TEST_ASSERT(n == b);

	/* cleanup */
	object_drop(fs);
	TEST_EQUAL_UI(dummy_vol.base.refcount, 1);
	return EXIT_SUCCESS;
}
