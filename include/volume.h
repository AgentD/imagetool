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

struct volume_t {
	object_t base;

	uint32_t blocksize;

	uint64_t min_block_count;
	uint64_t max_block_count;

	int (*read_block)(volume_t *vol, uint64_t index, void *buffer);

	int (*write_block)(volume_t *vol, uint64_t index, const void *buffer);

	int (*move_block)(volume_t *vol, uint64_t src, uint64_t dst, int flags);

	int (*discard_blocks)(volume_t *vol, uint64_t index, uint64_t count);

	int (*commit)(volume_t *vol);

	volume_t *(*create_sub_volume)(volume_t *vol, uint64_t min_count,
				       uint64_t max_count);
};

#ifdef __cplusplus
extern "C" {
#endif

int volume_move_blocks(volume_t *vol, uint64_t src, uint64_t dst,
		       uint64_t count, int flags);

volume_t *volume_from_fd(const char *filename, int fd, uint64_t max_size);

volume_t *volume_blocksize_adapter_create(volume_t *vol, uint32_t blocksize);

#ifdef __cplusplus
}
#endif

#endif /* VOLUME_H */
