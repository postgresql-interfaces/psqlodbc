#-------------------------------------------------------------------------
#
# Makefile.am for psqlodbc30w (PostgreSQL ODBC driver)
#
# $Header: /home/heikki/psqlodbc-cvs-copy/psqlodbc/Makefile.am,v 1.11.2.3 2006/01/26 14:41:37 h-saito Exp $
#
#-------------------------------------------------------------------------

AUTOMAKE_OPTIONS = 1.6 foreign

lib_LTLIBRARIES = psqlodbc30w.la

psqlodbc30w_la_LDFLAGS = -module -no-undefined -avoid-version

psqlodbc30w_la_LIBADD = -lpq

psqlodbc30w_la_SOURCES = \
	info.c bind.c columninfo.c connection.c convert.c drvconn.c \
	environ.c execute.c lobj.c win_md5.c misc.c options.c \
	pgtypes.c psqlodbc.c qresult.c results.c socket.c parse.c \
	statement.c tuple.c dlg_specific.c \
	multibyte.c odbcapi.c descriptor.c \
	odbcapi30.c pgapi30.c info30.c mylog.c \
\
	bind.h columninfo.h connection.h convert.h descriptor.h \
	dlg_specific.h environ.h iodbc.h isql.h isqlext.h \
	lobj.h md5.h misc.h multibyte.h pgapifunc.h pgtypes.h \
	psqlodbc.h qresult.h resource.h socket.h statement.h tuple.h \
	version.h

if enable_unicode
psqlodbc30w_la_SOURCES += \
odbcapiw.c odbcapi30w.c win_unicode.c
endif

EXTRA_psqlodbc30w_la_SOURCES = md5.c

EXTRA_DIST = license.txt readme.txt odbcinst.ini \
	libtool.m4 psqlodbc_api30w.def win32_30w.mak \
	dlg_wingui.c win_setup.h setup.c \
	win_unicode.c psqlodbc.rc psqlodbc.reg \
	psqlodbc35w.dsp psqlodbc35w.dsw 
	
MAINTAINERCLEANFILES = \
	Makefile.in config.guess config.h.in config.sub configure \
	install-sh missing mkinstalldirs aclocal.m4 ltmain.sh \
	libtool.m4 depcomp

