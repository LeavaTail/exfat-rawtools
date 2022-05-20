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

#include "statexfat.h"
#include "exfat.h"

FILE *output;
unsigned int print_level = PRINT_WARNING;
struct exfat_info info;
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
	fprintf(stderr, "Usage: %s [OPTION]... IMAGE FILE\n", PROGRAM_NAME);
	fprintf(stderr, "display file status in exFAT\n");
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
 * exfat_get_fileinfo - Get exfat_fileinfo from cache index
 * @clu:                cluster index
 * @index:              cache index
 * @path:               file path
 * @f:                  file information (Output)
 */
static struct exfat_fileinfo *exfat_get_fileinfo(uint32_t clu, int index, char *path)
{
	int i;
	uint32_t p_clu;
	node2_t *tmp;

	tmp = info.root[index];

	if (path[strlen(path) - 1] == '/') {
		pr_err("'%s': No such file.\n", path);
		return NULL;
	}
	/* obtain parent directory*/
	for (i = strlen(path); i > 0 && path[i] != '/'; i--);
	path[i] = '\0';

	if ((p_clu = exfat_lookup(info.root_offset, path)) == 0)
		return NULL;
	tmp = info.root[exfat_get_cache(p_clu)];

	while (tmp->next != NULL) {
		tmp = tmp->next;
		if (tmp->index == clu) { 
			return (struct exfat_fileinfo *)tmp->data;
		}
	}

	return NULL;
}

/**
 * exfat_calculate_fragment - calculate FAT chain fragment
 * @f:                        file information
 */
double exfat_calculate_fragment(struct exfat_fileinfo *f)
{
	int i;
	uint32_t clu, next = 0;
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);
	long long sum = cluster_num * (info.cluster_count - 2);
	double weight = 0;

	if (f->flags & ALLOC_NOFATCHAIN)
		return 0;

	if (cluster_num <= 1)
		return 0;

	for (i = 0, clu = f->clu;
		i < cluster_num && clu != EXFAT_LASTCLUSTER;
		i++, clu = next) {

		if (exfat_get_fat(clu, &next))
			break;
		
		if (next == EXFAT_LASTCLUSTER)
			break;

		/* Continuous */
		if (next == clu + 1)
			continue;
		if (next - clu > 0)
			weight += next - clu - 1;
		if (next - clu < 0)
			weight += info.cluster_count - (clu - next) - 1;
	}

	return weight * 100 / sum;
}

/**
 * exfat_stat_file - print file status in exFAT
 * @f:               file information
 * @clu:             cluster index
 */
void exfat_stat_file(struct exfat_fileinfo *f)
{
	pr_msg("%-8s: %s\n", "File", f->name);
	pr_msg("%-8s: %lu\n", "Size", f->datalen);
	pr_msg("%-8s: %lu (Flagment: %.8lf%%)\n", "Cluster",
			ROUNDUP(f->datalen, info.cluster_size),
			exfat_calculate_fragment(f));
	pr_msg("%-8s: 0x%08x\n", "First", f->clu);
	pr_msg("%-8s: %c%c%c%c%c\n", "Attr", f->attr & ATTR_READ_ONLY ? 'R' : '-',
			f->attr & ATTR_HIDDEN ? 'H' : '-',
			f->attr & ATTR_SYSTEM ? 'S' : '-',
			f->attr & ATTR_DIRECTORY ? 'D' : '-',
			f->attr & ATTR_ARCHIVE ? 'D' : '-');
	pr_msg("%-8s: %s/ %s\n", "Flags",
			f->flags & ALLOC_NOFATCHAIN ? "NoFatChain" : "FatChain",
			f->flags & ALLOC_POSIBLE ? "AllocationPossible" : "AllocationImpossible");

	pr_msg("%-8s: %02d-%02d-%02d %02d:%02d:%02d\n", "Access",
			1980 + f->atime.tm_year, f->atime.tm_mon, f->atime.tm_mday,
			f->atime.tm_hour, f->atime.tm_min, f->atime.tm_sec);
	pr_msg("%-8s: %02d-%02d-%02d %02d:%02d:%02d\n", "Modify",
			1980 + f->mtime.tm_year, f->mtime.tm_mon, f->mtime.tm_mday,
			f->mtime.tm_hour, f->mtime.tm_min, f->mtime.tm_sec);
	pr_msg("%-8s: %02d-%02d-%02d %02d:%02d:%02d\n", "Create",
			1980 + f->ctime.tm_year, f->ctime.tm_mon, f->ctime.tm_mday,
			f->ctime.tm_hour, f->ctime.tm_min, f->ctime.tm_sec);
	pr_msg("\n");
}

/**
 * main   - main function
 * @argc:   argument count
 * @argv:   argument vector
 */
int main(int argc, char *argv[])
{
	int index;
	int opt;
	int longindex;
	int ret = 0;
	struct exfat_bootsec boot;
	uint32_t clu = 0;
	char *path = NULL;
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

	if (optind != argc - 2) {
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
	path = argv[optind + 1];

	if (exfat_load_bootsec(&boot))
		goto out;
	if (exfat_store_info(&boot))
		goto out;
	if (exfat_traverse_root_directory())
		goto out;
	if ((clu = exfat_lookup(info.root_offset, path)) == 0) {
		ret = ENOENT;
		goto out;
	}

	index = exfat_get_cache(clu);
	f = exfat_get_fileinfo(clu, index, path);
	if (!f)
		goto out;

	exfat_stat_file(f);

	ret = EXIT_SUCCESS;

out:
	exfat_clean_info();
	return ret;
}
