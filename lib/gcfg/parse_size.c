/* SPDX-License-Identifier: ISC */
/*
 * parse_size.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"

const char *gcfg_parse_size(gcfg_file_t *f, const char *in, uint64_t *ret)
{
	const char *start = in;
	uint64_t shift;

	in = gcfg_dec_num(f, in, ret, 0xFFFFFFFFFFFFFFFF);
	if (in == NULL)
		return NULL;

	switch (*in) {
	case 'k':
	case 'K':
		++in;
		shift = 10;
		break;
	case 'm':
	case 'M':
		++in;
		shift = 20;
		break;
	case 'g':
	case 'G':
		++in;
		shift = 30;
		break;
	case 't':
	case 'T':
		++in;
		shift = 40;
		break;
	default:
		shift = 0;
		break;
	}

	if ((*ret) > (0xFFFFFFFFFFFFFFFFUL >> shift)) {
		f->report_error(f, "numeric overflow in %.6s...", start);
		return NULL;
	}

	(*ret) <<= shift;
	return in;
}
