/* SPDX-License-Identifier: ISC */
/*
 * parse_vector.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"
#include "test.h"

static const struct {
	const char *in;
	size_t count;
	int ret;
	gcfg_number_t out[4];
} testvec[] = {
	{ "(+1,-2,+3,-4)", 4, 0,
	  {{1,1,0,0,0},{-1,2,0,0,0},{1,3,0,0,0},{-1,4,0,0,0}} },
	{ "(+1,-2,+3)", 3, 0,
	  {{1,1,0,0,0},{-1,2,0,0,0},{1,3,0,0,0}} },
	{ "(+1,-2)", 2, 0,
	  {{1,1,0,0,0},{-1,2,0,0,0}} },
	{ "(-2)", 1, 0, {{-1,2,0,0,0}} },
	{ "(+1,-2,+3,-4)", 3, -1, {{0}} },
	{ "(+1,-2,+3)", 4, -1, {{0}} },
};

static void test_case(gcfg_file_t *df, size_t i)
{
	gcfg_number_t out[4];
	const char *ret;

	ret = gcfg_parse_vector(df, testvec[i].in, (int)testvec[i].count, out);

	if ((ret == NULL && testvec[i].ret == 0) ||
	    (ret != NULL && testvec[i].ret != 0)) {
		fprintf(stderr, "Wrong return status for %zu\n", i);
		fprintf(stderr, "Input: '%s' was %s\n",
			testvec[i].in,
			ret == NULL ? "not accepted" : "accepted");
		exit(EXIT_FAILURE);
	}

	if (ret == NULL)
		return;

	if (memcmp(out, testvec[i].out, testvec[i].count * sizeof(out[0]))) {
		fprintf(stderr, "Mismatch for %zu\n", i);
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
