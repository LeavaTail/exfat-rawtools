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
#include <asm/byteorder.h>
#include <sys/stat.h>
#include "bitmap.h"
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
 * @return       == 0 (success)
 *               <  0 (failed)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_sector(void *data, off_t index, size_t count)
{
	size_t sector_size = info.sector_size;

	pr_debug("Get: Sector from 0x%lx to 0x%lx\n", index , index + (count * sector_size) - 1);
	if ((pread(info.fd, data, count * sector_size, index)) < 0) {
		pr_err("read: %s\n", strerror(errno));
		return -errno;
	}
	return 0;
}

/**
 * set_sector - Set Raw-Data from any sector
 * @data:       Sector raw data
 * @index:      Start bytes
 * @count:      The number of sectors
 *
 * @return      == 0 (success)
 *              <  0 (failed)
 *
 * NOTE: Need to allocate @data before call it.
 */
int set_sector(void *data, off_t index, size_t count)
{
	size_t sector_size = info.sector_size;

	pr_debug("Set: Sector from 0x%lx to 0x%lx\n", index, index + (count * sector_size) - 1);
	if ((pwrite(info.fd, data, count * sector_size, index)) < 0) {
		pr_err("write: %s\n", strerror(errno));
		return -errno;
	}
	return 0;
}

/**
 * get_cluster - Get Raw-Data from any cluster
 * @data:        cluster raw data (Output)
 * @index:       Start cluster index
 *
 * @return       == 0 (success)
 *               <  0 (failed)
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
 * @return       == 0 (success)
 *               <  0 (failed)
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
 * @return        == 0 (success)
 *                <  0 (failed)
 *
 * NOTE: Need to allocate @data before call it.
 */
