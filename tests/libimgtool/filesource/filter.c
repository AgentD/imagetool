/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filter.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "filesource.h"
#include "test.h"

static char hello_str[] = "Hello, world!\n";

static const char *get_hello_filename(istream_t *strm)
{
	(void)strm;
	return "usr/bin/hello.txt";
}

static istream_t hello_file = {
	.base = {
		.destroy = NULL,
		.refcount = 1,
	},

	.buffer_used = 14,
	.buffer_offset = 0,
	.eof = true,
	.buffer = (uint8_t *)hello_str,

	.precache = NULL,
	.get_filename = get_hello_filename,
};

static file_source_record_t records[] = {
	{
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"bin",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"lib",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"boot",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"dev",
	}, {
		.type = FILE_SOURCE_CHAR_DEV,
		.permissions = 0644,
		.devno = 1337,
		.full_path = (char *)"dev/console",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"usr",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"usr/bin",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"usr/lib",
	}, {
		.type = FILE_SOURCE_FILE,
		.permissions = 0644,
		.full_path = (char *)"usr/bin/hello.txt",
	}, {
		.type = FILE_SOURCE_HARD_LINK,
		.permissions = 0777,
		.full_path = (char *)"usr/lib64",
		.link_target = (char *)"usr/lib",
	}, {
		.type = FILE_SOURCE_SOCKET,
		.permissions = 0600,
		.uid = 1000,
		.gid = 100,
		.full_path = (char *)"boot/whatever",
	},
};

static size_t rec_idx = 0;

static int get_next_record(file_source_t *fs, file_source_record_t **out,
			   istream_t **stream_out)
{
	(void)fs;

	if (rec_idx >= sizeof(records) / sizeof(records[0]))
		return 1;

	*out = calloc(1, sizeof(**out));
	TEST_NOT_NULL(*out);

	memcpy(*out, &records[rec_idx], sizeof(**out));
	(*out)->full_path = strdup(records[rec_idx].full_path);
	TEST_NOT_NULL((*out)->full_path);

	if (records[rec_idx].link_target == NULL) {
		(*out)->link_target = NULL;
	} else {
		(*out)->link_target = strdup(records[rec_idx].link_target);
		TEST_NOT_NULL((*out)->link_target);
	}

	if (stream_out != NULL) {
		*stream_out = NULL;

		if (!strcmp((*out)->full_path, get_hello_filename(NULL))) {
			*stream_out = object_grab(&hello_file);
		}
	}

	rec_idx += 1;
	return 0;
}

static file_source_t filesource = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},
	.get_next_record = get_next_record,
};

/*****************************************************************************/

static file_source_record_t expected1[] = {
	{
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"boot",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"usr/bin",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"usr/lib",
	}, {
		.type = FILE_SOURCE_FILE,
		.permissions = 0644,
		.full_path = (char *)"usr/bin/hello.txt",
	}, {
		.type = FILE_SOURCE_HARD_LINK,
		.permissions = 0777,
		.full_path = (char *)"usr/lib64",
		.link_target = (char *)"usr/lib",
	}, {
		.type = FILE_SOURCE_SOCKET,
		.permissions = 0600,
		.uid = 1000,
		.gid = 100,
		.full_path = (char *)"boot/whatever",
	}
};

static file_source_record_t result1[sizeof(expected1) / sizeof(expected1[0])];

/*****************************************************************************/

static file_source_record_t expected2[] = {
	{
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"bin",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"lib",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"boot",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"dev",
	}, {
		.type = FILE_SOURCE_CHAR_DEV,
		.permissions = 0644,
		.devno = 1337,
		.full_path = (char *)"dev/console",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"usr",
	}, {
		.type = FILE_SOURCE_SOCKET,
		.permissions = 0600,
		.uid = 1000,
		.gid = 100,
		.full_path = (char *)"boot/whatever",
	},
};

static file_source_record_t result2[sizeof(expected2) / sizeof(expected2[0])];

/*****************************************************************************/

static int compare_records(const void *a, const void *b)
{
	const file_source_record_t *lhs = a, *rhs = b;

	return strcmp(lhs->full_path, rhs->full_path);
}

