/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * meta.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mbr.h"

enum {
	MBR_TYPE = 0,
	MBR_BOOTABLE,
};

static const property_enum_t mbr_types[] = {
	{ .name = "LinuxSwap", .value = MBR_PARTITION_TYPE_LINUX_SWAP, },
	{ .name = "Linux", .value = MBR_PARTITION_TYPE_LINUX_DATA, },
	{ .name = "FreeBSD", .value = MBR_PARTITION_TYPE_FREEBSD_DATA, },
	{ .name = "OpenBSD", .value = MBR_PARTITION_TYPE_OPENBSD_DATA, },
	{ .name = "NetBSD", .value = MBR_PARTITION_TYPE_NETBSD_DATA, },
	{ .name = "BSDI", .value = MBR_PARTITION_TYPE_BSDI_DATA, },
	{ .name = "Minix", .value = MBR_PARTITION_TYPE_MINIX_DATA, },
	{ .name = "UnixWare", .value = MBR_PARTITION_TYPE_UNIXWARE_DATA, },
};

static const property_desc_t mbr_part_prop[] = {
	[MBR_TYPE] = {
		.type = PROPERTY_TYPE_ENUM,
		.name = "type",
		.enum_ent = mbr_types,
		.enum_count = sizeof(mbr_types) / sizeof(mbr_types[0]),
	},
	[MBR_BOOTABLE] = {
		.type = PROPERTY_TYPE_BOOL,
		.name = "bootable",
	},
};

static size_t mbr_part_get_property_count(const meta_object_t *meta)
{
	(void)meta;
	return sizeof(mbr_part_prop) / sizeof(mbr_part_prop[0]);
}

static int mbr_part_get_property_desc(const meta_object_t *meta, size_t i,
				      property_desc_t *desc)
{
	(void)meta;

	if (i >= (sizeof(mbr_part_prop) / sizeof(mbr_part_prop[0])))
		return -1;

	*desc = mbr_part_prop[i];
	return 0;
}

static int mbr_part_set_property(const meta_object_t *meta, size_t i,
				 object_t *obj, const property_value_t *value)
{
	partition_t *part = (partition_t *)obj;
	uint64_t flags = part->get_flags(part);
	(void)meta;

	switch (i) {
	case MBR_TYPE:
		if (value->type != PROPERTY_TYPE_ENUM)
			return -1;
		if (value->value.ival & ~0x0FF00000)
			return -1;

		flags &= ~0x0FF00000;
		flags |= (uint64_t)value->value.ival << 20UL;

		return part->set_flags(part, flags);
	case MBR_BOOTABLE:
		if (value->value.boolean) {
			flags |= MBR_PARTITION_FLAG_BOOTABLE;
		} else {
			flags &= ~MBR_PARTITION_FLAG_BOOTABLE;
		}
		return part->set_flags(part, flags);
	default:
		return -1;
	}

	return 0;
}

static int mbr_part_get_property(const meta_object_t *meta, size_t i,
				 const object_t *obj,
				 property_value_t *value)
{
	partition_t *part = (partition_t *)obj;
	uint64_t flags = part->get_flags(part);
	(void)meta;

	switch (i) {
	case MBR_TYPE:
		value->type = PROPERTY_TYPE_ENUM;
		value->value.ival = (flags >> 20) & 0x0FF;
		break;
	case MBR_BOOTABLE:
		value->type = PROPERTY_TYPE_BOOL;
		value->value.boolean = !!(flags & MBR_PARTITION_FLAG_BOOTABLE);
		break;
	default:
		return -1;
	}

	return 0;
}

const meta_object_t mbr_part_meta = {
	.name = "mbr_part_t",
	.parent = &partition_meta,

	.get_property_count = mbr_part_get_property_count,
	.get_property_desc = mbr_part_get_property_desc,
	.set_property = mbr_part_set_property,
	.get_property = mbr_part_get_property,
};
