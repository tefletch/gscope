dnl Process this file with autoconf to produce a configure script.

AC_INIT([gscope],[4.25])
AC_CONFIG_SRCDIR([src/gscope4.cmb])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_DEFINE([GTK3_BUILD], [], [Build Control])
AC_DEFINE([GTK4_BUILD], [], [Build Control])
# Autoupdate added the next two lines to ensure that your configure
# script's behavior did not change.  They are probably safe to remove.
AC_CHECK_INCLUDES_DEFAULT
AC_PROG_EGREP

AC_SEARCH_LIBS([strerror],[cposix])

AC_CHECK_FUNCS([asprintf])

pkg_modules="gtk4 >= 4.8 gtksourceview-5 >= 5.0"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

AC_CONFIG_FILES([
Makefile
src/Makefile
])
AC_OUTPUT

