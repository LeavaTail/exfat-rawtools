// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#ifndef _EXFAT_H
#define _EXFAT_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <endian.h>
#include <linux/types.h>

#include "print.h"
#include "list2.h"
#include "utf8.h"
#include "bitmap.h"

#if __BYTE_ORDER == __BIG_ENDIAN
#define cpu_to_le8(x)        (x)
#define cpu_to_le16(x)       bswap_16(x)
#define cpu_to_le32(x)       bswap_32(x)
#define cpu_to_le64(x)       bswap_64(x)
#define le8_to_cpu(x)        (x)
#define le16_to_cpu(x)       bswap_16(x)
#define le32_to_cpu(x)       bswap_32(x)
#define le64_to_cpu(x)       bswap_64(x)
#else
/* __BYTE_ORDER == __LITTLE_ENDIAN */
#define cpu_to_le8(x)        (x)
#define cpu_to_le16(x)       (x)
#define cpu_to_le32(x)       (x)
#define cpu_to_le64(x)       (x)
#define le8_to_cpu(x)        (x)
#define le16_to_cpu(x)       (x)
#define le32_to_cpu(x)       (x)
#define le64_to_cpu(x)       (x)
#endif

#define SECTORSIZE       512
#define NAMELENGHT       1024
#define DENTRY_LISTSIZE  1024
#define PATHNAME_MAX     4096
#define DIRECTORY_FILES  1024
/*
 * exFAT definition
 */
#define ACTIVEFAT     0x0001
#define VOLUMEDIRTY   0x0002
#define MEDIAFAILURE  0x0004
#define CLEARTOZERO   0x0008

#define ENTRY_NAME_MAX          15
#define MAX_NAME_LENGTH         255

#define EXFAT_FIRST_CLUSTER  2
#define EXFAT_BADCLUSTER     0xFFFFFFF7
#define EXFAT_LASTCLUSTER    0xFFFFFFFF
#define EXFAT_SIGNATURE      0xAA55
#define EXFAT_EXSIGNATURE    0xAA550000


struct exfat_info {
	int fd;
	off_t total_size;
	uint64_t partition_offset;
	uint32_t vol_size;
	uint16_t sector_size;
	uint32_t cluster_size;
	uint32_t cluster_count;
	uint32_t fat_offset;
	uint32_t fat_length;
	uint32_t heap_offset;
	uint32_t root_offset;
	uint32_t alloc_offset;
	uint64_t alloc_length;
	uint8_t *alloc_table;
	uint32_t upcase_offset;
	uint32_t upcase_size;
	uint16_t *upcase_table;
	uint8_t vol_length;
	uint16_t *vol_label;
	node2_t **root;
	uint32_t root_size;
};

struct exfat_fileinfo {
	unsigned char *name;
	uint64_t namelen;
	uint64_t datalen;
	uint8_t cached;
	uint16_t attr;
	uint8_t flags;
	struct tm ctime;
	struct tm atime;
	struct tm mtime;
	uint16_t hash;
	uint32_t clu;
};

struct exfat_bootsec {
	__u8 JumpBoot[3];
	__u8 FileSystemName[8];
	__u8 MustBeZero[53];
	__le64 PartitionOffset;
	__le64 VolumeLength;
	__le32 FatOffset;
	__le32 FatLength;
	__le32 ClusterHeapOffset;
	__le32 ClusterCount;
	__le32 FirstClusterOfRootDirectory;
	__le32 VolumeSerialNumber;
	__le16 FileSystemRevision;
	__le16 VolumeFlags;
	__u8 BytesPerSectorShift;
	__u8 SectorsPerClusterShift;
	__u8 NumberOfFats;
	__u8 DriveSelect;
	__u8 PercentInUse;
	__u8 Reserved[7];
	__u8 BootCode[390];
	__le16 BootSignature;
};

