#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([exfat-rawtools], [0.3.1], [starbow.duster@gmail.com])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
: ${CFLAGS=""}
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug],
               [enable debugging, default: no]),
               [case "${enableval}" in
                       yes) debug=true ;;
                       no)  debug=false ;;
                       *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
               esac],
               [debug=false])

AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")

AC_ARG_ENABLE(gcov,
AS_HELP_STRING([--enable-gcov],
               [enable gcov, default: no]),
               [case "${enableval}" in
                       yes) gcov=true ;;
                       no)  gcov=false ;;
                       *)   AC_MSG_ERROR([bad value ${enableval} for --enable-gcov]) ;;
               esac],
               [debug=false])

AM_CONDITIONAL(GCOV, test x"$gcov" = x"true")

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h mntent.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strerror])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
