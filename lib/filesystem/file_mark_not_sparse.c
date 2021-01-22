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
	file_sparse_holes_t *it, *new;
	uint64_t rel_idx;

	for (it = n->data.file.sparse; it != NULL; it = it->next) {
		if (index < it->index)
			continue;

		rel_idx = index - it->index;
		if (rel_idx >= it->count)
			continue;

		if (rel_idx == 0) {
			it->index += 1;
			it->count -= 1;
			return 0;
		}

		if (rel_idx == (it->count - 1)) {
			it->count -= 1;
			return 0;
		}

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
		break;
	}

	return 0;
}
