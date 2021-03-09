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
	uint64_t bytes_used;

	uint64_t min_block_count;
	uint64_t max_block_count;

	uint8_t scratch[];
} file_volume_t;

/*****************************************************************************/

enum {
	PROP_MIN_SIZE = 0,
	PROP_MAX_SIZE,
	PROP_COUNT,
};

static size_t get_property_count(const meta_object_t *meta)
{
	(void)meta;
	return PROP_COUNT;
}

static int get_property_desc(const meta_object_t *meta, size_t i,
			     PROPERTY_TYPE *type, const char **name)
{
	(void)meta;
	switch (i) {
	case PROP_MIN_SIZE:
		*type = PROPERTY_TYPE_U64_SIZE;
		*name = "minsize";
		break;
	case PROP_MAX_SIZE:
		*type = PROPERTY_TYPE_U64_SIZE;
		*name = "maxsize";
		break;
	default:
		return -1;
	}
	return 0;
}

static int set_property(const meta_object_t *meta, size_t i,
			object_t *obj, const property_value_t *value)
{
	volume_t *vol = (volume_t *)obj;
	file_volume_t *fvol = (file_volume_t *)vol;
	(void)meta;

	switch (i) {
	case PROP_MIN_SIZE:
		if (value->type != PROPERTY_TYPE_U64_SIZE)
			return -1;

		fvol->min_block_count = value->value.u64 / vol->blocksize;

		if (value->value.u64 % vol->blocksize)
			fvol->min_block_count += 1;

		if (fvol->min_block_count > fvol->max_block_count)
			fvol->min_block_count = fvol->max_block_count;
		break;
	case PROP_MAX_SIZE:
		if (value->type != PROPERTY_TYPE_U64_SIZE)
			return -1;

		fvol->max_block_count = value->value.u64 / vol->blocksize;
		if (fvol->max_block_count < fvol->min_block_count)
			fvol->max_block_count = fvol->min_block_count;
		break;
	default:
		return -1;
	}

	return 0;
}

static int get_property(const meta_object_t *meta, size_t i,
			const object_t *obj, property_value_t *value)
{
	const volume_t *vol = (const volume_t *)obj;
	const file_volume_t *fvol = (const file_volume_t *)vol;
	(void)meta;

	switch (i) {
	case PROP_MIN_SIZE:
		value->type = PROPERTY_TYPE_U64_SIZE;
		value->value.u64 = vol->blocksize * fvol->min_block_count;
		break;
	case PROP_MAX_SIZE:
		value->type = PROPERTY_TYPE_U64_SIZE;
		value->value.u64 = vol->blocksize * fvol->max_block_count;
		break;
	default:
		return -1;
	}

	return 0;
}

static const meta_object_t file_volume_meta = {
	.name = "file_volume_t",
	.parent = NULL,

	.get_property_count = get_property_count,
	.get_property_desc = get_property_desc,
	.set_property = set_property,
	.get_property = get_property,
};

/*****************************************************************************/

