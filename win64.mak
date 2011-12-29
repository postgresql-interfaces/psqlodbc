#
# File:			win64.mak
#
# Description:		psqlodbc35w Unicode 64bit version Makefile.
#			(can be built using platform SDK's buildfarm) 
#
# Configurations:	Debug, Release
# Build Types:		ALL, CLEAN
# Usage:		NMAKE /f win64.mak CFG=[Release | Debug] [ALL | CLEAN]
#
# Comments:		Created by Hiroshi Inoue, 2006-10-31
#

!IF "$(CPU)" == ""
!MESSAGE Making 64bit DLL...
!MESSAGE You should set the CPU environemt variable
!MESSAGE to distinguish your OS
!ENDIF

!IF "$(ANSI_VERSION)" == "yes"
!MESSAGE Building the PostgreSQL ANSI 3.0 Driver for $(CPU)...
!ELSE
!MESSAGE Building the PostgreSQL Unicode 3.5 Driver for $(CPU)...
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
!MESSAGE NMAKE /f win64.mak CFG=[Release | Debug] [ALL | CLEAN]
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Release" ($(CPU) Release DLL)
!MESSAGE "Debug" ($(CPU) Debug DLL)
!MESSAGE 
!ERROR An invalid configuration was specified.
!ENDIF 

ADD_DEFINES=/D _WIN64
#
#	Include libraries as well as import libraries
#	may be different from those of 32bit ones.
#	Please set PG_INC, PG_LIB, SSL_INC or PG_LIB
#	variables to appropriate ones. 
#
!IF "$(PG_INC)" == ""
PG_INC=$(PROGRAMFILES)\PostgreSQL\9.1\include
!MESSAGE Using default PostgreSQL Include directory: $(PG_INC)
!ENDIF

!IF "$(PG_LIB)" == ""
PG_LIB=C:\develop\lib\$(CPU)
!MESSAGE Using default PostgreSQL Library directory: $(PG_LIB)
!ENDIF

!IF "$(USE_LIBPQ)" != "no"
!IF "$(SSL_INC)" == ""
SSL_INC=C:\OpenSSL\include
!MESSAGE Using default OpenSSL Include directory: $(SSL_INC)
!ENDIF

!IF "$(SSL_LIB)" == ""
SSL_LIB=C:\develop\lib\$(CPU)
!MESSAGE Using default OpenSSL Library directory: $(SSL_LIB)
!ENDIF

SSL_DLL = "SSLEAY32.dll"
RESET_CRYPTO = yes
ADD_DEFINES = $(ADD_DEFINES) /D "SSL_DLL=\"$(SSL_DLL)\"" /D USE_SSL
!ELSE
ADD_DEFINES = $(ADD_DEFINES) /D NOT_USE_LIBPQ
!ENDIF

!IF "$(USE_SSPI)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /D USE_SSPI
!ENDIF
!IF "$(USE_GSS)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /D USE_GSS
!ENDIF

!IF "$(ANSI_VERSION)" == "yes"
DTCLIB = pgenlista
!ELSE
DTCLIB = pgenlist
!ENDIF
DTCDLL = $(DTCLIB).dll
 
!IF "$(_NMAKE_VER)" == "6.00.9782.0"
VC07_DELAY_LOAD=
MSDTC=no
!ELSE
!IF "$(USE_LIBPQ)" != "no"
VC07_DELAY_LOAD=/DelayLoad:libpq.dll /DelayLoad:$(SSL_DLL)
!IF "$(RESET_CRYPTO)" == "yes"
VC07_DELAY_LOAD=$(VC07_DELAY_LOAD) /DelayLoad:libeay32.dll
ADD_DEFINES=$(ADD_DEFINES) /D RESET_CRYPTO_CALLBACKS
!ENDIF
!ENDIF
!IF "$(USE_SSPI)" == "yes"
VC07_DELAY_LOAD=$(VC07_DELAY_LOAD) /DelayLoad:secur32.dll /Delayload:crypt32.dll
!ENDIF
!IF "$(USE_GSS)" == "yes"
VC07_DELAY_LOAD=$(VC07_DELAY_LOAD) /Delayload:gssapi64.dll
!ENDIF
VC07_DELAY_LOAD="$(VC07_DELAY_LOAD) /DelayLoad:$(DTCDLL) /DELAY:UNLOAD"
!ENDIF
ADD_DEFINES = $(ADD_DEFINES) /D "DYNAMIC_LOAD"

