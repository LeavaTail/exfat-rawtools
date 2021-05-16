// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <error.h>
#include "print.h"

/**
 * hexdump - Hex dump of a given data
 * @data:    Input data
 * @size:    Input data size
 */
void hexdump(void *data, size_t size)
{
	unsigned long skip = 0;
	size_t line, byte = 0;
	size_t count = size / 0x10;
	const char zero[0x10] = {0};

	for (line = 0; line < count; line++) {
		if ((line != count - 1) && (!memcmp(data + line * 0x10, zero, 0x10))) {
			switch (skip++) {
				case 0:
					break;
				case 1:
					pr_msg("*\n");
					/* FALLTHROUGH */
				default:
					continue;
			}
		} else {
			skip = 0;
		}

		pr_msg("%08lX:  ", line * 0x10);
		for (byte = 0; byte < 0x10; byte++) {
			pr_msg("%02X ", ((unsigned char *)data)[line * 0x10 + byte]);
		}
		putchar(' ');
		for (byte = 0; byte < 0x10; byte++) {
			char ch = ((unsigned char *)data)[line * 0x10 + byte];
			pr_msg("%c", isprint(ch) ? ch : '.');
		}
		pr_msg("\n");
	}
}

/**
 * allwrite - write all data
 * @fd        Output file discriptor
 * @buf:      data
 * @count:    data size
 *
 * @return    == 0 (success)
 *            <  0 (Failed)
 */
int allwrite(int fd, void *buf, size_t count)
{
	ssize_t n = 0;
	ssize_t total = 0;

	while (count) {
		if ((n = write(fd, buf, count)) < 0)
			return n;

		total += n;
		buf += n;
		count -=n;
	}

	return 0;
}

/**
 * allread  - read all data
 * @fd        Input file discriptor
 * @buf:      data
 * @count:    data size
 *
 * @return    == 0 (success)
 *            <  0 (Failed)
 */
int allread(int fd, void *buf, size_t count)
{
	ssize_t n = 0;
	ssize_t total = 0;

	while (count) {
		if ((n = read(fd, buf, count)) < 0)
			return n;

		total += n;
		buf += n;
		count -=n;
	}

	return 0;
}
