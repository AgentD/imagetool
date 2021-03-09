/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * blocksize_adapter2.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "test.h"
#include "volume.h"

static char dummy_buffer[29] = "AAAaaaABBbbbBBCcccCCCdddDDDd";

static int discard_sequence[10];
static int num_discarded = 0;

static int dummy_read_block(volume_t *vol, uint64_t index, void *buffer)
{
	(void)vol;
	TEST_ASSERT(index < 4);
	memcpy(buffer, dummy_buffer + index * 7, 7);
	return 0;
}

static int dummy_read_partial_block(volume_t *vol, uint64_t index,
				    void *buffer, uint32_t offset,
				    uint32_t size)
{
	(void)vol;
	TEST_ASSERT(index < 4);
	TEST_ASSERT(offset <= 7);
	TEST_ASSERT(size <= (7 - offset));
	memcpy(buffer, dummy_buffer + index * 7 + offset, size);
	return 0;
}

static int dummy_write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	(void)vol;
	TEST_ASSERT(index < 4);
	if (buffer == NULL) {
		memset(dummy_buffer + index * 7, 0, 7);
	} else {
		memcpy(dummy_buffer + index * 7, buffer, 7);
	}
	return 0;
}

static int dummy_write_partial_block(volume_t *vol, uint64_t index,
				     const void *buffer, uint32_t offset,
				     uint32_t size)
{
	(void)vol;
	TEST_ASSERT(index < 4);
	TEST_ASSERT(offset <= 7);
	TEST_ASSERT(size <= (7 - offset));
	if (buffer == NULL) {
		memset(dummy_buffer + index * 7 + offset, 0, size);
	} else {
		memcpy(dummy_buffer + index * 7 + offset, buffer, size);
	}
	return 0;
}

static int dummy_discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	(void)vol;
	while (index < 4 && count > 0) {
		discard_sequence[num_discarded++] = index;

		memset(dummy_buffer + index * 7, 0, 7);
		++index;
		--count;
	}
	return 0;
}

static int dummy_move_block(volume_t *vol, uint64_t src, uint64_t dst)
{
	(void)vol;
	TEST_ASSERT(src < 4);
	TEST_ASSERT(dst < 4);
	memmove(dummy_buffer + dst * 7, dummy_buffer + src * 7, 7);
	return 0;
}

static int dummy_move_block_partial(volume_t *vol, uint64_t src, uint64_t dst,
				    size_t src_offset, size_t dst_offset,
				    size_t size)
{
	(void)vol;
	TEST_ASSERT(src < 10);
	TEST_ASSERT(dst < 10);
	TEST_ASSERT(src_offset < 7);
	TEST_ASSERT(dst_offset < 7);
	TEST_ASSERT((src_offset + size) <= 7);
	TEST_ASSERT((dst_offset + size) <= 7);
	memmove(dummy_buffer + dst * 7 + dst_offset,
		dummy_buffer + src * 7 + src_offset, size);
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
	return 4;
}

static volume_t dummy = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 7,

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
	char temp[4];
	int ret;

	vol = volume_blocksize_adapter_create(&dummy, 3, 0);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(dummy.base.refcount, 2);

	TEST_EQUAL_UI(vol->blocksize, 3);
	TEST_EQUAL_UI(vol->get_min_block_count(vol), 0);
	TEST_EQUAL_UI(vol->get_max_block_count(vol), 9);

	/* read blocks */
	temp[3] = '\0';

	ret = vol->read_block(vol, 0, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "AAA");

	ret = vol->read_block(vol, 1, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "aaa");

	ret = vol->read_block(vol, 2, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "ABB");

	ret = vol->read_block(vol, 3, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "bbb");

	ret = vol->read_block(vol, 4, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "BBC");

	ret = vol->read_block(vol, 5, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "ccc");

	ret = vol->read_block(vol, 6, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "CCC");

	ret = vol->read_block(vol, 7, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "ddd");

	ret = vol->read_block(vol, 8, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "DDD");

	ret = vol->read_block(vol, 9, temp);
	TEST_ASSERT(ret != 0);

	/* overwrite blocks */
	ret = vol->write_block(vol, 1, "zzz");
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAAzzzABBbbbBBCcccCCCdddDDDd");

	ret = vol->write_block(vol, 2, "FFF");
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAAzzzFFFbbbBBCcccCCCdddDDDd");

	ret = vol->write_block(vol, 9, "MMM");
	TEST_ASSERT(ret != 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAAzzzFFFbbbBBCcccCCCdddDDDd");

	/* discard blocks */
	ret = vol->discard_blocks(vol, 0, 3);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	ret = memcmp(dummy_buffer,
		     "\0\0\0\0\0\0\0\0\0bbbBBCcccCCCdddDDDd", 29);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(num_discarded, 1);
	TEST_EQUAL_I(discard_sequence[0], 0);

	ret = vol->discard_blocks(vol, 3, 2);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	ret = memcmp(dummy_buffer,
		     "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0cccCCCdddDDDd", 29);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(num_discarded, 1);
	TEST_EQUAL_I(discard_sequence[0], 0);

	/* partial read */
	memset(temp, 0, sizeof(temp));
	ret = vol->read_partial_block(vol, 5, temp, 1, 2);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "cc");

	memset(temp, 0, sizeof(temp));
	ret = vol->read_partial_block(vol, 5, temp, 0, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "ccc");

	ret = vol->read_partial_block(vol, 5, temp, 0, 4);
	TEST_ASSERT(ret != 0);

	ret = vol->read_partial_block(vol, 5, temp, 1, 3);
	TEST_ASSERT(ret != 0);

	/* partial write */
	ret = vol->write_partial_block(vol, 5, "ZZ", 1, 2);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer + 15, "cZZCCCdddDDDd");

	ret = vol->write_partial_block(vol, 5, "XX", 0, 2);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer + 15, "XXZCCCdddDDDd");

	ret = vol->write_partial_block(vol, 5, "YYY", 1, 3);
	TEST_ASSERT(ret != 0);
	TEST_STR_EQUAL(dummy_buffer + 15, "XXZCCCdddDDDd");

	/* partial write holes */
	ret = vol->write_partial_block(vol, 5, NULL, 1, 2);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(dummy_buffer + 15, "X\0\0CCCdddDDDd", 13);
	TEST_EQUAL_I(ret, 0);

	ret = vol->write_partial_block(vol, 7, NULL, 0, 2);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(dummy_buffer + 15, "X\0\0CCC\0\0dDDDd", 13);
	TEST_EQUAL_I(ret, 0);

	ret = vol->write_partial_block(vol, 6, NULL, 0, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(dummy_buffer + 15, "X\0\0\0\0\0\0\0dDDDd", 13);
	TEST_EQUAL_I(ret, 0);

	ret = vol->write_partial_block(vol, 6, NULL, 0, 4);
	TEST_ASSERT(ret != 0);
	ret = memcmp(dummy_buffer + 15, "X\0\0\0\0\0\0\0dDDDd", 13);
	TEST_EQUAL_I(ret, 0);

	/* cleanup */
	object_drop(vol);
	TEST_EQUAL_UI(dummy.base.refcount, 1);
	return EXIT_SUCCESS;
}
