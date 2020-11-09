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
	size_t remaining, resolved;
	tree_node_t *n, *target;

	/* resolve links */
	n = fs->nodes_by_type[TREE_NODE_HARD_LINK];

	while (n != NULL) {
		target = fstree_node_from_path(fs, NULL, n->data.link.target,
					       -1, false);

		if (target == NULL) {
			target = fstree_node_from_path(fs, n->parent,
						       n->data.link.target,
						       -1, false);
		}

		if (target == NULL) {
			fprintf(stderr, "resolving hardlink %s -> %s: %s\n",
				n->name, n->data.link.target, strerror(errno));
			return -1;
		}

		n->data.link.resolved = target;
		n = n->next_by_type;
	}

	/* recursively resolve links to links */
	for (;;) {
		n = fs->nodes_by_type[TREE_NODE_HARD_LINK];
		remaining = 0;
		resolved = 0;

		while (n != NULL) {
			target = n->data.link.resolved;

			if (target->type == TREE_NODE_HARD_LINK) {
				target = target->data.link.resolved;
				n->data.link.resolved = target;

				if (target->type == TREE_NODE_HARD_LINK) {
					remaining += 1;
				} else {
					resolved += 1;
				}
			}

			n = n->next_by_type;
		}

		if (remaining == 0)
			break;

		if (resolved == 0) {
			fputs("resolving hardlinks: cycle detected!\n", stderr);
			return -1;
		}
	}

	/* compute link count for all nodes */
	n = fs->nodes_by_type[TREE_NODE_HARD_LINK];

	while (n != NULL) {
		target = n->data.link.resolved;
		target->link_count += 1;
		n = n->next_by_type;
	}

	return 0;
}
