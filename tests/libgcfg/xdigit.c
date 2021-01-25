/* SPDX-License-Identifier: ISC */
/*
 * xdigit.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"
#include "test.h"

static const struct {
	int in;
	int out;
} testvec[] = {
	{ '!', -1 },
	{ '0', 0 },
	{ '1', 1 },
	{ '2', 2 },
	{ '3', 3 },
	{ '4', 4 },
	{ '5', 5 },
	{ '6', 6 },
	{ '7', 7 },
	{ '8', 8 },
	{ '9', 9 },
	{ '}', -1 },
	{ 'a', 0x0A },
	{ 'b', 0x0B },
	{ 'c', 0x0C },
	{ 'd', 0x0D },
	{ 'e', 0x0E },
	{ 'f', 0x0F },
	{ 'g', -1 },
	{ 'A', 0x0A },
	{ 'B', 0x0B },
	{ 'C', 0x0C },
	{ 'D', 0x0D },
	{ 'E', 0x0E },
	{ 'F', 0x0F },
	{ 'G', -1 },
};

int main(void)
{
	size_t i;
	int ret;

	for (i = 0; i < sizeof(testvec) / sizeof(testvec[0]); ++i) {
		ret = gcfg_xdigit(testvec[i].in);

		if (ret != testvec[i].out) {
			fprintf(stderr, "Mismatch for %zu\n", i);
			fprintf(stderr, "'%c' mapped to %d instead of %d\n",
				testvec[i].in, ret, testvec[i].out);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
