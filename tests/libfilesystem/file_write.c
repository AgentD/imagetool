/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_write.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "../test.h"
#include "volume.h"
#include "fstree.h"

#define BLK_COUNT (20)
#define BLK_SIZE (4)

static char dummy_buffer[BLK_SIZE * BLK_COUNT + 1] =
	"X___AAAABBBBCCCCDDDDAAA_EEEEFFFFBB__";

static size_t used = 9;

static int dummy_read_partial_block(volume_t *vol, uint64_t index,
				    void *buffer, uint32_t offset,
				    uint32_t size)
{
	(void)vol;
	TEST_ASSERT(index < BLK_COUNT);
	TEST_ASSERT(offset <= BLK_SIZE);
	TEST_ASSERT(size <= (BLK_SIZE - offset));
	memcpy(buffer, dummy_buffer + index * BLK_SIZE + offset, size);
	return 0;
}

static int dummy_write_partial_block(volume_t *vol, uint64_t index,
				     const void *buffer, uint32_t offset,
				     uint32_t size)
{
	(void)vol;
	TEST_ASSERT(index < BLK_COUNT);
	TEST_ASSERT(offset <= BLK_SIZE);
	TEST_ASSERT(size <= (BLK_SIZE - offset));
	if (index >= used)
		used = index + 1;
	if (buffer == NULL) {
		memset(dummy_buffer + index * BLK_SIZE + offset, 0, size);
	} else {
		memcpy(dummy_buffer + index * BLK_SIZE + offset, buffer, size);
	}
	return 0;
}

static int dummy_read_block(volume_t *vol, uint64_t index, void *buffer)
{
	(void)vol;
	TEST_ASSERT(index < BLK_COUNT);
	memcpy(buffer, dummy_buffer + index * BLK_SIZE, BLK_SIZE);
	return 0;
}

static int dummy_write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	(void)vol;
	TEST_ASSERT(index < BLK_COUNT);
	if (index >= used)
		used = index + 1;
	if (buffer == NULL) {
		memset(dummy_buffer + index * BLK_SIZE, 0, BLK_SIZE);
	} else {
		memcpy(dummy_buffer + index * BLK_SIZE, buffer, BLK_SIZE);
	}
	return 0;
}

static int dummy_move_block(volume_t *vol, uint64_t src, uint64_t dst,
			    int mode)
{
	(void)vol;
	TEST_EQUAL_I(mode, 0);
	TEST_ASSERT(src < BLK_COUNT);
	TEST_ASSERT(dst < BLK_COUNT);

	if (dst >= used)
		used = dst + 1;

	memmove(dummy_buffer + dst * BLK_SIZE, dummy_buffer + src * BLK_SIZE,
		BLK_SIZE);
	return 0;
}

static int dummy_move_block_partial(volume_t *vol, uint64_t src, uint64_t dst,
				    size_t src_offset, size_t dst_offset,
				    size_t size)
{
	(void)vol;
	TEST_ASSERT(src < BLK_COUNT);
	TEST_ASSERT(dst < BLK_COUNT);
	TEST_ASSERT(src_offset < BLK_SIZE);
	TEST_ASSERT(dst_offset < BLK_SIZE);
	TEST_ASSERT((src_offset + size) < BLK_SIZE);
	TEST_ASSERT((dst_offset + size) < BLK_SIZE);

	if (dst >= used)
		used = dst + 1;

	memmove(dummy_buffer + dst * BLK_SIZE + dst_offset,
		dummy_buffer + src * BLK_SIZE + src_offset, size);
	return 0;
}

static int dummy_discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	(void)vol;
	TEST_ASSERT(index < BLK_COUNT);
	TEST_ASSERT((index + count) < BLK_COUNT);
	memset(dummy_buffer + index * BLK_SIZE, 0, count * BLK_SIZE);

	if (index + count >= used)
		used = index;

	return 0;
}

static volume_t dummy = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = BLK_SIZE,

	.min_block_count = 0,
	.max_block_count = BLK_COUNT,

	.read_partial_block = dummy_read_partial_block,
	.read_block = dummy_read_block,
	.write_partial_block = dummy_write_partial_block,
	.write_block = dummy_write_block,
	.move_block = dummy_move_block,
	.move_block_partial = dummy_move_block_partial,
	.discard_blocks = dummy_discard_blocks,
	.commit = NULL,
};

