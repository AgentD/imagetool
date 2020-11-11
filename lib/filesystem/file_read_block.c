/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_read_block.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <string.h>

int fstree_file_read_block(fstree_t *fs, tree_node_t *n,
			   uint64_t index, void *data)
{
	file_sparse_holes_t *sparse;
	uint64_t start, end, count;
	uint32_t tail_size;

	count = n->data.file.size / fs->volume->blocksize;
	tail_size = n->data.file.size % fs->volume->blocksize;

	/* is it in a sparse region? */
	sparse = n->data.file.sparse;

	while (sparse != NULL) {
		start = sparse->index;
		end = sparse->index + sparse->count;

		if (index >= start && index < end)
			goto out_sparse;

		sparse = sparse->next;
	}

	if (index > count || (index == count && tail_size == 0))
		goto out_sparse;

	/* read block or tail end data */
	if (index < count) {
		start = n->data.file.start_index + index;

		sparse = n->data.file.sparse;

		while (sparse != NULL) {
			if (sparse->index < index)
				start -= sparse->count;

			sparse = sparse->next;
		}

		if (fs->volume->read_block(fs->volume, start, data))
			return -1;
	} else {
		start = n->data.file.tail_index;

		if (fs->volume->read_block(fs->volume, start, data))
			return -1;

		if (n->data.file.tail_offset > 0) {
			memmove(data, (char *)data + n->data.file.tail_offset,
				tail_size);
		}

		memset((char *)data + tail_size, 0,
		       fs->volume->blocksize - tail_size);
	}

	return 0;
out_sparse:
	memset(data, 0, fs->volume->blocksize);
	return 0;
}