!IF "$(MSDTC)" != "no"
ADD_DEFINES = $(ADD_DEFINES) /D "_HANDLE_ENLIST_IN_DTC_"
!ENDIF
!IF "$(MEMORY_DEBUG)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /D "_MEMORY_DEBUG_" /GS
!ENDIF
!IF "$(CPU)" == "AMD64"
CPUTYPE = x64
!ELSE
CPUTYPE = $(CPU)
!ENDIF
!IF "$(ANSI_VERSION)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /D "DBMS_NAME=\"PostgreSQL ANSI($(CPUTYPE))\"" /D "ODBCVER=0x0300"
!ELSE
ADD_DEFINES = $(ADD_DEFINES) /D "DBMS_NAME=\"PostgreSQL Unicode($(CPUTYPE))\"" /D "ODBCVER=0x0351" /D "UNICODE_SUPPORT"
RSC_DEFINES = $(RSC_DEFINES) /D "UNICODE_SUPPORT"
!ENDIF

!IF "$(PORT_CHECK)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /Wp64
!ENDIF

!IF "$(PG_INC)" != ""
INC_OPT = $(INC_OPT) /I "$(PG_INC)"
!ENDIF
!IF "$(SSL_INC)" != ""
INC_OPT = $(INC_OPT) /I "$(SSL_INC)"
!ENDIF
!IF "$(ADDL_INC)" != ""
INC_OPT = $(INC_OPT) /I "$(ADD_INC)"
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
OUTDIR=.\$(CPU)ANSI
OUTDIRBIN=.\$(CPU)ANSI
INTDIR=.\$(CPU)ANSI
!ELSE
OUTDIR=.\$(CPU)
OUTDIRBIN=.\$(CPU)
INTDIR=.\$(CPU)
!ENDIF
!ELSEIF  "$(CFG)" == "Debug"
!IF  "$(ANSI_VERSION)" == "yes"
OUTDIR=.\$(CPU)ANSIDebug
OUTDIRBIN=.\$(CPU)ANSIDebug
INTDIR=.\$(CPU)ANSIDebug
!ELSE
OUTDIR=.\$(CPU)Debug
OUTDIRBIN=.\$(CPU)Debug
INTDIR=.\$(CPU)Debug
!ENDIF
!ENDIF

ALLDLL  = "$(INTDIR)"
!IF "$(OUTDIR)" != "$(INTDIR)"
ALLDLL  = $(ALLDLL) "$(INTDIR)"
!ENDIF

ALLDLL  = $(ALLDLL) "$(OUTDIR)\$(MAINDLL)"

!IF  "$(MSDTC)" != "no"
ALLDLL = $(ALLDLL) "$(OUTDIR)\$(XADLL)" "$(OUTDIR)\$(DTCDLL)"
!ENDIF

ALL : $(ALLDLL)

CLEAN :
	-@erase "$(INTDIR)\*.obj"
	-@erase "$(INTDIR)\*.res"
	-@erase "$(OUTDIR)\*.lib"
	-@erase "$(OUTDIR)\*.exp"
	-@erase "$(INTDIR)\*.pch"
	-@erase "$(OUTDIR)\$(MAINDLL)"
!IF "$(MSDTC)" != "no"
	-@erase "$(OUTDIR)\$(DTCDLL)"
	-@erase "$(OUTDIR)\$(XADLL)"
!ENDIF

!IF  "$(MSDTC)" != "no"
"$(OUTDIR)\$(MAINDLL)": "$(OUTDIR)\$(DTCLIB).lib"
!ENDIF

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

!IF "$(OUTDIR)" != "$(INTDIR)"
"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"
!ENDIF

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /EHsc $(INC_OPT) /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_CRT_SECURE_NO_DEPRECATE" /D "PSQLODBC_EXPORTS" /D "WIN_MULTITHREAD_SUPPORT" $(ADD_DEFINES) /Fp"$(INTDIR)\psqlodbc.pch" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD 
!IF  "$(CFG)" == "Release"
CPP_PROJ=$(CPP_PROJ) /O2 /D "NDEBUG"
!ELSEIF  "$(CFG)" == "Debug"
CPP_PROJ=$(CPP_PROJ) /Gm /ZI /Od /D "_DEBUG" /GZ
!ENDIF

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) /c $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) /c $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) /c $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) /c $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) /c $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) /c $< 
<<

