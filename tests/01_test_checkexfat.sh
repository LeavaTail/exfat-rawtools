#!/bin/bash

PROG=./checkexfat
IMAGE=exfat.img
FAILURE_IMAGE=error.img

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

### main function ###
${PROG} ${IMAGE}
${PROG} ${FAILURE_IMAGE}
