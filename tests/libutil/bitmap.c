/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bitmap.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "test.h"
#include "bitmap.h"

int main(void)
{
	bitmap_t *bitmap;
	size_t i;

	/* initialize */
	bitmap = bitmap_create(128);
	TEST_NOT_NULL(bitmap);

	for (i = 0; i < 32768; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	/* set a bit */
	TEST_EQUAL_UI(bitmap_msb_index(bitmap), 0);
	TEST_EQUAL_I(bitmap_set(bitmap, 69), 0);
	TEST_EQUAL_UI(bitmap_msb_index(bitmap), 69);

	for (i = 0; i < 69; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	TEST_ASSERT(bitmap_is_set(bitmap, i++));

	for (; i < 32768; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	/* set another one below the MSB */
	TEST_EQUAL_I(bitmap_set(bitmap, 45), 0);
	TEST_EQUAL_UI(bitmap_msb_index(bitmap), 69);

	for (i = 0; i < 45; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	TEST_ASSERT(bitmap_is_set(bitmap, i++));

	for (; i < 69; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	TEST_ASSERT(bitmap_is_set(bitmap, i++));

	for (; i < 32768; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	/* set one above the MSB that causes the map to grow */
	TEST_EQUAL_I(bitmap_set(bitmap, 8192), 0);
	TEST_EQUAL_UI(bitmap_msb_index(bitmap), 8192);

	for (i = 0; i < 45; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	TEST_ASSERT(bitmap_is_set(bitmap, i++));

	for (; i < 69; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	TEST_ASSERT(bitmap_is_set(bitmap, i++));

	for (; i < 8192; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	TEST_ASSERT(bitmap_is_set(bitmap, i++));

	for (; i < 32768; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	/* clear a bit below the MSB */
	bitmap_clear(bitmap, 69);
	TEST_EQUAL_UI(bitmap_msb_index(bitmap), 8192);

	for (i = 0; i < 45; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	TEST_ASSERT(bitmap_is_set(bitmap, i++));

	for (; i < 8192; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	TEST_ASSERT(bitmap_is_set(bitmap, i++));

	for (; i < 32768; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	/* clear the MSB */
	bitmap_clear(bitmap, 8192);
	TEST_EQUAL_UI(bitmap_msb_index(bitmap), 45);

	for (i = 0; i < 45; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	TEST_ASSERT(bitmap_is_set(bitmap, i++));

	for (; i < 32768; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	/* clear the last remaining bit */
	bitmap_clear(bitmap, 45);
	TEST_EQUAL_UI(bitmap_msb_index(bitmap), 0);

	for (; i < 32768; ++i) {
		TEST_ASSERT(!bitmap_is_set(bitmap, i));
	}

	object_drop(bitmap);
	return EXIT_SUCCESS;
}
