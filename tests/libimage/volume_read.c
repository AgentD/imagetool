/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume_read.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "test.h"
#include "volume.h"

static char dummy_buffer[31] = "AAABBBCCCDDDEEEFFFGGGHHHIIIJJJ";

static int dummy_read_partial_block(volume_t *vol, uint64_t index,
				    void *buffer, uint32_t offset,
				    uint32_t size)
{
	(void)vol;
	if (index >= 10 || offset > 3 || size > (3 - offset))
		return -1;
	memcpy(buffer, dummy_buffer + index * 3 + offset, size);
	return 0;
}

static int dummy_read_block(volume_t *vol, uint64_t index, void *buffer)
{
	return dummy_read_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static volume_t dummy = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 3,

	.min_block_count = 0,
	.max_block_count = 10,

	.read_block = dummy_read_block,
	.read_partial_block = dummy_read_partial_block,
};

int main(void)
{
	char buffer[13];
	int ret;

	memset(buffer, 0, sizeof(buffer));
	ret = volume_read(&dummy, 4, buffer, 12);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(buffer, "BBCCCDDDEEEF");

	memset(buffer, 0, sizeof(buffer));
	ret = volume_read(&dummy, 28, buffer, 2);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(buffer, "JJ");

	memset(buffer, 0, sizeof(buffer));
	ret = volume_read(&dummy, 28, buffer, 3);
	TEST_ASSERT(ret != 0);

	return EXIT_SUCCESS;
}
