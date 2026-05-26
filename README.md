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

### checkexfat

The [exFAT file system specification](https://docs.microsoft.com/en-us/windows/win32/fileio/exfat-specification)
describes parameter requirements.
checkexfat can detect the following exFAT filesystem failures:

- Main boot region parameter
- Main boot region parameters
- Directory entry order
- File timestamps
- Consistency between the Allocation Bitmap and actual cluster use

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
