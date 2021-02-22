/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * imagebuild.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

static imgtool_state_t *state = NULL;

/*****************************************************************************/

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

static int cb_add_listing_line(gcfg_file_t *file, object_t *object,
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

static plugin_t plugin_tar = {
	.type = PLUGIN_TYPE_FILESYSTEM,
	.name = "tar",
	.next = NULL,
	.cfg_sub_nodes = NULL,
	.cfg_line_callback = NULL,
	.create = {
		.filesystem = create_tar_fs,
	},
};

static plugin_t plugin_cpio = {
	.type = PLUGIN_TYPE_FILESYSTEM,
	.name = "cpio",
	.next = &plugin_tar,
	.cfg_sub_nodes = NULL,
	.cfg_line_callback = NULL,
	.create = {
		.filesystem = create_cpio_fs,
	},
};

static plugin_t plugin_fat = {
	.type = PLUGIN_TYPE_FILESYSTEM,
	.name = "fat",
	.next = &plugin_cpio,
	.cfg_sub_nodes = NULL,
	.cfg_line_callback = NULL,
	.create = {
		.filesystem = create_fat_fs,
	},
};

static plugin_t plugin_listing = {
	.type = PLUGIN_TYPE_FILE_SOURCE,
	.name = "listing",
	.next = &plugin_fat,
	.cfg_sub_nodes = NULL,
	.cfg_line_callback = cb_add_listing_line,

	.create = {
		.file_source = create_listing,
	},
};

static plugin_t *plugins = &plugin_listing;

/*****************************************************************************/

static plugin_t *find_plugin(PLUGIN_TYPE type, const char *name)
{
	plugin_t *it;

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type == type && strcmp(it->name, name) == 0)
			break;
	}

	return it;
}

static object_t *cb_create_fs(const gcfg_keyword_t *kwd, gcfg_file_t *file,
			      object_t *parent, const char *string)
{
	volume_t *vol = (volume_t *)parent;
	filesystem_t *fs;
	plugin_t *plugin;

	plugin = find_plugin(PLUGIN_TYPE_FILESYSTEM, kwd->name);
	assert(plugin != NULL);

	fs = plugin->create.filesystem(plugin, vol);
	if (fs == NULL) {
		file->report_error(file, "error creating '%s' filesystem '%s'",
				   kwd->name, string);
		return NULL;
	}

	if (fs_dep_tracker_add_fs(state->dep_tracker, fs, vol, string)) {
		file->report_error(file, "error registering "
				   "%s filesystem '%s'", plugin->name, string);
		object_drop(fs);
		return NULL;
	}

	return (object_t *)fs;
}

static object_t *cb_create_data_source(const gcfg_keyword_t *kwd,
				       gcfg_file_t *file, object_t *object,
				       const char *string)
{
	mount_group_t *mg = (mount_group_t *)object;
	file_source_t *src;
	plugin_t *plugin;

	plugin = find_plugin(PLUGIN_TYPE_FILE_SOURCE, kwd->name);
	assert(plugin != NULL);

	src = plugin->create.file_source(plugin, string);
	if (src == NULL) {
		file->report_error(file, "error creating file source");
		return NULL;
	}

	if (mount_group_add_source(mg, src)) {
		file->report_error(file, "error adding source to mount group");
		object_drop(src);
		return NULL;
	}

	return (object_t *)src;
}

/*****************************************************************************/

static object_t *cb_create_volumefile(const gcfg_keyword_t *kwd,
				      gcfg_file_t *file, object_t *parent,
				      const char *string)
{
	filesystem_t *fs = (filesystem_t *)parent;
	volume_t *volume;
	tree_node_t *n;
	(void)kwd;

	n = fstree_add_file(fs->fstree, string);
	if (n == NULL) {
		file->report_error(file, "%s: %s", string, strerror(errno));
		return NULL;
	}

	volume = fstree_file_volume_create(fs->fstree, n,
					   fs->fstree->volume->blocksize,
					   0, 0xFFFFFFFFFFFFFFFFUL);
	if (volume == NULL) {
		file->report_error(file, "%s: %s", string,
				   "error creating volume wrapper");
		return NULL;
	}

	if (fs_dep_tracker_add_volume_file(state->dep_tracker, volume, fs)) {
		file->report_error(file, "%s: %s", string,
				   "error reginstering volume wrapper");
		object_drop(volume);
		return NULL;
	}

	return (object_t *)volume;
}

static gcfg_keyword_t *cfg_filesystems = NULL;
static gcfg_keyword_t *cfg_filesystems_common = NULL;

