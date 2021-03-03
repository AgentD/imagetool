/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mbrdisk.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "test.h"
#include "volume.h"

#include <unistd.h>
#include <fcntl.h>

int main(void)
{
	volume_t *vol, *p0, *p1, *p2, *p3;
	partition_mgr_t *mbr;
	int ret, fd, reffd;
	char sector[512];

	/* setup temp file volume with mbr disk on top */
	fd = open_temp_file("mbrdisk1.bin");
	TEST_ASSERT(fd >= 0);

	vol = volume_from_fd("mbrdisk1.bin", fd, 20 * 1024 * 1024);
	TEST_NOT_NULL(vol);

	TEST_EQUAL_UI(((object_t *)vol)->refcount, 1);

	mbr = mbrdisk_create(vol);
	TEST_NOT_NULL(mbr);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 2);
	TEST_EQUAL_UI(((object_t *)mbr)->refcount, 1);

	/* create a few partitions */
	p0 = mbr->create_parition(mbr, 5, COMMON_PARTION_FLAG_GROW);
	TEST_NOT_NULL(p0);
	TEST_EQUAL_UI(((object_t *)mbr)->refcount, 2);
	TEST_EQUAL_UI(((object_t *)p0)->refcount, 1);

	p1 = mbr->create_parition(mbr, 10, 0);
	TEST_NOT_NULL(p1);
	TEST_EQUAL_UI(((object_t *)mbr)->refcount, 3);
	TEST_EQUAL_UI(((object_t *)p1)->refcount, 1);

	p2 = mbr->create_parition(mbr, 42, 0);
	TEST_NOT_NULL(p2);
	TEST_EQUAL_UI(((object_t *)mbr)->refcount, 4);
	TEST_EQUAL_UI(((object_t *)p2)->refcount, 1);

	p3 = mbr->create_parition(mbr, 3072, 0);
	TEST_NOT_NULL(p3);
	TEST_EQUAL_UI(((object_t *)mbr)->refcount, 5);
	TEST_EQUAL_UI(((object_t *)p3)->refcount, 1);

	TEST_EQUAL_UI(p0->blocksize, 512);
	TEST_EQUAL_UI(p1->blocksize, 512);
	TEST_EQUAL_UI(p2->blocksize, 512);
	TEST_EQUAL_UI(p3->blocksize, 512);

	TEST_EQUAL_UI(p0->min_block_count, 2048);
	TEST_EQUAL_UI(p1->min_block_count, 2048);
	TEST_EQUAL_UI(p2->min_block_count, 2048);
	TEST_EQUAL_UI(p3->min_block_count, 4096);

	/* write to the partitions */
	memset(sector, 0, sizeof(sector));
	strcpy(sector, "Hello, World!");
	ret = p0->write_block(p0, 0, sector);
	TEST_EQUAL_I(ret, 0);

	memset(sector, 0, sizeof(sector));
	strcpy(sector, "A different string");
	ret = p1->write_block(p1, 0, sector);
	TEST_EQUAL_I(ret, 0);

	ret = p0->read_block(p0, 0, sector);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(sector, "Hello, World!", 13);
	TEST_EQUAL_I(ret, 0);

	ret = p1->read_block(p1, 0, sector);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(sector, "A different string", 18);
	TEST_EQUAL_I(ret, 0);

	ret = vol->read_partial_block(vol,
				      (1024 * 1024) / vol->blocksize, sector,
				      (1024 * 1024) % vol->blocksize, 512);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(sector, "Hello, World!", 13);
	TEST_EQUAL_I(ret, 0);

	ret = vol->read_partial_block(vol,
				      (2 * 1024 * 1024) / vol->blocksize, sector,
				      (2 * 1024 * 1024) % vol->blocksize, 512);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(sector, "A different string", 18);
	TEST_EQUAL_I(ret, 0);

	/* do a write that causes the first partition to grow */
	memset(sector, 0, sizeof(sector));
	strcpy(sector, "Foo");
	ret = p0->write_block(p0, 2048, sector);
	TEST_EQUAL_I(ret, 0);

	ret = p0->read_block(p0, 0, sector);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(sector, "Hello, World!", 13);
	TEST_EQUAL_I(ret, 0);

	ret = p0->read_block(p0, 2048, sector);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(sector, "Foo", 3);
	TEST_EQUAL_I(ret, 0);

	ret = p1->read_block(p1, 0, sector);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(sector, "A different string", 18);
	TEST_EQUAL_I(ret, 0);

	ret = vol->read_partial_block(vol,
				      (1024 * 1024) / vol->blocksize, sector,
				      (1024 * 1024) % vol->blocksize, 512);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(sector, "Hello, World!", 13);
	TEST_EQUAL_I(ret, 0);

	ret = vol->read_partial_block(vol,
				      (2 * 1024 * 1024) / vol->blocksize, sector,
				      (2 * 1024 * 1024) % vol->blocksize, 512);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(sector, "Foo", 3);
	TEST_EQUAL_I(ret, 0);

	ret = vol->read_partial_block(vol,
				      (3 * 1024 * 1024) / vol->blocksize, sector,
				      (3 * 1024 * 1024) % vol->blocksize, 512);
	TEST_EQUAL_I(ret, 0);
	ret = memcmp(sector, "A different string", 18);
	TEST_EQUAL_I(ret, 0);

	/* commit */
	ret = mbr->commit(mbr);
	TEST_EQUAL_I(ret, 0);

	reffd = open(TEST_PATH, O_RDONLY);
	TEST_ASSERT(reffd >= 0);

	ret = compare_files_equal(fd, reffd);
	TEST_EQUAL_I(ret, 0);

	close(reffd);

	/* cleanup */
	object_drop(mbr);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 2);

	object_drop(p0);
	object_drop(p1);
	object_drop(p2);
	object_drop(p3);
	TEST_EQUAL_UI(((object_t *)vol)->refcount, 1);

	object_drop(vol);
	cleanup_temp_files();
	return EXIT_SUCCESS;
}
