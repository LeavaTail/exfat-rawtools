#!/bin/bash

PROG=./lsexfat
IMAGE=exfat.img

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

### main function ###
${PROG} ${IMAGE} /
${PROG} ${IMAGE} /0_SIMPLE

### Option function ###
${PROG} --help
${PROG} --version
${PROG} -c ${IMAGE} /0_SIMPLE
${PROG} -u ${IMAGE} /0_SIMPLE
