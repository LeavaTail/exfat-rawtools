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

#include "lsexfat.h"
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
 * exfat_print_dentry - print dentry like "ls -l"
 * @f:                  file information
 *
 * @return              0 (success)
 */
static int exfat_print_dentry(struct exfat_fileinfo *f)
{
	char ro, hidden, sys, dir, arch;
	char buf[80] = {};
	struct tm time = f->mtime;

	ro = f->attr & ATTR_READ_ONLY ? 'R' : '-';
	hidden = f->attr & ATTR_HIDDEN ? 'H' : '-';
	sys = f->attr & ATTR_SYSTEM ? 'S' : '-';
	dir = f->attr & ATTR_DIRECTORY ? 'D' : '-';
	arch = f->attr & ATTR_ARCHIVE ? 'A' : '-';

	sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d",
		1980 + time.tm_year, time.tm_mon, time.tm_mday,
		time.tm_hour, time.tm_min, time.tm_sec);

	pr_msg("%c%c%c%c%c ", ro, hidden, sys, dir, arch);
	pr_msg("%8lu %s ", f->datalen, buf);
	pr_msg("%s\n", f->name);

	return 0;
}

/**
 * exfat_print_file - print file like "ls -l"
 * @index:            parent directory cache index
 * @clu               file first cluster
 *
 * @return            0 (success)
 *                    -ENOENT (Not Found)
 */
static int exfat_print_file(int index, uint32_t clu)
{
	node2_t *tmp;
	struct exfat_fileinfo *f;

	tmp = info.root[index];
	if (!tmp)
		return -ENOENT;

	while (tmp->next != NULL) {
		tmp = tmp->next;
		if (tmp->index == clu) { 
			f = (struct exfat_fileinfo *)tmp->data;
			exfat_print_dentry(f);
			return 0;
		}
	}
	return -ENOENT;
}

/**
 * exfat_print_directory - print directory like "ls -l"
 * @index:                 directory cache index
 * @clu                    directory first cluster
 *
 * @return                 0 (success)
 *                         -ENOENT (Not Found)
 */
static int exfat_print_directory(int index, uint32_t clu)
{
	node2_t *tmp;
	struct exfat_fileinfo *f;

	tmp = info.root[index];
	if (!tmp)
		return -ENOENT;
	
	exfat_traverse_directory(clu);
	while (tmp->next != NULL) {
		tmp = tmp->next;
		f = (struct exfat_fileinfo *)tmp->data;
		exfat_print_dentry(f);
	}
	return 0;
}

/**
 * main   - main function
 * @argc:   argument count
 * @argv:   argument vector
 */
int main(int argc, char *argv[])
{
	int i, index;
	int opt;
	int longindex;
	int ret = -EINVAL;
	struct exfat_bootsec boot;
	uint32_t clu = 0;
	uint32_t p_clu = 0;
	char *path = NULL;

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
	/* Directory */
	if (info.root[index]) {
		exfat_print_directory(index, clu);
	/* File */
	} else {
		if (path[strlen(path) - 1] == '/') {
			pr_err("'%s': No such file.\n", path);
			ret = -ENOENT;
			goto out;
		}
		/* obtain parent directory*/
		for (i = strlen(path); i > 0 && path[i] != '/'; i--);
		path[i] = '\0';

		if ((p_clu = exfat_lookup(info.root_offset, path)) == 0) {
			ret = ENOENT;
			goto out;
		}
		index = exfat_get_cache(p_clu);
		exfat_print_file(index, clu);
	}

	ret = EXIT_SUCCESS;

out:
	exfat_clean_info();
	return ret;
}
