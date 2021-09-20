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

#include "statfsexfat.h"
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
	fprintf(stderr, "Usage: %s [OPTION]... FILE\n", PROGRAM_NAME);
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
 * exfat_print_bootsec - print boot sector in exFAT
 * @b:                  boot sector pointer in exFAT (Output)
 */
void exfat_print_bootsec(struct exfat_bootsec *b)
{
	uint64_t secsize = 1 << b->BytesPerSectorShift;

	pr_msg("%-28s\t: 0x%08llx (sector)\n", "media-relative sector offset",
			le64_to_cpu(b->PartitionOffset));
	pr_msg("%-28s\t: 0x%08x (sector)\n", "Offset of the First FAT",
			le32_to_cpu(b->FatOffset));
	pr_msg("%-28s\t: %10u (sector)\n", "Length of FAT table",
			le32_to_cpu(b->FatLength));
	pr_msg("%-28s\t: 0x%08x (sector)\n", "Offset of the Cluster Heap",
			le32_to_cpu(b->ClusterHeapOffset));
	pr_msg("%-28s\t: %10u (cluster)\n", "The number of clusters",
			le32_to_cpu(b->ClusterCount));
	pr_msg("%-28s\t: %10u (cluster)\n", "The first cluster of the root",
			le32_to_cpu(b->FirstClusterOfRootDirectory));
	pr_msg("%-28s\t: %10llu (sector)\n", "Size of exFAT volumes",
			le16_to_cpu(b->VolumeLength));
	pr_msg("%-28s\t: %10" PRIu64 " (byte)\n", "Bytes per sector",
			secsize);
	pr_msg("%-28s\t: %10" PRIu64 " (byte)\n", "Bytes per cluster",
			(1 << b->SectorsPerClusterShift) * secsize);
	pr_msg("%-28s\t: %10u\n", "The number of FATs",
			b->NumberOfFats);
	pr_msg("%-28s\t: %10u (%%)\n", "The percentage of clusters",
			b->PercentInUse);
	pr_msg("\n");
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
	int ret = 0;
	struct exfat_bootsec boot;

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

	exfat_print_bootsec(&boot);
out:
	exfat_clean_info();
	return ret;
}
