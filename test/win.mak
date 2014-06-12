#
# File:                 win.mak
#
# Description:          Makefile for regression tests on Windows
#                       (can be built using platform SDK's buildfarm)
#
# Usage:                NMAKE /f win.mak [ installcheck ]
#
# Comments:             Created by Michael Paquier, 2014-05-21
#

# Environment checks
!IF "$(CPU)" == ""
!MESSAGE Making 64bit DLL...
!MESSAGE You should set the CPU environment variable
!MESSAGE to distinguish your OS
!ENDIF

!IFNDEF PG_BIN
PG_LIB=C:\develop\bin\$(CPU)
!MESSAGE Using default PostgreSQL Binary directory: $(PG_BIN)
!ENDIF


# Include the list of tests
!INCLUDE tests

# The 'tests' file contains names of the test programs, in form
# src/<testname>-test. Extract the base names of the tests, by stripping the
# "src/" prefix and "-test" suffix. (It would seem more straightforward to do
# it the other way round, but it is surprisingly difficult to add a
# prefix/suffix to a list in nmake. Removing them is much easier.)
TESTS = $(TESTBINS:src/=)
TESTS = $(TESTS:-test=)
TESTS = sampletables $(TESTS)

# Now create names of the test .exe and .sql files from the base names

# src\<testname>.exe
TESTEXES = $(TESTBINS:-test=-test.exe)
TESTEXES = $(TESTEXES:src/=src\)

# sql\<testname>.sql
TESTSQLS = $(TESTBINS:-test=.sql)
TESTSQLS = $(TESTSQLS:src/=sql\)


# Flags
CLFLAGS=/D WIN32
LINKFLAGS=/link odbc32.lib odbccp32.lib

# Build an executable for each test.
#
# XXX: Note that nmake syntax doesn't allow passing a dependent on an
# inference rule. Hence, we cannot have a dependency to common.c here. So,
# we fail to notice if common.c changes. Also, we build common.c separately
# for each test - ideally we would build common.obj once and just link it
# to each test.
.c.exe:
	cl /Fe.\src\ /Fo.\src\ $*.c src/common.c $(CLFLAGS) $(LINKFLAGS)

all: $(TESTEXES) $(TESTSQLS)

# A rule to generate sql/<testname>.sql files. The expected output files
# are used as the dependent files, to give nmake a hint on how to build
# them, even though the .sql files don't really depend on the .out files.
{expected\}.out{sql\}.sql:
	echo \! "./src/$(*:sql\=)-test" > $*.sql

# activate the above inference rule
.SUFFIXES: .out


# Run regression tests
installcheck:
	cmd /c $(PG_BIN)\pg_regress --inputdir=. --psqldir="$(PG_BIN)" \
	    --dbname="contrib_regression" $(REGRESSOPTS) \
	    $(TESTS)

clean:
	-del src\*.exe
	-del src\*.obj
	-del $(TESTSQLS)
