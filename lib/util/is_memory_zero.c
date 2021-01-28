/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * is_memory_zero.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util.h"

#include <stdint.h>

#define U64THRESHOLD (128)

static bool test_u8(const unsigned char *blob, size_t size)
{
	while (size--) {
		if (*(blob++) != 0)
			return false;
	}

	return true;
}

bool is_memory_zero(const void *blob, size_t size)
{
	const uint64_t *u64ptr;
	size_t diff;

	if (size < U64THRESHOLD)
		return test_u8(blob, size);

	diff = (uintptr_t)blob % sizeof(uint64_t);

	if (diff != 0) {
		diff = sizeof(uint64_t) - diff;

		if (!test_u8(blob, diff))
			return false;

		blob = (const char *)blob + diff;
		size -= diff;
	}

	u64ptr = blob;

	while (size >= sizeof(uint64_t)) {
		if (*(u64ptr++) != 0)
			return false;

		size -= sizeof(uint64_t);
	}

	return test_u8((const unsigned char *)u64ptr, size);
}
