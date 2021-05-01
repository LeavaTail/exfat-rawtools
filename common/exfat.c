// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2021 LeavaTail
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include "exfat.h"

extern struct exfat_info info;
/*************************************************************************************************/
/*                                                                                               */
/* GENERIC FUNCTION                                                                              */
/*                                                                                               */
/*************************************************************************************************/
/**
 * get_sector - Get Raw-Data from any sector
 * @data:       Sector raw data (Output)
 * @index:      Start bytes
 * @count:      The number of sectors
 *
 * @return       0 (success)
 *              -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_sector(void *data, off_t index, size_t count)
{
	size_t sector_size = info.sector_size;

	pr_debug("Get: Sector from 0x%lx to 0x%lx\n", index , index + (count * sector_size) - 1);
	if ((pread(info.fd, data, count * sector_size, index)) < 0) {
		pr_err("read: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

/**
 * set_sector - Set Raw-Data from any sector
 * @data:       Sector raw data
 * @index:      Start bytes
 * @count:      The number of sectors
 *
 * @return       0 (success)
 *              -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int set_sector(void *data, off_t index, size_t count)
{
	size_t sector_size = info.sector_size;

	pr_debug("Set: Sector from 0x%lx to 0x%lx\n", index, index + (count * sector_size) - 1);
	if ((pwrite(info.fd, data, count * sector_size, index)) < 0) {
		pr_err("write: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

/**
 * get_cluster - Get Raw-Data from any cluster
 * @data:        cluster raw data (Output)
 * @index:       Start cluster index
 *
 * @return        0 (success)
 *               -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_cluster(void *data, off_t index)
{
	return get_clusters(data, index, 1);
}

/**
 * set_cluster - Set Raw-Data from any cluster
 * @data:        cluster raw data
 * @index:       Start cluster index
 *
 * @return        0 (success)
 *               -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int set_cluster(void *data, off_t index)
{
	return set_clusters(data, index, 1);
}

/**
 * get_clusters - Get Raw-Data from any cluster
 * @data:         cluster raw data (Output)
 * @index:        Start cluster index
 * @num:          The number of clusters
 *
 * @return         0 (success)
 *                -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_clusters(void *data, off_t index, size_t num)
{
	size_t clu_per_sec = info.cluster_size / info.sector_size;
	off_t heap_start = info.heap_offset * info.sector_size;

	if (index < 2 || index + num > info.cluster_count) {
		pr_err("invalid cluster index %lu.\n", index);
		return -1;
	}

	return get_sector(data,
			heap_start + ((index - 2) * info.cluster_size),
			clu_per_sec * num);
}

/**
 * set_clusters - Set Raw-Data from any cluster
 * @data:         cluster raw data
 * @index:        Start cluster index
 * @num:          The number of clusters
 *
 * @return         0 (success)
 *                -1 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int set_clusters(void *data, off_t index, size_t num)
{
	size_t clu_per_sec = info.cluster_size / info.sector_size;
	off_t heap_start = info.heap_offset * info.sector_size;

	if (index < 2 || index + num > info.cluster_count) {
		pr_err("invalid cluster index %lu.\n", index);
		return -1;
	}

	return set_sector(data,
			heap_start + ((index - 2) * info.cluster_size),
			clu_per_sec * num);
}

/**
 * init_device_info - Initialize member in struct device_info
 */
void exfat_init_info(struct exfat_info info)
{
	  info.fd = -1;
	  info.total_size = 0;
	  info.partition_offset = 0;
	  info.vol_size = 0;
	  info.sector_size = 0;
	  info.cluster_size = 0;
	  info.cluster_count = 0;
	  info.fat_offset = 0;
	  info.fat_length = 0;
	  info.heap_offset = 0;
	  info.root_offset = 0;
	  info.alloc_offset = 0;
	  info.alloc_length = 0;
	  info.alloc_table = NULL;
	  info.upcase_table = NULL;
	  info.upcase_size = 0;
	  info.vol_label = NULL;
	  info.vol_length = 0;
	  info.root_size = DENTRY_LISTSIZE;
	  info.root = calloc(info.root_size, sizeof(node2_t *));
} 

/**
 * exfat_clean - function to clean opeartions
 * @index:       directory chain index
 *
 * @return        0 (success)
 *               -1 (already released)
 */
int exfat_clean_info(struct exfat_info info)
{
	int index;
	node2_t *tmp;
	struct exfat_fileinfo *f;

	for(index = 0; index < info.root_size && info.root[index]; index++) {
		if ((!info.root[index])) {
			pr_warn("index %d was already released.\n", index);
			return -1;
		}

		tmp = info.root[index];
		f = (struct exfat_fileinfo *)tmp->data;
		free(f->name);
		f->name = NULL;

		exfat_clean_cache(index);
		free(tmp->data);
		free(tmp);
	}
	free(info.root);
	return 0;
}

