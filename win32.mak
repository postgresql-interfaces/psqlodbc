#
# File:			win32.mak
#
# Description:		psqlodbc Unicode version Makefile for Win32.
#
# Configurations:	Debug, Release
# Build Types:		ALL, CLEAN
# Usage:	 	NMAKE /f win32.mak [CFG=[Release | Debug]]
#                                          [ENCODING=[ANSI | UNICODE]]
#                                          [PG_INC=<PG include folder> PG_LIB=<PG lib folder>]
#                                          [ALL | CLEAN] 
# Comments:		Created by Dave Page, 2001-02-12
#				Modified by Siva Kumar , 2005-09-15
#				Modified by Anoop Kumar, 2005-09-19
#

!MESSAGE Building the PostgreSQL ODBC Driver for Win32 (Libpq Version)...
!MESSAGE
!IF "$(CFG)" == ""
CFG=Release
!MESSAGE No configuration specified. Defaulting to Release.
!ENDIF 

!IF "$(PG_INC)" == ""
PG_INC=$(PROGRAMFILES)\PostgreSQL\8.1\include
!MESSAGE Using default PostgreSQL Include directory: $(PG_INC)
!ENDIF 

!IF "$(PG_LIB)" == ""
PG_LIB=$(PROGRAMFILES)\PostgreSQL\8.1\lib\ms
!MESSAGE Using default PostgreSQL Library directory: $(PG_LIB)
!ENDIF 

!IF "$(ENCODING)" == "ANSI"
DLLNAME=psqlodbca
!MESSAGE Building an ANSI driver.
!ENDIF 

!IF "$(ENCODING)" == "UNICODE"
DLLNAME=psqlodbcw
!MESSAGE Building a Unicode driver.
!ENDIF

!IF "$(ENCODING)" == ""
DLLNAME=psqlodbca
ENCODING=ANSI
!MESSAGE No encoding specified. Building an ANSI driver.
!ENDIF 

!MESSAGE

!IF "$(CFG)" != "Release" && "$(CFG)" != "Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f win32.mak [CFG=[Release | Debug]]
!MESSAGE                    [ENCODING=[ANSI | UNICODE]]
!MESSAGE                    [PG_INC=<PG include folder> PG_LIB=<PG lib folder>]
!MESSAGE                    [ALL | CLEAN]
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Release" (Win32 Release DLL)
!MESSAGE "Debug"   (Win32 Debug DLL)
!MESSAGE 
!ERROR An invalid configuration was specified.
!ENDIF 

!IF "$(ENCODING)"  != "ANSI" && "$(ENCODING)"  != "UNICODE"
!MESSAGE Invalid encoding "$(ENCODING)" specified.
!MESSAGE You can specify the connection mode and PostgreSQL include folder
!MESSAGE by defining the macros MODE  and  PG_INC on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f win32.mak [CFG=[Release | Debug]]
!MESSAGE                    [ENCODING=[ANSI | UNICODE]]
!MESSAGE                    [PG_INC=<PG include folder> PG_LIB=<PG lib folder>]
!MESSAGE                    [ALL | CLEAN]
!MESSAGE 
!MESSAGE Possible choices for mode are:
!MESSAGE 
!MESSAGE "ANSI"    (Build an ANSI/Multibyte driver)
!MESSAGE "UNICODE" (Build a Unicode driver)
!MESSAGE 
!ERROR An invalid mode was specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "Release"

!IF "$(ENCODING)"=="UNICODE"
OUTDIR=.\Unicode-Release
OUTDIRBIN=.\Unicode-Release
INTDIR=.\Unicode-Release
!ELSE
OUTDIR=.\ANSI-Release
OUTDIRBIN=.\ANSI-Release
INTDIR=.\ANSI-Release
!ENDIF

ALL : "$(OUTDIRBIN)\$(DLLNAME).dll"


CLEAN :
	-@erase "$(INTDIR)\bind.obj"
	-@erase "$(INTDIR)\columninfo.obj"
	-@erase "$(INTDIR)\connection.obj"
	-@erase "$(INTDIR)\convert.obj"
	-@erase "$(INTDIR)\dlg_specific.obj"
	-@erase "$(INTDIR)\dlg_wingui.obj"
	-@erase "$(INTDIR)\drvconn.obj"
	-@erase "$(INTDIR)\environ.obj"
	-@erase "$(INTDIR)\execute.obj"
	-@erase "$(INTDIR)\info.obj"
	-@erase "$(INTDIR)\info30.obj"
	-@erase "$(INTDIR)\win_md5.obj"
	-@erase "$(INTDIR)\misc.obj"
	-@erase "$(INTDIR)\pgapi30.obj"
	-@erase "$(INTDIR)\multibyte.obj"
