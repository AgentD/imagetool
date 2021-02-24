/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gcfg.h
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef GCFG_H
#define GCFG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "predef.h"

#define GCFG_PRINTF_FUN(fmtidx, elidx) \
	__attribute__ ((format (printf, fmtidx, elidx)))

typedef enum {
	GCFG_ARG_NONE = 0x00,

	GCFG_ARG_BOOLEAN = 0x01,
	GCFG_ARG_STRING = 0x02,
	GCFG_ARG_ENUM = 0x04,

	GCFG_ARG_NUMBER = 0x05,
	GCFG_ARG_VEC2 = 0x06,
	GCFG_ARG_VEC3 = 0x07,
	GCFG_ARG_VEC4 = 0x08,

	GCFG_ARG_SIZE = 0x0D,
} GCFG_ARG_TYPE;

typedef struct {
	int32_t sign;
	uint32_t integer;
	uint32_t fraction;
	uint16_t fraction_digits;
	int16_t exponent;
} gcfg_number_t;

typedef struct {
	const char *name;
	intptr_t value;
} gcfg_enum_t;

typedef struct gcfg_file_t {
	object_t obj;

	void (*report_error)(struct gcfg_file_t *f, const char *msg, ...)
		GCFG_PRINTF_FUN(2, 3);

	/* Reads a line into the mutable buffer below. Returns > 0 on EOF, < 0
	   on internal error, 0 on success.

	   On success, line break at the end is removed from the line and a
	   null-terminator is added.
	*/
	int (*fetch_line)(struct gcfg_file_t *f);

	/* Mutable buffer holding the current line */
	char *buffer;
} gcfg_file_t;

typedef struct gcfg_keyword_t gcfg_keyword_t;

struct gcfg_keyword_t {
	uint32_t arg;
	uint32_t pad0;

	const char *name;

	union {
		const gcfg_enum_t *enumtokens;
	} option;

	union {
		object_t *(*cb_none)(const gcfg_keyword_t *kwd,
				     gcfg_file_t *file, object_t *parent);
		object_t *(*cb_bool)(const gcfg_keyword_t *kwd,
				     gcfg_file_t *file, object_t *parent,
				     bool boolean);
		object_t *(*cb_string)(const gcfg_keyword_t *kwd,
				       gcfg_file_t *file, object_t *parent,
				       const char *string);
		object_t *(*cb_enum)(const gcfg_keyword_t *kwd,
				     gcfg_file_t *file, object_t *parent,
				     int value);
		object_t *(*cb_number)(const gcfg_keyword_t *kwd,
				       gcfg_file_t *file, object_t *parent,
				       const gcfg_number_t *num, int count);
		object_t *(*cb_size)(const gcfg_keyword_t *kwd,
				     gcfg_file_t *file, object_t *parent,
				     uint64_t size);
	} handle;

	const gcfg_keyword_t *children;

	int (*finalize_object)(gcfg_file_t *file, object_t *child);

	int (*handle_listing)(gcfg_file_t *file, object_t *child,
			      const char *line);
};

#ifdef __cplusplus
extern "C" {
#endif

double gcfg_number_to_double(const gcfg_number_t *num);

int gcfg_parse_file(gcfg_file_t *file, const gcfg_keyword_t *keywords,
		    object_t *usr);



const char *gcfg_dec_num(gcfg_file_t *f, const char *str,
			 uint64_t *out, uint64_t max);

const char *gcfg_parse_number(gcfg_file_t *f, const char *in,
			      gcfg_number_t *num);

const char *gcfg_parse_boolean(gcfg_file_t *f, const char *in, int *out);

const char *gcfg_parse_vector(gcfg_file_t *f, const char *in,
			      int count, gcfg_number_t *out);

const char *gcfg_parse_enum(gcfg_file_t *f, const char *in,
			    const gcfg_enum_t *tokens, int *out);

const char *gcfg_parse_size(gcfg_file_t *f, const char *in, uint64_t *ret);

const char *gcfg_parse_string(gcfg_file_t *f, const char *in, char *out);

bool gcfg_is_valid_cp(uint32_t cp);

bool gcfg_is_valid_utf8(const uint8_t *str, size_t len);

int gcfg_xdigit(int c);

#ifdef __cplusplus
}
#endif

#endif /* GCFG_H */
