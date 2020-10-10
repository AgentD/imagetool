/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * blocksize_adapter1.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "volume.h"
#include "test.h"

static char dummy_buffer[31] = "AAABBBCCCDDDEEEFFFGGGHHHIIIJJJ";

static int discard_sequence[10];
static int num_discarded = 0;

static int compare_ints(const void *lhs, const void *rhs)
{
	return *((const int *)lhs) - *((const int *)rhs);
}

static int dummy_read_block(volume_t *vol, uint64_t index, void *buffer)
{
	(void)vol;
	TEST_ASSERT(index < 10);
	memcpy(buffer, dummy_buffer + index * 3, 3);
	return 0;
}

static int dummy_write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	(void)vol;
	TEST_ASSERT(index < 10);
	memcpy(dummy_buffer + index * 3, buffer, 3);
	return 0;
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

static int dummy_commit(volume_t *vol)
{
	(void)vol;
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

	.read_block = dummy_read_block,
	.write_block = dummy_write_block,
	.swap_blocks = NULL,
	.discard_blocks = dummy_discard_blocks,
	.commit = dummy_commit,
	.create_sub_volume = NULL,
};

int main(void)
{
	volume_t *vol;
	char temp[8];
	int ret;

	vol = volume_blocksize_adapter_create(&dummy, 7);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(dummy.base.refcount, 2);

	TEST_EQUAL_UI(vol->blocksize, 7);
	TEST_EQUAL_UI(vol->min_block_count, 0);
	TEST_EQUAL_UI(vol->max_block_count, 4);

	/* read blocks */
	temp[7] = '\0';

	ret = vol->read_block(vol, 0, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "AAABBBC");

	ret = vol->read_block(vol, 1, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "CCDDDEE");

	ret = vol->read_block(vol, 2, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "EFFFGGG");

	ret = vol->read_block(vol, 3, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "HHHIIIJ");

	ret = vol->read_block(vol, 4, temp);
	TEST_ASSERT(ret != 0);

	/* overwrite blocks */
	ret = vol->write_block(vol, 1, "ZZZZZZZ");
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAABBBCZZZZZZZEFFFGGGHHHIIIJJJ");

	ret = vol->write_block(vol, 3, "LLLLLLLL");
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAABBBCZZZZZZZEFFFGGGLLLLLLLJJ");

	ret = vol->write_block(vol, 4, "MMMMMMM");
	TEST_ASSERT(ret != 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAABBBCZZZZZZZEFFFGGGLLLLLLLJJ");

	/* swap blocks */
	ret = vol->swap_blocks(vol, 0, 2);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "EFFFGGGZZZZZZZAAABBBCLLLLLLLJJ");

	/* discard blocks */
	ret = vol->discard_blocks(vol, 1, 2);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	ret = memcmp(dummy_buffer,
		     "EFFFGGG\0\0\0\0\0\0\0\0\0\0\0\0\0\0LLLLLLLJJ", 30);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_I(num_discarded, 4);

	qsort(discard_sequence, 4, sizeof(discard_sequence[0]), compare_ints);

	TEST_EQUAL_I(discard_sequence[0], 3);
	TEST_EQUAL_I(discard_sequence[1], 4);
	TEST_EQUAL_I(discard_sequence[2], 5);
	TEST_EQUAL_I(discard_sequence[3], 6);

	/* cleanup */
	object_drop(vol);
	TEST_EQUAL_UI(dummy.base.refcount, 1);
	return EXIT_SUCCESS;
}
