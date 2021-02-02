/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_volume.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"
#include "bitmap.h"
#include "util.h"

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

static int transfer_blocks(file_volume_t *fvol, uint64_t src, uint64_t dst,
			   size_t count)
{
	size_t size, diff, processed;
	loff_t off_in, off_out;
	ssize_t ret;

	/* fast-path: ask the kernel to copy-on-write remap the region */
	processed = 0;
	size = ((volume_t *)fvol)->blocksize * count;
	off_in = src * size;
	off_out = dst * size;

	while (processed < size) {
		ret = copy_file_range(fvol->fd, &off_in,
				      fvol->fd, &off_out,
				      size - processed, 0);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		processed += ret;
	}

	/* fallback: manually copy over the remaining data */
	while (processed < size) {
		diff = size - processed;
		if (diff > ((volume_t *)fvol)->blocksize)
			diff = ((volume_t *)fvol)->blocksize;

		if (read_retry(fvol->filename, fvol->fd, off_in,
			       fvol->scratch, diff)) {
			return -1;
		}

		if (write_retry(fvol->filename, fvol->fd, off_out,
				fvol->scratch, diff)) {
			return -1;
		}

		processed += size;
	}

	return 0;
}

static int swap_blocks(file_volume_t *fvol, uint64_t src, uint64_t dst)
{
	volume_t *vol = (volume_t *)fvol;

	if (read_retry(fvol->filename, fvol->fd, src * vol->blocksize,
		       fvol->scratch + vol->blocksize, vol->blocksize)) {
		return -1;
	}

	if (transfer_blocks(fvol, dst, src, 1))
		return -1;

	return write_retry(fvol->filename, fvol->fd, dst * vol->blocksize,
			   fvol->scratch + vol->blocksize, vol->blocksize);
}

static int check_bounds(file_volume_t *fvol, uint64_t index,
			uint32_t offset, uint32_t size)
{
	if (index >= ((volume_t *)fvol)->max_block_count)
		goto fail;

	if (offset > ((volume_t *)fvol)->blocksize)
		goto fail;

	if (size > (((volume_t *)fvol)->blocksize - offset))
		goto fail;

	return 0;
fail:
	fprintf(stderr, "%s: out of bounds block access attempted.\n",
		fvol->filename);
	return -1;
}

static int truncate_file(int fd, uint64_t size)
{
	int ret;

	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

static int punch_hole(int fd, uint64_t offset, uint64_t size)
{
	int ret;

	do {
		ret = fallocate(fd,
				FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
				offset, size);
	} while (ret < 0 && errno == EINTR);

	return ret;
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

	if (check_bounds(fvol, index, offset, size))
		return -1;

	if (size == 0)
		return 0;

	if (!bitmap_is_set(fvol->bitmap, index)) {
		memset(buffer, 0, size);
		return 0;
	}

	return read_retry(fvol->filename, fvol->fd,
			  index * vol->blocksize + offset,
			  buffer, size);
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

	if (check_bounds(fvol, index, offset, size))
		return -1;

	if (bitmap_set(fvol->bitmap, index))
		goto fail_flag;

	if (buffer == NULL) {
		memset(fvol->scratch, 0, size);
		buffer = fvol->scratch;
	}

	return write_retry(fvol->filename, fvol->fd,
			   index * vol->blocksize + offset,
			   buffer, size);
fail_flag:
	fprintf(stderr, "%s: failed to mark block as used.\n", fvol->filename);
	return -1;
}

static int discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	int ret;

	/* sanity check */
	if (index >= vol->max_block_count)
		return 0;

	if (count > (vol->max_block_count - index))
		count = vol->max_block_count - index;

	if (count == 0)
		return 0;

	/* fast-path */
	if (count == (vol->max_block_count - index)) {
		ret = truncate_file(fvol->fd, index * vol->blocksize);
	} else {
		ret = punch_hole(fvol->fd, index * vol->blocksize,
				 count * vol->blocksize);
	}

	if (ret == 0) {
		while (count--)
			bitmap_clear(fvol->bitmap, index++);

		return 0;
	}

	/* fallback: manually write block of 0 bytes */
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

static int write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	if (buffer == NULL)
		return discard_blocks(vol, index, 1);

	return write_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static int move_block(volume_t *vol, uint64_t src, uint64_t dst, int mode)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	bool src_set, dst_set;

	if (check_bounds(fvol, src, 0, vol->blocksize))
		return -1;

	if (check_bounds(fvol, dst, 0, vol->blocksize))
		return -1;

	src_set = bitmap_is_set(fvol->bitmap, src);
	dst_set = bitmap_is_set(fvol->bitmap, dst);

	switch (mode) {
	case MOVE_SWAP:
		if (src == dst || (!src_set && !dst_set))
			return 0;

		if (src_set && dst_set)
			return swap_blocks(fvol, src, dst);

		if (src_set) {
			if (transfer_blocks(fvol, src, dst, 1))
				return -1;
			if (bitmap_set(fvol->bitmap, dst))
				goto fail_flag;
			return discard_blocks(vol, src, 1);
		}

		if (transfer_blocks(fvol, dst, src, 1))
			return -1;
		if (bitmap_set(fvol->bitmap, src))
			goto fail_flag;
		return discard_blocks(vol, dst, 1);
	case MOVE_ERASE_SOURCE:
		if (src == dst)
			return src_set ? discard_blocks(vol, src, 1) : 0;

		if (!src_set)
			return dst_set ? discard_blocks(vol, dst, 1) : 0;

		if (transfer_blocks(fvol, src, dst, 1))
			return -1;
		if (bitmap_set(fvol->bitmap, dst))
			goto fail_flag;
		return discard_blocks(vol, src, 1);
	default:
		if (src == dst || (!src_set && !dst_set))
			return 0;

		if (!src_set)
			return dst_set ? discard_blocks(vol, dst, 1) : 0;

		if (transfer_blocks(fvol, src, dst, 1))
			return -1;
		if (bitmap_set(fvol->bitmap, dst))
			goto fail_flag;
		break;
	}

	return 0;
fail_flag:
	fprintf(stderr, "%s: failed to mark block as used after move.\n",
		fvol->filename);
	return -1;
}

static int move_block_partial(volume_t *vol, uint64_t src, uint64_t dst,
			      size_t src_offset, size_t dst_offset,
			      size_t size)
{
	file_volume_t *fvol = (file_volume_t *)vol;

	if (check_bounds(fvol, src, src_offset, size))
		return -1;

	if (check_bounds(fvol, dst, dst_offset, size))
		return -1;

	if (read_retry(fvol->filename, fvol->fd,
		       src * vol->blocksize + src_offset,
		       fvol->scratch, size)) {
		return -1;
	}

	return write_retry(fvol->filename, fvol->fd,
			   dst * vol->blocksize + dst_offset,
			   fvol->scratch, size);
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

	if (truncate_file(fvol->fd, size) != 0) {
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
	((volume_t *)fvol)->move_block_partial = move_block_partial;
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
