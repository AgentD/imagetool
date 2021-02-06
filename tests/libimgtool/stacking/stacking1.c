/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * stacking.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../../test.h"

#include "fsdeptracker.h"
#include "filesystem.h"
#include "filesink.h"
#include "volume.h"
#include "util.h"

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#define NUM_EXPECTED_BLOCKS (14)

static const char *listing[] = {
	"dir /etc 0755 0 0",
	"slink /bin 0777 0 0 /usr/bin",
	"slink /lib 0777 0 0 /usr/lib",
	"dir /dev 0755 0 0",
	"nod /dev/console 0600 6 7 c 13 37",
	"nod /dev/blkdev0 0600 8 9 b 42 21",
	"dir /home 0755 0 0",
	"dir /home/goliath 0755 1000 100",
	"dir /home/foobar 0755 1001 100",
	"dir /usr 0755 0 0",
	"dir /usr/bin 0755 0 0",
	"dir /usr/lib 0755 0 0",
	"slink /usr/lib64 0755 0 0 lib",
};

static int compare_results(int afd, int bfd)
{
	char abuf[512], bbuf[512];
	int i;

	for (i = 0; i < NUM_EXPECTED_BLOCKS; ++i) {
		if (read_retry("generated file", afd, i * 512, abuf, 512))
			return -1;

		if (read_retry("reference file", bfd, i * 512, bbuf, 512))
			return -1;

		if (memcmp(abuf, bbuf, 512) != 0) {
			fputs("tar file not equal to reference!\n", stderr);
			return -1;
		}
	}

	return 0;
}

int main(void)
{
	file_source_listing_t *lst;
	fs_dep_tracker_t *tracker;
	filesystem_t *fs, *fs1;
	int fd, reffd, ret;
	tree_node_t *node;
	file_sink_t *sink;
	volume_t *vol;
	size_t i;

	/* create a dependency tracker */
	tracker = fs_dep_tracker_create();
	TEST_NOT_NULL(tracker);
	TEST_EQUAL_UI(((object_t *)tracker)->refcount, 1);

	/* create a file sink */
	sink = file_sink_create();
	TEST_NOT_NULL(sink);
	TEST_EQUAL_UI(((object_t *)sink)->refcount, 1);

	/* create a memory mapped, temporary file */
	fd = memfd_create("test.tar", 0);
	TEST_ASSERT(fd > 0);

	vol = volume_from_fd("test.tar", fd, 131072);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 1);

	ret = fs_dep_tracker_add_volume(tracker, vol, NULL);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 2);

	/* create a tar filesystem on the output volume */
	fs = filesystem_tar_create(vol);
	TEST_NOT_NULL(fs);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 1);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 3);

	ret = fs_dep_tracker_add_fs(tracker, fs, vol, "tarball");
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 2);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 3);

	vol = object_drop(vol);

	/* create a file with a nested filesystem on top of the tarball */
	node = fstree_add_file(fs->fstree, "/usr.cpio");
	TEST_NOT_NULL(node);

	vol = fstree_file_volume_create(fs->fstree, node, 512, 0, 131072);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(((object_t *)fs->fstree)->refcount, 2);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 1);

	ret = fs_dep_tracker_add_volume_file(tracker, vol, fs);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 2);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 2);

	fs1 = filesystem_cpio_create(vol);
	TEST_NOT_NULL(fs1);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 2);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 3);
	TEST_EQUAL_UI(((object_t *)fs1)->refcount, 1);

	ret = fs_dep_tracker_add_fs(tracker, fs1, vol, "cpiofs");
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)fs1)->refcount, 2);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 3);

	ret = file_sink_bind(sink, "/usr", fs1);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)fs1)->refcount, 3);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 3);

	fs1 = object_drop(fs1);
	vol = object_drop(vol);

	/* create a second file with a nested filesystem on top of the tarball */
	node = fstree_add_file(fs->fstree, "/home.tar");
	TEST_NOT_NULL(node);

	vol = fstree_file_volume_create(fs->fstree, node, 512, 0, 131072);
	TEST_NOT_NULL(vol);
	TEST_EQUAL_UI(((object_t *)fs->fstree)->refcount, 3);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 1);

	ret = fs_dep_tracker_add_volume_file(tracker, vol, fs);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 2);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 2);

	fs1 = filesystem_tar_create(vol);
	TEST_NOT_NULL(fs1);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 2);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 3);
	TEST_EQUAL_UI(((object_t *)fs1)->refcount, 1);

	ret = fs_dep_tracker_add_fs(tracker, fs1, vol, "tarfs");
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)fs1)->refcount, 2);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 3);

	ret = file_sink_bind(sink, "/home", fs1);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)fs1)->refcount, 3);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 3);

	fs1 = object_drop(fs1);
	vol = object_drop(vol);

	/* bind the main filesystem */
	ret = file_sink_bind(sink, "/", fs);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(((object_t *)fs)->refcount, 3);
	fs = object_drop(fs);

	/* add a source listing */
	lst = file_source_listing_create(".");
	TEST_NOT_NULL(lst);
	TEST_EQUAL_UI(((object_t *)lst)->refcount, 1);

	for (i = 0; i < sizeof(listing) / sizeof(listing[0]); ++i) {
		ret = file_source_listing_add_line(lst, listing[i], NULL);
		TEST_EQUAL_I(ret, 0);
	}

	ret = file_sink_add_data(sink, (file_source_t *)lst);
	TEST_EQUAL_I(ret, 0);
	lst = object_drop(lst);

	/* build the filesystems and flush the underlying volumes */
	ret = fs_dep_tracker_commit(tracker);
	TEST_EQUAL_I(ret, 0);

	/* compare with the reference */
	reffd = open(TEST_PATH, O_RDONLY);
	if (reffd < 0) {
		perror(TEST_PATH);
		abort();
	}

	ret = compare_results(fd, reffd);
	TEST_EQUAL_I(ret, 0);
	close(reffd);

	/* cleanup */
	object_drop(sink);
	object_drop(tracker);
	return EXIT_SUCCESS;
}
