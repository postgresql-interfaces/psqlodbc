#-------------------------------------------------------------------------
#
# Makefile.am for psqlodbc30w (PostgreSQL ODBC driver)
#
# $Header: /home/heikki/psqlodbc-cvs-copy/psqlodbc/Makefile.am,v 1.36 2006/08/29 13:12:58 dpage Exp $
#
#-------------------------------------------------------------------------

AUTOMAKE_OPTIONS = 1.6 foreign

lib_LTLIBRARIES = psqlodbc.la

psqlodbc_la_LDFLAGS = -module -no-undefined -avoid-version

psqlodbc_la_LIBADD = -lpq

psqlodbc_la_SOURCES = \
	info.c bind.c columninfo.c connection.c convert.c drvconn.c \
	environ.c execute.c lobj.c win_md5.c misc.c options.c \
	pgtypes.c psqlodbc.c qresult.c results.c socket.c parse.c \
	statement.c tuple.c dlg_specific.c loadlib.c \
	multibyte.c odbcapi.c descriptor.c \
	odbcapi30.c pgapi30.c info30.c mylog.c \
\
	bind.h catfunc.h columninfo.h connection.h convert.h \
	descriptor.h dlg_specific.h environ.h \
	lobj.h md5.h misc.h multibyte.h pgapifunc.h pgtypes.h \
	psqlodbc.h qresult.h resource.h socket.h statement.h tuple.h \
	version.h loadlib.h

if enable_unicode
psqlodbc_la_SOURCES += \
odbcapiw.c odbcapi25w.c odbcapi30w.c win_unicode.c
endif

EXTRA_psqlodbc_la_SOURCES = md5.c

EXTRA_DIST = license.txt readme.txt \
	libtool.m4 psqlodbc.def psqlodbca.def \
	dlg_wingui.c inouealc.c win_setup.h \
        setup.c win_unicode.c psqlodbc.rc \
        win32.mak psqlodbc.reg psqlodbc.dsp \
        psqlodbc.sln msdtc_enlist.cpp pgxalib.cpp \
        pgxalib.def odbc.sql odbc-drop.sql \
 \
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

