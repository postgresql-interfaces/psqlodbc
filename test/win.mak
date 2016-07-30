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


# Include the list of tests
!INCLUDE tests

SRCDIR=src
OBJDIR=exe
EXEDIR=exe

# The 'tests' file contains names of the test programs, in form
# exe/<testname>-test. Extract the base names of the tests, by stripping the
# "exe/" prefix and "-test" suffix. (It would seem more straightforward to do
# it the other way round, but it is surprisingly difficult to add a
# prefix/suffix to a list in nmake. Removing them is much easier.)
TESTS = $(TESTBINS:exe/=)
TESTS = $(TESTS:-test=)

# Now create names of the test .exe from the base names

# exe\<testname>.exe
TESTEXES = $(TESTBINS:-test=-test.exe)
TESTEXES = $(TESTEXES:/=\)

COMSRC = $(SRCDIR)\common.c
COMOBJ = $(OBJDIR)\common.obj

# Flags
CLFLAGS=/W3 /D WIN32 /D _CRT_SECURE_NO_DEPRECATE
LINKFLAGS=/link odbc32.lib odbccp32.lib

# Build an executable for each test.
#
{$(SRCDIR)\}.c{$(EXEDIR)\}.exe:
	$(CC) /Fe.\$(EXEDIR)\ /Fo.\$(OBJDIR)\ $< $(COMOBJ) $(CLFLAGS) $(LINKFLAGS)

all: $(TESTEXES) runsuite.exe

$(TESTEXES): $(OBJDIR) $(COMOBJ)

$(COMOBJ): $(COMSRC)
	$(CC) $(CLFLAGS) /c $? /Fo$@

$(OBJDIR) :
!IF !EXIST($(OBJDIR))
	mkdir $(OBJDIR)
!ENDIF
!IF !EXIST($(EXEDIR)) && "$(EXEDIR)" != "$(OBJDIR)"
	mkdir "$(EXEDIR)"
!ENDIF


runsuite.exe: runsuite.c
	cl runsuite.c $(CLFLAGS) $(LINKFLAGS)

reset-db.exe: reset-db.c
	cl reset-db.c $(CLFLAGS) $(LINKFLAGS)

# activate the above inference rule
.SUFFIXES: .out

# Run regression tests
RESDIR=results
installcheck: runsuite.exe $(TESTEXES) reset-db.exe
	del regression.diffs
	.\reset-db < sampletables.sql
!IF !EXIST($(RESDIR))
	mkdir $(RESDIR)
!ENDIF
	.\runsuite $(TESTS)

clean:
	-del $(EXEDIR)\*.exe
	-del $(OBJDIR)\*.obj
