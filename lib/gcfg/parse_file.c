/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * parse_file.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "gcfg.h"

#include <string.h>

static const char *skip_space(const char *ptr)
{
	while (*ptr == ' ' || *ptr == '\t')
		++ptr;
	return ptr;
}

static int is_line_end(int c)
{
	return c == '\0' || c == '#';
}

static const char *find_keyword(gcfg_file_t *file,
				const gcfg_keyword_t *keywords,
				const char *ptr,
				const gcfg_keyword_t **out)
{
	const gcfg_keyword_t *kwd;
	size_t len;

	if (!(*ptr >= 'a' && *ptr <= 'z') &&
	    !(*ptr >= 'A' && *ptr <= 'Z') &&
	    !(*ptr >= '0' && *ptr <= '9')) {
		file->report_error(file, "Expected keyword, found '%.6s...'",
				   ptr);
		return NULL;
	}

	for (kwd = keywords; kwd != NULL; kwd = kwd->next) {
		len = strlen(kwd->name);

		if (strncmp(ptr, kwd->name, len) != 0)
			continue;

		if (ptr[len] == ' ' || ptr[len] == '\t' ||
		    is_line_end(ptr[len])) {
			*out = kwd;
			return skip_space(ptr + len);
		}
	}

	file->report_error(file, "Unknown keyword '%.6s...'", ptr);
	return NULL;
}

static const char *apply_arg(gcfg_file_t *file, const gcfg_keyword_t *kwd,
			     const char *ptr, object_t *parent,
			     object_t **child_out)
{
	gcfg_number_t num[4];
	uint64_t lval;
	char *strval;
	int iret;

	switch (kwd->arg) {
	case GCFG_ARG_NONE:
		*child_out = kwd->handle.cb_none(kwd, file, parent);
		break;
	case GCFG_ARG_BOOLEAN:
		ptr = gcfg_parse_boolean(file, ptr, &iret);
		if (ptr == NULL)
			return NULL;
		*child_out = kwd->handle.cb_bool(kwd, file, parent, iret != 0);
		break;
	case GCFG_ARG_STRING:
		strval = file->buffer;
		ptr = gcfg_parse_string(file, ptr, strval);
		if (ptr == NULL)
			return NULL;
		*child_out = kwd->handle.cb_string(kwd, file, parent, strval);
		break;
	case GCFG_ARG_ENUM:
		ptr = gcfg_parse_enum(file, ptr, kwd->option.enumtokens, &iret);
		if (ptr == NULL)
			return NULL;
		*child_out = kwd->handle.cb_enum(kwd, file, parent, iret);
		break;
	case GCFG_ARG_NUMBER:
		ptr = gcfg_parse_number(file, ptr, num);
		if (ptr == NULL)
			return NULL;
		*child_out = kwd->handle.cb_number(kwd, file, parent, num, 1);
		break;
	case GCFG_ARG_SIZE:
		ptr = gcfg_parse_size(file, ptr, &lval);
		if (ptr == NULL)
			return NULL;
		*child_out = kwd->handle.cb_size(kwd, file, parent, lval);
		break;
	case GCFG_ARG_VEC2:
		ptr = gcfg_parse_vector(file, ptr, 2, num);
		if (ptr == NULL)
			return NULL;
		*child_out = kwd->handle.cb_number(kwd, file, parent, num, 2);
		break;
	case GCFG_ARG_VEC3:
		ptr = gcfg_parse_vector(file, ptr, 3, num);
		if (ptr == NULL)
			return NULL;
		*child_out = kwd->handle.cb_number(kwd, file, parent, num, 3);
		break;
	case GCFG_ARG_VEC4:
		ptr = gcfg_parse_vector(file, ptr, 4, num);
		if (ptr == NULL)
			return NULL;
		*child_out = kwd->handle.cb_number(kwd, file, parent, num, 4);
		break;
	default:
		file->report_error(file,
				   "[BUG] unknown argument type for '%s'",
				   kwd->name);
		return NULL;
	}

	return skip_space(ptr);
}