/**
 * exfat_concat_cluster - Contatenate cluster @data with next_cluster
 * @f:                    file information pointer
 * @clu:                  index of the cluster
 * @data:                 The cluster (Output)
 *
 * @retrun:               cluster count (@clu has next cluster)
 *                        0             (@clu doesn't have next cluster, or failed to realloc)
 */
static uint32_t exfat_concat_cluster(struct exfat_fileinfo *f, uint32_t clu, void **data)
{
	int i;
	void *tmp;
	uint32_t tmp_clu = 0;
	size_t allocated = 1;
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	if (cluster_num <= 1)
		return cluster_num;

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		if (!(tmp = realloc(*data, info.cluster_size * cluster_num)))
			return 0;
		*data = tmp;
		for (i = 1; i < cluster_num; i++) {
			if (exfat_load_bitmap(clu + i) != 1) {
				pr_warn("cluster %u isn't allocated cluster.\n", clu + i);
				break;
			}
		}
		get_clusters(*data + info.cluster_size, clu + 1, cluster_num - 1);
		return cluster_num;
	}

	/* FAT_CHAIN */
	for (tmp_clu = clu; allocated < cluster_num; allocated++) { 
		if (!(tmp_clu = exfat_get_fat(tmp_clu)))
			break;
		if (exfat_load_bitmap(tmp_clu) != 1)
			pr_warn("cluster %u isn't allocated cluster.\n", tmp_clu);
	}

	if (!(tmp = realloc(*data, info.cluster_size * allocated)))
		return 0;
	*data = tmp;

	for (i = 1; i < allocated; i++) {
		clu = exfat_get_fat(clu);
		get_cluster(*data + info.cluster_size * i, clu);
	}

	return allocated;
}

/**
 * exfat_set_cluster - Set Raw-Data from any sector
 * @f:                 file information pointer
 * @clu:               index of the cluster
 * @data:              The cluster
 *
 * @retrun:            cluster count (@clu has next cluster)
 *                     0             (@clu doesn't have next cluster, or failed to realloc)
 */
static uint32_t exfat_set_cluster(struct exfat_fileinfo *f, uint32_t clu, void *data)
{
	size_t allocated = 0;
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	if (cluster_num <= 1) {
		set_cluster(data, clu);
		return cluster_num;
	}
	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		set_clusters(data, clu, cluster_num);
		return cluster_num;
	}

	/* FAT_CHAIN */
	for (allocated = 0; allocated < cluster_num; allocated++) {
		set_cluster(data + info.cluster_size * allocated, clu);
		if (!(clu = exfat_get_fat(clu)))
			break;
	}

	return allocated + 1;
}

/*************************************************************************************************/
/*                                                                                               */
/* BOOT SECTOR FUNCTION                                                                          */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_load_bootsec - load boot sector
 * @b:                  boot sector pointer in exFAT (Output)
 *
 * @return               0 (success)
 *                      -1 (failed to read)
 */
static int exfat_load_bootsec(struct exfat_bootsec *b)
{
	return get_sector(b, 0, 1);
}

/**
 * exfat_print_upcase - print upcase table
 */
static void exfat_print_upcase(void)
{
	int byte, offset;
	size_t uni_count = 0x10 / sizeof(uint16_t);
	size_t length = info.upcase_size;

	/* Output table header */
	pr_msg("Offset  ");
	for (byte = 0; byte < uni_count; byte++)
		pr_msg("  +%u ",byte);
	pr_msg("\n");

	/* Output Table contents */
	for (offset = 0; offset < length / uni_count; offset++) {
		pr_msg("%04lxh:  ", offset * 0x10 / sizeof(uint16_t));
		for (byte = 0; byte < uni_count; byte++) {
			pr_msg("%04x ", info.upcase_table[offset * uni_count + byte]);
		}
		pr_msg("\n");
	}
}

/**
 * exfat_print_label - print volume label
 */
static void exfat_print_label(void)
{
	unsigned char *name;

	pr_msg("volume Label: ");
	name = malloc(info.vol_length * sizeof(uint16_t) + 1);
	memset(name, '\0', info.vol_length * sizeof(uint16_t) + 1);
	utf16s_to_utf8s(info.vol_label, info.vol_length, name);
	pr_msg("%s\n", name);
	free(name);
}

/**
 * exfat_print_fat - print FAT
 */
