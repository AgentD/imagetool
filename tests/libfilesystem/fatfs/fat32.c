/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fat32.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "../../test.h"
#include "filesystem.h"
#include "util.h"

#include <sys/mman.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define REF_FILE STRVALUE(TESTFILE)

#define NUM_EXPECTED_BLOCKS (20536)

static int compare_results(int afd, int bfd)
{
	char abuf[1024], bbuf[1024];
	int i;

	for (i = 0; i < NUM_EXPECTED_BLOCKS; ++i) {
		if (read_retry("generated file", afd, i * 1024, abuf, 1024))
			return -1;

		if (read_retry("reference file", bfd, i * 1024, bbuf, 1024))
			return -1;

		if (memcmp(abuf, bbuf, 1024) != 0) {
			fputs("tar file not equal to reference!\n", stderr);
			return -1;
		}
	}

	return 0;
}

int main(void)
{
	int fd, reffd, ret;
	filesystem_t *fs;
	tree_node_t *n;
	volume_t *vol;

	/* create a memory mapped, temporary file */
	fd = memfd_create("fat32_test.bin", 0);
	TEST_ASSERT(fd > 0);

	vol = volume_from_fd("fat32_test.bin", fd,
			     10UL * 1024UL * 1024UL * 1024UL);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 1);

	/* create the actual tar filesystem */
	fs = filesystem_fatfs_create(vol);
	TEST_NOT_NULL(fs);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 1);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 3);

	/* add some stuff */
	n = fstree_add_directory(fs->fstree, "/dev");
	TEST_NOT_NULL(n);

	n = fstree_add_directory(fs->fstree, "/usr/bin");
	TEST_NOT_NULL(n);

	n = fstree_add_directory(fs->fstree, "/usr/lib");
	TEST_NOT_NULL(n);

	n = fstree_add_file(fs->fstree, "/home/hello.txt");
	TEST_NOT_NULL(n);
	ret = fstree_file_append(fs->fstree, n, "Hello, world!\n", 14);
	TEST_EQUAL_I(ret, 0);
	n->ctime = 1057318662;
	n->mtime = 1175378400;

	n = fstree_add_file(fs->fstree,
			    "/home/really-long-filename-for-dos.txt");
	TEST_NOT_NULL(n);

	n = fstree_add_file(fs->fstree, "/tmp/bye.txt");
	TEST_NOT_NULL(n);
	ret = fstree_file_append(fs->fstree, n, "再见!\n", 6 + 2);
	TEST_EQUAL_I(ret, 0);

	n = fstree_add_file(fs->fstree, "/etc/empty.cfg");
	TEST_NOT_NULL(n);

	n = fstree_add_file(fs->fstree, "/tmp/sparse.bin");
	TEST_NOT_NULL(n);
	ret = fstree_file_append(fs->fstree, n, NULL, 16384);
	TEST_EQUAL_I(ret, 0);

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

	ret = compare_results(fd, reffd);
	TEST_EQUAL_I(ret, 0);
	close(reffd);

	/* complete cleanup */
	object_drop(vol);
	return EXIT_SUCCESS;
}