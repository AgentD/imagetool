/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * builtin.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

static filesystem_t *create_tar_fs(plugin_t *plugin, volume_t *parent)
{
	(void)plugin;
	return filesystem_tar_create(parent);
}

static filesystem_t *create_cpio_fs(plugin_t *plugin, volume_t *parent)
{
	(void)plugin;
	return filesystem_cpio_create(parent);
}

static filesystem_t *create_fat_fs(plugin_t *plugin, volume_t *parent)
{
	(void)plugin;
	return filesystem_fatfs_create(parent);
}

static int listing_add_line(gcfg_file_t *file, object_t *object,
			    const char *line)
{
	file_source_listing_t *list = (file_source_listing_t *)object;

	while (isspace(*line))
		++line;

	if (*line == '#' || *line == '\n')
		return 0;

	if (*line == '}')
		return 1;

	return file_source_listing_add_line(list, line, file);
}

static file_source_t *create_listing(plugin_t *plugin, const char *arg)
{
	(void)plugin;
	return (file_source_t *)file_source_listing_create(arg);
}

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

static plugin_t plugin_tar = {
	.type = PLUGIN_TYPE_FILESYSTEM,
	.name = "tar",
	.create = {
		.filesystem = create_tar_fs,
	},
};

static plugin_t plugin_cpio = {
	.type = PLUGIN_TYPE_FILESYSTEM,
	.name = "cpio",
	.create = {
		.filesystem = create_cpio_fs,
	},
};

static plugin_t plugin_fat = {
	.type = PLUGIN_TYPE_FILESYSTEM,
	.name = "fat",
	.create = {
		.filesystem = create_fat_fs,
	},
};

static plugin_t plugin_listing = {
	.type = PLUGIN_TYPE_FILE_SOURCE,
	.name = "listing",
	.cfg_line_callback = listing_add_line,
	.create = {
		.file_source = create_listing,
	},
};

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

EXPORT_PLUGIN(plugin_tar)
EXPORT_PLUGIN(plugin_cpio)
EXPORT_PLUGIN(plugin_fat)
EXPORT_PLUGIN(plugin_listing)
EXPORT_PLUGIN(plugin_raw_volume)
