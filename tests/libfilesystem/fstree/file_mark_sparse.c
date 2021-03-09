/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_mark_sparse.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "test.h"
#include "volume.h"
#include "fstree.h"

static volume_t dummy = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},

	.blocksize = 3,
};

int main(void)
{
	tree_node_t *f0;
	fstree_t *fs;
	int ret;

	/* setup */
	fs = fstree_create(&dummy);
	TEST_NOT_NULL(fs);

	f0 = fstree_add_file(fs, "afile");
	TEST_NOT_NULL(f0);
	TEST_NULL(f0->data.file.sparse);

	f0->data.file.size = 20;
	f0->data.file.start_index = 0;

	/* mark sparse in reverse order, regions must be sorted */
	ret = fstree_file_mark_sparse(f0, 3);
	TEST_EQUAL_I(ret, 0);

	TEST_NOT_NULL(f0->data.file.sparse);
	TEST_EQUAL_UI(f0->data.file.sparse->index, 3);
	TEST_EQUAL_UI(f0->data.file.sparse->count, 1);
	TEST_NULL(f0->data.file.sparse->next);

	ret = fstree_file_mark_sparse(f0, 1);
	TEST_EQUAL_I(ret, 0);

	TEST_NOT_NULL(f0->data.file.sparse);
	TEST_EQUAL_UI(f0->data.file.sparse->index, 1);
	TEST_EQUAL_UI(f0->data.file.sparse->count, 1);
	TEST_NOT_NULL(f0->data.file.sparse->next);

	TEST_EQUAL_UI(f0->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f0->data.file.sparse->next->count, 1);
	TEST_NULL(f0->data.file.sparse->next->next);

	/* mark block before first region sparse -> expands first region */
	ret = fstree_file_mark_sparse(f0, 0);
	TEST_EQUAL_I(ret, 0);

	TEST_NOT_NULL(f0->data.file.sparse);
	TEST_EQUAL_UI(f0->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f0->data.file.sparse->count, 2);
	TEST_NOT_NULL(f0->data.file.sparse->next);

	TEST_EQUAL_UI(f0->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f0->data.file.sparse->next->count, 1);
	TEST_NULL(f0->data.file.sparse->next->next);

	/* mark block after second region sparse -> expands second region */
	ret = fstree_file_mark_sparse(f0, 4);
	TEST_EQUAL_I(ret, 0);

	TEST_NOT_NULL(f0->data.file.sparse);
	TEST_EQUAL_UI(f0->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f0->data.file.sparse->count, 2);
	TEST_NOT_NULL(f0->data.file.sparse->next);

	TEST_EQUAL_UI(f0->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f0->data.file.sparse->next->count, 2);
	TEST_NULL(f0->data.file.sparse->next->next);

	/* mark block in between -> merges the two regions */
	ret = fstree_file_mark_sparse(f0, 2);
	TEST_EQUAL_I(ret, 0);

	TEST_NOT_NULL(f0->data.file.sparse);
	TEST_EQUAL_UI(f0->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f0->data.file.sparse->count, 5);
	TEST_NULL(f0->data.file.sparse->next);

	/* mark a block as not sparse -> splits the two regions */
	ret = fstree_file_mark_not_sparse(f0, 2);
	TEST_EQUAL_I(ret, 0);

	TEST_NOT_NULL(f0->data.file.sparse);
	TEST_EQUAL_UI(f0->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f0->data.file.sparse->count, 2);
	TEST_NOT_NULL(f0->data.file.sparse->next);

	TEST_EQUAL_UI(f0->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f0->data.file.sparse->next->count, 2);
	TEST_NULL(f0->data.file.sparse->next->next);

	/* mark block at end of second region non sparse -> shrinks it */
	ret = fstree_file_mark_not_sparse(f0, 4);
	TEST_EQUAL_I(ret, 0);

	TEST_NOT_NULL(f0->data.file.sparse);
	TEST_EQUAL_UI(f0->data.file.sparse->index, 0);
	TEST_EQUAL_UI(f0->data.file.sparse->count, 2);
	TEST_NOT_NULL(f0->data.file.sparse->next);

	TEST_EQUAL_UI(f0->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f0->data.file.sparse->next->count, 1);
	TEST_NULL(f0->data.file.sparse->next->next);

	/* mark block at start of first region non sparse -> shrinks it */
	ret = fstree_file_mark_not_sparse(f0, 0);
	TEST_EQUAL_I(ret, 0);

	TEST_NOT_NULL(f0->data.file.sparse);
	TEST_EQUAL_UI(f0->data.file.sparse->index, 1);
	TEST_EQUAL_UI(f0->data.file.sparse->count, 1);
	TEST_NOT_NULL(f0->data.file.sparse->next);

	TEST_EQUAL_UI(f0->data.file.sparse->next->index, 3);
	TEST_EQUAL_UI(f0->data.file.sparse->next->count, 1);
	TEST_NULL(f0->data.file.sparse->next->next);

	/* remove the first region entirely it */
	ret = fstree_file_mark_not_sparse(f0, 1);
	TEST_EQUAL_I(ret, 0);

	TEST_NOT_NULL(f0->data.file.sparse);
	TEST_EQUAL_UI(f0->data.file.sparse->index, 3);
	TEST_EQUAL_UI(f0->data.file.sparse->count, 1);
	TEST_NULL(f0->data.file.sparse->next);

	/* remove the second region entirely it */
	ret = fstree_file_mark_not_sparse(f0, 3);
	TEST_EQUAL_I(ret, 0);

	TEST_NULL(f0->data.file.sparse);

	/* cleanup */
	object_drop(fs);
	return EXIT_SUCCESS;
}
