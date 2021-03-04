/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mbrdisk.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "volume.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SECTOR_SIZE (512)
#define MAX_MBR_PARTITIONS (4)
#define IBM_BOOT_MAGIC (0xAA55)
#define MBR_PART_ALIGN (1024 * 1024 / SECTOR_SIZE)

typedef struct {
	partition_mgr_t base;

	volume_t *volume;

	size_t part_used;

	struct {
		uint64_t index;
		uint64_t blk_count;
		uint64_t flags;
	} partitions[MAX_MBR_PARTITIONS];
} mbr_disk_t;

typedef struct {
	partition_t base;

	mbr_disk_t *parent;
	size_t index;
} mbr_part_t;

typedef struct {
	uint8_t boot_code[446];

	struct {
		uint8_t flags;
		uint8_t first_sector_chs[3];
		uint8_t type;
		uint8_t last_sector_chs[3];
		uint32_t first_sector_lba;
		uint32_t num_sectors;
	} partitions[MAX_MBR_PARTITIONS];

	uint16_t boot_magic;
} __attribute__((packed)) mbr_header_t;

static void lba_to_chs(uint32_t lba, uint8_t *chs)
{
	/* XXX: sane default values imposed by the UEFI spec */
	static uint32_t sectors_per_track = 63;
	static uint32_t heads_per_cylinder = 254;

	uint16_t c = lba / (heads_per_cylinder * sectors_per_track);
	uint8_t h = (lba / sectors_per_track) % heads_per_cylinder;
	uint8_t s = 1 + (lba % sectors_per_track);

	chs[0] = h;
	chs[1] = ((c >> 2) & 0xC0) | (s & 0x3F);
	chs[2] = c & 0xFF;
}

/*****************************************************************************/

static int grow_partition(mbr_part_t *part, uint64_t diff)
{
	uint64_t start = part->parent->partitions[part->index].index;
	uint64_t count = part->parent->partitions[part->index].blk_count;
	uint64_t max = 0;
	size_t i;
	int ret;

	if (diff % MBR_PART_ALIGN || diff == 0)
		diff += MBR_PART_ALIGN - (diff % MBR_PART_ALIGN);

	for (i = 0; i < part->parent->part_used; ++i) {
		uint64_t current = part->parent->partitions[i].index;
		current += part->parent->partitions[i].blk_count - 1;

		if (current > max)
			max = current;
	}

	ret = volume_memmove(part->parent->volume,
			     (start + count + diff) * SECTOR_SIZE,
			     (start + count) * SECTOR_SIZE,
			     (max - (start + count - 1)) * SECTOR_SIZE);
	if (ret)
		return -1;

	ret = volume_write(part->parent->volume, (start + count) * SECTOR_SIZE,
			   NULL, diff * SECTOR_SIZE);
	if (ret)
		return -1;

	for (i = 0; i < part->parent->part_used; ++i) {
		if (part->parent->partitions[i].index > start)
			part->parent->partitions[i].index += diff;
	}

	part->parent->partitions[part->index].blk_count += diff;
	return 0;
}

static int part_read_partial_block(volume_t *vol, uint64_t index,
				   void *buffer, uint32_t offset,
				   uint32_t size)
{
	mbr_part_t *part = (mbr_part_t *)vol;
	uint64_t start = part->parent->partitions[part->index].index;
	uint64_t count = part->parent->partitions[part->index].blk_count;
	uint64_t flags = part->parent->partitions[part->index].flags;
	volume_t *volume = part->parent->volume;

	if (index >= count) {
		if (!(flags & COMMON_PARTION_FLAG_GROW)) {
			fprintf(stderr, "Out-of-bounds read on "
				"MBR partition %zu.\n", part->index);
			return -1;
		}

		memset(buffer, 0, vol->blocksize);
		return 0;
	}

	return volume->read_partial_block(volume, start + index, buffer,
					  offset, size);
}

static int part_write_partial_block(volume_t *vol, uint64_t index,
				    const void *buffer, uint32_t offset,
				    uint32_t size)
{
	mbr_part_t *part = (mbr_part_t *)vol;
	uint64_t start = part->parent->partitions[part->index].index;
	uint64_t count = part->parent->partitions[part->index].blk_count;
	uint64_t flags = part->parent->partitions[part->index].flags;
	volume_t *volume = part->parent->volume;

	if (index >= count) {
		if (!(flags & COMMON_PARTION_FLAG_GROW)) {
			fprintf(stderr, "Out-of-bounds write on "
				"MBR partition %zu.\n", part->index);
			return -1;
		}

		if (grow_partition(part, index - count + 1))
			return -1;

		count = part->parent->partitions[part->index].blk_count;
	}

	return volume->write_partial_block(volume, start + index, buffer,
					   offset, size);
}

