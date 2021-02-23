/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tarfs.c
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
	tree_node_t *n;
	volume_t *vol;

	/* create a memory mapped, temporary file */
	fd = open_temp_file("test_tarfs.tar");
	TEST_ASSERT(fd > 0);

	vol = volume_from_fd("test_tarfs.tar", fd, 131072);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 1);

	/* create the actual tar filesystem */
	fs = filesystem_tar_create(vol);
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
				 "/var/run/whatever");
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

	ret = compare_files_equal(fd, reffd);
	TEST_EQUAL_I(ret, 0);
	close(reffd);

	/* complete cleanup */
	object_drop(vol);
	cleanup_temp_files();
	return EXIT_SUCCESS;
}
