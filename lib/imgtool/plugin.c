/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * plugin.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "plugin.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void destroy(object_t *base)
{
	plugin_registry_t *reg = (plugin_registry_t *)base;

	free(reg);
}

plugin_registry_t *plugin_registry_create(void)
{
	plugin_registry_t *reg = calloc(1, sizeof(*reg));
	object_t *obj = (object_t *)reg;

	if (reg == NULL) {
		perror("creating plugin registry");
		return NULL;
	}

	obj->destroy = destroy;
	obj->refcount = 1;
	return reg;
}

int plugin_registry_add_plugin(plugin_registry_t *registry, plugin_t *plugin)
{
	int type;

	type = plugin->type;
	if (type < 0 || type >= PLUGIN_TYPE_COUNT) {
		fprintf(stderr, "Tried to register '%s' plugin with "
			"unknown type '%d'\n", plugin->name, type);
		return -1;
	}

	if (plugin_registry_find_plugin(registry,
					plugin->type, plugin->name) != NULL) {
		fprintf(stderr, "Tried to register more than "
			"one '%s' plugin.\n", plugin->name);
		return -1;
	}

	plugin->next = registry->plugins[type];
	registry->plugins[type] = plugin;
	return 0;
}

plugin_t *plugin_registry_find_plugin(plugin_registry_t *registry, int type,
				      const char *name)
{
	plugin_t *it;

	if (type < 0 || type >= PLUGIN_TYPE_COUNT)
		return NULL;

	for (it = registry->plugins[type]; it != NULL; it = it->next) {
		if (strcmp(it->name, name) == 0)
			break;
	}

	return it;
}
