/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume_ostream.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "volume.h"
#include "test.h"

static char dummy_buffer[31] = "______________________________";

static int discard_sequence[10];
static int num_discarded = 0;

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
	ostream_t *ostrm = volume_ostream_create(&dummy, "FOO", 4, 11);
	int ret;

	TEST_NOT_NULL(ostrm);
	TEST_EQUAL_UI(dummy.base.refcount, 2);
	TEST_STR_EQUAL(ostrm->get_filename(ostrm), "FOO");

	ret = ostrm->append(ostrm, "ABCDEF", 6);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(dummy_buffer, "____ABCDEF____________________");

	ret = ostrm->append_sparse(ostrm, 5);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(dummy_buffer, "____ABCDEF\0\0\0\0\0_______________", 30);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(num_discarded, 1);
	TEST_EQUAL_I(discard_sequence[0], 4);

	ret = ostrm->append(ostrm, "FOO", 3);
	TEST_ASSERT(ret != 0);

	object_drop(ostrm);
	TEST_EQUAL_UI(dummy.base.refcount, 1);
	return EXIT_SUCCESS;
}
