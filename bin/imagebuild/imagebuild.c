/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * imagebuild.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "imagebuild.h"

static imgtool_state_t *state = NULL;

plugin_t *plugins;

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE;
	gcfg_file_t *gcfg;
	options_t opt;

	process_options(&opt, argc, argv);

	state = imgtool_state_create(opt.output_path);
	if (state == NULL)
		return EXIT_FAILURE;

	while (plugins != NULL) {
		plugin_t *it = plugins;
		plugins = plugins->next;

		if (plugin_registry_add_plugin(state->registry, it))
			goto out_state;
	}

	if (imgtool_state_init_config(state))
		goto out_state;

	gcfg = open_gcfg_file(opt.config_path);
	if (gcfg == NULL)
		goto out_state;

	if (gcfg_parse_file(gcfg, state->cfg_global, (object_t *)state))
		goto out;

	if (imgtool_state_process(state))
		goto out;

	status = EXIT_SUCCESS;
out:
	object_drop(gcfg);
out_state:
	object_drop(state);
	return status;
}
