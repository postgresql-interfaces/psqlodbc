# Macros that test various C library quirks
# $PostgreSQL: pgsql/config/c-library.m4,v 1.31 2005/02/24 01:34:45 tgl Exp $


# PGAC_FUNC_GETTIMEOFDAY_1ARG
# ---------------------------
# Check if gettimeofday() has only one arguments. (Normal is two.)
# If so, define GETTIMEOFDAY_1ARG.
AC_DEFUN([PGAC_FUNC_GETTIMEOFDAY_1ARG],
[AC_CACHE_CHECK(whether gettimeofday takes only one argument,
pgac_cv_func_gettimeofday_1arg,
[AC_TRY_COMPILE([#include <sys/time.h>],
[struct timeval *tp;
struct timezone *tzp;
gettimeofday(tp,tzp);],
[pgac_cv_func_gettimeofday_1arg=no],
[pgac_cv_func_gettimeofday_1arg=yes])])
if test x"$pgac_cv_func_gettimeofday_1arg" = xyes ; then
  AC_DEFINE(GETTIMEOFDAY_1ARG,, [Define to 1 if gettimeofday() takes only 1 argument.])
fi
AH_VERBATIM(GETTIMEOFDAY_1ARG_,
[@%:@ifdef GETTIMEOFDAY_1ARG
@%:@ define gettimeofday(a,b) gettimeofday(a)
@%:@endif])dnl
])# PGAC_FUNC_GETTIMEOFDAY_1ARG


# PGAC_FUNC_GETPWUID_R_5ARG
# ---------------------------
# Check if getpwuid_r() takes a fifth argument (later POSIX standard, not draft version)
# If so, define GETPWUID_R_5ARG
AC_DEFUN([PGAC_FUNC_GETPWUID_R_5ARG],
[AC_CACHE_CHECK(whether getpwuid_r takes a fifth argument,
pgac_func_getpwuid_r_5arg,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <pwd.h>],
[uid_t uid;
struct passwd *space;
char *buf;
size_t bufsize;
struct passwd **result;
getpwuid_r(uid, space, buf, bufsize, result);],
[pgac_func_getpwuid_r_5arg=yes],
[pgac_func_getpwuid_r_5arg=no])])
if test x"$pgac_func_getpwuid_r_5arg" = xyes ; then
  AC_DEFINE(GETPWUID_R_5ARG,, [Define to 1 if getpwuid_r() takes a 5th argument.])
fi
])# PGAC_FUNC_GETPWUID_R_5ARG


# PGAC_FUNC_STRERROR_R_INT
# ---------------------------
# Check if strerror_r() returns an int (SUSv3) rather than a char * (GNU libc)
# If so, define STRERROR_R_INT
AC_DEFUN([PGAC_FUNC_STRERROR_R_INT],
[AC_CACHE_CHECK(whether strerror_r returns int,
pgac_func_strerror_r_int,
[AC_TRY_COMPILE([#include <string.h>],
[#ifndef _AIX
int strerror_r(int, char *, size_t);
#else
/* Older AIX has 'int' for the third argument so we don't test the args. */
int strerror_r();
#endif],
[pgac_func_strerror_r_int=yes],
[pgac_func_strerror_r_int=no])])
if test x"$pgac_func_strerror_r_int" = xyes ; then
  AC_DEFINE(STRERROR_R_INT,, [Define to 1 if strerror_r() returns a int.])
fi
])# PGAC_FUNC_STRERROR_R_INT
