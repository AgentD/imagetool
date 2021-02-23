/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * cpiofs.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "cpiofs.h"

static int write_tree(filesystem_t *fs, ostream_t *strm)
{
	tree_node_t *n, *tgt, dummy;
	char *path;
	size_t i;

	for (i = 0; i < fs->fstree->num_inodes; ++i) {
		n = fs->fstree->inode_table[i];

		if (n->type != TREE_NODE_FILE) {
			if (cpio_write_header(strm, n, false))
				return -1;
		}
	}

	n = fs->fstree->nodes_by_type[TREE_NODE_HARD_LINK];

	for (; n != NULL; n = n->next_by_type) {
		tgt = n->data.link.resolved;

		if (tgt->type != TREE_NODE_FILE) {
			path = fstree_get_path(n);
			if (path != NULL)
				fstree_canonicalize_path(path);

			fprintf(stderr, "%s: cpio cannot store hardlinks "
				"to something not a file\n", path);
			free(path);
			return -1;
		}

		dummy = *tgt;
		dummy.parent = n->parent;
		dummy.name = n->name;

		if (cpio_write_header(strm, &dummy, true))
			return -1;
	}

	return 0;
}

static int estimate_tree_size(filesystem_t *fs, uint64_t *size)
{
	null_ostream_t *null_sink = null_ostream_create();

	if (null_sink == NULL)
		return -1;

	if (write_tree(fs, (ostream_t *)null_sink))
		return -1;

	*size = null_sink->bytes_written;
	object_drop(null_sink);
	return 0;
}

static int insert_file_headers(filesystem_t *fs)
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

		if (cpio_write_header((ostream_t *)null_sink, fit, false)) {
			object_drop(null_sink);
			return -1;
		}

		start = fit->data.file.start_index *
			fs->fstree->volume->blocksize;

		ret = fstree_add_gap(fs->fstree, fit->data.file.start_index,
				     null_sink->bytes_written);
		if (ret)
			return -1;

		vstrm = volume_ostream_create(fs->fstree->volume,
					      "cpio filesystem", start,
					      null_sink->bytes_written);
		if (vstrm == NULL)
			return -1;

		if (cpio_write_header(vstrm, fit, false))
			return -1;

		object_drop(vstrm);
	}

	object_drop(null_sink);
	return 0;
}

static int append_zero_size_files(filesystem_t *fs, ostream_t *vstrm)
{
	tree_node_t *fit = fs->fstree->nodes_by_type[TREE_NODE_FILE];

	for (; fit != NULL; fit = fit->next_by_type) {
		if (fstree_file_physical_size(fs->fstree, fit) != 0)
			continue;

		if (cpio_write_header(vstrm, fit, false))
			return -1;
	}

	return 0;
}

static int cpio_build_format(filesystem_t *fs)
{
	ostream_t *vstrm = NULL;
	uint64_t start, size;

	/* prepare tree */
	fstree_sort(fs->fstree);

	if (fstree_resolve_hard_links(fs->fstree)) {
		fputs("Error resolving hard links for cpio filesystem.\n",
		      stderr);
		return -1;
	}

	if (fstree_create_inode_table(fs->fstree)) {
		fputs("Error creating inode table for cpio filesystem.\n",
		      stderr);
		return -1;
	}

	/* serialize the tree except for all the files */
	if (estimate_tree_size(fs, &size))
		goto fail_internal;

	if (fstree_add_gap(fs->fstree, 0, size))
		goto fail_internal;

	vstrm = volume_ostream_create(fs->fstree->volume, "cpio filesystem",
				      0, size);
	if (vstrm == NULL)
		goto fail_internal;

	if (write_tree(fs, vstrm))
		goto fail;

	vstrm = object_drop(vstrm);

	/* add headers in front of the files */
	if (insert_file_headers(fs))
		goto fail_internal;

	/* append all the zero size files and the CPIO trailer */
	start = fs->fstree->data_offset * fs->fstree->volume->blocksize;

	vstrm = volume_ostream_create(fs->fstree->volume, "cpio filesystem",
				      start, 0xFFFFFFFFFFFFFFFF);
	if (vstrm == NULL)
		goto fail_internal;

	if (append_zero_size_files(fs, vstrm))
		goto fail_internal;

	if (cpio_write_trailer(start, vstrm))
		goto fail_internal;

	object_drop(vstrm);
	return 0;
fail_internal:
	fputs("Internal error creating cpio filesystem.\n", stderr);
fail:
	if (vstrm != NULL)
		object_drop(vstrm);
	return -1;
}

static void cpio_destroy(object_t *base)
{
	filesystem_t *fs = (filesystem_t *)base;

	object_drop(fs->fstree);
	free(fs);
}

filesystem_t *filesystem_cpio_create(volume_t *volume)
{
	filesystem_t *fs = calloc(1, sizeof(*fs));
	volume_t *wrapper;

	if (fs == NULL) {
		perror("creating cpio filesystem instance");
		return NULL;
	}

	wrapper = volume_blocksize_adapter_create(volume, 4, 0);
	if (wrapper == NULL)
		goto fail_free;

	fs->fstree = fstree_create(wrapper);
	if (fs->fstree == NULL)
		goto fail_wrapper;

	wrapper = object_drop(wrapper);
	fs->fstree->flags |= FSTREE_FLAG_NO_SPARSE;

	fs->build_format = cpio_build_format;
	((object_t *)fs)->refcount = 1;
	((object_t *)fs)->destroy = cpio_destroy;
	return fs;
fail_wrapper:
	object_drop(wrapper);
fail_free:
	free(fs);
	return NULL;
}
