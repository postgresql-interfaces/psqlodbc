# Microsoft Developer Studio Project File - Name="psqlodbc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=psqlodbc - Win32 ANSI Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "psqlodbc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "psqlodbc.mak" CFG="psqlodbc - Win32 ANSI Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "psqlodbc - Win32 ANSI Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "psqlodbc - Win32 Unicode Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "psqlodbc - Win32 ANSI Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "psqlodbc - Win32 Unicode Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "psqlodbc - Win32 ANSI Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ANSI Release"
# PROP BASE Intermediate_Dir "ANSI Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ANSI-Release"
# PROP Intermediate_Dir "ANSI-Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "C:/Program Files/PostgreSQL/8.1/include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D ODBCVER=0x0300 /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG" /d "MULTIBYTE"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libpq.lib /nologo /dll /machine:I386 /out:"ANSI-Release/psqlodbca.dll" /libpath:"C:/Program Files/PostgreSQL/8.1/lib/ms"

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Unicode Release"
# PROP BASE Intermediate_Dir "Unicode Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Unicode-Release"
# PROP Intermediate_Dir "Unicode-Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /I "C:/Program Files/PostgreSQL/8.1/include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D ODBCVER=0x0300 /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /FR"ANSI-Release/" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /GX /O2 /I "C:/Program Files/PostgreSQL/8.1/include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D ODBCVER=0x0300 /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /D "UNICODE_SUPPORT" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /fo"ANSI-Release/psqlodbc.res" /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG" /d "MULTIBYTE" /d "UNICODE_SUPPORT"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo /o"ANSI-Release/psqlodbc.bsc"
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libpq.lib /nologo /dll /machine:I386 /out:"ANSI-Release/psqlodbca.dll" /libpath:"C:/Program Files/PostgreSQL/8.1/lib/ms"
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libpq.lib /nologo /dll /machine:I386 /out:"Unicode-Release/psqlodbcw.dll" /libpath:"C:/Program Files/PostgreSQL/8.1/lib/ms"
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 ANSI Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ANSI Debug"
# PROP BASE Intermediate_Dir "ANSI Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ANSI-Debug"
# PROP Intermediate_Dir "ANSI-Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /I "C:/Program Files/PostgreSQL/8.1/include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D ODBCVER=0x0300 /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /FR"ANSI-Release/" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MTd /W3 /GX /ZI /Od /I "C:/Program Files/PostgreSQL/8.1/include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D ODBCVER=0x0300 /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /fo"ANSI-Release/psqlodbc.res" /d "NDEBUG"
# ADD RSC /l 0x809 /d "_DEBUG" /d "MULTIBYTE"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo /o"ANSI-Release/psqlodbc.bsc"
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libpq.lib /nologo /dll /machine:I386 /out:"ANSI-Release/psqlodbca.dll" /libpath:"C:/Program Files/PostgreSQL/8.1/lib/ms"
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libpq.lib /nologo /dll /profile /debug /machine:I386 /out:"ANSI-Debug/psqlodbca.dll" /libpath:"C:/Program Files/PostgreSQL/8.1/lib/ms"

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Unicode Debug"
# PROP BASE Intermediate_Dir "Unicode Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Unicode-Debug"
# PROP Intermediate_Dir "Unicode-Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /I "C:/Program Files/PostgreSQL/8.1/include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D ODBCVER=0x0300 /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /D "UNICODE_SUPPORT" /FR"Unicode-Release/" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MTd /W3 /GX /Od /I "C:/Program Files/PostgreSQL/8.1/include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D ODBCVER=0x0300 /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /D "UNICODE_SUPPORT" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /fo"Unicode-Release/psqlodbc.res" /d "NDEBUG"
# ADD RSC /l 0x809 /d "_DEBUG" /d "MULTIBYTE" /d "UNICODE_SUPPORT"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo /o"Unicode-Release/psqlodbc.bsc"
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libpq.lib /nologo /dll /pdb:"Unicode-Release/psqlodbcw.pdb" /machine:I386 /out:"Unicode-Release/psqlodbcw.dll" /libpath:"C:/Program Files/PostgreSQL/8.1/lib/ms"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libpq.lib /nologo /dll /profile /debug /machine:I386 /out:"Unicode-Debug/psqlodbcw.dll" /libpath:"C:/Program Files/PostgreSQL/8.1/lib/ms"

!ENDIF 

# Begin Target

# Name "psqlodbc - Win32 ANSI Release"
# Name "psqlodbc - Win32 Unicode Release"
# Name "psqlodbc - Win32 ANSI Debug"
# Name "psqlodbc - Win32 Unicode Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\bind.c
# End Source File
# Begin Source File

SOURCE=.\columninfo.c
# End Source File
# Begin Source File

SOURCE=.\connection.c
# End Source File
# Begin Source File

SOURCE=.\convert.c
# End Source File
# Begin Source File

SOURCE=.\descriptor.c
# End Source File
# Begin Source File

