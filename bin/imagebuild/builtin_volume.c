/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * builtin_volume.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

static object_t *raw_set_minsize(const gcfg_keyword_t *kwd, gcfg_file_t *file,
				 object_t *obj, uint64_t size)
{
	volume_t *vol = (volume_t *)obj;
	(void)file;
	(void)kwd;

	vol->min_block_count = size / vol->blocksize;
	if (size % vol->blocksize)
		vol->min_block_count += 1;

	return object_grab(vol);
}

static object_t *raw_set_maxsize(const gcfg_keyword_t *kwd, gcfg_file_t *file,
				 object_t *obj, uint64_t size)
{
	volume_t *vol = (volume_t *)obj;
	(void)file;
	(void)kwd;

	vol->max_block_count = size / vol->blocksize;
	return object_grab(vol);
}

static volume_t *create_raw_volume(plugin_t *plugin, imgtool_state_t *state)
{
	(void)plugin;
	return object_grab(state->out_file);
}

static gcfg_keyword_t cfg_raw_volume[] = {
	{
		.arg = GCFG_ARG_SIZE,
		.name = "minsize",
		.handle = {
			.cb_size = raw_set_minsize,
		},
	}, {
		.arg = GCFG_ARG_SIZE,
		.name = "maxsize",
		.handle = {
			.cb_size = raw_set_maxsize,
		},
	}, {
		.name = NULL,
	},
};

static plugin_t plugin_raw_volume = {
	.type = PLUGIN_TYPE_VOLUME,
	.name = "raw",
	.cfg_sub_nodes = cfg_raw_volume,
	.create = {
		.volume = create_raw_volume,
	},
};

EXPORT_PLUGIN(plugin_raw_volume)
