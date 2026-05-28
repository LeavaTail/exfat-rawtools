// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2026 LeavaTail
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

#include "diffexfat.h"
#include "exfat.h"

FILE *output;
unsigned int print_level = PRINT_WARNING;
struct exfat_info info;
struct exfat_info info_dist;

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
	fprintf(stderr, "Usage: %s [OPTION]... FILE1 FILE2\n", PROGRAM_NAME);
	fprintf(stderr, "Compare 2 exfat image and print difference\n");
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
 * validate_image - validate that @path is readable as an exFAT image
 * @path:            image file path
 *
 * @return:          == 0 (Success)
 *                   <  0 (failed)
 */
static int validate_image(const char *path)
{
	int ret;
	bool initialized = false;
	struct exfat_bootsec boot;

	ret = exfat_init_info();
	if (ret)
		return ret;
	initialized = true;

	info.fd = open(path, O_RDONLY);
	if (info.fd < 0) {
		pr_err("%s: open: %s\n", path, strerror(errno));
		ret = -EIO;
		goto out;
	}

	ret = exfat_load_bootsec(&boot);
	if (ret) {
		pr_err("%s: invalid Main Boot Sector\n", path);
		goto out;
	}

	ret = exfat_store_info(&boot);
	if (ret) {
		pr_err("%s: can't load exFAT volume information\n", path);
		goto out;
	}

	ret = exfat_check_extend_bootsec();
	if (ret) {
		pr_err("%s: invalid Main Extended Boot Sectors\n", path);
		goto out;
	}

	ret = exfat_check_bootchecksum();
	if (ret) {
		pr_err("%s: invalid Main Boot Checksum\n", path);
		goto out;
	}

out:
	if (initialized)
		exfat_clean_info();
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
	int ret = EXIT_FAILURE;

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

#ifdef DIFFEXFAT_DEBUG
	print_level = PRINT_DEBUG;
#endif

	if (optind != argc - 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	output = stdout;
	if (validate_image(argv[optind]))
		goto out;
	if (validate_image(argv[optind + 1]))
		goto out;

	ret = EXIT_SUCCESS;
out:
	return ret;
}