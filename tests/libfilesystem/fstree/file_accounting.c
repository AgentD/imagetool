/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_accounting.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "test.h"
#include "fstree.h"
#include "volume.h"

static volume_t dummy = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 512,

	.min_block_count = 0,
	.max_block_count = 1337,
};

int main(void)
{
	tree_node_t *f0, *f1;
	uint64_t size;
	fstree_t *fs;

	/* setup */
	fs = fstree_create(&dummy);
	TEST_NOT_NULL(fs);

	f0 = fstree_add_file(fs, "afile");
	TEST_NOT_NULL(f0);

	f1 = fstree_add_file(fs, "bfile");
	TEST_NOT_NULL(f1);

	/* sparse byte accounting */
	size = fstree_file_physical_size(fs, f0);
	TEST_EQUAL_UI(size, 0);

	size = fstree_file_sparse_bytes(fs, f0);
	TEST_EQUAL_UI(size, 0);

	f0->data.file.sparse = calloc(1, sizeof(*f0->data.file.sparse));
	TEST_NOT_NULL(f0->data.file.sparse);
	f0->data.file.sparse->index = 0;
	f0->data.file.sparse->count = 1;
	f0->data.file.size = 512;

	size = fstree_file_physical_size(fs, f0);
	TEST_EQUAL_UI(size, 0);
	size = fstree_file_sparse_bytes(fs, f0);
	TEST_EQUAL_UI(size, 512);

	f0->data.file.size = 768;
	size = fstree_file_physical_size(fs, f0);
	TEST_EQUAL_UI(size, 256);
	size = fstree_file_sparse_bytes(fs, f0);
	TEST_EQUAL_UI(size, 512);

	f0->data.file.sparse->index = 1;

	size = fstree_file_physical_size(fs, f0);
	TEST_EQUAL_UI(size, 512);
	size = fstree_file_sparse_bytes(fs, f0);
	TEST_EQUAL_UI(size, 256);

	free(f0->data.file.sparse);
	f0->data.file.sparse = NULL;

	size = fstree_file_physical_size(fs, f0);
	TEST_EQUAL_UI(size, 768);
	size = fstree_file_sparse_bytes(fs, f0);
	TEST_EQUAL_UI(size, 0);

	/* cleanup */
	object_drop(fs);
	return EXIT_SUCCESS;
}
