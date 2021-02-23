/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fat32_empty.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "test.h"
#include "filesystem.h"
#include "util.h"

#include <sys/mman.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define REF_FILE STRVALUE(TESTFILE)

int main(void)
{
	int fd, reffd, ret;
	filesystem_t *fs;
	volume_t *vol;

	/* create a memory mapped, temporary file */
	fd = memfd_create("fat32_empty.bin", 0);
	TEST_ASSERT(fd > 0);

	vol = volume_from_fd("fat32_empty.bin", fd,
			     10UL * 1024UL * 1024UL * 1024UL);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 1);

	/* create the actual tar filesystem */
	fs = filesystem_fatfs_create(vol);
	TEST_NOT_NULL(fs);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 1);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 3);

	/* generate the filesystem */
	ret = fs->build_format(fs);
	TEST_EQUAL_I(ret, 0);

	fs->fstree->volume->commit(fs->fstree->volume);

	/* cleanup the filesystem & flush the underlying volume */
	object_drop(fs);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 1);

	vol->commit(vol);

	/* compare with the reference */
	reffd = open(TEST_PATH "/" REF_FILE, O_RDONLY);
	if (reffd < 0) {
		perror(TEST_PATH "/" REF_FILE);
		abort();
	}

	ret = compare_files_equal(fd, reffd);
	TEST_EQUAL_I(ret, 0);
	close(reffd);

	/* complete cleanup */
	object_drop(vol);
	return EXIT_SUCCESS;
}
