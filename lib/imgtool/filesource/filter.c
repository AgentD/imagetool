/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filter.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "filesource.h"

#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct rule_t {
	struct rule_t *next;
	int target;

	char pattern[];
} rule_t;

struct file_source_filter_t {
	file_source_t base;

	file_source_t *wrapped;

	rule_t *rules;
	rule_t *rules_last;
};

static int get_next_record(file_source_t *fs, file_source_record_t **out,
			   istream_t **stream_out)
{
	file_source_filter_t *filter = (file_source_filter_t *)fs;
	rule_t *r;
	int ret;

	for (;;) {
		ret = filter->wrapped->get_next_record(filter->wrapped, out,
						       stream_out);
		if (ret != 0)
			return ret;

		for (r = filter->rules; r != NULL; r = r->next) {
			ret = fnmatch(r->pattern, (*out)->full_path, 0);
			if (ret == 0)
				break;
		}

		if (r != NULL) {
			if (r->target == FILE_SOURCE_FILTER_ALLOW)
				break;
		}

		free((*out)->full_path);
		free((*out)->link_target);
		free(*out);
		*out = NULL;

		if (stream_out != NULL && (*stream_out) != NULL) {
			object_drop(*stream_out);
			*stream_out = NULL;
		}
	}

	return 0;
}

static void destroy(object_t *obj)
{
	file_source_filter_t *filter = (file_source_filter_t *)obj;

	while (filter->rules != NULL) {
		rule_t *r = filter->rules;
		filter->rules = r->next;
		free(r);
	}

	object_drop(filter->wrapped);
	free(filter);
}

file_source_filter_t *file_source_filter_create(file_source_t *wrapped)
{
	file_source_filter_t *filter = calloc(1, sizeof(*filter));
	file_source_t *source = (file_source_t *)filter;
	object_t *obj = (object_t *)filter;

	if (filter == NULL) {
		perror("creating file listing filter");
		return NULL;
	}

	filter->wrapped = object_grab(wrapped);
	source->get_next_record = get_next_record;
	obj->refcount = 1;
	obj->destroy = destroy;
	return filter;
}

int file_source_filter_add_glob_rule(file_source_filter_t *filter,
				     const char *pattern, int target)
{
	rule_t *r = calloc(1, sizeof(*r) + strlen(pattern) + 1);

	if (r == NULL) {
		perror("creating file listing filter rule");
		return -1;
	}

	strcpy(r->pattern, pattern);
	r->target = target;

	if (filter->rules == NULL) {
		filter->rules = r;
		filter->rules_last = r;
	} else {
		filter->rules_last->next = r;
		filter->rules_last = r;
	}

	return 0;
}
