/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * imagebuild.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

static object_t *cb_create_fs(const gcfg_keyword_t *kwd, gcfg_file_t *file,
			      object_t *parent, const char *string)
{
	volume_t *vol = (volume_t *)parent;
	imgtool_state_t *state = kwd->user;
	filesystem_t *fs;
	plugin_t *plugin;

	plugin = plugin_registry_find_plugin(state->registry,
					     PLUGIN_TYPE_FILESYSTEM,
					     kwd->name);
	assert(plugin != NULL);

	fs = plugin->create.filesystem(plugin, vol);
	if (fs == NULL) {
		file->report_error(file, "error creating '%s' filesystem '%s'",
				   kwd->name, string);
		return NULL;
	}

	if (state->dep_tracker->add_fs(state->dep_tracker, fs, vol, string)) {
		file->report_error(file, "error registering "
				   "%s filesystem '%s'", plugin->name, string);
		object_drop(fs);
		return NULL;
	}

	return (object_t *)fs;
}

static object_t *cb_create_stackable_source(const gcfg_keyword_t *kwd,
					    gcfg_file_t *file, object_t *object)
{
	mount_group_t *mg = (mount_group_t *)object;
	imgtool_state_t *state = kwd->user;
	file_source_stackable_t *src;
	plugin_t *plugin;

	plugin = plugin_registry_find_plugin(state->registry,
					     PLUGIN_TYPE_FILE_SOURCE_STACKABLE,
					     kwd->name);
	assert(plugin != NULL);

	src = plugin->create.stackable_source(plugin);
	if (src == NULL) {
		file->report_error(file, "error creating file source");
		return NULL;
	}

	if (mount_group_add_source(mg, (file_source_t *)src)) {
		file->report_error(file, "error adding source to mount group");
		object_drop(src);
		return NULL;
	}

	return (object_t *)src;
}

