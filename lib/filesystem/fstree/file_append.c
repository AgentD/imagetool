/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_append.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

static int append_to_tail(fstree_t *fs, tree_node_t *n, uint64_t tail_index,
			  uint32_t tail_size, const void *data, size_t size)
{
	file_sparse_holes_t *it = n->data.file.sparse, *prev = NULL;
	uint64_t real_index = n->data.file.start_index + tail_index;
	int ret;

	while (it != NULL && it->count < (tail_index - it->index)) {
		real_index -= it->count;
		prev = it;
		it = it->next;
	}

	if (it != NULL) {
		if (data == NULL || is_memory_zero(data, size))
			return 0;

		if (fstree_file_move_to_end(fs, n))
			return -1;

		ret = fs->volume->write_partial_block(fs->volume,
						      fs->data_offset,
						      NULL, 0, tail_size);
		if (ret)
			return -1;

		real_index = fs->data_offset++;
		it->count -= 1;

		if (it->count == 0) {
			if (prev == NULL) {
				n->data.file.sparse = it->next;
			} else {
				prev->next = it->next;
			}
			free(it);
		}
	}

	return fs->volume->write_partial_block(fs->volume, real_index, data,
					       tail_size, size);
}

static int append_block(fstree_t *fs, tree_node_t *n,
			const void *data, size_t size)
{
	uint32_t diff;
	int ret;

	if (fstree_file_move_to_end(fs, n))
		return -1;

	if (size == fs->volume->blocksize) {
		if (fs->volume->write_block(fs->volume, fs->data_offset, data))
			return -1;
	} else {
		ret = fs->volume->write_partial_block(fs->volume,
						      fs->data_offset, data,
						      0, size);
		if (ret)
			return -1;

		diff = fs->volume->blocksize - size;
		ret = fs->volume->write_partial_block(fs->volume,
						      fs->data_offset, NULL,
						      size, diff);
		if (ret)
			return -1;
	}

	fs->data_offset += 1;
	return 0;
}

int fstree_file_append(fstree_t *fs, tree_node_t *n,
		       const void *data, size_t size)
{
	uint64_t tail_index = n->data.file.size / fs->volume->blocksize;
	uint32_t tail_size = n->data.file.size % fs->volume->blocksize;

	while (size > 0) {
		uint32_t diff = fs->volume->blocksize - tail_size;
		diff = diff > size ? size : diff;

		if (tail_size > 0) {
			if (append_to_tail(fs, n, tail_index, tail_size,
					   data, diff))
				return -1;
		} else if (data == NULL || is_memory_zero(data, diff)) {
			if (fstree_file_mark_sparse(n, tail_index))
				return -1;
		} else {
			if (append_block(fs, n, data, diff))
				return -1;
		}

		if (data != NULL)
			data = (const char *)data + diff;

		size -= diff;
		n->data.file.size += diff;
		tail_index += 1;
		tail_size = 0;
	}

	return 0;
}
