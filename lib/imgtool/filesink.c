/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filesink.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "filesink.h"
#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static void file_sink_destroy(object_t *base)
{
	file_sink_t *sink = (file_sink_t *)base;
	file_sink_bind_t *bind;

	while (sink->binds != NULL) {
		bind = sink->binds;
		sink->binds = bind->next;

		object_drop(bind->target);
		free(bind);
	}

	free(sink);
}

file_sink_t *file_sink_create(void)
{
	file_sink_t *sink = calloc(1, sizeof(*sink));
	object_t *obj = (object_t *)sink;

	if (sink == NULL) {
		perror("creating file sink object");
		return NULL;
	}

	obj->destroy = file_sink_destroy;
	obj->refcount = 1;
	return sink;
}

int file_sink_bind(file_sink_t *fs, const char *path, filesystem_t *target)
{
	file_sink_bind_t *new = calloc(1, sizeof(*new) + strlen(path) + 1);
	file_sink_bind_t *it;

	if (new == NULL) {
		perror("creating bind attachment");
		return -1;
	}

	strcpy(new->prefix, path);
	fstree_canonicalize_path(new->prefix);
	new->target = object_grab(target);

	for (it = fs->binds; it != NULL; it = it->next) {
		if (strcmp(it->prefix, new->prefix) == 0) {
			it->target = object_drop(it->target);
			it->target = object_grab(target);

			object_drop(new->target);
			free(new);
			return 0;
		}
	}

	new->next = fs->binds;
	fs->binds = new;
	return 0;
}

/*****************************************************************************/

static file_sink_bind_t *bind_point_from_path(file_sink_t *dst,
					      const char *path)
{
	file_sink_bind_t *it, *match = NULL;
	size_t len = strlen(path);

	for (it = dst->binds; it != NULL; it = it->next) {
		size_t plen = strlen(it->prefix);

		if (plen >= len || strncmp(it->prefix, path, plen) != 0)
			continue;

		if (plen > 0 && path[plen] != '/')
			continue;

		if (match == NULL || strlen(match->prefix) < plen)
			match = it;
	}

	return match;
}

static const char *retarget_path(file_sink_bind_t *it, const char *path)
{
	size_t len = strlen(path), plen = strlen(it->prefix);

	if (plen >= len || strncmp(it->prefix, path, plen) != 0)
		return path;

	if (plen == 0 || path[plen] != '/')
		return path;

	return path + plen + 1;
}

static int append_file_data(fstree_t *fs, tree_node_t *n, istream_t *strm)
{
	char buffer[256];
	int ret;

	for (;;) {
		ret = istream_read(strm, buffer, sizeof(buffer));
		if (ret == 0)
			break;
		if (ret < 0)
			return -1;

		if (fstree_file_append(fs, n, buffer, ret))
			return -1;
	}

	return 0;
}

static tree_node_t *create_node(fstree_t *fs, const file_source_record_t *rec,
				const char *name, const char *target)
{
	tree_node_t *n;

	switch (rec->type) {
	case FILE_SOURCE_DIR:
		n = fstree_add_directory(fs, name);
		break;
	case FILE_SOURCE_FILE:
		n = fstree_add_file(fs, name);
		break;
	case FILE_SOURCE_FIFO:
		n = fstree_add_fifo(fs, name);
		break;
	case FILE_SOURCE_SOCKET:
		n = fstree_add_socket(fs, name);
		break;
	case FILE_SOURCE_CHAR_DEV:
		n = fstree_add_character_device(fs, name, rec->devno);
		break;
	case FILE_SOURCE_BLOCK_DEV:
		n = fstree_add_block_device(fs, name, rec->devno);
		break;
	case FILE_SOURCE_SYMLINK:
		if (target == NULL || *target == '\0') {
			errno = EINVAL;
			return NULL;
		}
		n = fstree_add_symlink(fs, name, target);
		break;
	case FILE_SOURCE_HARD_LINK:
		if (target == NULL || *target == '\0') {
			errno = EINVAL;
			return NULL;
		}

		n = fstree_add_hard_link(fs, name, target);
		break;
	default:
		errno = EINVAL;
		return NULL;
	}

	if (n == NULL) {
		fprintf(stderr, "Adding %s: %s\n",
			rec->full_path, strerror(errno));
		return NULL;
	}

	n->uid = rec->uid;
	n->gid = rec->gid;
	n->mtime = rec->mtime;
	n->mtime = rec->ctime;

	if (rec->type == FILE_SOURCE_SYMLINK ||
	    rec->type == FILE_SOURCE_HARD_LINK) {
		n->permissions = 0777;
	} else {
		n->permissions = rec->permissions;
	}
	return n;
}

int file_sink_add_data(file_sink_t *dst, file_source_t *src)
{
	file_source_record_t *rec;
	const char *name, *target;
	file_sink_bind_t *match;
	istream_t *strm;
	tree_node_t *n;
	int ret;

	for (;;) {
		ret = src->get_next_record(src, &rec, &strm);
		if (ret > 0)
			break;
		if (ret < 0)
			return -1;

		match = bind_point_from_path(dst, rec->full_path);
		if (match == NULL)
			goto skip;

		name = retarget_path(match, rec->full_path);
		if (*name == '\0')
			goto skip;

		target = rec->link_target;
		if (rec->type == FILE_SOURCE_HARD_LINK && target != NULL)
			target = retarget_path(match, target);

		n = create_node(match->target->fstree, rec, name, target);
		if (n == NULL)
			goto fail;

		if (rec->type == FILE_SOURCE_FILE) {
			if (append_file_data(match->target->fstree, n, strm))
				goto fail;
		}

	skip:
		free(rec->full_path);
		free(rec->link_target);
		free(rec);
		if (strm != NULL)
			object_drop(strm);
	}

	return 0;
fail:
	free(rec->full_path);
	free(rec->link_target);
	free(rec);
	if (strm != NULL)
		object_drop(strm);
	return -1;
}
