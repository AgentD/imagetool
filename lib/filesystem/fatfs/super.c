/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * super.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fatfs.h"

static int write_super_block_fat32(fatfs_filesystem_t *fatfs)
{
	fat32_super_t super;

	memset(&super, 0, sizeof(super));

	memcpy(super.oem_name, "Goliath ",    sizeof(super.oem_name));
	memcpy(super.fs_name,  "FAT32   ",    sizeof(super.fs_name));
	memcpy(super.label,    "NO NAME    ", sizeof(super.label));

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
	super.total_sector_count   = htole32(fatfs->total_sectors);
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

static int write_fs_info_block(fatfs_filesystem_t *fatfs)
{
	filesystem_t *fs = (filesystem_t *)fatfs;
	uint32_t free_count, next_free;
	fat32_info_sector_t info;

	free_count = fatfs->total_clusters - fs->fstree->data_offset;
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
	if (write_super_block_fat32(fatfs))
		return -1;

	return write_fs_info_block(fatfs);
}
