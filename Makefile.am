#bin_PROGRAMS = diffexfat
#diffexfat_SOURCES = diff/diffexfat.c common/print.c common/utf8.c common/exfat.c
#man_MANS = diffexfat.8

EXTRA_DIST = common
AM_CPPFLAGS = -I$(top_srcdir)/common

if DEBUG
AM_CPPFLAGS += -O0 -g3 -Wall -DEXFAT_DEBUG
else
AM_CPPFLAGS += -O2
endif

#diffexfat.8: diffexfat$(EXEEXT)
#	help2man --no-discard-stderr --section=8 -N -o diffexfat.8 ./diffexfat
