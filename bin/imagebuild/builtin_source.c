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

	return list->add_line(list, line, file);
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

static file_source_stackable_t *create_filter_source(plugin_t *plugin)
{
	(void)plugin;
	return (file_source_stackable_t *)file_source_filter_create();
}

static object_t *cb_filter_allow(const gcfg_keyword_t *kwd,
				 gcfg_file_t *file, object_t *parent,
				 const char *string)
{
	file_source_filter_t *filter = (file_source_filter_t *)parent;
	(void)kwd;

	if (filter->add_glob_rule(filter, string, FILE_SOURCE_FILTER_ALLOW)) {
		file->report_error(file, "internal error storing glob rule");
		return NULL;
	}

	return object_grab(filter);
}

static object_t *cb_filter_discard(const gcfg_keyword_t *kwd,
				   gcfg_file_t *file, object_t *parent,
				   const char *string)
{
	file_source_filter_t *filter = (file_source_filter_t *)parent;
	(void)kwd;

	if (filter->add_glob_rule(filter, string, FILE_SOURCE_FILTER_DISCARD)) {
		file->report_error(file, "internal error storing glob rule");
		return NULL;
	}

	return object_grab(filter);
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

static gcfg_keyword_t filter_discard = {
	.arg = GCFG_ARG_STRING,
	.name = "discard",
	.handle = {
		.cb_string = cb_filter_discard,
	},
	.next = NULL,
};

static gcfg_keyword_t filter_allow = {
	.arg = GCFG_ARG_STRING,
	.name = "allow",
	.handle = {
		.cb_string = cb_filter_allow,
	},
	.next = &filter_discard,
};

static plugin_t plugin_filter = {
	.type = PLUGIN_TYPE_FILE_SOURCE_STACKABLE,
	.name = "filter",
	.cfg_sub_nodes = &filter_allow,
	.create = {
		.stackable_source = create_filter_source,
	},
};

EXPORT_PLUGIN(plugin_listing)
EXPORT_PLUGIN(plugin_dirscan)
EXPORT_PLUGIN(plugin_tarunpack)
EXPORT_PLUGIN(plugin_filter)
