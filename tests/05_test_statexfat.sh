#!/bin/bash

PROG=./statexfat
IMAGE=exfat.img
RET=0

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

### main function ###
${PROG} ${IMAGE} /0_SIMPLE
${PROG} ${IMAGE} /4_FATCHAIN/FILE2.TXT

### Option function ###
${PROG} --help
${PROG} --version
${PROG} -v ${IMAGE} /4_FATCHAIN/FILE2.TXT

### Error path ###
${PROG} ${IMAGE} 0 0 0 || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Argument verification may be wrong"
fi
RET=0

# Failure parse verification
${PROG} -z ${IMAGE} / || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Option Parser verification may be wrong"
fi
RET=0

# Failure exist verification
${PROG} nothing.img / || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Open file verification may be wrong"
fi
RET=0

# Failure exFAT image verification
${PROG} README.md / || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Image verification may be wrong"
fi
RET=0

# Failure traverse verification
${PROG} ${IMAGE} /NOTHING/NONE/NO || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Traverse verification may be wrong"
fi
RET=0
