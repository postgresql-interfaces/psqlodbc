# Process this file with autoconf to produce a configure script.
AC_INIT(psqlodbc, 09.01.0200, [pgsql-odbc@postgresql.org])
AC_PREREQ(2.57)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([bind.c])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE


# 0. Options processing


#
# Whether unixODBC driver manager is used
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
		with_unixodbc=yes
	fi
	],
[with_unixodbc=yes])


#
# Whether iODBC driver manager is used
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
		with_iodbc=yes
	fi
	],
[with_iodbc=no])


if test "$with_iodbc" = yes; then
  with_unixodbc=no
  AC_DEFINE(WITH_IODBC, 1, [Define to 1 to build with iODBC support])
fi

if test "$with_unixodbc" = yes; then
  AC_DEFINE(WITH_UNIXODBC, 1,
            [Define to 1 to build with unixODBC support])
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
# SQLCOLATTRIBUTE_SQLLEN check
#
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[#include <sql.h>>
	SQLRETURN  SQL_API SQLColAttribute
	(SQLHSTMT StatementHandle,SQLUSMALLINT ColumnNumber,
	SQLUSMALLINT FieldIdentifier, SQLPOINTER CharacterAttribute,
	SQLSMALLINT BufferLength, SQLSMALLINT *StringLength, 
	SQLLEN *NumercAttribute);]],[[]])],
	AC_DEFINE(SQLCOLATTRIBUTE_SQLLEN, 1,
	[Define to 1 if SQLColAttribute use SQLLEN]))