static int plain_listing(gcfg_file_t *file, const gcfg_keyword_t *keyword,
			 object_t *object)
{
	const char *ptr;
	int ret;

	for (;;) {
		ret = file->fetch_line(file);
		if (ret < 0)
			return -1;
		if (ret > 0)
			goto fail_closeing;

		ptr = file->buffer;
		if (!gcfg_is_valid_utf8((const uint8_t *)ptr, strlen(ptr)))
			goto fail_utf8;

		ret = keyword->handle_listing(file, object, ptr);
		if (ret < 0)
			return -1;

		if (ret > 0) {
			ptr = skip_space(ptr);
			if (*ptr != '}')
				goto fail_closeing;

			ptr = skip_space(ptr + 1);
			if (!is_line_end(*ptr))
				goto fail_brace_extra;
			break;
		}
	}
	return 0;
fail_closeing:
	file->report_error(file, "missing '}' after listing");
	return -1;
fail_utf8:
	file->report_error(file, "encoding error (expected UTF-8)");
	return -1;
fail_brace_extra:
	file->report_error(file, "'{' or '}' must be followed by a line break");
	return -1;
}

static const char *apply_reflection_arg(gcfg_file_t *file, const char *ptr,
					object_t *obj, size_t idx,
					property_desc_t *desc)
{
	property_value_t value;
	uint32_t u32val;
	char *strval;
	int iret;

	value.type = desc->type;

	switch (desc->type) {
	case PROPERTY_TYPE_STRING:
		strval = file->buffer;
		ptr = gcfg_parse_string(file, ptr, strval);
		if (ptr == NULL)
			return NULL;
		value.value.string = strval;
		break;
	case PROPERTY_TYPE_BOOL:
		ptr = gcfg_parse_boolean(file, ptr, &iret);
		if (ptr == NULL)
			return NULL;
		value.value.boolean = iret;
		break;
	case PROPERTY_TYPE_U32_NUMBER:
		ptr = gcfg_dec_num(file, ptr, &value.value.u64, 0x0FFFFFFFFUL);
		if (ptr == NULL)
			return NULL;
		u32val = value.value.u64 & 0x0FFFFFFFFUL;
		value.value.u32 = u32val;
		break;
	case PROPERTY_TYPE_U64_NUMBER:
		ptr = gcfg_dec_num(file, ptr, &value.value.u64, ~(0UL));
		if (ptr == NULL)
			return NULL;
		break;
	case PROPERTY_TYPE_U64_SIZE:
		ptr = gcfg_parse_size(file, ptr, &value.value.u64);
		if (ptr == NULL)
			return NULL;
		break;
	case PROPERTY_TYPE_NONE:
	default:
		goto fail_type;
	}

	if (object_set_property(obj, idx, &value))
		goto fail_set;

	return skip_space(ptr);
fail_set:
	file->report_error(file, "error setting property '%s'", desc->name);
	return NULL;
fail_type:
	file->report_error(file, "[BUG] unknown data type for '%s'",
			   desc->name);
	return NULL;
}

