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

typedef struct object_t object_t;
typedef struct meta_object_t meta_object_t;
typedef struct property_value_t property_value_t;

typedef enum {
	PROPERTY_TYPE_NONE = 0,
	PROPERTY_TYPE_STRING,
	PROPERTY_TYPE_BOOL,
	PROPERTY_TYPE_U32_NUMBER,
	PROPERTY_TYPE_U64_NUMBER,
	PROPERTY_TYPE_U64_SIZE,
} PROPERTY_TYPE;

struct object_t {
	size_t refcount;
	const meta_object_t *meta;

	void (*destroy)(object_t *obj);
};

struct property_value_t {
	PROPERTY_TYPE type;

	union {
		const char *string;
		bool boolean;
		uint32_t u32;
		uint64_t u64;
	} value;
};

struct meta_object_t {
	/* name of this data structure */
	const char *name;

	/* pointer to the data structure that this one is derived from */
	const meta_object_t *parent;

	size_t (*get_property_count)(const meta_object_t *meta);

	int (*get_property_desc)(const meta_object_t *meta, size_t i,
				 PROPERTY_TYPE *type, const char **name);

	int (*set_property)(const meta_object_t *meta, size_t i,
			    object_t *obj, const property_value_t *value);

	int (*get_property)(const meta_object_t *meta, size_t i,
			    const object_t *obj,
			    property_value_t *value);
};

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

static inline const meta_object_t *object_reflect(const void *obj)
{
	return ((object_t *)obj)->meta;
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
typedef struct partition_mgr_t partition_mgr_t;

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

#ifdef __cplusplus
extern "C" {
#endif

size_t object_get_property_count(const void *obj);

int object_get_property_desc(const void *obj, size_t idx,
			     PROPERTY_TYPE *type, const char **name);

int object_get_property(const void *obj, size_t idx, property_value_t *out);

int object_set_property(void *obj, size_t idx, const property_value_t *value);

#ifdef __cplusplus
}
#endif

#endif /* PREDEF_H */