static int config_init_fs(void)
{
	size_t i, count = 0;
	plugin_t *it;

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type == PLUGIN_TYPE_FILESYSTEM)
			++count;
	}

	cfg_filesystems = calloc(count + 1, sizeof(cfg_filesystems[0]));
	if (cfg_filesystems == NULL) {
		perror("initializing filesystem config parser");
		return -1;
	}

	cfg_filesystems_common = calloc(2, sizeof(cfg_filesystems_common[0]));
	if (cfg_filesystems_common == NULL) {
		perror("initializing filesystem config parser");
		free(cfg_filesystems);
		return -1;
	}

	/* common options block */
	cfg_filesystems_common[0].arg = GCFG_ARG_STRING;
	cfg_filesystems_common[0].name = "volumefile";
	cfg_filesystems_common[0].handle.cb_string = cb_create_volumefile;
	cfg_filesystems_common[0].children = cfg_filesystems;
	cfg_filesystems_common[1].name = NULL;

	/* filesystem options block */
	i = 0;

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type != PLUGIN_TYPE_FILESYSTEM)
			continue;

		cfg_filesystems[i].arg = GCFG_ARG_STRING;
		cfg_filesystems[i].name = it->name;
		cfg_filesystems[i].children = cfg_filesystems_common;
		cfg_filesystems[i].handle.cb_string = cb_create_fs;
		++i;
	}

	cfg_filesystems[i].name = NULL;
	return 0;
}

static void config_cleanup_fs(void)
{
	free(cfg_filesystems_common);
	free(cfg_filesystems);
}

/*****************************************************************************/

static object_t *cb_raw_set_minsize(const gcfg_keyword_t *kwd,
				    gcfg_file_t *file, object_t *obj,
				    uint64_t size)
{
	volume_t *vol = (volume_t *)obj;
	(void)file;
	(void)kwd;

	vol->min_block_count = size / vol->blocksize;
	if (size % vol->blocksize)
		vol->min_block_count += 1;

	return object_grab(vol);
}

static object_t *cb_raw_set_maxsize(const gcfg_keyword_t *kwd,
				    gcfg_file_t *file, object_t *obj,
				    uint64_t size)
{
	volume_t *vol = (volume_t *)obj;
	(void)file;
	(void)kwd;

	vol->max_block_count = size / vol->blocksize;
	return object_grab(vol);
}

static object_t *cb_create_raw_volume(const gcfg_keyword_t *kwd,
				      gcfg_file_t *file, object_t *parent)
{
	(void)file; (void)kwd;
	return object_grab(((imgtool_state_t *)parent)->out_file);
}

static const gcfg_keyword_t cfg_raw_volume[] = {
	{
		.arg = GCFG_ARG_SIZE,
		.name = "minsize",
		.handle = {
			.cb_size = cb_raw_set_minsize,
		},
	}, {
		.arg = GCFG_ARG_SIZE,
		.name = "maxsize",
		.handle = {
			.cb_size = cb_raw_set_maxsize,
		},
	}
};

/*****************************************************************************/

static object_t *cb_mp_add_bind(const gcfg_keyword_t *kwd, gcfg_file_t *file,
				object_t *object, const char *line)
{
	mount_group_t *mg = (mount_group_t *)object;
	filesystem_t *fs;
	const char *ptr;
	size_t len;
	char *temp;
	(void)kwd;

	ptr = strrchr(line, ':');
	if (ptr == NULL || ptr == line || strlen(ptr + 1) == 0) {
		file->report_error(file, "expected \"<path>:<filesystem>\"");
		return NULL;
	}

	fs = fs_dep_tracker_get_fs_by_name(state->dep_tracker, ptr + 1);
	if (fs == NULL) {
		file->report_error(file, "cannot find filesystem '%s'",
				   ptr + 1);
		return NULL;
	}

	len = ptr - line;
	temp = calloc(1, len + 1);
	if (temp == NULL) {
		file->report_error(file, "creating bind entry: %s",
				   strerror(errno));
		object_drop(fs);
		return NULL;
	}

	memcpy(temp, line, len);
	temp[len] = '\0';

	fstree_canonicalize_path(temp);

	if (file_sink_bind(mg->sink, temp, fs)) {
		file->report_error(file, "error creating binding");
		object_drop(fs);
		free(temp);
		return NULL;
	}

	object_drop(fs);
	free(temp);
	return object_grab(object);
}

