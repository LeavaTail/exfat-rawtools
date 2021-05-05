#!/bin/bash

set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

function test_options () {
	$1 $2
}

### main function ###
test_options ./checkexfat exfat.img