static int part_discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	mbr_part_t *part = (mbr_part_t *)vol;
	uint64_t blk_start = part->parent->partitions[part->index].index;
	uint64_t blk_count = part->parent->partitions[part->index].blk_count;
	volume_t *volume = part->parent->volume;

	if (index >= blk_count)
		return 0;

	if (count > (blk_count - index))
		count = blk_count - index;

	if (count == 0)
		return 0;

	return volume->discard_blocks(volume, blk_start + index, count);
}

static int part_move_block_partial(volume_t *vol, uint64_t src, uint64_t dst,
				   size_t src_offset, size_t dst_offset,
				   size_t size)
{
	mbr_part_t *part = (mbr_part_t *)vol;
	uint64_t blk_start = part->parent->partitions[part->index].index;
	uint64_t blk_count = part->parent->partitions[part->index].blk_count;
	uint64_t flags = part->parent->partitions[part->index].flags;
	volume_t *volume = part->parent->volume;

	if (src >= blk_count || dst >= blk_count) {
		if (!(flags & COMMON_PARTION_FLAG_GROW)) {
			fprintf(stderr, "Out-of-bounds block move on "
				"MBR partition %zu.\n", part->index);
			return -1;
		}
	}

	if (src >= blk_count && dst >= blk_count)
		return 0;

	if (src >= blk_count)
		return part_discard_blocks(vol, dst, 1);

	if (dst >= blk_count) {
		if (grow_partition(part, dst - blk_count + 1))
			return -1;
	}

	if (src_offset == 0 && dst_offset == 0 && size == vol->blocksize) {
		return volume->move_block(volume, blk_start + src,
					  blk_start + dst);
	}

	return volume->move_block_partial(volume, blk_start + src,
					  blk_start + dst, src_offset,
					  dst_offset, size);
}

static int part_read_block(volume_t *vol, uint64_t index, void *buffer)
{
	return part_read_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static int part_write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	return part_write_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static int part_move_block(volume_t *vol, uint64_t src, uint64_t dst)
{
	return part_move_block_partial(vol, src, dst, 0, 0, vol->blocksize);
}

static int part_commit(volume_t *vol)
{
	(void)vol;
	return 0;
}

static void part_destroy(object_t *obj)
{
	mbr_part_t *part = (mbr_part_t *)obj;

	object_drop(part->parent);
	free(part);
}

static mbr_part_t *part_create(mbr_disk_t *parent, size_t index,
			       uint32_t min_count, uint32_t max_count)
{
	mbr_part_t *part = calloc(1, sizeof(*part));
	volume_t *vol = (volume_t *)part;
	object_t *obj = (object_t *)part;

	if (part == NULL) {
		perror("creating MBR partition");
		return NULL;
	}

	part->parent = object_grab(parent);
	part->index = index;
	vol->min_block_count = min_count;
	vol->max_block_count = max_count;
	vol->blocksize = parent->volume->blocksize;
	vol->read_partial_block = part_read_partial_block;
	vol->write_partial_block = part_write_partial_block;
	vol->move_block_partial = part_move_block_partial;
	vol->discard_blocks = part_discard_blocks;
	vol->read_block = part_read_block;
	vol->write_block = part_write_block;
	vol->move_block = part_move_block;
	vol->commit = part_commit;
	obj->refcount = 1;
	obj->destroy = part_destroy;
	return part;
}

/*****************************************************************************/

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

	index = MBR_PART_ALIGN;

	for (i = 0; i < disk->part_used; ++i) {
		uint64_t next_free = disk->partitions[i].index;
		next_free += disk->partitions[i].blk_count;

		if (next_free > index)
			index = next_free;
	}

	disk->partitions[disk->part_used].index = index;
	disk->partitions[disk->part_used].blk_count = blk_count;
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
		max_count = disk->volume->max_block_count - index;
	} else {
		max_count = min_count;
	}

	part = part_create(disk, disk->part_used, min_count, max_count);
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
