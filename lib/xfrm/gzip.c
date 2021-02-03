/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gzip.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>

#include "xfrm.h"

typedef struct {
	xfrm_stream_t base;

	z_stream strm;
	bool compress;
} xfrm_stream_gzip_t;

static const int zlib_action[] = {
	[XFRM_STREAM_FLUSH_NONE] = Z_NO_FLUSH,
	[XFRM_STREAM_FLUSH_SYNC] = Z_SYNC_FLUSH,
	[XFRM_STREAM_FLUSH_FULL] = Z_FINISH,
};

static int process_data(xfrm_stream_t *stream, const void *in,
			uint32_t in_size, void *out, uint32_t out_size,
			uint32_t *in_read, uint32_t *out_written,
			int flush_mode)
{
	xfrm_stream_gzip_t *gzip = (xfrm_stream_gzip_t *)stream;
	uint32_t diff;
	int ret;

	if (flush_mode < 0 || flush_mode >= XFRM_STREAM_FLUSH_COUNT)
		flush_mode = XFRM_STREAM_FLUSH_NONE;

	while (in_size > 0 && out_size > 0) {
		gzip->strm.next_in = (void *)in;
		gzip->strm.avail_in = in_size;

		gzip->strm.next_out = out;
		gzip->strm.avail_out = out_size;

		if (gzip->compress) {
			ret = deflate(&gzip->strm, zlib_action[flush_mode]);
		} else {
			ret = inflate(&gzip->strm, zlib_action[flush_mode]);
		}

		if (ret == Z_STREAM_ERROR)
			return XFRM_STREAM_ERROR;

		diff = in_size - gzip->strm.avail_in;
		in = (const char *)in + diff;
		in_size -= diff;
		*in_read += diff;

		diff = out_size - gzip->strm.avail_out;
		out = (char *)out + diff;
		out_size -= diff;
		*out_written += diff;

		if (ret == Z_STREAM_END)
			return XFRM_STREAM_END;

		if (ret == Z_BUF_ERROR)
			return XFRM_STREAM_BUFFER_FULL;
	}

	return XFRM_STREAM_OK;
}

static void destroy(object_t *obj)
{
	xfrm_stream_gzip_t *gzip = (xfrm_stream_gzip_t *)obj;

	if (gzip->compress) {
		deflateEnd(&gzip->strm);
	} else {
		inflateEnd(&gzip->strm);
	}
	free(gzip);
}

static xfrm_stream_t *create_stream(const compressor_config_t *cfg,
				    bool compress, bool fmt_gzip)
{
	xfrm_stream_gzip_t *gzip = calloc(1, sizeof(*gzip));
	xfrm_stream_t *xfrm = (xfrm_stream_t *)gzip;
	object_t *obj = (object_t *)xfrm;
	int ret, strategy;

	if (gzip == NULL) {
		perror("creating gzip stream compressor");
		return NULL;
	}

	if (compress) {
		strategy = Z_DEFAULT_STRATEGY;

		if (cfg->flags & COMP_FLAG_GZIP_FILTERED)
			strategy = Z_FILTERED;
		if (cfg->flags & COMP_FLAG_GZIP_HUFFMAN)
			strategy = Z_HUFFMAN_ONLY;
		if (cfg->flags & COMP_FLAG_GZIP_RLE)
			strategy = Z_RLE;
		if (cfg->flags & COMP_FLAG_GZIP_FIXED)
			strategy = Z_FIXED;

		ret = deflateInit2(&gzip->strm, cfg->level, Z_DEFLATED,
				   cfg->opt.gzip.window_size +
				   (fmt_gzip ? 16 : 0), 8, strategy);
	} else {
		ret = inflateInit2(&gzip->strm, fmt_gzip ? (16 + 15) : 15);
	}

	if (ret != Z_OK) {
		fputs("internal error creating gzip compressor.\n", stderr);
		free(gzip);
		return NULL;
	}

	gzip->compress = compress;
	xfrm->process_data = process_data;
	obj->refcount = 1;
	obj->destroy = destroy;
	return xfrm;
}

xfrm_stream_t *compressor_stream_gzip_create(const compressor_config_t *cfg)
{
	return create_stream(cfg, true, true);
}

xfrm_stream_t *decompressor_stream_gzip_create(void)
{
	return create_stream(NULL, false, true);
}

xfrm_stream_t *compressor_stream_zlib_create(const compressor_config_t *cfg)
{
	return create_stream(cfg, true, false);
}

xfrm_stream_t *decompressor_stream_zlib_create(void)
{
	return create_stream(NULL, false, false);
}
