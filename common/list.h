// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#ifndef _LIST_H
#define _LIST_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct node {
	uint64_t index;
	struct node *next;
} node_t;

static inline int init_node(node_t **head)
{
	return 0;
}

static inline void insert_node(node_t **head, uint64_t i)
{
	node_t *node;

	if (*head == NULL) {
		node = malloc(sizeof(node_t));
		node->index = i;
		node->next = NULL;
		*head = node;
	} else {
		node = malloc(sizeof(node_t));
		node->index = i;
		node->next = (*head)->next;
		(*head)->next = node;
	}
}

static inline void append_node(node_t **head, uint64_t i)
{
	node_t *node;
	node_t *tail = *head;

	if (*head == NULL) {
		node = malloc(sizeof(node_t));
		node->index = i;
		node->next = NULL;
		*head = node;
		return;
	} 

	while (tail->next != NULL)
		tail = tail->next;

	node = malloc(sizeof(node_t));
	node->index = i;
	node->next = tail->next;
	tail->next = node;
}

static inline int delete_node(node_t **head, uint64_t min, uint64_t max)
{
	int count = 0;
	node_t *tmp;

	while (*head != NULL) {
		if (((*head)->index >= min) && ((*head)->index < max)) {
			tmp = *head;
			*head = (*head)->next;
			free(tmp);
			count++;
		} else {
			head = &((*head)->next);
		}
	}
	return count;
}

static inline void free_node(node_t **head)
{
	node_t *tmp;

	if (!head && !(*head))
		return;
	tmp = *head;

	while (tmp != NULL) {
		(*head)->next = tmp->next;
		free(tmp);
		tmp = (*head)->next;
	}

	free(*head);
	*head = NULL;
}

static inline void print_node(node_t *node)
{
	while (node != NULL) {
		fprintf(stdout, "%lu -> ", node->index);
		node = node->next;
	}
	fprintf(stdout, "NULL\n");
}

#endif /*_LIST_H */
