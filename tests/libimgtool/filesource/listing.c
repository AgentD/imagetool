/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * listing.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../../test.h"

#include "filesource.h"

#include <stdarg.h>
#include <string.h>

static const char *source[] = {
	"dir /etc 0755 0 0",
	"slink /bin 0777 0 0 /usr/bin",
	"slink /lib 0777 0 0 /usr/lib",
	"dir /dev 0755 0 0",
	"nod /dev/console 0600 6 7 c 13 37",
	"nod /dev/blkdev0 0600 8 9 b 42 21",
	"dir /tmp 0755 0 0",
	"sock   /tmp/.X12-socket  0777 13 14",
	"pipe   /tmp/whatever  0644  1000  100",
	"link /tmp/foo  0777 0 0  /tmp/whatever",
	"file /tmp/test.txt 0644 1000 100 hello.txt",
	"file /hello.txt 0644 13 37",
	"dir \"/foo bar\" 0755 0 0",
	"dir \"/foo bar/ test \\\"/\" 0755 0 0",
};

static struct result_t {
	const char *path;
	const char *target;
	uint32_t uid;
	uint32_t gid;
	uint16_t mode;
	uint16_t type;
} expected[] = {
	{ "etc", NULL, 0, 0, 0755, FILE_SOURCE_DIR },
	{ "bin", "/usr/bin", 0, 0, 0777, FILE_SOURCE_SYMLINK },
	{ "lib", "/usr/lib", 0, 0, 0777, FILE_SOURCE_SYMLINK },
	{ "dev", NULL, 0, 0, 0755, FILE_SOURCE_DIR },
	{ "dev/console", NULL, 6, 7, 0600, FILE_SOURCE_CHAR_DEV },
	{ "dev/blkdev0", NULL, 8, 9, 0600, FILE_SOURCE_BLOCK_DEV },
	{ "tmp", NULL, 0, 0, 0755, FILE_SOURCE_DIR },
	{ "tmp/.X12-socket", NULL, 13, 14, 0777, FILE_SOURCE_SOCKET },
	{ "tmp/whatever", NULL, 1000, 100, 0644, FILE_SOURCE_FIFO },
	{ "tmp/foo", "tmp/whatever", 0, 0, 0777, FILE_SOURCE_HARD_LINK },
	{ "tmp/test.txt", NULL, 1000, 100, 0644, FILE_SOURCE_FILE },
	{ "hello.txt", NULL, 13, 37, 0644, FILE_SOURCE_FILE },
	{ "foo bar", NULL, 0, 0, 0755, FILE_SOURCE_DIR },
	{ "foo bar/ test \"", NULL, 0, 0, 0755, FILE_SOURCE_DIR },
};

static file_source_record_t *actual[sizeof(expected) / sizeof(expected[0])];

static int expected_compare(const void *a, const void *b)
{
	const struct result_t *lhs = a, *rhs = b;
	return strcmp(lhs->path, rhs->path);
}

static int actual_compare(const void *a, const void *b)
{
	const file_source_record_t *const *lhs = a, *const *rhs = b;
	return strcmp((*lhs)->full_path, (*rhs)->full_path);
}

static void dummy_report_error(gcfg_file_t *f, const char *msg, ...)
{
	(void)f;
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
}

static gcfg_file_t dummy_file = {
	.report_error = dummy_report_error,
};

int main(void)
{
	file_source_listing_t *lst;
	file_source_record_t *out;
	istream_t *ifstream;
	char buffer[128];
	size_t i;
	int ret;

	lst = file_source_listing_create(TEST_PATH);
	TEST_NOT_NULL(lst);
	TEST_EQUAL_UI(((object_t *)lst)->refcount, 1);

	for (i = 0; i < sizeof(source) / sizeof(source[0]); ++i) {
		ret = file_source_listing_add_line(lst, source[i],
						   &dummy_file);
		TEST_EQUAL_I(ret, 0);
	}

	i = 0;

	for (;;) {
		ret = ((file_source_t *)lst)->
			get_next_record((file_source_t *)lst, &out, &ifstream);
		if (ret > 0)
			break;
		TEST_EQUAL_I(ret, 0);

		if (out->type == FILE_SOURCE_FILE) {
			TEST_NOT_NULL(ifstream);
			TEST_EQUAL_UI(((object_t *)ifstream)->refcount, 1);

			ret = istream_read(ifstream, buffer, sizeof(buffer));
			TEST_EQUAL_I(ret, 14);
			ret = memcmp(buffer, "Hello, world!\n", 14);
			TEST_EQUAL_I(ret, 0);

			TEST_EQUAL_UI(out->size, 14);

			object_drop(ifstream);
		} else {
			TEST_NULL(ifstream);
		}

		TEST_ASSERT(i < (sizeof(actual) / sizeof(actual[0])));
		actual[i++] = out;
	}

	TEST_EQUAL_UI(i, (sizeof(actual) / sizeof(actual[0])));

	qsort(expected, i, sizeof(expected[0]), expected_compare);
	qsort(actual, i, sizeof(actual[0]), actual_compare);

	for (i = 0; i < sizeof(actual) / sizeof(actual[0]); ++i) {
		printf("%s 0%o %u %u\n", actual[i]->full_path,
		       actual[i]->permissions, actual[i]->uid, actual[i]->gid);

		TEST_EQUAL_UI(actual[i]->type, expected[i].type);
		TEST_STR_EQUAL(actual[i]->full_path, expected[i].path);
		TEST_EQUAL_UI(actual[i]->uid, expected[i].uid);
		TEST_EQUAL_UI(actual[i]->gid, expected[i].gid);
		TEST_EQUAL_UI(actual[i]->permissions, expected[i].mode);

		if (actual[i]->type == FILE_SOURCE_SYMLINK ||
		    actual[i]->type == FILE_SOURCE_HARD_LINK) {
			TEST_STR_EQUAL(actual[i]->link_target,
				       expected[i].target);
		}

		free(actual[i]->full_path);
		free(actual[i]->link_target);
		free(actual[i]);
	}

	object_drop(lst);
	return EXIT_SUCCESS;
}
