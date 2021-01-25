/* SPDX-License-Identifier: ISC */
/*
 * number_to_double.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"

#include <math.h>

double gcfg_number_to_double(const gcfg_number_t *num)
{
	double ret;

	ret = num->integer;
	ret += num->fraction * pow(10.0, -((int)num->fraction_digits));
	ret *= pow(10.0, num->exponent);

	return num->sign < 0 ? -ret : ret;
}
