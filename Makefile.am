#-------------------------------------------------------------------------
#
# Makefile.am for psqlodbc (PostgreSQL ODBC driver)
#
# $Header: /home/heikki/psqlodbc-cvs-copy/psqlodbc/Makefile.am,v 1.1 2002/10/16 07:39:53 dpage Exp $
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
	multibyte.c \
	gpps.c

psqlodbc_la_SOURCES += \
	bind.h columninfo.h connection.h convert.h descriptor.h \
	dlg_specific.h environ.h gpps.h iodbc.h isql.h isqlext.h \
	lobj.h md5.h misc.h multibyte.h pgapifunc.h pgtypes.h \
	psqlodbc.h qresult.h resource.h socket.h statement.h tuple.h \
	tuplelist.h version.h

EXTRA_psqlodbc_la_SOURCES = md5.c

if with_unixodbc
psqlodbc_la_LIBADD = -lodbcinst
else
if with_iodbc
psqlodbc_la_LIBADD = -liodbcinst
else
AM_CPPFLAGS = -DODBCINSTDIR='"$(odbcinst_ini_dir)"'
endif
endif

dist_pkgdata_DATA = odbc.sql odbc-drop.sql
BUILT_SOURCES = odbc-drop.sql

EXTRA_DIST = license.txt notice.txt readme.txt odbcinst.ini libtool.m4
MAINTAINERCLEANFILES = odbc-drop.sql \
	Makefile.in config.guess config.h.in config.sub configure \
	install-sh missing mkinstalldirs aclocal.m4 ltmain.sh \
	libtool.m4 depcomp

$(srcdir)/odbc-drop.sql: $(srcdir)/odbc.sql
	sed -n '/^CREATE OR REPLACE FUNCTION/s/CREATE OR REPLACE FUNCTION \([^ (][^ (]*([^)]*)\).*/DROP FUNCTION \1;/p' $(srcdir)/odbc.sql >$(srcdir)/odbc-drop.sql
