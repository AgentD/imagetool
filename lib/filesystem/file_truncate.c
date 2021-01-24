/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_truncate.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <stdlib.h>

static void truncate_sparse(fstree_t *fs, tree_node_t *n, uint64_t size)
{
	file_sparse_holes_t *it = n->data.file.sparse, *prev = NULL;
	uint64_t count;

	count = size / fs->volume->blocksize;
	if (size % fs->volume->blocksize)
		count += 1;

	while (it != NULL) {
		if (it->index >= count) {
			if (prev == NULL) {
				n->data.file.sparse = it->next;
				free(it);
				it = n->data.file.sparse;
			} else {
				prev->next = it->next;
				free(it);
				it = prev->next;
			}
		} else {
			if (it->count >= count - it->index)
				it->count = count - it->index;

			prev = it;
			it = it->next;
		}
	}
}

int fstree_file_truncate(fstree_t *fs, tree_node_t *n, uint64_t size)
{
	uint64_t old_size, new_size, old_count, new_count;
	uint64_t src, dst, diff;
	uint32_t tail_size;
	tree_node_t *fit;
	int ret;

	if (size > n->data.file.size) {
		return fstree_file_append(fs, n, NULL,
					  size - n->data.file.size);
	}

	if (size == n->data.file.size)
		return 0;

	old_size = fstree_file_physical_size(fs, n);
	old_count = old_size / fs->volume->blocksize;
	if (old_size % fs->volume->blocksize)
		old_count += 1;

	truncate_sparse(fs, n, size);
	n->data.file.size = size;

	new_size = fstree_file_physical_size(fs, n);
	new_count = new_size / fs->volume->blocksize;
	if (new_size % fs->volume->blocksize)
		new_count += 1;

	if (new_count < old_count) {
		src = n->data.file.start_index + old_count;
		dst = n->data.file.start_index + new_count;
		diff = fs->data_offset - src;

		src *= fs->volume->blocksize;
		dst *= fs->volume->blocksize;
		diff *= fs->volume->blocksize;

		if (volume_memmove(fs->volume, dst, src, diff))
			return -1;

		diff = old_count - new_count;
		src = fs->data_offset - diff;

		if (fs->volume->discard_blocks(fs->volume, src, diff))
			return -1;

		fs->data_offset -= diff;

		fit = fs->nodes_by_type[TREE_NODE_FILE];

		for (; fit != NULL; fit = fit->next_by_type) {
			if (fstree_file_physical_size(fs, fit) == 0)
				continue;

			if (fit->data.file.start_index >
			    n->data.file.start_index) {
				fit->data.file.start_index -= diff;
			}
		}
	}

	tail_size = new_size % fs->volume->blocksize;

	if (tail_size > 0) {
		src = n->data.file.start_index + new_count - 1;
		diff = fs->volume->blocksize - tail_size;

		ret = fs->volume->write_partial_block(fs->volume, src, NULL,
						      tail_size, diff);
		if (ret)
			return -1;
	}

	return 0;
}
