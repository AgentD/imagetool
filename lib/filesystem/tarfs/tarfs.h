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

#include <sys/sysmacros.h>
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

int padd_file(ostream_t *fp, uint64_t size);

int write_tar_header(ostream_t *fp, const struct stat *sb, const char *name,
		     const char *slink_target, const tar_xattr_t *xattr,
		     unsigned int counter);

int write_hard_link(ostream_t *fp, const struct stat *sb, const char *name,
		    const char *target, unsigned int counter);

#ifdef __cplusplus
}
#endif

#endif /* TARFS_H */
