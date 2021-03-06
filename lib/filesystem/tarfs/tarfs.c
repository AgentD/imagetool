/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tarfs.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tarfs.h"

static int append_zero_size_files(filesystem_t *fs, ostream_t *vstrm,
				  unsigned int *counter)
{
	tree_node_t *fit = fs->fstree->nodes_by_type[TREE_NODE_FILE];

	for (; fit != NULL; fit = fit->next_by_type) {
		if (fstree_file_physical_size(fs->fstree, fit) != 0)
			continue;

		if (tarfs_write_header(vstrm, fit, *counter))
			return -1;

		(*counter) += 1;
	}

	return 0;
}

static int append_hard_links(filesystem_t *fs, ostream_t *vstrm,
			     unsigned int *counter)
{
	tree_node_t *fit = fs->fstree->nodes_by_type[TREE_NODE_HARD_LINK];

	for (; fit != NULL; fit = fit->next_by_type) {
		if (tarfs_write_hard_link(vstrm, fit, *counter))
			return -1;

		(*counter) += 1;
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
		ret = tarfs_write_header((ostream_t *)null_sink, fit, *counter);
		if (ret)
			return -1;

		start = fit->data.file.start_index *
			fs->fstree->volume->blocksize;

		ret = fstree_add_gap(fs->fstree, fit->data.file.start_index,
				     null_sink->bytes_written);
		if (ret)
			return -1;

		vstrm = volume_ostream_create(fs->fstree->volume,
					      "tar filesystem", start,
					      null_sink->bytes_written);
		if (vstrm == NULL)
			return -1;

		if (tarfs_write_header(vstrm, fit, *counter))
			return -1;

		*counter += 1;
		object_drop(vstrm);
	}

	object_drop(null_sink);
	return 0;
}

static int write_tree_dfs(ostream_t *out, unsigned int *counter,
			  tree_node_t *n)
{
	int ret;

	if (n->type == TREE_NODE_FILE || n->type == TREE_NODE_HARD_LINK)
		return 0;

	if (n->parent != NULL) {
		ret = tarfs_write_header(out, n, *counter);
		if (ret != 0)
			return -1;

		(*counter) += 1;
	}

	if (n->type == TREE_NODE_DIR) {
		for (n = n->data.dir.children; n != NULL; n = n->next) {
			if (write_tree_dfs(out, counter, n))
				return -1;
		}
	}
	return 0;
}

static int estimate_tree_size(filesystem_t *fs, uint64_t *size)
{
	null_ostream_t *null_sink = null_ostream_create();
	unsigned int counter = 0;

	if (null_sink == NULL)
		return -1;

	if (write_tree_dfs((ostream_t *)null_sink, &counter, fs->fstree->root))
		return -1;

	*size = null_sink->bytes_written;
	object_drop(null_sink);
	return 0;
}

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

	if (fstree_add_gap(fs->fstree, 0, size))
		goto fail_internal;

	vstrm = volume_ostream_create(fs->fstree->volume, "tar filesystem",
				      0, size);
	if (vstrm == NULL)
		goto fail_internal;

	counter = 0;
	if (write_tree_dfs(vstrm, &counter, fs->fstree->root))
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

filesystem_t *filesystem_tar_create(volume_t *volume)
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
