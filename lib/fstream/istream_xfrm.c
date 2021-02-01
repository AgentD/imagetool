/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream_xfrm.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"

typedef struct istream_xfrm_t {
	istream_t base;

	istream_t *wrapped;
	xfrm_stream_t *xfrm;

	uint8_t uncompressed[BUFSZ];
} istream_xfrm_t;

static int precache(istream_t *base)
{
	istream_xfrm_t *comp = (istream_xfrm_t *)base;
	istream_t *wrapped = comp->wrapped;
	uint32_t in_diff, out_diff;
	int ret, mode = XFRM_STREAM_FLUSH_NONE;

	do {
		ret = istream_precache(wrapped);
		if (ret != 0)
			return ret;

		if (wrapped->eof)
			mode = XFRM_STREAM_FLUSH_FULL;

		in_diff = 0;
		out_diff = 0;

		ret = comp->xfrm->process_data(comp->xfrm,
					       wrapped->buffer,
					       wrapped->buffer_used,
					       base->buffer + base->buffer_used,
					       BUFSZ - base->buffer_used,
					       &in_diff, &out_diff, mode);

		if (ret == XFRM_STREAM_ERROR)
			goto fail;

		base->buffer_used += out_diff;
		wrapped->buffer_offset += in_diff;

		if (ret == XFRM_STREAM_END) {
			base->eof = true;
			break;
		}
	} while (ret == XFRM_STREAM_OK);

	return 0;
fail:
	fprintf(stderr,
		"%s: internal error in data decoder.\n",
		wrapped->get_filename(wrapped));
	return -1;
}

static const char *comp_get_filename(istream_t *strm)
{
	istream_xfrm_t *comp = (istream_xfrm_t *)strm;

	return comp->wrapped->get_filename(comp->wrapped);
}

static void comp_destroy(object_t *obj)
{
	istream_xfrm_t *comp = (istream_xfrm_t *)obj;

	object_drop(comp->xfrm);
	object_drop(comp->wrapped);
	free(comp);
}

istream_t *istream_xfrm_create(istream_t *strm, xfrm_stream_t *xfrm)
{
	istream_xfrm_t *comp = calloc(1, sizeof(*comp));
	istream_t *base = (istream_t *)comp;
	object_t *obj = (object_t *)comp;

	if (comp == NULL) {
		fprintf(stderr, "Creating data unpacking wrapper for %s: %s\n",
			strm->get_filename(strm), strerror(errno));
		return NULL;
	}

	comp->xfrm = object_grab(xfrm);
	comp->wrapped = object_grab(strm);

	base = (istream_t *)comp;
	base->get_filename = comp_get_filename;
	base->buffer = comp->uncompressed;
	base->precache = precache;
	base->eof = false;

	obj = (object_t *)comp;
	obj->refcount = 1;
	obj->destroy = comp_destroy;
	return base;
}