struct exfat_dentry {
	__u8 EntryType;
	union {
		/* Allocation Bitmap Directory Entry */
		struct {
			__u8 BitmapFlags;
			__u8 Reserved[18];
			__le32 FirstCluster;
			__le64 DataLength;
		} __attribute__((packed)) bitmap;
		/* Up-case Table Directory Entry */
		struct {
			__u8 Reserved1[3];
			__le32 TableCheckSum;
			__u8 Reserved2[12];
			__le32 FirstCluster;
			__le32 DataLength;
		} __attribute__((packed)) upcase;
		/* Volume Label Directory Entry */
		struct {
			__u8 CharacterCount;
			__le16 VolumeLabel[11];
			__u8 Reserved[8];
		} __attribute__((packed)) vol;
		/* File Directory Entry */
		struct {
			__u8 SecondaryCount;
			__le16 SetChecksum;
			__le16 FileAttributes;
			__u8 Reserved1[2];
			__le32 CreateTimestamp;
			__le32 LastModifiedTimestamp;
			__le32 LastAccessedTimestamp;
			__u8 Create10msIncrement;
			__u8 LastModified10msIncrement;
			__u8 CreateUtcOffset;
			__u8 LastModifiedUtcOffset;
			__u8 LastAccessdUtcOffset;
			__u8 Reserved2[7];
		} __attribute__((packed)) file;
		/* Volume GUID Directory Entry */
		struct {
			__u8 SecondaryCount;
			__le16 SetChecksum;
			__le16 GeneralPrimaryFlags;
			__u8 VolumeGuid[16];
			__u8 Reserved[10];
		} __attribute__((packed)) guid;
		/* Stream Extension Directory Entry */
		struct {
			__u8 GeneralSecondaryFlags;
			__u8 Reserved1;
			__u8 NameLength;
			__le16 NameHash;
			__u8 Reserved2[2];
			__le64 ValidDataLength;
			__u8 Reserved3[4];
			__le32 FirstCluster;
			__le64 DataLength;
		} __attribute__((packed)) stream;
		/* File Name Directory Entry */
		struct {
			__u8 GeneralSecondaryFlags;
			uint16_t FileName[15];
		} __attribute__((packed)) name;
		/* Vendor Extension Directory Entry */
		struct {
			__u8 GeneralSecondaryFlags;
			__u8 VendorGuid[16];
			__u8 VendorDefined[14];
		} __attribute__((packed)) vendor;
		/* Vendor Allocation Directory Entry */
		struct {
			__u8 GeneralSecondaryFlags;
			__u8 VendorGuid[16];
			__u8 VendorDefined[2];
			__le32 FirstCluster;
			__le64 DataLength;
		} __attribute__((packed)) vendor_alloc;
	} __attribute__((packed)) dentry;
} __attribute__ ((packed));

/* FAT/exFAT File Attributes */
#define ATTR_READ_ONLY       0x01
#define ATTR_HIDDEN          0x02
#define ATTR_SYSTEM          0x04
#define ATTR_VOLUME_ID       0x08
#define ATTR_DIRECTORY       0x10
#define ATTR_ARCHIVE         0x20
#define ATTR_LONG_FILE_NAME  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)
/* exFAT dentry type */
#define DENTRY_UNUSED        0x00
#define DENTRY_BITMAP        0x81
#define DENTRY_UPCASE        0x82
#define DENTRY_VOLUME        0x83
#define DENTRY_FILE          0x85
#define DENTRY_GUID          0xA0
#define DENTRY_STREAM        0xC0
#define DENTRY_NAME          0xC1
#define DENTRY_VENDOR        0xE0
#define DENTRY_VENDOR_ALLOC  0xE1

/* exFAT EntryType */
#define EXFAT_TYPECODE       0x1F
#define EXFAT_TYPEIMPORTANCE 0x20
#define EXFAT_CATEGORY       0x40
#define EXFAT_INUSE          0x80

/* exFAT GeneralSecondaryFlags */
#define ALLOC_POSIBLE         0x01
#define ALLOC_NOFATCHAIN      0x02

/* TimeStamp */
#define FAT_DAY      0
#define FAT_MONTH    5
#define FAT_YEAR     9
#define EXFAT_DSEC   0
#define EXFAT_MINUTE 5
#define EXFAT_HOUR   11
#define EXFAT_DAY    16
#define EXFAT_MONTH  21
#define EXFAT_YEAR   25

#define MAX(a, b)      ((a) > (b) ? (a) : (b))
#define MIN(a, b)      ((a) < (b) ? (a) : (b))
#define ROUNDUP(a, b)  ((a + b - 1) / b)

static inline bool is_power2(unsigned int n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}

