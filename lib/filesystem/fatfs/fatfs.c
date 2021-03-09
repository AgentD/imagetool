/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fatfs.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fatfs.h"

static void compute_fs_parameters(uint64_t disk_size, fatfs_filesystem_t *fatfs)
{
	uint32_t slots_per_fatsec;

	if (disk_size > MAX_DISK_SIZE)
		disk_size = MAX_DISK_SIZE;

	fatfs->total_sectors = disk_size / SECTOR_SIZE;

	if (disk_size <= MAX_FLOPPY_SIZE) {
		fatfs->type = FAT_TYPE_12;
		fatfs->secs_per_cluster = 1;
		fatfs->total_clusters = fatfs->total_sectors;

		while (fatfs->total_clusters > 4000) {
			fatfs->secs_per_cluster *= 2;
			fatfs->total_clusters =
				fatfs->total_sectors / fatfs->secs_per_cluster;
		}

		fatfs->secs_per_fat = (3 * fatfs->total_clusters) /
			(2 * SECTOR_SIZE);

		if ((3 * fatfs->total_clusters) % (2 * SECTOR_SIZE))
			fatfs->secs_per_fat += 1;
	} else if (fatfs->total_sectors <= FAT16_SECTOR_THRESHOLD) {
		fatfs->type = FAT_TYPE_16;
		fatfs->secs_per_cluster = 8;
		fatfs->total_clusters = fatfs->total_sectors / 8;

		while (fatfs->total_clusters < 5000) {
			fatfs->secs_per_cluster /= 2;
			fatfs->total_clusters =
				fatfs->total_sectors / fatfs->secs_per_cluster;
		}

		slots_per_fatsec = SECTOR_SIZE / 2;
		fatfs->secs_per_fat = fatfs->total_clusters / slots_per_fatsec;
		if (fatfs->total_clusters % slots_per_fatsec)
			fatfs->secs_per_fat += 1;
	} else {
		fatfs->type = FAT_TYPE_32;
		fatfs->secs_per_cluster = 8;
		fatfs->total_clusters = fatfs->total_sectors / 8;

		while (fatfs->total_clusters < 66000) {
			fatfs->secs_per_cluster /= 2;
			fatfs->total_clusters =
				fatfs->total_sectors / fatfs->secs_per_cluster;
		}

		slots_per_fatsec = SECTOR_SIZE / 4;
		fatfs->secs_per_fat = fatfs->total_clusters / slots_per_fatsec;
		if (fatfs->total_clusters % slots_per_fatsec)
			fatfs->secs_per_fat += 1;
	}

	if (fatfs->type == FAT_TYPE_32) {
		fatfs->fatstart = FAT32_RESERVED_COUNT * SECTOR_SIZE;
	} else {
		fatfs->fatstart = SECTOR_SIZE;
	}

	fatfs->fatsize = fatfs->secs_per_fat * SECTOR_SIZE;
}

static int compute_dir_sizes(fatfs_filesystem_t *fatfs, uint64_t *total)
{
	filesystem_t *fs = (filesystem_t *)fatfs;
	tree_node_t *it = fs->fstree->nodes_by_type[TREE_NODE_DIR];
	uint32_t cluster_size = SECTOR_SIZE * fatfs->secs_per_cluster;
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

static int build_format(filesystem_t *fs)
{
	uint64_t dir_size;

	fstree_sort(fs->fstree);

	if (compute_dir_sizes((fatfs_filesystem_t *)fs, &dir_size))
		goto fail_serialize;

	if (fstree_add_gap(fs->fstree, 0, dir_size))
		goto fail_serialize;

	if (write_directory_contents((fatfs_filesystem_t *)fs))
		goto fail_serialize;

	if (fatfs_write_super_block((fatfs_filesystem_t *)fs))
		goto fail_super;

	if (fatfs_build_fats((fatfs_filesystem_t *)fs))
		goto fail_fat;

	return 0;
fail_super:
	fputs("Error writing FAT information structures.\n", stderr);
	return -1;
fail_serialize:
	fputs("Error serializing FAT directory tree.\n", stderr);
	return -1;
fail_fat:
	fputs("Error building FAT data structure.\n", stderr);
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
	uint64_t size, rsvp;

	if (fs == NULL)
		goto fail_errno;

	if (MUL64_OV(volume->blocksize, volume->get_max_block_count(volume),
		     &size)) {
		size = MAX_DISK_SIZE;
	}

	compute_fs_parameters(size, fatfs);

	rsvp = fatfs->fatstart + 2 * fatfs->fatsize;
	size = fatfs->secs_per_cluster * (uint64_t)SECTOR_SIZE;

	adapter = volume_blocksize_adapter_create(volume, size, rsvp);
	if (adapter == NULL)
		goto fail;

	fs->fstree = fstree_create(adapter);
	if (fs->fstree == NULL)
		goto fail_errno;

	adapter = object_drop(adapter);

	fs->fstree->flags |= FSTREE_FLAG_NO_SPARSE;

	fatfs->orig_volume = object_grab(volume);
	fs->build_format = build_format;
	obj->refcount = 1;
	obj->destroy = destroy;
	return fs;
fail_errno:
	perror("creating FAT filesystem");
fail:
	if (adapter != NULL)
		object_drop(adapter);
	free(fs);
	return NULL;
}
