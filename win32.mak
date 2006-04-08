#
# File:			win32_30w.mak
#
# Description:		psqlodbc35w Unicode version Makefile for Win32.
#
# Configurations:	Release, Debug
# Build Types:		ALL, CLEAN
# Usage:		NMAKE /f win32_30.mak CFG=[Release | Debug] [ALL | CLEAN]
#
# Comments:		Created by Dave Page, 2001-02-12
#

!MESSAGE Building the PostgreSQL Unicode 3.51 Driver for Win32...
!MESSAGE
!IF "$(CFG)" == ""
CFG=Release
!MESSAGE No configuration specified. Defaulting to Release.
!MESSAGE
!ENDIF 

!IF "$(CFG)" != "Release" && "$(CFG)" != "Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f win32_30.mak CFG=[Release | Debug] [ALL | CLEAN]
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Release" (Win32 Release DLL)
!MESSAGE "Debug" (Win32 Debug DLL)
!MESSAGE 
!ERROR An invalid configuration was specified.
!ENDIF 

#
#
!IF "$(PG_INC)" == ""
PG_INC=$(PROGRAMFILES)\PostgreSQL\8.1\include
!MESSAGE Using default PostgreSQL Include directory: $(PG_INC)
!ENDIF

!IF "$(PG_LIB)" == ""
PG_LIB=$(PROGRAMFILES)\PostgreSQL\8.1\lib\ms
!MESSAGE Using default PostgreSQL Library directory: $(PG_LIB)
!ENDIF

!IF "$(SSL_INC)" == ""
SSL_INC=C:\OpenSSL\include
!MESSAGE Using default OpenSSL Include directory: $(SSL_INC)
!ENDIF

!IF "$(SSL_LIB)" == ""
SSL_LIB=C:\OpenSSL\lib\VC
!MESSAGE Using default OpenSSL Library directory: $(SSL_LIB)
!ENDIF

SSL_DLL = "SSLEAY32.dll"
ADD_DEFINES = $(ADD_DEFINES) /D "SSL_DLL=\"$(SSL_DLL)\""

!IF "$(_NMAKE_VER)" == "6.00.9782.0"
MSVC_VERSION=vc60
VC07_DELAY_LOAD=
ADD_DEFINES = $(ADD_DEFINES) /D "DYNAMIC_LOAD"
!ELSE
MSVC_VERSION=vc70
VC07_DELAY_LOAD="/DelayLoad:libpq.dll /DelayLoad:$(SSL_DLL) /DELAY:UNLOAD"
ADD_DEFINES = $(ADD_DEFINES) /D "DYNAMIC_LOAD"
!ENDIF

!IF "$(MEMORY_DEBUG)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /D "_MEMORY_DEBUG_"
!ENDIF

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "Release"

OUTDIR=.\Release
OUTDIRBIN=.\Release
INTDIR=.\Release

ALL : "$(OUTDIRBIN)\psqlodbc35w.dll"


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
	-@erase "$(INTDIR)\lobj.obj"
	-@erase "$(INTDIR)\win_md5.obj"
	-@erase "$(INTDIR)\misc.obj"
	-@erase "$(INTDIR)\mylog.obj"
	-@erase "$(INTDIR)\pgapi30.obj"
	-@erase "$(INTDIR)\multibyte.obj"
	-@erase "$(INTDIR)\odbcapiw.obj"
	-@erase "$(INTDIR)\odbcapi30w.obj"
	-@erase "$(INTDIR)\win_unicode.obj"
	-@erase "$(INTDIR)\options.obj"
	-@erase "$(INTDIR)\parse.obj"
	-@erase "$(INTDIR)\pgtypes.obj"
	-@erase "$(INTDIR)\psqlodbc.obj"
	-@erase "$(INTDIR)\psqlodbc.res"
	-@erase "$(INTDIR)\qresult.obj"
	-@erase "$(INTDIR)\results.obj"
	-@erase "$(INTDIR)\setup.obj"
	-@erase "$(INTDIR)\socket.obj"
	-@erase "$(INTDIR)\statement.obj"
	-@erase "$(INTDIR)\tuple.obj"
	-@erase "$(INTDIR)\odbcapi.obj"
	-@erase "$(INTDIR)\odbcapi30.obj"
	-@erase "$(INTDIR)\descriptor.obj"
	-@erase "$(INTDIR)\loadlib.obj"
	-@erase "$(INTDIR)\$(MSVC_VERSION).idb"
	-@erase "$(OUTDIR)\psqlodbc35w.dll"
	-@erase "$(OUTDIR)\psqlodbc.exp"
	-@erase "$(OUTDIR)\psqlodbc.lib"
	-@erase "$(OUTDIR)\psqlodbc.pch"
