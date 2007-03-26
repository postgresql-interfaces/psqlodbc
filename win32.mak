#
# File:			win32.mak
#
# Description:		psqlodbc35w Unicode version Makefile for Win32.
#
# Configurations:	Debug, Release
# Build Types:		ALL, CLEAN
# Usage:		NMAKE /f win32.mak CFG=[Release | Debug] [ALL | CLEAN]
#
# Comments:		Created by Dave Page, 2001-02-12
#

!IF "$(ANSI_VERSION)" == "yes"
!MESSAGE Building the PostgreSQL ANSI 3.0 Driver for Win32...
!ELSE
!MESSAGE Building the PostgreSQL Unicode 3.5 Driver for Win32...
!ENDIF
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
!MESSAGE NMAKE /f win32.mak CFG=[Release | Debug] [ALL | CLEAN]
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
PG_INC=$(PROGRAMFILES)\PostgreSQL\8.2\include
!MESSAGE Using default PostgreSQL Include directory: $(PG_INC)
!ENDIF

!IF "$(PG_LIB)" == ""
PG_LIB=$(PROGRAMFILES)\PostgreSQL\8.2\lib\ms
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

!IF "$(LINKMT)" == ""
LINKMT=MT
!ENDIF
!IF "$(LINKMT)" == "MT"
!MESSAGE Linking static Multithread library
!ELSE
!MESSAGE Linking dynamic Multithread library
!ENDIF

SSL_DLL = "SSLEAY32.dll"
ADD_DEFINES = $(ADD_DEFINES) /D "SSL_DLL=\"$(SSL_DLL)\""

!IF "$(_NMAKE_VER)" == "6.00.9782.0"
MSVC_VERSION=vc60
VC07_DELAY_LOAD=
MSDTC=no
VC_FLAGS=/GX /YX
!ELSE
MSVC_VERSION=vc70
VC07_DELAY_LOAD="/DelayLoad:libpq.dll /DelayLoad:$(SSL_DLL) /DelayLoad:XOLEHLP.dll /DELAY:UNLOAD"
VC_FLAGS=/EHsc
!ENDIF
ADD_DEFINES = $(ADD_DEFINES) /D "DYNAMIC_LOAD"

!IF "$(MSDTC)" != "no"
ADD_DEFINES = $(ADD_DEFINES) /D "_HANDLE_ENLIST_IN_DTC_"
!ENDIF
!IF "$(MEMORY_DEBUG)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /D "_MEMORY_DEBUG_" /GS
!ENDIF
!IF "$(ANSI_VERSION)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /D "DBMS_NAME=\"PostgreSQL ANSI\"" /D "ODBCVER=0x0300"
!ELSE
ADD_DEFINES = $(ADD_DEFINES) /D "UNICODE_SUPPORT" /D "ODBCVER=0x0351"
RSC_DEFINES = $(RSC_DEFINES) /D "UNICODE_SUPPORT"
!ENDIF
!IF "$(PORTCHECK_64BIT)" == "yes"
# ADD_DEFINES = $(ADD_DEFINES) /Wp64
ADD_DEFINES = $(ADD_DEFINES) /D _WIN64
!ENDIF

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF

!IF "$(ANSI_VERSION)" == "yes"
MAINLIB = psqlodbc30a
!ELSE
MAINLIB = psqlodbc35w
!ENDIF
MAINDLL = $(MAINLIB).dll 
XALIB = pgxalib 
XADLL = $(XALIB).dll 

!IF  "$(CFG)" == "Release"
!IF  "$(ANSI_VERSION)" == "yes"
OUTDIR=.\MultibyteRelease
OUTDIRBIN=.\MultibyteRelease
INTDIR=.\MultibyteRelease
!ELSE
OUTDIR=.\Release
OUTDIRBIN=.\Release
INTDIR=.\Release
!ENDIF
!ELSEIF  "$(CFG)" == "Debug"
!IF  "$(ANSI_VERSION)" == "yes"
OUTDIR=.\MultibyteDebug
OUTDIRBIN=.\MultibyteDebug
INTDIR=.\MultibyteDebug
!ELSE
OUTDIR=.\Debug
OUTDIRBIN=.\Debug
INTDIR=.\Debug
!ENDIF
!ENDIF

ALLDLL  = "$(OUTDIR)\$(MAINDLL)"

!IF  "$(MSDTC)" != "no"
ALLDLL = $(ALLDLL) "$(OUTDIR)\$(XADLL)"
!ENDIF

ALL : $(ALLDLL)

CLEAN :
	-@erase "$(INTDIR)\*.obj"
	-@erase "$(INTDIR)\*.res"
	-@erase "$(INTDIR)\*.lib"
	-@erase "$(INTDIR)\*.exp"
	-@erase "$(INTDIR)\*.pch"
	-@erase "$(OUTDIR)\$(MAINDLL)"
!IF "$(MSDTC)" != "no"
	-@erase "$(OUTDIR)\$(XADLL)"
!ENDIF

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

