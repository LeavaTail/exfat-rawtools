#!/bin/bash

PROG=./lsexfat
IMAGE=exfat.img
RET=0

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

### main function ###
${PROG} ${IMAGE} /
${PROG} ${IMAGE} /0_SIMPLE
${PROG} ${IMAGE} /0_SIMPLE/FILE.TXT

### Option function ###
${PROG} --help
${PROG} --version
${PROG} -c ${IMAGE} /0_SIMPLE
${PROG} -u ${IMAGE} /0_SIMPLE

### Error path ###

# Failure argument verification
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

# Failure File/Directory verification
${PROG} ${IMAGE} /0_SIMPLE/FILE.TXT/ || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: File or Directory verification may be wrong"
fi
RET=0

# Failure traverse verification
${PROG} ${IMAGE} /NOTHING/NONE/NO || RET=$?
if [ $RET -eq 0 ]; then
	echo "ERROR: Traverse verification may be wrong"
fi
RET=0
