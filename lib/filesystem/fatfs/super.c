/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * super.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fatfs.h"

static int write_super_block_fat32(fatfs_filesystem_t *fatfs,
				   uint32_t sector_count)
{
	fat32_super_t super;
	size_t len;

	memset(&super, 0, sizeof(super));

	len = strlen((const char *)fatfs->fs_oem);
	len = len > sizeof(super.oem_name) ? sizeof(super.oem_name) : len;

	memset(super.oem_name, ' ', sizeof(super.oem_name));
	memcpy(super.oem_name, fatfs->fs_oem, len);

	len = strlen((const char *)fatfs->fs_label);
	len = len > sizeof(super.label) ? sizeof(super.label) : len;

	memset(super.label, ' ', sizeof(super.label));
	memcpy(super.label, fatfs->fs_label, len);

	memcpy(super.fs_name,  "FAT32   ",    sizeof(super.fs_name));

	super.boot_signature       = htole16(IBM_BOOT_MAGIC);
	super.volume_id            = htole32(MAGIC_VOLUME_ID);
	super.bytes_per_sector     = htole16(SECTOR_SIZE);
	super.num_reserved_sectors = htole16(FAT32_RESERVED_COUNT);
	super.phys_drive_num       = FAT_DRIVE_NUMBER;
	super.ext_boot_signature   = FAT_BOOT_SIG_MAGIC;
	super.sectors_per_cluster  = fatfs->secs_per_cluster;
	super.media_descriptor     = FAT_MEDIA_DESCRIPTOR_DISK;
	super.num_fats             = 2;
	super.fs_info_index        = htole16(FAT_FS_INFO_INDEX);
	super.boot_sec_copy_index  = htole16(FAT_BS_COPY_INDEX);
	super.sectors_per_track    = htole16(1);
	super.heads_per_disk       = htole16(1);
	super.total_sector_count   = htole32(sector_count);
	super.sectors_per_fat      = htole32(fatfs->secs_per_fat);
	super.root_dir_index       = htole32(CLUSTER_OFFSET);

	super.jump[0] = 0xEB;
	super.jump[1] = 0xFE;
	super.jump[2] = 0x90;
	memset(super.boot_code, 0x90, sizeof(super.boot_code));

	if (volume_write(fatfs->orig_volume, 0, &super, sizeof(super)))
		return -1;

	return volume_write(fatfs->orig_volume,
			    FAT_BS_COPY_INDEX * SECTOR_SIZE,
			    &super, sizeof(super));
}

static int write_fs_info_block(fatfs_filesystem_t *fatfs,
			       uint32_t sector_count)
{
	uint32_t cluster_count, free_count, next_free;
	filesystem_t *fs = (filesystem_t *)fatfs;
	fat32_info_sector_t info;

	cluster_count = sector_count - (FAT32_FAT_START / SECTOR_SIZE);
	cluster_count -= fatfs->secs_per_fat * 2;
	cluster_count /= fatfs->secs_per_cluster;

	free_count = cluster_count - fs->fstree->data_offset;
	next_free = fs->fstree->data_offset;

	memset(&info, 0, sizeof(info));
	info.magic1 = htole32(INFO_MAGIC_1);
	info.magic2 = htole32(INFO_MAGIC_2);
	info.num_free_clusters = htole32(free_count);
	info.next_free_cluster = htole32(next_free + CLUSTER_OFFSET);
	info.magic3 = htole32(INFO_MAGIC_3);

	return volume_write(fatfs->orig_volume,
			    FAT_FS_INFO_INDEX * SECTOR_SIZE,
			    &info, sizeof(info));
}

int fatfs_write_super_block(fatfs_filesystem_t *fatfs)
{
	uint32_t sector_count;
	uint64_t size;

	if (MUL64_OV(fatfs->orig_volume->blocksize,
		     fatfs->orig_volume->get_block_count(fatfs->orig_volume),
		     &size)) {
		size = MAX_DISK_SIZE;
	}

	if (size > MAX_DISK_SIZE)
		size = MAX_DISK_SIZE;

	sector_count = size / SECTOR_SIZE;

	if (write_super_block_fat32(fatfs, sector_count))
		return -1;

	return write_fs_info_block(fatfs, sector_count);
}
