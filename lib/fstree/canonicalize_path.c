/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * canonicalize_path.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

void fstree_canonicalize_path(char *path)
{
	char *dst, *src;

	for (src = path; *src != '\0'; ++src) {
		if (*src == '\\')
			*src = '/';
	}

	src = path;
	dst = path;

	while (*src == '/')
		++src;

	while (*src != '\0') {
		if (*src == '/') {
			while (*src == '/')
				++src;

			if (*src == '\0')
				break;

			*(dst++) = '/';
		} else {
			*(dst++) = *(src++);
		}
	}

	*dst = '\0';
}
