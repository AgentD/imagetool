/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_sort_type.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "test.h"
#include "fstree.h"
#include "volume.h"

static tree_node_t *mknode(const char *name)
{
	tree_node_t *node = calloc(1, sizeof(*node) + strlen(name) + 1);

	TEST_NOT_NULL(node);

	node->type = TREE_NODE_FIFO;
	node->name = (const char *)node->payload;
	strcpy((char *)node->payload, name);
	return node;
}

static int name_compare_cb(const tree_node_t *lhs, const tree_node_t *rhs)
{
	return strcmp(lhs->name, rhs->name);
}

int main(void)
{
	tree_node_t *a, *b, *c, *d;

	a = mknode("a");
	b = mknode("b");
	c = mknode("c");
	d = mknode("d");

	/* empty list */
	TEST_NULL(fstree_sort_node_list_by_type(NULL, name_compare_cb));

	/* single element */
	TEST_ASSERT(fstree_sort_node_list_by_type(a, name_compare_cb) == a);
	TEST_NULL(a->next_by_type);

	/* two elements, reverse order */
	b->next_by_type = a;
	TEST_ASSERT(fstree_sort_node_list_by_type(b, name_compare_cb) == a);
	TEST_ASSERT(a->next_by_type == b);
	TEST_NULL(b->next_by_type);

	/* two elements, sorted order */
	TEST_ASSERT(fstree_sort_node_list_by_type(a, name_compare_cb) == a);
	TEST_ASSERT(a->next_by_type == b);
	TEST_NULL(b->next_by_type);

	/* three elements, reverse order */
	c->next_by_type = b;
	b->next_by_type = a;
	a->next_by_type = NULL;

	TEST_ASSERT(fstree_sort_node_list_by_type(c, name_compare_cb) == a);
	TEST_ASSERT(a->next_by_type == b);
	TEST_ASSERT(b->next_by_type == c);
	TEST_NULL(c->next_by_type);

	/* three elements, ordered */
	TEST_ASSERT(fstree_sort_node_list_by_type(a, name_compare_cb) == a);
	TEST_ASSERT(a->next_by_type == b);
	TEST_ASSERT(b->next_by_type == c);
	TEST_NULL(c->next_by_type);

	/* four elements, reverse order */
	d->next_by_type = c;
	c->next_by_type = b;
	b->next_by_type = a;
	a->next_by_type = NULL;

	TEST_ASSERT(fstree_sort_node_list_by_type(d, name_compare_cb) == a);
	TEST_ASSERT(a->next_by_type == b);
	TEST_ASSERT(b->next_by_type == c);
	TEST_ASSERT(c->next_by_type == d);
	TEST_NULL(d->next_by_type);

	/* four elements, sorted order */
	TEST_ASSERT(fstree_sort_node_list_by_type(a, name_compare_cb) == a);
	TEST_ASSERT(a->next_by_type == b);
	TEST_ASSERT(b->next_by_type == c);
	TEST_ASSERT(c->next_by_type == d);
	TEST_NULL(d->next_by_type);

	/* force merge sort to go through LRLR pattern */
	b->next_by_type = a;
	a->next_by_type = d;
	d->next_by_type = c;
	c->next_by_type = NULL;

	TEST_ASSERT(fstree_sort_node_list_by_type(b, name_compare_cb) == a);
	TEST_ASSERT(a->next_by_type == b);
	TEST_ASSERT(b->next_by_type == c);
	TEST_ASSERT(c->next_by_type == d);
	TEST_NULL(d->next_by_type);

	/* cleanup and done */
	free(a);
	free(b);
	free(c);
	free(d);
	return EXIT_SUCCESS;
}
