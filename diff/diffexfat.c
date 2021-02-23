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

#include "diffexfat.h"
#include "exfat.h"

int get_sector(struct device_info *, void *, off_t, size_t);
int get_cluster(struct device_info *, void *, off_t);
int get_clusters(struct device_info *, void *, off_t, size_t);

FILE *output = NULL;
unsigned int print_level = PRINT_WARNING;
struct device_info info1;
struct device_info info2;

node_t *bootlist = NULL;
node_t *fatlist = NULL;
node_t *datalist = NULL;

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
 * init_device_info - Initialize member in struct device_info
 */
static void init_device_info(struct device_info *info)
{
	info->fd = -1;
	info->attr = 0;
	info->total_size = 0;
	info->sector_size = 0;
	info->cluster_size = 0;
	info->cluster_count = 0;
	info->flags = 0;
	info->fat_offset = 0;
	info->fat_length = 0;
	info->heap_offset = 0;
	info->root_offset = 0;
	info->root_length = 0;
	info->alloc_table = NULL;
	info->upcase_table = NULL;
	info->upcase_size = 0;
	info->vol_label = NULL;
	info->vol_length = 0;
	info->root_size = DENTRY_LISTSIZE;
	info->root = calloc(info->root_size, sizeof(node2_t *));
}

/**
 * get_device_info - get device name and store in device_info
 * @attr:            command line options
 *
 * @return            0 (success)
 *                   -1 (failed to open)
 */
static int get_device_info(struct device_info *info)
{
	int fd;
	struct stat s;

	if ((fd = open(info->name, O_RDWR)) < 0) {
		pr_err("open: %s\n", strerror(errno));
		return -errno;
	}

	if (fstat(fd, &s) < 0) {
		pr_err("stat: %s\n", strerror(errno));
		close(fd);
		return -errno;
	}

	info->fd = fd;
	info->total_size = s.st_size;
	return 0;
}

/**
 * exfat_check_filesystem - Whether or not exFAT filesystem
 * @boot:                   boot sector pointer
 *
 * @return:                 1 (Image is exFAT filesystem)
 *                          0 (Image isn't exFAT filesystem)
 */
int exfat_check_filesystem(struct exfat_bootsec *b, struct device_info *info)
{
	struct exfat_fileinfo *f;

	if (strncmp((char *)b->FileSystemName, "EXFAT   ", 8))
		return 0;

	info->fat_offset = b->FatOffset;
	info->heap_offset = b->ClusterHeapOffset;
	info->root_offset = b->FirstClusterOfRootDirectory;
	info->sector_size  = 1 << b->BytesPerSectorShift;
	info->cluster_size = (1 << b->SectorsPerClusterShift) * info->sector_size;
	info->cluster_count = b->ClusterCount;
	info->fat_length = b->NumberOfFats * b->FatLength * info->sector_size;
	return 1;
}

/**
 * free_dentry_list - release list2_t
 *
 * @return            Number of lists freed
 */
static int free_dentry_list(struct device_info *info)
{
	free(info->root);
	return 0;
}

