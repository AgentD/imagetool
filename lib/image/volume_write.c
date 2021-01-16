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
	uint64_t blk_index = offset / vol->blocksize;
	uint32_t blk_offset = offset % vol->blocksize;
	uint32_t blk_size = vol->blocksize - blk_offset;
	int ret;

	while (size > 0) {
		blk_size = blk_size > size ? size : blk_size;

		if (blk_offset == 0 && blk_size == vol->blocksize) {
			if (data == NULL || is_sparse(data, blk_size)) {
				ret = vol->discard_blocks(vol, blk_index, 1);
			} else {
				ret = vol->write_block(vol, blk_index, data);
			}
		} else {
			ret = vol->write_partial_block(vol, blk_index, data,
						       blk_offset, blk_size);
		}

		if (ret)
			return -1;

		if (data != NULL)
			data = (const char *)data + blk_size;

		size -= blk_size;
		blk_index += 1;
		blk_offset = 0;
		blk_size = vol->blocksize;
	}

	return 0;
}
