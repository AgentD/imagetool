/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * resolve_hard_links.c
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
	tree_node_t *a, *b, *c, *d, *e;
	tree_node_t *ln0, *ln1, *ln2;
	fstree_t *fs;
	int ret;

	/* setup fstree */
	fs = fstree_create(&dummy_vol);
	TEST_NOT_NULL(fs);

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

	/* create hard links */
	ln0 = fstree_add_hard_link(fs, "dev/tty1", "tty0");
	TEST_NOT_NULL(ln0);

	ln1 = fstree_add_hard_link(fs, "dev/tty2", "/dev/tty1");
	TEST_NOT_NULL(ln1);

	/* resolve */
	ret = fstree_resolve_hard_links(fs);
	TEST_EQUAL_I(ret, 0);

	TEST_ASSERT(ln0->data.link.resolved == e);
	TEST_ASSERT(ln1->data.link.resolved == e);
	TEST_EQUAL_UI(e->link_count, 2);

	/* non existant target */
	ln0 = fstree_add_hard_link(fs, "dev/tty3", "foo");
	TEST_NOT_NULL(ln0);

	ret = fstree_resolve_hard_links(fs);
	TEST_ASSERT(ret != 0);

	/* loop */
	ln1 = fstree_add_hard_link(fs, "dev/foo", "bar");
	TEST_NOT_NULL(ln1);

	ln2 = fstree_add_hard_link(fs, "dev/bar", "tty3");
	TEST_NOT_NULL(ln2);

	ret = fstree_resolve_hard_links(fs);
	TEST_ASSERT(ret != 0);

	/* cleanup */
	object_drop(fs);
	return EXIT_SUCCESS;
}
