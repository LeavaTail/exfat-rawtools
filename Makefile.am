bin_PROGRAMS = checkexfat statfsexfat lsexfat catexfat statexfat
lib_LTLIBRARIES = libexfat.la

libexfat_la_SOURCES = common/exfat.c common/utf8.c common/print.c \
                      common/list2.h common/utf8.h common/exfat.h common/print.h
libexfat_la_LDFLAGS = -static
LDADD=libexfat.la $(INTLLIBS)

checkexfat_SOURCES = check/checkexfat.c check/checkexfat.h
statfsexfat_SOURCES = statfs/statfsexfat.c statfs/statfsexfat.h
lsexfat_SOURCES = ls/lsexfat.c ls/lsexfat.h
catexfat_SOURCES = cat/catexfat.c cat/catexfat.h
statexfat_SOURCES = stat/statexfat.c stat/statexfat.h

TESTS = tests/00_init.sh \
        tests/01_test_checkexfat.sh \
        tests/02_test_statfsexfat.sh \
        tests/03_test_lsexfat.sh \
        tests/04_test_catexfat.sh \
        tests/05_test_statexfat.sh

EXTRA_DIST = common
AM_CPPFLAGS = -I$(top_srcdir)/common

if DEBUG
AM_CPPFLAGS += -O0 -g3 -Wall -DEXFAT_DEBUG
else
AM_CPPFLAGS += -O2
endif

if GCOV
AM_CPPFLAGS += -O0 --no-inline -fprofile-arcs -ftest-coverage
LDFLAGS += -lgcov
endif

