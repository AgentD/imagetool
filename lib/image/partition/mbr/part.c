/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * part.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mbr.h"

static int shrink_partition(mbr_disk_t *disk, size_t index, uint64_t diff)
{
	uint64_t start = disk->partitions[index].index;
	uint64_t count = disk->partitions[index].blk_count;
	uint64_t max = 0;
	size_t i;

	if (diff % MBR_PART_ALIGN)
		diff -= diff % MBR_PART_ALIGN;

	if (diff > count)
		diff = count;

	if ((count - diff) < MBR_PART_ALIGN)
		diff = (count - MBR_PART_ALIGN);

	if ((count - diff) < disk->partitions[index].blk_count_min)
		diff = count - disk->partitions[index].blk_count_min;

	if (diff == 0)
		return 0;

	for (i = 0; i < disk->part_used; ++i) {
		uint64_t current = disk->partitions[i].index;
		current += disk->partitions[i].blk_count - 1;

		if (current > max)
			max = current;
	}

	if (disk->volume->discard_blocks(disk->volume, max - diff, diff))
		return -1;

	for (i = 0; i < disk->part_used; ++i) {
		if (disk->partitions[i].index > start)
			disk->partitions[i].index -= diff;
	}

	disk->partitions[index].blk_count -= diff;
	return 0;
}

static int grow_partition(mbr_part_t *part, uint64_t diff)
{
	uint64_t start = part->parent->partitions[part->index].index;
	uint64_t count = part->parent->partitions[part->index].blk_count;
	uint64_t max = 0;
	size_t i;
	int ret;

	for (i = 0; i < part->parent->part_used; ++i) {
		if (part->parent->partitions[i].index > start) {
			if (mbr_shrink_to_fit(part->parent, i))
				return -1;
		}
	}

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
	uint64_t used = part->parent->partitions[part->index].blk_used;
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

	if (index >= used)
		part->parent->partitions[part->index].blk_used = (index + 1);

	return volume->write_partial_block(volume, start + index, buffer,
					   offset, size);
}

static int part_discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	mbr_part_t *part = (mbr_part_t *)vol;
	uint64_t blk_start = part->parent->partitions[part->index].index;
	uint64_t blk_used = part->parent->partitions[part->index].blk_used;
	volume_t *volume = part->parent->volume;

	if (index >= blk_used)
		return 0;

	if (count > (blk_used - index))
		count = blk_used - index;

	if (count == 0)
		return 0;

	if ((index + count) == blk_used)
		part->parent->partitions[part->index].blk_used = index;

	return volume->discard_blocks(volume, blk_start + index, count);
}

static int part_move_block_partial(volume_t *vol, uint64_t src, uint64_t dst,
				   size_t src_offset, size_t dst_offset,
				   size_t size)
{
	mbr_part_t *part = (mbr_part_t *)vol;
	uint64_t blk_start = part->parent->partitions[part->index].index;
	uint64_t blk_count = part->parent->partitions[part->index].blk_count;
	uint64_t blk_used = part->parent->partitions[part->index].blk_used;
	uint64_t flags = part->parent->partitions[part->index].flags;
	volume_t *volume = part->parent->volume;

	if (src >= blk_count || dst >= blk_count) {
		if (!(flags & COMMON_PARTION_FLAG_GROW)) {
			fprintf(stderr, "Out-of-bounds block move on "
				"MBR partition %zu.\n", part->index);
			return -1;
		}
	}

	if (src >= blk_used && dst >= blk_used)
		return 0;

	if (src >= blk_used)
		return part_discard_blocks(vol, dst, 1);

	if (dst >= blk_count) {
		if (grow_partition(part, dst - blk_count + 1))
			return -1;
	}

	if (dst >= blk_used)
		part->parent->partitions[part->index].blk_used = dst;

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

int mbr_shrink_to_fit(mbr_disk_t *disk, size_t index)
{
	uint64_t diff;

	if (disk->partitions[index].blk_used <
	    disk->partitions[index].blk_count) {
		diff = disk->partitions[index].blk_count -
			disk->partitions[index].blk_used;

		if (shrink_partition(disk, index, diff))
			return -1;
	}

	return 0;
}

static uint64_t get_min_count(volume_t *vol)
{
	mbr_part_t *part = (mbr_part_t *)vol;

	return part->parent->partitions[part->index].blk_count_min;
}

static uint64_t get_max_count(volume_t *vol)
{
	mbr_part_t *part = (mbr_part_t *)vol;
	uint64_t count;
	size_t i;

	count = part->parent->partitions[part->index].blk_count;

	if (part->parent->partitions[part->index].flags &
	    COMMON_PARTION_FLAG_GROW) {
		uint64_t used = 0;
		uint64_t free = part->parent->volume->
			get_max_block_count(part->parent->volume);

		for (i = 0; i < part->parent->part_used; ++i)
			used += part->parent->partitions[i].blk_count;

		free = used < free ? (free - used) : 0;
		count += free;
	}

	return count;
}

mbr_part_t *mbr_part_create(mbr_disk_t *parent, size_t index)
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
	vol->get_min_block_count = get_min_count;
	vol->get_max_block_count = get_max_count;
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
