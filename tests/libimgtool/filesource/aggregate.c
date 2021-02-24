/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * aggregate.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "filesource.h"
#include "test.h"

static int get_next_record(file_source_t *fs, file_source_record_t **out,
			   istream_t **stream_out);

static file_source_record_t records1[] = {
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

static file_source_record_t records2[] = {
	{
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.full_path = (char *)"home",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.uid = 1000,
		.gid = 100,
		.full_path = (char *)"home/foo",
	}, {
		.type = FILE_SOURCE_DIR,
		.permissions = 0755,
		.uid = 1001,
		.gid = 100,
		.full_path = (char *)"home/bar",
	}, {
		.type = FILE_SOURCE_SOCKET,
		.permissions = 0600,
		.uid = 1001,
		.gid = 100,
		.full_path = (char *)"home/bar/.socket",
	},
};

static size_t rec_idx1 = 0;
static size_t rec_idx2 = 0;

static file_source_t filesource1 = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},
	.get_next_record = get_next_record,
};

static file_source_t filesource2 = {
	.base = {
		.refcount = 1,
		.destroy = NULL,
	},
	.get_next_record = get_next_record,
};

static int get_next_record(file_source_t *fs, file_source_record_t **out,
			   istream_t **stream_out)
{
	file_source_record_t *records;
	size_t index;
	(void)fs;

	if (fs == &filesource1) {
		if (rec_idx1 >= sizeof(records1) / sizeof(records1[0]))
			return 1;

		index = rec_idx1++;
		records = records1;
	} else {
		if (rec_idx2 >= sizeof(records2) / sizeof(records2[0]))
			return 1;

		index = rec_idx2++;
		records = records2;
	}

	*out = calloc(1, sizeof(**out));
	TEST_NOT_NULL(*out);

	memcpy(*out, &records[index], sizeof(**out));
	(*out)->full_path = strdup(records[index].full_path);
	TEST_NOT_NULL((*out)->full_path);

	if (records[index].link_target == NULL) {
		(*out)->link_target = NULL;
	} else {
		(*out)->link_target = strdup(records[index].link_target);
		TEST_NOT_NULL((*out)->link_target);
	}

	if (stream_out != NULL)
		*stream_out = NULL;

	return 0;
}

/*****************************************************************************/

static file_source_record_t result1[sizeof(records1) / sizeof(records1[0])];

static file_source_record_t result2[sizeof(records1) / sizeof(records1[0]) +
				    sizeof(records2) / sizeof(records2[0])];

static file_source_record_t ref2[sizeof(result2) / sizeof(result2[0])];

static int compare_records(const void *a, const void *b)
{
	const file_source_record_t *lhs = a, *rhs = b;

	return strcmp(lhs->full_path, rhs->full_path);
}

int main(void)
{
	file_source_stackable_t *aggregate;
	file_source_record_t *rec;
	size_t i;
	int ret;

	aggregate = file_source_aggregate_create();
	TEST_NOT_NULL(aggregate);
	TEST_EQUAL_UI(((object_t *)aggregate)->refcount, 1);

	/* empty */
	ret = ((file_source_t *)aggregate)->
		get_next_record((file_source_t *)aggregate, &rec, NULL);
	TEST_ASSERT(ret > 0);

	/* setup one source */
	ret = aggregate->add_nested(aggregate, &filesource1);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)&filesource1)->refcount, 2);

	/* iterate over entries */
	i = 0;

	for (;;) {
		ret = ((file_source_t *)aggregate)->
			get_next_record((file_source_t *)aggregate, &rec, NULL);
		if (ret > 0)
			break;
		TEST_EQUAL_I(ret, 0);

		TEST_ASSERT(i < (sizeof(result1) / sizeof(result1[0])));
		result1[i++] = *rec;
		free(rec);
	}

	TEST_EQUAL_UI(i, (sizeof(result1) / sizeof(result1[0])));

	qsort(records1, i, sizeof(records1[0]), compare_records);
	qsort(result1, i, sizeof(result1[0]), compare_records);

	for (i = 0; i < sizeof(result1) / sizeof(result1[0]); ++i) {
		TEST_EQUAL_UI(result1[i].type, records1[i].type);
		TEST_EQUAL_UI(result1[i].permissions, records1[i].permissions);
		TEST_EQUAL_UI(result1[i].uid, records1[i].uid);
		TEST_EQUAL_UI(result1[i].gid, records1[i].gid);
		TEST_STR_EQUAL(result1[i].full_path, records1[i].full_path);

		if (records1[i].link_target == NULL) {
			TEST_NULL(result1[i].link_target);
		} else {
			TEST_NOT_NULL(result1[i].link_target);
			TEST_STR_EQUAL(result1[i].full_path,
				       records1[i].full_path);
		}

		free(result1[i].link_target);
		free(result1[i].full_path);
	}

	/* two sources */
	object_drop(aggregate);
	TEST_EQUAL_UI(((object_t *)&filesource1)->refcount, 1);

	aggregate = file_source_aggregate_create();
	TEST_NOT_NULL(aggregate);
	TEST_EQUAL_UI(((object_t *)aggregate)->refcount, 1);

	ret = aggregate->add_nested(aggregate, &filesource1);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)&filesource1)->refcount, 2);

	ret = aggregate->add_nested(aggregate, &filesource2);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)&filesource2)->refcount, 2);

	rec_idx1 = 0;
	rec_idx2 = 0;

	i = 0;

	for (;;) {
		ret = ((file_source_t *)aggregate)->
			get_next_record((file_source_t *)aggregate, &rec, NULL);
		if (ret > 0)
			break;
		TEST_EQUAL_I(ret, 0);

		TEST_ASSERT(i < (sizeof(result2) / sizeof(result2[0])));
		result2[i++] = *rec;
		free(rec);
	}

	TEST_EQUAL_UI(i, (sizeof(result2) / sizeof(result2[0])));

	memcpy(ref2, records1, sizeof(records1));
	memcpy((char *)ref2 + sizeof(records1), records2, sizeof(records2));
	qsort(ref2, i, sizeof(ref2[0]), compare_records);
	qsort(result2, i, sizeof(result2[0]), compare_records);

	for (i = 0; i < sizeof(result2) / sizeof(result2[0]); ++i) {
		TEST_EQUAL_UI(result2[i].type, ref2[i].type);
		TEST_EQUAL_UI(result2[i].permissions, ref2[i].permissions);
		TEST_EQUAL_UI(result2[i].uid, ref2[i].uid);
		TEST_EQUAL_UI(result2[i].gid, ref2[i].gid);
		TEST_STR_EQUAL(result2[i].full_path, ref2[i].full_path);

		if (ref2[i].link_target == NULL) {
			TEST_NULL(result2[i].link_target);
		} else {
			TEST_NOT_NULL(result2[i].link_target);
			TEST_STR_EQUAL(result2[i].full_path, ref2[i].full_path);
		}

		free(result2[i].link_target);
		free(result2[i].full_path);
	}

	/* cleanup */
	object_drop(aggregate);
	TEST_EQUAL_UI(((object_t *)&filesource1)->refcount, 1);
	TEST_EQUAL_UI(((object_t *)&filesource2)->refcount, 1);
	return EXIT_SUCCESS;
}
