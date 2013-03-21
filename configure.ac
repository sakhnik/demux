#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([demux], [1.0.0], [sakhnik@gmail.com])
AM_INIT_AUTOMAKE([1.9])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_CC_C_O

# Checks for libraries.
m4_include([ax_pthread.m4])

AX_PTHREAD
PKG_CHECK_MODULES(LIBAVUTIL, libavutil)
PKG_CHECK_MODULES(LIBAVFORMAT, libavformat)
PKG_CHECK_MODULES(LIBAVCODEC, libavcodec)

AC_OUTPUT(Makefile src/Makefile)
