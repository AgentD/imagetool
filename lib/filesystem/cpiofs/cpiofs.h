/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * cpiofs.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef CPIOFS_H
#define CPIOFS_H

#include "config.h"
#include "filesystem.h"

#include <sys/types.h>
#ifdef __linux__
#include <sys/sysmacros.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int cpio_write_header(ostream_t *strm, tree_node_t *n, bool hardlink);

int cpio_write_trailer(uint64_t offset, ostream_t *strm);

#endif /* CPIOFS_H */
