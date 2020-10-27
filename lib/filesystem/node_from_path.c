/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * node_from_path.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

tree_node_t *fstree_node_from_path(fstree_t *fs, const char *path,
				   size_t len, bool create_implicit)
{
	tree_node_t *it, *n = fs->root;
	size_t comp_len;

	while (len > 0) {
		/* skip preceeding slashes */
		while (len > 0) {
			if (*path == '/')
				break;

			if (*path == '\0') {
				len = 0;
				break;
			}

			++path;
			--len;
		}

		/* isolate path component */
		for (comp_len = 0; comp_len < len; ++comp_len) {
			if (path[comp_len] == '/')
				break;

			if (path[comp_len] == '\0') {
				len = comp_len;
				break;
			}
		}

		if (comp_len == 0)
			break;

		if (n->type != TREE_NODE_DIR) {
			errno = ENOTDIR;
			return NULL;
		}

		/* special case handling */
		if (comp_len == 1 && strncmp(path, ".", comp_len) == 0) {
			path += comp_len;
			len -= comp_len;
			continue;
		}

		if (comp_len == 2 && strncmp(path, "..", comp_len) == 0) {
			if (n->parent != NULL)
				n = n->parent;

			path += comp_len;
			len -= comp_len;
			continue;
		}

		/* find child by name */
		for (it = n->data.dir.children; it != NULL; it = it->next) {
			if (strncmp(it->name, path, comp_len) != 0)
				continue;

			if (strlen(it->name) == comp_len)
				break;
		}

		if (it == NULL) {
			if (!create_implicit) {
				errno = ENOENT;
				return NULL;
			}

			it = calloc(1, sizeof(*it) + comp_len + 1);
			if (it == NULL)
				return NULL;

			it->ctime = fs->default_ctime;
			it->mtime = fs->default_mtime;
			it->uid = fs->default_uid;
			it->gid = fs->default_gid;
			it->next = n->data.dir.children;
			n->data.dir.children = it;
			it->parent = n;
			it->name = (const char *)n->payload;
			it->type = TREE_NODE_DIR;
			it->permissions = fs->default_permissions;
			it->data.dir.created_implicitly = true;

			memcpy(it->payload, path, comp_len);
			((char *)it->payload)[comp_len] = '\0';
		}

		path += comp_len;
		len -= comp_len;
		n = it;
	}

	return n;
}