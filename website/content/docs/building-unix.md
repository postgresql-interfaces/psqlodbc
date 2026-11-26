---
title: "Compiling psqlODBC on Unix"
weight: 3
aliases:
  - /docs/unix-compilation.html
---

Following psqlODBC's move away from the main PostgreSQL source tree, Peter Eisentraut was kind enough provide a build environment for us which is now included in psqlODBC tarballs.

## Installation

To install, just try something like:

> 
>     % tar -zxvf psqlodbc-xx.xx.xxxx.tar.gz
>     % cd psqlodbc-xx.xx.xxxx
>     % ./configure
>     % make
>     % make install

The configure script will accept the following useful options:

- --with-libpq=DIR postgresql path (mandatory)
- --with-unixodbc=DIR path or direct odbc_config file (default:yes)
- --with-iodbc=DIR path or direct iodbc-config file
- --with-odbcver=VERSION change default ODBC version number \[0x0351\]
- --enable-pthreads (thread-safe driver on some platforms)
- --disable-unicode (build non-Unicode driver)
- --help

## Building a distribution

### Prerequisites

- Autoconf 2.59 or higher
- Automake 1.9.2 (Lower versions are rejected, version 1.7 untested.)
- Libtool 1.5.10 or higher
- PostgreSQL source tree (9.0 branch)

### Bootstrapping

> 
>     autoreconf -i

You can run 'make maintainer-clean' to remove all the files this generates.

### Building

> 
>     ./configure
>     make
>     make install

configure has the same options that the PostgreSQL configure used to have for ODBC, namely --with-iodbc or --with-unixodbc

### Making a Source Distribution

'make dist' makes a file psqlodbc-xx.xx.xxxx.tar.gz. It's even better to use 'make distcheck', which runs a number of tests to see if the distribution is internally consistent.

The distribution does not contain any Windows-specific files (\*.def, maybe others). If it is desired to include them, edit Makefile.am.
