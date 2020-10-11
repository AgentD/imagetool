/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

static tree_node_t *mknode_at(fstree_t *fs, const char *path, int type,
			      const char *extra)
{
	size_t prefix_len, suffix_len, size;
	tree_node_t *parent, *n;
	const char *suffix;
	char *target;

	suffix = strrchr(path, '/');

	if (suffix == NULL) {
		suffix = path;
		prefix_len = 0;
	} else {
		prefix_len = suffix - path;
		suffix += 1;
	}

	suffix_len = strlen(suffix);

	parent = fstree_node_from_path(fs, path, prefix_len, true);
	if (parent == NULL)
		return NULL;

	if (parent->type != TREE_NODE_DIR) {
		errno = ENOTDIR;
		return NULL;
	}

	for (n = parent->data.dir.children; n != NULL; n = n->next) {
		if (strncmp(n->name, suffix, suffix_len) == 0)
			break;
	}

	if (n != NULL) {
		if (n->type != TREE_NODE_DIR || type != TREE_NODE_DIR)
			goto fail_exist;

		if (!n->data.dir.created_implicitly)
			goto fail_exist;

		n->data.dir.created_implicitly = false;
	} else {
		size = sizeof(*n) + suffix_len + 1;
		if (extra != NULL)
			size += strlen(extra) + 1;

		n = calloc(1, size);
		if (n == NULL)
			return NULL;

		n->ctime = fs->default_ctime;
		n->mtime = fs->default_mtime;
		n->uid = fs->default_uid;
		n->gid = fs->default_gid;
		n->name = (const char *)n->payload;
		n->type = type;
		n->permissions = fs->default_permissions;

		n->parent = parent;
		n->next = parent->data.dir.children;
		parent->data.dir.children = n;

		memcpy((char *)n->payload, suffix, suffix_len);

		if (n->type == TREE_NODE_SYMLINK ||
		    n->type == TREE_NODE_HARD_LINK) {
			target = (char *)n->payload + suffix_len + 1;

			n->data.link.target = (const char *)target;
			strcpy(target, extra);

			if (n->type == TREE_NODE_HARD_LINK)
				fstree_canonicalize_path(target);
		}
	}

	return n;
fail_exist:
	errno = EEXIST;
	return NULL;
}

tree_node_t *fstree_add_direcory(fstree_t *fs, const char *path)
{
	return mknode_at(fs, path, TREE_NODE_DIR, NULL);
}

tree_node_t *fstree_add_file(fstree_t *fs, const char *path)
{
	tree_node_t *n = mknode_at(fs, path, TREE_NODE_FILE, NULL);

	if (n != NULL)
		n->permissions &= 0666;

	return n;
}

tree_node_t *fstree_add_fifo(fstree_t *fs, const char *path)
{
	tree_node_t *n = mknode_at(fs, path, TREE_NODE_FIFO, NULL);

	if (n != NULL)
		n->permissions &= 0666;

	return n;
}

tree_node_t *fstree_add_socket(fstree_t *fs, const char *path)
{
	tree_node_t *n = mknode_at(fs, path, TREE_NODE_SOCKET, NULL);

	if (n != NULL)
		n->permissions &= 0666;

	return n;
}

tree_node_t *fstree_add_block_device(fstree_t *fs, const char *path,
				     uint32_t devno)
{
	tree_node_t *n = mknode_at(fs, path, TREE_NODE_BLOCK_DEV, NULL);

	if (n != NULL) {
		n->permissions &= 0666;
		n->data.device_number = devno;
	}

	return n;
}

tree_node_t *fstree_add_character_device(fstree_t *fs, const char *path,
					 uint32_t devno)
{
	tree_node_t *n = mknode_at(fs, path, TREE_NODE_CHAR_DEV, NULL);

	if (n != NULL) {
		n->permissions &= 0666;
		n->data.device_number = devno;
	}

	return n;
}

tree_node_t *fstree_add_symlink(fstree_t *fs, const char *path,
				const char *target)
{
	tree_node_t *n = mknode_at(fs, path, TREE_NODE_SYMLINK, target);

	if (n != NULL)
		n->permissions = 0777;

	return n;
}

tree_node_t *fstree_add_hard_link(fstree_t *fs, const char *path,
				  const char *target)
{
	tree_node_t *n = mknode_at(fs, path, TREE_NODE_HARD_LINK, target);

	if (n != NULL)
		n->permissions = 0777;

	return n;
}
