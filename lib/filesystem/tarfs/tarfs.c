/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tarfs.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tarfs.h"

static int tarfs_add_gap(fstree_t *fs, uint64_t index, uint64_t size)
{
	uint64_t src, dst, totalsz, diff;
	tree_node_t *fit;

	/* move the file data out of the way */
	if (size % fs->volume->blocksize)
		size += fs->volume->blocksize - size % fs->volume->blocksize;

	src = index * fs->volume->blocksize;
	dst = src + size;

	totalsz = (fs->data_offset - index) * fs->volume->blocksize;

	if (volume_memmove(fs->volume, dst, src, totalsz))
		return -1;

	/* adjust the file offsets of everything after the gap */
	diff = size / fs->volume->blocksize;
	fs->data_offset += diff;

	fit = fs->nodes_by_type[TREE_NODE_FILE];

	while (fit != NULL) {
		if (fit->data.file.start_index >= index)
			fit->data.file.start_index += diff;

		fit = fit->next_by_type;
	}

	/* clear the gap we just created */
	return fs->volume->discard_blocks(fs->volume, index, diff);
}

static int append_zero_size_files(filesystem_t *fs, ostream_t *vstrm,
				  unsigned int *counter)
{
	tree_node_t *fit = fs->fstree->nodes_by_type[TREE_NODE_FILE];

	for (; fit != NULL; fit = fit->next_by_type) {
		if (fstree_file_physical_size(fs->fstree, fit) != 0)
			continue;

		if (tarfs_write_file_hdr(vstrm, counter, fit))
			return -1;
	}

	return 0;
}

static int append_hard_links(filesystem_t *fs, ostream_t *vstrm,
			     unsigned int *counter)
{
	tree_node_t *fit = fs->fstree->nodes_by_type[TREE_NODE_HARD_LINK];

	for (; fit != NULL; fit = fit->next_by_type) {
		if (tarfs_write_hard_link(vstrm, counter, fit))
			return -1;
	}

	return 0;
}

static int insert_file_headers(filesystem_t *fs, unsigned int *counter)
{
	null_ostream_t *null_sink;
	tree_node_t *fit;
	ostream_t *vstrm;
	uint64_t start;
	int ret;

	null_sink = null_ostream_create();
	if (null_sink == NULL)
		return -1;

	fit = fs->fstree->nodes_by_type[TREE_NODE_FILE];

	for (; fit != NULL; fit = fit->next_by_type) {
		if (fstree_file_physical_size(fs->fstree, fit) == 0)
			continue;

		null_sink->bytes_written = 0;
		ret = tarfs_write_file_hdr((ostream_t *)null_sink,
					   counter, fit);
		if (ret)
			return -1;
		*counter -= 1;

		start = fit->data.file.start_index *
			fs->fstree->volume->blocksize;

		ret = tarfs_add_gap(fs->fstree, fit->data.file.start_index,
				    null_sink->bytes_written);
		if (ret)
			return -1;

		vstrm = volume_ostream_create(fs->fstree->volume,
					      "tar filesystem", start,
					      null_sink->bytes_written);
		if (vstrm == NULL)
			return -1;

		if (tarfs_write_file_hdr(vstrm, counter, fit))
			return -1;

		vstrm = object_drop(vstrm);
	}

	object_drop(null_sink);
	return 0;
}

static int estimate_tree_size(filesystem_t *fs, uint64_t *size)
{
	null_ostream_t *null_sink = null_ostream_create();
	unsigned int counter = 0;

	if (null_sink == NULL)
		return -1;

	if (tarfs_write_tree_dfs((ostream_t *)null_sink, &counter,
				 fs->fstree->root)) {
		return -1;
	}

	*size = null_sink->bytes_written;
	object_drop(null_sink);
	return 0;
}

/****************************************************************************/

static int tarfs_build_format(filesystem_t *fs)
{
	ostream_t *vstrm = NULL;
	unsigned int counter;
	uint64_t start, size;

	fstree_sort(fs->fstree);

	if (fstree_resolve_hard_links(fs->fstree)) {
		fputs("Error resolving hard links for tar filesystem.\n",
		      stderr);
		return -1;
	}

	if (estimate_tree_size(fs, &size))
		goto fail_internal;

	if (tarfs_add_gap(fs->fstree, 0, size))
		goto fail_internal;

	vstrm = volume_ostream_create(fs->fstree->volume, "tar filesystem",
				      0, size);
	if (vstrm == NULL)
		goto fail_internal;

	counter = 0;
	if (tarfs_write_tree_dfs(vstrm, &counter, fs->fstree->root))
		goto fail;
	vstrm = object_drop(vstrm);

	if (insert_file_headers(fs, &counter))
		goto fail_internal;

	start = fs->fstree->data_offset * fs->fstree->volume->blocksize;
	vstrm = volume_ostream_create(fs->fstree->volume, "tar filesystem",
				      start, 0xFFFFFFFFFFFFFFFF);
	if (vstrm == NULL)
		goto fail_internal;

	if (append_zero_size_files(fs, vstrm, &counter))
		goto fail;

	if (append_hard_links(fs, vstrm, &counter))
		goto fail;

	object_drop(vstrm);
	return 0;
fail_internal:
	fputs("Internal error creating tar filesystem.\n", stderr);
fail:
	if (vstrm != NULL)
		object_drop(vstrm);
	return -1;
}

static void tarfs_destroy(object_t *base)
{
	filesystem_t *fs = (filesystem_t *)base;

	object_drop(fs->fstree);
	free(fs);
}

/****************************************************************************/

static filesystem_t *tarfs_create_instance(volume_t *volume)
{
	filesystem_t *fs = calloc(1, sizeof(*fs));
	volume_t *wrapper;

	if (fs == NULL) {
		perror("creating tar filesystem instance");
		return NULL;
	}

	if (volume->blocksize == TAR_RECORD_SIZE) {
		fs->fstree = fstree_create(volume);
		if (fs->fstree == NULL)
			goto fail_free;
	} else {
		wrapper = volume_blocksize_adapter_create(volume,
							  TAR_RECORD_SIZE, 0);
		if (wrapper == NULL)
			goto fail_free;

		fs->fstree = fstree_create(wrapper);
		if (fs->fstree == NULL)
			goto fail_wrapper;

		wrapper = object_drop(wrapper);
	}

	fs->fstree->flags |= FSTREE_FLAG_NO_SPARSE;

	fs->build_format = tarfs_build_format;
	((object_t *)fs)->refcount = 1;
	((object_t *)fs)->destroy = tarfs_destroy;
	return fs;
fail_wrapper:
	object_drop(wrapper);
fail_free:
	free(fs);
	return NULL;
}

static void tarfs_factory_destroy(object_t *obj)
{
	free(obj);
}

filesystem_factory_t *filesystem_factory_tar_create(void)
{
	filesystem_factory_t *factory = calloc(1, sizeof(*factory));

	if (factory == NULL) {
		perror("creating tar filesystem factory");
		return NULL;
	}

	((object_t *)factory)->refcount = 1;
	((object_t *)factory)->destroy = tarfs_factory_destroy;
	factory->name = "tar";
	factory->create_instance = tarfs_create_instance;
	return factory;
}
