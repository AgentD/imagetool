/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_move_to_end.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

int fstree_file_move_to_end(fstree_t *fs, tree_node_t *n)
{
	uint64_t phys_size, blk_count, src, dst, size;
	tree_node_t *fit;

	/* determine the on-disk size of the file */
	phys_size = fstree_file_physical_size(fs, n);
	if (phys_size == 0) {
		n->data.file.start_index = fs->data_offset;
		return 0;
	}

	blk_count = phys_size / fs->volume->blocksize;
	if (phys_size % fs->volume->blocksize)
		blk_count += 1;

	if (blk_count >= (fs->data_offset - n->data.file.start_index))
		return 0;

	/* move the data to the end */
	src = n->data.file.start_index * fs->volume->blocksize;
	dst = fs->data_offset * fs->volume->blocksize;
	size = blk_count * fs->volume->blocksize;

	if (volume_memmove(fs->volume, dst, src, size))
		return -1;

	/* close the gap */
	dst = src;
	src += size;
	size = fs->data_offset - n->data.file.start_index - blk_count;
	size *= fs->volume->blocksize;

	if (volume_memmove(fs->volume, dst, src, size))
		return -1;

	if (fs->volume->discard_blocks(fs->volume, fs->data_offset, blk_count))
		return -1;

	/* update file accounting */
	src = n->data.file.start_index;
	n->data.file.start_index = fs->data_offset;

	fit = fs->nodes_by_type[TREE_NODE_FILE];

	for (; fit != NULL; fit = fit->next_by_type) {
		if (fstree_file_physical_size(fs, fit) == 0)
			continue;

		if (fit->data.file.start_index >= src)
			fit->data.file.start_index -= blk_count;
	}

	return 0;
}
