# Microsoft Developer Studio Project File - Name="psqlODBC" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=psqlODBC - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "psqlodbc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "psqlodbc.mak" CFG="psqlODBC - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "psqlODBC - Win32 Release" ("Win32 (x86) Console Application")
!MESSAGE "psqlODBC - Win32 Debug" ("Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "psqlODBC - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /D "DYNAMIC_LOAD" /D "MULTIBYTE" /D "UNICODE_SUPPORT" /Fp"psqlodbc.pch" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "C:\Program Files\PostgreSQL\9.3\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /D "DYNAMIC_LOAD" /D "MULTIBYTE" /D "UNICODE_SUPPORT" /Fp"psqlodbc.pch" /YX /FD" /c
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /i "." /d "NDEBUG"
# SUBTRACT RSC /x
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib XOleHlp.lib winmm.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 wsock32.lib XOleHlp.lib winmm.lib libpq.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /pdb:"psqlodbc35w.pdb" /machine:I386 /out:"psqlodbc35w.dll" /implib:"psqlodbc35w.lib" /libpath:"C:\Program Files\PostgreSQL\9.3\lib\ms"
# SUBTRACT LINK32 /pdb:none /nodefaultlib

!ELSEIF  "$(CFG)" == "psqlODBC - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /D "DYNAMIC_LOAD" /D "MULTIBYTE" /D "UNICODE_SUPPORT" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "C:\Program Files\PostgreSQL\9.3\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PSQLODBC_EXPORTS" /D "DRIVER_CURSOR_IMPLEMENT" /D "WIN_MULTITHREAD_SUPPORT" /D "DYNAMIC_LOAD" /D "MULTIBYTE" /D "UNICODE_SUPPORT" /FR /YX /FD /c
# SUBTRACT CPP /X
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /i "." /d "_DEBUG"
# SUBTRACT RSC /x
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib XOleHlp.lib winmm.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 wsock32.lib XOleHlp.lib winmm.lib libpq.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /pdb:"psqlodbc35w.pdb" /debug /machine:I386 /out:"psqlodbc35w.dll" /implib:"psqlodbc35w.lib" /libpath:"C:\Program Files\PostgreSQL\9.3\lib\ms"
# SUBTRACT LINK32 /pdb:none /nodefaultlib

!ENDIF 

# Begin Target

# Name "psqlODBC - Win32 Release"
# Name "psqlODBC - Win32 Debug"
# Begin Group "source"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=bind.c
# End Source File
# Begin Source File

SOURCE=columninfo.c
# End Source File
# Begin Source File

SOURCE=connection.c
# End Source File
# Begin Source File

SOURCE=convert.c
# End Source File
# Begin Source File

SOURCE=.\descriptor.c
# End Source File
# Begin Source File

SOURCE=dlg_specific.c
# End Source File
# Begin Source File

SOURCE=dlg_wingui.c
# End Source File
# Begin Source File

SOURCE=drvconn.c
# End Source File
# Begin Source File

SOURCE=environ.c
# End Source File
# Begin Source File

SOURCE=execute.c
# End Source File
# Begin Source File

SOURCE=info.c
# End Source File
# Begin Source File

SOURCE=.\inouealc.c
# End Source File
# Begin Source File

SOURCE=.\loadlib.c
# End Source File
# Begin Source File

SOURCE=lobj.c
# End Source File
# Begin Source File

SOURCE=misc.c
# End Source File
# Begin Source File

SOURCE=multibyte.c
# End Source File
# Begin Source File

SOURCE=.\mylog.c
# End Source File
# Begin Source File

SOURCE=.\odbcapi.c
# End Source File
# Begin Source File

SOURCE=.\odbcapi30.c
# End Source File
# Begin Source File

SOURCE=.\odbcapi30w.c
# End Source File
# Begin Source File

SOURCE=.\odbcapiw.c
# End Source File
# Begin Source File

SOURCE=options.c
# End Source File
# Begin Source File

SOURCE=parse.c
# End Source File
# Begin Source File

SOURCE=pgapi30.c
# End Source File
# Begin Source File

SOURCE=pgtypes.c
# End Source File
# Begin Source File

SOURCE=psqlodbc.c
# End Source File
# Begin Source File

SOURCE=qresult.c
# End Source File
# Begin Source File

SOURCE=results.c
# End Source File
# Begin Source File

SOURCE=setup.c
# End Source File
# Begin Source File

SOURCE=statement.c
# End Source File
# Begin Source File

SOURCE=tuple.c
# End Source File
# Begin Source File

SOURCE=.\win_unicode.c
# End Source File
# End Group
# Begin Group "include"

# PROP Default_Filter ""
# Begin Source File

SOURCE=bind.h
# End Source File
# Begin Source File

SOURCE=columninfo.h
# End Source File
# Begin Source File

SOURCE=connection.h
# End Source File
# Begin Source File

SOURCE=convert.h
# End Source File
# Begin Source File

SOURCE=descriptor.h
# End Source File
# Begin Source File

SOURCE=dlg_specific.h
# End Source File
# Begin Source File

SOURCE=environ.h
# End Source File
# Begin Source File

SOURCE=iodbc.h
# End Source File
# Begin Source File

SOURCE=isql.h
# End Source File
# Begin Source File

SOURCE=isqlext.h
# End Source File
# Begin Source File

SOURCE=.\loadlib.h
# End Source File
# Begin Source File

SOURCE=lobj.h
# End Source File
# Begin Source File

SOURCE=misc.h
# End Source File
# Begin Source File

SOURCE=multibyte.h
# End Source File
# Begin Source File

SOURCE=pgapifunc.h
# End Source File
# Begin Source File

SOURCE=pgtypes.h
# End Source File
# Begin Source File

SOURCE=psqlodbc.h
# End Source File
# Begin Source File

SOURCE=qresult.h
# End Source File
# Begin Source File

SOURCE=socket.h
# End Source File
# Begin Source File

SOURCE=statement.h
# End Source File
# Begin Source File

SOURCE=tuple.h
# End Source File
# Begin Source File

SOURCE=tuplelist.h
# End Source File
# Begin Source File

SOURCE=win_setup.h
# End Source File
# End Group
# Begin Group "resource"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\psqlodbc.def
# End Source File
# Begin Source File

SOURCE=.\psqlodbc.rc
# End Source File
# Begin Source File

SOURCE=resource.h
# End Source File
# End Group
# End Target
# End Project
