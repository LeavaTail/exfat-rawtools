bin_PROGRAMS = checkexfat statfsexfat lsexfat
checkexfat_SOURCES = check/checkexfat.c common/print.c common/utf8.c common/exfat.c
statfsexfat_SOURCES = statfs/statfsexfat.c common/print.c common/utf8.c common/exfat.c
lsexfat_SOURCES = ls/lsexfat.c common/print.c common/utf8.c common/exfat.c

man_MANS = checkexfat.8 statfsexfat.8 lsexfat.8

TESTS = tests/00_init.sh \
        tests/01_test_checkexfat.sh \
        tests/02_test_statfsexfat.sh \
        tests/03_test_lsexfat.sh

EXTRA_DIST = common
AM_CPPFLAGS = -I$(top_srcdir)/common

if DEBUG
AM_CPPFLAGS += -O0 -g3 -Wall -DEXFAT_DEBUG
else
AM_CPPFLAGS += -O2
endif

checkexfat.8: checkexfat$(EXEEXT)
	help2man --no-discard-stderr --section=8 -N -o checkexfat.8 ./checkexfat
statfsexfat.8: statfsexfat$(EXEEXT)
	help2man --no-discard-stderr --section=8 -N -o statfsexfat.8 ./statfsexfat
lsexfat.8: lsexfat$(EXEEXT)
	help2man --no-discard-stderr --section=8 -N -o lsexfat.8 ./lsexfat
