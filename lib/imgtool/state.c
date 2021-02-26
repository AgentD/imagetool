/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * state.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fsdeptracker.h"
#include "filesystem.h"
#include "libimgtool.h"
#include "filesource.h"
#include "filesink.h"
#include "plugin.h"
#include "volume.h"
#include "fstree.h"
#include "gcfg.h"

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

static object_t *cb_create_fs(const gcfg_keyword_t *kwd, gcfg_file_t *file,
			      object_t *parent, const char *string)
{
	volume_t *vol = (volume_t *)parent;
	imgtool_state_t *state = kwd->state;
	plugin_t *plugin = kwd->plugin;
	filesystem_t *fs;

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

static int mount_group_add_source(mount_group_t *mg, file_source_t *source)
{
	file_source_stackable_t *aggregate;

	if (mg->source == NULL) {
		mg->source = object_grab(source);
		return 0;
	}

	if (mg->have_aggregate) {
		aggregate = (file_source_stackable_t *)mg->source;
	} else {
		aggregate = (file_source_stackable_t *)
			file_source_aggregate_create();
		if (aggregate == NULL)
			return -1;

		if (aggregate->add_nested(aggregate, mg->source)) {
			object_drop(aggregate);
			return -1;
		}

		mg->source = (file_source_t *)aggregate;
		mg->have_aggregate = true;
	}

	return aggregate->add_nested(aggregate, source);
}

static object_t *cb_create_stackable_source(const gcfg_keyword_t *kwd,
					    gcfg_file_t *file, object_t *object)
{
	mount_group_t *mg = (mount_group_t *)object;
	plugin_t *plugin = kwd->plugin;
	file_source_stackable_t *src;

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
	plugin_t *plugin = kwd->plugin;
	file_source_t *src;

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
	plugin_t *plugin = kwd->plugin;
	file_source_stackable_t *src;

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
	plugin_t *plugin = kwd->plugin;
	file_source_t *src;

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
	plugin_t *plugin = kwd->plugin;
	volume_t *volume;

	volume = plugin->create.volume(plugin, (imgtool_state_t *)parent);
	if (volume == NULL) {
		file->report_error(file, "error creating %s volume",
				   kwd->name);
		return NULL;
	}

	return (object_t *)volume;
}

static object_t *cb_create_volumefile(const gcfg_keyword_t *kwd,
				      gcfg_file_t *file, object_t *parent,
				      const char *string)
{
	filesystem_t *fs = (filesystem_t *)parent;
	imgtool_state_t *state = kwd->state;
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
	imgtool_state_t *state = kwd->state;
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

static void mountgroup_destroy(object_t *obj)
{
	mount_group_t *mg = (mount_group_t *)obj;

	if (mg->next != NULL)
		mg->next = object_drop(mg->next);

	object_drop(mg->sink);
	object_drop(mg->source);
	free(mg);
}

static object_t *cb_create_mount_group(const gcfg_keyword_t *kwd,
				       gcfg_file_t *file, object_t *object)
{
	imgtool_state_t *state = (imgtool_state_t *)object;
	mount_group_t *mg = calloc(1, sizeof(*mg));
	(void)kwd;

	if (mg == NULL) {
		file->report_error(file, "creating mount group: %s",
				   strerror(errno));
		return NULL;
	}

	mg->sink = file_sink_create();
	if (mg->sink == NULL) {
		free(mg);
		return NULL;
	}

	((object_t *)mg)->refcount = 1;
	((object_t *)mg)->destroy = mountgroup_destroy;

	if (state->mg_list == NULL) {
		state->mg_list = object_grab(mg);
		state->mg_list_last = object_grab(mg);
	} else {
		state->mg_list_last->next = object_grab(mg);
		state->mg_list_last = object_drop(state->mg_list_last);
		state->mg_list_last = object_grab(mg);
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

/*****************************************************************************/

static void state_destroy(object_t *obj)
{
	imgtool_state_t *state = (imgtool_state_t *)obj;
	gcfg_keyword_t *it;

	if (state->mg_list_last != NULL)
		state->mg_list_last = object_drop(state->mg_list_last);

	if (state->mg_list != NULL)
		state->mg_list = object_drop(state->mg_list);

	while (state->cfg_global != NULL) {
		it = state->cfg_global;
		state->cfg_global = it->next;
		free(it);
	}

	while (state->cfg_filesystems != NULL) {
		it = state->cfg_filesystems;
		state->cfg_filesystems = it->next;
		free(it);
	}

	while (state->cfg_fs_common != NULL) {
		it = state->cfg_fs_common;
		state->cfg_fs_common = it->next;
		free(it);
	}

	while (state->cfg_sources != NULL) {
		it = state->cfg_sources;
		state->cfg_sources = it->next;
		free(it);
	}

	while (state->cfg_sources_rec != NULL) {
		it = state->cfg_sources_rec;
		state->cfg_sources_rec = it->next;
		free(it);
	}

	object_drop(state->out_file);
	object_drop(state->dep_tracker);
	object_drop(state->registry);
	free(state);
}

imgtool_state_t *imgtool_state_create(const char *out_path)
{
	imgtool_state_t *state = calloc(1, sizeof(*state));
	object_t *obj = (object_t *)state;
	int fd;

	if (state == NULL) {
		perror("creating state object");
		return NULL;
	}

	state->registry = plugin_registry_create();
	if (state->registry == NULL)
		goto fail_free;

	state->dep_tracker = fs_dep_tracker_create();
	if (state->dep_tracker == NULL)
		goto fail_registry;

	fd = open(out_path, O_RDWR | O_CREAT | O_EXCL, 0644);
	if (fd < 0) {
		perror(out_path);
		goto fail_tracker;
	}

	state->out_file = volume_from_fd(out_path, fd, 0xFFFFFFFFFFFFFFFFUL);
	if (state->out_file == NULL) {
		close(fd);
		goto fail_tracker;
	}

	if (state->dep_tracker->add_volume(state->dep_tracker,
					   state->out_file, NULL)) {
		goto fail_file;
	}

	obj->refcount = 1;
	obj->destroy = state_destroy;
	return state;
fail_file:
	object_drop(state->out_file);
fail_tracker:
	object_drop(state->dep_tracker);
fail_registry:
	object_drop(state->registry);
fail_free:
	free(state);
	return NULL;
}

int imgtool_state_init_config(imgtool_state_t *state)
{
	gcfg_keyword_t *kwd_it, *copy, *last;
	plugin_t *it;

	/* filesystems */
	state->cfg_fs_common = calloc(1, sizeof(state->cfg_fs_common[0]));
	if (state->cfg_fs_common == NULL)
		goto fail;

	it = state->registry->plugins[PLUGIN_TYPE_FILESYSTEM];

	for (; it != NULL; it = it->next) {
		kwd_it = calloc(1, sizeof(*kwd_it));
		if (kwd_it == NULL)
			goto fail;

		kwd_it->arg = GCFG_ARG_STRING;
		kwd_it->name = it->name;
		kwd_it->children = it->cfg_sub_nodes;
		kwd_it->handle_listing = it->cfg_line_callback;
		kwd_it->handle.cb_string = cb_create_fs;
		kwd_it->state = state;
		kwd_it->plugin = it;

		kwd_it->next = state->cfg_filesystems;
		state->cfg_filesystems = kwd_it;

		if (kwd_it->children == NULL) {
			kwd_it->children = state->cfg_fs_common;
		} else {
			kwd_it = kwd_it->children;
			while (kwd_it->next != NULL)
				kwd_it = kwd_it->next;

			kwd_it->next = state->cfg_fs_common;
		}
	}

	state->cfg_fs_common[0].arg = GCFG_ARG_STRING;
	state->cfg_fs_common[0].name = "volumefile";
	state->cfg_fs_common[0].handle.cb_string = cb_create_volumefile;
	state->cfg_fs_common[0].state = state;
	state->cfg_fs_common[0].next = NULL;
	state->cfg_fs_common[0].children = state->cfg_filesystems;

	/* file sources */
	state->cfg_sources_rec = NULL;
	last = state->cfg_sources_rec;

	it = state->registry->plugins[PLUGIN_TYPE_FILE_SOURCE];

	while (it != NULL) {
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

		kwd_it->state = state;
		kwd_it->plugin = it;
		kwd_it->name = it->name;
		kwd_it->children = it->cfg_sub_nodes;
		kwd_it->handle_listing = it->cfg_line_callback;
		kwd_it->next = state->cfg_sources;
		state->cfg_sources = kwd_it;

		/* copy in the nested sources list */
		copy = calloc(1, sizeof(*copy));
		if (copy == NULL)
			goto fail;

		memcpy(copy, kwd_it, sizeof(*copy));
		copy->next = NULL;

		if (last == NULL) {
			state->cfg_sources_rec = copy;
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
				kwd_it->children = state->cfg_sources_rec;
				copy->children = state->cfg_sources_rec;
			} else {
				kwd_it = kwd_it->children;
				while (kwd_it->next != NULL)
					kwd_it = kwd_it->next;

				kwd_it->next = state->cfg_sources_rec;
			}
		}

		/* advance */
		if (it->next == NULL) {
			if (it->type == PLUGIN_TYPE_FILE_SOURCE_STACKABLE)
				break;
			it = state->registry->
				plugins[PLUGIN_TYPE_FILE_SOURCE_STACKABLE];
		} else {
			it = it->next;
		}
	}

	/* root level entries */
	it = state->registry->plugins[PLUGIN_TYPE_VOLUME];

	for (; it != NULL; it = it->next) {
		kwd_it = calloc(1, sizeof(*kwd_it));
		if (kwd_it == NULL)
			goto fail;

		kwd_it->arg = GCFG_ARG_NONE;
		kwd_it->name = it->name;
		kwd_it->state = state;
		kwd_it->plugin = it;
		kwd_it->children = it->cfg_sub_nodes;
		kwd_it->handle_listing = it->cfg_line_callback;
		kwd_it->handle.cb_none = cb_create_volume;
		kwd_it->next = state->cfg_global;
		state->cfg_global = kwd_it;

		if (kwd_it->children == NULL) {
			kwd_it->children = state->cfg_filesystems;
		} else {
			kwd_it = kwd_it->children;
			while (kwd_it->next != NULL)
				kwd_it = kwd_it->next;

			kwd_it->next = state->cfg_filesystems;
		}
	}

	kwd_it = calloc(1, sizeof(*kwd_it));
	if (kwd_it == NULL)
		goto fail;

	kwd_it->arg = GCFG_ARG_NONE;
	kwd_it->name = "mountgroup";
	kwd_it->children = &cfg_mount_group_bind;
	kwd_it->handle.cb_none = cb_create_mount_group;
	kwd_it->state = state;
	kwd_it->next = state->cfg_global;
	state->cfg_global = kwd_it;

	if (kwd_it->children == NULL) {
		kwd_it->children = state->cfg_sources;
	} else {
		kwd_it = kwd_it->children;

		while (kwd_it->next != NULL) {
			kwd_it->state = state;
			kwd_it = kwd_it->next;
		}

		kwd_it->state = state;
		kwd_it->next = state->cfg_sources;
	}

	return 0;
fail:
	perror("initializing configuration parser");
	return -1;
}

int imgtool_state_process(imgtool_state_t *state)
{
	mount_group_t *mg;

	for (mg = state->mg_list; mg != NULL; mg = mg->next) {
		if (file_sink_add_data(mg->sink, mg->source))
			return -1;
	}

	if (state->dep_tracker->commit(state->dep_tracker))
		return -1;

	return 0;
}
