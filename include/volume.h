/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume.h
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef VOLUME_H
#define VOLUME_H

#include "predef.h"

/*
  A "volume" represents what Unix might call a block device. It manages a chunk
  of data that is divided into uniformly sized blocks that can be read,
  overwritten, and so on, but only with block granularity.
 */
struct volume_t {
	object_t base;

	uint32_t blocksize;

	uint64_t (*get_min_block_count)(volume_t *vol);

	uint64_t (*get_max_block_count)(volume_t *vol);

	/*
	  Read a block using its block index into a buffer that must be large
	  enough to hold a single block. Returns 0 on success, -1 on failure.
	 */
	int (*read_block)(volume_t *vol, uint64_t index, void *buffer);

	/*
	  Read only part of a block using its block index and a byte offset
	  into the block. Both offset and size from there must not cross the
	  block boundary.

	  Returns 0 on success, -1 on failure.
	 */
	int (*read_partial_block)(volume_t *vol, uint64_t index,
				  void *buffer, uint32_t offset, uint32_t size);

	/*
	  Write a buffer, holding an entire block to the volume at a specific
	  block index. Returns 0 on success, -1 on failure.

	  If buffer is NULL, the function clears the block to zero, which is
	  effectively the same as using discard_blocks.
	 */
	int (*write_block)(volume_t *vol, uint64_t index, const void *buffer);

	/*
	  Overwrite only part of a block using its block index and a byte
	  offset into the block. Both offset and size from there must not
	  cross the block boundary.

	  The implementation internally splices together the existing data
	  in the block with the given data fragment.

	  If buffer is NULL, the function clears the region of the block
	  to zero.

	  Returns 0 on success, -1 on failure.
	 */
	int (*write_partial_block)(volume_t *vol, uint64_t index,
				   const void *buffer, uint32_t offset,
				   uint32_t size);

	/*
	  Move a single block within a volume. A source and destination index
	  are given.

	  Returns 0 on success, -1 on failure.
	 */
	int (*move_block)(volume_t *vol, uint64_t src, uint64_t dst);

	/*
	  Basically a memmove of a part of a block to some other block or move
	  a part around within a block.

	  Returns 0 on success, -1 on failure.
	 */
	int (*move_block_partial)(volume_t *vol, uint64_t src, uint64_t dst,
				  size_t src_offset, size_t dst_offset,
				  size_t size);

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

typedef enum {
	/*
	  Interpret the size as a minimum and allow the partition to grow,
	  displacing any partitions that comes afterwards if necessary.
	 */
	COMMON_PARTITION_FLAG_GROW = 0x01,

	/*
	  Expand the partition to fill the entire disk. Can only be set on
	  one partition.
	 */
	COMMON_PARTITION_FLAG_FILL = 0x02,
} COMMON_PARTITION_FLAGS;

typedef enum {
	MBR_PARTITION_FLAG_BOOTABLE = 0x00010000,

	MBR_PARTITION_TYPE_LINUX_SWAP = 0x08200000,
	MBR_PARTITION_TYPE_LINUX_DATA = 0x08300000,
	MBR_PARTITION_TYPE_FREEBSD_DATA = 0x0A500000,
	MBR_PARTITION_TYPE_OPENBSD_DATA = 0x0A600000,
	MBR_PARTITION_TYPE_NETBSD_DATA = 0x0A900000,
	MBR_PARTITION_TYPE_BSDI_DATA = 0x0B700000,
	MBR_PARTITION_TYPE_MINIX_DATA = 0x08100000,
	MBR_PARTITION_TYPE_UNIXWARE_DATA = 0x06300000,
} MBR_PARTITION_FLAGS;

struct partition_t {
	volume_t base;

	/*
	  Change the minimum number of blocks occupied by the partition
	  and resize it if necessary.
	 */
	int (*set_base_block_count)(partition_t *part, uint64_t size);

	uint64_t (*get_flags)(partition_t *part);

	/* Change partition flags after creation */
	int (*set_flags)(partition_t *part, uint64_t flags);
};

/*
  An object that divides an underlying volume into several partitions and
  possibly adds meta data structures describing the partitions.
 */
struct partition_mgr_t {
	object_t base;

	/*
	  Create a parition at the next available block index with the
	  given size. The flags are a combination of COMMON_PARTITION_FLAGS
	  base flags and implementation specific additiona flags.
	 */
	partition_t *(*create_parition)(partition_mgr_t *parent,
					uint64_t blk_count, uint64_t flags);

	/*
	  Update the meta data structure and call commit on the
	  underlying volume.
	 */
	int (*commit)(partition_mgr_t *mgr);
};

#ifdef __cplusplus
extern "C" {
#endif

/*
  Creates a volume that wrapps a Unix file descriptor. The filename is copied
  internally and used to print error messages. The blocksize is derived from
  the file descriptor using stat(2).
 */
volume_t *volume_from_fd(const char *filename, int fd, uint64_t max_size);

/*
  Creates a volume that internally wrapps another volume and emulates having
  a different blocksize. It can also be set to start at an arbitrary byte
  offset from the beginning of the original volume. Maximum size is derived
  from the wrapped volume.
 */
volume_t *volume_blocksize_adapter_create(volume_t *vol, uint32_t blocksize,
					  uint32_t offset);

partition_mgr_t *mbrdisk_create(volume_t *base);

/*
  Helper function that allows reading arbitrary byte sized chunks of data at
  arbitrary byte offsets from a volume. It internally calls read_partial_block
  and read_block on the volume to paste the data together.

  Returns 0 on success.
 */
int volume_read(volume_t *vol, uint64_t offset, void *data, size_t size);

/*
  Helper function that allows writing arbitrary byte sized buffers at
  arbitrary byte offsets in a volume. It internally calls write_partial_block
  and write_block on the volume to write the data.

  If data is NULL, it zero-fills from the specified region and optionally
  calls discard_blocks if applicable.

  The function also auto-detects zero blocks being and uses discard instead.

  Returns 0 on success.
 */
int volume_write(volume_t *vol, uint64_t offset, const void *data,
		 size_t size);

/*
  Helper function that implements memmove for volumes. It supports arbitrary
  byte offsets & sizes and internally uses move_block and move_block_partial
  to shuffle the data around.

  Returns 0 on success.
 */
int volume_memmove(volume_t *vol, uint64_t dst, uint64_t src, uint64_t size);

/*
  Create an ostream_t instance that writes to a specific region of an
  underlying volume. The given offset and size are in bytes, the name
  is copied internally and used for error reporting.
 */
ostream_t *volume_ostream_create(volume_t *vol, const char *name,
				 uint64_t offset, uint64_t max_size);

#ifdef __cplusplus
}
#endif

#endif /* VOLUME_H */
