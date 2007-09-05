# Process this file with autoconf to produce a configure script.
AC_INIT(psqlodbc, 08.02.0500, [pgsql-odbc@postgresql.org])
AC_PREREQ(2.57)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([bind.c])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE


# 0. Options processing


#
# Decide which unixODBC driver manager to use
#
AC_ARG_WITH(unixodbc, [  --with-unixodbc=DIR     odbc_config installation of unixODBC (default)],
[
	if test "$withval" != no; then
		if test "$withval" = yes; then
			AC_PATH_PROGS(ODBC_CONFIG, odbc_config)
		else
			ODBC_CONFIG=$withval
		fi
		if test ! -f "${ODBC_CONFIG}/bin/odbc_config"; then
			if test ! -f "${ODBC_CONFIG}"; then
				AC_MSG_ERROR([odbc_config not found (required for unixODBC build)])
			fi
		else
			ODBC_CONFIG=${ODBC_CONFIG}/bin/odbc_config
		fi
	fi

	with_unixodbc=yes],
[with_unixodbc=yes])


#
# Decide which iODBC driver manager to use
#
AC_ARG_WITH(iodbc, [  --with-iodbc=DIR	  iodbc_config installation of iODBC],
[
	if test "$withval" != no; then
		if test "$withval" = yes; then
			AC_PATH_PROGS(ODBC_CONFIG, iodbc-config)
		else
			ODBC_CONFIG=$withval
		fi
		if test ! -f "${ODBC_CONFIG}/bin/iodbc-config"; then
			if test ! -f "${ODBC_CONFIG}"; then
				AC_MSG_ERROR([iodbc-config not found (required for iODBC build)])
			fi
		else
			ODBC_CONFIG=${ODBC_CONFIG}/bin/iodbc-config
		fi
	fi

	with_iodbc=yes],
[with_iodbc=no])


if test "$with_iodbc" = yes; then
  with_unixodbc=no
  AC_DEFINE(WITH_IODBC, 1, [Define to 1 to build with iODBC support])
fi

if test "$with_unixodbc" = yes; then
  AC_DEFINE(WITH_UNIXODBC, 1,
            [Define to 1 to build with unixODBC support])
  AC_DEFINE(SQL_WCHART_CONVERT, [],
            [Define to use wchar_t as SQLWCHAR in unixODBC])
fi


#
# ODBC include and library
#

if test "$ODBC_CONFIG" != ""; then
	if test "$with_iodbc" = yes; then
		ODBC_INCLUDE=`${ODBC_CONFIG} --cflags`
		CPPFLAGS="$CPPFLAGS ${ODBC_INCLUDE}"
		ODBC_LIBS=`${ODBC_CONFIG} --libs`
		LIBS="$LIBS ${ODBC_LIBS}"
	else
		ODBC_INCLUDE=`${ODBC_CONFIG} --include-prefix`
		CPPFLAGS="$CPPFLAGS -I${ODBC_INCLUDE}"
		ODBC_LIBS=`${ODBC_CONFIG} --libs`
		LIBS="$LIBS ${ODBC_LIBS}"
	fi
	AC_MSG_NOTICE([using $ODBC_INCLUDE $ODBC_LIBS])
fi


#
# Default odbc version number (--with-odbcver), default 0x0351
#

PGAC_ARG_REQ(with, odbcver,
             [  --with-odbcver=VERSION  change default ODBC version number [[0x0351]]],
             [],
             [with_odbcver=0x0351])
AC_DEFINE_UNQUOTED(ODBCVER, [$with_odbcver], 
                   [Define to ODBC version (--with-odbcver)])


#
# Unicode support
#

PGAC_ARG_BOOL(enable, unicode, yes,
              [  --disable-unicode       do not build Unicode support],
              [AC_DEFINE(UNICODE_SUPPORT, 1,
                         [Define to 1 to build with Unicode support (--enable-unicode)])])

AM_CONDITIONAL(enable_unicode, [test x"$enable_unicode" = xyes])


#
# Pthreads
#

PGAC_ARG_BOOL(enable, pthreads, yes,
              [  --disable-pthreads      do not build with POSIX threads],
              [AC_DEFINE(POSIX_MULTITHREAD_SUPPORT, 1,
                         [Define to 1 to build with pthreads support (--enable-pthreads)])
               AC_DEFINE(_REENTRANT, 1, [Define _REENTRANT for several plaforms])])


#
# Find libpq headers and libraries
#

AC_PATH_PROGS(PG_CONFIG, pg_config)

if test -z "$PG_CONFIG"; then
  AC_MSG_ERROR([pg_config not found (set PG_CONFIG environment variable)])
fi

pg_includedir=`$PG_CONFIG --includedir`
pg_libdir=`$PG_CONFIG --libdir`
CPPFLAGS="$CPPFLAGS -I$pg_includedir"
LDFLAGS="$LDFLAGS -L$pg_libdir"



# 1. Programs

AC_PROG_CC


# 2. Libraries

AC_LIBTOOL_WIN32_DLL
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

if test "$with_unixodbc" = yes; then
  AC_SEARCH_LIBS(SQLGetPrivateProfileString, odbcinst, [],
                 [AC_MSG_ERROR([unixODBC library "odbcinst" not found])])
fi

if test "$with_iodbc" = yes; then
  AC_SEARCH_LIBS(SQLGetPrivateProfileString, iodbcinst, [],
                 [AC_MSG_ERROR([iODBC library "iodbcinst" not found])])
fi

if test "$enable_pthreads" = yes; then
  AC_CHECK_LIB(pthreads, pthread_create,
               [],
	       [AC_CHECK_LIB(pthread, pthread_create)])
fi


# 3. Header files

AC_CHECK_HEADERS(locale.h sys/un.h sys/time.h)
AC_HEADER_TIME


# 4. Types

# unixODBC wants the following to get sane behavior for ODBCINT64
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(void *)
AC_CHECK_TYPES(long long)

AC_CHECK_TYPES(ssize_t)
AC_TYPE_SIZE_T


# 5. Structures

AC_STRUCT_TM
PGAC_STRUCT_ADDRINFO


# 6. Compiler characteristics

AC_C_CONST


# 7. Functions, global variables

AC_CHECK_FUNCS(strerror_r)
AC_CHECK_FUNCS(strtoul strtoll)

if test x"$enable_unicode" = xyes; then
  AC_CHECK_FUNCS(iswascii)
fi

if test "$enable_pthreads" = yes; then
  AC_CHECK_FUNCS(localtime_r strtok_r pthread_mutexattr_settype)

  if test x"$ac_cv_func_pthread_mutexattr_settype" = xyes; then
    AC_TRY_COMPILE([#include <pthread.h>],
                 [int i = PTHREAD_MUTEX_RECURSIVE;],
                 [AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE,
                            [Define if you have PTHREAD_MUTEX_RECURSIVE])],
                 [AC_TRY_COMPILE([#include <pthread.h>],
		                 [int i = PTHREAD_MUTEX_RECURSIVE_NP;],
		                 [AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE_NP,
                                            [Define if you have PTHREAD_MUTEX_RECURSIVE_NP])])])
  fi
fi



AC_CONFIG_FILES([Makefile])
AC_OUTPUT
