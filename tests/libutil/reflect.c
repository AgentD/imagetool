/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * reflect.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "predef.h"
#include "test.h"

typedef struct {
	object_t base;
	uint32_t x;
	uint32_t y;
} shape_t;

typedef struct {
	shape_t base;
	uint32_t width;
	uint32_t height;
} rectangle_t;

typedef struct {
	shape_t base;
	uint32_t radius;
} circle_t;

/*****************************************************************************/

enum {
	SHAPE_PROP_X = 0,
	SHAPE_PROP_Y,
	SHAPE_PROP_COUNT,
};

static size_t shape_get_property_count(const meta_object_t *meta)
{
	(void)meta;
	return SHAPE_PROP_COUNT;
}

static int shape_get_property_desc(const meta_object_t *meta, size_t i,
				   PROPERTY_TYPE *type, const char **name)
{
	(void)meta;
	switch (i) {
	case SHAPE_PROP_X:
		*type = PROPERTY_TYPE_U32_NUMBER;
		*name = "x";
		break;
	case SHAPE_PROP_Y:
		*type = PROPERTY_TYPE_U32_NUMBER;
		*name = "y";
		break;
	default:
		return -1;
	}
	return 0;
}

static int shape_set_property(const meta_object_t *meta, size_t i,
			      object_t *obj, const property_value_t *value)
{
	(void)meta;
	switch (i) {
	case SHAPE_PROP_X:
		if (value->type != PROPERTY_TYPE_U32_NUMBER)
			return -1;
		((shape_t *)obj)->x = value->value.u32;
		break;
	case SHAPE_PROP_Y:
		if (value->type != PROPERTY_TYPE_U32_NUMBER)
			return -1;
		((shape_t *)obj)->y = value->value.u32;
		break;
	default:
		return -1;
	}
	return 0;
}

static int shape_get_property(const meta_object_t *meta, size_t i,
			      const object_t *obj, property_value_t *value)
{
	(void)meta;
	switch (i) {
	case SHAPE_PROP_X:
		value->type = PROPERTY_TYPE_U32_NUMBER;
		value->value.u32 = ((shape_t *)obj)->x;
		break;
	case SHAPE_PROP_Y:
		value->type = PROPERTY_TYPE_U32_NUMBER;
		value->value.u32 = ((shape_t *)obj)->y;
		break;
	default:
		return -1;
	}
	return 0;
}

static const meta_object_t meta_shape = {
	.name = "shape_t",
	.parent = NULL,
	.get_property_count = shape_get_property_count,
	.get_property_desc = shape_get_property_desc,
	.set_property = shape_set_property,
	.get_property = shape_get_property,
};

/*****************************************************************************/

enum {
	RECT_PROP_WIDTH = 0,
	RECT_PROP_HEIGHT,
	RECT_PROP_COUNT,
};

static size_t rect_get_property_count(const meta_object_t *meta)
{
	(void)meta;
	return RECT_PROP_COUNT;
}

static int rect_get_property_desc(const meta_object_t *meta, size_t i,
				  PROPERTY_TYPE *type, const char **name)
{
	(void)meta;
	switch (i) {
	case RECT_PROP_WIDTH:
		*type = PROPERTY_TYPE_U32_NUMBER;
		*name = "width";
		break;
	case RECT_PROP_HEIGHT:
		*type = PROPERTY_TYPE_U32_NUMBER;
		*name = "height";
		break;
	default:
		return -1;
	}
	return 0;
}

static int rect_set_property(const meta_object_t *meta, size_t i,
			     object_t *obj, const property_value_t *value)
{
	(void)meta;
	switch (i) {
	case RECT_PROP_WIDTH:
		if (value->type != PROPERTY_TYPE_U32_NUMBER)
			return -1;
		((rectangle_t *)obj)->width = value->value.u32;
		break;
	case RECT_PROP_HEIGHT:
		if (value->type != PROPERTY_TYPE_U32_NUMBER)
			return -1;
		((rectangle_t *)obj)->height = value->value.u32;
		break;
	default:
		return -1;
	}
	return 0;
}

