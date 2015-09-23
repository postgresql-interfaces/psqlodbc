This directory contains a regression test suite for the psqlODBC driver.

Prerequisites
-------------

To run the regression tests, you must have a PostgreSQL server running and
accepting connections from the host where you run the regression tests.

By default in Linux, the regression tests use the driver built from the
parent directory, ../.libs/psqlodbcw.so, for the tests. You can edit
odbcinst.ini in this directory to test a different version.

Running the tests
-----------------

Linux
=====

To run the test suite, type:

  make installcheck

The test suite uses the normal ODBC / libpq defaults, which assume that the
server is running on the same host, at port 5432, and the username is the
same as the OS username. You can use PGHOST, PGUSER, etc. environment
variables or .pgpass to override these defaults.

You can also run "make installcheck-all" to run the regression suite with
different combinations of configuration options.

Windows
=======

To run the test suite, you need first to build and install the driver, and
create a Data Source with the name "psqlodbc_test_dsn". This DSN is used by
all the regression tests, and it should point to a valid PostgreSQL server.

Then type the following commands to run the tests:

  nmake /f win.mak
  nmake /f win.mak installcheck

Development
-----------

To add a test, add a *-test.c file to src/ directory, using one of the
existing tests as example. Also add the test to the TESTS list in the
"tests" file, and create an expected output file in expected/ directory.

The current test suite only tests a small fraction of the codebase. Whenever
you add a new feature, or fix a non-trivial bug, please add a test case to
cover it.
