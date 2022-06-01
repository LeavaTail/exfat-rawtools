#!/bin/bash

PROG=./catexfat
IMAGE=exfat.img

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

### main function ###
${PROG} ${IMAGE} /0_SIMPLE/FILE.TXT

### Option function ###
${PROG} --help
${PROG} --version
