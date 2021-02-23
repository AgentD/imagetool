/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_accounting.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <string.h>

uint64_t fstree_file_sparse_bytes(const fstree_t *fs, const tree_node_t *n)
{
	const file_sparse_holes_t *it;
	uint64_t count = 0;

	for (it = n->data.file.sparse; it != NULL; it = it->next) {
		uint64_t start = it->index * (uint64_t)fs->volume->blocksize;
		uint64_t length = it->count * (uint64_t)fs->volume->blocksize;

		if (start >= n->data.file.size)
			continue;

		if (length > (n->data.file.size - start))
			length = n->data.file.size - start;

		count += length;
	}

	return count;
}

uint64_t fstree_file_physical_size(const fstree_t *fs, const tree_node_t *n)
{
	uint64_t sparse = fstree_file_sparse_bytes(fs, n);

	return sparse >= n->data.file.size ? 0 : (n->data.file.size - sparse);
}
