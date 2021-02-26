/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * imagebuild.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

plugin_t *plugins;

int main(int argc, char **argv)
{
	imgtool_state_t *state = NULL;
	int status = EXIT_FAILURE;
	options_t opt;

	process_options(&opt, argc, argv);

	state = imgtool_state_create(opt.output_path);
	if (state == NULL)
		return EXIT_FAILURE;

	while (plugins != NULL) {
		plugin_t *it = plugins;
		plugins = plugins->next;

		if (plugin_registry_add_plugin(state->registry, it))
			goto out;
	}

	if (imgtool_state_init_config(state))
		goto out;

	if (imgtool_process_config_file(state, opt.config_path))
		goto out;

	if (imgtool_state_process(state))
		goto out;

	status = EXIT_SUCCESS;
out:
	object_drop(state);
	return status;
}
