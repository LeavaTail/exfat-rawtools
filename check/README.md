# checkexfat

checkexfat checks whether an image can be read as an exFAT volume and reports
findings about metadata and cluster-use consistency.

It currently checks the following areas:

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

## Current Check Coverage

| ID | Area | Check | Impact | Current handling |
| :-- | :-- | :-- | :-- | :-- |
| BOOT-001 | Main boot region | Validate required boot sector fields and ranges | fatal | Stops check |
| BOOT-002 | Main boot region | Validate extended boot sector signatures | fatal | Stops check |
| BOOT-003 | Main boot region | Validate boot region checksum | fatal | Stops check |
| BOOT-004 | Main boot region | Validate that `VolumeLength` fits the input size when available | fatal | Stops check |
| ROOT-001 | Root directory | Find required Allocation Bitmap and Up-case Table entries | fatal | Stops check |
| ROOT-002 | Root directory | Detect root directory cluster-chain loop | fatal | Stops check |
| META-001 | Up-case Table | Validate Up-case Table checksum | warning | Printed only |
| META-002 | Volume label | Validate volume label length | error | Stops root traversal |
| META-003 | Timestamps | Detect timestamps that cannot be converted | warning | Printed only |
| DIR-001 | Directory entries | Detect unexpected file/stream/name entry order | warning | Printed only |
| DIR-002 | Directory entries | Validate secondary count and file name length | warning | Printed only |
| DIR-003 | Directory entries | Validate Stream Extension `ValidDataLength <= DataLength` | error | Printed only |
| DIR-004 | Directory entries | Validate Stream Extension `DataLength` does not exceed the cluster heap size | error | Printed only |
| DIR-005 | Directory entries | Validate Stream Extension `FirstCluster` and `DataLength` consistency | error | Printed only |
| FAT-001 | Cluster chain | Detect FAT chain shorter than file size | error | Printed only |
| FAT-002 | Cluster chain | Detect FAT chain loop | error | Printed only |
| FAT-003 | Cluster chain | Detect FAT entry pointing outside the cluster heap | error | Printed only |
| ALLOC-001 | Allocation Bitmap | Detect referenced cluster marked as free | error | Printed only |
| ALLOC-002 | Allocation Bitmap | Detect duplicate cluster references | error | Printed only |
| ALLOC-003 | Allocation Bitmap | Detect allocated but unreferenced clusters | warning | Printed only |

## Example Output

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
