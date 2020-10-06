/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <string.h>
#include <stdlib.h>

static size_t path_length(tree_node_t *node)
{
	if (node->parent == NULL)
		return 0;

	return path_length(node->parent) + 1 + strlen(node->name);
}

static char *path_print(char *str, tree_node_t *node)
{
	if (node->parent == NULL)
		return str;

	str = path_print(str, node->parent);
	*(str++) = '/';
	strcpy(str, node->name);
	return str + strlen(str);
}

char *fstree_get_path(tree_node_t *node)
{
	size_t len;
	char *str;

	if (node->parent == NULL)
		return strdup("/");

	len = path_length(node);

	str = malloc(len + 1);
	if (str == NULL)
		return NULL;

	path_print(str, node);
	return str;
}
