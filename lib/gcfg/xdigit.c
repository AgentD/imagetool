/* SPDX-License-Identifier: ISC */
/*
 * xdigit.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"

int gcfg_xdigit(int c)
{
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 0x0A;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 0x0A;
	if (c >= '0' && c <= '9')
		return c - '0';
	return -1;
}
