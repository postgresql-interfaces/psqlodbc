#-------------------------------------------------------------------------
#
# Makefile.am for psqlodbc (PostgreSQL ODBC driver)
#
# $Header: /home/heikki/psqlodbc-cvs-copy/psqlodbc/Makefile.am,v 1.19 2005/08/01 20:17:43 dpage Exp $
#
#-------------------------------------------------------------------------

AUTOMAKE_OPTIONS = 1.6 foreign

lib_LTLIBRARIES = psqlodbc.la

psqlodbc_la_LDFLAGS = -module -no-undefined -avoid-version

psqlodbc_la_SOURCES = \
	info.c bind.c columninfo.c connection.c convert.c drvconn.c \
	environ.c execute.c lobj.c win_md5.c misc.c options.c \
	pgtypes.c psqlodbc.c qresult.c results.c socket.c parse.c \
	statement.c tuple.c tuplelist.c dlg_specific.c odbcapi.c \
	multibyte.c odbcapi30w.c odbcapiw.c \
	odbcapi30.c pgapi30.c info30.c descriptor.c \
    win_unicode.c

psqlodbc_la_SOURCES += \
	bind.h columninfo.h connection.h convert.h descriptor.h \
	dlg_specific.h environ.h \
	lobj.h libpqconnection.h md5.h misc.h multibyte.h pgapifunc.h pgtypes.h \
	psqlodbc.h qresult.h resource.h socket.h statement.h tuple.h \
	tuplelist.h version.h

EXTRA_psqlodbc_la_SOURCES = md5.c

EXTRA_DIST = README.unix license.txt notice.txt readme.txt odbcinst.ini \
	libtool.m4 psqlodbc_win32.def win32.mak \
	dlg_wingui.c win_setup.h win_md5.c setup.c \
	psqlodbc.rc psqlodbc.reg \
    installer/banner.bmp installer/lgpl.rtf \
    installer/Make.bat installer/psqlodbc.wxs \
    installer/psqlodbcm.wxs installer/README.txt
	
MAINTAINERCLEANFILES = \
	Makefile.in config.guess config.h.in config.sub configure \
	install-sh missing mkinstalldirs aclocal.m4 ltmain.sh \
	libtool.m4 depcomp
