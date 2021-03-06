/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fat.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fatfs.h"

#define FAT_WINDOW_SIZE (4096)

static int dir_compare_location(const tree_node_t *lhs, const tree_node_t *rhs)
{
	if (lhs->data.dir.start < rhs->data.dir.start)
		return -1;

	return (lhs->data.dir.start > rhs->data.dir.start) ? 1 : 0;
}

static int file_compare_location(const tree_node_t *lhs, const tree_node_t *rhs)
{
	if (lhs->data.file.start_index < rhs->data.file.start_index)
		return -1;

	return (lhs->data.file.start_index >
		rhs->data.file.start_index) ? 1 : 0;
}

/*****************************************************************************/

static int slide_window(fatfs_filesystem_t *fs, unsigned char *window,
			size_t *window_offset, size_t *next_wr_offset)
{
	size_t offset, diff;

	if (fs->fatsize <= FAT_WINDOW_SIZE)
		return 0;

	if ((*next_wr_offset) <= (FAT_WINDOW_SIZE / 2))
		return 0;

	offset = FAT32_FAT_START + *window_offset;
	diff = FAT_WINDOW_SIZE / 2;

	if (volume_write(fs->orig_volume, offset, window, diff))
		return -1;

	offset += fs->fatsize;

	if (volume_write(fs->orig_volume, offset, window, diff))
		return -1;

	memmove(window, window + diff, *next_wr_offset - diff);

	*window_offset += diff;
	*next_wr_offset -= diff;

	memset(window + *next_wr_offset, 0, FAT_WINDOW_SIZE - *next_wr_offset);
	return 0;
}

static int flush_window(fatfs_filesystem_t *fs, unsigned char *window,
			size_t window_offset)
{
	size_t offset = FAT32_FAT_START + window_offset;
	size_t size = fs->fatsize - window_offset;

	size = size > FAT_WINDOW_SIZE ? FAT_WINDOW_SIZE : size;

	if (volume_write(fs->orig_volume, offset, window, size))
		return -1;

	offset += fs->fatsize;

	return volume_write(fs->orig_volume, offset, window, size);
}

/*****************************************************************************/

static int write_cluster_chain_32(fatfs_filesystem_t *fs,
				  unsigned char *window, size_t *window_offset,
				  size_t index, size_t count)
{
	size_t i, fat_offset = index * 4 - *window_offset;
	uint32_t next;

	for (i = 0; i < count; ++i) {
		if (slide_window(fs, window, window_offset, &fat_offset))
			return -1;

		next = (i + 1) < count ? (index + i + 1) : 0xFFFFFFFF;
		next &= 0x0FFFFFFF;

		window[fat_offset++] =  next        & 0xFF;
		window[fat_offset++] = (next >>  8) & 0xFF;
		window[fat_offset++] = (next >> 16) & 0xFF;
		window[fat_offset++] = (next >> 24) & 0xFF;
	}

	return 0;
}

/*****************************************************************************/

int fatfs_build_fats(fatfs_filesystem_t *fs)
{
	filesystem_t *base = (filesystem_t *)fs;
	size_t window_offset, clustersize;
	unsigned char *window = NULL;
	tree_node_t *list, *it;

	clustersize = fs->secs_per_cluster * SECTOR_SIZE;

	if (volume_write(fs->orig_volume, FAT32_FAT_START, NULL, fs->fatsize))
		return -1;

	/* initialize the sliding window and FAT */
	window_offset = 0;
	window = calloc(1, fs->fatsize < FAT_WINDOW_SIZE ?
			FAT_WINDOW_SIZE : fs->fatsize);
	if (window == NULL) {
		perror("creating FAT buffer");
		return -1;
	}

	memcpy(window, "\xF0\xFF\xFF\x0F\xFF\xFF\xFF\x0F", 8);

	/* handle directory entries */
	list = base->fstree->nodes_by_type[TREE_NODE_DIR];
	list = fstree_sort_node_list_by_type(list, dir_compare_location);
	base->fstree->nodes_by_type[TREE_NODE_DIR] = list;

	for (it = list; it != NULL; it = it->next_by_type) {
		uint32_t index = it->data.dir.start / clustersize;
		uint32_t count = it->data.dir.size / clustersize;

		if ((it->data.dir.size % clustersize) || it->data.dir.size == 0)
			count += 1;

		index += CLUSTER_OFFSET;

		if (write_cluster_chain_32(fs, window, &window_offset,
					   index, count)) {
			free(window);
			return -1;
		}
	}

	/* handle files */
	list = base->fstree->nodes_by_type[TREE_NODE_FILE];
	list = fstree_sort_node_list_by_type(list, file_compare_location);
	base->fstree->nodes_by_type[TREE_NODE_FILE] = list;

	for (it = list; it != NULL; it = it->next_by_type) {
		uint32_t index = it->data.file.start_index + CLUSTER_OFFSET;
		uint32_t count = it->data.file.size / clustersize;

		if (it->data.file.size % clustersize)
			count += 1;

		if (write_cluster_chain_32(fs, window, &window_offset,
					   index, count)) {
			free(window);
			return -1;
		}
	}

	/* cleanup */
	if (flush_window(fs, window, window_offset)) {
		free(window);
		return -1;
	}

	free(window);
	return 0;
}
