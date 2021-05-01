// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#ifndef _STACK_H
#define _STACK_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define STACKSIZE 256

typedef struct {
	uint64_t *data;
	size_t tail;
	size_t size;
} stack_t;

static inline int init_stack(stack_t *s)
{
	s->data = calloc(STACKSIZE, sizeof(uint64_t));
	s->tail = 0;
	s->size = STACKSIZE;

	return s->data ? 0 : -1;
}

static inline int push(stack_t *s, uint64_t d)
{
	uint64_t *tmp;

	if (!d)
		return -1;

	if (s->tail >= s->size) {
		s->size += STACKSIZE;
		tmp = realloc(s->data, s->size);
		if (!tmp)
			return -1;

		s->data = tmp;
	}
	s->data[s->tail++] = d;

	return 0;
}

static inline uint64_t pop(stack_t *s)
{
	uint64_t ret;

	if (!s->tail)
		return 0;

	ret = s->data[--s->tail];
	return ret;
}

static inline void print_stack(stack_t *s)
{
	size_t i;
	for (i = 0; i < s->tail; i++)
		fprintf(stdout, "%lu ", s->data[i]);
	fprintf(stdout, "\n");
}

static inline void free_stack(stack_t *s)
{
	free(s->data);
}

#endif /*_STACK_H */
