#-------------------------------------------------------------------------
#
# Makefile.am for psqlodbc (PostgreSQL ODBC driver)
#
# $Header: /home/heikki/psqlodbc-cvs-copy/psqlodbc/Makefile.am,v 1.31 2006/01/14 18:12:00 petere Exp $
#
#-------------------------------------------------------------------------

AUTOMAKE_OPTIONS = 1.6 foreign

if enable_unicode
lib_LTLIBRARIES = psqlodbcw.la
else
lib_LTLIBRARIES = psqlodbca.la
endif

AM_LDFLAGS = -module -no-undefined -avoid-version
LIBS += -lpq

psqlodbca_la_SOURCES = \
	info.c bind.c columninfo.c connection.c convert.c drvconn.c \
	environ.c execute.c win_md5.c misc.c options.c \
	pgtypes.c psqlodbc.c qresult.c results.c parse.c \
	statement.c tuple.c tuplelist.c dlg_specific.c odbcapi.c \
	multibyte.c \
	odbcapi30.c pgapi30.c info30.c descriptor.c

psqlodbca_la_SOURCES += \
	bind.h columninfo.h connection.h convert.h descriptor.h \
	dlg_specific.h environ.h \
	connection.h md5.h misc.h multibyte.h pgapifunc.h pgtypes.h \
	psqlodbc.h qresult.h resource.h statement.h tuple.h \
	tuplelist.h version.h

psqlodbcw_la_SOURCES = $(psqlodbca_la_SOURCES) \
	odbcapi30w.c odbcapiw.c win_unicode.c 	 

EXTRA_psqlodbca_la_SOURCES = md5.c
EXTRA_psqlodbcw_la_SOURCES = md5.c

EXTRA_DIST = license.txt readme.txt odbcinst.ini \
	libtool.m4 psqlodbca.def psqlodbcw.def win32.mak \
	dlg_wingui.c win_setup.h win_md5.c setup.c \
	win_unicode.c psqlodbc.rc psqlodbc.reg \
	psqlodbc.dsp psqlodbc.dsw \
	docs/index.html docs/config.html \
	docs/faq.html docs/howto-accesslo.html \
	docs/howto-accessvba.html docs/howto-bo.html \
	docs/howto-ch.html docs/howto-csharp.html \
	docs/howto-vb.html docs/howto-vblo.html \
	docs/unix-compilation.html docs/win32-compilation.html \
	docs/release.html \
	installer/banner.bmp installer/lgpl.rtf \
	installer/new.bmp installer/up.bmp \
	installer/Make.bat installer/upgrade.bat \
	installer/psqlodbc.wxs installer/psqlodbcm.wxs \
	installer/README.txt
	
MAINTAINERCLEANFILES = \
	Makefile.in config.guess config.h.in config.sub configure \
	install-sh missing mkinstalldirs aclocal.m4 ltmain.sh \
	libtool.m4 depcomp
