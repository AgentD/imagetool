
/* SPDX-License-Identifier: ISC */
/*
 * parse_number.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"

const char *gcfg_parse_number(gcfg_file_t *f, const char *in,
			      gcfg_number_t *num)
{
	uint64_t temp;
	int i;

	num->sign = (*in == '-') ? -1 : 1;
	if (*in == '+' || *in == '-')
		++in;

	in = gcfg_dec_num(f, in, &temp, 0xFFFFFFFF);
	if (in == NULL)
		return NULL;

	num->integer = (uint32_t)temp;

	if (*in == '.') {
		++in;
		for (i = 0; in[i] >= '0' && in[i] <= '9'; ++i)
			;

		in = gcfg_dec_num(f, in, &temp, 0xFFFFFFFF);
		if (in == NULL)
			return NULL;

		num->fraction_digits = (uint16_t)i;
		num->fraction = (uint32_t)temp;
	} else {
		num->fraction_digits = 0;
		num->fraction = 0;
	}

	switch (*in) {
	case 'e':
	case 'E':
		i = (in[1] == '-') ? -1 : 1;
		in += (in[1] == '-' || in[1] == '+') ? 2 : 1;

		in = gcfg_dec_num(f, in, &temp, 0x7FFF);
		if (in == NULL)
			return NULL;

		num->exponent = (int16_t)((int)temp * i);
		break;
	case '%':
		++in;
		num->exponent = -2;
		break;
	default:
		num->exponent = 0;
		break;
	}

	return in;
}
