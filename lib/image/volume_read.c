/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume_read.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"

int volume_read(volume_t *vol, uint64_t offset, void *data, size_t size)
{
	uint64_t blk_index = offset / vol->blocksize;
	uint32_t blk_offset = offset % vol->blocksize;
	uint32_t blk_size = vol->blocksize - blk_offset;
	int ret;

	while (size > 0) {
		blk_size = blk_size > size ? size : blk_size;

		if (blk_offset == 0 && blk_size == vol->blocksize) {
			ret = vol->read_block(vol, blk_index, data);
		} else {
			ret = vol->read_partial_block(vol, blk_index, data,
						      blk_offset, blk_size);
		}

		if (ret)
			return -1;

		size -= blk_size;
		data = (char *)data + blk_size;

		blk_index += 1;
		blk_offset = 0;
		blk_size = vol->blocksize;
	}

	return 0;
}
