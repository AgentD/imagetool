/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar2.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../../test.h"

#include "filesource.h"
#include "util.h"

int main(void)
{
	file_source_record_t *rec;
	file_source_t *fs;
	char buffer[1024];
	istream_t *is;
	size_t i;
	int ret;

	fs = file_source_tar_create(TEST_PATH);
	TEST_NOT_NULL(fs);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 1);

	ret = fs->get_next_record(fs, &rec, &is);
	TEST_EQUAL_I(ret, 0);

	TEST_STR_EQUAL(rec->full_path, "input.bin");
	TEST_EQUAL_UI(rec->uid, 1000);
	TEST_EQUAL_UI(rec->gid, 1000);
	TEST_EQUAL_UI(rec->permissions, 0644);
	TEST_EQUAL_UI(rec->type, FILE_SOURCE_FILE);
	TEST_EQUAL_UI(rec->size, 2097152);

	TEST_NOT_NULL(is);
	TEST_EQUAL_UI(((object_t *)is)->refcount, 1);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 2);

	for (i = 0; i < 2048; ++i) {
		ret = istream_read(is, buffer, sizeof(buffer));
		TEST_ASSERT(ret > 0);
		TEST_EQUAL_UI((size_t)ret, sizeof(buffer));

		if ((i % 256) == 0) {
			ret = memcmp(buffer, "test\n", 5);
			TEST_EQUAL_I(ret, 0);
			TEST_ASSERT(is_memory_zero(buffer + 5,
						   sizeof(buffer) - 5));
		} else {
			TEST_ASSERT(is_memory_zero(buffer, sizeof(buffer)));
		}
	}

	ret = istream_read(is, buffer, sizeof(buffer));
	TEST_EQUAL_I(ret, 0);

	object_drop(is);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 1);

	free(rec->full_path);
	free(rec->link_target);
	free(rec);

	ret = fs->get_next_record(fs, &rec, &is);
	TEST_ASSERT(ret > 0);
	TEST_NULL(rec);
	TEST_NULL(is);

	object_drop(fs);
	return EXIT_SUCCESS;
}
