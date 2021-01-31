/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xfrm.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef XFRM_H
#define XFRM_H

#include "predef.h"

typedef enum {
	XFRM_STREAM_FLUSH_NONE = 0,
	XFRM_STREAM_FLUSH_SYNC,
	XFRM_STREAM_FLUSH_FULL,

	XFRM_STREAM_FLUSH_COUNT,
} XFRM_STREAM_FLUSH;

typedef enum {
	XFRM_STREAM_ERROR = -1,
	XFRM_STREAM_OK = 0,
	XFRM_STREAM_END = 1,
	XFRM_STREAM_BUFFER_FULL = 2,
} XFRM_STREAM_RESULT;

typedef struct xfrm_stream_t {
	object_t base;

	int (*process_data)(struct xfrm_stream_t *stream,
			    const void *in, uint32_t in_size,
			    void *out, uint32_t out_size,
			    uint32_t *in_read, uint32_t *out_written,
			    int flush_mode);
} xfrm_stream_t;

typedef struct xfrm_block_t {
	object_t base;

	int32_t (*process_block)(struct xfrm_block_t *xfrm,
				 void *buffer, uint32_t size);
} xfrm_block_t;


#ifdef __cplusplus
extern "C" {
#endif

xfrm_stream_t *compressor_stream_bzip2_create(void);

xfrm_stream_t *decompressor_stream_bzip2_create(void);

xfrm_stream_t *compressor_stream_xz_create(void);

xfrm_stream_t *decompressor_stream_xz_create(void);

xfrm_stream_t *compressor_stream_gzip_create(void);

xfrm_stream_t *decompressor_stream_gzip_create(void);

xfrm_stream_t *compressor_stream_zlib_create(void);

xfrm_stream_t *decompressor_stream_zlib_create(void);

xfrm_stream_t *compressor_stream_zstd_create(void);

xfrm_stream_t *decompressor_stream_zstd_create(void);

#ifdef __cplusplus
}
#endif

#endif /* XFRM_H */
