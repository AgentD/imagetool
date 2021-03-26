/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fatfs.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fatfs.h"

static void compute_fs_parameters(uint64_t disk_size, fatfs_filesystem_t *fatfs)
{
	uint32_t i, sectors = disk_size / SECTOR_SIZE;
	uint32_t secs_per_cluster = 1;
	uint32_t clusters = sectors;

	for (i = 1; i <= (CLUSTER_SIZE_PREFERRED / SECTOR_SIZE); ++i) {
		uint32_t new = sectors / i;

		if (new < FAT32_MIN_SECTORS)
			break;

		secs_per_cluster = i;
		clusters = new;
	}

	fatfs->secs_per_cluster = secs_per_cluster;
	fatfs->secs_per_fat = clusters / FAT32_ENTRIES_PER_SECTOR;

	if (clusters % FAT32_ENTRIES_PER_SECTOR)
		fatfs->secs_per_fat += 1;

	fatfs->fatsize = fatfs->secs_per_fat * SECTOR_SIZE;
}

static int compute_dir_sizes(fatfs_filesystem_t *fatfs, uint64_t *total)
{
	filesystem_t *fs = (filesystem_t *)fatfs;
	tree_node_t *it = fs->fstree->nodes_by_type[TREE_NODE_DIR];
	uint32_t cluster_size = CLUSTER_SIZE_PREFERRED;
	null_ostream_t *ostrm;
	uint64_t offset = 0;

	/* XXX: empty root requires special treatment because it
	   contains no . or .. entries */
	if (fs->fstree->root->data.dir.children == NULL) {
		fs->fstree->root->data.dir.start = 0;
		fs->fstree->root->data.dir.size = 0;
		*total = 1;
		return 0;
	}

	/* compute size of each directory + total required size */
	ostrm = null_ostream_create();

	if (ostrm == NULL)
		return -1;

	*total = 0;

	for (; it != NULL; it = it->next_by_type) {
		ostrm->bytes_written = 0;

		if (fatfs_serialize_directory(fatfs, it, (ostream_t *)ostrm))
			goto fail;

		it->data.dir.size = ostrm->bytes_written;

		(*total) += it->data.dir.size / cluster_size;
		if (it->data.dir.size % cluster_size)
			(*total) += 1;
	}

	ostrm = object_drop(ostrm);
	(*total) *= cluster_size;

	/* compute cluster aligned locations of each directory */
	fs->fstree->root->data.dir.start = offset;

	offset += fs->fstree->root->data.dir.size;
	if (offset % cluster_size)
		offset += cluster_size - (offset % cluster_size);

	it = fs->fstree->nodes_by_type[TREE_NODE_DIR];

	for (; it != NULL; it = it->next_by_type) {
		if (it == fs->fstree->root)
			continue;

		it->data.dir.start = offset;

		offset += it->data.dir.size;
		if (offset % cluster_size)
			offset += cluster_size - (offset % cluster_size);
	}

	return 0;
fail:
	object_drop(ostrm);
	return -1;
}

static int write_directory_contents(fatfs_filesystem_t *fatfs)
{
	filesystem_t *fs = (filesystem_t *)fatfs;
	tree_node_t *it = fs->fstree->nodes_by_type[TREE_NODE_DIR];

	for (; it != NULL; it = it->next_by_type) {
		ostream_t *ostrm;

		ostrm = volume_ostream_create(fs->fstree->volume, it->name,
					      it->data.dir.start,
					      it->data.dir.size);
		if (ostrm == NULL)
			return -1;

		if (fatfs_serialize_directory(fatfs, it, ostrm)) {
			object_drop(ostrm);
			return -1;
		}

		object_drop(ostrm);
	}

	return 0;
}

static int enforce_min_size(fatfs_filesystem_t *fat)
{
	uint64_t size;

	size = fat->orig_volume->get_block_count(fat->orig_volume);
	if (MUL64_OV(fat->orig_volume->blocksize, size, &size))
		size = MAX_DISK_SIZE;

	if (size < (FAT32_MIN_SECTORS * SECTOR_SIZE)) {
		size = FAT32_MIN_SECTORS * SECTOR_SIZE;

		if (fat->orig_volume->truncate(fat->orig_volume, size))
			goto fail_resize;
	}

	return 0;
fail_resize:
	fprintf(stderr, "Error resizing volume to minimum required "
		"size for FAT32 (~%d MiB).\n", FAT32_MIN_SECTORS / (2 * 1024));
	return -1;
}

