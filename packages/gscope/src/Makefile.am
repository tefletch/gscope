## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = gscope

gscope_SOURCES = \
	app_config.c \
	app_config.h \
	app_types.h  \
	auto_gen.c \
	auto_gen.h \
	browser.c \
	browser.h \
	build.c \
	build.h \
	callbacks.c \
	callbacks.h \
	crossref.c \
	crossref.h \
	dir.c \
	dir.h \
	display.c \
	display.h \
	fileview.c \
	fileview.h \
	global.h \
	interface.c \
	interface.h \
	lookup.c \
	lookup.h \
	main.c \
	scanner.c \
	scanner.h \
	search.c \
	search.h \
	support.c \
	support.h \
	utils.c \
	utils.h \
	version.h 

gscope_LDADD = @PACKAGE_LIBS@

