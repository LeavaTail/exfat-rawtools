#!/bin/bash

PROG=./diffexfat
IMAGE1=exfat.img
IMAGE2=exfat.img
DIFF_IMAGE=diff_percent.img
DIFF_LOG=diff_percent.log
BITMAP_IMAGE=diff_bitmap.img
BITMAP_LOG=diff_bitmap.log
UPCASE_IMAGE=diff_upcase.img
UPCASE_LOG=diff_upcase.log
RET=0

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR
trap 'rm -f ${DIFF_IMAGE} ${DIFF_LOG} ${BITMAP_IMAGE} ${BITMAP_LOG} ${UPCASE_IMAGE} ${UPCASE_LOG}' EXIT

### main function ###
${PROG} ${IMAGE1} ${IMAGE2}

### Option function ###
${PROG} --help
${PROG} --version

### Difference verification ###

# PercentInUse is excluded from the Boot Checksum.
cp ${IMAGE1} ${DIFF_IMAGE}
printf '\377' | dd of=${DIFF_IMAGE} bs=1 seek=112 count=1 conv=notrunc status=none
${PROG} ${IMAGE1} ${DIFF_IMAGE} > ${DIFF_LOG} || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: PercentInUse difference may be undetected"
	exit 1
fi
grep "Boot Sector: PercentInUse differs" ${DIFF_LOG}
RET=0

# Allocation Bitmap bit 20 represents cluster #22 in the sample image.
cp ${IMAGE1} ${BITMAP_IMAGE}
printf '\037' | dd of=${BITMAP_IMAGE} bs=1 seek=2097154 count=1 conv=notrunc status=none
${PROG} ${IMAGE1} ${BITMAP_IMAGE} > ${BITMAP_LOG} || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Allocation Bitmap difference may be undetected"
	exit 1
fi
grep "Allocation Bitmap: cluster #22 differs: image1=free image2=allocated" ${BITMAP_LOG}
RET=0

# Up-case Table starts at cluster #3 in the sample image.
cp ${IMAGE1} ${UPCASE_IMAGE}
printf '\001' | dd of=${UPCASE_IMAGE} bs=1 seek=2101248 count=1 conv=notrunc status=none
${PROG} ${IMAGE1} ${UPCASE_IMAGE} > ${UPCASE_LOG} || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Up-case Table difference may be undetected"
	exit 1
fi
grep "Up-case Table: entry #0x0000 differs: image1=0x0000 image2=0x0001" ${UPCASE_LOG}
RET=0

### Error path ###

# Failure argument verification
${PROG} ${IMAGE1} || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Argument verification may be wrong"
	exit 1
fi
RET=0

${PROG} ${IMAGE1} ${IMAGE2} ${IMAGE1} || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Argument verification may be wrong"
	exit 1
fi
RET=0

# Failure parse verification
${PROG} -z ${IMAGE1} ${IMAGE2} || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Option Parser verification may be wrong"
	exit 1
fi
RET=0

# Failure exist verification
${PROG} nothing.img ${IMAGE2} || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Open file verification may be wrong"
	exit 1
fi
RET=0

${PROG} ${IMAGE1} nothing.img || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Open file verification may be wrong"
	exit 1
fi
RET=0

# Failure exFAT image verification
${PROG} README.md ${IMAGE2} || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Image verification may be wrong"
	exit 1
fi
RET=0

${PROG} ${IMAGE1} README.md || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Image verification may be wrong"
	exit 1
fi
RET=0
