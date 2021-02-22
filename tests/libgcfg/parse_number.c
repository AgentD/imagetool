/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * parse_number.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"
#include "test.h"

static const struct {
	const char *in;
	gcfg_number_t out;
	double dbl;
	int ret;
} testvec[] = {
	{ "", .ret = -1 },
	{ "+", .ret = -1 },
	{ "-", .ret = -1 },
	{ "13", {1,13,0,0,0}, 13.0, 0 },
	{ "+13", {1,13,0,0,0}, 13.0, 0 },
	{ "-13", {-1,13,0,0,0}, -13.0, 0 },
	{ "13.37", {1,13,37,2,0}, 13.37, 0 },
	{ "+13.37", {1,13,37,2,0}, 13.37, 0 },
	{ "-13.37", {-1,13,37,2,0}, -13.37, 0 },
	{ "13%", {1,13,0,0,-2}, .13, 0 },
	{ "+13%", {1,13,0,0,-2}, .13, 0 },
	{ "-13%", {-1,13,0,0,-2}, -.13, 0 },
	{ "13.37%", {1,13,37,2,-2}, .1337, 0 },
	{ "+13.37%", {1,13,37,2,-2}, .1337, 0 },
	{ "-13.37%", {-1,13,37,2,-2}, -.1337, 0 },
	{ "13e-42", {1,13,0,0,-42}, 13e-42, 0 },
	{ "+13e+42", {1,13,0,0,42}, 13e42, 0 },
	{ "-13e-42", {-1,13,0,0,-42}, -13e-42, 0 },
	{ "13.37e-42", {1,13,37,2,-42}, 13.37e-42, 0 },
	{ "+13.37e42", {1,13,37,2,42}, 13.37e42, 0 },
	{ "-13.37e-42", {-1,13,37,2,-42}, -13.37e-42, 0 },
};

static void print_num(char *buffer, const gcfg_number_t *out)
{
	sprintf(buffer, "%c%u.%u * 10^%d",
		out->sign > 0 ? '+' : '-',
		out->integer, out->fraction, out->exponent);
}

static void test_case(gcfg_file_t *df, size_t i)
{
	gcfg_number_t out;
	char buffer[128];
	const char *ret;

	ret = gcfg_parse_number(df, testvec[i].in, &out);

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

	if (memcmp(&out, &testvec[i].out, sizeof(out)) != 0) {
		fprintf(stderr, "Mismatch for %zu\n", i);
		print_num(buffer, &testvec[i].out);
		fprintf(stderr, "Expected: %s\n", buffer);
		print_num(buffer, &out);
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
