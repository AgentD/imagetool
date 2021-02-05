/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * options.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

static struct option long_opts[] = {
	{ "config", required_argument, NULL, 'c' },
	{ "output", required_argument, NULL, 'O' },
	{ "version", no_argument, NULL, 'V' },
	{ "help", no_argument, NULL, 'h' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "c:O:hV";

static const char *help_string =
"Usage: %s [OPTIONS...]\n"
"\n"
"Mandatory options:\n"
"\n"
"  --config, -c <file>  The path to the main configuration file.\n"
"  --output, -O <file>  The name of the output file to generate.\n"
"\n";

void process_options(options_t *opt, int argc, char **argv)
{
	int i;

	memset(opt, 0, sizeof(*opt));

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'c':
			opt->config_path = optarg;
			break;
		case 'O':
			opt->output_path = optarg;
			break;
		case 'h':
			printf(help_string, __progname);
			exit(EXIT_SUCCESS);
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
		default:
			goto fail_arg;
		}
	}

	if (opt->config_path == NULL) {
		fputs("No layout file specified.\n", stderr);
		goto fail_arg;
	}

	if (opt->output_path == NULL) {
		fputs("No output file specified.\n", stderr);
		goto fail_arg;
	}

	if (optind < argc) {
		fputs("Unknown extra arguments specified.\n", stderr);
		goto fail_arg;
	}
	return;
fail_arg:
	fprintf(stderr, "Try `%s --help' for more information.\n", __progname);
	exit(EXIT_FAILURE);
}
