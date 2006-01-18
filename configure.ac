# Process this file with autoconf to produce a configure script.
AC_INIT(psqlodbc, 07.03.0258, [pgsql-odbc@postgresql.org])
AC_PREREQ(2.52)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([bind.c])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE


AC_DEFINE(DRIVER_CURSOR_IMPLEMENT, 1,
	[Define to 1 to build with driver cursors option)])

PGAC_ARG_BOOL(with, unixodbc, no,
              [  --with-unixodbc         build ODBC driver for unixODBC],
              [AC_DEFINE(WITH_UNIXODBC, 1,
                         [Define to 1 to build with unixODBC support (--with-unixodbc)])])

PGAC_ARG_BOOL(with, iodbc, no,
              [  --with-iodbc            build ODBC driver for iODBC],
              [AC_DEFINE(WITH_IODBC, 1,
                         [Define to 1 to build with iODBC support (--with-iodbc)])])

if test "$with_unixodbc" = yes && test "$with_iodbc" = yes; then
  AC_MSG_ERROR([ODBC driver cannot be built for both unixODBC and iODBC])
fi

#
# Default odbc version number (--with-odbcver), default 0x0300
#
AC_MSG_CHECKING([for ODBC version number])
PGAC_ARG_REQ(with, odbcver, [  --with-odbcver=VERSION  change default ODBC version number
 [0x0300]],
             [AC_DEFINE_UNQUOTED(ODBCVER, ${withval},
          	[Define to ODBC version (--with-odbcver)])])
AC_MSG_RESULT([$with_odbcver])

AM_CONDITIONAL(with_unixodbc, [test $with_unixodbc = yes])
AM_CONDITIONAL(with_iodbc, [test $with_iodbc = yes])

AC_CHECK_FUNCS(strtoll)

# to implement the thread-safe driver
PGAC_ARG_BOOL(enable, pthreads, no,
    [  --enable-pthreads         build pthread implementation if possible],
    [AC_DEFINE(POSIX_MULTITHREAD_SUPPORT, 1,
	[Define to 1 to build with pthreads support (--enable-pthreads)])
     AC_DEFINE(_REENTRANT, 1,
	[Define _REENTRANT for several plaforms])
     AC_CHECK_LIB(c_r, gethostbyname)
     AC_CHECK_LIB(nsl, gethostbyname_r, [],
	[AC_CHECK_FUNCS(getipnodebyname gethostbyname_r, break)])
     if test x"$ac_cv_func_gethostbyname_r" = xyes || test x"$ac_cv_lib_nsl_gethostbyname_r" = xyes; then
	AC_TRY_COMPILE([#include <netdb.h>],
	[ gethostbyname_r((char *) 0, (struct hostent *) 0, (char *) 0, 0, (int *) 0);],
        [AC_DEFINE(PGS_REENTRANT_API_1, 1, [Define if you have 5 parameter gethostbyname_r])],
        [AC_DEFINE(PGS_REENTRANT_API_2, 1, [Define if you have 6 parameter gethostbyname_r])])
     fi
     AC_CHECK_FUNCS(localtime_r strtok_r)
     AC_CHECK_LIB(pthreads, pthread_create, [],
       [AC_CHECK_LIB(pthread, pthread_create)])
    ])

# Allow for overriding the default location of the odbcinst.ini
# file which is normally ${sysconfdir} (i.e., ${prefix}/etc).
PGAC_ARG_REQ(with, odbcinst,
             [  --with-odbcinst=DIR     default directory for odbcinst.ini [sysconfdir]],
             [odbcinst_ini_dir=$withval],
             [odbcinst_ini_dir="\${sysconfdir}"])
AC_SUBST([odbcinst_ini_dir])

AC_PROG_CC

AC_LIBTOOL_WIN32_DLL
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

AC_CHECK_HEADERS([locale.h])
AC_CHECK_TYPES(uint8)
PGAC_VAR_INT_TIMEZONE

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