!IF "$(ENCODING)"=="UNICODE"
	-@erase "$(INTDIR)\odbcapiw.obj"
	-@erase "$(INTDIR)\odbcapi30w.obj"
	-@erase "$(INTDIR)\win_unicode.obj"
!ENDIF
	-@erase "$(INTDIR)\options.obj"
	-@erase "$(INTDIR)\parse.obj"
	-@erase "$(INTDIR)\pgtypes.obj"
	-@erase "$(INTDIR)\psqlodbc.obj"
	-@erase "$(INTDIR)\$(DLLNAME).res"
	-@erase "$(INTDIR)\qresult.obj"
	-@erase "$(INTDIR)\results.obj"
	-@erase "$(INTDIR)\setup.obj"
	-@erase "$(INTDIR)\statement.obj"
	-@erase "$(INTDIR)\tuple.obj"
	-@erase "$(INTDIR)\tuplelist.obj"
	-@erase "$(INTDIR)\odbcapi.obj"
	-@erase "$(INTDIR)\odbcapi30.obj"
	-@erase "$(INTDIR)\descriptor.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\$(DLLNAME).dll"
	-@erase "$(OUTDIR)\$(DLLNAME).exp"
	-@erase "$(OUTDIR)\$(DLLNAME).lib"
	-@erase "$(OUTDIR)\$(DLLNAME).pch"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe

!IF "$(ENCODING)"=="UNICODE"
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "$(PG_INC)" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D "UNICODE_SUPPORT"  /D "ODBCVER=0x0300" /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" $(ADD_DEFINES) /Fp"$(INTDIR)\$(DLLNAME).pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
!ELSE
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "$(PG_INC)" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS"  /D "ODBCVER=0x0300" /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" $(ADD_DEFINES) /Fp"$(INTDIR)\$(DLLNAME).pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
!ENDIF

#!ENDIF

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\$(DLLNAME).res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\$(DLLNAME).bsc" 
BSC32_SBRS= \
	
LINK32=link.exe

LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libpq.lib  /nologo /dll /incremental:no /pdb:"$(OUTDIR)\$(DLLNAME).pdb" /machine:I386 /def:"$(DLLNAME).def" /out:"$(OUTDIRBIN)\$(DLLNAME).dll" /implib:"$(OUTDIR)\$(DLLNAME).lib" /libpath:"$(PG_LIB)"

DEF_FILE= "$(DLLNAME).def"
LINK32_OBJS= \
	"$(INTDIR)\bind.obj" \
	"$(INTDIR)\columninfo.obj" \
	"$(INTDIR)\connection.obj" \
	"$(INTDIR)\convert.obj" \
	"$(INTDIR)\dlg_specific.obj" \
	"$(INTDIR)\dlg_wingui.obj" \
	"$(INTDIR)\drvconn.obj" \
	"$(INTDIR)\environ.obj" \
	"$(INTDIR)\execute.obj" \
	"$(INTDIR)\info.obj" \
	"$(INTDIR)\info30.obj" \
	"$(INTDIR)\win_md5.obj" \
	"$(INTDIR)\misc.obj" \
	"$(INTDIR)\pgapi30.obj" \
	"$(INTDIR)\multibyte.obj" \
!IF "$(ENCODING)"=="UNICODE"
	"$(INTDIR)\odbcapiw.obj" \
	"$(INTDIR)\odbcapi30w.obj" \
	"$(INTDIR)\win_unicode.obj" \
!ENDIF
	"$(INTDIR)\options.obj" \
	"$(INTDIR)\parse.obj" \
	"$(INTDIR)\pgtypes.obj" \
	"$(INTDIR)\psqlodbc.obj" \
	"$(INTDIR)\qresult.obj" \
	"$(INTDIR)\results.obj" \
	"$(INTDIR)\setup.obj" \
	"$(INTDIR)\statement.obj" \
	"$(INTDIR)\tuple.obj" \
	"$(INTDIR)\tuplelist.obj" \
	"$(INTDIR)\odbcapi.obj" \
	"$(INTDIR)\odbcapi30.obj" \
	"$(INTDIR)\descriptor.obj" \
	"$(INTDIR)\$(DLLNAME).res"

"$(OUTDIRBIN)\$(DLLNAME).dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Debug"

