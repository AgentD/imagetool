/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * blocksize_adapter2.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "volume.h"
#include "test.h"

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

static int dummy_write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	(void)vol;
	TEST_ASSERT(index < 4);
	memcpy(dummy_buffer + index * 7, buffer, 7);
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
	.write_block = dummy_write_block,
	.move_block = NULL,
	.discard_blocks = dummy_discard_blocks,
	.commit = dummy_commit,
	.create_sub_volume = NULL,
};

int main(void)
{
	volume_t *vol;
	char temp[4];
	int ret;

	vol = volume_blocksize_adapter_create(&dummy, 3);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(dummy.base.refcount, 2);

	TEST_EQUAL_UI(vol->blocksize, 3);
	TEST_EQUAL_UI(vol->min_block_count, 0);
	TEST_EQUAL_UI(vol->max_block_count, 9);

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

	/* swap blocks */
	ret = vol->move_block(vol, 0, 2, MOVE_SWAP);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "FFFzzzAAAbbbBBCcccCCCdddDDDd");

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
	TEST_EQUAL_I(num_discarded, 2);
	TEST_EQUAL_I(discard_sequence[0], 0);
	TEST_EQUAL_I(discard_sequence[1], 1);

	/* cleanup */
	object_drop(vol);
	TEST_EQUAL_UI(dummy.base.refcount, 1);
	return EXIT_SUCCESS;
}
