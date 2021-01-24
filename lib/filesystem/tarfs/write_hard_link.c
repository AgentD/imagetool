/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_hard_link.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tarfs.h"

int tarfs_write_hard_link(ostream_t *out, unsigned int *counter,
			  tree_node_t *n)
{
	struct stat sb;
	char *path;

	if (n->type != TREE_NODE_HARD_LINK)
		return 0;

	path = fstree_get_path(n);
	if (path == NULL) {
		perror("generating tar hard link header");
		return -1;
	}

	fstree_canonicalize_path(path);

	memset(&sb, 0, sizeof(sb));
	sb.st_uid = n->uid;
	sb.st_gid = n->gid;
	sb.st_nlink = n->link_count;
	sb.st_mode = n->permissions;
	sb.st_size = n->data.file.size;

	/* TODO: 64 vs 32 bit time_t??? */
	sb.st_mtime = n->mtime;
	sb.st_ctime = n->ctime;

	if (write_hard_link(out, &sb, path, n->data.link.target, *counter)) {
		fprintf(stderr, "%s: error writing file metadata to "
			"tar filesystem", path);
		free(path);
		return -1;
	}

	(*counter) += 1;
	free(path);
	return 0;
}
