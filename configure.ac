# Process this file with autoconf to produce a configure script.
AC_INIT(psqlodbc, 07.05.0002, [pgsql-odbc@postgresql.org])
AC_PREREQ(2.52)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([bind.c])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE


AC_DEFINE(DRIVER_CURSOR_IMPLEMENT, 1,
          [Define to 1 to build with driver cursors option.])

#
# Decide which ODBC driver manager to use
#
PGAC_ARG_BOOL(with, unixodbc, no,
              [  --with-unixodbc         build ODBC driver for unixODBC (default)],
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
  AC_DEFINE(WITH_UNIXODBC, 1, [Define to 1 to build with unixODBC support])
  AC_DEFINE(SQL_WCHART_CONVERT, [], [Define to use wchar_t as SQLWCHAR in unixODBC])
fi

if test "$with_iodbc" = yes; then
  AC_DEFINE(WITH_IODBC, 1, [Define to 1 to build with iODBC support])
fi


#
# Default odbc version number (--with-odbcver), default 0x0300
#
AC_MSG_CHECKING([for ODBC version number])
PGAC_ARG_REQ(with, odbcver, [  --with-odbcver=VERSION  change default ODBC version number [[0x0300]]],
             [],
             [with_odbcver=0x0300])
AC_MSG_RESULT([$with_odbcver])
AC_DEFINE_UNQUOTED(ODBCVER, [$with_odbcver], [Define to ODBC version (--with-odbcver)])

# to implement the Unicode driver
PGAC_ARG_BOOL(enable, unicode, yes,
              [  --enable-unicode        build ODBC driver for Unicode [[yes]]],
              [AC_DEFINE(UNICODE_SUPPORT, 1,
                         [Define to 1 to build with Unicode support (--enable-unicode)])
               AC_DEFINE(UNICODE, [],
                         [Define to use wide APIs])
               AC_DEFINE(SQL_NOUNICODEMAP, [],
                         [Define to disable mapping SQL* to SQL*W])
               AC_CHECK_FUNCS(iswascii)])

AM_CONDITIONAL(enable_unicode, [test x"$enable_unicode" = xyes])

AC_CHECK_FUNCS(strtoul strtoll)
AC_CHECK_LIB(socket, socket)

# to implement the thread-safe driver
PGAC_ARG_BOOL(enable, pthreads, no,
    [  --enable-pthreads       build pthread implementation if possible],
    [AC_DEFINE(POSIX_MULTITHREAD_SUPPORT, 1,
	[Define to 1 to build with pthreads support (--enable-pthreads)])
     AC_DEFINE(_REENTRANT, 1,
	[Define _REENTRANT for several plaforms])
     AC_CHECK_LIB(c_r, gethostbyname)
     AC_CHECK_LIB(pthreads, pthread_create, [],
       [AC_CHECK_LIB(pthread, pthread_create)])
     AC_CHECK_LIB(nsl, gethostbyname_r, [AC_DEFINE(HAVE_GETHOSTBYNAME_R, 1, [])],
	[AC_CHECK_FUNCS(getipnodebyname gethostbyname_r, break)])
     if test x"$ac_cv_func_gethostbyname_r" = xyes || test x"$ac_cv_lib_nsl_gethostbyname_r" = xyes; then
	AC_TRY_COMPILE([#include <netdb.h>],
	[ gethostbyname_r((char *) 0, (struct hostent *) 0, (char *) 0, 0, (int *) 0);],
        [AC_DEFINE(PGS_REENTRANT_API_1, 1, [Define if you have 5 parameter gethostbyname_r])],
        [AC_DEFINE(PGS_REENTRANT_API_2, 1, [Define if you have 6 parameter gethostbyname_r])])
     fi
     AC_CHECK_FUNCS(localtime_r strtok_r pthread_mutexattr_settype)
     if test x"$ac_cv_func_pthread_mutexattr_settype" = xyes; then
	AC_TRY_COMPILE([#include <pthread.h>],
	[ int i = PTHREAD_MUTEX_RECURSIVE;],
        [AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE, [Define if you have PTHREAD_MUTEX_RECURSIVE])], [
	AC_TRY_COMPILE([#include <pthread.h>],
	[ int i = PTHREAD_MUTEX_RECURSIVE_NP;],
        [AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE_NP, [Define if you have PTHREAD_MUTEX_RECURSIVE_NP])])])
     fi
    ])

AC_PROG_CC

AC_LIBTOOL_WIN32_DLL
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

AC_CHECK_HEADERS([locale.h])
AC_CHECK_HEADERS([sys/un.h])
AC_CHECK_TYPES(uint8)
PGAC_VAR_INT_TIMEZONE

# unixODBC wants the following to get sane behavior for ODBCINT64
AC_CHECK_SIZEOF(long)
AC_CHECK_TYPES(long long)

AC_C_CONST
AC_TYPE_SIZE_T
AC_STRUCT_TM

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
