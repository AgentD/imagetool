/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_move_to_end.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <string.h>

static int close_gap(fstree_t *fs, uint64_t index, uint64_t count)
{
	tree_node_t *fit;
	int ret;

	ret = volume_memmove(fs->volume, index, index + count,
			     fs->data_offset - (index + count));
	if (ret)
		return -1;

	fs->data_offset -= count;

	if (fs->volume->discard_blocks(fs->volume, fs->data_offset, count))
		return -1;

	fit = fs->nodes_by_type[TREE_NODE_FILE];

	for (; fit != NULL; fit = fit->next_by_type) {
		uint64_t sparse_count = fstree_file_sparse_bytes(fs, fit);
		uint64_t phys_size = fit->data.file.size - sparse_count;

		if (phys_size == 0)
			continue;

		if (phys_size > fs->volume->blocksize &&
		    fit->data.file.start_index >= (index + count)) {
			fit->data.file.start_index -= count;
		}

		if (phys_size % fs->volume->blocksize &&
		    fit->data.file.tail_index >= (index + count)) {
			fit->data.file.tail_index -= count;
		}
	}

	return 0;
}

static int close_tail_block_gap(fstree_t *fs, uint64_t index,
				uint32_t offset, uint32_t size)
{
	tree_node_t *fit = fs->nodes_by_type[TREE_NODE_FILE];
	uint32_t used = 0;
	int ret;

	for (; fit != NULL; fit = fit->next_by_type) {
		uint64_t sparse_count = fstree_file_sparse_bytes(fs, fit);
		uint64_t phys_size = fit->data.file.size - sparse_count;
		uint32_t tail_size = phys_size % fs->volume->blocksize;

		if (tail_size > 0 && fit->data.file.tail_index == index) {
			uint32_t end = fit->data.file.tail_offset + tail_size;
			used = (end > used) ? end : used;

			if (fit->data.file.tail_offset >= (offset + size))
				fit->data.file.tail_offset -= size;
		}
	}

	if (used > (offset + size)) {
		ret  = fs->volume->move_block_partial(fs->volume, index, index,
						      offset + size, offset,
						      used - (offset + size));
		if (ret)
			return -1;

		used -= size;
	} else {
		used = offset;
	}

	if (used == fs->volume->blocksize)
		return 0;

	return fs->volume->write_partial_block(fs->volume, index, NULL, used,
					       fs->volume->blocksize - used);
}

/*****************************************************************************/

static int move_blocks_to_end(fstree_t *fs, tree_node_t *n, uint64_t count)
{
	uint64_t start = n->data.file.start_index;
	uint64_t src = start * fs->volume->blocksize;
	uint64_t dst = fs->data_offset * fs->volume->blocksize;

	if (volume_memmove(fs->volume, dst, src, count * fs->data_offset))
		return -1;

	n->data.file.start_index = fs->data_offset;
	fs->data_offset += count;

	return close_gap(fs, start, count);
}

static int copy_tail_to_end(fstree_t *fs, tree_node_t *n, uint32_t tail_size)
{
	uint32_t tail_off = n->data.file.tail_offset, diff;
	uint64_t tail_idx = n->data.file.tail_index;
	int ret;

	ret = fs->volume->move_block_partial(fs->volume, tail_idx,
					     fs->data_offset, tail_off, 0,
					     tail_size);
	if (ret)
		return -1;

	n->data.file.tail_index = fs->data_offset++;
	n->data.file.tail_offset = 0;

	diff = fs->volume->blocksize - tail_size;
	ret = fs->volume->write_partial_block(fs->volume,
					      n->data.file.tail_index, NULL,
					      tail_size, diff);
	if (ret)
		return -1;

	return close_tail_block_gap(fs, tail_idx, tail_off, tail_size);
}

static int move_tail_to_end(fstree_t *fs, tree_node_t *n)
{
	uint64_t index = n->data.file.tail_index;

	if (fs->volume->move_block(fs->volume, index, fs->data_offset, 0))
		return -1;

	n->data.file.tail_index = fs->data_offset++;
	return close_gap(fs, index, 1);
}

int fstree_file_move_to_end(fstree_t *fs, tree_node_t *n)
{
	uint64_t phys_size, blk_count;
	uint32_t tail_size;
	int ret;

	if (fstree_file_is_at_end(fs, n))
		return 0;

	phys_size = fstree_file_physical_size(fs, n);
	blk_count = phys_size / fs->volume->blocksize;
	tail_size = phys_size % fs->volume->blocksize;

	if (blk_count > 0) {
		ret = move_blocks_to_end(fs, n, blk_count);
		if (ret)
			return -1;
	}

	if (tail_size == 0)
		return 0;

	if (fstree_file_is_tail_shared(fs, n))
		return copy_tail_to_end(fs, n, tail_size);

	return move_tail_to_end(fs, n);
}
