/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_mark_not_sparse.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int fstree_file_mark_not_sparse(tree_node_t *n, uint64_t index)
{
	file_sparse_holes_t *it, *new, *prev;
	uint64_t rel_idx;

	it = n->data.file.sparse;
	prev = NULL;

	while (it != NULL) {
		if (index >= it->index) {
			rel_idx = index - it->index;
			if (rel_idx < it->count)
				break;
		}

		prev = it;
		it = it->next;
	}

	if (it == NULL)
		return 0;

	if (rel_idx == 0 || rel_idx == (it->count - 1)) {
		it->count -= 1;

		if (it->count == 0) {
			if (prev == NULL) {
				n->data.file.sparse = it->next;
			} else {
				prev->next = it->next;
			}
			free(it);
		} else if (rel_idx == 0) {
			it->index += 1;
		}
	} else {
		new = calloc(1, sizeof(*new));
		if (new == NULL) {
			perror("allocating sparse region");
			return -1;
		}

		new->index = it->index + rel_idx + 1;
		new->count = it->count - rel_idx - 1;

		it->count = rel_idx;

		new->next = it->next;
		it->next = new;
	}
	return 0;
}
