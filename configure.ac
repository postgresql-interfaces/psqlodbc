# Process this file with autoconf to produce a configure script.
AC_INIT(psqlodbc, 08.02.0400, [pgsql-odbc@postgresql.org])
AC_PREREQ(2.57)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([bind.c])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE

AC_PROG_CC

#
# Decide which unixODBC driver manager to use
#
AC_ARG_WITH(unixodbc, [  --with-unixodbc=DIR     odbc-config installation of unixODBC(default)],
[
	if test "$withval" != no
	then
		if test "$withval" = yes;then
			ODBC_HOME=/usr
		else
			ODBC_HOME=$withval
		fi
		if test ! -f "${ODBC_HOME}/bin/odbc_config"
		then
			if test ! -f "${ODBC_HOME}/local/bin/odbc_config"
			then
				if test ! -f "${ODBC_HOME}"
				then
					AC_MSG_ERROR([odbc_config not find unixODBC installation in ${withval}])
				else
					ODBC_CONFIG=${ODBC_HOME}
				fi
			else
				ODBC_CONFIG=${ODBC_HOME}/local/bin/odbc_config
			fi
		else
			ODBC_CONFIG=${ODBC_HOME}/bin/odbc_config
		fi
	else
		ODBC_HOME=/usr
		if test ! -f "${ODBC_HOME}/bin/odbc_config"
		then
			if test ! -f "${ODBC_HOME}/local/bin/odbc_config"
			then
				if test ! -f "${ODBC_HOME}/odbc_config"
				then
					AC_MSG_ERROR([you must specify a valid unixODBC installation with --with-unixodbc=DIR])
				else
					ODBC_CONFIG=${ODBC_HOME}/odbc_config
				fi
			else
				ODBC_CONFIG=${ODBC_HOME}/local/bin/odbc_config
			fi
		else
			ODBC_CONFIG=${ODBC_HOME}/bin/odbc_config
		fi
	fi
	AC_DEFINE(WITH_UNIXODBC, 1,
	[Define to 1 to build with unixODBC support])
	[with_unixodbc=yes]
],[with_unixodbc=yes])

#
# Decide which iODBC driver manager to use
#
AC_ARG_WITH(iodbc, [  --with-iodbc=DIR	  iodbc-config installation of iODBC],
[
	if test "$withval" != no
	then
		if test "$withval" = yes;then
			ODBC_HOME=/usr
		else
			ODBC_HOME=$withval
		fi
		if test ! -f "${ODBC_HOME}/bin/iodbc-config"
		then
			if test ! -f "${ODBC_HOME}/local/bin/iodbc-config"
			then
				if test ! -f "${ODBC_HOME}"
				then
					AC_MSG_ERROR([iodbc-config not find iODBC installation in ${withval}])
				else
					ODBC_CONFIG=${ODBC_HOME}
				fi
			else
				ODBC_CONFIG=${ODBC_HOME}/local/bin/iodbc-config
			fi
		else
			ODBC_CONFIG=${ODBC_HOME}/bin/iodbc-config
		fi
	else
		ODBC_HOME=/usr
		if test ! -f "${ODBC_HOME}/bin/iodbc-config"
		then
			if test ! -f "${ODBC_HOME}/local/bin/iodbc-config"
			then
				if test ! -f "${ODBC_HOME}/iodbc-config"
				then
					AC_MSG_ERROR([you must specify a valid iODBC installation with --with-iodbc=DIR])
				else
					ODBC_CONFIG=${ODBC_HOME}/iodbc-config
				fi
			else
				ODBC_CONFIG=${ODBC_HOME}/local/bin/iodbc-config
			fi
		else
			ODBC_CONFIG=${ODBC_HOME}/bin/iodbc-config
		fi
	fi
	AC_DEFINE(WITH_IODBC, 1,
	[Define to 1 to build with iODBC support])
	[with_iodbc=yes]
],[with_iodbc=no])

#
# Default odbc version number (--with-odbcver), default 0x0351
#
AC_ARG_WITH(odbcver,
	[  --with-odbcver=VERSION  change default ODBC version number [[0x0351]]],
	[if test "$withval" != no;then
		ODBC_VER="$withval"
	else
		ODBC_VER="0x0351"
	fi],
[ODBC_VER="0x0351"])

AC_DEFINE_UNQUOTED(ODBCVER, $ODBC_VER,
                [Define to ODBC version (--with-odbcver)])
