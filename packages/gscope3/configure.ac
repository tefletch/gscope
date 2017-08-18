dnl Process this file with autoconf to produce a configure script.

AC_INIT([gscope], [3.10])
AC_CONFIG_SRCDIR([src/gscope3.glade])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_DEFINE([GTK3_BUILD], [], [Build Control])
AC_HEADER_STDC
AC_SEARCH_LIBS([strerror],[cposix])

AC_CHECK_FUNCS([asprintf])

pkg_modules="gtk+-3.0 >= 3.0 gtksourceview-3.0 >= 3.8"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

AC_CONFIG_FILES([
Makefile
src/Makefile
])
AC_OUTPUT

