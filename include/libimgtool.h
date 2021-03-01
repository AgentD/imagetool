/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * libimgtool.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef LIBIMGTOOL_H
#define LIBIMGTOOL_H

#include "predef.h"

struct imgtool_state_t {
	object_t base;

	mount_group_t *mg_list;
	mount_group_t *mg_list_last;

	fs_dep_tracker_t *dep_tracker;

	volume_t *out_file;

	plugin_registry_t *registry;

	gcfg_keyword_t *cfg_global;
	gcfg_keyword_t *cfg_fs_or_volume;
	gcfg_keyword_t *cfg_fs_common;
	gcfg_keyword_t *cfg_sources;
};

#ifdef __cplusplus
extern "C" {
#endif

gcfg_file_t *open_gcfg_file(const char *path);

imgtool_state_t *imgtool_state_create(const char *out_path);

int imgtool_state_init_config(imgtool_state_t *state);

int imgtool_process_config_file(imgtool_state_t *state, const char *path);

int imgtool_state_process(imgtool_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* LIBIMGTOOL_H */
