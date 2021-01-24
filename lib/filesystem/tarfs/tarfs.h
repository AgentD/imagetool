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
#include "filesystem.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int tarfs_write_tree_dfs(ostream_t *out, unsigned int *counter,
			 tree_node_t *n);

int tarfs_write_file_hdr(ostream_t *out, unsigned int *counter,
			 tree_node_t *n);

int tarfs_write_hard_link(ostream_t *out, unsigned int *counter,
			  tree_node_t *n);

#ifdef __cplusplus
}
#endif

#endif /* TARFS_H */
