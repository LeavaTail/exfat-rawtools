#!/bin/bash
set -eu -o pipefail
trap 'echo "ERROR: l.$LINENO, exit status = $?" >&2; exit 1' ERR

if [ $# -ne 1 ]; then
	echo "Usage: $0 architecture "
	exit 1
fi

MAINLOG='test-suite.log'
SUBLOG='tests/*.log'
OUTPUT='./logs/'$1

if [ ! -d ${OUTPUT} ]; then
	mkdir -p ${OUTPUT}
fi

for file in `ls ${SUBLOG}`; do
	if [ -e ${file} ]; then
		cp ${file} ${OUTPUT}
		echo "Copy ${file} => ${OUTPUT}"
	fi
done
	
if [ -e ${MAINLOG} ]; then
	cp ${MAINLOG} ${OUTPUT}
	echo "Copy ${MAINLOG} => ${OUTPUT}"
fi
