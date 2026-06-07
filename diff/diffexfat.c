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

#include "diffexfat.h"
#include "exfat.h"

FILE *output;
unsigned int print_level = PRINT_WARNING;
struct exfat_info info;

struct diffexfat_table {
	uint32_t offset;
	uint64_t length;
	uint8_t *data;
};

struct diffexfat_image {
	struct exfat_bootsec boot;
	struct diffexfat_table bitmap;
	struct diffexfat_table upcase;
	uint8_t vol_length;
	uint16_t vol_label[11];
};

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
	fprintf(stderr, "Compare two exFAT images and print differences\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "  --help\tdisplay this help and exit.\n");
	fprintf(stderr, "  --version\toutput version information and exit.\n");
	fprintf(stderr, "\n");
}

/**
 * version        - print out program version
 * @command_name:   command name
 * @version:        program version
 * @author:         program author
 */
static void version(const char *command_name, const char *version, const char *author)
{
	fprintf(stdout, "%s %s\n", command_name, version);
	fprintf(stdout, "\n");
	fprintf(stdout, "Written by %s.\n", author);
}

/**
 * clean_image - clean image metadata snapshot
 * @image:       loaded image metadata
 */
static void clean_image(struct diffexfat_image *image)
{
	free(image->bitmap.data);
	free(image->upcase.data);
	image->bitmap.data = NULL;
	image->upcase.data = NULL;
}

static int copy_table(uint8_t **dst, const void *src, uint64_t size)
{
	if (!src || size == 0 || size > SIZE_MAX)
		return -EINVAL;

	*dst = malloc(size);
	if (!*dst)
		return -ENOMEM;

	memcpy(*dst, src, size);
	return 0;
}

/**
 * load_image - validate that @path is readable as an exFAT image
 * @path:       image file path
 * @image:      loaded image metadata
 *
 * @return:          == 0 (Success)
 *                   <  0 (failed)
 */
static int load_image(const char *path, struct diffexfat_image *image)
{
	int ret;
	bool initialized = false;

	memset(image, 0, sizeof(*image));

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

	ret = exfat_load_bootsec(&image->boot);
	if (ret) {
		pr_err("%s: invalid Main Boot Sector\n", path);
		goto out;
	}

	ret = exfat_store_info(&image->boot);
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

	ret = exfat_traverse_root_directory();
	if (ret) {
		pr_err("%s: can't load Root Directory special entries\n", path);
		goto out;
	}

	image->bitmap.offset = info.alloc_offset;
	image->bitmap.length = info.alloc_length;
	ret = copy_table(&image->bitmap.data, info.alloc_table, image->bitmap.length);
	if (ret) {
		pr_err("%s: can't snapshot Allocation Bitmap\n", path);
		goto out;
	}
	image->upcase.offset = info.upcase_offset;
	image->upcase.length = info.upcase_size;
	ret = copy_table(&image->upcase.data, info.upcase_table, image->upcase.length);
	if (ret) {
		pr_err("%s: can't snapshot Up-case Table\n", path);
		goto out;
	}
	image->vol_length = info.vol_length;
	memcpy(image->vol_label, info.vol_label, sizeof(image->vol_label));

out:
	if (initialized)
		exfat_clean_info();
	return ret;
}

static int compare_boot_field_u8(const char *name, uint8_t src, uint8_t dst)
{
	if (src == dst)
		return 0;

	pr_msg("Boot Sector: %s differs: image1=%u image2=%u\n", name, src, dst);
	return 1;
}

static int compare_boot_field_u16_hex(const char *name, uint16_t src, uint16_t dst)
{
	if (src == dst)
		return 0;

	pr_msg("Boot Sector: %s differs: image1=0x%04x image2=0x%04x\n", name, src, dst);
	return 1;
}

static int compare_boot_field_u32(const char *name, uint32_t src, uint32_t dst)
{
	if (src == dst)
		return 0;

	pr_msg("Boot Sector: %s differs: image1=%u image2=%u\n", name, src, dst);
	return 1;
}

static int compare_boot_field_u32_hex(const char *name, uint32_t src, uint32_t dst)
{
	if (src == dst)
		return 0;

	pr_msg("Boot Sector: %s differs: image1=0x%08x image2=0x%08x\n", name, src, dst);
	return 1;
}

static int compare_boot_field_u64(const char *name, uint64_t src, uint64_t dst)
{
	if (src == dst)
		return 0;

	pr_msg("Boot Sector: %s differs: image1=%" PRIu64 " image2=%" PRIu64 "\n",
			name, src, dst);
	return 1;
}