static object_t *cb_create_mount_group(const gcfg_keyword_t *kwd,
				       gcfg_file_t *file, object_t *object)
{
	mount_group_t *mg;
	(void)kwd;

	mg = imgtool_state_add_mount_group((imgtool_state_t *)object);
	if (mg == NULL) {
		file->report_error(file, "error creating mount group");
		return NULL;
	}

	return (object_t *)mg;
}

static const gcfg_keyword_t cfg_mount_group[] = {
	{
		.arg = GCFG_ARG_STRING,
		.name = "bind",
		.handle = {
			.cb_string = cb_mp_add_bind,
		},
	}
};

static gcfg_keyword_t *cfg_raw_dyn_child = NULL;
static gcfg_keyword_t *cfg_mg_dyn_child = NULL;
static gcfg_keyword_t *cfg_global = NULL;

static int init_config(void)
{
	size_t i, j, count, src_count, fs_count;
	gcfg_keyword_t *list;
	plugin_t *it;

	if (config_init_fs())
		return -1;

	cfg_global = calloc(3, sizeof(cfg_global[0]));
	if (cfg_global == NULL) {
		perror("initializing configuration parser");
		config_cleanup_fs();
		return -1;
	}

	/* raw children */
	fs_count = 0;

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type == PLUGIN_TYPE_FILESYSTEM)
			++fs_count;
	}

	count = sizeof(cfg_raw_volume) / sizeof(cfg_raw_volume[0]) + fs_count;

	list = calloc(count + 1, sizeof(list[0]));
	if (list == NULL) {
		perror("initializing raw volume parser");
		free(cfg_global);
		config_cleanup_fs();
		return -1;
	}

	j = 0;

	count = sizeof(cfg_raw_volume) / sizeof(cfg_raw_volume[0]);
	for (i = 0; i < count; ++i)
		list[j++] = cfg_raw_volume[i];

	for (i = 0; i < fs_count; ++i)
		list[j++] = cfg_filesystems[i];

	list[j].name = NULL;
	cfg_raw_dyn_child = list;

	/* mountgroup children */
	src_count = 0;

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type == PLUGIN_TYPE_FILE_SOURCE)
			++src_count;
	}

	count = sizeof(cfg_mount_group) / sizeof(cfg_mount_group[0]);

	list = calloc(count + src_count + 1, sizeof(list[0]));
	if (list == NULL) {
		perror("initializing mountgroup parser");
		goto fail;
	}

	j = 0;
	for (i = 0; i < count; ++i)
		list[j++] = cfg_mount_group[i];

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type != PLUGIN_TYPE_FILE_SOURCE)
			continue;

		list[j].arg = GCFG_ARG_STRING;
		list[j].name = it->name;
		list[j].handle_listing = it->cfg_line_callback;
		list[j].children = it->cfg_sub_nodes;
		list[j].handle.cb_string = cb_create_data_source;
		++j;
	}

	list[j].name = NULL;
	cfg_mg_dyn_child = list;

	/* root level entries */
	cfg_global[0].arg = GCFG_ARG_NONE;
	cfg_global[0].name = "raw";
	cfg_global[0].children = cfg_raw_dyn_child;
	cfg_global[0].handle.cb_none = cb_create_raw_volume;

	cfg_global[1].arg = GCFG_ARG_NONE;
	cfg_global[1].name = "mountgroup";
	cfg_global[1].children = cfg_mg_dyn_child;
	cfg_global[1].handle.cb_none = cb_create_mount_group;

	cfg_global[2].name = NULL;
	return 0;
fail:
	free(cfg_raw_dyn_child);
	free(cfg_mg_dyn_child);
	free(cfg_global);
	config_cleanup_fs();
	return -1;
}

static void cleanup_config(void)
{
	free(cfg_raw_dyn_child);
	free(cfg_mg_dyn_child);
	free(cfg_global);
	config_cleanup_fs();
}

/*****************************************************************************/

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE;
	gcfg_file_t *gcfg;
	options_t opt;

	process_options(&opt, argc, argv);

	if (init_config())
		return EXIT_FAILURE;

	gcfg = open_gcfg_file(opt.config_path);
	if (gcfg == NULL)
		goto out_config;

	state = imgtool_state_create(opt.output_path);
	if (state == NULL)
		goto out_gcfg;

	if (gcfg_parse_file(gcfg, cfg_global, (object_t *)state))
		goto out;

	if (imgtool_state_process(state))
		goto out;

	status = EXIT_SUCCESS;
out:
	object_drop(state);
out_gcfg:
	object_drop(gcfg);
out_config:
	cleanup_config();
	return status;
}
