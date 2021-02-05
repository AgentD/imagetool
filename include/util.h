/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * util.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MUL64_OV(a, b, res) __builtin_mul_overflow(a, b, res)

#ifdef __cplusplus
extern "C" {
#endif

/*
  Returns true if the given region of memory is filled with zero-bytes only.
 */
bool is_memory_zero(const void *blob, size_t size);

int read_retry(const char *filename, int fd, uint64_t offset,
	       void *data, size_t size);

int write_retry(const char *filename, int fd, uint64_t offset,
		const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_H */
