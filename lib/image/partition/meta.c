/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * meta.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "volume.h"

enum {
	PART_GROW = 0,
	PART_FILL,
};

static const property_desc_t common_part_prop[] = {
	[PART_GROW] = {
		.type = PROPERTY_TYPE_BOOL,
		.name = "grow",
	},
	[PART_FILL] = {
		.type = PROPERTY_TYPE_BOOL,
		.name = "fill",
	},
};

static size_t part_get_property_count(const meta_object_t *meta)
{
	(void)meta;
	return sizeof(common_part_prop) / sizeof(common_part_prop[0]);
}

static int part_get_property_desc(const meta_object_t *meta, size_t i,
				  property_desc_t *desc)
{
	(void)meta;

	if (i >= (sizeof(common_part_prop) / sizeof(common_part_prop[0])))
		return -1;

	*desc = common_part_prop[i];
	return 0;
}

static int part_set_property(const meta_object_t *meta, size_t i,
			     object_t *obj, const property_value_t *value)
{
	partition_t *part = (partition_t *)obj;
	uint64_t flags = part->get_flags(part);
	(void)meta;

	switch (i) {
	case PART_GROW:
		if (value->value.boolean) {
			flags |= COMMON_PARTITION_FLAG_GROW;
		} else {
			flags &= ~COMMON_PARTITION_FLAG_GROW;
		}
		return part->set_flags(part, flags);
	case PART_FILL:
		if (value->value.boolean) {
			flags |= COMMON_PARTITION_FLAG_FILL;
		} else {
			flags &= ~COMMON_PARTITION_FLAG_FILL;
		}
		return part->set_flags(part, flags);
	default:
		return -1;
	}

	return 0;
}

static int part_get_property(const meta_object_t *meta, size_t i,
			     const object_t *obj, property_value_t *value)
{
	partition_t *part = (partition_t *)obj;
	uint64_t flags = part->get_flags(part);
	(void)meta;

	switch (i) {
	case PART_GROW:
		value->type = PROPERTY_TYPE_BOOL;
		value->value.boolean = !!(flags & COMMON_PARTITION_FLAG_GROW);
		break;
	case PART_FILL:
		value->type = PROPERTY_TYPE_BOOL;
		value->value.boolean = !!(flags & COMMON_PARTITION_FLAG_FILL);
		break;
	default:
		return -1;
	}

	return 0;
}

const meta_object_t partition_meta = {
	.name = "partition_t",
	.parent = NULL,

	.get_property_count = part_get_property_count,
	.get_property_desc = part_get_property_desc,
	.set_property = part_set_property,
	.get_property = part_get_property,
};