int get_clusters(void *data, off_t index, size_t num)
{
	size_t clu_per_sec = info.cluster_size / info.sector_size;
	off_t heap_start = info.heap_offset * info.sector_size;

	if (index < EXFAT_FIRST_CLUSTER || index + num > info.cluster_count) {
		pr_err("Internal Error: invalid cluster range %lu ~ %lu.\n", index, index + num - 1);
		return -EINVAL;
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
 * @return        == 0 (success)
 *                <  0 (failed to read)
 *
 * NOTE: Need to allocate @data before call it.
 */
int set_clusters(void *data, off_t index, size_t num)
{
	size_t clu_per_sec = info.cluster_size / info.sector_size;
	off_t heap_start = info.heap_offset * info.sector_size;

	if (index < EXFAT_FIRST_CLUSTER || index + num > info.cluster_count) {
		pr_err("Internal Error: invalid cluster range %lu ~ %lu.\n", index, index + num - 1);
		return -EINVAL;
	}

	return set_sector(data,
			heap_start + ((index - 2) * info.cluster_size),
			clu_per_sec * num);
}

/*************************************************************************************************/
/*                                                                                               */
/* SUPERBLOCK FUNCTION                                                                           */
/*                                                                                               */
/*************************************************************************************************/

/**
 * init_device_info - Initialize member in struct device_info
 *
 * @return            == 0 (success)
 *                    <  0 (failed)
 */
int exfat_init_info(void)
{
	info.fd = -1;
	info.total_size = 0;
	info.partition_offset = 0;
	info.vol_size = 0;
	info.sector_size = SECTORSIZE;
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
	info.vol_label = calloc(sizeof(uint16_t), 11);
	info.vol_length = 0;
	info.root_size = DENTRY_LISTSIZE;
	info.root = calloc(info.root_size, sizeof(node2_t *));

	if (!info.vol_label || !info.root)
		return -ENOMEM;

	return 0;
} 

/**
 * exfat_store_filesystem - store BootSector to exfat_info
 * @boot:                   boot sector pointer
 *
 * @return:                 == 0 (Success)
 *                          <  0 (failed to read)
 */
int exfat_store_info(struct exfat_bootsec *b)
{
	int ret = 0;
	struct stat s;
	struct exfat_fileinfo *f;

	if (fstat(info.fd, &s) < 0) {
		pr_err("stat: %s\n", strerror(errno));
		return -errno;
	}

	if ((f = calloc(sizeof(struct exfat_fileinfo), 1)) == NULL)
		return -ENOMEM;
	if ((f->name = calloc(sizeof(unsigned char *), (strlen("/") + 1))) == NULL) {
		free(f);
		return -ENOMEM;
	}

	info.total_size = s.st_size;
	info.partition_offset = cpu_to_le64(b->PartitionOffset);
	info.vol_size = cpu_to_le64(b->VolumeLength);
	info.sector_size = 1 << b->BytesPerSectorShift;
	info.cluster_size = (1 << b->SectorsPerClusterShift) * info.sector_size;
	info.cluster_count = cpu_to_le32(b->ClusterCount);
	info.fat_offset = cpu_to_le32(b->FatOffset);
	info.fat_length = b->NumberOfFats * cpu_to_le32(b->FatLength) * info.sector_size;
	info.heap_offset = cpu_to_le32(b->ClusterHeapOffset);
	info.root_offset = cpu_to_le32(b->FirstClusterOfRootDirectory);
	info.root[0] = init_node2(info.root_offset, f);
	if (!info.root[0]) {
		free(f->name);
		free(f);
		return -ENOMEM;
	}

	strncpy((char *)f->name, "/", strlen("/") + 1);
	f->namelen = strlen("/");
	f->datalen = info.cluster_count * info.cluster_size;
	f->attr = ATTR_DIRECTORY;
	f->clu = cpu_to_le32(b->FirstClusterOfRootDirectory);

	return ret;
}

/**
 * exfat_clean_info - function to clean opeartions
 *
 * @return            0 (success)
 */
int exfat_clean_info(void)
{
	int index;
	node2_t *tmp;
	struct exfat_fileinfo *f;

	free(info.alloc_table);
	free(info.upcase_table);
	free(info.vol_label);

	for(index = 0; index < info.root_size && info.root[index]; index++) {
		tmp = info.root[index];
		f = (struct exfat_fileinfo *)tmp->data;
		free(f->name);
		f->name = NULL;
		exfat_clean_cache(index);
		free(tmp->data);
		tmp->data = NULL;
		free(tmp);
	}
	free(info.root);

	info.alloc_table = NULL;
	info.upcase_table = NULL;
	info.vol_label = NULL;
	info.root = NULL;

	if (info.fd != -1)
		close(info.fd);

	return 0;
}

/**
 * exfat_load_bootsec - load boot sector
 * @b:                  boot sector pointer in exFAT (Output)
 *
 * @return:             == 0 (Success)
 *                      <  0 (failed)
 */
int exfat_load_bootsec(struct exfat_bootsec *b)
{
	if (get_sector(b, 0, 1))
		return -EIO;

	return exfat_check_bootsec(b);
}

/**
 * exfat_check_bootsec - verify boot sector
 * @b:                   boot sector pointer in exFAT (Output)
 *
 * @return:              == 0 (Success)
 *                       <  0 (failed)
 */
int exfat_check_bootsec(struct exfat_bootsec *b)
{
	int ret = 0;
	uint8_t zero[sizeof(struct exfat_bootsec)] = {0};
	uint8_t bps = b->BytesPerSectorShift;
	uint8_t spc = b->SectorsPerClusterShift;
	uint32_t fatoff = cpu_to_le32(b->FatOffset);
	uint32_t fatlen = cpu_to_le32(b->FatLength);
	uint64_t vollen = cpu_to_le64(b->VolumeLength);
	uint32_t cluoff = cpu_to_le32(b->ClusterHeapOffset);
	uint32_t clucnt = cpu_to_le32(b->ClusterCount);
	uint32_t rootclu = cpu_to_le32(b->FirstClusterOfRootDirectory);

	if ((b->JumpBoot[0] != 0xEB) || (b->JumpBoot[1] != 0x76) || (b->JumpBoot[2] != 0x90)) {
		pr_err("invalid JumpBoot: 0x%x%x%x\n", b->JumpBoot[0], b->JumpBoot[1], b->JumpBoot[2]);
		ret = -EINVAL;
	}

	if (memcmp(b->FileSystemName, "EXFAT   ", 8)) {
		pr_err("invalid FileSystemName: \"%8s\"\n", b->FileSystemName);
		ret = -EINVAL;
	}

	if (memcmp(b->MustBeZero, &zero, 53)) {
		pr_err("invalid MustBeZero \"%53s\"\n", b->MustBeZero);
		ret = -EINVAL;
	}

	if (vollen < (power2(20) / power2(bps))) {
		pr_err("invalid VolumeLength: %" PRIu64 "\n", vollen);
		ret = -EINVAL;
	}

	if ((fatoff < 24) || ((cluoff - fatlen * b->NumberOfFats) < fatoff)) {
		pr_err("invalid FatOffset: 0x%x\n", b->FatOffset);
		ret = -EINVAL;
	}

	if ((fatlen < (clucnt + EXFAT_FIRST_CLUSTER) * 2 / power2(bps - 1))
		|| (((cluoff - fatoff) / b->NumberOfFats) < fatoff)) {
		pr_err("invalid FatLength : 0x%x\n", fatlen);
		ret = -EINVAL;
	}

	if ((cluoff < (fatoff + fatlen * b->NumberOfFats)) || ((clucnt * power2(bps)) < cluoff)) {
		pr_err("invalid ClusterHeapOffset : 0x%x\n", cluoff);
		ret = -EINVAL;
	}

	if (((vollen - cluoff) / power2(b->SectorsPerClusterShift) != clucnt)
		&& (power2(32) - 11 != clucnt)) {
		pr_err("invalid ClusterCount: 0x%x\n", clucnt);
		ret = -EINVAL;
	}

	if (cpu_to_le16(b->FileSystemRevision) < 0x0100) {
		pr_err("invalid FileSystemRevision: 0x%04x\n", cpu_to_le16(b->FileSystemRevision));
		ret = -EINVAL;
	}

	if ((rootclu < 2) || (clucnt + 1 < rootclu)) {
		pr_err("invalid FirstClusterOfRootDirectory: 0x%x\n", rootclu);
		ret = -EINVAL;
	}

	if ((bps < 9) || (12 < bps)) {
		pr_err("invalid BytesPerSectorShift: 0x%x\n", bps);
		ret = -EINVAL;
	}

	if (25 - bps < spc) {
		pr_err("invalid SectorsPerClusterShift: 0x%x\n", spc);
		ret = -EINVAL;
	}

	if ((b->NumberOfFats != 1) && (b->NumberOfFats != 2)) {
		pr_err("invalid NumberOfFats: 0x%x\n", b->NumberOfFats);
		ret = -EINVAL;
	}

	if ((b->PercentInUse > 100) && (b->PercentInUse != 0xFF)) {
		pr_err("invalid PercentInUse: 0x%x\n", b->PercentInUse);
		ret = -EINVAL;
	}

	if ((b->BootSignature != EXFAT_SIGNATURE)) {
		pr_err("invalid BootSignature: 0x%x\n", b->BootSignature);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * exfat_check_extend_bootsec - verify extended boot sector
 *
 * @return                      == 0 (success)
 *                              <  0 (failed)
 */
int exfat_check_extend_bootsec(void)
{
	int i;
	int ret = 0;
	uint32_t *b;
	int index = info.sector_size / sizeof(uint32_t) - 1;

	if ((b = calloc(info.sector_size, 1)) == NULL)
		return -ENOMEM;

	for (i = 0; i < 8; i++) {
		if (get_sector(b, info.sector_size * (i + 1), 1)) {
			free(b);
			return -EIO;
		}
		if (cpu_to_le32(b[index]) != EXFAT_EXSIGNATURE) {
			pr_err("invalid ExtendedBootSignature: 0x%08x\n", cpu_to_le32(b[index]));
			ret = -EINVAL;
		}
	}

	free(b);
	return ret;
}

/**
 * exfat_check_bootchecksum - verify Main Boot region checksum
 *
 * @return                    == 0 (success)
 *                            <  0 (failed)
 */
int exfat_check_bootchecksum(void)
{
	int i;
	uint8_t *b;
	uint32_t *bootchecksum = NULL;
	uint32_t checksum = 0;

	if ((b = calloc(info.sector_size, 11)) == NULL)
		return -ENOMEM;

	if (get_sector(b, 0, 11)) {
		free(b);
		return -EIO;
	}
	checksum = exfat_calculate_bootchecksum(b, info.sector_size);

	if ((bootchecksum = calloc(info.sector_size, 1)) == NULL) {
		free(b);
		return -ENOMEM;
	}

	if (get_sector(bootchecksum, info.sector_size * 11, 1)) {
		free(bootchecksum);
		free(b);
		return -EIO;
	}

	for (i = 0; i < info.sector_size / sizeof(uint32_t); i++) {
		if (cpu_to_le32(bootchecksum[i]) != checksum) {
			pr_err("Boot region checksum(%08x) is unmatched.\n", checksum);
			free(bootchecksum);
			free(b);
			return -EINVAL;
		}
	}

	free(b);
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FAT-ENTRY FUNCTION                                                                            */
/*                                                                                               */
/*************************************************************************************************/

/**
 * exfat_get_fat - Whether or not cluster is continuous
 * @clu:           index of the cluster want to check
 * @entry:         any cluster index (Output)
 *
 * @return         == 0 (success)
 *                 <  0 (failed)
 */
int exfat_get_fat(uint32_t clu, uint32_t *entry)
{
	int ret = -EINVAL;
	size_t entry_per_sector = info.sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info.fat_offset +  clu / entry_per_sector) * info.sector_size;
	uint32_t *fat;
	uint32_t offset = (clu) % entry_per_sector;

	if ((fat = malloc(info.sector_size)) == NULL)
		return -ENOMEM;;
	if (get_sector(fat, fat_index, 1))
		goto out;

	if (clu == EXFAT_BADCLUSTER)
		pr_err("Internal Error: Cluster %x is bad cluster.\n", clu);
	else if (clu == EXFAT_LASTCLUSTER)
		pr_err("Internal Error: Cluster: %u is the last cluster.\n", clu);
	else if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1)
		pr_err("Internal Error: Cluster %u is invalid.\n", clu);
	else
		ret = 0;

	if (!ret) {
		*entry = cpu_to_le32(fat[offset]);
		pr_debug("Get FAT[%u]  0x%x.\n", clu, *entry);
	}

out:
	free(fat);
	return ret;
}

/**
 * exfat_set_fat - Update FAT Entry to any cluster
 * @clu:           index of the cluster want to check
 * @entry:         any cluster index
 *
 * @return         == 0 (success)
 *                 <  0 (failed)
 */
int exfat_set_fat(uint32_t clu, uint32_t entry)
{
	uint32_t ret = -EINVAL;
	size_t entry_per_sector = info.sector_size / sizeof(uint32_t);
	uint32_t fat_index = (info.fat_offset +  clu / entry_per_sector) * info.sector_size;
	uint32_t *fat;
	uint32_t offset = (clu) % entry_per_sector;

	if ((fat = malloc(info.sector_size)) == NULL)
		return -ENOMEM;
	if (get_sector(fat, fat_index, 1))
		goto out;

	ret = fat[offset];

	if (clu == EXFAT_BADCLUSTER || entry == EXFAT_BADCLUSTER)
		pr_err("Internal Error: Cluster %x or Entry %x is bad cluster.\n", clu, entry);
	else if (clu == EXFAT_LASTCLUSTER)
		pr_err("Internal Error: Cluster: %u is the last cluster.\n", clu);
	else if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1)
		pr_err("Internal Error: Cluster %u is invalid.\n", clu);
	else if (entry < EXFAT_FIRST_CLUSTER || entry > info.cluster_count + 1)
		pr_err("Internal Error: Entry %u is invalid.\n", entry);
	else
		ret = 0;
	
	if (!ret) {
		fat[offset] = cpu_to_le32(entry);
		set_sector(fat, fat_index, 1);
		pr_debug("Set FAT[%u]  0x%x -> 0x%x.\n", clu, ret, fat[offset]);
	}

out:
	free(fat);
	return ret;
}

/**
 * exfat_set_fat_chain - Change NoFatChain to FatChain in file
 * @f:                   file information pointer
 * @clu:                 first cluster
 *
 * @return               == 0 (success)
 *                       <  0 (failed)
 */
int exfat_set_fat_chain(struct exfat_fileinfo *f, uint32_t clu)
{
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);

	while (--cluster_num) {
		if (exfat_set_fat(clu, clu + 1))
			return -EINVAL;
		clu++;
	}
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* CLUSTER FUNCTION                                                                              */
/*                                                                                               */
/*************************************************************************************************/

/**
 * exfat_alloc_clusters - Allocate cluster to file
 * @f:                    file information pointer
 * @clu:                  first cluster
 * @num_alloc:            number of cluster
 *
 * @return                the number of allocated cluster
 */
int exfat_alloc_clusters(struct exfat_fileinfo *f, uint32_t clu, size_t num_alloc)
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
int exfat_free_clusters(struct exfat_fileinfo *f, uint32_t clu, size_t num_alloc)
{
	int i;
	uint32_t tmp = clu;
	uint32_t next_clu;
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		for (i = cluster_num - num_alloc; i < cluster_num; i++)
			exfat_save_bitmap(clu + i, 0);
		return 0;
	}

	/* FAT_CHAIN */
	for (i = 0; i < cluster_num - num_alloc - 1; i++) 
		if (exfat_get_fat(tmp, &tmp))
			break;

	while (i++ < cluster_num - 1) {
		exfat_get_fat(clu, &next_clu);
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
int exfat_new_clusters(size_t num_alloc)
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

/**
 * exfat_concat_cluster_fast - Contatenate cluster @data with next_cluster (Only FAT_CHAIN)
 * @clu:                       index of the cluster
 * @data:                      The cluster (Output)
 * @len:                       Data length
 *
 * @retrun:                    cluster count (@clu has next cluster)
 *                             0             (@clu doesn't have next cluster, or failed to realloc)
 */
uint32_t exfat_concat_cluster_fast(uint32_t clu, void **data, size_t len)
{
	void *tmp;
	uint32_t next_clu;
	size_t allocated;
	size_t cluster_num = ROUNDUP(len, info.cluster_size);

	if (cluster_num <= 1)
		return cluster_num;

	if (!(tmp = realloc(*data, info.cluster_size * cluster_num)))
		return 0;
	*data = tmp;

	for (allocated = 1; allocated < cluster_num; allocated++) {
		if (exfat_get_fat(clu, &next_clu))
			break;
		get_cluster(*data + info.cluster_size * allocated, next_clu);
		clu = next_clu;
	}

	return allocated;
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
uint32_t exfat_concat_cluster(struct exfat_fileinfo *f, uint32_t clu, void **data)
{
	int i;
	void *tmp;
	uint32_t tmp_clu = clu;
	uint32_t next_clu;
	uint64_t allocated;
	uint64_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);
	bitmap_t b;

	if (cluster_num <= 1)
		return cluster_num;

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		if (!(tmp = realloc(*data, info.cluster_size * cluster_num)))
			return 0;
		*data = tmp;
		for (i = 1; i < cluster_num; i++) {
			if (exfat_load_bitmap(clu + i) != 0x1) {
				pr_err("Cluster #%u becomes allcation consistency. Ignore #%u ~ %" PRIu64 ".\n",
					clu, clu + i, clu + cluster_num - 1);
				break;
			}
		}
		get_clusters(*data + info.cluster_size, clu + 1, cluster_num - 1);
		return cluster_num;
	}

	init_bitmap(&b, info.cluster_count);

	/* FAT_CHAIN */
	for (allocated = 1; allocated < cluster_num; allocated++) { 
		if (!exfat_get_fat(tmp_clu, &tmp_clu))
			break;;
		if (get_bitmap(&b, tmp_clu - EXFAT_FIRST_CLUSTER)) {
			pr_err("Detected a loop in File (Cluster #%u).\n", clu);
			break;
		}
		set_bitmap(&b, tmp_clu - EXFAT_FIRST_CLUSTER);
		if (tmp_clu == EXFAT_LASTCLUSTER) {
			pr_err("File size(%" PRIu64 ") and FAT chain size(%" PRIu64 ") are un-matched.\n",
				f->datalen, allocated * info.cluster_size);
			break;
		}
		if (exfat_load_bitmap(tmp_clu) != 1) {
			pr_err("FAT and Allocation Bitmap are un-matched. Ignore #%u.\n", tmp_clu);
			break;
		}
	}

	free_bitmap(&b);

	if (!(tmp = realloc(*data, info.cluster_size * allocated)))
		return 0;
	*data = tmp;

	for (i = 1; i < allocated; i++) {
		if (exfat_get_fat(clu, &next_clu))
			break;
		get_cluster(*data + info.cluster_size * i, next_clu);
		clu = next_clu;
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
uint32_t exfat_set_cluster(struct exfat_fileinfo *f, uint32_t clu, void *data)
{
	size_t allocated = 0;
	size_t cluster_num = (f->datalen + (info.cluster_size - 1)) / info.cluster_size;

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		set_clusters(data, clu, cluster_num);
		return cluster_num;
	}

	/* FAT_CHAIN */
	for (allocated = 0; allocated < cluster_num; allocated++) {
		set_cluster(data + info.cluster_size * allocated, clu);
	}

	return allocated;
}

/**
 * exfat_check_last_cluster - check whether @clu is last cluster
 * @f:                        file information pointer
 * @clu:                      cluster
 *
 * @return                    1 (@clu is last cluster)
 *                            0 (@clu is not last cluster)
 */
int exfat_check_last_cluster(struct exfat_fileinfo *f, uint32_t clu)
{
	uint32_t next_clu;
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size - 1);

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN)
		return (clu == (f->clu + cluster_num - 1));

	/* FAT_CHAIN */
	exfat_get_fat(clu, &next_clu);
	return (next_clu == EXFAT_LASTCLUSTER);
}

/**
 * exfat_next_cluster - obtain next cluster
 * @f:                  file information pointer
 * @clu:                current cluster
 *
 * @return              Last cluster
 *                      0 (Failed)
 */
uint32_t exfat_next_cluster(struct exfat_fileinfo *f, uint32_t clu)
{
	uint32_t next_clu;
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);

	/* NO_FAT_CHAIN */
	if (f->flags & ALLOC_NOFATCHAIN) {
		if (clu + 1 >= f->clu + cluster_num)
			next_clu = EXFAT_LASTCLUSTER;
		else
			next_clu = clu + 1;
	} else {
	/* FAT_CHAIN */
		if (exfat_get_fat(clu, &next_clu))
			return 0;
	}

	if (next_clu != EXFAT_LASTCLUSTER && exfat_load_bitmap(next_clu) != 1) {
		pr_err("Cluster#%u isn't allocated.\n", next_clu);
		return 0;
	}

	return next_clu;
}