$(INTDIR)\connection.obj $(INTDIR)\psqlodbc.res: version.h

CPP=cl.exe
!IF  "$(CFG)" == "Release"
CPP_PROJ=/nologo /$(LINKMT) /W3 $(VC_FLAGS) /O2 /I "$(PG_INC)" /I "$(SSL_INC)" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_CRT_SECURE_NO_DEPRECATE" /D "PSQLODBC_EXPORTS" /D "WIN_MULTITHREAD_SUPPORT" $(ADD_DEFINES) /Fp"$(INTDIR)\psqlodbc.pch" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
!ELSEIF  "$(CFG)" == "Debug"
CPP_PROJ=/nologo /$(LINKMT)d /W3 /Gm $(VC_FLAGS) /ZI /Od /I "$(PG_INC)" /I "$(SSL_INC)" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_CRT_SECURE_NO_DEPRECATE" /D "PSQLODBC_EXPORTS" /D "WIN_MULTITHREAD_SUPPORT" $(ADD_DEFINES) /Fp"$(INTDIR)\psqlodbc.pch" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c
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
RSC=rc.exe
BSC32=bscmake.exe
!IF  "$(CFG)" == "Release"
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\psqlodbc.res" /d "NDEBUG" 
BSC32_FLAGS=/nologo /o"$(OUTDIR)\psqlodbc.bsc" 
!ELSE
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\psqlodbc.res" /d "_DEBUG" 
BSC32_FLAGS=/nologo /o"$(OUTDIR)\psqlodbc.bsc" 
!ENDIF
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib XOleHlp.lib winmm.lib /nologo /dll
!IF  "$(ANSI_VERSION)" == "yes"
DEF_FILE= "psqlodbca.def"
!ELSE
DEF_FILE= "psqlodbc.def"
!ENDIF
!IF  "$(CFG)" == "Release"
LINK32_FLAGS=$(LINK32_FLAGS) /incremental:no /pdb:"$(OUTDIR)\psqlodbc.pdb" /machine:I386 /def:"$(DEF_FILE)" /out:"$(OUTDIRBIN)\$(MAINDLL)" /implib:"$(OUTDIR)\psqlodbc.lib"
!ELSE
LINK32_FLAGS=$(LINK32_FLAGS) /incremental:yes /pdb:"$(OUTDIR)\psqlodbc.pdb" /debug /machine:I386 /def:"$(DEF_FILE)" /out:"$(OUTDIR)\$(MAINDLL)" /implib:"$(OUTDIR)\psqlodbc.lib" /pdbtype:sept
!ENDIF
LINK32_FLAGS=$(LINK32_FLAGS) "$(VC07_DELAY_LOAD)" /libpath:"$(PG_LIB)" /libpath:"$(SSL_LIB)"

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
!IF "$(ANSI_VERSION)" != "yes"
	"$(INTDIR)\win_unicode.obj" \
	"$(INTDIR)\odbcapiw.obj" \
	"$(INTDIR)\odbcapi30w.obj" \
!ENDIF
!IF "$(MSDTC)" != "no"
	"$(INTDIR)\msdtc_enlist.obj" \
!ENDIF
!IF "$(MEMORY_DEBUG)" == "yes"
	"$(INTDIR)\inouealc.obj" \
!ENDIF
	"$(INTDIR)\psqlodbc.res"

XADEF_FILE= "$(XALIB).def"
LINK32_XAFLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib odbc32.lib odbccp32.lib uuid.lib wsock32.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\$(XALIB).pdb" /machine:I386 /def:"$(XADEF_FILE)" /out:"$(OUTDIRBIN)\$(XADLL)" /implib:"$(OUTDIR)\$(XALIB).lib"
LINK32_XAOBJS= \
	"$(INTDIR)\pgxalib.obj" 

"$(OUTDIRBIN)\$(MAINDLL)" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

"$(OUTDIRBIN)\$(XADLL)" : "$(OUTDIR)" $(XADEF_FILE) $(LINK32_XAOBJS)
    $(LINK32) @<<
  $(LINK32_XAFLAGS) $(LINK32_XAOBJS)
<<


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
	$(RSC) /l 0x809 /fo"$(INTDIR)\psqlodbc.res" /d "NDEBUG" /d "MULTIBYTE" $(RSC_DEFINES) $(SOURCE)
!ENDIF

!IF "$(CFG)" == "Debug"
"$(INTDIR)\psqlodbc.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x809 /fo"$(INTDIR)\psqlodbc.res" /d "_DEBUG" $(RSC_DEFINES) $(SOURCE)
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

!IF "$(MSDTC)" != "no"
SOURCE=msdtc_enlist.cpp

"$(INTDIR)\msdtc_enlist.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=pgxalib.cpp

"$(INTDIR)\pgxalib.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)
!ENDIF

!IF "$(MEMORY_DEBUG)" == "yes"
SOURCE=inouealc.c

"$(INTDIR)\inouealc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)
!ENDIF


!ENDIF 
