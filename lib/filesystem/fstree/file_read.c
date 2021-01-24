/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_read.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <string.h>

static int read_partial_block(fstree_t *fs, tree_node_t *n,
			      uint64_t index, void *data,
			      uint32_t offset, uint32_t size)
{
	file_sparse_holes_t *sparse = n->data.file.sparse;
	uint64_t start = n->data.file.start_index + index;

	while (sparse != NULL) {
		if (index >= sparse->index &&
		    (index - sparse->index) < sparse->count) {
			memset(data, 0, size);
			return 0;
		}

		if (sparse->index < index)
			start -= sparse->count;

		sparse = sparse->next;
	}

	if (offset == 0 && size == fs->volume->blocksize)
		return fs->volume->read_block(fs->volume, start, data);

	return fs->volume->read_partial_block(fs->volume, start, data,
					      offset, size);
}

int fstree_file_read(fstree_t *fs, tree_node_t *n,
		     uint64_t offset, void *data, size_t size)
{
	uint32_t blk_offset, blk_size;
	uint64_t available, blk_index;
	int ret;

	if (size == 0)
		return 0;

	if (offset >= n->data.file.size) {
		memset(data, 0, size);
		return 0;
	}

	available = n->data.file.size - offset;

	if (size > available) {
		memset((char *)data + available, 0, size - available);
		size = available;
	}

	blk_index = offset / fs->volume->blocksize;
	blk_offset = offset % fs->volume->blocksize;
	blk_size = fs->volume->blocksize - blk_offset;
	blk_size = blk_size > size ? size : blk_size;

	while (size > 0) {
		ret = read_partial_block(fs, n, blk_index, data,
					 blk_offset, blk_size);
		if (ret)
			return -1;

		size -= blk_size;
		data = (char *)data + blk_size;

		blk_index += 1;
		blk_offset = 0;
		blk_size = fs->volume->blocksize;
		blk_size = blk_size > size ? size : blk_size;
	}

	return 0;
}
