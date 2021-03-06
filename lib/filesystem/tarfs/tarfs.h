/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tarfs.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef TARFS_H
#define TARFS_H

#include "config.h"

#include "tar.h"
#include "fstree.h"
#include "volume.h"
#include "filesystem.h"

#include <sys/types.h>
#ifdef __linux__
#include <sys/sysmacros.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int tarfs_write_header(ostream_t *fp, const tree_node_t *n,
		       unsigned int counter);

int tarfs_write_hard_link(ostream_t *fp, const tree_node_t *n,
			  unsigned int counter);

#ifdef __cplusplus
}
#endif

#endif /* TARFS_H */