static void exfat_print_fat(void)
{
	uint32_t i, j;
	uint32_t *fat;
	size_t sector_num = (info.fat_length + (info.sector_size - 1)) / info.sector_size;
	size_t list_size = 0;
	node2_t **fat_chain, *tmp;

	pr_msg("FAT:\n");
	fat = malloc(info.sector_size * sector_num);
	get_sector(fat, info.fat_offset * info.sector_size, sector_num);

	/* Read fat and create list */
	for (i = 0; i < info.cluster_count - 2; i++) {
		if (EXFAT_FIRST_CLUSTER <= fat[i] && fat[i] < EXFAT_BADCLUSTER)
			list_size++;
	}

	fat_chain = calloc(list_size, sizeof(node2_t *));
	for (i = 0; i < info.cluster_count - 2; i++) {
		if (EXFAT_FIRST_CLUSTER <= fat[i] && fat[i] < EXFAT_BADCLUSTER) {
			for (j = 0; j < list_size; j++) {
				if (fat_chain[j] && fat_chain[j]->index == fat[i]) {
					insert_node2(fat_chain[j], i, NULL);
					break;
				} else if (fat_chain[j] && fat[last_node2(fat_chain[j])->index] == i) {
					append_node2(fat_chain[j], i, NULL);
					break;
				} else if (!fat_chain[j]) {
					fat_chain[j] = init_node2(i, NULL);
					break;
				}
			}
		}
	}

	for (j = 0; j < list_size; j++) {
		if (fat_chain[j] && fat_chain[j]->next) {
			tmp = fat_chain[j];
			pr_msg("%u -> ", fat_chain[j]->index);
			while (tmp->next != NULL) {
				tmp = tmp->next;
				pr_msg("%u -> ", tmp->index);
			}
			pr_msg("NULL\n");
		}
	}
	pr_msg("\n");

	/* Clean up */
	for (i = 0; i < list_size && fat_chain[i]; i++)
		free(fat_chain[i]);
	free(fat_chain);
	free(fat);
}

/**
 * exfat_print_bitmap - print allocation bitmap
 */
static void exfat_print_bitmap(void)
{
	int offset, byte;
	uint8_t entry;
	uint32_t clu;

	pr_msg("Allocation Bitmap:\n");
	pr_msg("Offset    0 1 2 3 4 5 6 7 8 9 a b c d e f\n");
	/* Allocation bitmap consider first cluster is 2 */
	pr_msg("%08x  - - ", 0);

	for (clu = EXFAT_FIRST_CLUSTER; clu < info.cluster_size; clu++) {

		byte = (clu - EXFAT_FIRST_CLUSTER) / CHAR_BIT;
		offset = (clu - EXFAT_FIRST_CLUSTER) % CHAR_BIT;
		entry = info.alloc_table[byte];

		switch (clu % 0x10) {
			case 0x0:
				pr_msg("%08x  ", clu);
				pr_msg("%c ", ((entry >> offset) & 0x01) ? 'o' : '-');
				break;
			case 0xf:
				pr_msg("%c ", ((entry >> offset) & 0x01) ? 'o' : '-');
				pr_msg("\n");
				break;
			default:
				pr_msg("%c ", ((entry >> offset) & 0x01) ? 'o' : '-');
				break;
		}
	}
	pr_msg("\n");
}

/**
 * exfat_load_bitmap - function to load allocation table
 * @clu:               cluster index
 *
 * @return              0 (cluster as available for allocation)
 *                      1 (cluster as not available for allocation)
 *                     -1 (failed)
 */
static int exfat_load_bitmap(uint32_t clu)
{
	int offset, byte;
	uint8_t entry;

	if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1) {
		pr_warn("cluster: %u is invalid.\n", clu);
		return -1;
	}

	clu -= EXFAT_FIRST_CLUSTER;
	byte = clu / CHAR_BIT;
	offset = clu % CHAR_BIT;

	entry = info.alloc_table[byte];
	return (entry >> offset) & 0x01;
}

/**
 * exfat_save_bitmap - function to save allocation table
 * @clu:               cluster index
 * @value:             Bit
 *
 * @return              0 (success)
 *                     -1 (failed)
 */
static int exfat_save_bitmap(uint32_t clu, uint32_t value)
{
	int offset, byte;
	uint8_t mask = 0x01;
	uint8_t *raw_bitmap;

	if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1) {
		pr_err("cluster: %u is invalid.\n", clu);
		return -1;
	}

	clu -= EXFAT_FIRST_CLUSTER;
	byte = clu / CHAR_BIT;
	offset = clu % CHAR_BIT;

	pr_debug("index %u: allocation bitmap is 0x%x ->", clu, info.alloc_table[byte]);
	mask <<= offset;
	if (value)
		info.alloc_table[byte] |= mask;
	else
		info.alloc_table[byte] &= ~mask;

	pr_debug("0x%x\n", info.alloc_table[byte]);
	raw_bitmap = malloc(info.cluster_size);
	get_cluster(raw_bitmap, info.alloc_offset);
	if (value)
		raw_bitmap[byte] |= mask;
	else
		raw_bitmap[byte] &= ~mask;
	set_cluster(raw_bitmap, info.alloc_offset);
	free(raw_bitmap);
	return 0;
}

/**
 * exfat_load_bitmap_cluster - function to load Allocation Bitmap
 * @d:                         directory entry about allocation bitmap
 *
 * @return                      0 (success)
 *                             -1 (bitmap was already loaded)
 */
