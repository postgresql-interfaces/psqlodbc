# Process this file with autoconf to produce a configure script.
AC_INIT(psqlodbclibpq, 08.01.0004, [pgsql-odbc@postgresql.org])
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

AC_CHECK_FUNCS(strtoul strtoll)

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

#Decide whether libpq or socket based driver to be built 
PGAC_ARG_BOOL(with, libpq, yes,
          [  --with-libpq            build libpq enabled odbc driver(default)],[use_libpq=yes],[use_libpq=no])

#
#Options for pgsql headers and libraries
#
if test "$use_libpq" = "yes"; then
        AC_MSG_CHECKING(for pgsql include files)
        AC_ARG_WITH(pgsql-include,
        [  --with-pgsql-include=DIR  Look for postgreSQL include files in DIR],
        [if test "$withval" != no; then
            AC_MSG_RESULT(yes)
            pgsql_include="$withval"
        else
            AC_MSG_RESULT(no)
            AC_MSG_ERROR([you must specify a directory when using --with-pgsql-include=DIR])
         fi])
fi

#
# Library directories
#
if test "$use_libpq" = "yes"; then
        AC_MSG_CHECKING(for pgsql)
        AC_ARG_WITH(pgsql,
        [  --with-pgsql=DIR        look for PostgreSQL libraries and headers in DIR],
        [if test "$withval" != no
        then
            AC_MSG_RESULT(yes)
            LIBPQ_HOME="$withval"
        else
            AC_MSG_RESULT(no)
        fi], [
        AC_MSG_RESULT(yes)
        LIBPQ_HOME="/usr/local/pgsql"
        if test "$pgsql_include" = ""
        then
                if test ! -f "${LIBPQ_HOME}/include/libpq-fe.h"
                then
                    LIBPQ_HOME=/usr/local
                    if test ! -f "${LIBPQ_HOME}/include/libpq-fe.h"
                    then
                        LIBPQ_HOME=/usr
                    fi
                fi
                if test -f "/usr/include/libpq-fe.h"
                then
                    pgsql_include="/usr/include"
                else
                    if test -f "/usr/include/pgsql/libpq-fe.h"
                    then
                        pgsql_include="/usr/include/pgsql"
                    fi
                fi
         fi
	])
#
# Check for libpq libraries and headers
#
        if test -n "${LIBPQ_HOME}"
        then
            OLD_LDFLAGS="$LDFLAGS"
            OLD_CPPFLAGS="$CPPFLAGS"
            LDFLAGS="$LDFLAGS -L${LIBPQ_HOME}/lib"
            AC_CHECK_LIB(pq, PQexec, [pgsql_cv_libpq=yes], [pgsql_cv_libpq=no])

            if test "$pgsql_include" != ""
            then
                CPPFLAGS="$CPPFLAGS -I${pgsql_include}"
            else
                CPPFLAGS="$CPPFLAGS -I${LIBPQ_HOME}/include"
            fi
            AC_CHECK_HEADER(libpq-fe.h, [pgsql_cv_libpqfe_h=yes], [pgsql_cv_libpqfe_h=no])

            if test "$pgsql_cv_libpq" = "yes" -a "$pgsql_cv_libpqfe_h" = "yes"
            then
                AC_MSG_CHECKING(pgsql in ${LIBPQ_HOME})
                AC_MSG_RESULT(ok)
            else
                AC_MSG_CHECKING(pgsql in ${LIBPQ_HOME})
                AC_MSG_RESULT(failed)
                LDFLAGS="$OLD_LDFLAGS"
                CPPFLAGS="$OLD_CPPFLAGS"
                AC_MSG_ERROR([you must specify a valid pgsql installation with --with-pgsql=DIR])
            fi
            LIBS="$LIBS -lpq"
            CPPFLAGS="$CPPFLAGS -DUSE_LIBPQ"

        fi
else
        AC_CHECK_LIB(socket, socket)
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
