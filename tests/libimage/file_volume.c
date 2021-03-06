/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file_volume.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "test.h"
#include "volume.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
	void *block_buffer, *buffer, *ptr;
	size_t i, j, blocksz;
	struct stat sb;
	volume_t *vol;
	int fd, ret;

	/* create a memory mapped, temporary file that has 16 blocks,
	   each filled with its own block number in every byte */
	blocksz = sysconf(_SC_PAGESIZE);
	printf("Block size is %zu.\n", blocksz);

	fd = open_temp_file("testfile");
	TEST_ASSERT(fd > 0);

	TEST_EQUAL_I(ftruncate(fd, blocksz * 16), 0);

	buffer = mmap(NULL, blocksz * 16, PROT_READ | PROT_WRITE,
		      MAP_SHARED, fd, 0);
	TEST_ASSERT(buffer != MAP_FAILED);

	for (i = 0; i < 16; ++i) {
		memset((char *)buffer + i * blocksz, i, blocksz);
	}

	/* a buffer for reading/writing blocks */
	block_buffer = malloc(blocksz);
	TEST_NOT_NULL(block_buffer);

	/* create a volume using that fd that can grow to 32 blocks at most */
	vol = volume_from_fd("testfile", fd, blocksz * 32);
	TEST_NOT_NULL(vol);

	TEST_EQUAL_UI(vol->blocksize, blocksz);
	TEST_EQUAL_UI(vol->get_min_block_count(vol), 0);
	TEST_EQUAL_UI(vol->get_max_block_count(vol), 32);
	TEST_EQUAL_UI(vol->get_block_count(vol), 16);

	/* read blocks */
	for (i = 0; i < 16; ++i) {
		ret = vol->read_block(vol, i, block_buffer);
		TEST_EQUAL_I(ret, 0);

		for (j = 0; j < blocksz; ++j) {
			TEST_EQUAL_UI(((char *)block_buffer)[j], i);
		}
	}

	for (i = 16; i < 32; ++i) {
		ret = vol->read_block(vol, i, block_buffer);
		TEST_EQUAL_I(ret, 0);

		for (j = 0; j < blocksz; ++j) {
			TEST_EQUAL_UI(((char *)block_buffer)[j], 0);
		}
	}

	ret = vol->read_block(vol, 33, block_buffer);
	TEST_ASSERT(ret != 0);

	/* overwrite existing blocks */
	memset(block_buffer, 0xAA, blocksz);
	ret = vol->write_block(vol, 3, block_buffer);
	TEST_EQUAL_I(ret, 0);

	memset(block_buffer, 0x55, blocksz);
	ret = vol->write_block(vol, 7, block_buffer);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	for (i = 0; i < 16; ++i) {
		ptr = (char *)buffer + i * blocksz;

		if (i == 3) {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], 0xAA);
			}
		} else if (i == 7) {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], 0x55);
			}
		} else {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], i);
			}
		}

		ret = vol->read_block(vol, i, block_buffer);
		TEST_EQUAL_I(ret, 0);

		ret = memcmp(ptr, block_buffer, blocksz);
		TEST_EQUAL_I(ret, 0);
	}

	for (; i < 32; ++i) {
		ret = vol->read_block(vol, i, block_buffer);
		TEST_EQUAL_I(ret, 0);

		for (j = 0; j < blocksz; ++j) {
			TEST_EQUAL_UI(((char *)block_buffer)[j], 0);
		}
	}

	/* extend the file */
	memset(block_buffer, 0xFF, blocksz);
	ret = vol->write_block(vol, 24, block_buffer);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	ret = fstat(fd, &sb);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(sb.st_size, 25 * blocksz);

	TEST_EQUAL_UI(vol->get_min_block_count(vol), 0);
	TEST_EQUAL_UI(vol->get_max_block_count(vol), 32);
	TEST_EQUAL_UI(vol->get_block_count(vol), 25);

	munmap(buffer, blocksz * 16);
	buffer = mmap(NULL, blocksz * 25, PROT_READ | PROT_WRITE,
		      MAP_SHARED, fd, 0);
	TEST_ASSERT(buffer != MAP_FAILED);

	for (i = 0; i < 25; ++i) {
		ptr = (char *)buffer + i * blocksz;

		if (i == 3) {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], 0xAA);
			}
		} else if (i == 7) {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], 0x55);
			}
		} else if (i < 16) {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], i);
			}
		} else if (i == 24) {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], 0xFF);
			}
		} else {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], 0);
			}
		}

		ret = vol->read_block(vol, i, block_buffer);
		TEST_EQUAL_I(ret, 0);

		ret = memcmp(ptr, block_buffer, blocksz);
		TEST_EQUAL_I(ret, 0);
	}

	for (; i < 32; ++i) {
		ret = vol->read_block(vol, i, block_buffer);
		TEST_EQUAL_I(ret, 0);

		for (j = 0; j < blocksz; ++j) {
			TEST_EQUAL_UI(((char *)block_buffer)[j], 0);
		}
	}

	ret = vol->write_block(vol, 33, block_buffer);
	TEST_ASSERT(ret != 0);

	TEST_EQUAL_UI(vol->get_min_block_count(vol), 0);
	TEST_EQUAL_UI(vol->get_max_block_count(vol), 32);
	TEST_EQUAL_UI(vol->get_block_count(vol), 25);

	/* discard (blow a few holes in the file) */
	ret = vol->discard_blocks(vol, 3, 5);
	TEST_EQUAL_I(ret, 0);

	ret = vol->discard_blocks(vol, 15, 1);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	for (i = 0; i < 25; ++i) {
		ptr = (char *)buffer + i * blocksz;

		if ((i >= 3 && i <= 7) || (i == 15)) {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], 0);
			}
		} else if (i < 16) {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], i);
			}
		} else if (i == 24) {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], 0xFF);
			}
		} else {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], 0);
			}
		}

		ret = vol->read_block(vol, i, block_buffer);
		TEST_EQUAL_I(ret, 0);

		ret = memcmp(ptr, block_buffer, blocksz);
		TEST_EQUAL_I(ret, 0);
	}

	for (; i < 32; ++i) {
		ret = vol->read_block(vol, i, block_buffer);
		TEST_EQUAL_I(ret, 0);

		for (j = 0; j < blocksz; ++j) {
			TEST_EQUAL_UI(((char *)block_buffer)[j], 0);
		}
	}

	TEST_EQUAL_UI(vol->get_min_block_count(vol), 0);
	TEST_EQUAL_UI(vol->get_max_block_count(vol), 32);
	TEST_EQUAL_UI(vol->get_block_count(vol), 25);

	/* discard the last block, causing the file to shrink */
	ret = vol->discard_blocks(vol, 24, 1);
	TEST_EQUAL_I(ret, 0);

	ret = vol->commit(vol);
	TEST_EQUAL_I(ret, 0);

	ret = fstat(fd, &sb);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(sb.st_size, 15 * blocksz);

	TEST_EQUAL_UI(vol->get_min_block_count(vol), 0);
	TEST_EQUAL_UI(vol->get_max_block_count(vol), 32);
	TEST_EQUAL_UI(vol->get_block_count(vol), 15);

	for (i = 0; i < 15; ++i) {
		ptr = (char *)buffer + i * blocksz;

		if (i >= 3 && i <= 7) {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], 0);
			}
		} else {
			for (j = 0; j < blocksz; ++j) {
				TEST_EQUAL_UI(((uint8_t *)ptr)[j], i);
			}
		}

		ret = vol->read_block(vol, i, block_buffer);
		TEST_EQUAL_I(ret, 0);

		ret = memcmp(ptr, block_buffer, blocksz);
		TEST_EQUAL_I(ret, 0);
	}

	for (; i < 32; ++i) {
		ret = vol->read_block(vol, i, block_buffer);
		TEST_EQUAL_I(ret, 0);

		for (j = 0; j < blocksz; ++j) {
			TEST_EQUAL_UI(((char *)block_buffer)[j], 0);
		}
	}

	/* truncate to shrink */
	munmap(buffer, blocksz * 25);

	memset(block_buffer, 0xAA, blocksz);
	ret = vol->write_block(vol, 0, block_buffer);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(vol->get_min_block_count(vol), 0);
	TEST_EQUAL_UI(vol->get_max_block_count(vol), 32);
	TEST_EQUAL_UI(vol->get_block_count(vol), 15);

	ret = vol->truncate(vol, 1337);
	TEST_EQUAL_I(ret, 0);

	ret = fstat(fd, &sb);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(sb.st_size, 1337);

	TEST_EQUAL_UI(vol->get_min_block_count(vol), 0);
	TEST_EQUAL_UI(vol->get_max_block_count(vol), 32);
	TEST_EQUAL_UI(vol->get_block_count(vol), 1);

	ret = vol->read_block(vol, 0, block_buffer);
	TEST_EQUAL_I(ret, 0);

	for (i = 0; i < 1337; ++i) {
		TEST_EQUAL_UI(((uint8_t *)block_buffer)[i], 0xAA);
	}

	for (; i < blocksz; ++i) {
		TEST_EQUAL_UI(((uint8_t *)block_buffer)[i], 0);
	}

	/* truncate to grow */
	ret = vol->truncate(vol, blocksz * 2);
	TEST_EQUAL_I(ret, 0);

	ret = fstat(fd, &sb);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(sb.st_size, blocksz * 2);

	TEST_EQUAL_UI(vol->get_min_block_count(vol), 0);
	TEST_EQUAL_UI(vol->get_max_block_count(vol), 32);
	TEST_EQUAL_UI(vol->get_block_count(vol), 2);

	ret = vol->read_block(vol, 0, block_buffer);
	TEST_EQUAL_I(ret, 0);

	for (i = 0; i < 1337; ++i) {
		TEST_EQUAL_UI(((uint8_t *)block_buffer)[i], 0xAA);
	}

	for (; i < blocksz; ++i) {
		TEST_EQUAL_UI(((uint8_t *)block_buffer)[i], 0);
	}

	ret = vol->read_block(vol, 1, block_buffer);
	TEST_EQUAL_I(ret, 0);

	for (i = 0; i < blocksz; ++i) {
		TEST_EQUAL_UI(((uint8_t *)block_buffer)[i], 0);
	}

	/* cleanup */
	object_drop(vol);
	free(block_buffer);
	cleanup_temp_files();
	return EXIT_SUCCESS;
}