static int parse(gcfg_file_t *file, const gcfg_keyword_t *keywords,
		 object_t *parent, unsigned int level)
{
	const gcfg_keyword_t *kwd;
	const char *name;
	const char *ptr;
	object_t *child;
	bool have_args;
	int ret;

	for (;;) {
		ret = file->fetch_line(file);
		if (ret < 0)
			return -1;
		if (ret > 0) {
			if (level != 0)
				goto fail_closeing;
			break;
		}

		if (!gcfg_is_valid_utf8((const uint8_t *)file->buffer,
					strlen(file->buffer))) {
			goto fail_utf8;
		}

		kwd = NULL;
		child = NULL;
		ptr = skip_space(file->buffer);

		if (parent != NULL) {
			size_t i, len, count;
			property_desc_t desc;

			count = object_get_property_count(parent);

			for (i = 0; i < count; ++i) {
				int ret = object_get_property_desc(parent, i,
								   &desc);

				name = desc.name;
				if (ret)
					continue;

				len = strlen(name);
				if (strncmp(ptr, name, len) != 0)
					continue;

				if (ptr[len] == ' ' || ptr[len] == '\t' ||
				    is_line_end(ptr[len])) {
					break;
				}
			}

			if (i < count) {
				ptr = skip_space(ptr + len);
				ptr = apply_reflection_arg(file, ptr, parent,
							   i, &desc);
				if (ptr == NULL)
					return -1;

				if (*ptr == '{') {
					goto fail_children;
				} else if (!is_line_end(*ptr)) {
					goto fail_kwd_extra;
				} else {
					continue;
				}
			}
		}

		if ((*ptr >= 'a' && *ptr <= 'z') ||
		    (*ptr >= 'A' && *ptr <= 'Z')) {
			ptr = find_keyword(file, keywords, ptr, &kwd);
			if (ptr == NULL)
				return -1;

			have_args = !is_line_end(*ptr) && *ptr != '{' &&
				    *ptr != '}';

			if (!have_args && kwd->arg != GCFG_ARG_NONE)
				goto fail_missing_arg;

			if (have_args && kwd->arg == GCFG_ARG_NONE)
				goto fail_have_arg;

			ptr = apply_arg(file, kwd, ptr, parent, &child);
			if (ptr == NULL || child == NULL)
				return -1;

			if (*ptr == '{') {
				ptr = skip_space(ptr + 1);
				if (!is_line_end(*ptr)) {
					object_drop(child);
					goto fail_brace_extra;
				}
				if (kwd->handle_listing != NULL) {
					if (plain_listing(file, kwd, child)) {
						object_drop(child);
						return -1;
					}
				} else {
					if (kwd->children == NULL) {
						object_drop(child);
						name = kwd->name;
						goto fail_children;
					}
					if (parse(file, kwd->children,
						  child, level + 1)) {
						object_drop(child);
						return -1;
					}
				}
			} else if (!is_line_end(*ptr)) {
				object_drop(child);
				name = kwd->name;
				goto fail_kwd_extra;
			}

			if (kwd->finalize_object != NULL) {
				if (kwd->finalize_object(file, child)) {
					object_drop(child);
					return -1;
				}
			}

			child = object_drop(child);
		} else if (*ptr == '}') {
			if (level == 0)
				goto fail_level;
			ptr = skip_space(ptr + 1);
			if (!is_line_end(*ptr))
				goto fail_brace_extra;
			break;
		} else if (!is_line_end(*ptr)) {
			file->report_error(file, "Unexpected %.6s...", ptr);
			return -1;
		}
	}

	return 0;
fail_closeing:
	file->report_error(file, "missing '}' before end-of-file");
	return -1;
fail_utf8:
	file->report_error(file, "encoding error (expected UTF-8)");
	return -1;
fail_kwd_extra:
	if (kwd != NULL && kwd->arg == GCFG_ARG_NONE) {
		file->report_error(file, "'%s' must be folled "
				   "by a line break", name);
	} else {
		file->report_error(file, "'%s <argument>' must be folled "
				   "by a line break", name);
	}
	return -1;
fail_brace_extra:
	file->report_error(file, "'{' or '}' must be followed by a line break");
	return -1;
fail_level:
	file->report_error(file, "Unexpected '}' outside block");
	return -1;
fail_children:
	file->report_error(file, "Unexpected '{' after %s", name);
	return -1;
fail_missing_arg:
	file->report_error(file, "Missing argument after %s", kwd->name);
	return -1;
fail_have_arg:
	file->report_error(file, "%s cannot have any arguments", kwd->name);
	return -1;
}

int gcfg_parse_file(gcfg_file_t *file, const gcfg_keyword_t *keywords,
		    object_t *usr)
{
	return parse(file, keywords, usr, 0);
}
