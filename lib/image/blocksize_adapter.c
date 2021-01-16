/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * blocksize_adapter.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
	volume_t base;

	volume_t *wrapped;
	uint32_t offset;

	uint8_t scratch[];
} adapter_t;

static int check_bounds(volume_t *vol, uint64_t index,
			uint32_t blk_offset, uint32_t size)
{
	if (index >= vol->max_block_count || blk_offset > vol->blocksize ||
	    size > (vol->blocksize - blk_offset)) {
		fputs("Out of bounds access on block size adapter.\n", stderr);
		return -1;
	}
	return 0;
}

static int read_partial_block(volume_t *vol, uint64_t index,
			      void *buffer, uint32_t blk_offset, uint32_t size)
{
	adapter_t *adapter = (adapter_t *)vol;
	uint64_t offset;

	if (check_bounds(vol, index, blk_offset, size))
		return -1;

	offset = adapter->offset + index * vol->blocksize + blk_offset;

	return volume_read(adapter->wrapped, offset, buffer, size);
}

static int read_block(volume_t *vol, uint64_t index, void *buffer)
{
	return read_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static int write_partial_block(volume_t *vol, uint64_t index,
			       const void *buffer, uint32_t blk_offset,
			       uint32_t size)
{
	adapter_t *adapter = (adapter_t *)vol;
	uint64_t offset;

	if (check_bounds(vol, index, blk_offset, size))
		return -1;

	offset = adapter->offset + index * vol->blocksize + blk_offset;

	return volume_write(adapter->wrapped, offset, buffer, size);
}

static int discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	adapter_t *adapter = (adapter_t *)vol;
	uint64_t offset, size;

	if (index >= vol->max_block_count)
		return 0;

	if (count > (vol->max_block_count - index))
		count = vol->max_block_count - index;

	offset = adapter->offset + index * vol->blocksize;
	size = count * vol->blocksize;

	return volume_write(adapter->wrapped, offset, NULL, size);
}

static int write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	return write_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static int move_block(volume_t *vol, uint64_t src, uint64_t dst, int mode)
{
	adapter_t *adapter = (adapter_t *)vol;
	int ret;

	if (mode == MOVE_SWAP && read_block(vol, dst, adapter->scratch) != 0)
		return -1;

	ret = volume_memmove(adapter->wrapped,
			     adapter->offset + dst * vol->blocksize,
			     adapter->offset + src * vol->blocksize,
			     vol->blocksize);
	if (ret)
		return -1;

	if (mode == MOVE_SWAP)
		return write_block(vol, src, adapter->scratch);

	if (mode == MOVE_ERASE_SOURCE)
		return write_block(vol, src, NULL);

	return 0;
}

static int move_block_partial(volume_t *vol, uint64_t src, uint64_t dst,
			      size_t src_offset, size_t dst_offset,
			      size_t size)
{
	adapter_t *adapter = (adapter_t *)vol;

	if (read_partial_block(vol, src, adapter->scratch,
			       src_offset, size)) {
		return -1;
	}

	return write_partial_block(vol, dst, adapter->scratch,
				   dst_offset, size);
}

static int commit(volume_t *vol)
{
	adapter_t *adapter = (adapter_t *)vol;

	return adapter->wrapped->commit(adapter->wrapped);
}

static void destroy(object_t *base)
{
	adapter_t *adapter = (adapter_t *)base;

	object_drop(adapter->wrapped);
	free(base);
}

volume_t *volume_blocksize_adapter_create(volume_t *vol, uint32_t blocksize,
					  uint32_t offset)
{
	adapter_t *adapter;
	uint64_t count;

	adapter = calloc(1, sizeof(*adapter) + blocksize);
	if (adapter == NULL) {
		perror("creating block size adapter volume");
		return NULL;
	}

	count = (vol->max_block_count * vol->blocksize - offset) / blocksize;

	adapter->offset = offset;
	adapter->wrapped = object_grab(vol);

	((object_t *)adapter)->refcount = 1;
	((object_t *)adapter)->destroy = destroy;
	((volume_t *)adapter)->blocksize = blocksize;
	((volume_t *)adapter)->min_block_count = 0;
	((volume_t *)adapter)->max_block_count = count;
	((volume_t *)adapter)->read_block = read_block;
	((volume_t *)adapter)->read_partial_block = read_partial_block;
	((volume_t *)adapter)->write_block = write_block;
	((volume_t *)adapter)->write_partial_block = write_partial_block;
	((volume_t *)adapter)->move_block = move_block;
	((volume_t *)adapter)->move_block_partial = move_block_partial;
	((volume_t *)adapter)->discard_blocks = discard_blocks;
	((volume_t *)adapter)->commit = commit;
	return (volume_t *)adapter;
}
