/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * predef.h
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef PREDEF_H
#define PREDEF_H

#include "config.h"

#ifdef __FreeBSD__
#include <sys/endian.h>
#else
#include <endian.h>
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct object_t {
	size_t refcount;

	void (*destroy)(struct object_t *obj);
} object_t;

static inline void* object_grab(void *obj)
{
	((object_t *)obj)->refcount += 1;
	return obj;
}

static inline void* object_drop(void *obj)
{
	if (((object_t *)obj)->refcount == 1) {
		((object_t *)obj)->destroy(obj);
	} else {
		((object_t *)obj)->refcount -= 1;
	}
	return NULL;
}

#if defined(__GNUC__) && __GNUC__ >= 5
#	define SZ_ADD_OV __builtin_add_overflow
#	define SZ_MUL_OV __builtin_mul_overflow
#elif defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
#	if SIZEOF_SIZE_T <= SIZEOF_INT
#		define SZ_ADD_OV __builtin_uadd_overflow
#		define SZ_MUL_OV __builtin_umul_overflow
#	elif SIZEOF_SIZE_T == SIZEOF_LONG
#		define SZ_ADD_OV __builtin_uaddl_overflow
#		define SZ_MUL_OV __builtin_umull_overflow
#	elif SIZEOF_SIZE_T == SIZEOF_LONG_LONG
#		define SZ_ADD_OV __builtin_uaddll_overflow
#		define SZ_MUL_OV __builtin_umulll_overflow
#	else
#		error Cannot determine maximum value of size_t
#	endif
#else
static inline int _sz_add_overflow(size_t a, size_t b, size_t *res)
{
	*res = a + b;
	return (*res < a) ? 1 : 0;
}

static inline int _sz_mul_overflow(size_t a, size_t b, size_t *res)
{
	*res = a * b;
	return (b > 0 && (a > SIZE_MAX / b)) ? 1 : 0;
}
#	define SZ_ADD_OV _sz_add_overflow
#	define SZ_MUL_OV _sz_mul_overflow
#endif

typedef struct ostream_t ostream_t;
typedef struct istream_t istream_t;
typedef struct null_ostream_t null_ostream_t;

typedef struct xfrm_stream_t xfrm_stream_t;
typedef struct xfrm_block_t xfrm_block_t;
typedef struct compressor_config_t compressor_config_t;

typedef struct bitmap_t bitmap_t;

typedef struct volume_t volume_t;

typedef struct gcfg_number_t gcfg_number_t;
typedef struct gcfg_enum_t gcfg_enum_t;
typedef struct gcfg_file_t gcfg_file_t;
typedef struct gcfg_keyword_t gcfg_keyword_t;

typedef struct fstree_t fstree_t;
typedef struct tree_node_t tree_node_t;
typedef struct filesystem_t filesystem_t;

typedef struct file_sink_t file_sink_t;
typedef struct fs_dep_tracker_t fs_dep_tracker_t;

typedef struct file_source_t file_source_t;
typedef struct file_source_stackable_t file_source_stackable_t;
typedef struct file_source_filter_t file_source_filter_t;
typedef struct file_source_listing_t file_source_listing_t;

typedef struct mount_group_t mount_group_t;
typedef struct imgtool_state_t imgtool_state_t;
typedef struct plugin_t plugin_t;
typedef struct plugin_registry_t plugin_registry_t;

#endif /* PREDEF_H */