static int transfer_blocks(file_volume_t *fvol, uint64_t src, uint64_t dst,
			   size_t count)
{
	size_t size, diff, processed;
#ifdef HAVE_COPY_FILE_RANGE
	loff_t off_in, off_out;
	ssize_t ret;
#else
	off_t off_in, off_out;
#endif

	processed = 0;
	size = ((volume_t *)fvol)->blocksize * count;
	off_in = src * size;
	off_out = dst * size;

#ifdef HAVE_COPY_FILE_RANGE
	/* fast-path: ask the kernel to copy-on-write remap the region */
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
#endif
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

static int check_bounds(file_volume_t *fvol, uint64_t index,
			uint32_t offset, uint32_t size)
{
	if (index >= fvol->max_block_count)
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
#ifdef HAVE_FALLOCATE
	int ret;

	do {
		ret = fallocate(fd,
				FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
				offset, size);
	} while (ret < 0 && errno == EINTR);

	return ret;
#else
	(void)fd; (void)offset; (void)size;
	return -1;
#endif
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
	uint64_t last;

	if (check_bounds(fvol, index, offset, size))
		return -1;

	if (bitmap_set(fvol->bitmap, index))
		goto fail_flag;

	if (buffer == NULL) {
		memset(fvol->scratch, 0, size);
		buffer = fvol->scratch;
	}

	last = index * vol->blocksize + offset + size;
	if (last > fvol->bytes_used)
		fvol->bytes_used = last;

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
	uint64_t last_index, total_size;
	int ret;

	/* sanity check */
	if (index >= fvol->max_block_count)
		return 0;

	if (count > (fvol->max_block_count - index))
		count = fvol->max_block_count - index;

	if (count == 0)
		return 0;

	/* fast-path */
	if (count == (fvol->max_block_count - index)) {
		ret = truncate_file(fvol->fd, index * vol->blocksize);
	} else {
		ret = punch_hole(fvol->fd, index * vol->blocksize,
				 count * vol->blocksize);
	}

	if (ret == 0) {
		while (count--)
			bitmap_clear(fvol->bitmap, index++);

		goto out;
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

out:
	last_index = bitmap_msb_index(fvol->bitmap);

	if (last_index == 0 && !bitmap_is_set(fvol->bitmap, 0)) {
		total_size = 0;
	} else {
		total_size = (last_index + 1) * vol->blocksize;
	}

	if (total_size < fvol->bytes_used)
		fvol->bytes_used = total_size;

	return 0;
}

static int write_block(volume_t *vol, uint64_t index, const void *buffer)
{
	if (buffer == NULL)
		return discard_blocks(vol, index, 1);

	return write_partial_block(vol, index, buffer, 0, vol->blocksize);
}

static int move_block(volume_t *vol, uint64_t src, uint64_t dst)
{
	file_volume_t *fvol = (file_volume_t *)vol;
	bool src_set, dst_set;
	uint64_t size;

	if (check_bounds(fvol, src, 0, vol->blocksize))
		return -1;

	if (check_bounds(fvol, dst, 0, vol->blocksize))
		return -1;

	src_set = bitmap_is_set(fvol->bitmap, src);
	dst_set = bitmap_is_set(fvol->bitmap, dst);

	if (src == dst || (!src_set && !dst_set))
		return 0;

	if (!src_set)
		return discard_blocks(vol, dst, 1);

	if (transfer_blocks(fvol, src, dst, 1))
		return -1;

	size = (dst + 1) * vol->blocksize;
	if (size > fvol->bytes_used)
		fvol->bytes_used = size;

	if (bitmap_set(fvol->bitmap, dst))
		goto fail_flag;

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
	uint64_t maxsz;

	if (check_bounds(fvol, src, src_offset, size))
		return -1;

	if (check_bounds(fvol, dst, dst_offset, size))
		return -1;

	if (read_retry(fvol->filename, fvol->fd,
		       src * vol->blocksize + src_offset,
		       fvol->scratch, size)) {
		return -1;
	}

	maxsz = dst * vol->blocksize + dst_offset + size;
	if (maxsz > fvol->bytes_used)
		fvol->bytes_used = maxsz;

	return write_retry(fvol->filename, fvol->fd,
			   dst * vol->blocksize + dst_offset,
			   fvol->scratch, size);
}

static int commit(volume_t *vol)
{
	file_volume_t *fvol = (file_volume_t *)vol;

	if (truncate_file(fvol->fd, fvol->bytes_used) != 0) {
		perror(fvol->filename);
		return -1;
	}

	if (fsync(fvol->fd) != 0) {
		perror(fvol->filename);
		return -1;
	}

	return 0;
}

static uint64_t get_min_block_count(volume_t *vol)
{
	return ((file_volume_t *)vol)->min_block_count;
}

static uint64_t get_max_block_count(volume_t *vol)
{
	return ((file_volume_t *)vol)->max_block_count;
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

	if (S_ISBLK(sb.st_mode)) {
		blocksize = sb.st_blksize;
		if (blocksize == 0)
			blocksize = 512;

		used = sb.st_size / blocksize;
		max_count = sb.st_size / blocksize;
	} else {
		blocksize = 4096;

		used = sb.st_size / blocksize;

		if (sb.st_size % blocksize) {
			used += 1;

			if (ftruncate(fd, used * blocksize) != 0)
				goto fail;
		}

		max_count = max_size / blocksize;
		if (max_size % blocksize)
			max_count += 1;
	}

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
	fvol->bytes_used = used * blocksize;
	fvol->min_block_count = 0;
	fvol->max_block_count = max_count;
	((object_t *)fvol)->meta = &file_volume_meta;
	((object_t *)fvol)->refcount = 1;
	((object_t *)fvol)->destroy = destroy;
	((volume_t *)fvol)->blocksize = blocksize;
	((volume_t *)fvol)->get_min_block_count = get_min_block_count;
	((volume_t *)fvol)->get_max_block_count = get_max_block_count;
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
