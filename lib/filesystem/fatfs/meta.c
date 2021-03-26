/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fatfs.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fatfs.h"

enum {
	FS_OEM = 0,
	FS_LABEL,
};

static const property_desc_t fatfs_prop[] = {
	[FS_OEM] = {
		.type = PROPERTY_TYPE_STRING,
		.name = "oem",
	},
	[FS_LABEL] = {
		.type = PROPERTY_TYPE_STRING,
		.name = "label",
	},
};

static size_t fatfs_get_property_count(const meta_object_t *meta)
{
	(void)meta;
	return sizeof(fatfs_prop) / sizeof(fatfs_prop[0]);
}

static int fatfs_get_property_desc(const meta_object_t *meta, size_t i,
				   property_desc_t *desc)
{
	(void)meta;

	if (i >= (sizeof(fatfs_prop) / sizeof(fatfs_prop[0])))
		return -1;

	*desc = fatfs_prop[i];
	return 0;
}

static int fatfs_set_property(const meta_object_t *meta, size_t i,
			      object_t *obj, const property_value_t *value)
{
	fatfs_filesystem_t *fatfs = (fatfs_filesystem_t *)obj;
	size_t len;
	(void)meta;

	switch (i) {
	case FS_OEM:
		if (value->type != PROPERTY_TYPE_STRING)
			return -1;

		len = strlen(value->value.string);

		if (len >= sizeof(fatfs->fs_oem)) {
			fprintf(stderr, "FAT OEM name can be at "
				"most %zu characters long.\n",
				sizeof(fatfs->fs_oem) - 1);
			return -1;
		}

		memcpy(fatfs->fs_oem, value->value.string, len);
		fatfs->fs_oem[len] = '\0';
		break;
	case FS_LABEL:
		if (value->type != PROPERTY_TYPE_STRING)
			return -1;

		len = strlen(value->value.string);

		if (len >= sizeof(fatfs->fs_label)) {
			fprintf(stderr, "FAT filesystem label name can be at "
				"most %zu characters long.\n",
				sizeof(fatfs->fs_label) - 1);
			return -1;
		}

		memcpy(fatfs->fs_label, value->value.string, len);
		fatfs->fs_label[len] = '\0';
		break;
	default:
		return -1;
	}

	return 0;
}

static int fatfs_get_property(const meta_object_t *meta, size_t i,
			      const object_t *obj, property_value_t *value)
{
	fatfs_filesystem_t *fatfs = (fatfs_filesystem_t *)obj;
	(void)meta;

	switch (i) {
	case FS_OEM:
		value->type = PROPERTY_TYPE_STRING;
		value->value.string = (const char *)fatfs->fs_oem;
		break;
	case FS_LABEL:
		value->type = PROPERTY_TYPE_STRING;
		value->value.string = (const char *)fatfs->fs_label;
		break;
	default:
		return -1;
	}

	return 0;
}

const meta_object_t fatfs_meta = {
	.name = "fatfs_filesystem_t",
	.parent = NULL,

	.get_property_count = fatfs_get_property_count,
	.get_property_desc = fatfs_get_property_desc,
	.set_property = fatfs_set_property,
	.get_property = fatfs_get_property,
};
