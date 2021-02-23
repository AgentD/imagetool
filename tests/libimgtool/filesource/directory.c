/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * directory.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "test.h"

#include "filesource.h"

static const char *expected[] = {
	"CREDITS",
	"file-size",
	"format-acceptance",
	"large-mtime",
	"long-paths",
	"negative-mtime",
	"sparse-files",
	"sqfs.sha512",
	"user-group-largenum",
	"xattr",

	"file-size/12-digit.tar",
	"file-size/gnu.tar",
	"file-size/pax.tar",

	"format-acceptance/gnu-g.tar",
	"format-acceptance/gnu.tar",
	"format-acceptance/link_filled.tar",
	"format-acceptance/pax.tar",
	"format-acceptance/ustar-pre-posix.tar",
	"format-acceptance/ustar.tar",
	"format-acceptance/v7.tar",

	"large-mtime/12-digit.tar",
	"large-mtime/gnu.tar",
	"large-mtime/pax.tar",

	"long-paths/gnu.tar",
	"long-paths/pax.tar",
	"long-paths/ustar.tar",

	"negative-mtime/gnu.tar",
	"negative-mtime/pax.tar",

	"sparse-files/gnu-small.tar",
	"sparse-files/gnu.tar",
	"sparse-files/pax-gnu0-0.tar",
	"sparse-files/pax-gnu0-1.tar",
	"sparse-files/pax-gnu1-0.tar",

	"user-group-largenum/8-digit.tar",
	"user-group-largenum/gnu.tar",
	"user-group-largenum/pax.tar",

	"xattr/acl.tar",
	"xattr/xattr-libarchive.tar",
	"xattr/xattr-schily-binary.tar",
	"xattr/xattr-schily.tar",
};

static char *actual[sizeof(expected) / sizeof(expected[0])];

static int string_compare(const void *a, const void *b)
{
	const char *const *lhs = a, *const *rhs = b;

	return strcmp(*lhs, *rhs);
}

int main(void)
{
	size_t i = 0, max_count = sizeof(expected) / sizeof(expected[0]);
	file_source_t *fs;

	fs = file_source_directory_create(TEST_PATH);
	TEST_NOT_NULL(fs);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 1);

	for (;;) {
		file_source_record_t *rec;
		int ret;

		ret = fs->get_next_record(fs, &rec, NULL);
		if (ret > 0)
			break;
		TEST_EQUAL_I(ret, 0);

		TEST_ASSERT(i < max_count);
		actual[i++] = rec->full_path;

		free(rec->link_target);
		free(rec);
	}

	TEST_EQUAL_UI(i, max_count);

	qsort(expected, max_count, sizeof(expected[0]), string_compare);
	qsort(actual, max_count, sizeof(expected[0]), string_compare);

	for (i = 0; i < max_count; ++i) {
		TEST_STR_EQUAL(expected[i], actual[i]);
		free(actual[i]);
	}

	object_drop(fs);
	return EXIT_SUCCESS;
}