static object_t *cb_create_data_source(const gcfg_keyword_t *kwd,
				       gcfg_file_t *file, object_t *object,
				       const char *string)
{
	mount_group_t *mg = (mount_group_t *)object;
	imgtool_state_t *state = kwd->user;
	file_source_t *src;
	plugin_t *plugin;

	plugin = plugin_registry_find_plugin(state->registry,
					     PLUGIN_TYPE_FILE_SOURCE,
					     kwd->name);
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

static object_t *cb_create_stackable_sub_source(const gcfg_keyword_t *kwd,
						gcfg_file_t *file, object_t *object)
{
	file_source_stackable_t *parent = (file_source_stackable_t *)object;
	imgtool_state_t *state = kwd->user;
	file_source_stackable_t *src;
	plugin_t *plugin;

	plugin = plugin_registry_find_plugin(state->registry,
					     PLUGIN_TYPE_FILE_SOURCE_STACKABLE,
					     kwd->name);
	assert(plugin != NULL);

	src = plugin->create.stackable_source(plugin);
	if (src == NULL) {
		file->report_error(file, "error creating file source");
		return NULL;
	}

	if (parent->add_nested(parent, (file_source_t *)src)) {
		file->report_error(file, "error adding source to parent");
		object_drop(src);
		return NULL;
	}

	return (object_t *)src;
}

static object_t *cb_create_data_sub_source(const gcfg_keyword_t *kwd,
					   gcfg_file_t *file, object_t *object,
					   const char *string)
{
	file_source_stackable_t *parent = (file_source_stackable_t *)object;
	imgtool_state_t *state = kwd->user;
	file_source_t *src;
	plugin_t *plugin;

	plugin = plugin_registry_find_plugin(state->registry,
					     PLUGIN_TYPE_FILE_SOURCE,
					     kwd->name);
	assert(plugin != NULL);

	src = plugin->create.file_source(plugin, string);
	if (src == NULL) {
		file->report_error(file, "error creating file source");
		return NULL;
	}

	if (parent->add_nested(parent, src)) {
		file->report_error(file, "error adding source to parent");
		object_drop(src);
		return NULL;
	}

	return (object_t *)src;
}

static object_t *cb_create_volume(const gcfg_keyword_t *kwd,
				  gcfg_file_t *file, object_t *parent)
{
	imgtool_state_t *state = kwd->user;
	volume_t *volume;
	plugin_t *plugin;

	plugin = plugin_registry_find_plugin(state->registry,
					     PLUGIN_TYPE_VOLUME,
					     kwd->name);
	assert(plugin != NULL);

	volume = plugin->create.volume(plugin, (imgtool_state_t *)parent);
	if (volume == NULL) {
		file->report_error(file, "error creating %s volume",
				   kwd->name);
		return NULL;
	}

	return (object_t *)volume;
}

/*****************************************************************************/

static object_t *cb_create_volumefile(const gcfg_keyword_t *kwd,
				      gcfg_file_t *file, object_t *parent,
				      const char *string)
{
	filesystem_t *fs = (filesystem_t *)parent;
	imgtool_state_t *state = kwd->user;
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

	if (state->dep_tracker->add_volume_file(state->dep_tracker,
						volume, fs)) {
		file->report_error(file, "%s: %s", string,
				   "error reginstering volume wrapper");
		object_drop(volume);
		return NULL;
	}

	return (object_t *)volume;
}

static object_t *cb_mp_add_bind(const gcfg_keyword_t *kwd, gcfg_file_t *file,
				object_t *object, const char *line)
{
	mount_group_t *mg = (mount_group_t *)object;
	imgtool_state_t *state = kwd->user;
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

	fs = state->dep_tracker->get_fs_by_name(state->dep_tracker, ptr + 1);
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

static gcfg_keyword_t cfg_mount_group_bind = {
	.arg = GCFG_ARG_STRING,
	.name = "bind",
	.next = NULL,
	.handle = {
		.cb_string = cb_mp_add_bind,
	},
};

static gcfg_keyword_t *cfg_global = NULL;
static gcfg_keyword_t *cfg_filesystems = NULL;
static gcfg_keyword_t *cfg_fs_common = NULL;
static gcfg_keyword_t *cfg_sources = NULL;
static gcfg_keyword_t *cfg_sources_rec = NULL;
static imgtool_state_t *state = NULL;

static int init_config(void)
{
	gcfg_keyword_t *kwd_it, *copy, *last;
	plugin_t *it;

	/* filesystems */
	cfg_fs_common = calloc(1, sizeof(cfg_fs_common[0]));
	if (cfg_fs_common == NULL)
		goto fail;

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type != PLUGIN_TYPE_FILESYSTEM)
			continue;

		kwd_it = calloc(1, sizeof(*kwd_it));
		if (kwd_it == NULL)
			goto fail;

		kwd_it->arg = GCFG_ARG_STRING;
		kwd_it->name = it->name;
		kwd_it->children = it->cfg_sub_nodes;
		kwd_it->handle_listing = it->cfg_line_callback;
		kwd_it->handle.cb_string = cb_create_fs;
		kwd_it->user = state;

		kwd_it->next = cfg_filesystems;
		cfg_filesystems = kwd_it;

		if (kwd_it->children == NULL) {
			kwd_it->children = cfg_fs_common;
		} else {
			kwd_it = kwd_it->children;
			while (kwd_it->next != NULL)
				kwd_it = kwd_it->next;

			kwd_it->next = cfg_fs_common;
		}
	}

	cfg_fs_common[0].arg = GCFG_ARG_STRING;
	cfg_fs_common[0].name = "volumefile";
	cfg_fs_common[0].handle.cb_string = cb_create_volumefile;
	cfg_fs_common[0].user = state;
	cfg_fs_common[0].next = NULL;
	cfg_fs_common[0].children = cfg_filesystems;

	/* file sources */
	cfg_sources_rec = NULL;
	last = cfg_sources_rec;

	for (it = plugins; it != NULL; it = it->next) {
		if (it->type != PLUGIN_TYPE_FILE_SOURCE &&
		    it->type != PLUGIN_TYPE_FILE_SOURCE_STACKABLE) {
			continue;
		}

		kwd_it = calloc(1, sizeof(*kwd_it));
		if (kwd_it == NULL)
			goto fail;

		if (it->type == PLUGIN_TYPE_FILE_SOURCE_STACKABLE) {
			kwd_it->arg = GCFG_ARG_NONE;
			kwd_it->handle.cb_none = cb_create_stackable_source;
		} else {
			kwd_it->arg = GCFG_ARG_STRING;
			kwd_it->handle.cb_string = cb_create_data_source;
		}

		kwd_it->user = state;
		kwd_it->name = it->name;
		kwd_it->children = it->cfg_sub_nodes;
		kwd_it->handle_listing = it->cfg_line_callback;
		kwd_it->next = cfg_sources;
		cfg_sources = kwd_it;

		/* copy in the nested sources list */
		copy = calloc(1, sizeof(*copy));
		if (copy == NULL)
			goto fail;

		memcpy(copy, kwd_it, sizeof(*copy));
		copy->next = NULL;

		if (last == NULL) {
			cfg_sources_rec = copy;
			last = copy;
		} else {
			last->next = copy;
			last = copy;
		}

		if (it->type == PLUGIN_TYPE_FILE_SOURCE_STACKABLE) {
			copy->handle.cb_none = cb_create_stackable_sub_source;
		} else {
			copy->handle.cb_string = cb_create_data_sub_source;
		}

		/* rewire the list of children */
		if (it->type == PLUGIN_TYPE_FILE_SOURCE_STACKABLE) {
			if (kwd_it->children == NULL) {
				kwd_it->children = cfg_sources_rec;
				copy->children = cfg_sources_rec;
			} else {
				kwd_it = kwd_it->children;
				while (kwd_it->next != NULL)
					kwd_it = kwd_it->next;

				kwd_it->next = cfg_sources_rec;
			}
		}
	}

	/* root level entries */
	for (it = plugins; it != NULL; it = it->next) {
		if (it->type != PLUGIN_TYPE_VOLUME)
			continue;

		kwd_it = calloc(1, sizeof(*kwd_it));
		if (kwd_it == NULL)
			goto fail;

		kwd_it->arg = GCFG_ARG_NONE;
		kwd_it->name = it->name;
		kwd_it->user = state;
		kwd_it->children = it->cfg_sub_nodes;
		kwd_it->handle_listing = it->cfg_line_callback;
		kwd_it->handle.cb_none = cb_create_volume;
		kwd_it->next = cfg_global;
		cfg_global = kwd_it;

		if (kwd_it->children == NULL) {
			kwd_it->children = cfg_filesystems;
		} else {
			kwd_it = kwd_it->children;
			while (kwd_it->next != NULL)
				kwd_it = kwd_it->next;

			kwd_it->next = cfg_filesystems;
		}
	}

	kwd_it = calloc(1, sizeof(*kwd_it));
	if (kwd_it == NULL)
		goto fail;

	kwd_it->arg = GCFG_ARG_NONE;
	kwd_it->name = "mountgroup";
	kwd_it->children = &cfg_mount_group_bind;
	kwd_it->handle.cb_none = cb_create_mount_group;
	kwd_it->user = state;
	kwd_it->next = cfg_global;
	cfg_global = kwd_it;

	if (kwd_it->children == NULL) {
		kwd_it->children = cfg_sources;
	} else {
		kwd_it = kwd_it->children;

		while (kwd_it->next != NULL) {
			kwd_it->user = state;
			kwd_it = kwd_it->next;
		}

		kwd_it->user = state;
		kwd_it->next = cfg_sources;
	}

	return 0;
fail:
	perror("initializing configuration parser");
	return -1;
}

