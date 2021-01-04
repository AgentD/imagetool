/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"

int volume_move_blocks(volume_t *vol, uint64_t src, uint64_t dst,
		       uint64_t count, int mode)
{
	uint64_t i, j;

	if (src == dst || count == 0)
		return 0;

	if (src < dst && ((src + count - 1) >= dst)) {
		for (i = 0; i < count; ++i) {
			j = count - 1 - i;

			if (vol->move_block(vol, src + j, dst + j, mode))
				return -1;
		}
	} else {
		for (i = 0; i < count; ++i) {
			if (vol->move_block(vol, src + i, dst + i, mode))
				return -1;
		}
	}

	return 0;
}
