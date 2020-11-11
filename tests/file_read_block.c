/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_read_block.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "volume.h"
#include "fstree.h"
#include "test.h"

static char dummy_buffer[21] = "AAABBBCCCDDDEEEFFFAAB";

static int dummy_read_block(volume_t *vol, uint64_t index, void *buffer)
{
	(void)vol;
	TEST_ASSERT(index < 7);
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
	.max_block_count = 7,

	.read_block = dummy_read_block,
	.write_block = NULL,
	.move_block = NULL,
	.discard_blocks = NULL,
	.commit = NULL,
};

int main(void)
{
	tree_node_t *f0, *f1, *f2;
	char block[4];
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
	f0->data.file.tail_index = 6;
	f0->data.file.tail_offset = 0;

	/* second file has 2 data blocks, 3 sparse blocks, 1 byte tail end */
	f1->data.file.size = 16;
	f1->data.file.start_index = 4;
	f1->data.file.tail_index = 6;
	f1->data.file.tail_offset = 2;

	f1->data.file.sparse = calloc(1, sizeof(file_sparse_holes_t));
	TEST_NOT_NULL(f1->data.file.sparse);

	f1->data.file.sparse->index = 0;
	f1->data.file.sparse->count = 2;

	f1->data.file.sparse->next = calloc(1, sizeof(file_sparse_holes_t));
	TEST_NOT_NULL(f1->data.file.sparse->next);
	f1->data.file.sparse->next->index = 3;
	f1->data.file.sparse->next->count = 1;

	/* third file has 1 data block and no tail end. Tail index is
	   deliberately out of bounds to catch invalid access. */
	f2->data.file.size = 3;
	f2->data.file.tail_index = 0xFFFFFFFFFFFFFFFFUL;

	/* read blocks of the first file */
	block[3] = '\0';

	ret = fstree_file_read_block(fs, f0, 0, block);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(block, "AAA");

	ret = fstree_file_read_block(fs, f0, 1, block);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(block, "BBB");

	ret = fstree_file_read_block(fs, f0, 2, block);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(block, "CCC");

	ret = fstree_file_read_block(fs, f0, 3, block);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(block, "DDD");

	ret = fstree_file_read_block(fs, f0, 4, block);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(block, "AA\0", 3);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read_block(fs, f0, 5, block);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(block, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	/* read blocks of the second file */
	block[3] = '\0';

	ret = fstree_file_read_block(fs, f1, 0, block);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(block, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read_block(fs, f1, 1, block);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(block, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read_block(fs, f1, 2, block);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(block, "EEE");

	ret = fstree_file_read_block(fs, f1, 3, block);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(block, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read_block(fs, f1, 4, block);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(block, "FFF");

	ret = fstree_file_read_block(fs, f1, 5, block);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(block, "B\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_file_read_block(fs, f1, 6, block);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(block, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	/* read blocks of the third file */
	ret = fstree_file_read_block(fs, f2, 0, block);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(block, "AAA");

	ret = fstree_file_read_block(fs, f2, 1, block);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(block, "\0\0\0", 3);
	TEST_EQUAL_I(ret, 0);

	/* cleanup */
	object_drop(fs);
	return EXIT_SUCCESS;
}
