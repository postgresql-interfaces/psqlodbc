# Process this file with autoconf to produce a configure script.
AC_INIT(psqlodbc, 7.2.3, [pgsql-odbc@postgresql.org])
AC_PREREQ(2.52)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([bind.c])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE


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

AM_CONDITIONAL(with_unixodbc, [test $with_unixodbc = yes])
AM_CONDITIONAL(with_iodbc, [test $with_iodbc = yes])

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
