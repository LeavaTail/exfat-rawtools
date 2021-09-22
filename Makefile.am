bin_PROGRAMS = checkexfat statfsexfat lsexfat catexfat
lib_LTLIBRARIES = libexfat.la

libexfat_la_SOURCES = common/exfat.c common/utf8.c common/print.c \
                      common/list2.h common/utf8.h common/exfat.h common/print.h
libexfat_la_LDFLAGS = -static
LDADD=libexfat.la $(INTLLIBS)

checkexfat_SOURCES = check/checkexfat.c check/checkexfat.h
statfsexfat_SOURCES = statfs/statfsexfat.c statfs/statfsexfat.h
lsexfat_SOURCES = ls/lsexfat.c ls/lsexfat.h
catexfat_SOURCES = cat/catexfat.c cat/catexfat.h

man_MANS = checkexfat.8 statfsexfat.8 lsexfat.8 catexfat.8

TESTS = tests/00_init.sh \
        tests/01_test_checkexfat.sh \
        tests/02_test_statfsexfat.sh \
        tests/03_test_lsexfat.sh \
        tests/04_test_catexfat.sh

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
catexfat.8: catexfat$(EXEEXT)
	help2man --no-discard-stderr --section=8 -N -o catexfat.8 ./catexfat
