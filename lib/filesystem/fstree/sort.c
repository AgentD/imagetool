/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_sort.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 * Copyright (C) 2019 Zachary Dremann <dremann@gmail.com>
 */
#include "fstree.h"

#include <string.h>

static int name_compare_cb(const tree_node_t *lhs, const tree_node_t *rhs)
{
	return strcmp(lhs->name, rhs->name);
}

static tree_node_t *merge(tree_node_t *lhs, tree_node_t *rhs,
			  node_compare_fun_t cb)
{
	tree_node_t *it;
	tree_node_t *head = NULL;
	tree_node_t **next_ptr = &head;

	while (lhs != NULL && rhs != NULL) {
		if (cb(lhs, rhs) <= 0) {
			it = lhs;
			lhs = lhs->next;
		} else {
			it = rhs;
			rhs = rhs->next;
		}

		*next_ptr = it;
		next_ptr = &it->next;
	}

	it = (lhs != NULL ? lhs : rhs);
	*next_ptr = it;
	return head;
}

static tree_node_t *merge_by_type(tree_node_t *lhs, tree_node_t *rhs,
				  node_compare_fun_t cb)
{
	tree_node_t *it;
	tree_node_t *head = NULL;
	tree_node_t **next_ptr = &head;

	while (lhs != NULL && rhs != NULL) {
		if (cb(lhs, rhs) <= 0) {
			it = lhs;
			lhs = lhs->next_by_type;
		} else {
			it = rhs;
			rhs = rhs->next_by_type;
		}

		*next_ptr = it;
		next_ptr = &it->next_by_type;
	}

	it = (lhs != NULL ? lhs : rhs);
	*next_ptr = it;
	return head;
}

tree_node_t *fstree_sort_node_list(tree_node_t *head,
				   node_compare_fun_t cb)
{
	tree_node_t *it, *half, *prev;

	it = half = prev = head;
	while (it != NULL) {
		prev = half;
		half = half->next;
		it = it->next;
		if (it != NULL) {
			it = it->next;
		}
	}

	// half refers to the (count/2)'th element ROUNDED UP.
	// It will be null therefore only in the empty and the
	// single element list
	if (half == NULL) {
		return head;
	}

	prev->next = NULL;

	return merge(fstree_sort_node_list(head, cb),
		     fstree_sort_node_list(half, cb), cb);
}

tree_node_t *fstree_sort_node_list_by_type(tree_node_t *head,
					   node_compare_fun_t cb)
{
	tree_node_t *it, *half, *prev;

	it = half = prev = head;
	while (it != NULL) {
		prev = half;
		half = half->next_by_type;
		it = it->next_by_type;
		if (it != NULL) {
			it = it->next_by_type;
		}
	}

	// half refers to the (count/2)'th element ROUNDED UP.
	// It will be null therefore only in the empty and the
	// single element list
	if (half == NULL) {
		return head;
	}

	prev->next_by_type = NULL;

	return merge_by_type(fstree_sort_node_list_by_type(head, cb),
			     fstree_sort_node_list_by_type(half, cb), cb);
}

static void fstree_sort_node_recursive(tree_node_t *root)
{
	tree_node_t *it, *list;

	if (root->type != TREE_NODE_DIR)
		return;

	list = fstree_sort_node_list(root->data.dir.children, name_compare_cb);
	root->data.dir.children = list;

	for (it = root->data.dir.children; it != NULL; it = it->next) {
		fstree_sort_node_recursive(it);
	}
}

static tree_node_t *linearize_dir_tree(tree_node_t *root, int type)
{
	tree_node_t *list = NULL, *list_last = NULL;
	tree_node_t *it, *sublist;

	for (it = root->data.dir.children; it != NULL; it = it->next) {
		if (it->type == type) {
			if (list == NULL) {
				list = it;
			} else {
				list_last->next_by_type = it;
			}

			list_last = it;
			it->next_by_type = NULL;
		}
	}

	for (it = root->data.dir.children; it != NULL; it = it->next) {
		if (it->type != TREE_NODE_DIR)
			continue;

		sublist = linearize_dir_tree(it, type);
		if (sublist == NULL)
			continue;

		if (list == NULL) {
			list = sublist;
			list_last = sublist;
		} else {
			list_last->next_by_type = sublist;
		}

		while (list_last->next_by_type != NULL)
			list_last = list_last->next_by_type;
	}

	return list;
}

void fstree_sort(fstree_t *tree)
{
	tree_node_t *list;
	int i;

	fstree_sort_node_recursive(tree->root);

	for (i = 0; i < TREE_NODE_TYPE_COUNT; ++i) {
		list = linearize_dir_tree(tree->root, i);
		tree->nodes_by_type[i] = list;
	}

	tree->root->next_by_type = tree->nodes_by_type[TREE_NODE_DIR];
	tree->nodes_by_type[TREE_NODE_DIR] = tree->root;
}
