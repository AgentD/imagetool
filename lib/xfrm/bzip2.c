/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bzip2.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include <stdlib.h>
#include <stdio.h>
#include <bzlib.h>

#include "xfrm.h"

typedef struct {
	xfrm_stream_t base;

	bz_stream strm;
	bool compress;
} xfrm_stream_bzip2_t;

static const int bzlib_action[] = {
	[XFRM_STREAM_FLUSH_NONE] = BZ_RUN,
	[XFRM_STREAM_FLUSH_SYNC] = BZ_FLUSH,
	[XFRM_STREAM_FLUSH_FULL] = BZ_FINISH,
};

static int process_data(xfrm_stream_t *stream, const void *in, uint32_t in_size,
			void *out, uint32_t out_size,
			uint32_t *in_read, uint32_t *out_written,
			int flush_mode)
{
	xfrm_stream_bzip2_t *bzip2 = (xfrm_stream_bzip2_t *)stream;
	uint32_t diff;
	int ret;

	if (flush_mode < 0 || flush_mode >= XFRM_STREAM_FLUSH_COUNT)
		flush_mode = XFRM_STREAM_FLUSH_NONE;

	while (in_size > 0 && out_size > 0) {
		bzip2->strm.next_in = (char *)in;
		bzip2->strm.avail_in = in_size;

		bzip2->strm.next_out = (char *)out;
		bzip2->strm.avail_out = out_size;

		if (bzip2->compress) {
			ret = BZ2_bzCompress(&bzip2->strm,
					     bzlib_action[flush_mode]);
		} else {
			ret = BZ2_bzDecompress(&bzip2->strm);
		}

		if (ret < 0)
			return XFRM_STREAM_ERROR;

		diff = (in_size - bzip2->strm.avail_in);
		in = (const char *)in + diff;
		in_size -= diff;
		*in_read += diff;

		diff = (out_size - bzip2->strm.avail_out);
		out = (char *)out + diff;
		out_size -= diff;
		*out_written += diff;

		if (ret == BZ_OUTBUFF_FULL)
			return XFRM_STREAM_BUFFER_FULL;

		if (ret == BZ_STREAM_END)
			return XFRM_STREAM_END;
	}

	return XFRM_STREAM_OK;
}

static void destroy(object_t *obj)
{
	xfrm_stream_bzip2_t *bzip2 = (xfrm_stream_bzip2_t *)obj;

	if (bzip2->compress) {
		BZ2_bzCompressEnd(&bzip2->strm);
	} else {
		BZ2_bzDecompressEnd(&bzip2->strm);
	}

	free(bzip2);
}

static xfrm_stream_t *stream_create(bool compress)
{
	xfrm_stream_bzip2_t *bzip2 = calloc(1, sizeof(*bzip2));
	xfrm_stream_t *xfrm = (xfrm_stream_t *)bzip2;
	object_t *obj = (object_t *)xfrm;
	int ret;

	if (bzip2 == NULL) {
		perror("creating bzip2 stream compressor");
		return NULL;
	}

	if (compress) {
		ret = BZ2_bzCompressInit(&bzip2->strm, 9, 0, 30);
	} else {
		ret = BZ2_bzDecompressInit(&bzip2->strm, 0, 0);
	}

	if (ret != BZ_OK) {
		fputs("internal error creating bzip2 stream compressor.\n",
		      stderr);
		free(bzip2);
		return NULL;
	}

	bzip2->compress = compress;
	xfrm->process_data = process_data;
	obj->refcount = 1;
	obj->destroy = destroy;
	return xfrm;
}

xfrm_stream_t *compressor_stream_bzip2_create(void)
{
	return stream_create(true);
}

xfrm_stream_t *decompressor_stream_bzip2_create(void)
{
	return stream_create(false);
}
