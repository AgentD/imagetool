/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_retry.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util.h"

#include <unistd.h>
#include <errno.h>
#include <stdio.h>

int write_retry(const char *filename, int fd, uint64_t offset,
		const void *data, size_t size)
{
	while (size > 0) {
		ssize_t ret = pwrite(fd, data, size, offset);

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			perror(filename);
			return -1;
		}

		if (ret == 0) {
			fprintf(stderr, "%s: truncated write\n", filename);
			return -1;
		}

		data = (const char *)data + ret;
		size -= ret;
		offset += ret;
	}

	return 0;
}
