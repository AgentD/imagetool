/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_read.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "../../test.h"
#include "volume.h"
#include "fstree.h"

static char dummy_buffer[24] = "AAABBBCCCDDDAA_EEEFFFB__";

static int dummy_read_partial_block(volume_t *vol, uint64_t index,
				    void *buffer, uint32_t offset,
				    uint32_t size)
{
	(void)vol;
	TEST_ASSERT(index < 8);
	TEST_ASSERT(offset <= 3);
	TEST_ASSERT(size <= (3 - offset));
	memcpy(buffer, dummy_buffer + index * 3 + offset, size);
	return 0;
}

static int dummy_read_block(volume_t *vol, uint64_t index, void *buffer)
{
	(void)vol;
	TEST_ASSERT(index < 8);
	memcpy(buffer, dummy_buffer + index * 3, 3);
	return 0;
}

static volume_t dummy = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 3,

	.min_block_count = 0,
	.max_block_count = 8,

	.read_partial_block = dummy_read_partial_block,
	.read_block = dummy_read_block,
	.write_block = NULL,
	.move_block = NULL,
	.discard_blocks = NULL,
	.commit = NULL,
};

int main(void)
{
	tree_node_t *f0, *f1, *f2;
	char data[10];
	fstree_t *fs;
	int ret;

	/* setup */
	fs = fstree_create(&dummy);
	TEST_NOT_NULL(fs);

	f0 = fstree_add_file(fs, "afile");
	TEST_NOT_NULL(f0);

	f1 = fstree_add_file(fs, "bfile");
	TEST_NOT_NULL(f1);

	f2 = fstree_add_file(fs, "cfile");
	TEST_NOT_NULL(f2);

	/* first file has 4 data blocks, 2 byte tail end */
	f0->data.file.size = 14;
	f0->data.file.start_index = 0;

	/* second file has 2 data blocks, 3 sparse blocks, 1 byte tail end */
	f1->data.file.size = 16;
	f1->data.file.start_index = 5;

	f1->data.file.sparse = calloc(1, sizeof(file_sparse_holes_t));
	TEST_NOT_NULL(f1->data.file.sparse);

	f1->data.file.sparse->index = 0;
	f1->data.file.sparse->count = 2;

	f1->data.file.sparse->next = calloc(1, sizeof(file_sparse_holes_t));
	TEST_NOT_NULL(f1->data.file.sparse->next);
	f1->data.file.sparse->next->index = 3;
	f1->data.file.sparse->next->count = 1;

	/* third file has 1 data block and no tail end */
	f2->data.file.size = 3;

	/* read first file at exact block boundaries */
	data[3] = '\0';

	ret = fstree_file_read(fs, f0, 0, data, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(data, "AAA");

	ret = fstree_file_read(fs, f0, 3, data, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(data, "BBB");

	ret = fstree_file_read(fs, f0, 6, data, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(data, "CCC");

	ret = fstree_file_read(fs, f0, 9, data, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(data, "DDD");

	ret = fstree_file_read(fs, f0, 12, data, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "AA\0", 3);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read(fs, f0, 15, data, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	/* read first file across block boundaries and read partial blocks */
	data[6] = '\0';

	ret = fstree_file_read(fs, f0, 2, data, 6);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(data, "ABBBCC");

	data[3] = '\0';
	ret = fstree_file_read(fs, f0, 10, data, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(data, "DDA");

	ret = fstree_file_read(fs, f0, 10, data, 5);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "DDAA\0", 5);
	TEST_EQUAL_I(ret, 0);

	/* read second file at exact block boundaries */
	data[3] = '\0';

	ret = fstree_file_read(fs, f1, 0, data, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read(fs, f1, 3, data, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read(fs, f1, 6, data, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(data, "EEE");

	ret = fstree_file_read(fs, f1, 9, data, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read(fs, f1, 12, data, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(data, "FFF");

	ret = fstree_file_read(fs, f1, 15, data, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "B\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read(fs, f1, 18, data, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	/* read second file across block boundaries and read partial blocks */
	ret = fstree_file_read(fs, f1, 4, data, 7);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "\0\0EEE\0\0", 7);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read(fs, f1, 11, data, 6);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "\0FFFB\0", 6);
	TEST_EQUAL_I(ret, 0);

	/* read third file at exact block boundaries */
	data[3] = '\0';

	ret = fstree_file_read(fs, f2, 0, data, 3);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(data, "AAA");

	ret = fstree_file_read(fs, f2, 3, data, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	/* read second file across block boundaries and read partial blocks */
	ret = fstree_file_read(fs, f2, 2, data, 3);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(data, "A\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	/* cleanup */
	object_drop(fs);
	return EXIT_SUCCESS;
}
