/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_mark_sparse.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int try_merge(tree_node_t *n, uint64_t index)
{
	file_sparse_holes_t *it = n->data.file.sparse, *prev = NULL, *r;

	while (it != NULL) {
		if (index >= it->index) {
			if (it->count > (index - it->index))
				return 0;

			if (it->count == (index - it->index)) {
				it->count += 1;

				if (it->next != NULL &&
				    it->next->index == (it->index + it->count)) {
					r = it->next;
					it->count += r->count;
					it->next = r->next;
					free(r);
				}
				return 0;
			}
		} else if ((index + 1) == it->index) {
			it->index -= 1;
			it->count += 1;

			if (prev != NULL &&
			    it->index == (prev->index + prev->count)) {
				prev->count += it->count;
				prev->next = it->next;
				free(it);
			}
			return 0;
		}

		prev = it;
		it = it->next;
	}

	return -1;
}

static void insert_new_region(tree_node_t *n, file_sparse_holes_t *sparse)
{
	file_sparse_holes_t *it = n->data.file.sparse, *prev = NULL;

	while (it != NULL && it->index < sparse->index) {
		prev = it;
		it = it->next;
	}

	sparse->next = it;

	if (prev == NULL) {
		n->data.file.sparse = sparse;
	} else {
		prev->next = sparse;
	}
}

int fstree_file_mark_sparse(tree_node_t *n, uint64_t index)
{
	file_sparse_holes_t *sparse;

	if (try_merge(n, index) == 0)
		return 0;

	sparse = calloc(1, sizeof(*sparse));
	if (sparse == NULL) {
		perror("allocating sparse region");
		return -1;
	}

	sparse->index = index;
	sparse->count = 1;

	insert_new_region(n, sparse);
	return 0;
}
