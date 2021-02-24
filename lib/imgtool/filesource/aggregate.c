/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * aggregate.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "filesource.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct sub_source_entry_t {
	struct sub_source_entry_t *next;

	file_source_t *src;
} sub_source_entry_t;

typedef struct {
	file_source_stackable_t base;

	sub_source_entry_t *list;
	sub_source_entry_t *list_end;

	sub_source_entry_t *it;
} file_source_aggregate_t;

static int get_next_record(file_source_t *fs, file_source_record_t **out,
			   istream_t **stream_out)
{
	file_source_aggregate_t *aggregate = (file_source_aggregate_t *)fs;
	int ret;

retry:
	if (aggregate->it == NULL)
		return 1;

	ret = aggregate->it->src->get_next_record(aggregate->it->src,
						  out, stream_out);
	if (ret > 0) {
		aggregate->it = aggregate->it->next;
		goto retry;
	}

	return ret;
}

static void destroy(object_t *obj)
{
	file_source_aggregate_t *fs = (file_source_aggregate_t *)obj;

	while (fs->list != NULL) {
		sub_source_entry_t *ent = fs->list;
		fs->list = ent->next;

		object_drop(ent->src);
		free(ent);
	}

	free(fs);
}

static int add_nested(file_source_stackable_t *base, file_source_t *sub)
{
	file_source_aggregate_t *source = (file_source_aggregate_t *)base;
	sub_source_entry_t *ent = calloc(1, sizeof(*ent));

	if (ent == NULL) {
		perror("creating aggregate source sub entry");
		return -1;
	}

	ent->src = object_grab(sub);

	if (source->list == NULL) {
		source->list = ent;
		source->list_end = ent;
	} else {
		source->list_end->next = ent;
		source->list_end = ent;
	}

	source->it = source->list;
	return 0;
}

file_source_stackable_t *file_source_aggregate_create(void)
{
	file_source_aggregate_t *aggregate = calloc(1, sizeof(*aggregate));
	file_source_stackable_t *stack = (file_source_stackable_t *)aggregate;
	file_source_t *src = (file_source_t *)stack;
	object_t *obj = (object_t *)src;

	if (aggregate == NULL) {
		perror("creating aggregate file source");
		return NULL;
	}

	stack->add_nested = add_nested;
	src->get_next_record = get_next_record;
	obj->destroy = destroy;
	obj->refcount = 1;
	return stack;
}
