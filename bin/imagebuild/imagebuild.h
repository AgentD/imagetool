/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * imagebuild.h
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IMAGE_BUILD_H
#define IMAGE_BUILD_H

#include "config.h"

#include "libimgtool.h"
#include "filesystem.h"
#include "fstree.h"
#include "volume.h"
#include "gcfg.h"

#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

typedef struct {
	const char *config_path;
	const char *output_path;
} options_t;

extern const char *__progname;

extern plugin_t *plugins;

void print_version(void);

void process_options(options_t *opt, int argc, char **argv);

#define EXPORT_PLUGIN(plugin)						\
	static void __attribute__((constructor)) register_##plugin(void) \
	{ \
		plugin_t *p = (plugin_t *)& (plugin);	\
		\
		p->next = plugins; \
		plugins = p; \
	}

#endif /* IMAGE_BUILD_H */