static int exfat_load_bitmap_cluster(struct exfat_dentry d)
{
	if (info.alloc_offset)
		return -1;

	pr_debug("Get: allocation table: cluster 0x%x, size: 0x%lx\n",
			d.dentry.bitmap.FirstCluster,
			d.dentry.bitmap.DataLength);
	info.alloc_offset = d.dentry.bitmap.FirstCluster;
	info.alloc_table = malloc(info.cluster_size);
	get_cluster(info.alloc_table, d.dentry.bitmap.FirstCluster);
	pr_info("Allocation Bitmap (#%u):\n", d.dentry.bitmap.FirstCluster);

	return 0;
}

/**
 * exfat_load_upcase_cluster - function to load Upcase table
 * @d:                         directory entry about Upcase table
 *
 * @return                      0 (success)
 *                             -1 (bitmap was already loaded)
 */
static int exfat_load_upcase_cluster(struct exfat_dentry d)
{
	uint32_t checksum = 0;
	uint64_t len;

	if (info.upcase_size)
		return -1;

	info.upcase_size = d.dentry.upcase.DataLength;
	len = (info.upcase_size + info.cluster_size - 1) / info.cluster_size;
	info.upcase_table = malloc(info.cluster_size * len);
	pr_debug("Get: Up-case table: cluster 0x%x, size: 0x%x\n",
			d.dentry.upcase.FirstCluster,
			d.dentry.upcase.DataLength);
	get_clusters(info.upcase_table, d.dentry.upcase.FirstCluster, len);
	checksum = exfat_calculate_tablechecksum((unsigned char *)info.upcase_table, info.upcase_size);
	if (checksum != d.dentry.upcase.TableCheckSum)
		pr_warn("Up-case table checksum is difference. (dentry: %x, calculate: %x)\n",
				d.dentry.upcase.TableCheckSum,
				checksum);

	return 0;
}

/**
 * exfat_load_volume_label - function to load volume label
 * @d:                       directory entry about volume label
 *
 * @return                    0 (success)
 *                           -1 (bitmap was already loaded)
 */
static int exfat_load_volume_label(struct exfat_dentry d)
{
	if (info.vol_length)
		return -1;

	info.vol_length = d.dentry.vol.CharacterCount;
	if (info.vol_length) {
		info.vol_label = malloc(sizeof(uint16_t) * info.vol_length);
		pr_debug("Get: Volume label: size: 0x%x\n",
				d.dentry.vol.CharacterCount);
		memcpy(info.vol_label, d.dentry.vol.VolumeLabel,
				sizeof(uint16_t) * info.vol_length);
	}

	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FAT-ENTRY FUNCTION                                                                            */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_get_fat - Whether or not cluster is continuous
 * @clu:                   index of the cluster want to check
 *
 * @retrun:                next cluster (@clu has next cluster)
 *                         0            (@clu doesn't have next cluster)
 */
static uint32_t exfat_get_fat(uint32_t clu)
{
	uint32_t ret;
	size_t entry_per_sector = info.sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info.fat_offset +  clu / entry_per_sector) * info.sector_size;
	uint32_t *fat;
	uint32_t offset = (clu) % entry_per_sector;

	fat = malloc(info.sector_size);
	get_sector(fat, fat_index, 1);
	/* validate index */
	if (clu == EXFAT_BADCLUSTER) {
		ret = 0;
		pr_err("cluster: %u is bad cluster.\n", clu);
	} else if (clu == EXFAT_LASTCLUSTER) {
		ret = 0;
		pr_debug("cluster: %u is the last cluster of cluster chain.\n", clu);
	} else if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1) {
		ret = 0;
		pr_debug("cluster: %u is invalid.\n", clu);
	} else {
		ret = fat[offset];
		if (ret == EXFAT_LASTCLUSTER)
			ret = 0;
		else
			pr_debug("cluster: %u has chain. next is 0x%x.\n", clu, fat[offset]);
	}

	free(fat);
	return ret;
}

/**
 * exfat_set_fat - Update FAT Entry to any cluster
 * @clu:                    index of the cluster want to check
 * @entry:                  any cluster index
 *
 * @retrun:                 previous FAT entry
 */
static uint32_t exfat_set_fat(uint32_t clu, uint32_t entry)
{
	uint32_t ret;
	size_t entry_per_sector = info.sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info.fat_offset +  clu / entry_per_sector) * info.sector_size;
	uint32_t *fat;
	uint32_t offset = (clu) % entry_per_sector;

	if (clu > info.cluster_count + 1) {
		pr_info("This Filesystem doesn't have Entry %u\n", clu);
		return 0;
	}

	fat = malloc(info.sector_size);
	get_sector(fat, fat_index, 1);

	ret = fat[offset];
	fat[offset] = entry;

	set_sector(fat, fat_index, 1);
	pr_debug("Rewrite Entry(%u) 0x%x to 0x%x.\n", clu, ret, fat[offset]);

	free(fat);
	return ret;
}

/**
 * exfat_set_fat_chain - Change NoFatChain to FatChain in file
 * @f:                      file information pointer
 * @clu:                    first cluster
 *
 * @retrun:                 0 (success)
 */
