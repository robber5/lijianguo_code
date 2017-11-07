#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(record, 1.0, lijianguo@detu.com)
AC_CONFIG_SRCDIR([record_search_debug.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE
# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL
# Checks for libraries.
# FIXME: Replace `main' with a function in `-lmp4muxer':
AC_CHECK_LIB([mp4muxer], [main])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset select])

AC_CONFIG_FILES(Makefile mp4muxer-custom/Makefile mp4muxer-custom/mp4lib/Makefile storage/Makefile)
AC_CONFIG_SUBDIRS([mp4muxer-custom] [storage])
AC_OUTPUT