int main(void)
{
	tree_node_t *f0, *f1, *f2, *f3;
	fstree_t *fs;
	int ret;

	/* setup */
	fs = fstree_create(&dummy);
	TEST_NOT_NULL(fs);
	fs->data_offset = used;

	f0 = fstree_add_file(fs, "afile");
	TEST_NOT_NULL(f0);

	f1 = fstree_add_file(fs, "bfile");
	TEST_NOT_NULL(f1);

	f2 = fstree_add_file(fs, "cfile");
	TEST_NOT_NULL(f2);

	f3 = fstree_add_file(fs, "dfile");
	TEST_NOT_NULL(f3);

	/* first file has 4 data blocks, 2 byte tail end */
	f0->data.file.size = 19;
	f0->data.file.start_index = 1;

	/* second file has 2 data blocks, 3 sparse blocks, 1 byte tail end */
	f1->data.file.size = 22;
	f1->data.file.start_index = 6;

	f1->data.file.sparse = calloc(1, sizeof(file_sparse_holes_t));
	TEST_NOT_NULL(f1->data.file.sparse);

	f1->data.file.sparse->index = 0;
	f1->data.file.sparse->count = 2;

	f1->data.file.sparse->next = calloc(1, sizeof(file_sparse_holes_t));
	TEST_NOT_NULL(f1->data.file.sparse->next);
	f1->data.file.sparse->next->index = 3;
	f1->data.file.sparse->next->count = 1;

	/* third file has 1 data block and no tail end */
	f2->data.file.size = 1;
	f2->data.file.start_index = 0;

	/* fourth file is completely sparse */
	f3->data.file.size = 2;
	f3->data.file.start_index = 0;

	f3->data.file.sparse = calloc(1, sizeof(file_sparse_holes_t));
	TEST_NOT_NULL(f3->data.file.sparse);

	f3->data.file.sparse->index = 0;
	f3->data.file.sparse->count = 1;

	/* overwrite regular data */
	ret = fstree_file_write(fs, f0, 10, "XXYYYYZZ", 8);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "X___AAAABBBBCCXXYYYYZZA_EEEEFFFFBB__");
	TEST_EQUAL_UI(used, 9);

	TEST_EQUAL_UI(f0->data.file.size, 19);
	TEST_EQUAL_UI(f0->data.file.start_index, 1);
	TEST_EQUAL_UI(f1->data.file.size, 22);
	TEST_EQUAL_UI(f1->data.file.start_index, 6);
	TEST_EQUAL_UI(f2->data.file.size, 1);
	TEST_EQUAL_UI(f2->data.file.start_index, 0);
	TEST_EQUAL_UI(f3->data.file.size, 2);
	TEST_EQUAL_UI(f3->data.file.start_index, 0);

	TEST_NOT_NULL(f1->data.file.sparse);
	TEST_EQUAL_UI(f1->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f1->data.file.sparse->count, 2);
	TEST_NOT_NULL(f1->data.file.sparse->next);
	TEST_EQUAL_UI(f1->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f1->data.file.sparse->next->count, 1);
	TEST_NULL(f1->data.file.sparse->next->next);

	TEST_NOT_NULL(f3->data.file.sparse);
	TEST_EQUAL_UI(f3->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f3->data.file.sparse->count, 1);
	TEST_NULL(f3->data.file.sparse->next);

	/* write sarts inside but goes beyond EOF */
	ret = fstree_file_write(fs, f2, 0, "FGGGG", 5);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAAABBBBCCXXYYYYZZA_EEEEFFFFBB__FGGGG");
	TEST_EQUAL_UI(used, 10);

	TEST_EQUAL_UI(f0->data.file.size, 19);
	TEST_EQUAL_UI(f0->data.file.start_index, 0);
	TEST_EQUAL_UI(f1->data.file.size, 22);
	TEST_EQUAL_UI(f1->data.file.start_index, 5);
	TEST_EQUAL_UI(f2->data.file.size, 5);
	TEST_EQUAL_UI(f2->data.file.start_index, 8);
	TEST_EQUAL_UI(f3->data.file.size, 2);
	TEST_EQUAL_UI(f3->data.file.start_index, 0);

	TEST_NOT_NULL(f1->data.file.sparse);
	TEST_EQUAL_UI(f1->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f1->data.file.sparse->count, 2);
	TEST_NOT_NULL(f1->data.file.sparse->next);
	TEST_EQUAL_UI(f1->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f1->data.file.sparse->next->count, 1);
	TEST_NULL(f1->data.file.sparse->next->next);

	TEST_NOT_NULL(f3->data.file.sparse);
	TEST_EQUAL_UI(f3->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f3->data.file.sparse->count, 1);
	TEST_NULL(f3->data.file.sparse->next);

	/* write zero bytes into sparse region */
	ret = fstree_file_write(fs, f1, 2, "\0\0\0\0\0\0", 6);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(dummy_buffer, "AAAABBBBCCXXYYYYZZA_EEEEFFFFBB__FGGGG");
	TEST_EQUAL_UI(used, 10);

	TEST_EQUAL_UI(f0->data.file.size, 19);
	TEST_EQUAL_UI(f0->data.file.start_index, 0);
	TEST_EQUAL_UI(f1->data.file.size, 22);
	TEST_EQUAL_UI(f1->data.file.start_index, 5);
	TEST_EQUAL_UI(f2->data.file.size, 5);
	TEST_EQUAL_UI(f2->data.file.start_index, 8);
	TEST_EQUAL_UI(f3->data.file.size, 2);
	TEST_EQUAL_UI(f3->data.file.start_index, 0);

	TEST_NOT_NULL(f1->data.file.sparse);
	TEST_EQUAL_UI(f1->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f1->data.file.sparse->count, 2);
	TEST_NOT_NULL(f1->data.file.sparse->next);
	TEST_EQUAL_UI(f1->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f1->data.file.sparse->next->count, 1);
	TEST_NULL(f1->data.file.sparse->next->next);

	TEST_NOT_NULL(f3->data.file.sparse);
	TEST_EQUAL_UI(f3->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f3->data.file.sparse->count, 1);
	TEST_NULL(f3->data.file.sparse->next);

	/* write non-zero bytes into sparse region */
	ret = fstree_file_write(fs, f1, 2, "XX", 2);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(used, 11);
	ret = memcmp(dummy_buffer,
		     "AAAABBBBCCXXYYYYZZA_FGGGG\0\0\0\0\0XXEEEEFFFFBB__",
		     used * BLK_SIZE);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(f0->data.file.size, 19);
	TEST_EQUAL_UI(f0->data.file.start_index, 0);
	TEST_EQUAL_UI(f1->data.file.size, 22);
	TEST_EQUAL_UI(f1->data.file.start_index, 7);
	TEST_EQUAL_UI(f2->data.file.size, 5);
	TEST_EQUAL_UI(f2->data.file.start_index, 5);
	TEST_EQUAL_UI(f3->data.file.size, 2);
	TEST_EQUAL_UI(f3->data.file.start_index, 0);

	TEST_NOT_NULL(f1->data.file.sparse);
	TEST_EQUAL_UI(f1->data.file.sparse->index, 1);
	TEST_EQUAL_UI(f1->data.file.sparse->count, 1);
	TEST_NOT_NULL(f1->data.file.sparse->next);
	TEST_EQUAL_UI(f1->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f1->data.file.sparse->next->count, 1);
	TEST_NULL(f1->data.file.sparse->next->next);

	TEST_NOT_NULL(f3->data.file.sparse);
	TEST_EQUAL_UI(f3->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f3->data.file.sparse->count, 1);
	TEST_NULL(f3->data.file.sparse->next);

	/* write into sparse block of a file that has no blocks */
	ret = fstree_file_write(fs, f3, 0, "FFF", 3);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(used, 12);
	ret = memcmp(dummy_buffer,
		     "AAAABBBBCCXXYYYYZZA_FGGGG\0\0\0\0\0XXEEEEFFFFBB__FFF\0",
		     used * BLK_SIZE);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(f0->data.file.size, 19);
	TEST_EQUAL_UI(f0->data.file.start_index, 0);
	TEST_EQUAL_UI(f1->data.file.size, 22);
	TEST_EQUAL_UI(f1->data.file.start_index, 7);
	TEST_EQUAL_UI(f2->data.file.size, 5);
	TEST_EQUAL_UI(f2->data.file.start_index, 5);
	TEST_EQUAL_UI(f3->data.file.size, 3);
	TEST_EQUAL_UI(f3->data.file.start_index, 11);

	TEST_NOT_NULL(f1->data.file.sparse);
	TEST_EQUAL_UI(f1->data.file.sparse->index, 1);
	TEST_EQUAL_UI(f1->data.file.sparse->count, 1);
	TEST_NOT_NULL(f1->data.file.sparse->next);
	TEST_EQUAL_UI(f1->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f1->data.file.sparse->next->count, 1);
	TEST_NULL(f1->data.file.sparse->next->next);
	TEST_NULL(f3->data.file.sparse);

	/* cleanup */
	object_drop(fs);
	return EXIT_SUCCESS;
}
