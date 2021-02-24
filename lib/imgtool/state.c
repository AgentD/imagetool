/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * state.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "libimgtool.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

static void mountgroup_destroy(object_t *obj)
{
	mount_group_t *mg = (mount_group_t *)obj;

	if (mg->next != NULL)
		mg->next = object_drop(mg->next);

	object_drop(mg->sink);
	object_drop(mg->source);
	free(mg);
}

static void state_destroy(object_t *obj)
{
	imgtool_state_t *state = (imgtool_state_t *)obj;

	if (state->mg_list_last != NULL)
		state->mg_list_last = object_drop(state->mg_list_last);

	if (state->mg_list != NULL)
		state->mg_list = object_drop(state->mg_list);

	object_drop(state->out_file);
	object_drop(state->dep_tracker);
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

	state->dep_tracker = fs_dep_tracker_create();
	if (state->dep_tracker == NULL)
		goto fail_free;

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

	if (fs_dep_tracker_add_volume(state->dep_tracker,
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
fail_free:
	free(state);
	return NULL;
}

mount_group_t *imgtool_state_add_mount_group(imgtool_state_t *state)
{
	mount_group_t *mg = calloc(1, sizeof(*mg));

	if (mg == NULL) {
		perror("creating mount group");
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

	return mg;
}

int mount_group_add_source(mount_group_t *mg, file_source_t *source)
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

int imgtool_state_process(imgtool_state_t *state)
{
	mount_group_t *mg;

	for (mg = state->mg_list; mg != NULL; mg = mg->next) {
		if (file_sink_add_data(mg->sink, mg->source))
			return -1;
	}

	if (fs_dep_tracker_commit(state->dep_tracker))
		return -1;

	return 0;
}
