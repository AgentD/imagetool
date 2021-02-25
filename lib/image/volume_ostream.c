/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * volume_ostream.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "volume.h"
#include "fstream.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	ostream_t base;

	volume_t *wrapped;

	uint64_t offset;
	uint64_t written;
	uint64_t max_size;

	char name[];
} vol_ostream_t;

static int vol_ostream_append(ostream_t *strm, const void *data, size_t size)
{
	vol_ostream_t *vstrm = (vol_ostream_t *)strm;

	if (size == 0)
		return 0;

	if (vstrm->written >= vstrm->max_size ||
	    size > (vstrm->max_size - vstrm->written)) {
		fprintf(stderr, "%s: no space left on the underlying volume.\n",
			vstrm->name);
		return -1;
	}

	if (volume_write(vstrm->wrapped, vstrm->offset + vstrm->written,
			 data, size)) {
		fprintf(stderr, "%s: error in volume while appending data.\n",
			vstrm->name);
		return -1;
	}

	vstrm->written += size;
	return 0;
}

static int vol_ostream_append_sparse(ostream_t *strm, size_t size)
{
	return vol_ostream_append(strm, NULL, size);
}

static int vol_ostream_flush(ostream_t *strm)
{
	(void)strm;
	return 0;
}

static const char *vol_ostream_get_filename(ostream_t *strm)
{
	vol_ostream_t *vstrm = (vol_ostream_t *)strm;
	return vstrm->name;
}

static void vol_ostream_destroy(object_t *obj)
{
	vol_ostream_t *vstrm = (vol_ostream_t *)obj;

	object_drop(vstrm->wrapped);
	free(vstrm);
}

ostream_t *volume_ostream_create(volume_t *vol, const char *name,
				 uint64_t offset, uint64_t max_size)
{
	vol_ostream_t *vstrm = calloc(1, sizeof(*vstrm) + strlen(name) + 1);
	ostream_t *ostrm = (ostream_t *)vstrm;
	object_t *obj = (object_t *)ostrm;

	if (vstrm == NULL) {
		fprintf(stderr, "creating volume stream wrapper for %s: %s\n",
			name, strerror(errno));
		return NULL;
	}

	strcpy(vstrm->name, name);

	vstrm->wrapped = object_grab(vol);
	vstrm->offset = offset;
	vstrm->max_size = max_size;
	ostrm->append = vol_ostream_append;
	ostrm->append_sparse = vol_ostream_append_sparse;
	ostrm->flush = vol_ostream_flush;
	ostrm->get_filename = vol_ostream_get_filename;
	obj->refcount = 1;
	obj->destroy = vol_ostream_destroy;
	return ostrm;
}
