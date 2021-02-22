/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * builtin_fs.c
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

EXPORT_PLUGIN(plugin_tar)
EXPORT_PLUGIN(plugin_cpio)
EXPORT_PLUGIN(plugin_fat)
