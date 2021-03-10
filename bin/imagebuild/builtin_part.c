/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * builtin_part.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

static partition_mgr_t *create_dosmbr(plugin_t *plugin, imgtool_state_t *state,
				      volume_t *parent)
{
	(void)plugin; (void)state;
	return mbrdisk_create(parent);
}

static plugin_t plugin_mbr_part = {
	.type = PLUGIN_TYPE_PARTITION_MGR,
	.name = "dosmbr",
	.create = {
		.part_mgr = create_dosmbr,
	},
};

EXPORT_PLUGIN(plugin_mbr_part)
