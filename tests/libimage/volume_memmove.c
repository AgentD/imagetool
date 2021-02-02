/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume_memmove.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "../test.h"
#include "volume.h"

static const char *initial = "AAABBBCCCDDDEEEFFFGGGHHHIIIJJJ";

static char dummy_buffer[31];

static int dummy_move_block(volume_t *vol, uint64_t src, uint64_t dst)
{
	(void)vol;
	TEST_ASSERT(src < 10);
	TEST_ASSERT(dst < 10);

	memmove(dummy_buffer + dst * 3, dummy_buffer + src * 3, 3);
	return 0;
}

static int dummy_move_block_partial(volume_t *vol, uint64_t src, uint64_t dst,
				    size_t src_offset, size_t dst_offset,
				    size_t size)
{
	(void)vol;
	TEST_ASSERT(src < 10);
	TEST_ASSERT(dst < 10);
	TEST_ASSERT(src_offset < 3);
	TEST_ASSERT(dst_offset < 3);
	TEST_ASSERT((src_offset + size) <= 3);
	TEST_ASSERT((dst_offset + size) <= 3);
	memmove(dummy_buffer + dst * 3 + dst_offset,
		dummy_buffer + src * 3 + src_offset, size);
	return 0;
}

static volume_t dummy = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 3,

	.min_block_count = 0,
	.max_block_count = 10,

	.move_block_partial = dummy_move_block_partial,
	.move_block = dummy_move_block,
};

int main(void)
{
	int ret;

	/* copy from high location to low location, no overlap */
	strcpy(dummy_buffer, initial);
	ret = volume_memmove(&dummy, 4, 19, 10);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer, "AAABGGHHHIIIJJEFFFGGGHHHIIIJJJ");

	/* copy from low location to high location, no overlap */
	strcpy(dummy_buffer, initial);
	ret = volume_memmove(&dummy, 19, 4, 10);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer, "AAABBBCCCDDDEEEFFFGBBCCCDDDEEJ");

	/* copy from high location to low location with overlap */
	strcpy(dummy_buffer, initial);
	ret = volume_memmove(&dummy, 4, 10, 10);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer, "AAABDDEEEFFFGGEFFFGGGHHHIIIJJJ");

	/* copy from low location to high location with overlap */
	strcpy(dummy_buffer, initial);
	ret = volume_memmove(&dummy, 10, 4, 10);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer, "AAABBBCCCDBBCCCDDDEEGHHHIIIJJJ");

	return EXIT_SUCCESS;
}
