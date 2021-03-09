/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * parse_enum.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"

#include <string.h>

const char *gcfg_parse_enum(gcfg_file_t *f, const char *in,
			    const property_enum_t *tokens, size_t count,
			    int *out)
{
	size_t i, len;

	for (i = 0; i < count; ++i) {
		len = strlen(tokens[i].name);

		if (strncmp(tokens[i].name, in, len) != 0)
			continue;

		if (in[len] == ' ' || in[len] == '\t' || in[len] == '\0')
			break;
	}

	if (i >= count) {
		if (f != NULL)
			f->report_error(f, "unexpected '%.5s...'", in);
		return NULL;
	}

	*out = (int)tokens[i].value;
	return in + len;
}