static int exfat_set_fat_chain(struct exfat_fileinfo *f, uint32_t clu)
{
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	while (--cluster_num) {
		exfat_set_fat(clu, clu + 1);
		clu++;
	}
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* CLUSTER FUNCTION FUNCTION                                                                     */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_get_last_cluster - find last cluster in file
 * @f:                      file information pointer
 * @clu:                    first cluster
 *
 * @return                  Last cluster
 *                          -1 (Failed)
 */
static int exfat_get_last_cluster(struct exfat_fileinfo *f, uint32_t clu)
{
	int i;
	uint32_t next_clu;
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN)
		return clu + cluster_num - 1;

	/* FAT_CHAIN */
	for (i = 0; i < cluster_num; i++) {
		next_clu = exfat_get_fat(clu);
		if (!next_clu)
			return clu;
		clu = next_clu;
	}
	return -1;
}

/**
 * exfat_alloc_clusters - Allocate cluster to file
 * @f:                    file information pointer
 * @clu:                  first cluster
 * @num_alloc:            number of cluster
 *
 * @return                the number of allocated cluster
 */
static int exfat_alloc_clusters(struct exfat_fileinfo *f, uint32_t clu, size_t num_alloc)
{
	uint32_t tmp = clu;
	uint32_t next_clu;
	uint32_t last_clu;
	int total_alloc = num_alloc;
	bool nofatchain = true;

	clu = next_clu = last_clu = exfat_get_last_cluster(f, clu);
	for (next_clu = last_clu + 1; next_clu != last_clu; next_clu++) {
		if (next_clu > info.cluster_count - 1)
			next_clu = EXFAT_FIRST_CLUSTER;

		if (exfat_load_bitmap(next_clu))
			continue;

		if (nofatchain && (next_clu - clu != 1))
			nofatchain = false;
		exfat_set_fat(next_clu, EXFAT_LASTCLUSTER);
		exfat_set_fat(clu, next_clu);
		exfat_save_bitmap(next_clu, 1);
		clu = next_clu;
		if (--total_alloc == 0)
			break;

	}
	if ((f->flags & ALLOC_NOFATCHAIN) && !nofatchain) {
		f->flags &= ~ALLOC_NOFATCHAIN;
		exfat_set_fat_chain(f, tmp);
	}
	f->datalen += num_alloc * info.cluster_size;
	exfat_update_filesize(f, tmp);
	return total_alloc;
}

/**
 * exfat_free_clusters - Free cluster in file
 * @f:                   file information pointer
 * @clu:                 first cluster
 * @num_alloc:           number of cluster
 *
 * @return               0 (success)
 */
static int exfat_free_clusters(struct exfat_fileinfo *f, uint32_t clu, size_t num_alloc)
{
	int i;
	uint32_t tmp = clu;
	uint32_t next_clu;
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		for (i = cluster_num - num_alloc; i < cluster_num; i++)
			exfat_save_bitmap(clu + i, 0);
		return 0;
	}

	/* FAT_CHAIN */
	for (i = 0; i < cluster_num - num_alloc - 1; i++)
		clu = exfat_get_fat(clu);

	while (i++ < cluster_num - 1) {
		next_clu = exfat_get_fat(clu);
		exfat_set_fat(clu, EXFAT_LASTCLUSTER);
		exfat_save_bitmap(next_clu, 0);
		clu = next_clu;
	}

	f->datalen -= num_alloc * info.cluster_size;
	exfat_update_filesize(f, tmp);
	return 0;
}

/**
 * exfat_new_cluster - Prepare to new cluster
 * @num_alloc:         number of cluster
 *
 * @return             allocated first cluster index
 */
static int exfat_new_clusters(size_t num_alloc)
{
	uint32_t next_clu, clu;
	uint32_t fst_clu = 0;

	for (next_clu = EXFAT_FIRST_CLUSTER; next_clu < info.cluster_count; next_clu++) {
		if (exfat_load_bitmap(next_clu))
			continue;

		if (!fst_clu) {
			fst_clu = clu = next_clu;
			exfat_set_fat(fst_clu, EXFAT_LASTCLUSTER);
			exfat_save_bitmap(fst_clu, 1);
		} else {
			exfat_set_fat(next_clu, EXFAT_LASTCLUSTER);
			exfat_set_fat(clu, next_clu);
			exfat_save_bitmap(clu, 1);
			clu = next_clu;
		}

		if (--num_alloc == 0)
			break;
	}

	return fst_clu;
}
/*************************************************************************************************/
/*                                                                                               */
/* DIRECTORY CHAIN FUNCTION                                                                      */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_print_cache - print directory chain
 */
static void exfat_print_cache(void)
{
	int i;
	node2_t *tmp;
	struct exfat_fileinfo *f;

	for (i = 0; i < info.root_size && info.root[i]; i++) {
		tmp = info.root[i];
		f = (struct exfat_fileinfo *)info.root[i]->data;
		pr_msg("%-16s(%u) | ", f->name, tmp->index);
		while (tmp->next != NULL) {
			tmp = tmp->next;
			f = (struct exfat_fileinfo *)tmp->data;
			pr_msg("%s(%u) ", f->name, tmp->index);
		}
		pr_msg("\n");
	}
	pr_msg("\n");
}