static int compare_special_field_u8(const char *entry, const char *name, uint8_t src, uint8_t dst)
{
	if (src == dst)
		return 0;

	pr_msg("%s: %s differs: image1=%u image2=%u\n", entry, name, src, dst);
	return 1;
}

static int compare_special_field_u32(const char *entry, const char *name, uint32_t src, uint32_t dst)
{
	if (src == dst)
		return 0;

	pr_msg("%s: %s differs: image1=%u image2=%u\n", entry, name, src, dst);
	return 1;
}

static int compare_special_field_u64(const char *entry, const char *name, uint64_t src, uint64_t dst)
{
	if (src == dst)
		return 0;

	pr_msg("%s: %s differs: image1=%" PRIu64 " image2=%" PRIu64 "\n",
			entry, name, src, dst);
	return 1;
}

/**
 * compare_boot_layout - compare critical Main Boot Sector layout fields
 * @src:                 source image boot sector
 * @dst:                 destination image boot sector
 *
 * @return:              == 0 (same)
 *                       != 0 (different)
 */
static int compare_boot_layout(const struct exfat_bootsec *src, const struct exfat_bootsec *dst)
{
	int diff = 0;

	diff |= compare_boot_field_u8("BytesPerSectorShift",
			src->BytesPerSectorShift, dst->BytesPerSectorShift);
	diff |= compare_boot_field_u8("SectorsPerClusterShift",
			src->SectorsPerClusterShift, dst->SectorsPerClusterShift);
	diff |= compare_boot_field_u64("VolumeLength",
			le64_to_cpu(src->VolumeLength), le64_to_cpu(dst->VolumeLength));
	diff |= compare_boot_field_u32("FatOffset",
			le32_to_cpu(src->FatOffset), le32_to_cpu(dst->FatOffset));
	diff |= compare_boot_field_u32("FatLength",
			le32_to_cpu(src->FatLength), le32_to_cpu(dst->FatLength));
	diff |= compare_boot_field_u32("ClusterHeapOffset",
			le32_to_cpu(src->ClusterHeapOffset), le32_to_cpu(dst->ClusterHeapOffset));
	diff |= compare_boot_field_u32("ClusterCount",
			le32_to_cpu(src->ClusterCount), le32_to_cpu(dst->ClusterCount));
	diff |= compare_boot_field_u32("FirstClusterOfRootDirectory",
			le32_to_cpu(src->FirstClusterOfRootDirectory),
			le32_to_cpu(dst->FirstClusterOfRootDirectory));
	diff |= compare_boot_field_u8("NumberOfFats",
			src->NumberOfFats, dst->NumberOfFats);

	return diff;
}

/**
 * compare_boot_metadata - compare non-layout Main Boot Sector fields
 * @src:                   source image boot sector
 * @dst:                   destination image boot sector
 *
 * @return:                == 0 (same)
 *                         != 0 (different)
 */
static int compare_boot_metadata(const struct exfat_bootsec *src, const struct exfat_bootsec *dst)
{
	int diff = 0;

	diff |= compare_boot_field_u64("PartitionOffset",
			le64_to_cpu(src->PartitionOffset), le64_to_cpu(dst->PartitionOffset));
	diff |= compare_boot_field_u32_hex("VolumeSerialNumber",
			le32_to_cpu(src->VolumeSerialNumber), le32_to_cpu(dst->VolumeSerialNumber));
	diff |= compare_boot_field_u16_hex("FileSystemRevision",
			le16_to_cpu(src->FileSystemRevision), le16_to_cpu(dst->FileSystemRevision));
	diff |= compare_boot_field_u16_hex("VolumeFlags",
			le16_to_cpu(src->VolumeFlags), le16_to_cpu(dst->VolumeFlags));
	diff |= compare_boot_field_u8("DriveSelect", src->DriveSelect, dst->DriveSelect);
	diff |= compare_boot_field_u8("PercentInUse", src->PercentInUse, dst->PercentInUse);

	return diff;
}

/**
 * compare_special_entries - compare Root Directory special entry metadata
 * @src:                    source image metadata
 * @dst:                    destination image metadata
 *
 * @return:                 == 0 (same)
 *                          != 0 (different)
 */
