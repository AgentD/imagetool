/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * blocksize_adapter4.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "../test.h"
#include "volume.h"

static char dummy_buffer[29] = "aaaaAAAbbbBBBbccCCCccdDDDddd";

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
	memcpy(dummy_buffer + index * 7, buffer, 7);
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
	memcpy(dummy_buffer + index * 7 + offset, buffer, size);
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

	.blocksize = 7,

	.min_block_count = 0,
	.max_block_count = 4,

	.read_block = dummy_read_block,
	.read_partial_block = dummy_read_partial_block,
	.write_block = dummy_write_block,
	.write_partial_block = dummy_write_partial_block,
	.move_block = NULL,
	.discard_blocks = dummy_discard_blocks,
	.commit = dummy_commit,
};

int main(void)
{
	volume_t *vol;
	char temp[4];
	int ret;

	vol = volume_blocksize_adapter_create(&dummy, 3, 4);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(dummy.base.refcount, 2);

	TEST_EQUAL_UI(vol->blocksize, 3);
	TEST_EQUAL_UI(vol->min_block_count, 0);
	TEST_EQUAL_UI(vol->max_block_count, 8);

	/* read blocks */
	temp[3] = '\0';

	ret = vol->read_block(vol, 0, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "AAA");

	ret = vol->read_block(vol, 1, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "bbb");

	ret = vol->read_block(vol, 2, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "BBB");

	ret = vol->read_block(vol, 3, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "bcc");

	ret = vol->read_block(vol, 4, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "CCC");

	ret = vol->read_block(vol, 5, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "ccd");

	ret = vol->read_block(vol, 6, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "DDD");

	ret = vol->read_block(vol, 7, temp);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(temp, "ddd");

	ret = vol->read_block(vol, 8, temp);
	TEST_ASSERT(ret != 0);

	/* overwrite blocks */
	ret = vol->write_block(vol, 1, "zzz");
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "aaaaAAAzzzBBBbccCCCccdDDDddd");

	ret = vol->write_block(vol, 2, "FFF");
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "aaaaAAAzzzFFFbccCCCccdDDDddd");

	ret = vol->write_block(vol, 8, "MMM");
	TEST_ASSERT(ret != 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "aaaaAAAzzzFFFbccCCCccdDDDddd");

	/* swap blocks */
	ret = vol->move_block(vol, 0, 2, MOVE_SWAP);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "aaaaFFFzzzAAAbccCCCccdDDDddd");

	/* discard blocks */
	ret = vol->discard_blocks(vol, 0, 3);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	ret = memcmp(dummy_buffer,
		     "aaaa\0\0\0\0\0\0\0\0\0bccCCCccdDDDddd", 29);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(num_discarded, 0);

	ret = vol->discard_blocks(vol, 3, 2);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	ret = memcmp(dummy_buffer,
		     "aaaa\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0ccdDDDddd", 29);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(num_discarded, 0);

	/* cleanup */
	object_drop(vol);
	TEST_EQUAL_UI(dummy.base.refcount, 1);
	return EXIT_SUCCESS;
}
