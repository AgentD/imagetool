/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * msdosfs_conv.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "fatfs.h"

#define SECS_PER_HOUR (3600)
#define SECS_PER_DAY (24 * SECS_PER_HOUR)

#define MSDOS_EPOCH (315532800L)
#define MSDOS_MAX_TS (4102444799L)

static bool is_leap_year(int year)
{
	return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

static int mdays[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

uint32_t fatfs_convert_timestamp(int64_t timestamp)
{
	int tm_hour, tm_min, tm_sec, tm_year, tm_month, tm_day;
	int64_t days_since_epoch;
	int secs_since_midnight;
	uint32_t value;

	timestamp = (timestamp < MSDOS_EPOCH) ? 0 : (timestamp - MSDOS_EPOCH);
	timestamp = (timestamp > MSDOS_MAX_TS) ? MSDOS_MAX_TS : timestamp;

	days_since_epoch = timestamp / SECS_PER_DAY;
	secs_since_midnight = timestamp % SECS_PER_DAY;

	/* time */
	tm_hour = secs_since_midnight / SECS_PER_HOUR;
	secs_since_midnight %= SECS_PER_HOUR;

	tm_min = secs_since_midnight / 60;
	tm_sec = secs_since_midnight % 60;

	/* date */
	tm_year = 1980;

	for (;;) {
		int days_in_year = is_leap_year(tm_year) ? 366 : 365;

		if (days_since_epoch < days_in_year)
			break;

		days_since_epoch -= days_in_year;
		tm_year += 1;
	}

	for (tm_month = 1; tm_month <= 12; ++tm_month) {
		int days_in_month = mdays[tm_month - 1];

		if (tm_month == 2 && is_leap_year(tm_year))
			days_in_month += 1;

		if (days_since_epoch < days_in_month)
			break;

		days_since_epoch -= days_in_month;
	}

	tm_day = days_since_epoch + 1;

	/* re-assemble */
	value  = (tm_sec / 2            ) & 0x0000001F;
	value |= (tm_min           <<  5) & 0x000007E0;
	value |= (tm_hour          << 11) & 0x0000F800;
	value |= (tm_day           << 16) & 0x001F0000;
	value |= (tm_month         << 21) & 0x01E00000;
	value |= ((tm_year - 1980) << 25) & 0xFE000000;
	return value;
}

static const char *illegal = "\"*+,./:;<=>?[\\]|";

static bool is_illegal_char(int c)
{
	return c <= 0x20 || c == 0x7F || (strchr(illegal, c) != NULL);
}

static int append_generation(uint8_t shortname[11], unsigned int gen)
{
	char gentext[8];
	size_t i;

	if (gen > 999999)
		return FAT_SHORTNAME_ERROR;

	i = 7;
	while (i > 0 && shortname[i] == ' ')
		--i;

	snprintf(gentext, sizeof(gentext), "~%u", gen);

	if (strlen(gentext) > (8 - i))
		i = 8 - strlen(gentext);

	memcpy(shortname + i, gentext, strlen(gentext));
	return FAT_SHORTNAME_SUFFIXED;
}

static void convert(FAT_SHORTNAME *conv,
		    const uint8_t *name, size_t namelen,
		    uint8_t *out, size_t outlen)
{
	size_t i, j;

	for (i = 0, j = 0; i < namelen && j < outlen; ++j) {
		if (is_illegal_char(name[i])) {
			while (is_illegal_char(name[i]) && i < namelen)
				++i;

			*conv = FAT_SHORTNAME_SUFFIXED;
			out[j] = '_';
		} else if (islower(name[i])) {
			if (*conv != FAT_SHORTNAME_SUFFIXED)
				*conv = FAT_SHORTNAME_OK;

			out[j] = toupper(name[i++]);
		} else {
			out[j] = name[i++];
		}
	}

	if (i < namelen)
		*conv = FAT_SHORTNAME_SUFFIXED;
}

FAT_SHORTNAME fatfs_mk_shortname(const uint8_t *name, uint8_t shortname[11],
				 size_t len, unsigned int gen)
{
	const uint8_t *ext = NULL, *ext_candidate = NULL;
	FAT_SHORTNAME conv = FAT_SHORTNAME_SAME;
	size_t i, extlen;

	memset(shortname, ' ', 11);

	if (name[0] == ' ' || name[0] == '.')
		return FAT_SHORTNAME_ERROR;

	/* Find the extension */
	for (i = 0; i < len; ++i) {
		switch (name[i]) {
		case ' ':
			break;
		case '.':
			if (ext_candidate == NULL)
				ext_candidate = name + i + 1;
			break;
		default:
			if (ext_candidate != NULL)
				ext = ext_candidate;
			ext_candidate = NULL;
			break;
		}
	}

	/* Convert the extension */
	if (ext != NULL) {
		extlen = ext_candidate == NULL ?
			(len - (size_t)(ext - name)) :
			(size_t)(ext_candidate - ext);

		convert(&conv, ext, extlen, shortname + 8, 11 - 8);

		len = ext - name - 1;
	} else {
		ext = name + len - 1;

		while (*ext == ' ' || *ext == '.')
			--ext;

		len = ext - name + 1;
	}

	/* Convert the name itself */
	if (len == 0) {
		shortname[0] = '_';
	} else {
		convert(&conv, name, len, shortname, 8);
	}

	/* Append the generation number if necessary */
	if (conv == FAT_SHORTNAME_SUFFIXED || gen > 1)
		conv = append_generation(shortname, gen);

	return conv;
}
