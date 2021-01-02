/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * canonicalize_path.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

static void normalize_slashes(char *filename)
{
	char *dst = filename, *src = filename;

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

void fstree_canonicalize_path(char *path)
{
	char *dst = path, *src = path;

	normalize_slashes(path);

	while (*src != '\0') {
		if (src[0] == '.') {
			if (src[1] == '\0')
				break;
			if (src[1] == '/') {
				src += 2;
				continue;
			}
		}

		while (*src != '\0' && *src != '/')
			*(dst++) = *(src++);

		if (*src == '/')
			*(dst++) = *(src++);
	}

	*dst = '\0';
	normalize_slashes(path);
}
