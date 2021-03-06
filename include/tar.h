/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef TAR_H
#define TAR_H

#include "config.h"
#include "predef.h"
#include "fstream.h"

#include <sys/types.h>
#include <sys/stat.h>

typedef struct sparse_map_t {
	struct sparse_map_t *next;
	uint64_t offset;
	uint64_t count;
} sparse_map_t;

typedef struct {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char typeflag;
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	union {
		struct {
			char prefix[155];
			char padding[12];
		} posix;

		struct {
			char atime[12];
			char ctime[12];
			char offset[12];
			char deprecated[4];
			char unused;
			struct {
				char offset[12];
				char numbytes[12];
			} sparse[4];
			char isextended;
			char realsize[12];
			char padding[17];
		} gnu;
	} tail;
} tar_header_t;

typedef struct {
	struct {
		char offset[12];
		char numbytes[12];
	} sparse[21];
	char isextended;
	char padding[7];
} gnu_sparse_t;

typedef struct tar_xattr_t {
	struct tar_xattr_t *next;
	char *key;
	uint8_t *value;
	size_t value_len;
	char data[];
} tar_xattr_t;

typedef struct {
	struct stat sb;
	char *name;
	char *link_target;
	sparse_map_t *sparse;
	uint64_t actual_size;
	uint64_t record_size;
	bool unknown_record;
	bool is_hard_link;
	tar_xattr_t *xattr;

	/* broken out since struct stat could contain
	   32 bit values on 32 bit systems. */
	int64_t mtime;
} tar_header_decoded_t;

#define TAR_TYPE_FILE '0'
#define TAR_TYPE_LINK '1'
#define TAR_TYPE_SLINK '2'
#define TAR_TYPE_CHARDEV '3'
#define TAR_TYPE_BLOCKDEV '4'
#define TAR_TYPE_DIR '5'
#define TAR_TYPE_FIFO '6'

#define TAR_TYPE_GNU_SLINK 'K'
#define TAR_TYPE_GNU_PATH 'L'
#define TAR_TYPE_GNU_SPARSE 'S'

#define TAR_TYPE_PAX 'x'
#define TAR_TYPE_PAX_GLOBAL 'g'

#define TAR_MAGIC "ustar"
#define TAR_VERSION "00"

#define TAR_MAGIC_OLD "ustar "
#define TAR_VERSION_OLD " "

#define TAR_RECORD_SIZE (512)

/* calcuate and skip the zero padding */
int skip_padding(istream_t *fp, uint64_t size);

/* round up to block size and skip the entire entry */
int skip_entry(istream_t *fp, uint64_t size);

int read_header(istream_t *fp, tar_header_decoded_t *out);

void free_xattr_list(tar_xattr_t *list);

void clear_header(tar_header_decoded_t *hdr);

#endif /* TAR_H */
