/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * predef.h
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef PREDEF_H
#define PREDEF_H

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

#endif /* PREDEF_H */
