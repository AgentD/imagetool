/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_write.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"
#include "volume.h"
#include "util.h"

#include <string.h>

static int insert_sparse_block(fstree_t *fs, tree_node_t *n,
			       uint64_t real_index, uint64_t index)
{
	uint64_t src = real_index * fs->volume->blocksize;
	uint64_t dst = src + fs->volume->blocksize;
	uint64_t size = (fs->data_offset - real_index) * fs->volume->blocksize;

	if (volume_memmove(fs->volume, dst, src, size))
		return -1;

	fs->data_offset += 1;

	if (fstree_file_mark_not_sparse(n, index))
		return -1;

	return fs->volume->discard_blocks(fs->volume, real_index, 1);
}

static int remove_file_block(fstree_t *fs, tree_node_t *n,
			     uint64_t real_index, uint64_t index)
{
	uint64_t dst = real_index * fs->volume->blocksize;
	uint64_t src = dst + fs->volume->blocksize;
	uint64_t size = fs->data_offset - real_index - 1;

	if (volume_memmove(fs->volume, dst, src, size * fs->volume->blocksize))
		return -1;

	fs->data_offset -= 1;

	if (fstree_file_mark_sparse(n, index))
		return -1;

	return fs->volume->discard_blocks(fs->volume, fs->data_offset, 1);
}

static int write_partial_block(fstree_t *fs, tree_node_t *n,
			       uint64_t index, const void *data,
			       uint32_t offset, uint32_t size)
{
	uint64_t start = n->data.file.start_index + index;
	file_sparse_holes_t *it;

	for (it = n->data.file.sparse; it != NULL; it = it->next) {
		if (index >= it->index && (index - it->index) < it->count) {
			if (data == NULL || is_memory_zero(data, size))
				return 0;

			start -= n->data.file.start_index;

			if (fstree_file_move_to_end(fs, n))
				return -1;

			start += n->data.file.start_index;
			start -= index - it->index;

			if (insert_sparse_block(fs, n, start, index))
				return -1;

			break;
		}

		if (it->index < index)
			start -= it->count;
	}

	if (offset == 0 && size == fs->volume->blocksize) {
		if (!(fs->flags & FSTREE_FLAG_NO_SPARSE) &&
		    (data == NULL || is_memory_zero(data, size))) {
			start -= n->data.file.start_index;
			if (fstree_file_move_to_end(fs, n))
				return -1;
			start += n->data.file.start_index;

			return remove_file_block(fs, n, start, index);
		}

		return fs->volume->write_block(fs->volume, start, data);
	}

	if (!(fs->flags & FSTREE_FLAG_NO_SPARSE) &&
	    offset == 0 &&
	    index == (n->data.file.size / fs->volume->blocksize) &&
	    size == (n->data.file.size % fs->volume->blocksize) &&
	    (data == NULL || is_memory_zero(data, size))) {
		start -= n->data.file.start_index;
		if (fstree_file_move_to_end(fs, n))
			return -1;
		start += n->data.file.start_index;

		return remove_file_block(fs, n, start, index);
	}

	return fs->volume->write_partial_block(fs->volume, start, data,
					       offset, size);
}

int fstree_file_write(fstree_t *fs, tree_node_t *n,
		      uint64_t offset, const void *data, size_t size)
{
	uint32_t blk_offset, blk_size;
	uint64_t available, blk_index;
	int ret;

	if (size == 0)
		return 0;

	if (offset > n->data.file.size) {
		ret = fstree_file_append(fs, n, NULL,
					 offset - n->data.file.size);
		if (ret)
			return -1;
	}

	available = n->data.file.size - offset;

	if (size > available) {
		ret = fstree_file_append(fs, n, (const char *)data + available,
					 size - available);
		if (ret)
			return -1;

		size = available;
	}

	blk_index = offset / fs->volume->blocksize;
	blk_offset = offset % fs->volume->blocksize;
	blk_size = fs->volume->blocksize - blk_offset;
	blk_size = blk_size > size ? size : blk_size;

	while (size > 0) {
		ret = write_partial_block(fs, n, blk_index, data,
					  blk_offset, blk_size);
		if (ret)
			return -1;

		size -= blk_size;

		if (data != NULL)
			data = (const char *)data + blk_size;

		blk_index += 1;
		blk_offset = 0;
		blk_size = fs->volume->blocksize;
		blk_size = blk_size > size ? size : blk_size;
	}

	return 0;
}
