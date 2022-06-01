#!/bin/bash

PROG=./statexfat
IMAGE=exfat.img

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

### main function ###
${PROG} ${IMAGE} /0_SIMPLE
${PROG} ${IMAGE} /4_FATCHAIN/FILE2.TXT

### Option function ###
${PROG} --help
${PROG} --version
${PROG} -v ${IMAGE} /4_FATCHAIN/FILE2.TXT
