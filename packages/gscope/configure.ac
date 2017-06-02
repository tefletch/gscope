dnl Process this file with autoconf to produce a configure script.

AC_INIT([gscope], [2.8])
AC_CONFIG_SRCDIR([gscope.glade])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_SEARCH_LIBS([strerror],[cposix])
AC_HEADER_STDC

AC_CHECK_FUNCS([asprintf])

pkg_modules="gtk+-2.0 >= 2.24 gtksourceview-2.0 >= 2.8"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

AC_CONFIG_FILES([
Makefile
src/Makefile
])
AC_OUTPUT