/**
 * get_sector - Get Raw-Data from any sector
 * @info:       Target device information
 * @data:       Sector raw data (Output)
 * @index:      Start bytes
 * @count:      The number of sectors
 *
 * @return       0 (success)
 *              -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_sector(struct device_info *info, void *data, off_t index, size_t count)
{
	size_t sector_size = info->sector_size;

	pr_debug("Get: Sector from 0x%lx to 0x%lx\n", index , index + (count * sector_size) - 1);
	if ((pread(info->fd, data, count * sector_size, index)) < 0) {
		pr_err("read: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

/**
 * get_cluster - Get Raw-Data from any cluster
 * @info:       Target device information
 * @data:        cluster raw data (Output)
 * @index:       Start cluster index
 *
 * @return        0 (success)
 *               -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_cluster(struct device_info *info, void *data, off_t index)
{
	return get_clusters(info, data, index, 1);
}

/**
 * get_clusters - Get Raw-Data from any cluster
 * @info:       Target device information
 * @data:         cluster raw data (Output)
 * @index:        Start cluster index
 * @num:          The number of clusters
 *
 * @return         0 (success)
 *                -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_clusters(struct device_info *info, void *data, off_t index, size_t num)
{
	size_t clu_per_sec = info->cluster_size / info->sector_size;
	off_t heap_start = info->heap_offset * info->sector_size;

	if (index < 2 || index + num > info->cluster_count) {
		pr_err("invalid cluster index %lu.\n", index);
		return -1;
	}

	return get_sector(info,
			data,
			heap_start + ((index - 2) * info->cluster_size),
			clu_per_sec * num);
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
	int fd[2] = {0};
	struct stat st[2]  = {0};
	FILE *fp = NULL;
	size_t count;
	char buffer[CMDSIZE] = {0};
	char cmdline[CMDSIZE] = {0};
	static struct exfat_bootsec boot, boot2;
	uint32_t clu = 0;
	stack_t stack;
	unsigned long x;

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

#ifdef DEBUGFATFS_DEBUG
	print_level = PRINT_DEBUG;
#endif

	if (optind != argc - 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	init_device_info(&info1);
	init_device_info(&info2);
	output = stdout;

	memcpy(info1.name, argv[optind], 255);
	memcpy(info2.name, argv[optind + 1], 255);

	ret = get_device_info(&info1);
	if (ret < 0)
		goto info_release;
	
	ret = get_device_info(&info2);
	if (ret < 0)
		goto device1_close;

	if (info1.total_size != info2.total_size) {
		pr_err("file size is different.\n");
		ret = info1.total_size - info2.total_size;
		goto device2_close;
	}

	count = pread(fd[0], &boot, SECSIZE, 0);
	if (count < 0) {
		pr_err("read: %s\n", strerror(errno));
		ret = -EIO;
		goto device2_close;
	}

	if (pread(fd[1], &boot2, SECSIZE, 0) != count) {
		pr_err("read: %s\n", strerror(errno));
		ret = -EIO;
		goto device2_close;
	}

	if (memcmp(&boot, &boot2, SECSIZE)) {
		pr_err("Boot sector is different.\n");
		ret = -EINVAL;
		goto device2_close;
	}

	if (!exfat_check_filesystem(&boot, &info1)) {
		pr_err("This is not exFAT filesystem.\n");
		ret = -EINVAL;
		goto device2_close;
	}

	snprintf(cmdline, CMDSIZE, "/bin/cmp -l %s %s", argv[1], argv[2]);
	if ((fp = popen(cmdline, "r")) == NULL) {
		pr_err("popen %s: %s\n", cmdline, strerror(errno));
		ret = -errno;
		goto device2_close;
	}

	if (init_stack(&stack))
		goto cmd_close;

	init_node(&bootlist);
	init_node(&fatlist);
	init_node(&datalist);

	while(fgets(buffer, CMDSIZE - 1, fp) != NULL) {
		x = strtoul(buffer, NULL, 0);
		if (x < EXFAT_FAT(boot)) {
			append_node(&bootlist, x);
		} else if (x < EXFAT_HEAP(boot)) {
			append_node(&fatlist, x);
		} else {
			append_node(&datalist, x);
		}
	}
	printf("\n");
	push(&stack, info1.root_offset);

	pr_msg("===== Boot Region =====\n");
	print_node(bootlist);
	pr_msg("===== FAT Region =====\n");
	print_node(fatlist);
	pr_msg("===== DATA Region =====\n");
	print_node(datalist);

	free_node(&bootlist);
	free_node(&fatlist);
	free_node(&datalist);

stack_release:
	free_stack(&stack);
cmd_close:
	pclose(fp);
device2_close:
	close(info2.fd);
device1_close:
	close(info1.fd);

info_release:
	free_dentry_list(&info1);
	free_dentry_list(&info2);

	return ret;
}