MTL=midl.exe
RSC=rc.exe
BSC32=bscmake.exe
MTL_PROJ=/nologo /mktyplib203 /win32 
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\psqlodbc.res" /d "MULTIBUTE" 
BSC32_FLAGS=/nologo /o"$(OUTDIR)\psqlodbc.bsc" 
!IF  "$(CFG)" == "Release"
MTL_PROJ=$(MTL_PROJ) /D "NDEBUG"
RSC_PROJ=$(RSC_PROJ) /d "NDEBUG" 
!ELSE
MTL_PROJ=$(MTL_PROJ) /D "_DEBUG" 
RSC_PROJ=$(RSC_PROJ) /d "_DEBUG" 
!ENDIF
BSC32_SBRS= \
	
LINK32=link.exe
LIB32=lib.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib advapi32.lib odbc32.lib odbccp32.lib wsock32.lib ws2_32.lib XOleHlp.lib winmm.lib "$(OUTDIR)\$(DTCLIB).lib" msvcrt.lib bufferoverflowu.lib /nologo /dll /machine:$(CPU) /def:"$(DEF_FILE)"
!IF  "$(ANSI_VERSION)" == "yes"
DEF_FILE= "psqlodbca.def"
!ELSE
DEF_FILE= "psqlodbc.def"
!ENDIF
!IF  "$(CFG)" == "Release"
LINK32_FLAGS=$(LINK32_FLAGS) /incremental:no
!ELSE
LINK32_FLAGS=$(LINK32_FLAGS) /incremental:yes /debug /pdbtype:sept
!ENDIF
LINK32_FLAGS=$(LINK32_FLAGS) "$(VC07_DELAY_LOAD)"
!IF "$(PG_LIB)" != ""
LINK32_FLAGS=$(LINK32_FLAGS) /libpath:"$(PG_LIB)"
!ENDIF
!IF "$(SSL_LIB)" != ""
LINK32_FLAGS=$(LINK32_FLAGS) /libpath:"$(SSL_LIB)"
!ENDIF

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
	"$(INTDIR)\md5.obj" \
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
!IF "$(USE_SSPI)" == "yes"
	"$(INTDIR)\sspisvcs.obj" \
!ENDIF
!IF "$(USE_GSS)" == "yes"
	"$(INTDIR)\gsssvcs.obj" \
!ENDIF
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
	"$(INTDIR)\xalibname.obj" \
!ENDIF
!IF "$(MEMORY_DEBUG)" == "yes"
	"$(INTDIR)\inouealc.obj" \
!ENDIF
	"$(INTDIR)\psqlodbc.res"

DTCDEF_FILE= "$(DTCLIB).def"
LIB32_DTCLIBFLAGS=/nologo /machine:$(CPU) /def:"$(DTCDEF_FILE)"

LINK32_DTCFLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib uuid.lib wsock32.lib XOleHlp.lib $(OUTDIR)\$(MAINLIB).lib bufferoverflowu.lib Delayimp.lib /DelayLoad:XOLEHLP.DLL /nologo /dll /incremental:no /machine:$(CPU)
LINK32_DTCOBJS= \
        "$(INTDIR)\msdtc_enlist.obj" "$(INTDIR)\xalibname.obj"

XADEF_FILE= "$(XALIB).def"
LINK32_XAFLAGS=/nodefaultlib:libcmt.lib kernel32.lib user32.lib gdi32.lib advapi32.lib odbc32.lib odbccp32.lib wsock32.lib XOleHlp.lib winmm.lib msvcrt.lib bufferoverflowu.lib /nologo /dll /incremental:no /machine:$(CPU) /def:"$(XADEF_FILE)"
LINK32_XAOBJS= \
	"$(INTDIR)\pgxalib.obj" 

"$(OUTDIR)\$(MAINDLL)" : $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS) /pdb:$*.pdb /implib:$*.lib /out:$@
<<

"$(OUTDIR)\$(DTCLIB).lib" : $(DEF_FILE) $(LINK32_DTCOBJS)
    $(LIB32) @<<
  $(LIB32_DTCLIBFLAGS) $(LINK32_DTCOBJS) /out:$@
<<

"$(OUTDIR)\$(DTCDLL)" : $(LINK32_DTCOBJS)
    $(LINK32) @<<
  $(LINK32_DTCFLAGS) $(LINK32_DTCOBJS) $*.exp /pdb:$*.pdb /out:$@
<<

"$(OUTDIR)\$(XADLL)" : $(XADEF_FILE) $(LINK32_XAOBJS)
    $(LINK32) @<<
  $(LINK32_XAFLAGS) $(LINK32_XAOBJS) /pdb:$*.pdb /implib:$*.lib /out:$@
<<


SOURCE=psqlodbc.rc

"$(INTDIR)\psqlodbc.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ)  $(RSC_DEFINES) $(SOURCE)
