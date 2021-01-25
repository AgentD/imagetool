/* SPDX-License-Identifier: ISC */
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

	GCFG_ARG_IPV4 = 0x09,
	GCFG_ARG_IPV6 = 0x0A,
	GCFG_ARG_MAC_ADDR = 0x0B,
	GCFG_ARG_BANDWIDTH = 0x0C,

	GCFG_ARG_SIZE = 0x0D,
} GCFG_ARG_TYPE;

typedef struct {
	union {
		uint32_t v4;
		uint16_t v6[8];
	} ip;

	uint32_t cidr_mask;
} gcfg_ip_addr_t;

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

typedef struct gcfg_keyword_t {
	uint32_t arg;
	uint32_t pad0;

	const char *name;

	union {
		const gcfg_enum_t *enumtokens;
	} option;

	union {
		void *(*cb_none)(gcfg_file_t *file, void *parent);
		void *(*cb_bool)(gcfg_file_t *file, void *parent, bool boolean);
		void *(*cb_string)(gcfg_file_t *file, void *parent,
				   const char *string);
		void *(*cb_enum)(gcfg_file_t *file, void *parent, int value);
		void *(*cb_number)(gcfg_file_t *file, void *parent,
				   const gcfg_number_t *num, int count);
		void *(*cb_ip)(gcfg_file_t *file, void *parent,
			       const gcfg_ip_addr_t *address);
		void *(*cb_mac)(gcfg_file_t *file, void *parent,
				uint32_t vendor, uint32_t device);
		void *(*cb_bandwidth)(gcfg_file_t *file, void *parent,
				      uint64_t bandwidth);
		void *(*cb_size)(gcfg_file_t *file, void *parent,
				 uint64_t size);
	} handle;

	const struct gcfg_keyword_t *children;

	int (*finalize_object)(gcfg_file_t *file, void *child);

	int (*handle_listing)(gcfg_file_t *file, void *child,
			      const char *line);
} gcfg_keyword_t;


#define GCFG_BEGIN_ENUM(name) static const gcfg_enum_t name[] = {

#define GCFG_ENUM(nam, val) { .name = nam, .value = val }

#define GCFG_END_ENUM() { 0, 0 } }


#define GCFG_KEYWORD_BASE(nam, karg, elist, clist, cbfield, cb, finalize) \
	{ \
		.name = nam, \
		.arg = karg, \
		.option = { .enumtokens = elist, }, \
		.children = clist, \
		.handle = { cbfield = cb }, \
		.finalize_object = finalize, \
	}

#define GCFG_BEGIN_KEYWORDS(listname) \
	static const gcfg_keyword_t listname[] = {

#define GCFG_KEYWORD_NO_ARG(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_NONE, NULL, childlist, \
			  .cb_none, callback, finalize)

#define GCFG_KEYWORD_BOOL(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_BOOLEAN, NULL, childlist, \
			  .cb_bool, callback, finalize)

#define GCFG_KEYWORD_STRING(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_STRING, NULL, childlist, \
			  .cb_string, callback, finalize)

#define GCFG_KEYWORD_ENUM(kwdname, childlist, enumlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_ENUM, enumlist, childlist, \
			  .cb_enum, callback, finalize)

#define GCFG_KEYWORD_NUMBER(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_NUMBER, NULL, childlist, \
			  .cb_number, callback, finalize)

#define GCFG_KEYWORD_VEC2(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_VEC2, NULL, childlist, \
			  .cb_number, callback, finalize)

#define GCFG_KEYWORD_VEC3(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_VEC3, NULL, childlist, \
			  .cb_number, callback, finalize)

#define GCFG_KEYWORD_VEC4(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_VEC4, NULL, childlist, \
			  .cb_number, callback, finalize)

#define GCFG_KEYWORD_IPV4(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_IPV4, NULL, childlist, \
			  .cb_ip, callback, finalize)

#define GCFG_KEYWORD_IPV6(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_IPV6, NULL, childlist, \
			  .cb_ip, callback, finalize)

#define GCFG_KEYWORD_MAC(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_MAC_ADDR, NULL, childlist, \
			  .cb_mac, callback, finalize)

#define GCFG_KEYWORD_BANDWIDTH(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_BANDWIDTH, NULL, childlist, \
			  .cb_bandwidth, callback, finalize)

#define GCFG_KEYWORD_SIZE(kwdname, childlist, callback, finalize) \
	GCFG_KEYWORD_BASE(kwdname, GCFG_ARG_SIZE, NULL, childlist, \
			  .cb_size, callback, finalize)

#define GCFG_END_KEYWORDS() \
		{ .name = NULL }, \
	}

#ifdef __cplusplus
extern "C" {
#endif

double gcfg_number_to_double(const gcfg_number_t *num);

int gcfg_parse_file(gcfg_file_t *file, const gcfg_keyword_t *keywords,
		    void *usr);

gcfg_file_t *gcfg_file_open(const char *path);

void gcfg_file_close(gcfg_file_t *file);





const char *gcfg_dec_num(gcfg_file_t *f, const char *str,
			 uint64_t *out, uint64_t max);

const char *gcfg_ipv4address(gcfg_file_t *f, const char *str, uint32_t *out);

const char *gcfg_parse_ipv4(gcfg_file_t *f, const char *in,
			    gcfg_ip_addr_t *ret);

const char *gcfg_parse_ipv6(gcfg_file_t *f, const char *in,
			    gcfg_ip_addr_t *ret);

const char *gcfg_parse_mac_addr(gcfg_file_t *f, const char *in,
				uint32_t *vendor, uint32_t *device);

const char *gcfg_parse_bandwidth(gcfg_file_t *f, const char *in, uint64_t *ret);

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
