This directory contains a regression test suite for the psqlODBC driver.

Prerequisites
-------------

To run the regression tests, you must have a PostgreSQL server running and
accepting connections at port 5432. You must have the PostgreSQL server
binaries, including the regression test driver pg_regress, in your $PATH.

By default in Linux, the regression tests use the driver built from the
parent directory, ../.libs/psqlodbcw.so, for the tests. You can edit
odbcinst.ini in this directory to test a different version.

Running the tests
-----------------

Linux
=====

To run the test suite, type:

  make installcheck

The PostgreSQL username used for the test is determined by the normal ODBC /
libpq rules. You can set the PGUSER environment variable or .pgpass to
override.

You can also run "make installcheck-all" to run the regression suite with
different combinations of configuration options.

Windows
=======

To run the test suite, you need first to build and install the driver, and
create a Data Source with the name "psqlodbc_test_dsn". This DSN is used by
all the regression tests, and it should point to a valid PostgreSQL server.

The Windows test suite makes use of environment variable PG_BIN to find the
location of pg_regress and the location of the binaries of PostgreSQL that
are passed to it. So be sure to set it accordingly. If the PostgreSQL server
is not running locally, at the default port, you will also need to pass
extra options to specify the hostname and port of the same server that the
DSN points to.

Then type the following commands to run the tests:

  nmake /f win.mak
  nmake /f win.mak installcheck

or for a non-default server host

  nmake /f win.mak installcheck REGRESSOPTS=--host=myserver.mydomain

Development
-----------

To add a test, add a *-test.c file to src/ directory, using one of the
existing tests as example. Also add the test to the TESTS list in the
"tests" file, and create an expected output file in expected/ directory.

The current test suite only tests a small fraction of the codebase. Whenever
you add a new feature, or fix a non-trivial bug, please add a test case to
cover it.
