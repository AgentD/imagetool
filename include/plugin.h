/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * plugin.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef PLUGIN_H
#define PLUGIN_H

#include "predef.h"

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

#endif /* PLUGIN_H */
