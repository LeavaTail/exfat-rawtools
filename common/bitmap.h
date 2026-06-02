// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#ifndef _BITMAP_H
#define _BITMAP_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

typedef struct {
	uint8_t *data;
	size_t size;
} bitmap_t;

static inline int init_bitmap(bitmap_t *b, size_t s)
{
	size_t bytes = s / CHAR_BIT + !!(s % CHAR_BIT);

	if (!bytes)
		bytes = 1;
	b->data = calloc(bytes, sizeof(*b->data));
	if (!b->data)
		return -ENOMEM;
	b->size = s;

	return 0;
}

static inline int get_bitmap(const bitmap_t *b, size_t value)
{
	size_t offset;
	size_t shift;
	uint8_t mask;

	if (!b->data || value >= b->size)
		return -EINVAL;

	offset = value / CHAR_BIT;
	shift = value % CHAR_BIT;
	mask = 1 << shift;

	return b->data[offset] & mask;
}

static inline int set_bitmap(bitmap_t *b, size_t value)
{
	size_t offset;
	size_t shift;
	uint8_t mask;

	if (!b->data || value >= b->size)
		return -EINVAL;

	offset = value / CHAR_BIT;
	shift = value % CHAR_BIT;
	mask = 1 << shift;

	b->data[offset] |= mask;

	return 0;
}

static inline void free_bitmap(bitmap_t *b)
{
	free(b->data);
	b->data = NULL;
	b->size = 0;
}

#endif /*_BITMAP_H */
