/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream_xfrm.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"

typedef struct ostream_xfrm_t {
	ostream_t base;

	ostream_t *wrapped;
	xfrm_stream_t *xfrm;

	size_t inbuf_used;

	uint8_t inbuf[BUFSZ];
	uint8_t outbuf[BUFSZ];
} ostream_xfrm_t;

static int flush_xfrm(ostream_xfrm_t *comp, int flush_mode)
{
	uint32_t in_diff = 0, out_diff = 0;
	int ret;

	ret = comp->xfrm->process_data(comp->xfrm,
				       comp->inbuf, comp->inbuf_used,
				       comp->outbuf, BUFSZ,
				       &in_diff, &out_diff, flush_mode);

	if (ret != XFRM_STREAM_OK && ret != XFRM_STREAM_END)
		goto fail;

	if (comp->wrapped->append(comp->wrapped, comp->outbuf, out_diff))
		return -1;

	if (in_diff < comp->inbuf_used) {
		memmove(comp->inbuf, comp->inbuf + in_diff,
			comp->inbuf_used - comp->inbuf_used);
	}

	comp->inbuf_used -= in_diff;
	return 0;
fail:
	fprintf(stderr, "%s: internal error processing data.\n",
		comp->wrapped->get_filename(comp->wrapped));
	return -1;
}

static int comp_append(ostream_t *strm, const void *data, size_t size)
{
	ostream_xfrm_t *comp = (ostream_xfrm_t *)strm;
	size_t diff;

	while (size > 0) {
		if (comp->inbuf_used >= BUFSZ) {
			if (flush_xfrm(comp, XFRM_STREAM_FLUSH_NONE))
				return -1;
		}

		diff = BUFSZ - comp->inbuf_used;

		if (diff > size)
			diff = size;

		memcpy(comp->inbuf + comp->inbuf_used, data, diff);

		comp->inbuf_used += diff;
		data = (const char *)data + diff;
		size -= diff;
	}

	return 0;
}

static int comp_flush(ostream_t *strm)
{
	ostream_xfrm_t *comp = (ostream_xfrm_t *)strm;

	if (comp->inbuf_used > 0) {
		if (flush_xfrm(comp, XFRM_STREAM_FLUSH_FULL))
			return -1;
	}

	return comp->wrapped->flush(comp->wrapped);
}

static const char *comp_get_filename(ostream_t *strm)
{
	ostream_xfrm_t *comp = (ostream_xfrm_t *)strm;

	return comp->wrapped->get_filename(comp->wrapped);
}

static void comp_destroy(object_t *obj)
{
	ostream_xfrm_t *comp = (ostream_xfrm_t *)obj;

	object_drop(comp->xfrm);
	object_drop(comp->wrapped);
	free(comp);
}

ostream_t *ostream_xfrm_create(ostream_t *strm, xfrm_stream_t *xfrm)
{
	ostream_xfrm_t *comp = calloc(1, sizeof(*comp));
	ostream_t *base = (ostream_t *)comp;
	object_t *obj = (object_t *)comp;

	if (comp == NULL) {
		fprintf(stderr, "Creating data processing wrapper for %s: %s\n",
			strm->get_filename(strm), strerror(errno));
		return NULL;
	}

	comp->xfrm = object_grab(xfrm);
	comp->wrapped = object_grab(strm);
	comp->inbuf_used = 0;

	base = (ostream_t *)comp;
	base->append = comp_append;
	base->flush = comp_flush;
	base->get_filename = comp_get_filename;

	obj = (object_t *)comp;
	obj->refcount = 1;
	obj->destroy = comp_destroy;
	return base;
}