/**
 * exfat_check_cache - check whether @index has already loaded
 * @clu:                index of the cluster
 *
 * @retrun:             1 (@clu has loaded)
 *                      0 (@clu hasn't loaded)
 */
static int exfat_check_cache(uint32_t clu)
{
	int i;

	for (i = 0; info.root[i] && i < info.root_size; i++) {
		if (info.root[i]->index == clu)
			return 1;
	}
	return 0;
}

/**
 * exfat_get_cache - get directory chain index by argument
 * @clu:             index of the cluster
 *
 * @return:          directory chain index
 *                   Start of unused area (if doesn't lookup directory cache)
 */
static int exfat_get_cache(uint32_t clu)
{
	int i;

	for (i = 0; i < info.root_size && info.root[i]; i++) {
		if (info.root[i]->index == clu)
			return i;
	}

	info.root_size += DENTRY_LISTSIZE;
	node2_t **tmp = realloc(info.root, sizeof(node2_t *) * info.root_size);
	if (tmp) {
		info.root = tmp;
		info.root[i] = NULL;
	} else {
		pr_warn("Can't expand directory chain, so delete last chain.\n");
		delete_node2(info.root[--i]);
	}

	return i;
}

/**
 * exfat_traverse_directory - function to traverse one directory
 * @clu:                      index of the cluster want to check
 *
 * @return                    0 (success)
 *                           -1 (failed to read)
 */
static int exfat_traverse_directory(uint32_t clu)
{
	int i, j, name_len;
	uint8_t remaining;
	uint16_t uniname[MAX_NAME_LENGTH] = {0};
	size_t index = exfat_get_cache(clu);
	struct exfat_fileinfo *f = (struct exfat_fileinfo *)info.root[index]->data;
	size_t entries = info.cluster_size / sizeof(struct exfat_dentry);
	size_t cluster_num = 1;
	void *data;
	struct exfat_dentry d, next, name;

	if (f->cached) {
		pr_debug("Directory %s was already traversed.\n", f->name);
		return 0;
	}

	data = malloc(info.cluster_size);
	get_cluster(data, clu);

	cluster_num = exfat_concat_cluster(f, clu, &data);
	entries = (cluster_num * info.cluster_size) / sizeof(struct exfat_dentry);

	for (i = 0; i < entries; i++) {
		d = ((struct exfat_dentry *)data)[i];

		switch (d.EntryType) {
			case DENTRY_UNUSED:
				break;
			case DENTRY_BITMAP:
				exfat_load_bitmap_cluster(d);
				break;
			case DENTRY_UPCASE:
				exfat_load_upcase_cluster(d);
				break;
			case DENTRY_VOLUME:
				exfat_load_volume_label(d);
				break;
			case DENTRY_FILE:
				remaining = d.dentry.file.SecondaryCount;
				/* Stream entry */
				next = ((struct exfat_dentry *)data)[i + 1];
				while ((!(next.EntryType & EXFAT_INUSE)) && (next.EntryType != DENTRY_UNUSED)) {
					pr_debug("This entry was deleted (0x%x).\n", next.EntryType);
					next = ((struct exfat_dentry *)data)[++i + 1];
				}
				if (next.EntryType != DENTRY_STREAM) {
					pr_info("File should have stream entry, but This don't have.\n");
					continue;
				}
				/* Filename entry */
				name = ((struct exfat_dentry *)data)[i + 2];
				while ((!(name.EntryType & EXFAT_INUSE)) && (name.EntryType != DENTRY_UNUSED)) {
					pr_debug("This entry was deleted (0x%x).\n", name.EntryType);
					name = ((struct exfat_dentry *)data)[++i + 2];
				}
				if (name.EntryType != DENTRY_NAME) {
					pr_info("File should have name entry, but This don't have.\n");
					return -1;
				}
				name_len = next.dentry.stream.NameLength;
				for (j = 0; j < remaining - 1; j++) {
					name_len = MIN(ENTRY_NAME_MAX,
							next.dentry.stream.NameLength - j * ENTRY_NAME_MAX);
					memcpy(uniname + j * ENTRY_NAME_MAX,
							(((struct exfat_dentry *)data)[i + 2 + j]).dentry.name.FileName,
							name_len * sizeof(uint16_t));
				}

				exfat_create_cache(info.root[index], clu,
						&d, &next, uniname);
				i += remaining;
				break;
		}
	}
	free(data);
	return 0;
}

/**
 * exfat_clean_cache - function to clean opeartions
 * @index:              directory chain index
 *
 * @return              0 (success)
 *                     -1 (already released)
 */