static void adjust_file_indices(fatfs_filesystem_t *fat)
{
	filesystem_t *fs = (filesystem_t *)fat;
	tree_node_t *it = fs->fstree->nodes_by_type[TREE_NODE_FILE];
	uint32_t old_secs_per_cluster = CLUSTER_SIZE_PREFERRED / SECTOR_SIZE;

	for (; it != NULL; it = it->next_by_type) {
		uint32_t sector;

		sector = it->data.file.start_index * old_secs_per_cluster;
		it->data.file.start_index = sector / fat->secs_per_cluster;
	}
}

static int build_format(filesystem_t *fs)
{
	fatfs_filesystem_t *fat = (fatfs_filesystem_t *)fs;
	uint64_t dir_size, size;
	int ret;

	fstree_sort(fs->fstree);

	if (compute_dir_sizes(fat, &dir_size))
		goto fail_serialize;

	if (fstree_add_gap(fs->fstree, 0, dir_size))
		goto fail_serialize;

	if (enforce_min_size(fat))
		return -1;

	size = fat->orig_volume->get_block_count(fat->orig_volume);
	if (MUL64_OV(fat->orig_volume->blocksize, size, &size))
		size = MAX_DISK_SIZE;

	compute_fs_parameters(size, fat);
	adjust_file_indices(fat);

	if (write_directory_contents(fat))
		goto fail_serialize;

	/* move the data out of the way */
	ret = volume_memmove(fat->orig_volume,
			     FAT32_FAT_START + 2 * fat->fatsize, 0,
			     fs->fstree->data_offset *
			     fs->fstree->volume->blocksize);
	if (ret)
		goto fail_fat;

	ret = volume_write(fat->orig_volume, 0, NULL,
			   FAT32_FAT_START + 2 * fat->fatsize);
	if (ret)
		goto fail_fat;

	if (fatfs_write_super_block(fat))
		goto fail_fat;

	if (fatfs_build_fats(fat))
		goto fail_fat;

	return 0;
fail_serialize:
	fputs("Error serializing FAT directory tree.\n", stderr);
	return -1;
fail_fat:
	fputs("Error building FAT data structures.\n", stderr);
	return -1;
}

static void destroy(object_t *obj)
{
	fatfs_filesystem_t *fatfs = (fatfs_filesystem_t *)obj;
	filesystem_t *fs = (filesystem_t *)fatfs;

	object_drop(fatfs->orig_volume);
	object_drop(fs->fstree);
	free(fs);
}

filesystem_t *filesystem_fatfs_create(volume_t *volume)
{
	fatfs_filesystem_t *fatfs = calloc(1, sizeof(*fatfs));
	filesystem_t *fs = (filesystem_t *)fatfs;
	object_t *obj = (object_t *)fatfs;
	volume_t *adapter = NULL;
	uint64_t size;

	if (fs == NULL)
		goto fail_errno;

	if (MUL64_OV(volume->blocksize, volume->get_max_block_count(volume),
		     &size)) {
		size = MAX_DISK_SIZE;
	}

	if (size < (FAT32_MIN_SECTORS * SECTOR_SIZE)) {
		fprintf(stderr, "Cannot make FAT 32 filesystem with less "
			"than %d sectors (%d MiB)\n", FAT32_MIN_SECTORS,
			FAT32_MIN_SECTORS / (2 * 1024));
		goto fail;
	}

	adapter = volume_blocksize_adapter_create(volume,
						  CLUSTER_SIZE_PREFERRED, 0);
	if (adapter == NULL)
		goto fail;

	fs->fstree = fstree_create(adapter);
	if (fs->fstree == NULL)
		goto fail_errno;

	adapter = object_drop(adapter);

	fs->fstree->flags |= FSTREE_FLAG_NO_SPARSE;

	strcpy((char *)fatfs->fs_oem, "Goliath");
	strcpy((char *)fatfs->fs_label, "NO NAME");

	fatfs->secs_per_cluster = CLUSTER_SIZE_PREFERRED / SECTOR_SIZE;
	fatfs->orig_volume = object_grab(volume);
	fs->build_format = build_format;
	obj->refcount = 1;
	obj->destroy = destroy;
	obj->meta = &fatfs_meta;
	return fs;
fail_errno:
	perror("creating FAT filesystem");
fail:
	if (adapter != NULL)
		object_drop(adapter);
	free(fs);
	return NULL;
}
