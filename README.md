# exfat-rawtools

Tools for exFAT filesystem images

## Table of Contents

- [Introduction](#introduction)
- [Description](#description)
- [Requirements](#requirements)
- [Prerequisite](#prerequisite)
- [Install](#install)
- [Authors](#authors)

## Introduction

exfat-rawtools provides tools for exFAT filesystem images.

These tools can inspect exFAT filesystem images without mounting the
filesystem.


## Description

The following functions have been implemented:

- `checkexfat`: Check filesystem status
- `statfsexfat`: Display information in the Main Boot Sector
- `lsexfat`: List directory contents
- `catexfat`: Display file contents
- `statexfat`: Display file or directory status
- `diffexfat`: Compare two exFAT filesystem images

### checkexfat

The [exFAT file system specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification)
describes parameter requirements.
checkexfat checks whether an image can be read as an exFAT volume and reports
findings about metadata and cluster-use consistency. It currently checks the
following areas:

- Main boot region parameters
- Extended boot sector signatures
- Main boot region checksum
- Required root directory entries
- Directory entry order and file name entry layout
- File timestamps
- FAT chain and Allocation Bitmap consistency
- Duplicate cluster references
- Allocated but unreferenced clusters

Findings are described by impact:

- `fatal`: root directory traversal cannot continue
- `error`: file data or cluster references may be affected
- `warning`: metadata looks suspicious, but traversal can continue

Current checkexfat output summarizes the number of printed errors and warnings,
but it does not yet summarize findings by impact. Some findings are printed as
warnings or errors without changing the process exit status.

Current check coverage:

| ID | Area | Check | Impact | Current handling |
| :-- | :-- | :-- | :-- | :-- |
| BOOT-001 | Main boot region | Validate required boot sector fields and ranges | fatal | Stops check |
| BOOT-002 | Main boot region | Validate extended boot sector signatures | fatal | Stops check |
| BOOT-003 | Main boot region | Validate boot region checksum | fatal | Stops check |
| ROOT-001 | Root directory | Find required Allocation Bitmap and Up-case Table entries | fatal | Stops check |
| ROOT-002 | Root directory | Detect root directory cluster-chain loop | fatal | Stops check |
| META-001 | Up-case Table | Validate Up-case Table checksum | warning | Printed only |
| META-002 | Volume label | Validate volume label length | error | Stops root traversal |
| META-003 | Timestamps | Detect timestamps that cannot be converted | warning | Printed only |
| DIR-001 | Directory entries | Detect unexpected file/stream/name entry order | warning | Printed only |
| DIR-002 | Directory entries | Validate secondary count and file name length | warning | Printed only |
| DIR-003 | Directory entries | Validate Stream Extension `ValidDataLength <= DataLength` | error | Printed only |
| FAT-001 | Cluster chain | Detect FAT chain shorter than file size | error | Printed only |
| FAT-002 | Cluster chain | Detect FAT chain loop | error | Printed only |
| FAT-003 | Cluster chain | Detect FAT entry pointing outside the cluster heap | error | Printed only |
| ALLOC-001 | Allocation Bitmap | Detect referenced cluster marked as free | error | Printed only |
| ALLOC-002 | Allocation Bitmap | Detect duplicate cluster references | error | Printed only |
| ALLOC-003 | Allocation Bitmap | Detect allocated but unreferenced clusters | warning | Printed only |

```
$ checkexfat tests/sample/error.img
Cluster#13 isn't allocated.
Cluster#32 isn't allocated.
Cluster#14 is referenced from other cluster.
Cluster#17 is referenced from other cluster.
Cluster#12 isn't used at all.
Cluster#15 isn't used at all.

/               (5) | 0_BITMAP(6) 1_FAT(7) 2_LOOP(8) 3_DOUBLE(9) 4_FILESIZE(10)
0_BITMAP        (6) | FILE1.TXT(11) FILE2.TXT(12)
1_FAT           (7) | FILE6.TXT(17)
2_LOOP          (8) |
3_DOUBLE        (9) | FILE3.TXT(14) FILE4.TXT(14)
4_FILESIZE      (10) | FILE5.TXT(16) FILE7.TXT(18)

Summary:
  check: completed
  diagnostics: 2 error(s), 4 warning(s)
  result: issues found
```

### statfsexfat

statfsexfat displays information in the Main Boot Sector.

```
$ statfsexfat exfat.img
media-relative sector offset    : 0x00000000 (sector)
Offset of the First FAT         : 0x00000800 (sector)
Length of FAT table             :        256 (sector)
Offset of the Cluster Heap      : 0x00001000 (sector)
The number of clusters          :      32256 (cluster)
The first cluster of the root   :          5 (cluster)
Size of exFAT volumes           :     262144 (sector)
Bytes per sector                :        512 (byte)
Bytes per cluster               :       4096 (byte)
The number of FATs              :          1
The percentage of clusters      :          0 (%)
```

### lsexfat

lsexfat lists directory contents without mounting the filesystem.
If the argument is a directory, lsexfat displays all listable files in that directory.

It writes the FileAttributes field, DataLength field, timestamp field, and file name in a single-column format.
By default, the timestamp displayed is the last modification time.

```
$ lsexfat exfat.img /
---D-     4096 2021-05-05 01:48:47 0_SIMPLE
---D-     4096 2021-05-05 01:54:36 1_FILENAME
---D-     4096 2021-05-05 01:55:19 2_DELETED
---D-     4096 2021-05-05 01:53:19 3_NOFATCHAIN
---D-     4096 2021-05-05 01:53:05 4_FATCHAIN

$ lsexfat exfat.img /0_SIMPLE/FILE.TXT
----A        2 2021-05-05 01:48:42 FILE.TXT
```

### catexfat

catexfat prints file contents without mounting the filesystem.

```
$ catexfat exfat.img /0_SIMPLE/FILE.TXT
A
```

### statexfat

statexfat displays file or directory status for an exFAT filesystem image.

```
$ statexfat exfat.img /4_FATCHAIN/FILE2.TXT
File    : FILE2.TXT
Size    : 8194
Cluster : 3 
First   : 0x0000000d
Attr    : ----A
Flags   : FatChain/ AllocationPossible
Access  : 2021-05-05 01:52:36
Modify  : 2021-05-05 01:53:53
Create  : 2021-05-05 01:52:36
```

### diffexfat

diffexfat compares two exFAT filesystem images without mounting them.
It reports differences in the Main Boot Sector and Root Directory special entries.

```
$ diffexfat exfat.img modified.img
Allocation Bitmap: cluster #22 differs: image1=free image2=allocated
Up-case Table: entry #0x0000 differs: image1=0x0000 image2=0x0001
Volume Label: CharacterCount differs: image1=0 image2=1
Volume Label: VolumeLabel differs
```

## Requirements

The following operating systems have been confirmed.

- Ubuntu 20.04

For a 64GB exFAT filesystem, at least 200MB of memory is required.
Memory consumption depends on the number of directory entries.

## Prerequisite

These tools need the following packages to build.

- [autoconf](http://www.gnu.org/software/autoconf/)
- [automake](https://www.gnu.org/software/automake/)
- [libtool](https://www.gnu.org/software/libtool/)
- [make](https://www.gnu.org/software/make/)

On Ubuntu, install them with the following command.

```bash
$ sudo apt install autoconf automake libtool make
```

## Install

```bash
$ ./scripts/bootstrap.sh
$ ./configure
$ make
$ sudo make install
```

## Authors

[LeavaTail](https://github.com/LeavaTail)