static int rect_get_property(const meta_object_t *meta, size_t i,
			     const object_t *obj, property_value_t *value)
{
	(void)meta;
	switch (i) {
	case RECT_PROP_WIDTH:
		value->type = PROPERTY_TYPE_U32_NUMBER;
		value->value.u32 = ((rectangle_t *)obj)->width;
		break;
	case RECT_PROP_HEIGHT:
		value->type = PROPERTY_TYPE_U32_NUMBER;
		value->value.u32 = ((rectangle_t *)obj)->height;
		break;
	default:
		return -1;
	}
	return 0;
}

static const meta_object_t meta_rect = {
	.name = "rectangle_t",
	.parent = &meta_shape,
	.get_property_count = rect_get_property_count,
	.get_property_desc = rect_get_property_desc,
	.set_property = rect_set_property,
	.get_property = rect_get_property,
};

/*****************************************************************************/

enum {
	CIRCLE_PROP_RADIUS = 0,
	CIRCLE_PROP_COUNT,
};

static size_t circle_get_property_count(const meta_object_t *meta)
{
	(void)meta;
	return CIRCLE_PROP_COUNT;
}

static int circle_get_property_desc(const meta_object_t *meta, size_t i,
				    PROPERTY_TYPE *type, const char **name)
{
	(void)meta;
	switch (i) {
	case CIRCLE_PROP_RADIUS:
		*type = PROPERTY_TYPE_U32_NUMBER;
		*name = "radius";
		break;
	default:
		return -1;
	}
	return 0;
}

static int circle_set_property(const meta_object_t *meta, size_t i,
			       object_t *obj, const property_value_t *value)
{
	(void)meta;
	switch (i) {
	case CIRCLE_PROP_RADIUS:
		if (value->type != PROPERTY_TYPE_U32_NUMBER)
			return -1;
		((circle_t *)obj)->radius = value->value.u32;
		break;
	default:
		return -1;
	}
	return 0;
}

static int circle_get_property(const meta_object_t *meta, size_t i,
			       const object_t *obj, property_value_t *value)
{
	(void)meta;
	switch (i) {
	case CIRCLE_PROP_RADIUS:
		value->type = PROPERTY_TYPE_U32_NUMBER;
		value->value.u32 = ((circle_t *)obj)->radius;
		break;
	default:
		return -1;
	}
	return 0;
}

static const meta_object_t meta_circle = {
	.name = "circle_t",
	.parent = &meta_shape,
	.get_property_count = circle_get_property_count,
	.get_property_desc = circle_get_property_desc,
	.set_property = circle_set_property,
	.get_property = circle_get_property,
};

/*****************************************************************************/

static void generic_destroy(object_t *obj)
{
	free(obj);
}

static rectangle_t *create_rectangle(uint32_t x, uint32_t y,
				     uint32_t width, uint32_t height)
{
	rectangle_t *r = calloc(1, sizeof(*r));

	if (r != NULL) {
		r->width = width;
		r->height = height;
		((shape_t *)r)->x = x;
		((shape_t *)r)->y = y;
		((object_t *)r)->meta = &meta_rect;
		((object_t *)r)->refcount = 1;
		((object_t *)r)->destroy = generic_destroy;
	}

	return r;
}

static circle_t *create_circle(uint32_t x, uint32_t y, uint32_t radius)
{
	circle_t *c = calloc(1, sizeof(*c));

	if (c != NULL) {
		c->radius = radius;
		((shape_t *)c)->x = x;
		((shape_t *)c)->y = y;
		((object_t *)c)->meta = &meta_circle;
		((object_t *)c)->refcount = 1;
		((object_t *)c)->destroy = generic_destroy;
	}

	return c;
}

/*****************************************************************************/

