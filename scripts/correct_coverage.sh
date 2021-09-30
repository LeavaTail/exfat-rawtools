#!/bin/bash

OUTPUT=logs
IGNORE="autom4te.cache\|debian\|scripts\|tests\|templates\|${OUTPUT}"
TOOLS=(`ls -d */ | grep -v ${IGNORE}`)

mkdir -p ${OUTPUT}/

for t in ${TOOLS[@]}
do
	for src in `ls -1 ${t}*.gcda`
		do
			NAME=`basename ${src} .gcda`
			gcov ${src} 2>&1 | tee ${OUTPUT}/${NAME}.log
			mv *.gcov ${OUTPUT}
		done
done

if !(type "awk" > /dev/null 2>&1); then
	echo "This scripts needs 'awk' command."
	exit 1
fi

awk -F '[: %]' '{a[NR]=$0; if( $1 == "Lines" && $3 < 50.00) { print a[NR-1] " is not well tested."}}' ${OUTPUT}/*.log 
