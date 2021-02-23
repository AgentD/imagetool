/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * temp_files.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "test.h"

#include <fcntl.h>
#include <unistd.h>

typedef struct temp_file_t {
	int fd;
	char *name;
	struct temp_file_t *next;
} temp_file_t;

static temp_file_t *temp_files;

int open_temp_file(const char *name)
{
	temp_file_t *tmp = calloc(1, sizeof(*tmp));

	if (tmp == NULL) {
		perror(name);
		return -1;
	}

	tmp->name = strdup(name);
	if (tmp->name == NULL) {
		perror(name);
		free(tmp);
		return -1;
	}

	tmp->fd = open(tmp->name, O_RDWR | O_EXCL | O_CREAT, 0644);
	if (tmp->fd < 0) {
		perror(tmp->name);
		free(tmp->name);
		free(tmp);
		return -1;
	}

	tmp->next = temp_files;
	temp_files = tmp;

	return tmp->fd;
}

void cleanup_temp_files(void)
{
	temp_file_t *it;

	while (temp_files != NULL) {
		it = temp_files;
		temp_files = temp_files->next;

		close(it->fd);
		unlink(it->name);

		free(it->name);
		free(it);
	}
}
