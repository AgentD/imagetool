/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gcfg_file.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "test.h"
#include "libimgtool.h"

static const char *lines[] = {
	"The quick",
	"brown fox",
	"jumps over",
	"the",
	"lazy",
	"dog",
};

int main(void)
{
	gcfg_file_t *fp;
	size_t i;
	int ret;

	fp = open_gcfg_file(STRVALUE(TESTFILE));
	TEST_NOT_NULL(fp);
	TEST_NULL(fp->buffer);
	TEST_EQUAL_UI(((object_t *)fp)->refcount, 1);

	for (i = 0; i < (sizeof(lines) / sizeof(lines[0])); ++i) {
		ret = fp->fetch_line(fp);
		TEST_EQUAL_I(ret, 0);
		TEST_NOT_NULL(fp->buffer);

		TEST_STR_EQUAL(fp->buffer, lines[i]);
	}

	ret = fp->fetch_line(fp);
	TEST_ASSERT(ret > 0);
	TEST_NULL(fp->buffer);

	ret = fp->fetch_line(fp);
	TEST_ASSERT(ret > 0);
	TEST_NULL(fp->buffer);

	object_drop(fp);
	return EXIT_SUCCESS;
}
