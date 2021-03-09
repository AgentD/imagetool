/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mbr.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef MBR_H
#define MBR_H

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
		uint64_t blk_count_min;
		uint64_t blk_count;
		uint64_t blk_used;
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

#ifdef __cplusplus
extern "C" {
#endif

mbr_part_t *mbr_part_create(mbr_disk_t *parent, size_t index);

int mbr_shrink_to_fit(mbr_disk_t *disk, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* MBR_H */
