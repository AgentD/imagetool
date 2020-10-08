/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bitmap.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "bitmap.h"

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#define MIN_BITMAP_WORDS (64)

typedef uint64_t bitmap_word_t;

struct bitmap_t {
	object_t base;

	size_t word_count;
	bitmap_word_t *words;
};

static void bitmap_destroy(object_t *base)
{
	free(((bitmap_t *)base)->words);
	free(base);
}

bitmap_t *bitmap_create(size_t bit_count)
{
	size_t bits_per_word = sizeof(bitmap_word_t) * CHAR_BIT;
	bitmap_t *bitmap = calloc(1, sizeof(*bitmap));

	if (bitmap == NULL) {
		perror("creating bitmap");
		return NULL;
	}

	if ((bit_count % bits_per_word) != 0)
		bit_count += bits_per_word - bit_count % bits_per_word;

	if ((bit_count / bits_per_word) < MIN_BITMAP_WORDS)
		bit_count = MIN_BITMAP_WORDS * bits_per_word;

	bitmap->word_count = bit_count / bits_per_word;

	bitmap->words = calloc(bitmap->word_count, sizeof(bitmap_word_t));
	if (bitmap->words == NULL) {
		perror("allocating bitmap");
		free(bitmap);
		return NULL;
	}

	((object_t *)bitmap)->refcount = 1;
	((object_t *)bitmap)->destroy = bitmap_destroy;
	return bitmap;
}

bool bitmap_is_set(bitmap_t *bitmap, size_t index)
{
	size_t word_idx, word_off;

	word_idx = index / (sizeof(bitmap->words[0]) * CHAR_BIT);
	word_off = index % (sizeof(bitmap->words[0]) * CHAR_BIT);

	if (word_idx >= bitmap->word_count)
		return false;

	return (bitmap->words[word_idx] & (1UL << word_off)) != 0;
}

int bitmap_set(bitmap_t *bitmap, size_t index)
{
	size_t word_idx = index / (sizeof(bitmap->words[0]) * CHAR_BIT);
	size_t word_off = index % (sizeof(bitmap->words[0]) * CHAR_BIT);
	size_t i, new_count;
	bitmap_word_t *new;

	if (word_idx >= bitmap->word_count) {
		new_count = bitmap->word_count * 2;

		while (word_idx >= new_count)
			new_count *= 2;

		new = realloc(bitmap->words,
			      new_count * sizeof(bitmap->words[0]));

		if (new == NULL) {
			perror("growing bitmap");
			return -1;
		}

		for (i = bitmap->word_count; i < new_count; ++i)
			new[i] = 0;

		bitmap->words = new;
		bitmap->word_count = new_count;
	}

	bitmap->words[word_idx] |= (1UL << word_off);
	return 0;
}

void bitmap_clear(bitmap_t *bitmap, size_t index)
{
	size_t word_idx = index / (sizeof(bitmap->words[0]) * CHAR_BIT);
	size_t word_off = index % (sizeof(bitmap->words[0]) * CHAR_BIT);

	if (word_idx < bitmap->word_count)
		bitmap->words[word_idx] &= ~(1UL << word_off);
}

int bitmap_set_value(bitmap_t *bitmap, size_t index, bool set)
{
	if (set)
		return bitmap_set(bitmap, index);

	bitmap_clear(bitmap, index);
	return 0;
}

void bitmap_shrink(bitmap_t *bitmap)
{
	void *new;
	size_t i;

	i = bitmap->word_count;

	while (i > 0 && bitmap->words[i - 1] == 0)
		--i;

	if (i > 0 && i <= bitmap->word_count / 2) {
		new = realloc(bitmap->words, i * sizeof(bitmap->words[0]));

		if (new != NULL) {
			bitmap->words = new;
			bitmap->word_count = i;
		}
	}
}

size_t bitmap_msb_index(bitmap_t *bitmap)
{
	bitmap_word_t word, mask;
	size_t i, index;

	if (bitmap->word_count == 0)
		return 0;

	i = bitmap->word_count - 1;
	while (i > 0 && bitmap->words[i] == 0)
		--i;

	word = bitmap->words[i];

	if (word == 0)
		return 0;

	index = i * (sizeof(bitmap->words[0]) * CHAR_BIT);

	mask = ~(1UL << (sizeof(bitmap->words[0]) * CHAR_BIT - 1));

	while (word != 0x01) {
		word = (word >> 1) & mask;
		index += 1;
	}

	return index;
}
