// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <mntent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "checkexfat.h"
#include "exfat.h"

FILE *output = NULL;
unsigned int print_level = PRINT_WARNING;
struct exfat_info info = {0};

/**
 * Special Option(no short option)
 */
enum
{
	GETOPT_HELP_CHAR = (CHAR_MIN - 2),
	GETOPT_VERSION_CHAR = (CHAR_MIN - 3)
};

/* option data {"long name", needs argument, flags, "short name"} */
static struct option const longopts[] =
{
	{"help", no_argument, NULL, GETOPT_HELP_CHAR},
	{"version", no_argument, NULL, GETOPT_VERSION_CHAR},
	{0,0,0,0}
};

/**
 * usage - print out usage
 */
static void usage(void)
{
	fprintf(stderr, "Usage: %s [OPTION]... FILE\n", PROGRAM_NAME);
	fprintf(stderr, "Write any data to exfat image\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  --help\tdisplay this help and exit.\n");
	fprintf(stderr, "  --version\toutput version information and exit.\n");
	fprintf(stderr, "\n");
}

/**
 * version        - print out program version
 * @command_name:   command name
 * @version:        program version
 * @author:         program authoer
 */
static void version(const char *command_name, const char *version, const char *author)
{
	fprintf(stdout, "%s %s\n", command_name, version);
	fprintf(stdout, "\n");
	fprintf(stdout, "Written by %s.\n", author);
}

/**
 * exfat_set_bitmap - set bit at @clu in @b
 * @b                 Allocation bitmap cache
 * @clu               cluster
 *
 * @return             0 (success)
 *                    -EINVAL (already allocated)
 */
static int exfat_set_bitmap(uint8_t *b, uint32_t clu)
{
	int ret = 0;
	int offset, byte;
	uint8_t mask = 0x01;

	byte = (clu - EXFAT_FIRST_CLUSTER) / CHAR_BIT;
	offset = (clu - EXFAT_FIRST_CLUSTER) % CHAR_BIT;
	mask <<= offset;

	if (b[byte] & mask) {
		pr_warn("Cluster#%u is referenced from other cluster.\n", clu);
		ret = -EINVAL;
	} else {
		b[byte] = b[byte] | mask;
	}

	return ret;
}

/**
 * exfat_set_reserve_bitmap - set bit at Allocation bitmap, Upcase-table, Root Directory in @b
 * @b                         Allocation bitmap cache
 * @clu                       cluster index
 * @len                       data length
 *
 * @return            0 (success)
 */
static int exfat_set_reserved_bitmap(uint8_t *b, uint32_t clu, uint64_t len)
{
	int ret = 0;
	struct exfat_fileinfo f = {0};

	f.clu = clu;
	f.datalen = len;

	while (clu != 0 && clu != EXFAT_LASTCLUSTER) {
		ret = exfat_set_bitmap(b, clu);
		clu = exfat_next_cluster(&f, clu);
	}
	return ret;
}

/**
 * exfat_check_bitmap - check whether Freed cluster is fine in Allocation bitmap 
 * @b                   Allocation bitmap cache
 *
 * @return            0 (success)
 */
static int exfat_check_bitmap(uint8_t *b)
{
	int i, j;
	int ret = 0;

	for (i = 0; i < info.cluster_count / CHAR_BIT; i++) {
		for (j = 0; j < CHAR_BIT; j++) {
			if (((b[i] >> j) & 0x01) ^ ((info.alloc_table[i] >> j) & 0x01)) {
				pr_warn("Cluster#%u isn't used at all.\n", EXFAT_FIRST_CLUSTER + i * CHAR_BIT + j);
				ret = -EINVAL;
			}
		}
	}
	pr_msg("\n");
	return ret;
}

/**
 * main   - main function
 * @argc:   argument count
 * @argv:   argument vector
 */
int main(int argc, char *argv[])
{
	int opt;
	int longindex;
	int i;
	int ret = -EINVAL;
	uint32_t clu = 0;
	struct exfat_bootsec boot;
	uint8_t *alloc_table = NULL;
	node2_t *tmp;
	struct exfat_fileinfo *f;

	while ((opt = getopt_long(argc, argv,
					"",
					longopts, &longindex)) != -1) {
		switch (opt) {
			case GETOPT_HELP_CHAR:
				usage();
				exit(EXIT_SUCCESS);
			case GETOPT_VERSION_CHAR:
				version(PROGRAM_NAME, PROGRAM_VERSION, PROGRAM_AUTHOR);
				exit(EXIT_SUCCESS);
			default:
				usage();
				exit(EXIT_FAILURE);
		}
	}

#ifdef EXFAT_DEBUG
	print_level = PRINT_DEBUG;
#endif

	if (optind != argc - 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	output = stdout;
	if (exfat_init_info())
		goto out;

	if ((info.fd = open(argv[optind], O_RDONLY)) < 0) {
		pr_err("open: %s\n", strerror(errno));
		ret = -EIO;
		goto out;
	}

	if (exfat_load_bootsec(&boot)) 
		goto out;
	if (exfat_store_info(&boot))
		goto out;
	if (exfat_check_extend_bootsec())
		goto out;
	if (exfat_check_bootchecksum())
		goto out;

	/* Ignore errot message in Root Directory */
	if (exfat_traverse_root_directory())
		goto out;
	f = (struct exfat_fileinfo *)info.root[0]->data;

	alloc_table = calloc(info.cluster_size, 1);
	if (!alloc_table)
		goto fat_free;

	exfat_set_reserved_bitmap(alloc_table, info.alloc_offset, info.alloc_length);
	exfat_set_reserved_bitmap(alloc_table, info.upcase_offset, info.upcase_size);
	exfat_set_reserved_bitmap(alloc_table, info.root_offset, f->datalen);

	for (i = 0; i < info.root_size && info.root[i]; i++) {
		exfat_traverse_directory(info.root[i]->index);
		tmp = info.root[i];
		clu = tmp->index;
		/* Traverse directory chain */
		while (tmp->next != NULL) {
			tmp = tmp->next;
			f = (struct exfat_fileinfo *)tmp->data;
			/* File */
			for (clu = tmp->index;
					clu != 0 && clu != EXFAT_LASTCLUSTER;
					clu = exfat_next_cluster(f, clu))
				exfat_set_bitmap(alloc_table, clu);
		}
	}
	exfat_check_bitmap(alloc_table);
	exfat_print_cache();

	ret = EXIT_SUCCESS;
fat_free:
	free(alloc_table);
out:
	exfat_clean_info();
	return ret;
}
