# Test Document

## Table of Contents

- [Filesystem Status](#Filesystem_Status)
- [Directory Structure](#Directory_Structure)
- [Cluster Contents](#Cluster_Contents)

## Filesystem_Status

```
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

## Directory_Structure

**exfat.img**

```
.
|-- 0_SIMPLE
|   |-- FILE.TXT
|   `-- DIR
|       `-- SUBFILE.TXT
|-- 1_FILENAME
|  |-- ABCDEFGHIJKLMNOPQRSTUVWXYZ!
|  |-- ¼
|  |-- Ō
|  `-- あいうえお
|-- 2_DELETE
|   |-- FILE5.TXT  (Deleted)
|   `-- FILE6.TXT
|-- 3_NOFATCHAIN
|   `-- FILE4.TXT
`-- 4_FATCHAIN
    |-- FILE2.TXT
    `-- FILE3.TXT
```

## Cluster_Contents

**exfat.img**

| Cluster | Description       | NOFATCHAIN | File size (Bytes) |
| :------ | :---------------- | :--------: | ----------------: |
| 2       | Allocation Bitmap |            | 4032              |
| 3       | Up-case Table [1] |            | 5836              |
| 4       | Up-case Table [2] |            | 5836              |
| 5       | Root Directory    |            | 4096              |
| 6       | 0\_SIMPLE         | X          | 4096              |
| 7       | 1\_FILENAME       | X          | 4096              |
| 8       | 2\_DELETED        | X          | 4096              |
| 9       | 3\_NOFATCHAIN     | X          | 4096              |
| 10      | 4\_FATCHAIN       | X          | 4096              |
| 11      | FILE.TXT          | X          |    2              |
| 12      | DIR               | X          | 4096              |
| 13      | FILE2.TXT [1]     |            | 8194              |
| 14      | FILE2.TXT [2]     |            | 8194              |
| 15      | FILE3.TXT [1]     |            | 8194              |
| 16      | FILE3.TXT [2]     |            | 8194              |
| 17      | FILE4.TXT [1]     | X          | 8194              |
| 18      | FILE4.TXT [2]     | X          | 8194              |
| 19      | FILE4.TXT [3]     | X          | 8194              |
| 20      | FILE2.TXT [3]     |            | 8194              |
| 21      | FILE3.TXT [3]     |            | 8194              |