static void config_cleanup(void)
{
	gcfg_keyword_t *it;

	while (cfg_global != NULL) {
		it = cfg_global;
		cfg_global = cfg_global->next;
		free(it);
	}

	while (cfg_filesystems != NULL) {
		it = cfg_filesystems;
		cfg_filesystems = cfg_filesystems->next;
		free(it);
	}

	while (cfg_fs_common != NULL) {
		it = cfg_fs_common;
		cfg_fs_common = cfg_fs_common->next;
		free(it);
	}

	while (cfg_sources != NULL) {
		it = cfg_sources;
		cfg_sources = cfg_sources->next;
		free(it);
	}

	while (cfg_sources_rec != NULL) {
		it = cfg_sources_rec;
		cfg_sources_rec = cfg_sources_rec->next;
		free(it);
	}
}

/*****************************************************************************/

plugin_t *plugins;

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE;
	gcfg_file_t *gcfg;
	options_t opt;

	process_options(&opt, argc, argv);

	state = imgtool_state_create(opt.output_path);
	if (state == NULL)
		return EXIT_FAILURE;

	if (init_config())
		goto out_config;

	while (plugins != NULL) {
		plugin_t *it = plugins;
		plugins = plugins->next;

		if (plugin_registry_add_plugin(state->registry, it))
			goto out_config;
	}

	gcfg = open_gcfg_file(opt.config_path);
	if (gcfg == NULL)
		goto out_config;

	if (gcfg_parse_file(gcfg, cfg_global, (object_t *)state))
		goto out;

	if (imgtool_state_process(state))
		goto out;

	status = EXIT_SUCCESS;
out:
	object_drop(gcfg);
out_config:
	config_cleanup();
	object_drop(state);
	return status;
}
