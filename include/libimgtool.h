/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * libimgtool.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef LIBIMGTOOL_H
#define LIBIMGTOOL_H

#include "predef.h"
#include "gcfg.h"

#ifdef __cplusplus
extern "C" {
#endif

gcfg_file_t *open_gcfg_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* LIBIMGTOOL_H */