int main(void)
{
	property_value_t value;
	PROPERTY_TYPE type;
	const char *name;
	object_t *r, *c;
	size_t count;
	int ret;

	r = (object_t *)create_rectangle(13, 37, 42, 69);
	TEST_NOT_NULL(r);

	c = (object_t *)create_circle(47, 11, 42);
	TEST_NOT_NULL(c);

	/* inspect the rectangle */
	TEST_ASSERT(object_reflect(r) == &meta_rect);

	count = object_get_property_count(r);
	TEST_EQUAL_UI(count, 4);

	ret = object_get_property_desc(r, 0, &type, &name);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(type, PROPERTY_TYPE_U32_NUMBER);
	TEST_STR_EQUAL(name, "width");

	ret = object_get_property(r, 0, &value);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(value.type, PROPERTY_TYPE_U32_NUMBER);
	TEST_EQUAL_UI(value.value.u32, 42);

	ret = object_get_property_desc(r, 1, &type, &name);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(type, PROPERTY_TYPE_U32_NUMBER);
	TEST_STR_EQUAL(name, "height");

	ret = object_get_property(r, 1, &value);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(value.type, PROPERTY_TYPE_U32_NUMBER);
	TEST_EQUAL_UI(value.value.u32, 69);

	ret = object_get_property_desc(r, 2, &type, &name);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(type, PROPERTY_TYPE_U32_NUMBER);
	TEST_STR_EQUAL(name, "x");

	ret = object_get_property(r, 2, &value);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(value.type, PROPERTY_TYPE_U32_NUMBER);
	TEST_EQUAL_UI(value.value.u32, 13);

	ret = object_get_property_desc(r, 3, &type, &name);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(type, PROPERTY_TYPE_U32_NUMBER);
	TEST_STR_EQUAL(name, "y");

	ret = object_get_property(r, 3, &value);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(value.type, PROPERTY_TYPE_U32_NUMBER);
	TEST_EQUAL_UI(value.value.u32, 37);

	ret = object_get_property_desc(r, 4, &type, &name);
	TEST_ASSERT(ret < 0);

	/* inspect the circle */
	TEST_ASSERT(object_reflect(c) == &meta_circle);

	count = object_get_property_count(c);
	TEST_EQUAL_UI(count, 3);

	ret = object_get_property_desc(c, 0, &type, &name);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(type, PROPERTY_TYPE_U32_NUMBER);
	TEST_STR_EQUAL(name, "radius");

	ret = object_get_property(c, 0, &value);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(value.type, PROPERTY_TYPE_U32_NUMBER);
	TEST_EQUAL_UI(value.value.u32, 42);

	ret = object_get_property_desc(c, 1, &type, &name);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(type, PROPERTY_TYPE_U32_NUMBER);
	TEST_STR_EQUAL(name, "x");

	ret = object_get_property(c, 1, &value);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(value.type, PROPERTY_TYPE_U32_NUMBER);
	TEST_EQUAL_UI(value.value.u32, 47);

	ret = object_get_property_desc(c, 2, &type, &name);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(type, PROPERTY_TYPE_U32_NUMBER);
	TEST_STR_EQUAL(name, "y");

	ret = object_get_property(c, 2, &value);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(value.type, PROPERTY_TYPE_U32_NUMBER);
	TEST_EQUAL_UI(value.value.u32, 11);

	ret = object_get_property_desc(c, 3, &type, &name);
	TEST_ASSERT(ret < 0);

	/* change the circle properties */
	value.type = PROPERTY_TYPE_U32_NUMBER;
	value.value.u32 = 17;
	ret = object_set_property(c, 0, &value);
	TEST_EQUAL_I(ret, 0);

	value.type = PROPERTY_TYPE_U32_NUMBER;
	value.value.u32 = 13;
	ret = object_set_property(c, 1, &value);
	TEST_EQUAL_I(ret, 0);

	value.type = PROPERTY_TYPE_U32_NUMBER;
	value.value.u32 = 37;
	ret = object_set_property(c, 2, &value);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(((circle_t *)c)->radius, 17);
	TEST_EQUAL_UI(((shape_t *)c)->x, 13);
	TEST_EQUAL_UI(((shape_t *)c)->y, 37);

	/* cleanup */
	object_drop(r);
	object_drop(c);
	return EXIT_SUCCESS;
}
