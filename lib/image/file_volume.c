/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_volume.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"
#include "bitmap.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

typedef struct {
	volume_t base;

	char *filename;
	int fd;

	bitmap_t *bitmap;

	uint8_t scratch[];
} file_volume_t;

/*****************************************************************************/

static int read_retry(const char *filename, int fd, uint64_t offset,
		      void *data, size_t size)
{
	while (size > 0) {
		ssize_t ret = pread(fd, data, size, offset);

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			perror(filename);
			return -1;
		}

		if (ret == 0) {
			fprintf(stderr, "%s: truncated read\n", filename);
			return -1;
		}

		data = (char *)data + ret;
		size -= ret;
		offset += ret;
	}

	return 0;
}

static int write_retry(const char *filename, int fd, uint64_t offset,
		       const void *data, size_t size)
{
	while (size > 0) {
		ssize_t ret = pwrite(fd, data, size, offset);

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			perror(filename);
			return -1;
		}

		if (ret == 0) {
			fprintf(stderr, "%s: truncated write\n", filename);
			return -1;
		}

		data = (const char *)data + ret;
		size -= ret;
		offset += ret;
	}

	return 0;
}

/*****************************************************************************/

static void destroy(object_t *base)
{
	file_volume_t *fvol = (file_volume_t *)base;

	object_drop(fvol->bitmap);

	close(fvol->fd);
	free(fvol->filename);
	free(fvol);
}

static int read_partial_block(volume_t *vol, uint64_t index,
			      void *buffer, uint32_t offset, uint32_t size)
{
	file_volume_t *fvol = (file_volume_t *)vol;

	if (index >= vol->max_block_count)
		goto fail_bounds;

	if (offset > vol->blocksize)
		goto fail_bounds;

	if (size > (vol->blocksize - offset))
		goto fail_bounds;

	if (size == 0)
		return 0;

	if (!bitmap_is_set(fvol->bitmap, index)) {
		memset(buffer, 0, size);
		return 0;
	}

	return read_retry(fvol->filename, fvol->fd,
			  index * vol->blocksize + offset,
			  buffer, size);
fail_bounds:
	fprintf(stderr, "%s: out of bounds read attempted.\n",
		fvol->filename);
	return -1;
}

static int read_block(volume_t *vol, uint64_t index, void *buffer)
{
	return read_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static int write_partial_block(volume_t *vol, uint64_t index,
			       const void *buffer, uint32_t offset,
			       uint32_t size)
{
	file_volume_t *fvol = (file_volume_t *)vol;

	if (index >= vol->max_block_count)
		goto fail_bounds;

	if (offset > vol->blocksize)
		goto fail_bounds;

	if (size > (vol->blocksize - offset))
		goto fail_bounds;

	if (bitmap_set(fvol->bitmap, index))
		goto fail_flag;

	return write_retry(fvol->filename, fvol->fd,
			   index * vol->blocksize + offset,
			   buffer, size);
fail_flag:
	fprintf(stderr, "%s: failed to mark block as used.\n", fvol->filename);
	return -1;
fail_bounds:
	fprintf(stderr, "%s: out of bounds write attempted.\n",
		fvol->filename);
	return -1;
}

static int write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	return write_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static int discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	int ret;

	memset(fvol->scratch, 0, vol->blocksize);

	while (count--) {
		if (!bitmap_is_set(fvol->bitmap, index)) {
			++index;
			continue;
		}

		ret = write_retry(fvol->filename, fvol->fd,
				  index * vol->blocksize,
				  fvol->scratch, vol->blocksize);
		if (ret)
			return -1;

		bitmap_clear(fvol->bitmap, index);
		++index;
	}

	return 0;
}

static int move_block(volume_t *vol, uint64_t src, uint64_t dst, int flags)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	void *src_ptr, *dst_ptr;
	bool src_set, dst_set;

	src_set = bitmap_is_set(fvol->bitmap, src);
	dst_set = bitmap_is_set(fvol->bitmap, dst);

	if (!src_set && !dst_set)
		return 0;

	src_ptr = fvol->scratch;
	dst_ptr = fvol->scratch + vol->blocksize;

	if (read_block(vol, src, src_ptr))
		return -1;

	if (flags & (MOVE_SWAP | MOVE_ERASE_SOURCE)) {
		if (flags & MOVE_ERASE_SOURCE) {
			memset(dst_ptr, 0, vol->blocksize);
			dst_set = false;
		} else {
			if (read_block(vol, dst, dst_ptr))
				return -1;
		}

		if (write_block(vol, src, dst_ptr))
			return -1;

		bitmap_set_value(fvol->bitmap, src, dst_set);
	}

	if (write_block(vol, dst, src_ptr))
		return -1;

	bitmap_set_value(fvol->bitmap, dst, src_set);
	return 0;
}

static int commit(volume_t *vol)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	uint64_t size;
	size_t index;

	index = bitmap_msb_index(fvol->bitmap);

	if (index == 0 && !bitmap_is_set(fvol->bitmap, 0)) {
		size = 0;
	} else {
		size = (index + 1) * vol->blocksize;
	}

	if (ftruncate(fvol->fd, size) != 0) {
		perror(fvol->filename);
		return -1;
	}

	if (fsync(fvol->fd) != 0) {
		perror(fvol->filename);
		return -1;
	}

	return 0;
}

volume_t *volume_from_fd(const char *filename, int fd, uint64_t max_size)
{
	uint64_t i, used, max_count;
	file_volume_t *fvol = NULL;
	size_t blocksize;
	struct stat sb;

	/* determine block size, current block count, maximum block count */
	if (fstat(fd, &sb) != 0)
		goto fail;

	blocksize = sb.st_blksize;
	if (blocksize == 0)
		blocksize = 512;

	used = sb.st_size / blocksize;

	if (sb.st_size % blocksize) {
		used += 1;

		if (ftruncate(fd, used * blocksize) != 0)
			goto fail;
	}

	max_count = max_size / blocksize;
	if (max_size % blocksize)
		max_count += 1;

	/* create wrapper */
	fvol = calloc(1, sizeof(*fvol) + 2 * blocksize);
	if (fvol == NULL)
		goto fail;

	fvol->filename = strdup(filename);
	if (fvol->filename == NULL)
		goto fail;

	fvol->bitmap = bitmap_create(used);
	if (fvol->bitmap == NULL)
		goto fail;

	fvol->fd = fd;
	((object_t *)fvol)->refcount = 1;
	((object_t *)fvol)->destroy = destroy;
	((volume_t *)fvol)->blocksize = blocksize;
	((volume_t *)fvol)->min_block_count = 0;
	((volume_t *)fvol)->max_block_count = max_count;
	((volume_t *)fvol)->read_block = read_block;
	((volume_t *)fvol)->read_partial_block = read_partial_block;
	((volume_t *)fvol)->write_block = write_block;
	((volume_t *)fvol)->write_partial_block = write_partial_block;
	((volume_t *)fvol)->move_block = move_block;
	((volume_t *)fvol)->discard_blocks = discard_blocks;
	((volume_t *)fvol)->commit = commit;

	/* fill the used block bitmap */
	for (i = 0; i < used; ++i) {
		bitmap_set(fvol->bitmap, i);
	}

	return (volume_t *)fvol;
fail:
	perror(filename);

	if (fvol != NULL) {
		if (fvol->bitmap != NULL)
			object_drop(fvol->bitmap);

		free(fvol->filename);
		free(fvol);
	}
	return NULL;
}
