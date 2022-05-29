# exfat-rawtools

exFAT filesystem image Tools

## Table of Contents

- [Introduction](#Introduction)
- [Description](#Description)
- [Requirements](#Requirements)
- [Prerequisite](Prerequisite)
- [Install](#Install)
- [Authors](#Authors)

## Introduction

exfat-rawtools includes some tools related exFAT filesystem.

These tools can update exFAT filesystem iamges or obtain exFAT filesystem
informations w/o mounting filesystem.


## Description

The following functions have been implemented:

- `checkexfat`: Check filesystem status
- `statfsexfat`: Display information in Main boot sector
- `lsexfat`: list directory contents
- `catexfat` Display file contents
- `statexfat` Display file or directory status

### checkexfat

[exFAT file system specification](https://docs.microsoft.com/en-us/windows/win32/fileio/exfat-specification) described parameter requirements.  
checkexfat can detect below exfat filesystem's failure.

- Main boot region parameter
- Order of dentry type
- file timestamp
- Consistency of Allocation bitmap and actual use

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

statfsexfat display information in Main Boot Sector.

```
$ statfsexfat tests/sample/exfat.img
media-relative sector offset    : 0x00000000 (sector)
Offset of the First FAT         : 0x00000800 (sector)
Length of FAT table             :        256 (sector)
Offset of the Cluster Heap      : 0x00001000 (sector)
The number of clusters          :      32256 (cluster)
The first cluster of the root   :          5 (cluster)
Size of exFAT volumes           :     262144 (sector)
Bytes per sector                :        512 (byte)
Bytes per cluster               :          0 (byte)
The number of FATs              :          1
The percentage of clusters      :          0 (%)
```

### lsexfat

lsexfat list directory contests without mount filesystem.  
An argument that is a directory will display all the listable files in the respective directory.

Writes down (in single-column format) the FileAttribute Field, DataLength Field, Timestamp Field, and name of the file. 
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

catexfat print file contests without mount filesystem.

```
$ catexfat exfat.img /0_SIMPLE/FILE.TXT
hello, world
```

### statexfat

statexfat display file or directory status for exFAT filesystem.

```
$ statexfat exfat.img /4_FATCHAIN/FILE2.TXT
File    : FILE2.TXT
Size    : 8194
Cluster : 3 
First   : 0x0000000d
Attr    : ----D
Flags   : FatChain/ AllocationPossible
Access  : 2021-05-05 01:52:36
Modify  : 2021-05-05 01:53:53
Create  : 2021-05-05 01:52:36
```

## Requirements

The following operating systems have been confirmed.

- Ubuntu 20.04

In case of 64GB exFAT filesystem,
there is at least at 200MB memory space to use it.
(memory consupmption is depends on the number of dentry)

## Prerequisite

This tools needs below packages to build.

- [autoconf](http://www.gnu.org/software/autoconf/)
- [automake](https://www.gnu.org/software/automake/)
- [libtool](https://www.gnu.org/software/libtool/)
- [make](https://www.gnu.org/software/make/)

For Ubuntu, The folowing command can be installed.

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
