#bin_PROGRAMS = diffexfat
#diffexfat_SOURCES = diff/diffexfat.c common/print.c common/utf8.c common/exfat.c
#man_MANS = diffexfat.8
bin_PROGRAMS = checkexfat statexfat
checkexfat_SOURCES = check/checkexfat.c common/print.c common/utf8.c common/exfat.c
statexfat_SOURCES = stat/statexfat.c common/print.c common/utf8.c common/exfat.c

man_MANS = checkexfat.8 statexfat.8

TESTS = tests/00_init.sh \
        tests/01_simple_option_check.sh

EXTRA_DIST = common
AM_CPPFLAGS = -I$(top_srcdir)/common

if DEBUG
AM_CPPFLAGS += -O0 -g3 -Wall -DEXFAT_DEBUG
else
AM_CPPFLAGS += -O2
endif

#diffexfat.8: diffexfat$(EXEEXT)
#	help2man --no-discard-stderr --section=8 -N -o diffexfat.8 ./diffexfat
checkexfat.8: checkexfat$(EXEEXT)
	help2man --no-discard-stderr --section=8 -N -o checkexfat.8 ./checkexfat
statexfat.8: statexfat$(EXEEXT)
	help2man --no-discard-stderr --section=8 -N -o statexfat.8 ./statexfat