!IF "$(MEMORY_DEBUG)" == "yes"
	-@erase "$(OUTDIR)\inouealc.obj"
!ENDIF

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "$(PG_INC)" /I "$(SSL_INC)" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D "ODBCVER=0x0351" /D "UNICODE_SUPPORT" /D "WIN_MULTITHREAD_SUPPORT" $(ADD_DEFINES) /Fp"$(INTDIR)\psqlodbc.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\psqlodbc.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\psqlodbc.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
DEF_FILE= "psqlodbc.def"
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\psqlodbc.pdb" /machine:I386 /def:"$(DEF_FILE)" /out:"$(OUTDIRBIN)\psqlodbc35w.dll" /implib:"$(OUTDIR)\psqlodbc.lib" "$(VC07_DELAY_LOAD)" /libpath:"$(PG_LIB)" /libpath:"$(SSL_LIB)"
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
	"$(INTDIR)\lobj.obj" \
	"$(INTDIR)\win_md5.obj" \
	"$(INTDIR)\misc.obj" \
	"$(INTDIR)\mylog.obj" \
	"$(INTDIR)\pgapi30.obj" \
	"$(INTDIR)\multibyte.obj" \
	"$(INTDIR)\odbcapiw.obj" \
	"$(INTDIR)\odbcapi30w.obj" \
	"$(INTDIR)\win_unicode.obj" \
	"$(INTDIR)\options.obj" \
	"$(INTDIR)\parse.obj" \
	"$(INTDIR)\pgtypes.obj" \
	"$(INTDIR)\psqlodbc.obj" \
	"$(INTDIR)\qresult.obj" \
	"$(INTDIR)\results.obj" \
	"$(INTDIR)\setup.obj" \
	"$(INTDIR)\socket.obj" \
	"$(INTDIR)\statement.obj" \
	"$(INTDIR)\tuple.obj" \
	"$(INTDIR)\odbcapi.obj" \
	"$(INTDIR)\odbcapi30.obj" \
	"$(INTDIR)\descriptor.obj" \
	"$(INTDIR)\loadlib.obj" \
!IF "$(MEMORY_DEBUG)" == "yes"
	"$(INTDIR)\inouealc.obj" \
!ENDIF
	"$(INTDIR)\psqlodbc.res"

"$(OUTDIRBIN)\psqlodbc35w.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Debug"

OUTDIR=.\Debug
OUTDIRBIN=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\psqlodbc35w.dll"


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
	-@erase "$(INTDIR)\lobj.obj"
	-@erase "$(INTDIR)\win_md5.obj"
	-@erase "$(INTDIR)\misc.obj"
	-@erase "$(INTDIR)\mylog.obj"
	-@erase "$(INTDIR)\pgapi30.obj"
	-@erase "$(INTDIR)\multibyte.obj"
	-@erase "$(INTDIR)\odbcapiw.obj"
	-@erase "$(INTDIR)\odbcapi30w.obj"
	-@erase "$(INTDIR)\win_unicode.obj"
	-@erase "$(INTDIR)\options.obj"
	-@erase "$(INTDIR)\parse.obj"
	-@erase "$(INTDIR)\pgtypes.obj"
	-@erase "$(INTDIR)\psqlodbc.obj"
	-@erase "$(INTDIR)\psqlodbc.res"
	-@erase "$(INTDIR)\qresult.obj"
	-@erase "$(INTDIR)\results.obj"
	-@erase "$(INTDIR)\setup.obj"
	-@erase "$(INTDIR)\socket.obj"
	-@erase "$(INTDIR)\statement.obj"
	-@erase "$(INTDIR)\tuple.obj"
	-@erase "$(INTDIR)\odbcapi.obj"
	-@erase "$(INTDIR)\odbcapi30.obj"
	-@erase "$(INTDIR)\descriptor.obj"
	-@erase "$(INTDIR)\loadlib.obj"
	-@erase "$(INTDIR)\$(MSVC_VERSION).idb"
	-@erase "$(INTDIR)\$(MSVC_VERSION).pdb"
	-@erase "$(OUTDIR)\psqlodbc35w.dll"
	-@erase "$(OUTDIR)\psqlodbc35w.ilk"
	-@erase "$(OUTDIR)\psqlodbc.exp"
	-@erase "$(OUTDIR)\psqlodbc.lib"
	-@erase "$(OUTDIR)\psqlodbc.pdb"
	-@erase "$(OUTDIR)\psqlodbc.pch"
