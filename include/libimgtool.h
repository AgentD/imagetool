/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * libimgtool.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef LIBIMGTOOL_H
#define LIBIMGTOOL_H

#include "predef.h"

struct mount_group_t {
	object_t base;

	mount_group_t *next;
	file_sink_t *sink;
	file_source_t *source;
	bool have_aggregate;
};

struct imgtool_state_t {
	object_t base;

	mount_group_t *mg_list;
	mount_group_t *mg_list_last;

	fs_dep_tracker_t *dep_tracker;

	volume_t *out_file;
};

#ifdef __cplusplus
extern "C" {
#endif

gcfg_file_t *open_gcfg_file(const char *path);

imgtool_state_t *imgtool_state_create(const char *out_path);

mount_group_t *imgtool_state_add_mount_group(imgtool_state_t *state);

int mount_group_add_source(mount_group_t *mg, file_source_t *source);

int imgtool_state_process(imgtool_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* LIBIMGTOOL_H */
