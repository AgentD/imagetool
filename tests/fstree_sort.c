/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_sort.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "volume.h"
#include "test.h"

static tree_node_t *mknode(const char *name)
{
	tree_node_t *node = calloc(1, sizeof(*node) + strlen(name) + 1);

	TEST_NOT_NULL(node);

	node->type = TREE_NODE_FIFO;
	node->name = (const char *)node->payload;
	strcpy((char *)node->payload, name);
	return node;
}

int main(void)
{
	tree_node_t *a, *b, *c, *d;

	a = mknode("a");
	b = mknode("b");
	c = mknode("c");
	d = mknode("d");

	/* empty list */
	TEST_NULL(fstree_sort_node_list(NULL));

	/* single element */
	TEST_ASSERT(fstree_sort_node_list(a) == a);
	TEST_NULL(a->next);

	/* two elements, reverse order */
	b->next = a;
	TEST_ASSERT(fstree_sort_node_list(b) == a);
	TEST_ASSERT(a->next == b);
	TEST_NULL(b->next);

	/* two elements, sorted order */
	TEST_ASSERT(fstree_sort_node_list(a) == a);
	TEST_ASSERT(a->next == b);
	TEST_NULL(b->next);

	/* three elements, reverse order */
	c->next = b;
	b->next = a;
	a->next = NULL;

	TEST_ASSERT(fstree_sort_node_list(c) == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_NULL(c->next);

	/* three elements, ordered */
	TEST_ASSERT(fstree_sort_node_list(a) == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_NULL(c->next);

	/* four elements, reverse order */
	d->next = c;
	c->next = b;
	b->next = a;
	a->next = NULL;

	TEST_ASSERT(fstree_sort_node_list(d) == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	/* four elements, sorted order */
	TEST_ASSERT(fstree_sort_node_list(a) == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	/* force merge sort to go through LRLR pattern */
	b->next = a;
	a->next = d;
	d->next = c;
	c->next = NULL;

	TEST_ASSERT(fstree_sort_node_list(b) == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	/* cleanup and done */
	free(a);
	free(b);
	free(c);
	free(d);
	return EXIT_SUCCESS;
}
