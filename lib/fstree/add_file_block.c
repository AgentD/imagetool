/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * add_file_block.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <stdlib.h>
#include <errno.h>

int fstree_add_file_block(tree_node_t *n, uint64_t on_disk_location,
			  uint64_t in_file_location, size_t size)
{
	size_t i, new_count;
	void *new;

	if (n->type != TREE_NODE_FILE) {
		errno = EINVAL;
		return -1;
	}

	if (n->data.file.num_blocks >= n->data.file.max_blocks) {
		if (n->data.file.max_blocks > 0) {
			new_count = n->data.file.max_blocks * 2;
		} else {
			new_count = 4;
		}

		new = realloc(n->data.file.blocks,
			      sizeof(n->data.file.blocks[0]) * new_count);
		if (new == NULL)
			return -1;

		n->data.file.blocks = new;
		n->data.file.max_blocks = new_count;
	}

	i = n->data.file.num_blocks++;

	n->data.file.blocks[i].disk_location = on_disk_location;
	n->data.file.blocks[i].file_location = in_file_location;
	n->data.file.blocks[i].size = size;
	return 0;
}
