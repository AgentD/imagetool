/* SPDX-License-Identifier: ISC */
/*
 * test.h
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gcfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void dummy_file_init(gcfg_file_t *f, const char *line);

void dummy_file_cleanup(gcfg_file_t *f);

#ifdef __cplusplus
}
#endif

#endif /* TEST_H */
