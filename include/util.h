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

#ifdef __cplusplus
extern "C" {
#endif

bool is_memory_zero(const void *blob, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_H */
