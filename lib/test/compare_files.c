/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compare_files.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "test.h"
#include "util.h"

#include <sys/stat.h>

int compare_files_equal(int gen_fd, int ref_fd)
{
	char abuf[1024], bbuf[1024];
	struct stat sb1, sb2;
	off_t size;

	if (fstat(gen_fd, &sb1) != 0) {
		perror("stat on generated file");
		return -1;
	}

	if (fstat(ref_fd, &sb2) != 0) {
		perror("stat on reference file");
		return -1;
	}

	if (sb1.st_size != sb2.st_size) {
		fprintf(stderr, "Reference file and generated file "
			"differ in size (%lu vs %lu)\n",
			(unsigned long)sb2.st_size,
			(unsigned long)sb1.st_size);
		return -1;
	}

	size = sb1.st_size;

	while (size > 0) {
		size_t diff = size > 1024 ? 1024 : size;

		if (read_retry("generated file", gen_fd, 0, abuf, diff))
			return -1;

		if (read_retry("reference file", ref_fd, 0, bbuf, diff))
			return -1;

		if (memcmp(abuf, bbuf, sizeof(abuf)) != 0) {
			fputs("file not equal to reference!\n", stderr);
			return -1;
		}

		size -= diff;
	}

	return 0;
}
