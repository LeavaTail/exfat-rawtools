#!/bin/bash

PROG=./checkexfat
IMAGE=exfat.img
FAILURE_IMAGE=error.img
RET=0
OUT=
VDL_IMAGE=
SHORT_IMAGE=

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR
trap 'rm -f "${VDL_IMAGE}" "${SHORT_IMAGE}"' EXIT

### main function ###
${PROG} ${IMAGE}
${PROG} ${FAILURE_IMAGE}

OUT=$(${PROG} ${IMAGE})
echo "$OUT" | grep -q "Summary:"
echo "$OUT" | grep -q "diagnostics: 0 error(s), 0 warning(s)"
echo "$OUT" | grep -q "result: no issues found"

OUT=$(${PROG} ${FAILURE_IMAGE})
echo "$OUT" | grep -q "Summary:"
echo "$OUT" | grep -q "result: issues found"

VDL_IMAGE=$(mktemp "${TMPDIR:-/tmp}/checkexfat-vdl.XXXXXX.img")
cp "${IMAGE}" "${VDL_IMAGE}"
printf '\x01\x10\x00\x00\x00\x00\x00\x00' |
	dd of="${VDL_IMAGE}" bs=1 seek=$((0x203088)) conv=notrunc status=none
OUT=$(${PROG} "${VDL_IMAGE}")
echo "$OUT" | grep -q "ValidDataLength(4097) exceeds DataLength(4096)"
echo "$OUT" | grep -q "result: issues found"

SHORT_IMAGE=$(mktemp "${TMPDIR:-/tmp}/checkexfat-short.XXXXXX.img")
cp "${IMAGE}" "${SHORT_IMAGE}"
truncate -s 1048576 "${SHORT_IMAGE}"
OUT=$(${PROG} "${SHORT_IMAGE}" || true)
echo "$OUT" | grep -q "VolumeLength requires"
echo "$OUT" | grep -q "input size is 1048576 bytes"
echo "$OUT" | grep -q "result: unreadable or incomplete check"

### Option function ###
${PROG} --help
${PROG} --version

### Error path ###

# Failure argument verification
${PROG} ${IMAGE} 0 0 0 || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Argument verification may be wrong"
fi
RET=0

# Failure parse verification
${PROG} -z ${IMAGE} || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Option Parser verification may be wrong"
fi
RET=0

# Failure exist verification
${PROG} nothing.img || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Open file verification may be wrong"
fi
RET=0

# Failure exFAT image verification
${PROG} README.md || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Image verification may be wrong"
fi
RET=0
