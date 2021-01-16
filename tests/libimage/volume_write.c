/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume_write.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "../test.h"
#include "volume.h"

static char dummy_buffer[31] = "AAABBBCCCDDDEEEFFFGGGHHHIIIJJJ";

static int discard_sequence[10];
static int num_discarded = 0;

static int compare_ints(const void *lhs, const void *rhs)
{
	return *((const int *)lhs) - *((const int *)rhs);
}

static int dummy_write_partial_block(volume_t *vol, uint64_t index,
				     const void *buffer, uint32_t offset,
				     uint32_t size)
{
	(void)vol;
	if (index >= 10 || offset > 3 || size > (3 - offset))
		return -1;
	if (buffer == NULL) {
		memset(dummy_buffer + index * 3 + offset, 0, size);
	} else {
		memcpy(dummy_buffer + index * 3 + offset, buffer, size);
	}
	return 0;
}

static int dummy_write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	return dummy_write_partial_block(vol, index, buffer, 0,
					 vol->blocksize);
}

static int dummy_discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	(void)vol;

	while (index < 10 && count > 0) {
		discard_sequence[num_discarded++] = index;

		memset(dummy_buffer + index * 3, 0, 3);
		++index;
		--count;
	}

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

	.write_block = dummy_write_block,
	.write_partial_block = dummy_write_partial_block,
	.discard_blocks = dummy_discard_blocks,
};

int main(void)
{
	int ret;

	ret = volume_write(&dummy, 4, "ZZZZYYYYXXXX", 12);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer, "AAABZZZZYYYYXXXXFFGGGHHHIIIJJJ");

	ret = volume_write(&dummy, 28, "PP", 2);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer, "AAABZZZZYYYYXXXXFFGGGHHHIIIJPP");

	ret = volume_write(&dummy, 4, NULL, 12);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(dummy_buffer,
		     "AAAB\0\0\0\0\0\0\0\0\0\0\0\0FFGGGHHHIIIJPP", 30);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_I(num_discarded, 3);
	qsort(discard_sequence, 3, sizeof(discard_sequence[0]), compare_ints);
	TEST_EQUAL_I(discard_sequence[0], 2);
	TEST_EQUAL_I(discard_sequence[1], 3);
	TEST_EQUAL_I(discard_sequence[2], 4);

	num_discarded = 0;
	ret = volume_write(&dummy, 4, "\0\0\0\0\0\0", 6);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(dummy_buffer,
		     "AAAB\0\0\0\0\0\0\0\0\0\0\0\0FFGGGHHHIIIJPP", 30);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(num_discarded, 1);
	TEST_EQUAL_I(discard_sequence[0], 2);

	ret = volume_write(&dummy, 28, "WWW", 3);
	TEST_ASSERT(ret != 0);

	return EXIT_SUCCESS;
}
