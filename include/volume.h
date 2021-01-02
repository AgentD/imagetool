/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume.h
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef VOLUME_H
#define VOLUME_H

#include "predef.h"
#include "fstream.h"

typedef struct volume_t volume_t;

enum {
	MOVE_SWAP = 0x01,
	MOVE_ERASE_SOURCE = 0x02,
};

/*
  A "volume" represents what Unix might call a block device. It manages a chunk
  of data that is divided into uniformly sized blocks that can be read,
  overwritten, and so on, but only with block granularity.

  A volume can have sub volumes that have a part of the space assigned. For
  instance, a GPT or MBR formated disk may be represented as a volume that on
  its own doesn't allow direct read/write access, but can create sub volumes
  representing partitions.
 */
struct volume_t {
	object_t base;

	uint32_t blocksize;

	uint64_t min_block_count;
	uint64_t max_block_count;

	/*
	  Read a block using its block index into a buffer that must be large
	  enough to hold a single block. Returns 0 on success, -1 on failure.
	 */
	int (*read_block)(volume_t *vol, uint64_t index, void *buffer);

	/*
	  Write a buffer, holding an entire block to the volume at a specific
	  block index. Returns 0 on success, -1 on failure.
	 */
	int (*write_block)(volume_t *vol, uint64_t index, const void *buffer);

	/*
	  Move a single within a volume. A source and destination index are
	  given. If the MOVE_SWAP flag is set, the blocks are swapped. If the
	  MOVE_ERASE_SOURCE flag is set, the source block is discarded after
	  moving it to the destination.

	  Returns 0 on success, -1 on failure.
	 */
	int (*move_block)(volume_t *vol, uint64_t src, uint64_t dst, int flags);

	/*
	  Mark a range of blocks as discarded. Any future reads will return
	  zero. The underlying implementation may perfrom certain optimzations,
	  knowing that the blocks are considered erased.

	  Returns 0 on success, -1 on failure.
	 */
	int (*discard_blocks)(volume_t *vol, uint64_t index, uint64_t count);

	/*
	  Flush any cached data to disk. Returns 0 on success, -1 on failure.
	 */
	int (*commit)(volume_t *vol);
};

#ifdef __cplusplus
extern "C" {
#endif

int volume_move_blocks(volume_t *vol, uint64_t src, uint64_t dst,
		       uint64_t count, int flags);

volume_t *volume_from_fd(const char *filename, int fd, uint64_t max_size);

volume_t *volume_blocksize_adapter_create(volume_t *vol, uint32_t blocksize,
					  uint32_t offset);

#ifdef __cplusplus
}
#endif

#endif /* VOLUME_H */