#
# odbc include and library
#
if test "$ODBC_CONFIG" != "";then
	if test "$with_iodbc" = yes;then
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
	AC_MSG_CHECKING($ODBC_CONFIG $CPPFLAGS $LIBS)
fi

#

if test "$with_iodbc" = yes; then
	with_unixodbc=no
        AC_DEFINE(WITH_IODBC, 1, [Define to 1 to build with iODBC support])
        AC_SEARCH_LIBS(SQLGetPrivateProfileString, iodbcinst, ,
                AC_MSG_ERROR("no iODBC Library found"))
fi

if test "$with_unixodbc" = yes; then
        AC_DEFINE(WITH_UNIXODBC, 1,
                [Define to 1 to build with unixODBC support])
        AC_DEFINE(SQL_WCHART_CONVERT, [],
                [Define to use wchar_t as SQLWCHAR in unixODBC])
        AC_SEARCH_LIBS(SQLGetPrivateProfileString, odbcinst, ,
                AC_MSG_ERROR("no unixODBC Library found"))
fi

#
# unicode support
#
AC_ARG_ENABLE(unicode, 
	[  --disable-unicode       unicode support (default enable).],
	[enable_unicode="$enableval"],
	[enable_unicode="yes"])

if test $enable_unicode = yes; then
	AM_CONDITIONAL(enable_unicode, [test x$enable_unicode = xyes])
	AC_DEFINE(UNICODE_SUPPORT, 1,
	[build with Unicode support])
	AC_CHECK_FUNCS(iswascii)
fi

AC_CHECK_FUNCS(strtoul strtoll)

#
# compile check the struct addrinfo, if exist define HAVE_STRUCT_ADDRINFO.
#
AC_MSG_CHECKING(for addrinfo)
AC_CACHE_VAL(ac_cv_struct_addrinfo,
AC_TRY_COMPILE([
#include <sys/types.h>
#include <netdb.h>],
	[struct addrinfo s],
	ac_cv_struct_addrinfo=yes,
	ac_cv_struct_addrinfo=no))
AC_MSG_RESULT($ac_cv_struct_addrinfo)

if test $ac_cv_struct_addrinfo = yes; then
	AC_DEFINE(HAVE_STRUCT_ADDRINFO, [1],
	[Define to 1 if you have struct addrinfo])
fi

#
# to implement the thread-safe driver
#
AC_ARG_ENABLE(pthread,
	[  --disable-pthread       pthread support (default enable).],
	[PTHREAD="$enableval"],
	[PTHREAD="yes"])

if test "$PTHREAD" = "yes"; then
	AC_DEFINE(POSIX_MULTITHREAD_SUPPORT, 1,
	[Define to 1 to build with pthreads support (--enable-pthreads)])
	AC_DEFINE(_REENTRANT, 1,
	[Define _REENTRANT for several plaforms])
	AC_CHECK_LIB(pthreads, pthread_create, [],
	[AC_CHECK_LIB(pthread, pthread_create)])
	AC_CHECK_FUNCS(localtime_r strtok_r strerror_r pthread_mutexattr_settype)
	if test x"$ac_cv_func_pthread_mutexattr_settype" = xyes; then
		AC_TRY_COMPILE([#include <pthread.h>],
		[ int i = PTHREAD_MUTEX_RECURSIVE;],
		[AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE,
		[Define if you have PTHREAD_MUTEX_RECURSIVE])], [
		AC_TRY_COMPILE([#include <pthread.h>],
		[ int i = PTHREAD_MUTEX_RECURSIVE_NP;],
		[AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE_NP,
		[Define if you have PTHREAD_MUTEX_RECURSIVE_NP])])])
	fi
fi

AC_CHECK_FUNCS(strerror_r)

AC_PROG_CC

AC_LIBTOOL_WIN32_DLL
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

AC_CHECK_HEADERS(locale.h sys/un.h sys/time.h)
AC_HEADER_TIME
AC_CHECK_TYPES(ssize_t)

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
AC_PATH_PROGS(PG_CONFIG, pg_config)

if test -z "$PG_CONFIG"; then
  AC_MSG_ERROR([pg_config not found (set PG_CONFIG environment variable)])
fi

pg_includedir=`$PG_CONFIG --includedir`
pg_libdir=`$PG_CONFIG --libdir`
CPPFLAGS="$CPPFLAGS -I$pg_includedir"
LDFLAGS="$LDFLAGS -L$pg_libdir"


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