/**
 * exfat_get_last_cluster - find last cluster in file
 * @f:                      file information pointer
 * @clu:                    first cluster
 *
 * @return                  Last cluster
 *                          -1 (Failed)
 */
int exfat_get_last_cluster(struct exfat_fileinfo *f, uint32_t clu)
{
	int i;
	uint32_t next_clu;
	size_t cluster_num = ROUNDUP(f->datalen, info.cluster_size);

	for (i = 0; i < cluster_num; i++) {
		next_clu = exfat_next_cluster(f, clu);
		if (!next_clu)
			return -1;
		clu = next_clu;
	}
	return next_clu;
}

/*************************************************************************************************/
/*                                                                                               */
/* DIRECTORY CACHE FUNCTION                                                                      */
/*                                                                                               */
/*************************************************************************************************/

/**
 * exfat_print_cache - print directory cache
 */
void exfat_print_cache(void)
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
 * @clu:               index of the cluster
 *
 * @retrun:            1 (@clu has loaded)
 *                     0 (@clu hasn't loaded)
 */
int exfat_check_cache(uint32_t clu)
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
int exfat_get_cache(uint32_t clu)
{
	int i;

	for (i = 0; i < info.root_size && info.root[i]; i++) {
		if (info.root[i]->index == clu)
			return i;
	}

	info.root_size += DENTRY_LISTSIZE;
	node2_t **tmp = calloc(sizeof(node2_t **), info.root_size);
	if (tmp) {
		memcpy(tmp, info.root, info.root_size - DENTRY_LISTSIZE);
		free(info.root);
		info.root = tmp;
	} else {
		pr_warn("Can't expand directory chain, so delete last chain.\n");
		delete_node2(info.root[--i]);
	}

	return i;
}

/**
 * exfat_clean_cache - function to clean opeartions
 * @index:             directory chain index
 *
 * @return              0 (success)
 *                     -1 (already released)
 */
int exfat_clean_cache(uint32_t index)
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

/**
 * exfat_create_cache - Create file infomarion
 * @head:               Directory chain head
 * @clu:                parent Directory cluster index
 * @file:               file dentry
 * @stream:             stream Extension dentry
 * @uniname:            File Name dentry
 *
 * @return              == 0 (success)
 *                      <  0 (failed)
 */
int exfat_create_cache(node2_t *head, uint32_t clu,
		struct exfat_dentry *file, struct exfat_dentry *stream, uint16_t *uniname)
{
	int index, next_index = le32_to_cpu(stream->dentry.stream.FirstCluster);
	struct exfat_fileinfo *f;
	size_t namelen = stream->dentry.stream.NameLength;

	if ((f = calloc(sizeof(struct exfat_fileinfo), 1)) == NULL)
		return -ENOMEM;
	if ((f->name = malloc(namelen * UTF8_MAX_CHARSIZE + 1)) == NULL) {
		free(f);
		return -ENOMEM;
	}
	memset(f->name, '\0', namelen * UTF8_MAX_CHARSIZE + 1);

	exfat_convert_uniname(uniname, namelen, f->name);
	f->namelen = namelen;
	f->datalen = le64_to_cpu(stream->dentry.stream.DataLength);
	f->attr = le16_to_cpu(file->dentry.file.FileAttributes);
	f->flags = stream->dentry.stream.GeneralSecondaryFlags;
	f->hash = le16_to_cpu(stream->dentry.stream.NameHash);
	f->clu = le32_to_cpu(stream->dentry.stream.FirstCluster);

	exfat_convert_unixtime(&f->ctime, le32_to_cpu(file->dentry.file.CreateTimestamp),
			file->dentry.file.Create10msIncrement,
			file->dentry.file.CreateUtcOffset);
	exfat_convert_unixtime(&f->mtime, le32_to_cpu(file->dentry.file.LastModifiedTimestamp),
			file->dentry.file.LastModified10msIncrement,
			file->dentry.file.LastModifiedUtcOffset);
	exfat_convert_unixtime(&f->atime, le32_to_cpu(file->dentry.file.LastAccessedTimestamp),
			0,
			file->dentry.file.LastAccessdUtcOffset);
	append_node2(head, next_index, f);
	((struct exfat_fileinfo *)(head->data))->cached = 1;

	/* If this entry is Directory, prepare to create next chain */
	if ((f->attr & ATTR_DIRECTORY) && (!exfat_check_cache(next_index))) {
		struct exfat_fileinfo *d = calloc(sizeof(struct exfat_fileinfo), 1);
		if ((d->name = malloc(f->namelen + 1)) == NULL) {
			free(f->name);
			free(f);
			return -ENOMEM;
		}
		strncpy((char *)d->name, (char *)f->name, f->namelen + 1);
		d->namelen = namelen;
		d->datalen = le64_to_cpu(stream->dentry.stream.DataLength);
		d->attr = le16_to_cpu(file->dentry.file.FileAttributes);
		d->flags = stream->dentry.stream.GeneralSecondaryFlags;
		d->hash = le16_to_cpu(stream->dentry.stream.NameHash);

		index = exfat_get_cache(next_index);
		info.root[index] = init_node2(next_index, d);
		if (info.root[index] == NULL) {
			free(f->name);
			free(f);
			free(d);
			return -ENOMEM;
		}
	}
	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* SPECIAL ENTRY FUNCTION                                                                        */
/*                                                                                               */
/*************************************************************************************************/

/**
 * exfat_print_upcase - print upcase table
 */
void exfat_print_upcase(void)
{
	int byte, offset;
	size_t uni_count = 0x10 / sizeof(uint16_t);
	size_t length = info.upcase_size;
	uint64_t index;

	if (!info.upcase_table) {
		pr_err("Can't print upcase table\n");
		return;
	}

	/* Output table header */
	pr_msg("Offset  ");
	for (byte = 0; byte < uni_count; byte++)
		pr_msg("  +%u ",byte);
	pr_msg("\n");

	/* Output Table contents */
	for (offset = 0; offset < length / uni_count; offset++) {
		index = offset * 0x10 / sizeof(uint16_t);
		pr_msg("%04" PRIx64 ":  ", index);
		for (byte = 0; byte < uni_count; byte++) {
			pr_msg("%04x ", cpu_to_le16(info.upcase_table[offset * uni_count + byte]));
		}
		pr_msg("\n");
	}
}

/**
 * exfat_print_label - print volume label
 *
 * NOTE: If malloc for UTF-8 is failed, some error has occurred.
 */
void exfat_print_label(void)
{
	unsigned char *name;

	name = malloc(info.vol_length * sizeof(uint16_t) + 1);
	memset(name, '\0', info.vol_length * sizeof(uint16_t) + 1);

	if (!info.vol_label || !name) {
		pr_err("Can't print Volume Label\n");
		return;
	}

	pr_msg("volume Label: ");
	utf16s_to_utf8s(cpu_to_le16(info.vol_label), info.vol_length, name);
	pr_msg("%s\n", name);
	free(name);
}

/**
 * exfat_print_fat - print FAT
 */
void exfat_print_fat(void)
{
	uint32_t i, j;
	uint32_t *fat;
	uint32_t contents;
	size_t sector_num = (info.fat_length + (info.sector_size - 1)) / info.sector_size;
	size_t list_size = 0;
	node2_t **fat_chain, *tmp;

	if ((fat = malloc(info.sector_size * sector_num)) == NULL) {
		pr_err("Can't print FAT\n");
		return;
	}
	if (get_sector(fat, info.fat_offset * info.sector_size, sector_num)) {
		free(fat);
		return;
	}

	/* Read fat and create list */
	for (i = 0; i < info.cluster_count - 2; i++) {
		contents = le32_to_cpu(fat[i]);
		if (EXFAT_FIRST_CLUSTER <= contents && contents < EXFAT_BADCLUSTER)
			list_size++;
	}

	if ((fat_chain = calloc(list_size, sizeof(node2_t *))) == NULL) {
		pr_err("Can't print FAT\n");
		free(fat);
		return;
	}

	pr_msg("FAT:\n");
	for (i = 0; i < info.cluster_count - 2; i++) {
		contents = le32_to_cpu(fat[i]);
		if (EXFAT_FIRST_CLUSTER <= contents && contents < EXFAT_BADCLUSTER) {
			for (j = 0; j < list_size; j++) {
				if (fat_chain[j] && fat_chain[j]->index == contents) {
					insert_node2(fat_chain[j], i, NULL);
					break;
				} else if (fat_chain[j] && le32_to_cpu(fat[last_node2(fat_chain[j])->index]) == i) {
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
void exfat_print_bitmap(void)
{
	int offset, byte;
	uint8_t entry;
	uint32_t clu;

	if (!info.alloc_table) {
		pr_err("Can't print Allocation Bitmap\n");
		return;
	}

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
 * @return             == 0 (cluster as available for allocation)
 *                     == 1 (cluster as not available for allocation)
 *                     <  0 (failed)
 */
int exfat_load_bitmap(uint32_t clu)
{
	int offset, byte;
	uint8_t entry;

	if (!info.alloc_table) {
		pr_err("Internal Error: Allocation Bitmap is not loaded.\n");
		return -ENODATA;
	}

	if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1) {
		pr_err("Internal Error: Cluster %u is invalid.\n", clu);
		return -EINVAL;
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
 * @return             == 0 (success)
 *                     <  0 (failed)
 */
int exfat_save_bitmap(uint32_t clu, uint32_t value)
{
	int offset, byte;
	uint8_t mask = 0x01;
	uint8_t *raw_bitmap;

	if (!info.alloc_table) {
		pr_err("Internal Error: Allocation Bitmap is not loaded.\n");
		return -ENODATA;
	}

	if (clu < EXFAT_FIRST_CLUSTER || clu > info.cluster_count + 1) {
		pr_err("cluster: %u is invalid.\n", clu);
		return -EINVAL;
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
 *                              1 (bitmap was already loaded)
 *                             -1 (failed)
 */
int exfat_load_bitmap_cluster(struct exfat_dentry d)
{
	uint32_t fstclu;
	uint64_t datalen;

	if (info.alloc_offset)
		return 1;

	fstclu = le32_to_cpu(d.dentry.bitmap.FirstCluster);
	datalen = le64_to_cpu(d.dentry.bitmap.DataLength);

	pr_debug("Get: allocation table: cluster 0x%x, size: 0x%" PRIx64 "\n", fstclu, datalen);
	info.alloc_offset = fstclu;
	info.alloc_length = datalen;
	info.alloc_table = calloc(info.cluster_size, 1);
	if (!info.alloc_table) {
		pr_err("Can't load bitmap cluster.\n");
		return -ENODATA;
	}

	get_cluster(info.alloc_table, info.alloc_offset);
	exfat_concat_cluster_fast(info.alloc_offset, (void **)(&(info.alloc_table)), info.alloc_length);
	pr_info("Allocation Bitmap (#%u):\n", info.alloc_offset);

	return 0;
}

/**
 * exfat_load_upcase_cluster - function to load Upcase table
 * @d:                         directory entry about Upcase table
 *
 * @return                     == 0 (success)
 *                             == 1 (bitmap was already loaded)
 *                             <  0 (failed)
 */
int exfat_load_upcase_cluster(struct exfat_dentry d)
{
	uint32_t fstclu;
	uint32_t datalen;
	uint32_t checksum = 0;

	if (info.upcase_size)
		return -EINVAL;

	fstclu = le32_to_cpu(d.dentry.upcase.FirstCluster);
	datalen = le64_to_cpu(d.dentry.upcase.DataLength);

	pr_debug("Get: Up-case table: cluster 0x%x, size: 0x%" PRIx32 "\n", fstclu, datalen);
	info.upcase_offset = fstclu;
	info.upcase_size = datalen;
	info.upcase_table = calloc(info.cluster_size, 1);

	if (!info.upcase_table) {
		pr_err("Can't load bitmap cluster.\n");
		return -ENOMEM;
	}

	get_cluster(info.upcase_table, info.upcase_offset);
	exfat_concat_cluster_fast(info.upcase_offset, (void **)(&(info.upcase_table)), info.upcase_size);

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
 *                            1 (bitmap was already loaded)
 */
int exfat_load_volume_label(struct exfat_dentry d)
{
	if (info.vol_length)
		return 1;

	info.vol_length = d.dentry.vol.CharacterCount;
	if (info.vol_length) {
		pr_debug("Get: Volume label: size: 0x%x\n",
				d.dentry.vol.CharacterCount);
		memcpy(info.vol_label, d.dentry.vol.VolumeLabel,
				sizeof(uint16_t) * info.vol_length);
	}

	return 0;
}

/*************************************************************************************************/
/*                                                                                               */
/* FILE FUNCTION                                                                                 */
/*                                                                                               */
/*************************************************************************************************/

/**
 * exfat_traverse_root_directory - function to traverse root directory
 *
 * @return                         == 0 (success)
 *                                 <  0 (failed)
 */
int exfat_traverse_root_directory(void)
{
	int i;
	int ret = 0;
	uint8_t bitmap = 0x00;
	uint32_t clu = info.root_offset;
	uint32_t next_clu = info.root_offset;
	void *data;
	struct exfat_fileinfo *root = (struct exfat_fileinfo *)info.root[0]->data;
	struct exfat_dentry d;
	size_t allocated = 0;

	if ((data = malloc(info.cluster_size)) == NULL) {
		pr_err("Can't allocate memory for root directory.\n");
		return -ENOMEM;
	}

	if ((get_cluster(data, clu))) {
		free(data);
		return -EIO;
	}

	for (i = 0; i < info.cluster_size / sizeof(struct exfat_dentry); i++) {
		d = ((struct exfat_dentry *)data)[i];
		switch (d.EntryType) {
			case DENTRY_BITMAP:
				ret = exfat_load_bitmap_cluster(d);
				bitmap |= 0x01;
				break;
			case DENTRY_UPCASE:
				ret = exfat_load_upcase_cluster(d);
				bitmap |= 0x02;
				break;
			case DENTRY_VOLUME:
				ret = exfat_load_volume_label(d);
				break;
			case DENTRY_UNUSED:
				goto out;
			default:
				break;
		}

		if (ret < 0) {
			bitmap = 0;
			goto out;
		}
	}
out:
	free(data);

	if (bitmap != 0x03) {
		pr_err("Root Directory doesn't have important entry (%0x)\n", bitmap);
		return -1;
	}

	for (allocated = 0; next_clu != EXFAT_LASTCLUSTER && next_clu != 0; allocated++)
		next_clu = exfat_next_cluster(root, next_clu);
	root->datalen = info.cluster_size * allocated;

	return exfat_traverse_directory(clu);
}


/**
 * exfat_traverse_directory - function to traverse one directory
 * @clu:                      index of the cluster want to check
 *
 * @return                    == 0 (success)
 *                            <  0 (failed)
 */
int exfat_traverse_directory(uint32_t clu)
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

	if ((data = malloc(info.cluster_size)) == NULL) {
		pr_err("Can't allocate memory for directory.\n");
		return -ENOMEM;
	}

	if ((get_cluster(data, clu))) {
		free(data);
		return -EIO;
	}

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
					pr_warn("File should have stream entry, but This don't have.\n");
					continue;
				}
				/* Filename entry */
				name = ((struct exfat_dentry *)data)[i + 2];
				while ((!(name.EntryType & EXFAT_INUSE)) && (name.EntryType != DENTRY_UNUSED)) {
					pr_debug("This entry was deleted (0x%x).\n", name.EntryType);
					name = ((struct exfat_dentry *)data)[++i + 2];
				}
				if (name.EntryType != DENTRY_NAME) {
					pr_warn("File should have name entry, but This don't have.\n");
					continue;
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
 * exfat_calculate_bootchecksum - Calculate Boot region Checksum
 * @sectors:                      points to an in-memory copy of the 11 sectors
 * @bps:                          bytes per sector
 *
 * @return                        Checksum
 */
uint32_t exfat_calculate_bootchecksum(unsigned char *sectors, unsigned short bps)
{
	uint32_t bytes = (uint32_t)bps * 11;
	uint32_t checksum = 0;
	uint32_t index;

	for (index = 0; index < bytes; index++)
	{
		if ((index == 106) || (index == 107) || (index == 112))
		{
			continue;
		}
		checksum = 
			((checksum & 1) ? 0x80000000 : 0) + (checksum >> 1) + cpu_to_le32((uint32_t)sectors[index]);
	}

	return checksum;
}

/**
 * exfat_calculate_checksum - Calculate file entry Checksum
 * @entry:                    points to an in-memory copy of the directory entry set
 * @count:                    the number of secondary directory entries
 *
 * @return                    Checksum
 */
uint16_t exfat_calculate_checksum(unsigned char *entry, unsigned char count)
{
	uint16_t bytes = ((uint16_t)count + 1) * 32;
	uint16_t checksum = 0;
	uint16_t index;

	for (index = 0; index < bytes; index++) {
		if ((index == 2) || (index == 3))
			continue;
		checksum = ((checksum & 1) ? 0x8000 : 0) + (checksum >> 1) +  le16_to_cpu((uint16_t)entry[index]);
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
uint32_t exfat_calculate_tablechecksum(unsigned char *table, uint64_t length)
{
	uint32_t checksum = 0;
	uint64_t index;

	for (index = 0; index < length; index++)
		checksum =
			((checksum & 1) ? 0x80000000 : 0) + (checksum >> 1) + le32_to_cpu((uint32_t)table[index]);

	return checksum;
}

/**
 * exfat_calculate_namehash - Calculate name hash
 * @name:                     points to an in-memory copy of the up-cased file name
 * @len:                      Name length
 *
 * @return                    NameHash
 */
uint16_t exfat_calculate_namehash(uint16_t *name, uint8_t len)
{
	unsigned char* buffer = (unsigned char *)name;
	uint16_t bytes = (uint16_t)len * 2;
	uint16_t hash = 0;
	uint16_t index;

	for (index = 0; index < bytes; index++)
		hash = ((hash & 1) ? 0x8000 : 0) + (hash >> 1) + (le16_to_cpu(uint16_t)buffer[index]);

	return hash;
}

/**
 * exfat_update_filesize - flush filesize to disk
 * @f:                     file information pointer
 * @clu:                   first cluster
 *
 * @return                 == 0 (success)
 *                         <  0 (failed)
 */
int exfat_update_filesize(struct exfat_fileinfo *f, uint32_t clu)
{
	int i, j;
	uint32_t parent_clu = 0;
	size_t cluster_num;
	struct exfat_fileinfo *dir;
	struct exfat_dentry d;
	void *data;
	uint32_t fstclu;

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
		return -EINVAL;
	}

	cluster_num = (dir->datalen + (info.cluster_size - 1)) / info.cluster_size;
	if ((data = malloc(info.cluster_size)) == NULL) {
		pr_err("Can't allocate memory for root directory.\n");
		return -ENOMEM;
	}

	for (i = 0; i < cluster_num; i++) {
		if (get_cluster(data, parent_clu))
			goto out;
		for (j = 0; j < (info.cluster_size / sizeof(struct exfat_dentry)); j++) {
			d = ((struct exfat_dentry *)data)[j];
			fstclu = le32_to_cpu(d.dentry.stream.FirstCluster);
			if (d.EntryType == DENTRY_STREAM && fstclu == clu) {
				d.dentry.stream.DataLength = cpu_to_le64(f->datalen);
				d.dentry.stream.GeneralSecondaryFlags = f->flags;
				goto out;
			}
		}
		/* traverse next cluster */
		if (dir->flags & ALLOC_NOFATCHAIN)
			parent_clu++;
		else
			exfat_get_fat(parent_clu, &parent_clu);
	}
	parent_clu = 0;
out:
	set_cluster(data, parent_clu);
	free(data);
	return 0;
}

/**
 * exfat_convert_unixname - function to get timestamp in file
 * @t:                      output pointer (Output)
 * @time:                   Timestamp Field in File Directory Entry
 * @subsec:                 10msincrement Field in File Directory Entry
 * @tz:                     UtcOffset in File Directory Entry
 */
void exfat_convert_unixtime(struct tm *t, uint32_t time, uint8_t subsec, uint8_t tz)
{
	uint8_t sec, min, hour, day, mon, year = 0;
	char buf[80] = {};

	year = (time >> EXFAT_YEAR) & 0x7f;
	mon  = (time >> EXFAT_MONTH) & 0x0f;
	day = (time >> EXFAT_DAY) & 0x1f;
	hour = (time >> EXFAT_HOUR) & 0x1f;
	min  = (time >> EXFAT_MINUTE) & 0x3f;
	sec  = (time & 0x1f);

	sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d",
		1980 + year, mon, day, hour, min, (sec * 2) + (subsec / 100));

	if ((mon < 1 || 12 < mon) ||
		(day < 1 || 31 < day) ||
		(23 < hour) ||
		(59 < min) ||
		(29 < sec || 199 < subsec))
		pr_warn("Timestamp error: %s\n", buf);

	t->tm_year = year;
	t->tm_mon  = mon;
	t->tm_mday = day;
	t->tm_hour = hour;
	t->tm_min  = min;
	t->tm_sec  = sec * 2;
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
int exfat_convert_timezone(uint8_t tz)
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
 * exfat_lookup - function interface to lookup pathname
 * @clu:          directory cluster index
 * @name:         file name
 *
 * @return:       cluster index
 *                0 (Not found)
 */
uint32_t exfat_lookup(uint32_t clu, char *name)
{
	int index, i = 0, depth = 0;
	bool found = false;
	char *path[MAX_NAME_LENGTH] = {};
	char fullpath[PATHNAME_MAX + 1] = {};
	node2_t *tmp;
	struct exfat_fileinfo *f;

	if (!name) {
		pr_err("Internal Error: invalid pathname.\n");
		return 0;
	}

	/* Absolute path */
	if (name[0] == '/')
		clu = info.root_offset;

	/* Separate pathname by slash */
	strncpy(fullpath, name, PATHNAME_MAX);
	path[depth] = strtok(fullpath, "/");
	while (path[depth] != NULL) {
		if (depth >= MAX_NAME_LENGTH) {
			pr_err("Pathname is too depth. (> %d)\n", MAX_NAME_LENGTH);
			return 0;
		}
		path[++depth] = strtok(NULL, "/");
	};

	for (i = 0; path[i] && i < depth + 1; i++) {
		pr_debug("Lookup %s in clu#%u\n", path[i], clu);
		found = false;
		index = exfat_get_cache(clu);
		f = (struct exfat_fileinfo *)info.root[index]->data;
		/* Directory doesn't cache yet */
		if ((!info.root[index]) || (!(f->cached))) {
			exfat_traverse_directory(clu);
			index = exfat_get_cache(clu);
			/* Directory doesn't exist */
			if (!info.root[index]) {
				pr_err("This Directory doesn't exist in filesystem.\n");
				return 0;
			}
		}

		tmp = info.root[index];
		while (tmp->next != NULL) {
			tmp = tmp->next;
			f = (struct exfat_fileinfo *)tmp->data;
			if (!strcmp(path[i], (char *)f->name)) {
				clu = tmp->index;
				found = true;
				break;
			}
		}

		if (!found) {
			pr_err("'%s': No such file or directory.\n", name);
			return 0;
		}
	}

	return clu;
}

/**
 * exfat_convert_uniname - function to get filename
 * @uniname:               filename dentry in UTF-16
 * @name_len:              filename length
 * @name:                  filename in UTF-8 (Output)
 */
void exfat_convert_uniname(uint16_t *uniname, uint64_t name_len, unsigned char *name)
{
	utf16s_to_utf8s(uniname, name_len, name);
}

/**
 * exfat_convert_upper - convert character to upper-character
 * @c:                   character in UTF-16
 *
 * @return:              upper character
 */
uint16_t exfat_convert_upper(uint16_t c)
{
	return info.upcase_table[c] ? info.upcase_table[c] : c;
}

/**
 * exfat_convert_upper_character - convert string to upper-string
 * @src:                           Target characters in UTF-16
 * @len:                           Target characters length
 * @dist:                          convert result in UTF-16 (Output)
 */
void exfat_convert_upper_character(uint16_t *src, size_t len, uint16_t *dist)
{
	int i;

	for (i = 0; i < len; i++)
		dist[i] = exfat_convert_upper(src[i]);
}