#
# Whether libpq functionalities are used
#
AC_ARG_WITH(libpq, [  --with-libpq[[=DIR]]	  [[default=yes]] DIR is the PostgreSQL base install
			  directory or the path to pg_config
  --without-libpq	  specify when PostgreSQL package isn't installed],
[
	if test "$withval" = no; then
  		AC_DEFINE(NOT_USE_LIBPQ, 1, 
			 [Define to 1 to build without libpq])
		enable_openssl=no
	else
		if test "$withval" != yes; then
			if test -d "$withval"; then
    				PATH="$PATH:$withval/bin"
				CPPFLAGS="$CPPFLAGS -I$withval/include"
    				LDFLAGS="$LDFLAGS -L$withval/lib"
			else
				if test -x "$withval"; then
					PG_CONFIG=$withval
				else
					AC_MSG_ERROR([specified pg_config not found])
				fi
			fi
		fi
		with_libpq=yes
	fi
	],
[with_libpq=yes]) 


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
# SSL support
#

PGAC_ARG_BOOL(enable, openssl, yes,
              [  --disable-openssl       do not build OpenSSL support],
              [AC_DEFINE(USE_SSL, 1,
                         [Define to 1 to build with OpenSSL support (--enable-openssl)])])

AM_CONDITIONAL(enable_openssl, [test x"$enable_openssl" = xyes])

#
# GSSAPI support
#

PGAC_ARG_BOOL(enable, gss, no,
              [  --disable-gss       do not build GSSAPI support],
              [AC_DEFINE(USE_GSS, 1,
                         [Define to 1 to build with GSSAPI support (--enable-gss)])])

AM_CONDITIONAL(enable_gss, [test x"$enable_gss" = xyes])

#
# GKerberos 5 support
#

PGAC_ARG_BOOL(enable, krb5, no,
              [  --disable-krb5       do not build Kerberos5 support],
              [AC_DEFINE(USE_KRB5, 1,
                         [Define to 1 to build with Kerberos 5 support (--enable-krb5)])])

AM_CONDITIONAL(enable_krb5, [test x"$enable_krb5" = xyes])

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

if test "$with_libpq" = yes; then
  if test -z "$PG_CONFIG"; then
  	AC_PATH_PROGS(PG_CONFIG, pg_config)
  fi

  if test -n "$PG_CONFIG"; then
    pg_includedir=`"$PG_CONFIG" --includedir`
    pg_libdir=`"$PG_CONFIG" --libdir`
    CPPFLAGS="$CPPFLAGS -I$pg_includedir"
    LDFLAGS="$LDFLAGS -L$pg_libdir"
  fi
fi



# 1. Programs

AC_PROG_CC

AM_CONDITIONAL([GCC], [test -n "$GCC"])

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

if test "$with_libpq" = yes; then
  AC_CHECK_LIB(pq, PQconnectdb, [],
	      [AC_MSG_ERROR([libpq library not found])])
fi

if test "$enable_openssl" = yes; then
  AC_CHECK_LIB(ssl, SSL_read, [], [AC_MSG_ERROR([ssl library not found])])
fi

if test "$enable_gss" = yes; then
  AC_SEARCH_LIBS(gss_init_sec_context, [gssapi_krb5 gss 'gssapi -lkrb5 -lcrypto'], [],  [AC_MSG_ERROR([gssapi library not found])])
fi

if test "$enable_krb5" = yes; then
  AC_SEARCH_LIBS(krb5_sendauth, [krb5 'krb5 -lcrypto -ldes -lasn1 -lroken'], [], [AC_MSG_ERROR([krb5 library not found])])
  AC_SEARCH_LIBS(com_err, [krb5 'krb5 -lcrypto -ldes -lasn1 -lroken' comerr 'comerr -lssl -lcrypto'], [], [AC_MSG_ERROR([comerr library not found])])
fi

# 3. Header files

AC_CHECK_HEADERS(locale.h sys/un.h sys/time.h)
if test "$with_libpq" = yes; then
  AC_CHECK_HEADER(libpq-fe.h,,[AC_MSG_ERROR([libpq header not found])])
fi
if test "$enable_openssl" = yes; then
  AC_CHECK_HEADER([openssl/ssl.h],,[AC_MSG_ERROR([ssl header not found])])
fi
if test "$enable_gss" = yes; then
  AC_CHECK_HEADERS([gssapi/gssapi.h], [],
	[AC_CHECK_HEADERS([gssapi.h], [], [AC_MSG_ERROR([gssapi header not found])])])
fi
if test "$enable_krb5" = yes; then
  AC_CHECK_HEADER([krb5.h],,[AC_MSG_ERROR([krb5 header not found])])
fi
AC_HEADER_TIME


# 4. Types

# unixODBC wants the following to get sane behavior for ODBCINT64
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(void *)
AC_CHECK_TYPES(long long)
AC_CHECK_TYPES(signed char)

AC_CHECK_TYPES(ssize_t)
AC_TYPE_SIZE_T

# 5. Structures

AC_STRUCT_TM
PGAC_STRUCT_ADDRINFO


# 6. Compiler characteristics

AC_C_CONST


# 7. Functions, global variables

AC_FUNC_STRERROR_R
AC_CHECK_FUNCS(strtoul strtoll strlcat strlcpy poll)

if test x"$enable_unicode" = xyes; then
  AC_CHECK_FUNCS(iswascii)
fi

if test "$enable_pthreads" = yes; then
  AC_CHECK_FUNCS(localtime_r strtok_r pthread_mutexattr_settype)

  if test x"$ac_cv_func_pthread_mutexattr_settype" = xyes; then
    AC_COMPILE_IFELSE(AC_LANG_PROGRAM([[#include <pthread.h>]],
                 [[int i = PTHREAD_MUTEX_RECURSIVE;]]),
                 [AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE,
                            [Define if you have PTHREAD_MUTEX_RECURSIVE])],
                 [AC_COMPILE_IFELSE(AC_LANG_PROGRAM([[#include <pthread.h>]],
		                 [[int i = PTHREAD_MUTEX_RECURSIVE_NP;]]),
		                 [AC_DEFINE(PG_RECURSIVE_MUTEXATTR, PTHREAD_MUTEX_RECURSIVE_NP,
                                            [Define if you have PTHREAD_MUTEX_RECURSIVE_NP])])])
  fi
fi
if test "$enable_krb5" = yes; then
  AC_CHECK_MEMBERS(krb5_error.text.data, [],
                   [AC_CHECK_MEMBERS(krb5_error.e_data, [],
                                     [AC_MSG_ERROR([could not determine how to extract Kerberos 5 error messages])],
                                     [#include <krb5.h>])],
                   [#include <krb5.h>])
  AC_MSG_CHECKING(for krb5_free_unparsed_name)
  AC_TRY_LINK([#include <krb5.h>],
              [krb5_free_unparsed_name(NULL,NULL);],
              [AC_DEFINE(HAVE_KRB5_FREE_UNPARSED_NAME, 1, [Define to 1 if you have krb5_free_unparsed_name]) 
AC_MSG_RESULT(yes)],
              [AC_MSG_RESULT(no)])
fi

# 8. Libltdl
AC_CHECK_LIB(ltdl, lt_dlopen)


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
