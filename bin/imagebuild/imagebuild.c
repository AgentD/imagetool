/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * imagebuild.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

static imgtool_state_t *state = NULL;

/*****************************************************************************/

static object_t *cb_create_fatfs(gcfg_file_t *file, object_t *parent,
				 const char *string)
{
	volume_t *vol = (volume_t *)parent;
	filesystem_t *fs = filesystem_fatfs_create(vol);

	if (fs == NULL) {
		file->report_error(file, "error creating FAT filesystem '%s'",
				   string);
		return NULL;
	}

	if (fs_dep_tracker_add_fs(state->dep_tracker, fs, vol, string)) {
		file->report_error(file, "error registering "
				   "FAT filesystem '%s'", string);
		object_drop(fs);
		return NULL;
	}

	return (object_t *)fs;
}

static object_t *cb_create_tarfs(gcfg_file_t *file, object_t *parent,
				 const char *string)
{
	volume_t *vol = (volume_t *)parent;
	filesystem_t *fs = filesystem_tar_create(vol);

	if (fs == NULL) {
		file->report_error(file, "error creating tar filesystem '%s'",
				   string);
		return NULL;
	}

	if (fs_dep_tracker_add_fs(state->dep_tracker, fs, vol, string)) {
		file->report_error(file, "error registering "
				   "tar filesystem '%s'", string);
		object_drop(fs);
		return NULL;
	}

	return (object_t *)fs;
}

static object_t *cb_create_cpiofs(gcfg_file_t *file, object_t *parent,
				  const char *string)
{
	volume_t *vol = (volume_t *)parent;
	filesystem_t *fs = filesystem_cpio_create(vol);

	if (fs == NULL) {
		file->report_error(file, "error creating cpio filesystem '%s'",
				   string);
		return NULL;
	}

	if (fs_dep_tracker_add_fs(state->dep_tracker, fs, vol, string)) {
		file->report_error(file, "error reginstering "
				   "cpio filesystem '%s'", string);
		object_drop(fs);
		return NULL;
	}

	return (object_t *)fs;
}

