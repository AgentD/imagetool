/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"


static int append_sparse_fallback(ostream_t *strm, size_t size)
{
	char buffer[512];
	size_t diff;

	memset(buffer, 0, sizeof(buffer));

	while (size > 0) {
		diff = size < sizeof(buffer) ? size : sizeof(buffer);

		if (strm->append(strm, buffer, diff))
			return -1;

		size -= diff;
	}

	return 0;
}


int ostream_append(ostream_t *strm, const void *data, size_t size)
{
	return strm->append(strm, data, size);
}

int ostream_append_sparse(ostream_t *strm, size_t size)
{
	if (strm->append_sparse == NULL)
		return append_sparse_fallback(strm, size);

	return strm->append_sparse(strm, size);
}

int ostream_flush(ostream_t *strm)
{
	return strm->flush(strm);
}

const char *ostream_get_filename(ostream_t *strm)
{
	return strm->get_filename(strm);
}