static int exfat_clean_cache(uint32_t index)
{
	node2_t *tmp;
	struct exfat_fileinfo *f;

	if ((!info.root[index])) {
		pr_warn("index %d was already released.\n", index);
		return -1;
	}

	tmp = info.root[index];

	while (tmp->next != NULL) {
		tmp = tmp->next;
		f = (struct exfat_fileinfo *)tmp->data;
		free(f->name);
		f->name = NULL;
	}
	free_list2(info.root[index]);
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FILE FUNCTION                                                                                 */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_create_cache - Create file infomarion
 * @head:               Directory chain head
 * @clu:                parent Directory cluster index
 * @file:               file dentry
 * @stream:             stream Extension dentry
 * @uniname:            File Name dentry
 */
static void exfat_create_cache(node2_t *head, uint32_t clu,
		struct exfat_dentry *file, struct exfat_dentry *stream, uint16_t *uniname)
{
	int index, next_index = stream->dentry.stream.FirstCluster;
	struct exfat_fileinfo *f;
	size_t namelen = stream->dentry.stream.NameLength;

	f = malloc(sizeof(struct exfat_fileinfo));
	memset(f, '\0', sizeof(struct exfat_fileinfo));
	f->name = malloc(namelen * UTF8_MAX_CHARSIZE + 1);
	memset(f->name, '\0', namelen * UTF8_MAX_CHARSIZE + 1);

	exfat_convert_uniname(uniname, namelen, f->name);
	f->namelen = namelen;
	f->datalen = stream->dentry.stream.DataLength;
	f->attr = file->dentry.file.FileAttributes;
	f->flags = stream->dentry.stream.GeneralSecondaryFlags;
	f->hash = stream->dentry.stream.NameHash;

	exfat_convert_unixtime(&f->ctime, file->dentry.file.CreateTimestamp,
			file->dentry.file.Create10msIncrement,
			file->dentry.file.CreateUtcOffset);
	exfat_convert_unixtime(&f->mtime, file->dentry.file.LastModifiedTimestamp,
			file->dentry.file.LastModified10msIncrement,
			file->dentry.file.LastModifiedUtcOffset);
	exfat_convert_unixtime(&f->atime, file->dentry.file.LastAccessedTimestamp,
			0,
			file->dentry.file.LastAccessdUtcOffset);
	append_node2(head, next_index, f);
	((struct exfat_fileinfo *)(head->data))->cached = 1;

	/* If this entry is Directory, prepare to create next chain */
	if ((f->attr & ATTR_DIRECTORY) && (!exfat_check_cache(next_index))) {
		struct exfat_fileinfo *d = malloc(sizeof(struct exfat_fileinfo));
		d->name = malloc(f->namelen + 1);
		strncpy((char *)d->name, (char *)f->name, f->namelen + 1);
		d->namelen = namelen;
		d->datalen = stream->dentry.stream.DataLength;
		d->attr = file->dentry.file.FileAttributes;
		d->flags = stream->dentry.stream.GeneralSecondaryFlags;
		d->hash = stream->dentry.stream.NameHash;

		index = exfat_get_cache(next_index);
		info.root[index] = init_node2(next_index, d);
	}
}

/**
 * exfat_calculate_checksum - Calculate file entry Checksum
 * @entry:                    points to an in-memory copy of the directory entry set
 * @count:                    the number of secondary directory entries
 *
 * @return                    Checksum
 */
static uint16_t exfat_calculate_checksum(unsigned char *entry, unsigned char count)
{
	uint16_t bytes = ((uint16_t)count + 1) * 32;
	uint16_t checksum = 0;
	uint16_t index;

	for (index = 0; index < bytes; index++) {
		if ((index == 2) || (index == 3))
			continue;
		checksum = ((checksum & 1) ? 0x8000 : 0) + (checksum >> 1) +  (uint16_t)entry[index];
	}
	return checksum;
}

/**
 * exfat_calculate_Tablechecksum - Calculate Up-case table Checksum
 * @entry:                         points to an in-memory copy of the directory entry set
 * @count:                         the number of secondary directory entries
 *
 * @return                         Checksum
 */
static uint32_t exfat_calculate_tablechecksum(unsigned char *table, uint64_t length)
{
	uint32_t checksum = 0;
	uint64_t index;

	for (index = 0; index < length; index++)
		checksum = ((checksum & 1) ? 0x80000000 : 0) + (checksum >> 1) + (uint32_t)table[index];

	return checksum;
}

/**
 * exfat_calculate_namehash - Calculate name hash
 * @name:                     points to an in-memory copy of the up-cased file name
 * @len:                      Name length
 *
 * @return                    NameHash
 */
static uint16_t exfat_calculate_namehash(uint16_t *name, uint8_t len)
{
	unsigned char* buffer = (unsigned char *)name;
	uint16_t bytes = (uint16_t)len * 2;
	uint16_t hash = 0;
	uint16_t index;

	for (index = 0; index < bytes; index++)
		hash = ((hash & 1) ? 0x8000 : 0) + (hash >> 1) + (uint16_t)buffer[index];

	return hash;
}

/**
 * exfat_update_filesize - flush filesize to disk
 * @f:                     file information pointer
 * @clu:                   first cluster
 *
 * @return                  0 (success)
 *                         -1 (Failed)
 */
static int exfat_update_filesize(struct exfat_fileinfo *f, uint32_t clu)
{
	int i, j;
	uint32_t parent_clu = 0;
	size_t cluster_num;
	struct exfat_fileinfo *dir;
	struct exfat_dentry d;
	void *data;

	if (clu == info.root_offset)
		return 0;

	for (i = 0; i < info.root_size && info.root[i]; i++) {
		if (search_node2(info.root[i], clu)) {
			parent_clu = info.root[i]->index;
			dir = info.root[i]->data;
			break;
		}
	}

	if (!parent_clu) {
		pr_err("Can't find cluster %u parent directory.\n", clu);
		return -1;
	}

	cluster_num = (dir->datalen + (info.cluster_size - 1)) / info.cluster_size;
	data = malloc(info.cluster_size);

	for (i = 0; i < cluster_num; i++) {
		get_cluster(data, parent_clu);
		for (j = 0; j < (info.cluster_size / sizeof(struct exfat_dentry)); j++) {
			d = ((struct exfat_dentry *)data)[j];
			if (d.EntryType == DENTRY_STREAM && d.dentry.stream.FirstCluster == clu) {
				d.dentry.stream.DataLength = f->datalen;
				d.dentry.stream.GeneralSecondaryFlags = f->flags;
				goto out;
			}
		}
		/* traverse next cluster */
		if (dir->flags & ALLOC_NOFATCHAIN)
			parent_clu++;
		else
			parent_clu = exfat_get_fat(parent_clu);
	}
	parent_clu = 0;
out:
	set_cluster(data, parent_clu);
	free(data);
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FILE NAME FUNCTION                                                                            */
/*                                                                                               */
/*************************************************************************************************/
/**
 * exfat_convert_unixname - function to get timestamp in file
 * @t:                      output pointer (Output)
 * @time:                   Timestamp Field in File Directory Entry
 * @subsec:                 10msincrement Field in File Directory Entry
 * @tz:                     UtcOffset in File Directory Entry
 */
static void exfat_convert_unixtime(struct tm *t, uint32_t time, uint8_t subsec, uint8_t tz)
{
	t->tm_year = (time >> EXFAT_YEAR) & 0x7f;
	t->tm_mon  = (time >> EXFAT_MONTH) & 0x0f;
	t->tm_mday = (time >> EXFAT_DAY) & 0x1f;
	t->tm_hour = (time >> EXFAT_HOUR) & 0x1f;
	t->tm_min  = (time >> EXFAT_MINUTE) & 0x3f;
	t->tm_sec  = (time & 0x1f) * 2;
	t->tm_sec += subsec / 100;
	/* OffsetValid */
	if (tz & 0x80) {
		int min = 0;
		time_t tmp_time = mktime(t);
		struct tm *t2;
		min = exfat_convert_timezone(tz);
		tmp_time += (min * 60);
		t2 = localtime(&tmp_time);
		*t = *t2;
	}
}

/**
 * exfat_convert_timezone - function to get timezone in file
 * @tz:                     UtcOffset in File Directory Entry
 *
 * @return                  difference minutes from timezone
 */
static int exfat_convert_timezone(uint8_t tz)
{
	int ex_min = 0;
	int ex_hour = 0;
	char offset = tz & 0x7f;

	/* OffsetValid */
	if (!(tz & 0x80))
		return 0;
	/* negative value */
	if (offset & 0x40) {
		offset = ((~offset) + 1) & 0x7f;
		ex_min = ((offset % 4) * 15) * -1;
		ex_hour = (offset / 4) * -1;
	} else {
		ex_min = (offset % 4) * 15;
		ex_hour = offset / 4;
	}
	return ex_min + ex_hour * 60;
}

/**
 * exfat_convert_uniname - function to get filename
 * @uniname:               filename dentry in UTF-16
 * @name_len:              filename length
 * @name:                  filename in UTF-8 (Output)
 */
static void exfat_convert_uniname(uint16_t *uniname, uint64_t name_len, unsigned char *name)
{
	utf16s_to_utf8s(uniname, name_len, name);
}

/**
 * exfat_convert_upper - convert character to upper-character
 * @c:                   character in UTF-16
 *
 * @return:              upper character
 */
static uint16_t exfat_convert_upper(uint16_t c)
{
	return info.upcase_table[c] ? info.upcase_table[c] : c;
}

/**
 * exfat_convert_upper_character - convert string to upper-string
 * @src:                           Target characters in UTF-16
 * @len:                           Target characters length
 * @dist:                          convert result in UTF-16 (Output)
 */
static void exfat_convert_upper_character(uint16_t *src, size_t len, uint16_t *dist)
{
	int i;

	if (!info.upcase_table || (info.upcase_size == 0))
		exfat_load_extra_entry();

	for (i = 0; i < len; i++)
		dist[i] = exfat_convert_upper(src[i]);
}
