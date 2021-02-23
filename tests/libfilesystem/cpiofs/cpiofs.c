/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * cpiofs.c
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

static char abuf[5244], bbuf[5244];

static int compare_results(int afd, int bfd)
{
	if (read_retry("generated file", afd, 0, abuf, sizeof(abuf)))
		return -1;

	if (read_retry("reference file", bfd, 0, bbuf, sizeof(bbuf)))
		return -1;

	if (memcmp(abuf, bbuf, sizeof(abuf)) != 0) {
		fputs("tar file not equal to reference!\n", stderr);
		return -1;
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
	fd = memfd_create("test.cpio", 0);
	TEST_ASSERT(fd > 0);

	vol = volume_from_fd("test.cpio", fd, 131072);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 1);

	/* create the actual filesystem */
	fs = filesystem_cpio_create(vol);
	TEST_NOT_NULL(fs);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 1);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 2);

	/* add some stuff */
	n = fstree_add_directory(fs->fstree, "/dev");
	TEST_NOT_NULL(n);

	n = fstree_add_character_device(fs->fstree, "/dev/console", 42);
	TEST_NOT_NULL(n);

	n = fstree_add_block_device(fs->fstree, "/dev/sda42", 1337);
	TEST_NOT_NULL(n);

	n = fstree_add_directory(fs->fstree, "/usr/bin");
	TEST_NOT_NULL(n);

	n = fstree_add_directory(fs->fstree, "/usr/lib");
	TEST_NOT_NULL(n);

	n = fstree_add_symlink(fs->fstree, "/bin", "/usr/bin");
	TEST_NOT_NULL(n);

	n = fstree_add_symlink(fs->fstree, "/lib", "/usr/lib");
	TEST_NOT_NULL(n);

	n = fstree_add_fifo(fs->fstree, "/var/run/whatever");
	TEST_NOT_NULL(n);

	n = fstree_add_hard_link(fs->fstree, "/var/run/foo",
				 "/etc/empty.cfg");
	TEST_NOT_NULL(n);

	n = fstree_add_hard_link(fs->fstree, "/var/run/link.txt",
				 "/home/hello.txt");
	TEST_NOT_NULL(n);

	n = fstree_add_file(fs->fstree, "/home/hello.txt");
	TEST_NOT_NULL(n);
	ret = fstree_file_append(fs->fstree, n, "Hello, world!\n", 14);
	TEST_EQUAL_I(ret, 0);

	n = fstree_add_file(fs->fstree, "/tmp/bye.txt");
	TEST_NOT_NULL(n);
	ret = fstree_file_append(fs->fstree, n, "再见!\n", 6 + 2);
	TEST_EQUAL_I(ret, 0);

	n = fstree_add_file(fs->fstree, "/etc/empty.cfg");
	TEST_NOT_NULL(n);

	n = fstree_add_file(fs->fstree, "/tmp/sparse.bin");
	TEST_NOT_NULL(n);
	ret = fstree_file_append(fs->fstree, n, NULL, 2048);
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
