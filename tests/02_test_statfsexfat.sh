#!/bin/bash

PROG=./statfsexfat
IMAGE=exfat.img
FAILURE_IMAGE=error.img

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

### main function ###
${PROG} ${IMAGE}
${PROG} ${FAILURE_IMAGE}

### Option function ###
${PROG} --help
${PROG} --version
