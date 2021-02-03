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


typedef struct {
	uint32_t flags;

	uint32_t level;

	union {
		struct {
			uint16_t window_size;

			uint8_t padd0[14];
		} gzip;

		struct {
			uint32_t dict_size;

			uint8_t lc;
			uint8_t lp;
			uint8_t pb;
			uint8_t padd0[9];
		} xz;

		struct {
			uint8_t work_factor;

			uint8_t padd0[16];
		} bzip2;

		uint64_t padd0[2];
	} opt;
} compressor_config_t;

typedef enum {
	COMP_FLAG_XZ_X86 = 0x0001,
	COMP_FLAG_XZ_POWERPC = 0x0002,
	COMP_FLAG_XZ_IA64 = 0x0004,
	COMP_FLAG_XZ_ARM = 0x0008,
	COMP_FLAG_XZ_ARMTHUMB = 0x0010,
	COMP_FLAG_XZ_SPARC = 0x0020,
	COMP_FLAG_XZ_EXTREME = 0x0100,
	COMP_FLAG_XZ_ALL = 0x013F,
} COMP_FLAG_XZ;

typedef enum {
	COMP_FLAG_GZIP_DEFAULT = 0x0001,
	COMP_FLAG_GZIP_FILTERED = 0x0002,
	COMP_FLAG_GZIP_HUFFMAN = 0x0004,
	COMP_FLAG_GZIP_RLE = 0x0008,
	COMP_FLAG_GZIP_FIXED = 0x0010,
	COMP_FLAG_GZIP_ALL = 0x001F,
} COMP_FLAG_GZIP;

#define COMP_GZIP_MIN_LEVEL (1)
#define COMP_GZIP_MAX_LEVEL (9)
#define COMP_GZIP_DEFAULT_LEVEL (9)

#define COMP_GZIP_MIN_WINDOW (8)
#define COMP_GZIP_MAX_WINDOW (15)
#define COMP_GZIP_DEFAULT_WINDOW (15)

#define COMP_ZSTD_MIN_LEVEL (1)
#define COMP_ZSTD_MAX_LEVEL (22)
#define COMP_ZSTD_DEFAULT_LEVEL (15)

#define COMP_BZIP2_MIN_LEVEL (1)
#define COMP_BZIP2_MAX_LEVEL (9)
#define COMP_BZIP2_DEFAULT_LEVEL (9)

#define COMP_BZIP2_MIN_WORK_FACTOR (0)
#define COMP_BZIP2_MAX_WORK_FACTOR (250)
#define COMP_BZIP2_DEFAULT_WORK_FACTOR (30)

#define COMP_XZ_MIN_LEVEL (0)
#define COMP_XZ_MAX_LEVEL (9)
#define COMP_XZ_DEFAULT_LEVEL (6)

#define COMP_XZ_MIN_LC (0)
#define COMP_XZ_MAX_LC (4)
#define COMP_XZ_DEFAULT_LC (3)

#define COMP_XZ_MIN_LP (0)
#define COMP_XZ_MAX_LP (4)
#define COMP_XZ_DEFAULT_LP (0)

#define COMP_XZ_MIN_PB (0)
#define COMP_XZ_MAX_PB (4)
#define COMP_XZ_DEFAULT_PB (2)

#ifdef __cplusplus
extern "C" {
#endif

xfrm_stream_t *compressor_stream_bzip2_create(const compressor_config_t *cfg);

xfrm_stream_t *decompressor_stream_bzip2_create(void);

xfrm_stream_t *compressor_stream_xz_create(const compressor_config_t *cfg);

xfrm_stream_t *decompressor_stream_xz_create(void);

xfrm_stream_t *compressor_stream_gzip_create(const compressor_config_t *cfg);

xfrm_stream_t *decompressor_stream_gzip_create(void);

xfrm_stream_t *compressor_stream_zlib_create(const compressor_config_t *cfg);

xfrm_stream_t *decompressor_stream_zlib_create(void);

xfrm_stream_t *compressor_stream_zstd_create(const compressor_config_t *cfg);

xfrm_stream_t *decompressor_stream_zstd_create(void);

#ifdef __cplusplus
}
#endif

#endif /* XFRM_H */
