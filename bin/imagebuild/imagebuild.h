/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * imagebuild.h
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IMAGE_BUILD_H
#define IMAGE_BUILD_H

#include "config.h"

#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>

typedef struct {
	const char *layout_path;
	const char *output_path;
} options_t;

extern const char *__progname;

void print_version(void);

void process_options(options_t *opt, int argc, char **argv);

#endif /* IMAGE_BUILD_H */