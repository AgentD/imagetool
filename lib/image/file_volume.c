/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_volume.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"
#include "bitmap.h"

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

static void destroy(object_t *base)
{
	file_volume_t *fvol = (file_volume_t *)base;

	object_drop(fvol->bitmap);

	close(fvol->fd);
	free(fvol->filename);
	free(fvol);
}

static int read_block(volume_t *vol, uint64_t index, void *buffer)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	uint64_t offset;
	size_t count;
	ssize_t ret;

	if (index >= vol->max_block_count) {
		fprintf(stderr, "%s: out of bounds read attempted.\n",
			fvol->filename);
		return -1;
	}

	if (!bitmap_is_set(fvol->bitmap, index)) {
		memset(buffer, 0, vol->blocksize);
		return 0;
	}

	count = vol->blocksize;
	offset = index * vol->blocksize;

	while (count > 0) {
		ret = pread(fvol->fd, buffer, count, offset);

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			perror(fvol->filename);
			return -1;
		}

		if (ret == 0) {
			fprintf(stderr, "%s: truncated read\n",
				fvol->filename);
			return -1;
		}

		buffer = (char *)buffer + ret;
		count -= ret;
		offset += ret;
	}

	return 0;
}

static int write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	uint64_t offset;
	size_t count;
	ssize_t ret;

	if (index >= vol->max_block_count) {
		fprintf(stderr, "%s: out of bounds write attempted.\n",
			fvol->filename);
		return -1;
	}

	count = vol->blocksize;
	offset = index * vol->blocksize;

	if (index >= vol->blocks_used) {
		if (ftruncate(fvol->fd, offset + count) != 0) {
			perror(fvol->filename);
			return -1;
		}

		vol->blocks_used = index + 1;
	}

	if (bitmap_set(fvol->bitmap, index)) {
		fprintf(stderr, "%s: failed to mark block as used.\n",
			fvol->filename);
		return -1;
	}

	while (count > 0) {
		ret = pwrite(fvol->fd, buffer, count, offset);

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			perror(fvol->filename);
			return -1;
		}

		if (ret == 0) {
			fprintf(stderr, "%s: truncated write\n",
				fvol->filename);
			return -1;
		}

		buffer = (const char *)buffer + ret;
		count -= ret;
		offset += ret;
	}

	return 0;
}

static int swap_blocks(volume_t *vol, uint64_t a, uint64_t b)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	bool a_set, b_set;

	a_set = bitmap_is_set(fvol->bitmap, a);
	b_set = bitmap_is_set(fvol->bitmap, b);

	if (!a_set && !b_set)
		return 0;

	if (a_set && !b_set) {
		if (read_block(vol, a, fvol->scratch))
			return -1;

		if (write_block(vol, b, fvol->scratch))
			return -1;

		bitmap_clear(fvol->bitmap, a);
	} else if (!a_set && b_set) {
		if (read_block(vol, b, fvol->scratch))
			return -1;

		if (write_block(vol, a, fvol->scratch))
			return -1;

		bitmap_clear(fvol->bitmap, b);
	} else {
		if (read_block(vol, a, fvol->scratch))
			return -1;

		if (read_block(vol, b, fvol->scratch + vol->blocksize))
			return -1;

		if (write_block(vol, b, fvol->scratch))
			return -1;

		if (write_block(vol, a, fvol->scratch + vol->blocksize))
			return -1;
	}

	return 0;
}

static int read_data(volume_t *vol, uint64_t offset, void *buffer, size_t size)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	size_t diff, block_off;
	uint64_t index;

	index = offset / vol->blocksize;
	block_off = offset % vol->blocksize;

	while (size > 0) {
		if (block_off == 0 && size >= vol->blocksize) {
			if (read_block(vol, index, buffer))
				return -1;

			diff = vol->blocksize;
		} else {
			if (read_block(vol, index, fvol->scratch))
				return -1;

			diff = vol->blocksize - block_off;
			diff = (diff > size ? size : diff);

			memcpy(buffer, fvol->scratch + block_off, diff);
		}

		index += 1;
		size -= vol->blocksize;
		buffer = (char *)buffer + vol->blocksize;
		block_off = 0;
	}

	return 0;
}

