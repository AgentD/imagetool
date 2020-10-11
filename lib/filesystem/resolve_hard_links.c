/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * resolve_hard_links.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>

static int resolve_dfs(fstree_t *fs, tree_node_t *n)
{
	tree_node_t *it;

	if (n->type == TREE_NODE_DIR) {
		for (it = n->data.dir.children; it != NULL; it = it->next) {
			if (resolve_dfs(fs, it))
				return -1;
		}
	} else if (n->type == TREE_NODE_HARD_LINK) {
		it = fstree_node_from_path(fs, n->data.link.target,
					   strlen(n->data.link.target), false);

		if (it == NULL) {
			fprintf(stderr, "resolving hardlink %s -> %s: %s\n",
				n->name, n->data.link.target, strerror(errno));
			return -1;
		}

		n->data.link.resolved = it;
		it->link_count += 1;
	}

	return 0;
}

int fstree_resolve_hard_links(fstree_t *fs)
{
	return resolve_dfs(fs, fs->root);
}
