#!/bin/bash

PROGS=("./checkexfat" "./statfsexfat")
IMAGE=$(mktemp -u "${TMPDIR:-/tmp}/exfat-rawtools-huge.XXXXXX.img")
SIZE=2T

set -eu -o pipefail
trap 'rm -f "${IMAGE}"' EXIT
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

# Create sparse file and format exfat
dd if=/dev/zero of=${IMAGE} bs=1 count=0 seek=${SIZE}
mkfs.exfat -v ${IMAGE}

# Run each program
for i in "${PROGS[@]}"
do
	${i} ${IMAGE}
done
