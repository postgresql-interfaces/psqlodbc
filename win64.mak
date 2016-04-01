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

# Include default configuration options, followed by any local overrides
!INCLUDE windows-defaults.mak
!IF EXISTS(windows-local.mak)
!INCLUDE windows-local.mak
!ENDIF

!IF "$(ANSI_VERSION)" == "yes"
!MESSAGE Building the PostgreSQL ANSI 3.0 Driver for $(TARGET_CPU)...
!ELSE
!MESSAGE Building the PostgreSQL Unicode 3.5 Driver for $(TARGET_CPU)...
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
!MESSAGE "Release" ($(TARGET_CPU) Release DLL)
!MESSAGE "Debug" ($(TARGET_CPU) Debug DLL)
!MESSAGE
!ERROR An invalid configuration was specified.
!ENDIF

#
#	Please replace the default options from the commandline if necessary
#
!IFNDEF	CUSTOMCLOPT
CUSTOMCLOPT=/nologo /W3 /wd4018 /EHsc
!IF  "$(CFG)" == "Release"
CUSTOMCLOPT=$(CUSTOMCLOPT) /MD
!ELSE
CUSTOMCLOPT=$(CUSTOMCLOPT) /MDd
!ENDIF
!ELSE
!MESSAGE CL option $(CUSTOMCLOPT) specified
!ENDIF

#
#	Please specify additional libraries to link from the command line.
#	For example specify
#		CUSTOMLINKLIBS=bufferoverflowu.lib
#	  when bufferoverflowu.lib is needed in old VC environment.
#
CUSTOMLINKLIBS=

# Print the paths that will be used in the build
!MESSAGE Using PostgreSQL Include directory: $(PG_INC)
!MESSAGE Using PostgreSQL Library directory: $(PG_LIB)

!IF "$(ANSI_VERSION)" == "yes"
DTCLIB = pgenlista
!ELSE
DTCLIB = pgenlist
!ENDIF
DTCDLL = $(DTCLIB).dll

!IF "$(TARGET_CPU)" == "x64"
ADD_DEFINES=$(ADD_DEFINES) /D _WIN64
!ENDIF

!IF "$(MSDTC)" != "no"
VC07_DELAY_LOAD=$(VC07_DELAY_LOAD) /DelayLoad:$(DTCDLL)
!ENDIF
VC07_DELAY_LOAD=$(VC07_DELAY_LOAD) /DELAY:UNLOAD

ADD_DEFINES = $(ADD_DEFINES) /D "DYNAMIC_LOAD"
!IF "$(MSDTC)" != "no"
ADD_DEFINES = $(ADD_DEFINES) /D "_HANDLE_ENLIST_IN_DTC_"
!ENDIF
!IF "$(MEMORY_DEBUG)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /D "_MEMORY_DEBUG_" /GS
!ENDIF
!IF "$(ANSI_VERSION)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /D "DBMS_NAME=\"PostgreSQL ANSI($(TARGET_CPU))\""
!ELSE
ADD_DEFINES = $(ADD_DEFINES) /D "DBMS_NAME=\"PostgreSQL Unicode($(TARGET_CPU))\"" /D "UNICODE_SUPPORT"
RSC_DEFINES = $(RSC_DEFINES) /D "UNICODE_SUPPORT"
!ENDIF

!IF "$(PORT_CHECK)" == "yes"
ADD_DEFINES = $(ADD_DEFINES) /Wp64
!ENDIF

!IF "$(PG_INC)" != ""
INC_OPT = $(INC_OPT) /I "$(PG_INC)"
!ENDIF
!IF "$(ADD_INC)" != ""
INC_OPT = $(INC_OPT) /I "$(ADD_INC)"
!ENDIF

!IF "$(ANSI_VERSION)" == "yes"
MAINLIB = psqlodbc30a
!ELSE
MAINLIB = psqlodbc35w
!ENDIF
MAINDLL = $(MAINLIB).dll
XALIB = pgxalib
XADLL = $(XALIB).dll

# Construct output directory name. The name consists of three parts,
# target CPU, ANSI/Unicode, and Debug/Release. For example,the output
# directory debug-enabled 32-bit ANSI-version is:
#
# .\x86_ANSI_Debug
#
OUTDIR=.\$(TARGET_CPU)
!IF  "$(ANSI_VERSION)" == "yes"
OUTDIR=$(OUTDIR)_ANSI
!ELSE
OUTDIR=$(OUTDIR)_Unicode
!ENDIF
!IF  "$(CFG)" == "Debug"
OUTDIR=$(OUTDIR)_Debug
!ELSE
OUTDIR=$(OUTDIR)_Release
!ENDIF

# Location for intermediary build targets (e.g. *.obj files).
INTDIR=$(OUTDIR)

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
	-@erase "$(INTDIR)\*.dll"
	-@erase "$(INTDIR)\*.pdb"

"$(INTDIR)" :
!IF !EXISTS($(INTDIR))
    mkdir "$(INTDIR)"
!ENDIF
!IF !EXISTS($(OUTDIR)) && "$(OUTDIR)" != "$(INTDIR)"
    mkdir "$(OUTDIR)"
!ENDIF

!IF  "$(MSDTC)" != "no"
"$(OUTDIR)\$(MAINDLL)": "$(OUTDIR)\$(DTCLIB).lib"
!ENDIF

