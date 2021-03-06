/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * blocksize_adapter3.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "test.h"
#include "volume.h"

static char dummy_buffer[31] = "AAABBBCCCDDDEEEFFFGGGHHHIIIJJJ";

static int discard_sequence[10];
static int num_discarded = 0;

static int compare_ints(const void *lhs, const void *rhs)
{
	return *((const int *)lhs) - *((const int *)rhs);
}

static int dummy_read_partial_block(volume_t *vol, uint64_t index,
				    void *buffer, uint32_t offset,
				    uint32_t size)
{
	(void)vol;
	TEST_ASSERT(index < 10);
	TEST_ASSERT(offset <= 3);
	TEST_ASSERT(size <= (3 - offset));
	memcpy(buffer, dummy_buffer + index * 3 + offset, size);
	return 0;
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
	if (buffer == NULL) {
		memset(dummy_buffer + index * 3, 0, 3);
	} else {
		memcpy(dummy_buffer + index * 3, buffer, 3);
	}
	return 0;
}

static int dummy_write_partial_block(volume_t *vol, uint64_t index,
				     const void *buffer, uint32_t offset,
				     uint32_t size)
{
	(void)vol;
	TEST_ASSERT(index < 10);
	TEST_ASSERT(offset <= 3);
	TEST_ASSERT(size <= (3 - offset));
	if (buffer == NULL) {
		memset(dummy_buffer + index * 3 + offset, 0, size);
	} else {
		memcpy(dummy_buffer + index * 3 + offset, buffer, size);
	}
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

static int dummy_commit(volume_t *vol)
{
	(void)vol;
	return 0;
}

static uint64_t dummy_get_min_block_count(volume_t *vol)
{
	(void)vol;
	return 0;
}

static uint64_t dummy_get_max_block_count(volume_t *vol)
{
	(void)vol;
	return 10;
}

static volume_t dummy = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 3,

	.get_min_block_count = dummy_get_min_block_count,
	.get_max_block_count = dummy_get_max_block_count,
	.read_block = dummy_read_block,
	.read_partial_block = dummy_read_partial_block,
	.write_block = dummy_write_block,
	.write_partial_block = dummy_write_partial_block,
	.move_block = dummy_move_block,
	.move_block_partial = dummy_move_block_partial,
	.discard_blocks = dummy_discard_blocks,
	.commit = dummy_commit,
};

int main(void)
{
	volume_t *vol;
	char temp[8];
	int ret;

	vol = volume_blocksize_adapter_create(&dummy, 7, 2);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(dummy.base.refcount, 2);

	TEST_EQUAL_UI(vol->blocksize, 7);
	TEST_EQUAL_UI(vol->get_min_block_count(vol), 0);
	TEST_EQUAL_UI(vol->get_max_block_count(vol), 4);

	/* read blocks */
	temp[7] = '\0';

	ret = vol->read_block(vol, 0, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "ABBBCCC");

	ret = vol->read_block(vol, 1, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "DDDEEEF");

	ret = vol->read_block(vol, 2, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "FFGGGHH");

	ret = vol->read_block(vol, 3, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "HIIIJJJ");

	ret = vol->read_block(vol, 4, temp);
	TEST_ASSERT(ret != 0);

	/* overwrite blocks */
	ret = vol->write_block(vol, 1, "ZZZZZZZ");
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAABBBCCCZZZZZZZFFGGGHHHIIIJJJ");

	ret = vol->write_block(vol, 3, "LLLLLLLL");
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAABBBCCCZZZZZZZFFGGGHHLLLLLLL");

	ret = vol->write_block(vol, 4, "MMMMMMM");
	TEST_ASSERT(ret != 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAABBBCCCZZZZZZZFFGGGHHLLLLLLL");

	/* discard blocks */
	ret = vol->discard_blocks(vol, 1, 2);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	ret = memcmp(dummy_buffer,
		     "AAABBBCCC\0\0\0\0\0\0\0\0\0\0\0\0\0\0LLLLLLL", 30);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_I(num_discarded, 4);

	qsort(discard_sequence, 4, sizeof(discard_sequence[0]), compare_ints);

	TEST_EQUAL_I(discard_sequence[0], 3);
	TEST_EQUAL_I(discard_sequence[1], 4);
	TEST_EQUAL_I(discard_sequence[2], 5);
	TEST_EQUAL_I(discard_sequence[3], 6);

	/* partial read */
	memset(temp, 0, sizeof(temp));
	ret = vol->read_partial_block(vol, 0, temp, 0, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "ABB");

	memset(temp, 0, sizeof(temp));
	ret = vol->read_partial_block(vol, 0, temp, 3, 2);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "BC");

	memset(temp, 0, sizeof(temp));
	ret = vol->read_partial_block(vol, 0, temp, 4, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "CCC");

	memset(temp, 0, sizeof(temp));
	ret = vol->read_partial_block(vol, 0, temp, 0, 7);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "ABBBCCC");

	ret = vol->read_partial_block(vol, 0, temp, 4, 5);
	TEST_ASSERT(ret != 0);

	/* partial write */
	ret = vol->write_partial_block(vol, 0, "ZZZ", 1, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer, "AAAZZZCCC");

	ret = vol->write_partial_block(vol, 0, "XX", 5, 2);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer, "AAAZZZCXX");

	ret = vol->write_partial_block(vol, 0, "YYY", 5, 3);
	TEST_ASSERT(ret != 0);
	TEST_STR_EQUAL(dummy_buffer, "AAAZZZCXX");

	/* partial write holes */
	ret = vol->write_partial_block(vol, 0, NULL, 1, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(dummy_buffer, "AAA\0\0\0CXX", 7);
	TEST_EQUAL_I(ret, 0);

	ret = vol->write_partial_block(vol, 0, NULL, 5, 2);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(dummy_buffer, "AAA\0\0\0C\0\0", 7);
	TEST_EQUAL_I(ret, 0);

	/* cleanup */
	object_drop(vol);
	TEST_EQUAL_UI(dummy.base.refcount, 1);
	return EXIT_SUCCESS;
}
