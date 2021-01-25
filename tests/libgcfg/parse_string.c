/* SPDX-License-Identifier: ISC */
/*
 * parse_string.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"
#include "test.h"

static const struct {
	const char *in;
	const char *out;
	int ret;
} testvec[] = {
	{ "", NULL, -1 },
	{ "\"", NULL, -1 },
	{ "\"\"", "", 0 },
	{ "\"foo\"", "foo", 0 },
	{ "\"\\\\\"", "\\", 0 },
	{ "\"\\\"\"", "\"", 0 },
	{ "\"b\\bt\\tn\\nr\\r\"", "b\bt\tn\nr\r", 0 },
	{ "\"\\a\"", NULL, -1 },
	{ "\"你好！\"", "你好！", 0 },
	{ "\"\\U+0041\"", "A", 0 },
	{ "\"\\U+00A5\"", "\xC2\xA5", 0 },
	{ "\"\\U+FFFF\"", NULL, -1 },
	{ "\"\\U+FFFE\"", NULL, -1 },
	{ "\"\\U+1FFFF\"", NULL, -1 },
	{ "\"\\U+1FFFE\"", NULL, -1 },
	{ "\"\\U+10FFFE\"", NULL, -1 },
	{ "\"\\U+10FFFF\"", NULL, -1 },
	{ "\"\\U+110000\"", NULL, -1 },
};

static void test_case(gcfg_file_t *df, size_t i)
{
	char buffer[128];
	const char *ret;

	ret = gcfg_parse_string(df, testvec[i].in, buffer);

	if ((ret == NULL && testvec[i].ret == 0) ||
	    (ret != NULL && testvec[i].ret != 0)) {
		fprintf(stderr, "Wrong return status for %zu\n", i);
		fprintf(stderr, "Input: '%s' was %s\n",
			testvec[i].in,
			ret == NULL ? "not accepted" : "accepted");
		exit(EXIT_FAILURE);
	}

	if (ret != NULL && strcmp(buffer, testvec[i].out) != 0) {
		fprintf(stderr, "Mismatch for %zu\n", i);
		fprintf(stderr, "Expected: %s\n", testvec[i].out);
		fprintf(stderr, "Received: %s\n", buffer);
		exit(EXIT_FAILURE);
	}
}

int main(void)
{
	gcfg_file_t df;
	size_t i;

	for (i = 0; i < sizeof(testvec) / sizeof(testvec[0]); ++i) {
		dummy_file_init(&df, testvec[i].in);
		test_case(&df, i);
		dummy_file_cleanup(&df);
	}

	return EXIT_SUCCESS;
}
