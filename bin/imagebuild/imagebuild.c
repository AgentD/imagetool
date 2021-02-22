/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * imagebuild.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

static imgtool_state_t *state = NULL;

plugin_t *plugins;

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

typedef struct config_vector_t {
	struct config_vector_t *next;

	size_t count;
	gcfg_keyword_t entries[];
} config_vector_t;

static config_vector_t *dyn_config = NULL;

static void dyn_config_cleanup(void)
{
	while (dyn_config != NULL) {
		config_vector_t *ent = dyn_config;
		dyn_config = dyn_config->next;
		free(ent);
	}
}

static config_vector_t *dyn_config_add(size_t count)
{
	config_vector_t *ent = calloc(1, sizeof(*ent) +
				      (count + 1) * sizeof(ent->entries[0]));

	if (ent == NULL) {
		perror("initializing configuration parser");
		return NULL;
	}

	ent->next = dyn_config;
	dyn_config = ent;
	ent->count = count;
	return ent;
}

static config_vector_t *dyn_config_combine(plugin_t *plugin,
					   const gcfg_keyword_t *base)
{
	size_t i, j, base_count = 0, sub_count = 0;
	config_vector_t *vec;

	if (base != NULL) {
		while (base[base_count].name != NULL)
			++base_count;
	}

	if (plugin->cfg_sub_nodes != NULL) {
		while (plugin->cfg_sub_nodes[sub_count].name != NULL)
			++sub_count;
	}

	vec = dyn_config_add(base_count + sub_count);
	if (vec == NULL)
		return NULL;

	j = 0;

	for (i = 0; i < base_count; ++i)
		vec->entries[j++] = base[i];

	for (i = 0; i < sub_count; ++i)
		vec->entries[j++] = plugin->cfg_sub_nodes[i];

	return vec;
}

/*****************************************************************************/

static config_vector_t *cfg_filesystems = NULL;

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

static int config_init_fs(void)
{
	config_vector_t *fs_common = NULL;
	config_vector_t *vec;
	size_t i, count = 0;
	plugin_t *it;

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type == PLUGIN_TYPE_FILESYSTEM)
			++count;
	}

	cfg_filesystems = dyn_config_add(count);
	if (cfg_filesystems == NULL)
		return -1;

	/* common options block */
	fs_common = dyn_config_add(1);
	if (fs_common == NULL)
		return -1;

	fs_common->entries[0].arg = GCFG_ARG_STRING;
	fs_common->entries[0].name = "volumefile";
	fs_common->entries[0].handle.cb_string = cb_create_volumefile;
	fs_common->entries[0].children = cfg_filesystems->entries;

	/* filesystem options block */
	i = 0;

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type != PLUGIN_TYPE_FILESYSTEM)
			continue;

		if (it->cfg_sub_nodes == NULL) {
			vec = fs_common;
		} else {
			vec = dyn_config_combine(it, fs_common->entries);
			if (vec == NULL)
				return -1;
		}

		cfg_filesystems->entries[i].arg = GCFG_ARG_STRING;
		cfg_filesystems->entries[i].name = it->name;
		cfg_filesystems->entries[i].children = vec->entries;
		cfg_filesystems->entries[i].handle.cb_string = cb_create_fs;
		++i;
	}

	return 0;
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

static config_vector_t *cfg_global = NULL;

static int init_config(void)
{
	config_vector_t *raw_opt, *mount_opt;
	size_t i, j, count, src_count;
	plugin_t *it;

	if (config_init_fs())
		return -1;

	/* raw children */
	count = sizeof(cfg_raw_volume) / sizeof(cfg_raw_volume[0]);

	raw_opt = dyn_config_add(count + cfg_filesystems->count);
	if (raw_opt == NULL)
		return -1;

	j = 0;

	for (i = 0; i < count; ++i)
		raw_opt->entries[j++] = cfg_raw_volume[i];

	for (i = 0; i < cfg_filesystems->count; ++i)
		raw_opt->entries[j++] = cfg_filesystems->entries[i];

	/* mountgroup children */
	src_count = 0;
	count = sizeof(cfg_mount_group) / sizeof(cfg_mount_group[0]);

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type == PLUGIN_TYPE_FILE_SOURCE)
			++src_count;
	}

	mount_opt = dyn_config_add(count + src_count);
	if (mount_opt == NULL)
		return -1;

	j = 0;

	for (i = 0; i < count; ++i)
		mount_opt->entries[j++] = cfg_mount_group[i];

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type != PLUGIN_TYPE_FILE_SOURCE)
			continue;

		mount_opt->entries[j].arg = GCFG_ARG_STRING;
		mount_opt->entries[j].name = it->name;
		mount_opt->entries[j].handle_listing = it->cfg_line_callback;
		mount_opt->entries[j].children = it->cfg_sub_nodes;
		mount_opt->entries[j].handle.cb_string = cb_create_data_source;
		++j;
	}

	/* root level entries */
	cfg_global = dyn_config_add(2);
	if (cfg_global == NULL)
		return -1;

	cfg_global->entries[0].arg = GCFG_ARG_NONE;
	cfg_global->entries[0].name = "raw";
	cfg_global->entries[0].children = raw_opt->entries;
	cfg_global->entries[0].handle.cb_none = cb_create_raw_volume;

	cfg_global->entries[1].arg = GCFG_ARG_NONE;
	cfg_global->entries[1].name = "mountgroup";
	cfg_global->entries[1].children = mount_opt->entries;
	cfg_global->entries[1].handle.cb_none = cb_create_mount_group;
	return 0;
}

/*****************************************************************************/

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE;
	gcfg_file_t *gcfg;
	options_t opt;

	process_options(&opt, argc, argv);

	if (init_config())
		goto out_config;

	gcfg = open_gcfg_file(opt.config_path);
	if (gcfg == NULL)
		goto out_config;

	state = imgtool_state_create(opt.output_path);
	if (state == NULL)
		goto out_gcfg;

	if (gcfg_parse_file(gcfg, cfg_global->entries, (object_t *)state))
		goto out;

	if (imgtool_state_process(state))
		goto out;

	status = EXIT_SUCCESS;
out:
	object_drop(state);
out_gcfg:
	object_drop(gcfg);
out_config:
	dyn_config_cleanup();
	return status;
}
