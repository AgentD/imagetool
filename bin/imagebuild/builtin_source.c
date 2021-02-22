/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * builtin_source.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

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

static file_source_t *create_dir_source(plugin_t *plugin, const char *arg)
{
	(void)plugin;
	return (file_source_t *)file_source_directory_create(arg);
}

static file_source_t *create_tar_source(plugin_t *plugin, const char *arg)
{
	(void)plugin;
	return (file_source_t *)file_source_tar_create(arg);
}

static plugin_t plugin_listing = {
	.type = PLUGIN_TYPE_FILE_SOURCE,
	.name = "listing",
	.cfg_line_callback = listing_add_line,
	.create = {
		.file_source = create_listing,
	},
};

static plugin_t plugin_dirscan = {
	.type = PLUGIN_TYPE_FILE_SOURCE,
	.name = "dirscan",
	.create = {
		.file_source = create_dir_source,
	},
};

static plugin_t plugin_tarunpack = {
	.type = PLUGIN_TYPE_FILE_SOURCE,
	.name = "tarunpack",
	.create = {
		.file_source = create_tar_source,
	},
};

EXPORT_PLUGIN(plugin_listing)
EXPORT_PLUGIN(plugin_dirscan)
EXPORT_PLUGIN(plugin_tarunpack)
