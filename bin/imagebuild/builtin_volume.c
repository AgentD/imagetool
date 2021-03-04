/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * builtin_volume.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

static volume_t *create_raw_volume(plugin_t *plugin, imgtool_state_t *state,
				   volume_t *parent)
{
	(void)plugin; (void)state;
	return object_grab(parent);
}

static plugin_t plugin_raw_volume = {
	.type = PLUGIN_TYPE_VOLUME,
	.name = "raw",
	.create = {
		.volume = create_raw_volume,
	},
};

EXPORT_PLUGIN(plugin_raw_volume)
