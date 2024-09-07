## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

AM_CFLAGS = -Wno-deprecated-declarations
bin_PROGRAMS = gscope

gscope_SOURCES = \
	main.c       \
	global.h     \
	auto_gen.c   \
	auto_gen.h   \
	build.c      \
	build.h      \
	crossref.c   \
	crossref.h   \
	dir.c        \
	dir.h        \
	gtk4_aux.c   \
	gtk4_aux.h   \
	lookup.c 	 \
	lookup.h 	 \
	scanner.c 	 \
	scanner.h 	 \
	utils.c      \
	utils.h
#
#	app_config.c \
#	app_config.h \
#	browser.c   \
#	broswer.h   \
#	callbacks.c \
#	callbacks.h \
#	display.c 	\
#	display.h 	\
#	fileview.c 	\
#	fileview.h 	\
#	global.h	\
#	main.c 		\
#	search.c 	\
#	search.h 	\
#	support.c	\
#	support.h	\
#	utils.c 	\
#	utils.h 	\
#	version.h

xmldir = $(prefix)/bin
xml_DATA = gscope4.cmb

gscope_LDADD = @PACKAGE_LIBS@

gscope_LDFLAGS = -rdynamic

