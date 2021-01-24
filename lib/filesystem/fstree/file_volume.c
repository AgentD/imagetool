/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_volume.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"
#include "fstree.h"

#include <stdlib.h>
#include <stdio.h>


typedef struct {
	volume_t base;

	fstree_t *fstree;
	tree_node_t *node;

	uint8_t scratch[];
} file_volume_t;


static int check_bounds(volume_t *vol, uint64_t index,
			uint32_t blk_offset, uint32_t size)
{
	file_volume_t *fsvol = (file_volume_t *)vol;
	char *path;

	if (index >= vol->max_block_count || blk_offset > vol->blocksize ||
	    size > (vol->blocksize - blk_offset)) {
		path = fstree_get_path(fsvol->node);
		fprintf(stderr,
			"%s: out-of-bounds access on file based volume.\n",
			path);
		free(path);
		return -1;
	}

	return 0;
}


static int fsvol_read_partial_block(volume_t *vol, uint64_t index,
				    void *buffer, uint32_t offset,
				    uint32_t size)
{
	file_volume_t *fsvol = (file_volume_t *)vol;

	if (check_bounds(vol, index, offset, size))
		return -1;

	return fstree_file_read(fsvol->fstree, fsvol->node,
				index * vol->blocksize + offset,
				buffer, size);
}

static int fsvol_write_partial_block(volume_t *vol, uint64_t index,
				     const void *buffer, uint32_t offset,
				     uint32_t size)
{
	file_volume_t *fsvol = (file_volume_t *)vol;

	if (check_bounds(vol, index, offset, size))
		return -1;

	return fstree_file_write(fsvol->fstree, fsvol->node,
				 index * vol->blocksize + offset,
				 buffer, size);
}

static int fsvol_read_block(volume_t *vol, uint64_t index, void *buffer)
{
	return fsvol_read_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static int fsvol_write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	return fsvol_write_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static int fsvol_discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	file_volume_t *fsvol = (file_volume_t *)vol;
	uint64_t blk_count;
	int ret;

	blk_count = fsvol->node->data.file.size / vol->blocksize;
	if (fsvol->node->data.file.size % vol->blocksize)
		blk_count += 1;

	if (index >= blk_count)
		return 0;

	if (count < (blk_count - index)) {
		return fstree_file_write(fsvol->fstree, fsvol->node,
					 index * vol->blocksize, NULL,
					 count * vol->blocksize);
	}

	ret = fstree_file_truncate(fsvol->fstree, fsvol->node,
				   index * vol->blocksize);
	if (ret)
		return -1;

	if (index < vol->min_block_count) {
		ret = fstree_file_truncate(fsvol->fstree, fsvol->node,
					   vol->min_block_count *
					   vol->blocksize);
		if (ret)
			return -1;
	}

	return 0;
}

static int fsvol_move_block(volume_t *vol, uint64_t src, uint64_t dst, int mode)
{
	file_volume_t *fsvol = (file_volume_t *)vol;

	if (fsvol_read_block(vol, src, fsvol->scratch))
		return -1;

	switch (mode) {
	case MOVE_SWAP:
		if (fsvol_read_block(vol, dst,
				     fsvol->scratch + vol->blocksize)) {
			return -1;
		}
		if (fsvol_write_block(vol, src,
				      fsvol->scratch + vol->blocksize)) {
			return -1;
		}
		break;
	case MOVE_ERASE_SOURCE:
		if (fsvol_discard_blocks(vol, src, 1))
			return -1;
		break;
	default:
		break;
	}

	return fsvol_write_block(vol, dst, fsvol->scratch);
}

static int fsvol_move_block_partial(volume_t *vol, uint64_t src, uint64_t dst,
				    size_t src_offset, size_t dst_offset,
				    size_t size)
{
	file_volume_t *fsvol = (file_volume_t *)vol;

	if (fsvol_read_partial_block(vol, src, fsvol->scratch,
				     src_offset, size)) {
		return -1;
	}

	return fsvol_write_partial_block(vol, dst, fsvol->scratch,
					 dst_offset, size);
}

static int fsvol_commit(volume_t *vol)
{
	(void)vol;
	return 0;
}

static void fsvol_destroy(object_t *obj)
{
	file_volume_t *fsvol = (file_volume_t *)obj;

	object_drop(fsvol->fstree);
	object_drop(fsvol->node);
	free(obj);
}

volume_t *fstree_file_volume_create(fstree_t *fs, tree_node_t *n,
				    uint32_t blocksize, uint64_t min_size,
				    uint64_t max_size)
{
	file_volume_t *fsvol = calloc(1, sizeof(*fsvol) + 2 * blocksize);
	volume_t *vol = (volume_t *)fsvol;
	object_t *obj = (object_t *)vol;
	uint64_t blk_used;
	int ret;

	vol->blocksize = blocksize;
	vol->max_block_count = max_size / blocksize;
	vol->min_block_count = min_size / blocksize;
	if (min_size % blocksize)
		vol->min_block_count += 1;

	blk_used = n->data.file.size / blocksize;
	if (n->data.file.size % blocksize)
		blk_used += 1;

	if (vol->min_block_count > 0 && blk_used < vol->min_block_count) {
		ret = fstree_file_truncate(fs, n,
					   vol->min_block_count * blocksize);
		if (ret) {
			free(fsvol);
			return NULL;
		}
	}

	vol->read_block = fsvol_read_block;
	vol->read_partial_block = fsvol_read_partial_block;
	vol->write_block = fsvol_write_block;
	vol->write_partial_block = fsvol_write_partial_block;
	vol->move_block = fsvol_move_block;
	vol->move_block_partial = fsvol_move_block_partial;
	vol->discard_blocks = fsvol_discard_blocks;
	vol->commit = fsvol_commit;
	obj->refcount = 1;
	obj->destroy = fsvol_destroy;
	fsvol->fstree = object_grab(fs);
	fsvol->node = n;
	return vol;
}
