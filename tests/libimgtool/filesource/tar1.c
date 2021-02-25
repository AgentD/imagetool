/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar1.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "test.h"

#include "filesource.h"
#include "fstream.h"

static const struct {
	const char *path;
	const char *target;
	uint32_t uid;
	uint32_t gid;
	uint16_t mode;
	uint16_t type;
	uint32_t size;
} ref_data[] = {
	{ "bin", "/usr/bin", 0, 0, 0777, FILE_SOURCE_SYMLINK, 0 },
	{ "dev", NULL, 0, 0, 0755, FILE_SOURCE_DIR, 0 },
	{ "dev/console", NULL, 0, 0, 0644, FILE_SOURCE_CHAR_DEV, 0 },
	{ "dev/sda42", NULL, 0, 0, 0644, FILE_SOURCE_BLOCK_DEV, 0 },
	{ "etc", NULL, 0, 0, 0755, FILE_SOURCE_DIR, 0 },
	{ "home", NULL, 0, 0, 0755, FILE_SOURCE_DIR, 0 },
	{ "lib", "/usr/lib", 0, 0, 0777, FILE_SOURCE_SYMLINK, 0 },
	{ "tmp", NULL, 0, 0, 0755, FILE_SOURCE_DIR, 0 },
	{ "usr", NULL, 0, 0, 0755, FILE_SOURCE_DIR, 0 },
	{ "usr/bin", NULL, 0, 0, 0755, FILE_SOURCE_DIR, 0 },
	{ "usr/lib", NULL, 0, 0, 0755, FILE_SOURCE_DIR, 0 },
	{ "var", NULL, 0, 0, 0755, FILE_SOURCE_DIR, 0 },
	{ "var/run", NULL, 0, 0, 0755, FILE_SOURCE_DIR, 0 },
	{ "var/run/whatever", NULL, 0, 0, 0644, FILE_SOURCE_FIFO, 0 },
	{ "home/hello.txt", NULL, 0, 0, 0644, FILE_SOURCE_FILE, 14 },
	{ "tmp/bye.txt", NULL, 0, 0, 0644, FILE_SOURCE_FILE, 8 },
	{ "tmp/sparse.bin", NULL, 0, 0, 0644, FILE_SOURCE_FILE, 2048 },
	{ "etc/empty.cfg", NULL, 0, 0, 0644, FILE_SOURCE_FILE, 0 },
	{ "var/run/foo", "var/run/whatever", 0, 0, 0777,
	  FILE_SOURCE_HARD_LINK, 0 },
};

int main(void)
{
	file_source_t *fs;
	char buffer[1024];
	size_t i = 0;

	fs = file_source_tar_create(TEST_PATH);
	TEST_NOT_NULL(fs);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 1);

	for (;;) {
		file_source_record_t *rec;
		istream_t *is;
		int ret;

		ret = fs->get_next_record(fs, &rec, &is);
		if (ret > 0)
			break;
		TEST_EQUAL_I(ret, 0);

		TEST_ASSERT(i < (sizeof(ref_data) / sizeof(ref_data[0])));
		TEST_STR_EQUAL(rec->full_path, ref_data[i].path);
		TEST_EQUAL_UI(rec->uid, ref_data[i].uid);
		TEST_EQUAL_UI(rec->gid, ref_data[i].gid);
		TEST_EQUAL_UI(rec->permissions, ref_data[i].mode);
		TEST_EQUAL_UI(rec->type, ref_data[i].type);
		TEST_EQUAL_UI(rec->size, ref_data[i].size);

		if (ref_data[i].target != NULL) {
			TEST_STR_EQUAL(rec->link_target, ref_data[i].target);
		}

		if (rec->type == FILE_SOURCE_FILE) {
			file_source_record_t *dummy = rec;

			TEST_NOT_NULL(is);
			TEST_EQUAL_UI(((object_t *)is)->refcount, 1);
			TEST_EQUAL_UI(((object_t *)fs)->refcount, 2);

			ret = fs->get_next_record(fs, &dummy, NULL);
			TEST_ASSERT(ret < 0);
			TEST_NULL(dummy);

			if (!strcmp(rec->full_path, "home/hello.txt")) {
				ret = istream_read(is, buffer, sizeof(buffer));
				TEST_ASSERT(ret >= 0);
				TEST_EQUAL_I(ret, 14);

				ret = memcmp(buffer, "Hello, world!\n", 14);
				TEST_EQUAL_I(ret, 0);

				ret = istream_read(is, buffer, sizeof(buffer));
				TEST_EQUAL_UI(ret, 0);
			}

			object_drop(is);
			TEST_EQUAL_UI(((object_t *)fs)->refcount, 1);
		} else {
			TEST_NULL(is);
			TEST_EQUAL_UI(((object_t *)fs)->refcount, 1);
		}

		++i;

		free(rec->full_path);
		free(rec->link_target);
		free(rec);
	}

	object_drop(fs);
	return EXIT_SUCCESS;
}
