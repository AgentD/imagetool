/* SPDX-License-Identifier: ISC */
/*
 * dec_num.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"

const char *gcfg_dec_num(gcfg_file_t *f, const char *str,
			 uint64_t *out, uint64_t max)
{
	const char *start = str;
	uint64_t x;

	if (*str < '0' || *str > '9')
		goto fail_num;

	if (str[0] == '0' && (str[1] >= '0' && str[1] <= '9'))
		goto fail_lead0;

	*out = 0;

	while (*str >= '0' && *str <= '9') {
		x = ((uint64_t)*(str++) & 0x00FF) - '0';

		if ((*out) > (max / 10) || ((*out) * 10) > (max - x))
			goto fail_ov;

		*out = (*out) * 10 + x;
	}

	return str;
fail_lead0:
	if (f != NULL)
		f->report_error(f, "numbers must not use leading zeros");
	return NULL;
fail_num:
	if (f != NULL)
		f->report_error(f, "expected digit instead of '%.6s...", str);
	return NULL;
fail_ov:
	if (f != NULL)
		f->report_error(f, "numeric overflow in '%.6s...'", start);
	return NULL;
}