SOURCE=.\dlg_specific.c
# End Source File
# Begin Source File

SOURCE=.\dlg_wingui.c
# End Source File
# Begin Source File

SOURCE=.\drvconn.c
# End Source File
# Begin Source File

SOURCE=.\environ.c
# End Source File
# Begin Source File

SOURCE=.\execute.c
# End Source File
# Begin Source File

SOURCE=.\info.c
# End Source File
# Begin Source File

SOURCE=.\info30.c
# End Source File
# Begin Source File

SOURCE=.\misc.c
# End Source File
# Begin Source File

SOURCE=.\multibyte.c
# End Source File
# Begin Source File

SOURCE=.\odbcapi.c
# End Source File
# Begin Source File

SOURCE=.\odbcapi30.c
# End Source File
# Begin Source File

SOURCE=.\odbcapi30w.c

!IF  "$(CFG)" == "psqlodbc - Win32 ANSI Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Release"

# PROP BASE Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 ANSI Debug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\odbcapiw.c

!IF  "$(CFG)" == "psqlodbc - Win32 ANSI Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Release"

# PROP BASE Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 ANSI Debug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\options.c
# End Source File
# Begin Source File

SOURCE=.\parse.c
# End Source File
# Begin Source File

SOURCE=.\pgapi30.c
# End Source File
# Begin Source File

SOURCE=.\pgtypes.c
# End Source File
# Begin Source File

SOURCE=.\psqlodbc.c
# End Source File
# Begin Source File

SOURCE=.\qresult.c
# End Source File
# Begin Source File

SOURCE=.\results.c
# End Source File
# Begin Source File

SOURCE=.\setup.c
# End Source File
# Begin Source File

SOURCE=.\statement.c
# End Source File
# Begin Source File

SOURCE=.\tuple.c
# End Source File
# Begin Source File

SOURCE=.\tuplelist.c
# End Source File
# Begin Source File

SOURCE=.\win_md5.c
# End Source File
# Begin Source File

SOURCE=.\win_unicode.c

!IF  "$(CFG)" == "psqlodbc - Win32 ANSI Release"

# PROP Intermediate_Dir "ANSI-Release"
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Release"

# PROP BASE Intermediate_Dir "ANSI-Release"
# PROP BASE Exclude_From_Build 1
# PROP Intermediate_Dir "ANSI-Release"

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 ANSI Debug"

# PROP BASE Intermediate_Dir "ANSI-Release"
# PROP BASE Exclude_From_Build 1
# PROP Intermediate_Dir "ANSI-Release"
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Debug"

# PROP BASE Intermediate_Dir "ANSI-Release"
# PROP Intermediate_Dir "ANSI-Release"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\bind.h
# End Source File
# Begin Source File

SOURCE=.\columninfo.h
# End Source File
# Begin Source File

SOURCE=.\connection.h
# End Source File
# Begin Source File

SOURCE=.\convert.h
# End Source File
# Begin Source File

SOURCE=.\descriptor.h
# End Source File
# Begin Source File

SOURCE=.\dlg_specific.h
# End Source File
# Begin Source File

SOURCE=.\environ.h
# End Source File
# Begin Source File

SOURCE=.\md5.h
# End Source File
# Begin Source File

SOURCE=.\misc.h
# End Source File
# Begin Source File

SOURCE=.\multibyte.h
# End Source File
# Begin Source File

SOURCE=.\pgapifunc.h
# End Source File
# Begin Source File

SOURCE=.\pgtypes.h
# End Source File
# Begin Source File

SOURCE=.\psqlodbc.h
# End Source File
# Begin Source File

SOURCE=.\qresult.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\statement.h
# End Source File
# Begin Source File

SOURCE=.\tuple.h
# End Source File
# Begin Source File

SOURCE=.\tuplelist.h
# End Source File
# Begin Source File

SOURCE=.\version.h
# End Source File
# Begin Source File

SOURCE=.\win_setup.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\psqlodbc.rc
# End Source File
# End Group
# Begin Group "Export Files"

# PROP Default_Filter ".def"
# Begin Source File

SOURCE=.\psqlodbca.def

!IF  "$(CFG)" == "psqlodbc - Win32 ANSI Release"

# PROP BASE Exclude_From_Build 1
# PROP Intermediate_Dir "ANSI-Release"

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Release"

# PROP BASE Intermediate_Dir "ANSI-Release"
# PROP Intermediate_Dir "ANSI-Release"
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 ANSI Debug"

# PROP BASE Intermediate_Dir "ANSI-Release"
# PROP Intermediate_Dir "ANSI-Release"

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Debug"

# PROP BASE Intermediate_Dir "ANSI-Release"
# PROP BASE Exclude_From_Build 1
# PROP Intermediate_Dir "ANSI-Release"
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\psqlodbcw.def

!IF  "$(CFG)" == "psqlodbc - Win32 ANSI Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Release"

# PROP BASE Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 ANSI Debug"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "psqlodbc - Win32 Unicode Debug"

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
