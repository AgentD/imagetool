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
	PLUGIN_TYPE_VOLUME,
	PLUGIN_TYPE_PARTITION_MGR,

	PLUGIN_TYPE_COUNT,
} PLUGIN_TYPE;

typedef enum {
	PLUGIN_FLAG_RECURSIVE_SOURCE = 0x00000001,
} PLUGIN_FLAGS;

struct plugin_t {
	PLUGIN_TYPE type;
	uint32_t flags;

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

		volume_t *(*volume)(plugin_t *plugin, imgtool_state_t *state,
				    volume_t *parent);

		partition_mgr_t *(*part_mgr)(plugin_t *plugin,
					     imgtool_state_t *state,
					     volume_t *parent);
	} create;
};

struct plugin_registry_t {
	object_t base;

	plugin_t *plugins[PLUGIN_TYPE_COUNT];
};

#ifdef __cplusplus
extern "C" {
#endif

plugin_registry_t *plugin_registry_create(void);

int plugin_registry_add_plugin(plugin_registry_t *registry, plugin_t *plugin);

plugin_t *plugin_registry_find_plugin(plugin_registry_t *registry, int type,
				      const char *name);

#ifdef __cplusplus
}
#endif

#endif /* PLUGIN_H */
