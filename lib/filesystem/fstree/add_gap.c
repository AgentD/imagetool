/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * add_gap.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"
#include "volume.h"

int fstree_add_gap(fstree_t *fs, uint64_t index, uint64_t size)
{
	uint64_t src, dst, totalsz, count;
	tree_node_t *fit;

	if (size == 0)
		return 0;

	if (size % fs->volume->blocksize)
		size += fs->volume->blocksize - size % fs->volume->blocksize;

	count = size / fs->volume->blocksize;

	if (index < fs->data_offset) {
		src = index * fs->volume->blocksize;
		dst = src + size;
		totalsz = (fs->data_offset - index) * fs->volume->blocksize;

		if (volume_memmove(fs->volume, dst, src, totalsz))
			return -1;

		fit = fs->nodes_by_type[TREE_NODE_FILE];

		while (fit != NULL) {
			if (fit->data.file.start_index >= index)
				fit->data.file.start_index += count;

			fit = fit->next_by_type;
		}

		fs->data_offset += count;
	} else {
		fs->data_offset = index + count;
	}

	return fs->volume->discard_blocks(fs->volume, index, count);
}