static object_t *cb_create_volumefile(gcfg_file_t *file, object_t *parent,
				      const char *string)
{
	filesystem_t *fs = (filesystem_t *)parent;
	volume_t *volume;
	tree_node_t *n;

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

static const gcfg_keyword_t cfg_filesystems[4];

static const gcfg_keyword_t cfg_filesystems_common[] = {
	{
		.arg = GCFG_ARG_STRING,
		.name = "volumefile",
		.handle = {
			.cb_string = cb_create_volumefile,
		},
		.children = cfg_filesystems,
	}, {
		.name = NULL,
	}
};

static const gcfg_keyword_t cfg_filesystems[4] = {
	{
		.arg = GCFG_ARG_STRING,
		.name = "tar",
		.children = cfg_filesystems_common,
		.handle = {
			.cb_string = cb_create_tarfs,
		},
	}, {
		.arg = GCFG_ARG_STRING,
		.name = "cpio",
		.children = cfg_filesystems_common,
		.handle = {
			.cb_string = cb_create_cpiofs,
		},
	}, {
		.arg = GCFG_ARG_STRING,
		.name = "fat",
		.children = cfg_filesystems_common,
		.handle = {
			.cb_string = cb_create_fatfs,
		},
	}, {
		.name = NULL,
	}
};

/*****************************************************************************/

static object_t *cb_create_listing(gcfg_file_t *file, object_t *object,
				   const char *string)
{
	mount_group_t *mg = (mount_group_t *)object;
	file_source_listing_t *list;

	list = file_source_listing_create(string);
	if (list == NULL) {
		file->report_error(file, "error creating source listing");
		return NULL;
	}

	if (mount_group_add_source(mg, (file_source_t *)list)) {
		file->report_error(file, "error adding source to mount group");
		object_drop(list);
		return NULL;
	}

	return (object_t *)list;
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

static object_t *cb_mp_add_bind(gcfg_file_t *file, object_t *object,
				const char *line)
{
	mount_group_t *mg = (mount_group_t *)object;
	filesystem_t *fs;
	const char *ptr;
	size_t len;
	char *temp;

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

static const gcfg_keyword_t cfg_data_source[] = {
	{
		.arg = GCFG_ARG_STRING,
		.name = "listing",
		.handle_listing = cb_add_listing_line,
		.handle = {
			.cb_string = cb_create_listing,
		},
	},
};

/*****************************************************************************/

static object_t *cb_raw_set_minsize(gcfg_file_t *file, object_t *obj,
				    uint64_t size)
{
	volume_t *vol = (volume_t *)obj;
	(void)file;

	vol->min_block_count = size / vol->blocksize;
	if (size % vol->blocksize)
		vol->min_block_count += 1;

	return object_grab(vol);
}

static object_t *cb_raw_set_maxsize(gcfg_file_t *file, object_t *obj,
				    uint64_t size)
{
	volume_t *vol = (volume_t *)obj;
	(void)file;

	vol->max_block_count = size / vol->blocksize;
	return object_grab(vol);
}

static object_t *cb_create_mount_group(gcfg_file_t *file, object_t *object)
{
	mount_group_t *mg;

	mg = imgtool_state_add_mount_group((imgtool_state_t *)object);
	if (mg == NULL) {
		file->report_error(file, "error creating mount group");
		return NULL;
	}

	return (object_t *)mg;
}

static object_t *cb_create_raw_volume(gcfg_file_t *file, object_t *parent)
{
	(void)file;
	return object_grab(((imgtool_state_t *)parent)->out_file);
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

static gcfg_keyword_t *cfg_raw_dyn_child = NULL;
static gcfg_keyword_t *cfg_mg_dyn_child = NULL;
static gcfg_keyword_t *cfg_global = NULL;

static int init_config(void)
{
	gcfg_keyword_t *list;
	size_t i, j, count;

	cfg_global = calloc(3, sizeof(cfg_global[0]));
	if (cfg_global == NULL) {
		perror("initializing configuration parser");
		return -1;
	}

	/* raw children */
	count = sizeof(cfg_raw_volume) / sizeof(cfg_raw_volume[0]);
	count += sizeof(cfg_filesystems) / sizeof(cfg_filesystems[0]);

	list = calloc(count + 1, sizeof(list[0]));
	if (list == NULL) {
		perror("initializing raw volume parser");
		free(cfg_global);
		return -1;
	}

	j = 0;

	count = sizeof(cfg_raw_volume) / sizeof(cfg_raw_volume[0]);
	for (i = 0; i < count; ++i)
		list[j++] = cfg_raw_volume[i];

	count = sizeof(cfg_filesystems) / sizeof(cfg_filesystems[0]);
	for (i = 0; i < count; ++i)
		list[j++] = cfg_filesystems[i];

	list[j].name = NULL;
	cfg_raw_dyn_child = list;

	/* mountgroup children */
	count = sizeof(cfg_data_source) / sizeof(cfg_data_source[0]);
	count += sizeof(cfg_mount_group) / sizeof(cfg_mount_group[0]);

	list = calloc(count + 1, sizeof(list[0]));
	if (list == NULL) {
		perror("initializing mountgroup parser");
		goto fail;
	}

	j = 0;

	count = sizeof(cfg_mount_group) / sizeof(cfg_mount_group[0]);
	for (i = 0; i < count; ++i)
		list[j++] = cfg_mount_group[i];

	count = sizeof(cfg_data_source) / sizeof(cfg_data_source[0]);
	for (i = 0; i < count; ++i)
		list[j++] = cfg_data_source[i];

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
	return -1;
}

static void cleanup_config(void)
{
	free(cfg_raw_dyn_child);
	free(cfg_mg_dyn_child);
	free(cfg_global);
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
