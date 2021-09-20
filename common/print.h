// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#ifndef _PRINT_H
#define _PRINT_H

#include <stdio.h>
#include <inttypes.h>
/**
 * Debug code
 */
extern unsigned int print_level;
extern FILE *output;
#define PRINT_ERR      1
#define PRINT_WARNING  2
#define PRINT_INFO     3
#define PRINT_DEBUG    4

#define print(level, fmt, ...) \
	do { \
		if (print_level >= level) { \
			if (level == PRINT_DEBUG) \
			fprintf( output, "(%s:%u): " fmt, \
					__func__, __LINE__, ##__VA_ARGS__); \
			else \
			fprintf( output, "" fmt, ##__VA_ARGS__); \
		} \
	} while (0) \

#define pr_err(fmt, ...)   print(PRINT_ERR, fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)  print(PRINT_WARNING, fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)  print(PRINT_INFO, fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) print(PRINT_DEBUG, fmt, ##__VA_ARGS__)
#define pr_msg(fmt, ...)   fprintf(output, fmt, ##__VA_ARGS__)

void hexdump(void *data, size_t size);
int allwrite(int, void *, size_t);
int allread(int, void *, size_t);

#endif // _PRINT_H