!IF "$(ENCODING)"=="UNICODE"
OUTDIR=.\Unicode-Debug
OUTDIRBIN=.\Unicode-Debug
INTDIR=.\Unicode-Debug
!ELSE
OUTDIR=.\ANSI-Debug
OUTDIRBIN=.\ANSI-Debug
INTDIR=.\ANSI-Debug
!ENDIF

ALL : "$(OUTDIR)\$(DLLNAME).dll"


CLEAN :
	-@erase "$(INTDIR)\bind.obj"
	-@erase "$(INTDIR)\columninfo.obj"
	-@erase "$(INTDIR)\connection.obj"
	-@erase "$(INTDIR)\convert.obj"
	-@erase "$(INTDIR)\dlg_specific.obj"
	-@erase "$(INTDIR)\dlg_wingui.obj"
	-@erase "$(INTDIR)\drvconn.obj"
	-@erase "$(INTDIR)\environ.obj"
	-@erase "$(INTDIR)\execute.obj"
	-@erase "$(INTDIR)\info.obj"
	-@erase "$(INTDIR)\info30.obj"
	-@erase "$(INTDIR)\win_md5.obj"
	-@erase "$(INTDIR)\misc.obj"
	-@erase "$(INTDIR)\pgapi30.obj"
	-@erase "$(INTDIR)\multibyte.obj"
!IF "$(ENCODING)"=="UNICODE"
	-@erase "$(INTDIR)\odbcapiw.obj"
	-@erase "$(INTDIR)\odbcapi30w.obj"
	-@erase "$(INTDIR)\win_unicode.obj"
!ENDIF
	-@erase "$(INTDIR)\options.obj"
	-@erase "$(INTDIR)\parse.obj"
	-@erase "$(INTDIR)\pgtypes.obj"
	-@erase "$(INTDIR)\psqlodbc.obj"
	-@erase "$(INTDIR)\$(DLLNAME).res"
	-@erase "$(INTDIR)\qresult.obj"
	-@erase "$(INTDIR)\results.obj"
	-@erase "$(INTDIR)\setup.obj"
	-@erase "$(INTDIR)\statement.obj"
	-@erase "$(INTDIR)\tuple.obj"
	-@erase "$(INTDIR)\tuplelist.obj"
	-@erase "$(INTDIR)\odbcapi.obj"
	-@erase "$(INTDIR)\odbcapi30.obj"
	-@erase "$(INTDIR)\descriptor.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\$(DLLNAME).dll"
	-@erase "$(OUTDIR)\$(DLLNAME).exp"
	-@erase "$(OUTDIR)\$(DLLNAME).ilk"
	-@erase "$(OUTDIR)\$(DLLNAME).lib"
	-@erase "$(OUTDIR)\$(DLLNAME).pdb"
	-@erase "$(OUTDIR)\$(DLLNAME).pch"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe

!IF "$(ENCODING)"=="UNICODE"
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od  /I "$(PG_INC)" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D "UNICODE_SUPPORT"  /D "ODBCVER=0x0300" /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" $(ADD_DEFINES) /Fp"$(INTDIR)\$(DLLNAME).pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
!ELSE
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od  /I "$(PG_INC)" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS"  /D "ODBCVER=0x0300" /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" $(ADD_DEFINES) /Fp"$(INTDIR)\$(DLLNAME).pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
!ENDIF


.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\$(DLLNAME).res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\$(DLLNAME).bsc" 
BSC32_SBRS= \
	
LINK32=link.exe

LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libpq.lib  /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\psqlodbclibpq.pdb" /debug /machine:I386 /def:"$(DLLNAME).def" /out:"$(OUTDIR)\$(DLLNAME).dll" /implib:"$(OUTDIR)\$(DLLNAME).lib" /pdbtype:sept /libpath:"$(PG_LIB)"

DEF_FILE= "$(DLLNAME).def"
LINK32_OBJS= \
	"$(INTDIR)\bind.obj" \
	"$(INTDIR)\columninfo.obj" \
	"$(INTDIR)\connection.obj" \
	"$(INTDIR)\convert.obj" \
	"$(INTDIR)\dlg_specific.obj" \
	"$(INTDIR)\dlg_wingui.obj" \
	"$(INTDIR)\drvconn.obj" \
	"$(INTDIR)\environ.obj" \
	"$(INTDIR)\execute.obj" \
	"$(INTDIR)\info.obj" \
	"$(INTDIR)\info30.obj" \
	"$(INTDIR)\win_md5.obj" \
	"$(INTDIR)\misc.obj" \
	"$(INTDIR)\pgapi30.obj" \
	"$(INTDIR)\multibyte.obj" \
