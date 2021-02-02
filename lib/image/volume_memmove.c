/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume_memmove.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"

static int copy_forward(volume_t *vol, uint64_t dst, uint64_t src,
			uint64_t size)
{
	while (size > 0) {
		uint64_t src_idx = src / vol->blocksize;
		uint32_t src_offset = src % vol->blocksize;
		uint32_t src_diff = vol->blocksize - src_offset;

		uint64_t dst_idx = dst / vol->blocksize;
		uint32_t dst_offset = dst % vol->blocksize;
		uint32_t dst_diff = vol->blocksize - dst_offset;

		uint32_t diff = src_diff < dst_diff ? src_diff : dst_diff;
		int ret;

		if (diff > size)
			diff = size;

		if (src_offset == 0 && dst_offset == 0 &&
		    diff == vol->blocksize) {
			ret = vol->move_block(vol, src_idx, dst_idx);
		} else {
			ret = vol->move_block_partial(vol, src_idx, dst_idx,
						      src_offset, dst_offset,
						      diff);
		}

		if (ret != 0)
			return -1;

		src += diff;
		dst += diff;
		size -= diff;
	}

	return 0;
}

static int copy_backward(volume_t *vol, uint64_t dst,
			 uint64_t src, uint64_t size)
{
	src += size - 1;
	dst += size - 1;

	while (size > 0) {
		uint64_t src_idx = src / vol->blocksize;
		uint32_t src_diff = (src % vol->blocksize) + 1;
		uint32_t src_offset = 0;

		uint64_t dst_idx = dst / vol->blocksize;
		uint32_t dst_diff = (dst % vol->blocksize) + 1;
		uint32_t dst_offset = 0;

		uint32_t diff = dst_diff < src_diff ? dst_diff : src_diff;
		int ret;

		if (diff > size)
			diff = size;

		if (src_diff > diff)
			src_offset += src_diff - diff;

		if (dst_diff > diff)
			dst_offset += dst_diff - diff;

		if (src_offset == 0 && dst_offset == 0 &&
		    diff == vol->blocksize) {
			ret = vol->move_block(vol, src_idx, dst_idx);
		} else {
			ret = vol->move_block_partial(vol, src_idx, dst_idx,
						      src_offset, dst_offset,
						      diff);
		}

		if (ret != 0)
			return -1;

		src -= diff;
		dst -= diff;
		size -= diff;
	}

	return 0;
}

int volume_memmove(volume_t *vol, uint64_t dst, uint64_t src, uint64_t size)
{
	if (src == dst || size == 0)
		return 0;

	if (src < dst && ((src + size - 1) >= dst))
		return copy_backward(vol, dst, src, size);

	return copy_forward(vol, dst, src, size);
}
