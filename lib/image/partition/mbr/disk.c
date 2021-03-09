/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * disk.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mbr.h"

/* (63 sectors) * (254 heads) * (1023 cylinders) */
#define MAX_LBA (16370046)

static void lba_to_chs(uint32_t lba, uint8_t *chs)
{
	/* XXX: sane default values imposed by the UEFI spec */
	static uint32_t sectors_per_track = 63;
	static uint32_t heads_per_cylinder = 254;
	uint8_t h, s;
	uint16_t c;

	if (lba >= MAX_LBA) {
		c = 1023;
		h = 254;
		s = 63;
	} else {
		c = lba / (heads_per_cylinder * sectors_per_track);
		h = (lba / sectors_per_track) % heads_per_cylinder;
		s = 1 + (lba % sectors_per_track);
	}

	chs[0] = h;
	chs[1] = ((c >> 2) & 0xC0) | (s & 0x3F);
	chs[2] = c & 0xFF;
}

static partition_t *mbr_disk_create_parition(partition_mgr_t *parent,
					     uint64_t blk_count,
					     uint64_t flags)
{
	mbr_disk_t *disk = (mbr_disk_t *)parent;
	uint32_t min_count, max_count;
	mbr_part_t *part;
	uint64_t index;
	size_t i;
	int ret;

	if (disk->part_used == MAX_MBR_PARTITIONS) {
		fprintf(stderr, "Cannot create more than %d "
			"partitions on an MBR disk.\n", MAX_MBR_PARTITIONS);
		return NULL;
	}

	if (blk_count == 0)
		blk_count = MBR_PART_ALIGN;

	if (blk_count % MBR_PART_ALIGN)
		blk_count += MBR_PART_ALIGN - (blk_count % MBR_PART_ALIGN);

	index = MBR_RESERVED;

	for (i = 0; i < disk->part_used; ++i) {
		uint64_t next_free = disk->partitions[i].index;
		next_free += disk->partitions[i].blk_count;

		if (next_free > index)
			index = next_free;
	}

	disk->partitions[disk->part_used].index = index;
	disk->partitions[disk->part_used].blk_count = blk_count;
	disk->partitions[disk->part_used].blk_count_min = blk_count;
	disk->partitions[disk->part_used].flags = flags;

	if (blk_count > 0) {
		while (blk_count--) {
			ret = disk->volume->write_block(disk->volume,
							index++, NULL);
			if (ret)
				return NULL;
		}
	}

	min_count = disk->partitions[disk->part_used].blk_count;

	if (flags & COMMON_PARTION_FLAG_GROW) {
		max_count = disk->volume->get_max_block_count(disk->volume);
		max_count -= index;
	} else {
		max_count = min_count;
	}

	part = mbr_part_create(disk, disk->part_used);
	if (part == NULL)
		return NULL;

	disk->part_used += 1;
	return (partition_t *)part;
}

static int mbr_disk_commit(partition_mgr_t *mgr)
{
	mbr_disk_t *disk = (mbr_disk_t *)mgr;
	mbr_header_t header;
	uint32_t lba, count;
	size_t i;

	for (i = 0; i < disk->part_used; ++i) {
		if (mbr_shrink_to_fit(disk, i))
			return -1;
	}

	for (i = 0; i < disk->part_used; ++i) {
		if (mbr_apply_expand_policy(disk, i))
			return -1;
	}

	memset(&header, 0, sizeof(header));

	memset(header.boot_code, 0x90, sizeof(header.boot_code));

	for (i = 0; i < disk->part_used; ++i) {
		/* layout */
		lba = disk->partitions[i].index;
		count = disk->partitions[i].blk_count;

		header.partitions[i].first_sector_lba = htole32(lba);
		header.partitions[i].num_sectors = htole32(count);

		lba_to_chs(lba, header.partitions[i].first_sector_chs);
		lba_to_chs(count > 0 ? (lba + count - 1) : lba,
			   header.partitions[i].last_sector_chs);

		/* flags */
		if (disk->partitions[i].flags & MBR_PARTITION_FLAG_BOOTABLE)
			header.partitions[i].flags = 0x80;

		header.partitions[i].type =
			(disk->partitions[i].flags >> 20) & 0x0FF;
	}

	header.boot_magic = htole16(IBM_BOOT_MAGIC);

	if (disk->volume->write_block(disk->volume, 0, &header))
		return -1;

	return disk->volume->commit(disk->volume);
}

static void mbr_disk_destroy(object_t *obj)
{
	object_drop(((mbr_disk_t *)obj)->volume);
	free(obj);
}

partition_mgr_t *mbrdisk_create(volume_t *base)
{
	mbr_disk_t *disk = calloc(1, sizeof(*disk));
	partition_mgr_t *mgr = (partition_mgr_t *)disk;
	object_t *obj = (object_t *)disk;

	if (disk == NULL) {
		perror("creating MBR disk");
		return NULL;
	}

	if (base->blocksize == SECTOR_SIZE) {
		disk->volume = object_grab(base);
	} else {
		disk->volume = volume_blocksize_adapter_create(base,
							       SECTOR_SIZE, 0);
		if (disk->volume == NULL) {
			fputs("Error creating blocksize adapter "
			      "for MBR disk.\n",stderr);
			return NULL;
		}
	}

	mgr->commit = mbr_disk_commit;
	mgr->create_parition = mbr_disk_create_parition;
	obj->destroy = mbr_disk_destroy;
	obj->refcount = 1;
	return mgr;
}