static int write_data(volume_t *vol, uint64_t offset,
		      const void *buffer, size_t size)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	size_t diff, block_off;
	uint64_t index;

	index = offset / vol->blocksize;
	block_off = offset % vol->blocksize;

	while (size > 0) {
		if (block_off == 0 && size >= vol->blocksize) {
			if (write_block(vol, index, buffer))
				return -1;

			diff = vol->blocksize;
		} else {
			if (read_block(vol, index, fvol->scratch))
				return -1;

			diff = vol->blocksize - block_off;
			diff = (diff > size ? size : diff);

			memcpy(fvol->scratch + block_off, buffer, diff);

			if (write_block(vol, index, fvol->scratch))
				return -1;
		}

		index += 1;
		size -= vol->blocksize;
		buffer = (char *)buffer + vol->blocksize;
		block_off = 0;
	}

	return 0;
}

static int discard_blocks(volume_t *vol, uint64_t index, uint64_t count)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	bool shrink_file = false;

	while (count--)
		bitmap_clear(fvol->bitmap, index++);

	while (vol->blocks_used > 0 &&
	       !bitmap_is_set(fvol->bitmap, vol->blocks_used - 1)) {
		vol->blocks_used -= 1;
		shrink_file = true;
	}

	if (shrink_file) {
		if (ftruncate(fvol->fd, vol->blocks_used * vol->blocksize)) {
			perror(fvol->filename);
			return -1;
		}

		bitmap_shrink(fvol->bitmap);
	}

	return 0;
}

static int commit(volume_t *vol)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	uint64_t i;

	memset(fvol->scratch, 0, vol->blocksize);

	for (i = 0; i < vol->blocks_used; ++i) {
		if (bitmap_is_set(fvol->bitmap, i))
			continue;

		if (write_block(vol, i, fvol->scratch))
			return -1;

		bitmap_clear(fvol->bitmap, i);
	}

	if (fsync(fvol->fd) != 0) {
		perror(fvol->filename);
		return -1;
	}

	return 0;
}

static volume_t *create_sub_volume(volume_t *vol, uint64_t min_count,
				   uint64_t max_count)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	(void)min_count; (void)max_count;

	fprintf(stderr, "%s: cannot create sub-volumes on a raw file.\n",
		fvol->filename);
	return NULL;
}

volume_t *volume_from_file(const char *filename, uint32_t blocksize,
			   uint64_t min_count, uint64_t max_count)
{
	file_volume_t *fvol;

	fvol = calloc(1, sizeof(*fvol) + 2 * blocksize);
	if (fvol == NULL)
		goto fail;

	fvol->filename = strdup(filename);
	if (fvol->filename == NULL)
		goto fail;

	fvol->fd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0644);
	if (fvol->fd < 0)
		goto fail;

	if (min_count > 0) {
		if (ftruncate(fvol->fd, min_count * blocksize) != 0) {
			perror(filename);
			goto fail;
		}
	}

	fvol->bitmap = bitmap_create(min_count);
	if (fvol->bitmap == NULL)
		goto fail;

	((object_t *)fvol)->refcount = 1;
	((object_t *)fvol)->destroy = destroy;
	((volume_t *)fvol)->blocksize = blocksize;
	((volume_t *)fvol)->min_block_count = min_count;
	((volume_t *)fvol)->max_block_count = max_count;
	((volume_t *)fvol)->blocks_used = min_count;
	((volume_t *)fvol)->read_block = read_block;
	((volume_t *)fvol)->write_block = write_block;
	((volume_t *)fvol)->swap_blocks = swap_blocks;
	((volume_t *)fvol)->read_data = read_data;
	((volume_t *)fvol)->write_data = write_data;
	((volume_t *)fvol)->discard_blocks = discard_blocks;
	((volume_t *)fvol)->commit = commit;
	((volume_t *)fvol)->create_sub_volume = create_sub_volume;
	return (volume_t *)fvol;
fail:
	perror(filename);

	if (fvol != NULL) {
		free(fvol->filename);
		free(fvol);
	}
	return NULL;
}
