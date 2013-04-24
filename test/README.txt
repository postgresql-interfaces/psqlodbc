This directory contains a regression test suite for the psqlODBC driver.

The Makefile is written for Unix-like systems, there is no script to run the
tests on Windows at the moment.

Prerequisites
-------------

To run the regression tests, you must have a PostgreSQL server running and
accepting connections at port 5432. You must have the PostgreSQL server
binaries, including the regression test driver pg_regress, in your $PATH.

By default, the regression tests use the driver built from the parent directory,
../.libs/psqlodbcw.so, for the tests. You can edit odbcinst.ini in this
directory to test a different version.

Running the tests
-----------------

To run the test suite, type:

  make installcheck

The PostgreSQL username used for the test is determined by the normal ODBC /
libpq rules. You can set the PGUSER environment variable or .pgpass to
override.

Development
-----------

To add a test, add a *-test.c file to src/ directory, using one of the
existing tests as example. Also add the test to the TESTS list in the Makefile,
and create an expected output file in expected/ directory.

The current test suite only tests a small fraction of the codebase. Whenever
you add a new feature, or fix a non-trivial bug, please add a test case to
cover it.