static inline uint64_t power2(uint32_t n)
{
	return 1 << n;
}

#define EXFAT_SECTOR(b)      (1 << b.BytesPerSectorShift)
#define EXFAT_CLUSTER(b)     ((1 << b.SectorsPerClusterShift) * EXFAT_SECTOR(b))
#define EXFAT_FAT(b)         (b.FatOffset * EXFAT_SECTOR(b))
#define EXFAT_HEAP(b)        (b.ClusterHeapOffset * EXFAT_SECTOR(b))

static inline uint64_t exfat_offset(struct exfat_info i, uint32_t clu)
{
	return ((i.heap_offset * i.sector_size) + clu * i.cluster_size);
}

/* Generic function prototype */
int get_sector(void *, off_t, size_t);
int set_sector(void *, off_t, size_t);
int get_cluster(void *, off_t);
int set_cluster(void *, off_t);
int get_clusters(void *, off_t, size_t);
int set_clusters(void *, off_t, size_t);

/* Superblock function prototype */
int exfat_init_info(void);
int exfat_store_info(struct exfat_bootsec *);
int exfat_clean_info(void);
int exfat_load_bootsec(struct exfat_bootsec *);
int exfat_check_bootsec(struct exfat_bootsec *);
int exfat_check_extend_bootsec(void);
int exfat_check_bootchecksum(void);

/* FAT-entry function prototype */
int exfat_get_fat(uint32_t, uint32_t *);
int exfat_set_fat(uint32_t, uint32_t);
int exfat_set_fat_chain(struct exfat_fileinfo *, uint32_t);
void exfat_print_fat_chain(struct exfat_fileinfo *, uint32_t);

/* cluster function prototype */
int exfat_alloc_clusters(struct exfat_fileinfo *, uint32_t, size_t);
int exfat_free_clusters(struct exfat_fileinfo *, uint32_t, size_t);
int exfat_new_clusters(size_t);
uint32_t exfat_concat_cluster(struct exfat_fileinfo *, uint32_t, void **);
uint32_t exfat_concat_cluster_fast(uint32_t, void **, size_t);
uint32_t exfat_set_cluster(struct exfat_fileinfo *, uint32_t, void *);
int exfat_check_last_cluster(struct exfat_fileinfo *, uint32_t);
uint32_t exfat_next_cluster(struct exfat_fileinfo *, uint32_t);
int exfat_get_last_cluster(struct exfat_fileinfo *, uint32_t);

/* Directory entry cache function prototype */
void exfat_print_cache(void);
int exfat_check_cache(uint32_t);
int exfat_get_cache(uint32_t);
int exfat_clean_cache(uint32_t);
int exfat_create_cache(node2_t *, uint32_t,
		struct exfat_dentry *, struct exfat_dentry *, uint16_t *);

/* Special entry function prototype */
void exfat_print_upcase(void);
void exfat_print_label(void);
void exfat_print_fat(void);
void exfat_print_bitmap(void);
int exfat_load_bitmap(uint32_t);
int exfat_save_bitmap(uint32_t, uint32_t);
int exfat_load_bitmap_cluster(struct exfat_dentry);
int exfat_load_upcase_cluster(struct exfat_dentry);
int exfat_load_volume_label(struct exfat_dentry);

/* File function prototype */
int exfat_traverse_root_directory(void);
int exfat_traverse_directory(uint32_t);
uint32_t exfat_calculate_bootchecksum(unsigned char *, unsigned short);
uint16_t exfat_calculate_checksum(unsigned char *, unsigned char);
uint32_t exfat_calculate_tablechecksum(unsigned char *, uint64_t);
uint16_t exfat_calculate_namehash(uint16_t *, uint8_t);
int exfat_update_filesize(struct exfat_fileinfo *, uint32_t);
void exfat_convert_unixtime(struct tm *, uint32_t, uint8_t, uint8_t);
int exfat_convert_timezone(uint8_t);
uint32_t exfat_lookup(uint32_t, char *);
void exfat_convert_uniname(uint16_t *, uint64_t, unsigned char *);
void exfat_convert_uniname(uint16_t *, uint64_t, unsigned char *);
uint16_t exfat_convert_upper(uint16_t);
void exfat_convert_upper_character(uint16_t *, size_t, uint16_t *);

#endif /*_EXFAT_H */
