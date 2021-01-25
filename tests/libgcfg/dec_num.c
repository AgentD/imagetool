/* SPDX-License-Identifier: ISC */
/*
 * dec_num.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"
#include "test.h"

typedef struct {
	const char *input;
	uint64_t maximum;
	uint64_t result;
} test_data_t;

static test_data_t must_work[] = {
	{ "0", 10, 0 },
	{ "7", 10, 7 },
	{ "65535", (1UL << 32UL) - 1, 0xFFFF },
	{ "65535asdf", (1UL << 32UL) - 1, 0xFFFF },
	{ "18446744073709551615", 0xFFFFFFFFFFFFFFFFUL, 0xFFFFFFFFFFFFFFFFUL },
};

static test_data_t must_not_work[] = {
	{ "", 10, 0 },
	{ "a", 10, 0 },
	{ "00", 10, 0 },
	{ "07", 10, 0 },
	{ "12", 10, 0 },
	{ "32768", 32767, 0 },
	{ "18446744073709551616", 0xFFFFFFFFFFFFFFFFUL, 0 },
};

int main(void)
{
	gcfg_file_t df;
	uint64_t out;
	size_t i;

	for (i = 0; i < sizeof(must_work) / sizeof(must_work[0]); ++i) {
		dummy_file_init(&df, must_work[i].input);

		if (gcfg_dec_num(&df, must_work[i].input, &out,
				 must_work[i].maximum) == NULL) {
			fprintf(stderr, "'%s' was rejected!\n",
				must_work[i].input);
			return EXIT_FAILURE;
		}

		if (out != must_work[i].result) {
			fprintf(stderr, "'%s' was parsed as '%lu'!\n",
				must_work[i].input, (unsigned long)out);
			return EXIT_FAILURE;
		}

		dummy_file_cleanup(&df);
	}

	for (i = 0; i < sizeof(must_not_work) / sizeof(must_not_work[0]); ++i) {
		dummy_file_init(&df, must_not_work[i].input);

		if (gcfg_dec_num(&df, must_not_work[i].input, &out,
				 must_not_work[i].maximum) != NULL) {
			fprintf(stderr, "'%s' was accepted!\n",
				must_work[i].input);
			return EXIT_FAILURE;
		}

		dummy_file_cleanup(&df);
	}

	return EXIT_SUCCESS;
}