int main(void)
{
	file_source_filter_t *filter;
	file_source_record_t *rec;
	istream_t *stream;
	size_t i;
	int ret;

	/********** filter with default drop behaviour **********/

	/* setup */
	filter = file_source_filter_create();
	TEST_NOT_NULL(filter);
	TEST_EQUAL_UI(((object_t *)filter)->refcount, 1);
	TEST_EQUAL_UI(((object_t *)&filesource)->refcount, 1);

	ret = ((file_source_stackable_t *)filter)->
		add_nested((file_source_stackable_t *)filter, &filesource);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)filter)->refcount, 1);
	TEST_EQUAL_UI(((object_t *)&filesource)->refcount, 2);

	filter->add_glob_rule(filter, "usr/*", FILE_SOURCE_FILTER_ALLOW);
	filter->add_glob_rule(filter, "bo*", FILE_SOURCE_FILTER_ALLOW);

	/* run */
	i = 0;

	for (;;) {
		ret = ((file_source_t *)filter)->
			get_next_record((file_source_t *)filter, &rec,
					&stream);
		if (ret > 0)
			break;

		TEST_EQUAL_I(ret, 0);
		TEST_NOT_NULL(rec);

		if (rec->type == FILE_SOURCE_FILE) {
			TEST_NOT_NULL(stream);
			TEST_EQUAL_UI(hello_file.base.refcount, 2);
		} else {
			TEST_NULL(stream);
		}

		TEST_ASSERT(i < (sizeof(result1) / sizeof(result1[0])));
		result1[i++] = *rec;

		if (stream != NULL) {
			object_drop(stream);
			TEST_EQUAL_UI(hello_file.base.refcount, 1);
		}

		free(rec);
	}

	TEST_EQUAL_UI(i, (sizeof(result1) / sizeof(result1[0])));

	qsort(result1, i, sizeof(result1[0]), compare_records);
	qsort(expected1, i, sizeof(result1[0]), compare_records);

	for (i = 0; i < sizeof(result1) / sizeof(result1[0]); ++i) {
		TEST_EQUAL_UI(expected1[i].type, result1[i].type);
		TEST_EQUAL_UI(expected1[i].uid, result1[i].uid);
		TEST_EQUAL_UI(expected1[i].gid, result1[i].gid);
		TEST_EQUAL_UI(expected1[i].permissions,
			      result1[i].permissions);
		TEST_STR_EQUAL(expected1[i].full_path,
			       result1[i].full_path);

		free(result1[i].full_path);
		free(result1[i].link_target);
	}

	/* cleanup */
	object_drop(filter);
	TEST_EQUAL_UI(((object_t *)&filesource)->refcount, 1);

	/********** filter with default accept behaviour **********/

	/* setup */
	rec_idx = 0;
	filter = file_source_filter_create();
	TEST_NOT_NULL(filter);
	TEST_EQUAL_UI(((object_t *)filter)->refcount, 1);
	TEST_EQUAL_UI(((object_t *)&filesource)->refcount, 1);

	ret = ((file_source_stackable_t *)filter)->
		add_nested((file_source_stackable_t *)filter, &filesource);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)filter)->refcount, 1);
	TEST_EQUAL_UI(((object_t *)&filesource)->refcount, 2);

	filter->add_glob_rule(filter, "usr/*", FILE_SOURCE_FILTER_DISCARD);
	filter->add_glob_rule(filter, "*", FILE_SOURCE_FILTER_ALLOW);

	/* run */
	i = 0;

	for (;;) {
		ret = ((file_source_t *)filter)->
			get_next_record((file_source_t *)filter, &rec,
					&stream);
		if (ret > 0)
			break;

		TEST_EQUAL_I(ret, 0);
		TEST_NOT_NULL(rec);

		if (rec->type == FILE_SOURCE_FILE) {
			TEST_NOT_NULL(stream);
			TEST_EQUAL_UI(hello_file.base.refcount, 2);
		} else {
			TEST_NULL(stream);
		}

		TEST_ASSERT(i < (sizeof(result2) / sizeof(result2[0])));
		result2[i++] = *rec;

		if (stream != NULL) {
			object_drop(stream);
			TEST_EQUAL_UI(hello_file.base.refcount, 1);
		}

		free(rec);
	}

	TEST_EQUAL_UI(i, (sizeof(result2) / sizeof(result2[0])));

	qsort(result2, i, sizeof(result2[0]), compare_records);
	qsort(expected2, i, sizeof(result2[0]), compare_records);

	for (i = 0; i < sizeof(result2) / sizeof(result2[0]); ++i) {
		TEST_EQUAL_UI(expected2[i].type, result2[i].type);
		TEST_EQUAL_UI(expected2[i].uid, result2[i].uid);
		TEST_EQUAL_UI(expected2[i].gid, result2[i].gid);
		TEST_EQUAL_UI(expected2[i].permissions,
			      result2[i].permissions);
		TEST_STR_EQUAL(expected2[i].full_path,
			       result2[i].full_path);

		free(result2[i].full_path);
		free(result2[i].link_target);
	}

	/* cleanup */
	object_drop(filter);
	TEST_EQUAL_UI(((object_t *)&filesource)->refcount, 1);
	return EXIT_SUCCESS;
}
