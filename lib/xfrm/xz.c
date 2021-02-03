/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xz.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include <stdlib.h>
#include <stdio.h>
#include <lzma.h>

#include "xfrm.h"

typedef struct {
	xfrm_stream_t base;

	lzma_stream strm;
} xfrm_xz_t;

static const lzma_action xzlib_action[] = {
	[XFRM_STREAM_FLUSH_NONE] = LZMA_RUN,
	[XFRM_STREAM_FLUSH_SYNC] = LZMA_FULL_FLUSH,
	[XFRM_STREAM_FLUSH_FULL] = LZMA_FINISH,
};

static lzma_vli vli_filter_from_flags(int flags)
{
	if (flags & COMP_FLAG_XZ_X86)
		return LZMA_FILTER_X86;
	if (flags & COMP_FLAG_XZ_POWERPC)
		return LZMA_FILTER_POWERPC;
	if (flags & COMP_FLAG_XZ_IA64)
		return LZMA_FILTER_IA64;
	if (flags & COMP_FLAG_XZ_ARM)
		return LZMA_FILTER_ARM;
	if (flags & COMP_FLAG_XZ_ARMTHUMB)
		return LZMA_FILTER_ARMTHUMB;
	if (flags & COMP_FLAG_XZ_SPARC)
		return LZMA_FILTER_SPARC;

	return LZMA_VLI_UNKNOWN;
}

static int process_data(xfrm_stream_t *stream, const void *in,
			uint32_t in_size, void *out, uint32_t out_size,
			uint32_t *in_read, uint32_t *out_written,
			int flush_mode)
{
	xfrm_xz_t *xz = (xfrm_xz_t *)stream;
	lzma_ret ret_xz;
	uint32_t diff;

	if (flush_mode < 0 || flush_mode >= XFRM_STREAM_FLUSH_COUNT)
		flush_mode = XFRM_STREAM_FLUSH_NONE;

	while (in_size > 0 && out_size > 0) {
		xz->strm.next_in = in;
		xz->strm.avail_in = in_size;

		xz->strm.next_out = out;
		xz->strm.avail_out = out_size;

		ret_xz = lzma_code(&xz->strm, xzlib_action[flush_mode]);

		if (ret_xz != LZMA_OK && ret_xz != LZMA_BUF_ERROR &&
		    ret_xz != LZMA_STREAM_END) {
			return XFRM_STREAM_ERROR;
		}

		diff = in_size - xz->strm.avail_in;
		in = (const char *)in + diff;
		in_size -= diff;
		*in_read += diff;

		diff = out_size - xz->strm.avail_out;
		out = (char *)out + diff;
		out_size -= diff;
		*out_written += diff;

		if (ret_xz == LZMA_BUF_ERROR)
			return XFRM_STREAM_BUFFER_FULL;

		if (ret_xz == LZMA_STREAM_END)
			return XFRM_STREAM_END;
	}

	return XFRM_STREAM_OK;
}

static void destroy(object_t *obj)
{
	xfrm_xz_t *xz = (xfrm_xz_t *)obj;

	lzma_end(&xz->strm);
	free(xz);
}

static xfrm_stream_t *create_stream(const compressor_config_t *cfg,
				    bool compress)
{
	xfrm_xz_t *xz = calloc(1, sizeof(*xz));
	xfrm_stream_t *xfrm = (xfrm_stream_t *)xz;
	object_t *obj = (object_t *)xz;
	uint64_t memlimit = 128 * 1024 * 1024;
	lzma_filter filters[3];
	lzma_options_lzma opt;
	lzma_vli vli_filter;
	uint32_t presets;
	lzma_ret ret_xz;
	int i = 0;

	if (xz == NULL) {
		perror("creating xz stream compressor");
		return NULL;
	}

	if (compress) {
		presets = cfg->level;
		if (cfg->flags & COMP_FLAG_XZ_EXTREME)
			presets |= LZMA_PRESET_EXTREME;

		if (lzma_lzma_preset(&opt, presets))
			goto fail_init;

		opt.lc = cfg->opt.xz.lc;
		opt.lp = cfg->opt.xz.lp;
		opt.pb = cfg->opt.xz.pb;
		opt.dict_size = cfg->opt.xz.dict_size;

		vli_filter = vli_filter_from_flags(cfg->flags);
		if (vli_filter != LZMA_VLI_UNKNOWN) {
			filters[i].id = vli_filter;
			filters[i].options = NULL;
			++i;
		}

		filters[i].id = LZMA_FILTER_LZMA2;
		filters[i].options = &opt;
		++i;

		filters[i].id = LZMA_VLI_UNKNOWN;
		filters[i].options = NULL;
		++i;

		ret_xz = lzma_stream_encoder(&xz->strm, filters,
					     LZMA_CHECK_CRC32);
		if (ret_xz != LZMA_OK)
			goto fail_init;
	} else {
		if (lzma_stream_decoder(&xz->strm, memlimit, 0) != LZMA_OK)
			goto fail_init;
	}

	xfrm->process_data = process_data;
	obj->refcount = 1;
	obj->destroy = destroy;
	return xfrm;
fail_init:
	fputs("error initializing XZ compressor\n", stderr);
	free(xz);
	return NULL;
}

xfrm_stream_t *compressor_stream_xz_create(const compressor_config_t *cfg)
{
	return create_stream(cfg, true);
}

xfrm_stream_t *decompressor_stream_xz_create(void)
{
	return create_stream(NULL, false);
}
