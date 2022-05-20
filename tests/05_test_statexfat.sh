#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

### main function ###
./statexfat exfat.img /0_SIMPLE
./statexfat exfat.img /4_FATCHAIN/FILE2.TXT
