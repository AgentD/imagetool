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
	file_sparse_holes_t *sparse;
	uint64_t start;

	start = n->data.file.start_index + index;

	/* is it in a sparse region? */
	sparse = n->data.file.sparse;

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

	/* read data block or tail end */
	if (index == (n->data.file.size / fs->volume->blocksize)) {
		start = n->data.file.tail_index;
		offset += n->data.file.tail_offset;
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
	uint64_t available, index;
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

	while (size > 0) {
		index = offset / fs->volume->blocksize;
		blk_offset = offset % fs->volume->blocksize;
		blk_size = fs->volume->blocksize - blk_offset;

		if (blk_size > size)
			blk_size = size;

		ret = read_partial_block(fs, n, index, data,
					 blk_offset, blk_size);
		if (ret)
			return -1;

		size -= blk_size;
		offset += blk_size;
		data = (char *)data + blk_size;
	}

	return 0;
}
