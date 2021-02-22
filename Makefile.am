bin_PROGRAMS = diffexfat
diffexfat_SOURCES = diff/diffexfat.c
man_MANS = diffexfat.8

EXTRA_DIST = include
AM_CPPFLAGS = -I$(top_srcdir)/include

if DEBUG
AM_CPPFLAGS += -O0 -g3 -Wall -DDIFFEXFAT_DEBUG
else
AM_CPPFLAGS += -O2
endif

diffexfat.8: diffexfat$(EXEEXT)
	help2man --no-discard-stderr --section=8 -N -o diffexfat.8 ./diffexfat