!IF "$(MEMORY_DEBUG)" == "yes"
	-@erase "$(OUTDIR)\inouealc.obj"
!ENDIF

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "$(PG_INC)" /I "$(SSL_INC)" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D "ODBCVER=0x0351" /D "UNICODE_SUPPORT" /D "WIN_MULTITHREAD_SUPPORT" $(ADD_DEFINES) /Fp"$(INTDIR)\psqlodbc.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\psqlodbc.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\psqlodbc.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
DEF_FILE= "psqlodbc.def"
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\psqlodbc.pdb" /debug /machine:I386 /def:"$(DEF_FILE)" /out:"$(OUTDIR)\psqlodbc35w.dll" /implib:"$(OUTDIR)\psqlodbc.lib" /pdbtype:sept "$(VC07_DELAY_LOAD)" /libpath:"$(PG_LIB)" /libpath:"$(SSL_LIB)"
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
	"$(INTDIR)\lobj.obj" \
	"$(INTDIR)\win_md5.obj" \
	"$(INTDIR)\misc.obj" \
	"$(INTDIR)\mylog.obj" \
	"$(INTDIR)\pgapi30.obj" \
	"$(INTDIR)\multibyte.obj" \
	"$(INTDIR)\odbcapiw.obj" \
	"$(INTDIR)\odbcapi30w.obj" \
	"$(INTDIR)\win_unicode.obj" \
	"$(INTDIR)\options.obj" \
	"$(INTDIR)\parse.obj" \
	"$(INTDIR)\pgtypes.obj" \
	"$(INTDIR)\psqlodbc.obj" \
	"$(INTDIR)\qresult.obj" \
	"$(INTDIR)\results.obj" \
	"$(INTDIR)\setup.obj" \
	"$(INTDIR)\socket.obj" \
	"$(INTDIR)\statement.obj" \
	"$(INTDIR)\tuple.obj" \
	"$(INTDIR)\odbcapi.obj" \
	"$(INTDIR)\odbcapi30.obj" \
	"$(INTDIR)\descriptor.obj" \
	"$(INTDIR)\loadlib.obj" \
!IF "$(MEMORY_DEBUG)" == "yes"
	"$(INTDIR)\inouealc.obj" \
!ENDIF
	"$(INTDIR)\psqlodbc.res"

"$(OUTDIR)\psqlodbc35w.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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


SOURCE=lobj.c

"$(INTDIR)\lobj.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=misc.c

"$(INTDIR)\misc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=mylog.c

"$(INTDIR)\mylog.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=multibyte.c

"$(INTDIR)\multibyte.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=odbcapiw.c

"$(INTDIR)\odbcapiw.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=pgapi30.c

"$(INTDIR)\pgapi30.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=odbcapi30w.c

"$(INTDIR)\odbcapi30w.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=win_unicode.c

"$(INTDIR)\win_unicode.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


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

!IF "$(CFG)" == "Release"
"$(INTDIR)\psqlodbc.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x809 /fo"$(INTDIR)\psqlodbc.res" /d "NDEBUG" /d "MULTIBYTE" $(SOURCE)
!ENDIF

!IF "$(CFG)" == "Debug"
"$(INTDIR)\psqlodbc.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x809 /fo"$(INTDIR)\psqlodbc.res" /d "_DEBUG" $(SOURCE)
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


SOURCE=socket.c

"$(INTDIR)\socket.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=statement.c

"$(INTDIR)\statement.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=tuple.c

"$(INTDIR)\tuple.obj" : $(SOURCE) "$(INTDIR)"
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

SOURCE=loadlib.c

"$(INTDIR)\loadlib.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

!IF "$(MEMORY_DEBUG)" == "yes"
SOURCE=inouealc.c

"$(INTDIR)\inouealc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)
!ENDIF


!ENDIF 
