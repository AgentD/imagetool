/* SPDX-License-Identifier: ISC */
/*
 * is_valid_utf8.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"
#include "test.h"

static const char *must_work[] = {
	"Hello, world!",
	"这是一些中文。",
	"هذه هي بعض النصوص العربي",
	"019\t\t { foo } äöü ;",
};

static const char *must_not_work[] = {
	"Foo\nBar",
	"Foo\rBar",
	"Foo\bBar",
	"\xC0\xA0",
	"\x7F",
	"\xFF",
};

int main(void)
{
	size_t i;

	for (i = 0; i < sizeof(must_work) / sizeof(must_work[0]); ++i) {
		if (!gcfg_is_valid_utf8((const uint8_t *)must_work[i],
					strlen(must_work[i]))) {
			fprintf(stderr, "'%s' was rejected!\n", must_work[i]);
			return EXIT_FAILURE;
		}
	}

	for (i = 0; i < sizeof(must_not_work) / sizeof(must_not_work[0]); ++i) {
		if (gcfg_is_valid_utf8((const uint8_t *)must_not_work[i],
				       strlen(must_not_work[i]))) {
			fprintf(stderr, "entry %lu was accepted!\n",
				(unsigned long)i);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
