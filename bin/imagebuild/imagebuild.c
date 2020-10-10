/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * imagebuild.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

int main(int argc, char **argv)
{
	options_t opt;

	process_options(&opt, argc, argv);
	return EXIT_SUCCESS;
}
