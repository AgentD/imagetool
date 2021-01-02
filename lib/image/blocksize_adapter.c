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

	bool cache_dirty;
	bool have_cached_block;

	uint32_t offset;

	uint64_t cached_index;
	uint8_t scratch[];
} adapter_t;

static int flush_cached_block(adapter_t *adapter)
{
	bool is_zero_block;
	size_t i;
	int ret;

	if (!adapter->have_cached_block || !adapter->cache_dirty)
		return 0;

	is_zero_block = true;

	for (i = 0; i < adapter->wrapped->blocksize; ++i) {
		if (adapter->scratch[i] != 0) {
			is_zero_block = false;
			break;
		}
	}

	if (is_zero_block) {
		ret = adapter->wrapped->discard_blocks(adapter->wrapped,
						       adapter->cached_index,
						       1);
	} else {
		ret = adapter->wrapped->write_block(adapter->wrapped,
						    adapter->cached_index,
						    adapter->scratch);
	}

	if (ret)
		return ret;

	adapter->cache_dirty = false;
	return 0;
}

static int cache_block(adapter_t *adapter, uint64_t index)
{
	if (adapter->have_cached_block && adapter->cached_index == index)
		return 0;

	if (flush_cached_block(adapter))
		return -1;

	if (adapter->wrapped->read_block(adapter->wrapped, index,
					 adapter->scratch)) {
		return -1;
	}

	adapter->have_cached_block = true;
	adapter->cache_dirty = false;
	adapter->cached_index = index;
	return 0;
}

static int read_block(volume_t *vol, uint64_t index, void *buffer)
{
	adapter_t *adapter = (adapter_t *)vol;
	volume_t *wrapped = adapter->wrapped;
	uint64_t offset;
	size_t size;

	if (index >= vol->max_block_count) {
		fputs("Out of bounds read attempted on block size adapter.\n",
		      stderr);
		return -1;
	}

	offset = adapter->offset + index * vol->blocksize;
	size = vol->blocksize;

	while (size > 0) {
		uint64_t target_idx = offset / wrapped->blocksize;
		size_t target_offset = offset % wrapped->blocksize;
		size_t target_size = wrapped->blocksize - target_offset;

		if (target_size > size)
			target_size = size;

		if (target_offset == 0 && target_size == wrapped->blocksize) {
			wrapped->read_block(wrapped, target_idx, buffer);
		} else {
			if (cache_block(adapter, target_idx))
				return -1;

			memcpy(buffer, adapter->scratch + target_offset,
			       target_size);
		}

		offset += target_size;
		size -= target_size;
		buffer = (char *)buffer + target_size;
	}

	return 0;
}

static int write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	adapter_t *adapter = (adapter_t *)vol;
	volume_t *wrapped = adapter->wrapped;
	uint64_t offset;
	size_t size;

	if (index >= vol->max_block_count) {
		fputs("Out of bounds write attempted on block size adapter.\n",
		      stderr);
		return -1;
	}

	offset = adapter->offset + index * vol->blocksize;
	size = vol->blocksize;

	while (size > 0) {
		uint64_t target_idx = offset / wrapped->blocksize;
		size_t target_offset = offset % wrapped->blocksize;
		size_t target_size = wrapped->blocksize - target_offset;

		if (target_size > size)
			target_size = size;

		if (target_offset == 0 && target_size == wrapped->blocksize) {
			wrapped->write_block(wrapped, target_idx, buffer);
		} else {
			if (cache_block(adapter, target_idx))
				return -1;

			memcpy(adapter->scratch + target_offset, buffer,
			       target_size);
			adapter->cache_dirty = true;
		}

		offset += target_size;
		size -= target_size;
		buffer = (char *)buffer + target_size;
	}

	return 0;
}

static int move_block(volume_t *vol, uint64_t src, uint64_t dst, int flags)
{
	adapter_t *adapter = (adapter_t *)vol;
	void *src_buf, *dst_buf;

	src_buf = adapter->scratch + adapter->wrapped->blocksize;
	dst_buf = (char *)src_buf + vol->blocksize;

	if (read_block(vol, src, src_buf))
		return -1;

	if (flags & (MOVE_SWAP | MOVE_ERASE_SOURCE)) {
		if (flags & MOVE_ERASE_SOURCE) {
			memset(dst_buf, 0, vol->blocksize);
		} else {
			if (read_block(vol, dst, dst_buf))
				return -1;
		}

		if (write_block(vol, src, dst_buf))
			return -1;
	}

	if (write_block(vol, dst, src_buf))
		return -1;

	return 0;
}

static int discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	adapter_t *adapter = (adapter_t *)vol;
	volume_t *wrapped = adapter->wrapped;
	uint64_t offset, size;

	if (index >= vol->max_block_count)
		return 0;

	if (count > (vol->max_block_count - index))
		count = vol->max_block_count - index;

	offset = adapter->offset + index * vol->blocksize;
	size = count * vol->blocksize;

	while (size > 0) {
		uint64_t target_idx = offset / wrapped->blocksize;
		size_t target_offset = offset % wrapped->blocksize;
		size_t target_size = wrapped->blocksize - target_offset;

		if (target_size > size)
			target_size = size;

		if (target_offset == 0 && target_size == wrapped->blocksize) {
			wrapped->discard_blocks(wrapped, target_idx, 1);
		} else {
			if (cache_block(adapter, target_idx))
				return -1;

			memset(adapter->scratch + target_offset, 0,
			       target_size);
			adapter->cache_dirty = true;
		}

		offset += target_size;
		size -= target_size;
	}

	return 0;
}

static int commit(volume_t *vol)
{
	adapter_t *adapter = (adapter_t *)vol;

	if (flush_cached_block(adapter))
		return -1;

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
	size_t scratch_size = vol->blocksize + blocksize * 2;
	adapter_t *adapter = calloc(1, sizeof(*adapter) + scratch_size);
	uint64_t count;

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
	((volume_t *)adapter)->write_block = write_block;
	((volume_t *)adapter)->move_block = move_block;
	((volume_t *)adapter)->discard_blocks = discard_blocks;
	((volume_t *)adapter)->commit = commit;
	return (volume_t *)adapter;
}
