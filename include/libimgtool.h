/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * libimgtool.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef LIBIMGTOOL_H
#define LIBIMGTOOL_H

#include "fsdeptracker.h"
#include "filesource.h"
#include "filesink.h"
#include "gcfg.h"

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


typedef enum {
	PLUGIN_TYPE_FILESYSTEM = 0,
	PLUGIN_TYPE_FILE_SOURCE,
	PLUGIN_TYPE_FILE_SOURCE_STACKABLE,
	PLUGIN_TYPE_VOLUME,
} PLUGIN_TYPE;

struct plugin_t {
	PLUGIN_TYPE type;

	const char *name;

	plugin_t *next;

	gcfg_keyword_t *cfg_sub_nodes;

	int (*cfg_line_callback)(gcfg_file_t *file, object_t *object,
				 const char *line);

	union {
		filesystem_t *(*filesystem)(plugin_t *plugin, volume_t *parent);

		file_source_t *(*file_source)(plugin_t *plugin,
					      const char *arg);

		file_source_stackable_t *(*stackable_source)(plugin_t *plugin);

		volume_t *(*volume)(plugin_t *plugin, imgtool_state_t *state);
	} create;
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
