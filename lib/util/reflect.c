/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * reflect.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "predef.h"

size_t object_get_property_count(const void *obj)
{
	const meta_object_t *meta;
	size_t sum = 0;

	for (meta = object_reflect(obj); meta != NULL; meta = meta->parent) {
		sum += meta->get_property_count(meta);
	}

	return sum;
}

static const meta_object_t *meta_from_property_index(const void *obj,
						     size_t *idx)
{
	const meta_object_t *meta = object_reflect(obj);

	while (meta != NULL) {
		size_t count = meta->get_property_count(meta);
		if (*idx < count)
			break;

		*idx -= count;
		meta = meta->parent;
	}

	return meta;
}

int object_get_property_desc(const void *obj, size_t idx, PROPERTY_TYPE *type,
			     const char **name)
{
	const meta_object_t *meta = meta_from_property_index(obj, &idx);

	if (meta == NULL)
		return -1;

	return meta->get_property_desc(meta, idx, type, name);
}

int object_get_property(const void *obj, size_t idx, property_value_t *out)
{
	const meta_object_t *meta = meta_from_property_index(obj, &idx);

	if (meta == NULL)
		return -1;

	return meta->get_property(meta, idx, obj, out);
}

int object_set_property(void *obj, size_t idx, const property_value_t *value)
{
	const meta_object_t *meta = meta_from_property_index(obj, &idx);

	if (meta == NULL)
		return -1;

	return meta->set_property(meta, idx, obj, value);
}
