/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fatfs.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FATFS_H
#define FATFS_H

#include "filesystem.h"
#include "fstream.h"
#include "fstree.h"
#include "volume.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define SECTOR_SIZE (512)
#define FAT32_RESERVED_COUNT (32)

#define MAX_DISK_SIZE (1024UL * 1024UL * 1024UL * 1024UL)
#define FAT32_MIN_SECTORS (66000)

#define FAT_CHAR_PER_LONG_ENT (13)
#define SEQ_NUMBER_LAST_FLAG (0x40)

#define CLUSTER_OFFSET (2)

#define IBM_BOOT_MAGIC (0xAA55)
#define MAGIC_VOLUME_ID (0xDECAFBAD)
#define FAT_BOOT_SIG_MAGIC (0x29)
#define FAT_DRIVE_NUMBER (0x80)

#define INFO_MAGIC_1 (0x41615252)
#define INFO_MAGIC_2 (0x61417272)
#define INFO_MAGIC_3 (0xAA550000)

#define FAT_BS_COPY_INDEX (6)
#define FAT_FS_INFO_INDEX (1)
#define FAT_MEDIA_DESCRIPTOR_DISK (0xF8)

typedef enum {
	FAT_SHORTNAME_ERROR = -1,

	/* short name was converted successfully */
	FAT_SHORTNAME_OK = 0,

	/* short name is the same as the original */
	FAT_SHORTNAME_SAME = 1,

	/* a "~generation_number" suffix was appended */
	FAT_SHORTNAME_SUFFIXED = 2,
} FAT_SHORTNAME;

typedef enum {
	DIR_ENT_READ_ONLY = 0x01,
	DIR_ENT_HIDDEN = 0x02,
	DIR_ENT_SYSTEM = 0x04,
	DIR_ENT_VOLUME_ID = 0x08,
	DIR_ENT_DIRECTORY = 0x10,
	DIR_ENT_ARCHIVE = 0x20,
	DIR_ENT_LFN = 0x0F,
} DIR_ENT_FLAGS;

typedef struct {
	uint8_t name[8];
	uint8_t extension[3];
	uint8_t flags;
	uint8_t unused[2];
	uint16_t ctime_hms;
	uint16_t ctime_ymd;
	uint16_t atime_ymd;
	uint16_t cluster_index_high;
	uint16_t mtime_hms;
	uint16_t mtime_ymd;
	uint16_t cluster_index_low;
	uint32_t size;
} fatfs_dir_ent_t;

typedef struct {
	uint8_t seq_number;
	uint16_t name_part[5];
	uint8_t attrib;
	uint8_t entry_type;
	uint8_t short_checksum;
	uint16_t name_part2[6];
	uint16_t unused;
	uint16_t name_part3[2];
} __attribute__((packed)) fatfs_long_dir_ent_t;

typedef struct {
	uint8_t jump[3];
	uint8_t oem_name[8];

	/* DOS 2.0 BPB */
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t num_reserved_sectors;
	uint8_t num_fats;
	uint8_t unused0[4];
	uint8_t media_descriptor;
	uint8_t unused1[2];

	/* DOS 3.31 BPB */
	uint16_t sectors_per_track;
	uint16_t heads_per_disk;
	uint8_t unused2[4];
	uint32_t total_sector_count;

	/* FAT32 BPB */
	uint32_t sectors_per_fat;
	uint16_t mirror_flags;
	uint16_t version;
	uint32_t root_dir_index;
	uint16_t fs_info_index;
	uint16_t boot_sec_copy_index;
	uint8_t unused3[12];
	uint8_t phys_drive_num;
	uint8_t wtf1;
	uint8_t ext_boot_signature;
	uint32_t volume_id;
	uint8_t label[11];
	uint8_t fs_name[8];

	/* boot sector code */
	uint8_t boot_code[420];

	/* magic signature */
	uint16_t boot_signature;
} __attribute__((packed)) fat32_super_t;

typedef struct {
	uint32_t magic1;
	uint8_t reserved0[480];
	uint32_t magic2;
	uint32_t num_free_clusters;
	uint32_t next_free_cluster;
	uint8_t reserved1[12];
	uint32_t magic3;
} fat32_info_sector_t;

typedef struct {
	filesystem_t base;

	volume_t *orig_volume;

	uint32_t secs_per_cluster;
	uint32_t secs_per_fat;
	uint32_t total_sectors;
	uint32_t total_clusters;

	size_t fatstart;
	size_t fatsize;
} fatfs_filesystem_t;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t fatfs_convert_timestamp(int64_t timestamp);

FAT_SHORTNAME fatfs_mk_shortname(const uint8_t *name, uint8_t shortname[11],
				 size_t len, unsigned int gen);

int fatfs_serialize_directory(fatfs_filesystem_t *fs, tree_node_t *root,
			      ostream_t *ostrm);

int fatfs_write_super_block(fatfs_filesystem_t *fs);

int fatfs_build_fats(fatfs_filesystem_t *fs);

#ifdef __cplusplus
}
#endif

#endif /* FATFS_H */
