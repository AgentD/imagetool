/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume_write.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"

static bool is_sparse(const unsigned char *data, size_t size)
{
	while (size--) {
		if (*(data++))
			return false;
	}

	return true;
}

int volume_write(volume_t *vol, uint64_t offset, const void *data, size_t size)
{
	uint32_t blk_offset;
	uint64_t index;
	size_t diff;
	int ret;

	while (size > 0) {
		index = offset / vol->blocksize;
		blk_offset = offset % vol->blocksize;
		diff = vol->blocksize - blk_offset;

		if (diff > size)
			diff = size;

		if (blk_offset == 0 && diff == vol->blocksize) {
			if (data == NULL || is_sparse(data, diff)) {
				ret = vol->discard_blocks(vol, index, 1);
			} else {
				ret = vol->write_block(vol, index, data);
			}
		} else {
			ret = vol->write_partial_block(vol, index,
						       data, blk_offset, diff);
		}

		if (ret)
			return -1;

		offset += diff;
		size -= diff;

		if (data != NULL)
			data = (const char *)data + diff;
	}

	return 0;
}
