/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bitmap.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef BITMAP_H
#define BITMAP_H

#include "predef.h"

typedef struct bitmap_t bitmap_t;

#ifdef __cplusplus
extern "C" {
#endif

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