CPP=cl.exe
CPP_PROJ=$(CUSTOMCLOPT) $(INC_OPT) /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_CRT_SECURE_NO_DEPRECATE" /D "WIN_MULTITHREAD_SUPPORT" $(ADD_DEFINES) /Fp"$(INTDIR)\psqlodbc.pch" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD
!IF  "$(CFG)" == "Release"
CPP_PROJ=$(CPP_PROJ) /O2
!ELSEIF  "$(CFG)" == "Debug"
CPP_PROJ=$(CPP_PROJ) /Gm /ZI /Od /D "_DEBUG" /RTC1
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

RSC=rc.exe
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\psqlodbc.res"
!IF  "$(CFG)" == "Debug"
RSC_PROJ=$(RSC_PROJ) /d "_DEBUG"
!ENDIF

LINK32=link.exe
LIB32=lib.exe
LINK32_FLAGS=libpq.lib kernel32.lib user32.lib gdi32.lib advapi32.lib odbc32.lib odbccp32.lib wsock32.lib ws2_32.lib secur32.lib XOleHlp.lib winmm.lib msvcrt.lib $(CUSTOMLINKLIBS) /nologo /dll /def:"$(DEF_FILE)"
!IF "$(MSDTC)" != "no"
LINK32_FLAGS=$(LINK32_FLAGS) "$(OUTDIR)\$(DTCLIB).lib"
!ENDIF


!IF  "$(ANSI_VERSION)" == "yes"
DEF_FILE= "psqlodbca.def"
!ELSE
DEF_FILE= "psqlodbc.def"
!ENDIF
!IF  "$(CFG)" == "Release"
LINK32_FLAGS=$(LINK32_FLAGS)
!ELSE
LINK32_FLAGS=$(LINK32_FLAGS) /debug /pdbtype:sept
!ENDIF
LINK32_FLAGS=$(LINK32_FLAGS) $(VC07_DELAY_LOAD)
!IF "$(PG_LIB)" != ""
LINK32_FLAGS=$(LINK32_FLAGS) /libpath:"$(PG_LIB)"
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
	"$(INTDIR)\lobj.obj" \
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
LIB32_DTCLIBFLAGS=/nologo /def:"$(DTCDEF_FILE)"

LINK32_DTCFLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib uuid.lib wsock32.lib XOleHlp.lib $(OUTDIR)\$(MAINLIB).lib $(CUSTOMLINKLIBS) Delayimp.lib /DelayLoad:XOLEHLP.DLL /nologo /dll
LINK32_DTCOBJS= \
	"$(INTDIR)\msdtc_enlist.obj" "$(INTDIR)\xalibname.obj"

XADEF_FILE= "$(XALIB).def"
LINK32_XAFLAGS=/nodefaultlib:libcmt.lib kernel32.lib user32.lib gdi32.lib advapi32.lib odbc32.lib odbccp32.lib wsock32.lib XOleHlp.lib winmm.lib msvcrt.lib $(CUSTOMLINKLIBS) /nologo /dll /def:"$(XADEF_FILE)"
LINK32_XAOBJS= \
	"$(INTDIR)\pgxalib.obj"

"$(OUTDIR)\$(MAINDLL)" : $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS) /pdb:$*.pdb /implib:$*.lib /out:$@
<<

"$(OUTDIR)\$(DTCLIB).lib" : $(DTCDEF_FILE) $(LINK32_DTCOBJS)
    $(LIB32) @<<
  $(LIB32_DTCLIBFLAGS) $(LINK32_DTCOBJS) /out:$@
<<

"$(OUTDIR)\$(DTCDLL)" : $(DTCDEF_FILE) $(LINK32_DTCOBJS)
    $(LINK32) @<<
  $(LINK32_DTCFLAGS) $(LINK32_DTCOBJS) $*.exp /pdb:$*.pdb /out:$@
<<

"$(OUTDIR)\$(XADLL)" : $(XADEF_FILE) $(LINK32_XAOBJS)
    $(LINK32) @<<
  $(LINK32_XAFLAGS) $(LINK32_XAOBJS) /pdb:$*.pdb /implib:$*.lib /out:$@
<<


SOURCE=psqlodbc.rc

"$(INTDIR)\psqlodbc.res" : $(SOURCE)
	$(RSC) $(RSC_PROJ) $(RSC_DEFINES) $(SOURCE)



####
# Umbrella targets, for building the driver in multiple configurations

unicode-driver:
	$(MAKE) -f win64.mak ANSI_VERSION=no
ansi-driver:
	$(MAKE) -f win64.mak ANSI_VERSION=yes

installer: unicode-driver ansi-driver
	cd installer && $(MAKE) -f installer.mak CFG=$(CFG)

# world target builds both 32-bit and 64-bit installers in one command.
#
# The "setenv" command, from Microsoft SDK, is used to switch the build
# target. However, it only changes the environment for the current shell;
# it is reset for the next command. So we cannot just call "setenv /x64"
# here one one line and nmake one the next one. I also tried creating a .bat
# file with the commands, but setenv contains a "EXIT /B" which stops the
# script from executing. But we can launch a new cmd.exe and pass the
# commands to it in stdin.
world:
	cmd < <<
setenv /x64
$(MAKE) -f win64.mak installer CFG=$(CFG)
setenv /x86
$(MAKE) -f win64.mak installer CFG=$(CFG)
<<

clean-world:
	-rd /Q /S x64_ANSI_Release x64_ANSI_Debug
	-rd /Q /S x64_Unicode_Release x64_Unicode_Debug
	-rd /Q /S x86_ANSI_Release x86_ANSI_Debug
	-rd /Q /S x86_Unicode_Release x86_Unicode_Debug
	cd installer && $(MAKE) /f installer.mak CFG=$(CFG) clean
