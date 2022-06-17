#!/bin/bash

PROG=./checkexfat
IMAGE=exfat.img
FAILURE_IMAGE=error.img
RET=0

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

### main function ###
${PROG} ${IMAGE}
${PROG} ${FAILURE_IMAGE}

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