!IF "$(ENCODING)"=="UNICODE"
	"$(INTDIR)\odbcapiw.obj" \
	"$(INTDIR)\odbcapi30w.obj" \
	"$(INTDIR)\win_unicode.obj" \
!ENDIF
	"$(INTDIR)\options.obj" \
	"$(INTDIR)\parse.obj" \
	"$(INTDIR)\pgtypes.obj" \
	"$(INTDIR)\psqlodbc.obj" \
	"$(INTDIR)\qresult.obj" \
	"$(INTDIR)\results.obj" \
	"$(INTDIR)\setup.obj" \
	"$(INTDIR)\statement.obj" \
	"$(INTDIR)\tuple.obj" \
	"$(INTDIR)\tuplelist.obj" \
	"$(INTDIR)\odbcapi.obj" \
	"$(INTDIR)\odbcapi30.obj" \
	"$(INTDIR)\descriptor.obj" \
	"$(INTDIR)\$(DLLNAME).res"

"$(OUTDIR)\$(DLLNAME).dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

!IF "$(CFG)" == "Release" || "$(CFG)" == "Debug"

SOURCE=bind.c

"$(INTDIR)\bind.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=columninfo.c

"$(INTDIR)\columninfo.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=connection.c

"$(INTDIR)\connection.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=convert.c

"$(INTDIR)\convert.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=dlg_specific.c

"$(INTDIR)\dlg_specific.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=dlg_wingui.c

"$(INTDIR)\dlg_wingui.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=drvconn.c

"$(INTDIR)\drvconn.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=environ.c

"$(INTDIR)\environ.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=execute.c

"$(INTDIR)\execute.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=info.c

"$(INTDIR)\info.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=info30.c

"$(INTDIR)\info30.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



SOURCE=misc.c

"$(INTDIR)\misc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=multibyte.c

"$(INTDIR)\multibyte.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=pgapi30.c

"$(INTDIR)\pgapi30.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

!IF "$(ENCODING)"=="UNICODE"

SOURCE=odbcapiw.c

"$(INTDIR)\odbcapiw.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=odbcapi30w.c

"$(INTDIR)\odbcapi30w.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=win_unicode.c

"$(INTDIR)\win_unicode.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

!ENDIF

SOURCE=options.c

"$(INTDIR)\options.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=parse.c

"$(INTDIR)\parse.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=pgtypes.c

"$(INTDIR)\pgtypes.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=psqlodbc.c

"$(INTDIR)\psqlodbc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=psqlodbc.rc

!IF "$(ENCODING)"=="UNICODE"

!IF "$(CFG)" == "Release"
"$(INTDIR)\$(DLLNAME).res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x809 /fo"$(INTDIR)\$(DLLNAME).res" /d "NDEBUG" /d "MULTIBYTE"  /d "UNICODE_SUPPORT" $(SOURCE)
!ENDIF

!IF "$(CFG)" == "Debug"
"$(INTDIR)\$(DLLNAME).res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x809 /fo"$(INTDIR)\$(DLLNAME).res" /d "_DEBUG" /d "MULTIBYTE"  /d "UNICODE_SUPPORT" $(SOURCE)
!ENDIF

!ELSE


!IF "$(CFG)" == "Release"
"$(INTDIR)\$(DLLNAME).res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x809 /fo"$(INTDIR)\$(DLLNAME).res" /d "NDEBUG" /d "MULTIBYTE"  $(SOURCE)
!ENDIF

!IF "$(CFG)" == "Debug"
"$(INTDIR)\$(DLLNAME).res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x809 /fo"$(INTDIR)\$(DLLNAME).res" /d "_DEBUG" /d "MULTIBYTE"  $(SOURCE)
!ENDIF

!ENDIF


SOURCE=qresult.c

"$(INTDIR)\qresult.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=results.c

"$(INTDIR)\results.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=setup.c

"$(INTDIR)\setup.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=statement.c

"$(INTDIR)\statement.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=tuple.c

"$(INTDIR)\tuple.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=tuplelist.c

"$(INTDIR)\tuplelist.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=win_md5.c

"$(INTDIR)\win_md5.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=odbcapi.c

"$(INTDIR)\odbcapi.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=odbcapi30.c

"$(INTDIR)\odbcapi30.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=descriptor.c

"$(INTDIR)\descriptor.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 
