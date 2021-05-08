# exfat-rawtools

exFAT filesystem image Tools

## Table of Contents

- [Description](#Description)
- [Requirement](#Requirement)
- [Install](#Install)
- [Authors](#Authors)

## Description

exfat-rawtools includes some tools related exFAT filesystem.

These tools can update exFAT filesystem iamges or obtain exFAT filesystem
informations w/o mounting filesystem.

### checkexfat

[exFAT file system specification](https://docs.microsoft.com/en-us/windows/win32/fileio/exfat-specification) described parameter requirements.  
checkexfat can detect below exfat filesystem's failure.

- Main boot region parameter
- Order of dentry type
- file timestamp
- Consistency of Allocation bitmap and actual use

```
$ ./checkexfat tests/sample/error.img
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


## Requirements

- [autoconf](http://www.gnu.org/software/autoconf/)
- [automake](https://www.gnu.org/software/automake/)
- [libtool](https://www.gnu.org/software/libtool/)
- [help2man](https://www.gnu.org/software/help2man/)
- [make](https://www.gnu.org/software/make/)

## Install

```bash
$ ./bootstrap.sh
$ ./configure
$ make
$ sudo make install
```

## Authors

[LeavaTail](https://github.com/LeavaTail)
