#!/bin/bash

PROGRAMS=`grep "bin_PROGRAMS" Makefile.am | cut -f 3- -d ' '`
SECTION=8
OUTPUT="mans"

# Command verification
if !(type "help2man" > /dev/null 2>&1); then
	echo "No command help2man. Please install..."
	exit 1
fi

# Output path verification
if [ ! -e ${OUTPUT} ]; then
	mkdir ${OUTPUT}
fi

for prog in ${PROGRAMS[@]}
do
	help2man --no-discard-stderr --section=${SECTION} -N -o ${OUTPUT}/${prog}.${SECTION} ./${prog}
done