static int compare_special_entries(const struct diffexfat_image *src, const struct diffexfat_image *dst)
{
	int diff = 0;

	diff |= compare_special_field_u32("Allocation Bitmap", "FirstCluster",
			src->bitmap.offset, dst->bitmap.offset);
	diff |= compare_special_field_u64("Allocation Bitmap", "DataLength",
			src->bitmap.length, dst->bitmap.length);
	diff |= compare_special_field_u32("Up-case Table", "FirstCluster",
			src->upcase.offset, dst->upcase.offset);
	diff |= compare_special_field_u32("Up-case Table", "DataLength",
			src->upcase.length, dst->upcase.length);
	diff |= compare_special_field_u8("Volume Label", "CharacterCount",
			src->vol_length, dst->vol_length);

	if (!memcmp(src->vol_label, dst->vol_label, sizeof(src->vol_label)))
		return diff;

	pr_msg("Volume Label: VolumeLabel differs\n");
	return 1;
}

/**
 * compare_allocation_bitmap - compare Allocation Bitmap contents
 * @src:                       source image metadata
 * @dst:                       destination image metadata
 *
 * @return:                    == 0 (same)
 *                             != 0 (different)
 */
static int compare_allocation_bitmap(const struct diffexfat_image *src, const struct diffexfat_image *dst)
{
	uint32_t index;
	uint32_t cluster_count = le32_to_cpu(src->boot.ClusterCount);
	uint64_t bitmap_length = ROUNDUP((uint64_t)cluster_count, CHAR_BIT);
	int diff = 0;

	if (src->bitmap.length != dst->bitmap.length)
		return 1;
	if (src->bitmap.length < bitmap_length) {
		pr_err("Allocation Bitmap: DataLength is too small: %" PRIu64 " < %" PRIu64 "\n",
			src->bitmap.length, bitmap_length);
		return 1;
	}

	for (index = 0; index < cluster_count; index++) {
		uint32_t clu = EXFAT_FIRST_CLUSTER + index;
		uint8_t mask = 1 << (index % CHAR_BIT);
		bool src_allocated = src->bitmap.data[index / CHAR_BIT] & mask;
		bool dst_allocated = dst->bitmap.data[index / CHAR_BIT] & mask;

		if (src_allocated == dst_allocated)
			continue;

		pr_msg("Allocation Bitmap: cluster #%u differs: image1=%s image2=%s\n",
				clu,
				src_allocated ? "allocated" : "free",
				dst_allocated ? "allocated" : "free");
		diff = 1;
	}

	return diff;
}

/**
 * compare_upcase_table - compare Up-case Table contents
 * @src:                 source image metadata
 * @dst:                 destination image metadata
 *
 * @return:              == 0 (same)
 *                       != 0 (different)
 */
static int compare_upcase_table(const struct diffexfat_image *src, const struct diffexfat_image *dst)
{
	uint32_t index;
	const uint16_t *src_table = (const uint16_t *)src->upcase.data;
	const uint16_t *dst_table = (const uint16_t *)dst->upcase.data;
	int diff = 0;

	if (src->upcase.length != dst->upcase.length)
		return 1;
	if (src->upcase.length % sizeof(uint16_t)) {
		pr_err("Up-case Table: DataLength is not aligned: %" PRIu64 "\n", src->upcase.length);
		return 1;
	}

	for (index = 0; index < src->upcase.length / sizeof(uint16_t); index++) {
		uint16_t src_value = le16_to_cpu(src_table[index]);
		uint16_t dst_value = le16_to_cpu(dst_table[index]);

		if (src_value == dst_value)
			continue;

		pr_msg("Up-case Table: entry #0x%04x differs: image1=0x%04x image2=0x%04x\n",
				index, src_value, dst_value);
		diff = 1;
	}

	return diff;
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
	int diff = 0;
	int ret = EXIT_FAILURE;
	struct diffexfat_image image_src = {0};
	struct diffexfat_image image_dst = {0};

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
	if (load_image(argv[optind], &image_src))
		goto out;
	if (load_image(argv[optind + 1], &image_dst))
		goto out;

	if (compare_boot_layout(&image_src.boot, &image_dst.boot))
		goto out;

	diff |= compare_boot_metadata(&image_src.boot, &image_dst.boot);
	diff |= compare_special_entries(&image_src, &image_dst);
	diff |= compare_allocation_bitmap(&image_src, &image_dst);
	diff |= compare_upcase_table(&image_src, &image_dst);

	ret = diff ? EXIT_FAILURE : EXIT_SUCCESS;
out:
	clean_image(&image_src);
	clean_image(&image_dst);
	return ret;
}