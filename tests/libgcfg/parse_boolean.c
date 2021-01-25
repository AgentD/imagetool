/* SPDX-License-Identifier: ISC */
/*
 * parse_boolean.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"
#include "test.h"

static const struct {
	const char *in;
	int out;
	int ret;
} testvec[] = {
	{ "true", 1, 0 },
	{ "yes", 1, 0 },
	{ "on", 1, 0 },
	{ "false", 0, 0 },
	{ "off", 0, 0 },
	{ "no", 0, 0 },
	{ "true ", 1, 0 },
	{ "yes ", 1, 0 },
	{ "on ", 1, 0 },
	{ "false ", 0, 0 },
	{ "off ", 0, 0 },
	{ "no ", 0, 0 },
	{ "trueA", 0, -1 },
	{ "yes0", 0, -1 },
	{ "on]", 0, -1 },
	{ "falseS", 0, -1 },
	{ "offD", 0, -1 },
	{ "noF", 0, -1 },
	{ "banana", 0, -1 },
	{ "", 0, -1 },
};

static void test_case(gcfg_file_t *df, size_t i)
{
	const char *ret;
	int out;

	ret = gcfg_parse_boolean(df, testvec[i].in, &out);

	if ((ret != NULL && testvec[i].ret != 0) ||
	    (ret == NULL && testvec[i].ret == 0)) {
		fprintf(stderr, "Wrong return status for %zu\n", i);
		fprintf(stderr, "Input: '%s' was %s\n",
			testvec[i].in,
			ret == NULL ? "not accepted" : "accepted");
		exit(EXIT_FAILURE);
	}

	if (ret == NULL)
		return;

	if (out != testvec[i].out) {
		fprintf(stderr, "Mismatch for %zu\n", i);
		fprintf(stderr, "'%s' was parsed as '%d'\n",
			testvec[i].in, out);
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
