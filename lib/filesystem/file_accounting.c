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
		uint64_t start = it->index * fs->volume->blocksize;
		uint64_t length = it->count * fs->volume->blocksize;

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

bool fstree_file_is_tail_shared(const fstree_t *fs, const tree_node_t *n)
{
	tree_node_t *it = fs->nodes_by_type[TREE_NODE_FILE];
	uint64_t size = fstree_file_physical_size(fs, n);

	if ((size % fs->volume->blocksize) == 0)
		return false;

	for (; it != NULL; it = it->next_by_type) {
		if (it == n)
			continue;

		size = fstree_file_physical_size(fs, it);

		if ((size % fs->volume->blocksize) == 0)
			continue;

		if (it->data.file.tail_index == n->data.file.tail_index)
			return true;
	}

	return false;
}

bool fstree_file_is_at_end(const fstree_t *fs, const tree_node_t *n)
{
	uint64_t phys_size = fstree_file_physical_size(fs, n);
	uint64_t blk_count = phys_size / fs->volume->blocksize;
	uint32_t tail_size = phys_size % fs->volume->blocksize;

	if (phys_size == 0)
		return true;

	if (tail_size > 0) {
		if (n->data.file.tail_index != (fs->data_offset - 1))
			return false;

		if (fstree_file_is_tail_shared(fs, n))
			return false;

		if (blk_count == 0)
			return true;

		return (n->data.file.start_index + blk_count) ==
			n->data.file.tail_index;
	}

	return blk_count >= (fs->data_offset - n->data.file.start_index);
}
