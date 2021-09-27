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

#include "exfat.h"
#include "catexfat.h"

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
	fprintf(stderr, "print on the standard output\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  --help\tDESCRIPTION.\n");
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
 * exfat_print_dentry - print dentry like "cat"
 * @fst:                first cluster
 * @index:              cache index
 *
 * @return              0 (success)
 */
static int exfat_print_file(uint32_t fst, int index)
{
	uint32_t clu;
	void *data;
	node2_t *tmp;
	struct exfat_fileinfo *f;

	tmp = info.root[index];
	if (!tmp)
		return -ENOENT;

	while (tmp->next != NULL) {
		tmp = tmp->next;
		if (tmp->index == fst) { 
			f = (struct exfat_fileinfo *)tmp->data;
			break;
		}
	}
	if (!tmp)
		return -EINVAL;

	data = malloc(info.cluster_size);
	for (clu = fst; clu != EXFAT_LASTCLUSTER; clu = exfat_next_cluster(f, clu)) {
		get_cluster(data, clu);
		allwrite(STDOUT_FILENO, data, info.cluster_size);
	}
	free(data);

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
	/* Directory */
	if (info.root[index]) {
		f = info.root[index]->data;
		pr_err("%s is a directory\n", f->name);
		ret = -EINVAL;
		goto out;
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
		exfat_print_file(clu, exfat_get_cache(p_clu));
	}

	ret = EXIT_SUCCESS;

out:
	exfat_clean_info();
	return ret;
}
