/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume_write.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"

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
			ret = vol->write_block(vol, index, data);
		} else {
			ret = vol->write_partial_block(vol, index,
						       data, blk_offset, diff);
		}

		if (ret)
			return -1;

		offset += diff;
		size -= diff;
		data = (const char *)data + diff;
	}

	return 0;
}
