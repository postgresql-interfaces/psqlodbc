# Process this file with autoconf to produce a configure script.
AC_INIT(psqlodbc, 08.02.0300, [pgsql-odbc@postgresql.org])
AC_PREREQ(2.52)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([bind.c])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE

#
# Decide which ODBC driver manager to use
#
PGAC_ARG_BOOL(with, unixodbc, no,
	[  --with-unixodbc         build ODBC driver for unixODBC],
		[AC_CHECK_LIB(odbcinst, SQLGetPrivateProfileString, [],
		[AC_MSG_ERROR([unixODBC driver manager not found])])])

PGAC_ARG_BOOL(with, iodbc, no,
	[  --with-iodbc            build ODBC driver for iODBC],
		[AC_CHECK_LIB(iodbcinst, SQLGetPrivateProfileString, [],
		[AC_MSG_ERROR([iODBC driver manager not found])])])

if test "$with_unixodbc" = yes && test "$with_iodbc" = yes; then
	AC_MSG_ERROR([ODBC driver cannot be built for both unixODBC and iODBC])
fi

if test "$with_unixodbc" != yes && test "$with_iodbc" != yes; then
	AC_CHECK_LIB(odbcinst, SQLGetPrivateProfileString,
		[with_unixodbc=yes],
	[AC_CHECK_LIB(iodbcinst, SQLGetPrivateProfileString,
		[with_iodbc=yes],
	[AC_MSG_ERROR([no suitable ODBC driver manager found])])])
fi

if test "$with_unixodbc" = yes; then
	AC_DEFINE(WITH_UNIXODBC, 1, 
		[Define to 1 to build with unixODBC support])
	AC_DEFINE(SQL_WCHART_CONVERT, [], 
		[Define to use wchar_t as SQLWCHAR in unixODBC])
	AC_SEARCH_LIBS(SQLGetPrivateProfileString, odbcinst, ,
		AC_MSG_ERROR("no unixODBC Library found"))
fi

if test "$with_iodbc" = yes; then
	AC_DEFINE(WITH_IODBC, 1, [Define to 1 to build with iODBC support])
	AC_SEARCH_LIBS(SQLGetPrivateProfileString, iodbcinst, ,
		AC_MSG_ERROR("no iODBC Library found"))
fi

#
# Default odbc version number (--with-odbcver), default 0x0351
#
PGAC_ARG_REQ(with, odbcver, 
	[  --with-odbcver=VERSION  change default ODBC version number [[0x0351]]],
	[],
	[with_odbcver=0x0351])
AC_MSG_RESULT([$with_odbcver])
AC_DEFINE_UNQUOTED(ODBCVER, [$with_odbcver], 
	[Define to ODBC version (--with-odbcver)])

# to implement the Unicode driver
PGAC_ARG_BOOL(enable, unicode, yes,
	[  --enable-unicode        build ODBC driver for Unicode [[yes]]],
	[AC_DEFINE(UNICODE_SUPPORT, 1,
		[Define to 1 to build with Unicode support (--enable-unicode)])
	 AC_CHECK_FUNCS(iswascii)])

AM_CONDITIONAL(enable_unicode, [test x"$enable_unicode" = xyes])

AC_CHECK_FUNCS(strtoul strtoll)

#
# If `struct addrinfo' exists, define HAVE_STRUCT_ADDRINFO.
#
PGAC_STRUCT_ADDRINFO

#
# to implement the thread-safe driver
#
PGAC_ARG_BOOL(enable, pthreads, no,
    [  --enable-pthreads       build pthread implementation if possible],
    [AC_DEFINE(POSIX_MULTITHREAD_SUPPORT, 1,
	[Define to 1 to build with pthreads support (--enable-pthreads)])
     AC_DEFINE(_REENTRANT, 1,
	[Define _REENTRANT for several plaforms])
     AC_CHECK_LIB(pthreads, pthread_create, [],
       [AC_CHECK_LIB(pthread, pthread_create)])
     AC_CHECK_FUNCS(localtime_r strtok_r strerror_r pthread_mutexattr_settype)
     if test x"$ac_cv_func_pthread_mutexattr_settype" = xyes; then
	AC_TRY_COMPILE([#include <pthread.h>],
	[ int i = PTHREAD_MUTEX_RECURSIVE;],
        [AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE, [Define if you have PTHREAD_MUTEX_RECURSIVE])], [
	AC_TRY_COMPILE([#include <pthread.h>],
	[ int i = PTHREAD_MUTEX_RECURSIVE_NP;],
        [AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE_NP, [Define if you have PTHREAD_MUTEX_RECURSIVE_NP])])])
     fi
     PGAC_FUNC_STRERROR_R_INT
    ])

AC_PROG_CC

AC_LIBTOOL_WIN32_DLL
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

AC_CHECK_HEADERS(locale.h sys/un.h sys/time.h)
AC_HEADER_TIME
AC_CHECK_TYPES(ssize_t)
PGAC_VAR_INT_TIMEZONE

# unixODBC wants the following to get sane behavior for ODBCINT64
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(void *)
AC_CHECK_TYPES(long long)

AC_C_CONST
AC_TYPE_SIZE_T
AC_STRUCT_TM

#
# Find libpq headers and libraries
#

PGAC_ARG(with, pgconf,
	[AC_HELP_STRING([--with-pgconf=pg_config],
	[specify pg_config with full path when it is not easy to find it.])],
	[pgconf="pg_config"],[option --without-pgconf does not apply],
	[pgconf=$withval],[pgconf="pg_config"])

AC_PATH_PROGS(PG_CONFIG, $pgconf)

if test -z "$PG_CONFIG" ; then
	AC_MSG_ERROR([pg_config not found])
fi

pg_includedir=`$PG_CONFIG --includedir`
pg_libdir=`$PG_CONFIG --libdir`
CPPFLAGS="$CPPFLAGS -I$pg_includedir"
LIBS="-L$pg_libdir $LIBS"

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
