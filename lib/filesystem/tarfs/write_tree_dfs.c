/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_tree_dfs.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tarfs.h"

int tarfs_write_tree_dfs(ostream_t *out, unsigned int *counter,
			 tree_node_t *n)
{
	const char *target = NULL;
	struct stat sb;
	char *path;
	int ret;

	/* special cases */
	if (n->type == TREE_NODE_FILE || n->type == TREE_NODE_HARD_LINK)
		return 0;

	if (n->parent == NULL)
		goto out_children;

	/* gather information we need */
	path = fstree_get_path(n);
	if (path == NULL) {
		perror("generating tar header");
		return -1;
	}

	fstree_canonicalize_path(path);

	memset(&sb, 0, sizeof(sb));
	sb.st_uid = n->uid;
	sb.st_gid = n->gid;
	sb.st_nlink = n->link_count;
	sb.st_mode = n->permissions;

	/* TODO: 64 vs 32 bit time_t??? */
	sb.st_mtime = n->mtime;
	sb.st_ctime = n->ctime;

	switch (n->type) {
	case TREE_NODE_DIR:
		sb.st_mode |= S_IFDIR;
		break;
	case TREE_NODE_FIFO:
		sb.st_mode |= S_IFIFO;
		break;
	case TREE_NODE_SOCKET:
		fprintf(stderr, "tar filesystem: %s: "
			"socket files are not supported.\n",
			path);
		free(path);
		return -1;
	case TREE_NODE_CHAR_DEV:
		sb.st_mode |= S_IFCHR;
		sb.st_rdev = n->data.device_number;
		break;
	case TREE_NODE_BLOCK_DEV:
		sb.st_mode |= S_IFBLK;
		sb.st_rdev = n->data.device_number;
		break;
	case TREE_NODE_SYMLINK:
		sb.st_mode |= S_IFLNK;
		sb.st_size = strlen(n->data.link.target);
		target = n->data.link.target;
		break;
	default:
		free(path);
		return 0;
	}

	/* write the header */
	if (n->type == TREE_NODE_HARD_LINK) {
		ret = write_hard_link(out, &sb, path, target, *counter);
	} else {
		ret = write_tar_header(out, &sb, path, target,
				       NULL, *counter);
	}

	if (ret != 0) {
		fprintf(stderr, "%s: error writing metadata to "
			"tar filesystem", path);
		free(path);
		return -1;
	}

	(*counter) += 1;
	free(path);

	/* recursively write out children */
out_children:
	if (n->type == TREE_NODE_DIR) {
		for (n = n->data.dir.children; n != NULL; n = n->next) {
			if (tarfs_write_tree_dfs(out, counter, n))
				return -1;
		}
	}
	return 0;
}
