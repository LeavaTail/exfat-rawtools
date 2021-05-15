#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

### main function ###
./lsexfat exfat.img /
./lsexfat exfat.img /0_SIMPLE
