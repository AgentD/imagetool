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

int fstree_resolve_hard_links(fstree_t *fs)
{
	tree_node_t *n = fs->nodes_by_type[TREE_NODE_HARD_LINK], *target;

	while (n != NULL) {
		target = fstree_node_from_path(fs, n->data.link.target,
					       strlen(n->data.link.target),
					       false);

		if (target == NULL) {
			fprintf(stderr, "resolving hardlink %s -> %s: %s\n",
				n->name, n->data.link.target, strerror(errno));
			return -1;
		}

		n->data.link.resolved = target;
		target->link_count += 1;

		n = n->next_by_type;
	}

	return 0;
}
