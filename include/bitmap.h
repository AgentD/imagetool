/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bitmap.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef BITMAP_H
#define BITMAP_H

#include "predef.h"

typedef uint64_t bitmap_word_t;

typedef struct {
	object_t base;

	size_t word_count;
	bitmap_word_t *words;
} bitmap_t;

#ifdef __cplusplus
extern "C" {
#endif

int bitmap_init(bitmap_t *bitmap, size_t bit_count);

void bitmap_cleanup(bitmap_t *bitmap);

bitmap_t *bitmap_create(size_t bit_count);

bool bitmap_is_set(bitmap_t *bitmap, size_t index);

int bitmap_set(bitmap_t *bitmap, size_t index);

void bitmap_clear(bitmap_t *bitmap, size_t index);

int bitmap_set_value(bitmap_t *bitmap, size_t index, bool set);

void bitmap_shrink(bitmap_t *bitmap);

size_t bitmap_msb_index(bitmap_t *bitmap);

#ifdef __cplusplus
}
#endif

#endif /* BITMAP_H */
