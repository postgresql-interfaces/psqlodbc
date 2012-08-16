@echo off
setlocal

rem
rem	The 64bit installer
rem
set CPUTYPE=x64

rem
rem	Set yes when clean operations are needed
rem
set CLEANUP=no
rem
rem	Set yes so as to build dlls before making the installer
rem
set DLLBUILD=no

:getparam
set para=%1
echo para="%para%"
if /i "%para:~1%" == "Drivers" (
	if "%para:~0,1%" == "+" set DLLBUILD=yes
	shift
	goto getparam
) else if /i "%para%" == "clean" (
	set CLEANUP=yes
	shift
	goto getparam
)

if "%CLEANUP%" == "yes" GOTO CLEAN

rem
rem	psqlodbc dlls build options
rem
SET USE_LIBPQ=no
SET USE_GSS=yes
SET USE_SSPI=yes

rem	
rem	Please specify the foler name where you placed libpq related dlls.
rem	Currently not used.
rem
rem SET LIBPQBINDIR=
echo LIBPQBINDIR=%LIBPQBINDIR%

rem	
rem	Please specify the foler name where you placed GSSAPI related dlls.
rem
SET GSSBINDIR="c:/cygwin/develop/bin/AMD64"

rem
rem	Build binaries if necessary
rem
set origdir=%CD%
if "%DLLBUILD%" == "yes" (
cd ..
nmake /f win64.mak USE_LIBPQ=%USE_LIBPQ% USE_SSPI=%USE_SSPI% USE_GSS=%USE_GSS%
nmake /f win64.mak ANSI_VERSION=yes USE_LIBPQ=%USE_LIBPQ% USE_SSPI=%USE_SSPI% USE_GSS=%USE_GSS%
)
cd %origdir%

set wix_dir="%WIX%bin"
echo wix_dir=%wix_dir%

rem Values to change include VERSION and SUBLOC, both below.

rem The subdirectory to install into
SET SUBLOC="0901"

if NOT "%1"=="" SET VERSION="%1"
if NOT "%1"=="" GOTO GOT_VERSION
GOTO NORMAL_EXEC

:CLEAN
echo.
echo cleaning derived files
echo.
del %CPUTYPE%\psqlodbc*.wix* %CPUTYPE%\psqlodbc*.ms*
if "%DLLBUILD%" == "yes" (
cd ..
nmake /f win64.mak clean
nmake /f win64.mak ANSI_VERSION=yes clean
)
GOTO EXIT

:NORMAL_EXEC
REM The full version number of the build in XXXX.XX.XX format
SET VERSION="09.01.0200"

echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

if not exist %CPUTYPE%\ mkdir %CPUTYPE%

echo.
echo Building psqlODBC merge module...

%wix_dir%\candle.exe -nologo -dPlatform="%CPUTYPE%" -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -dLIBPQBINDIR=%LIBPQBINDIR% -dGSSBINDIR=%GSSBINDIR% -o %CPUTYPE%\psqlodbcm.wixobj psqlodbcm_cpu.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo Linking psqlODBC merge module...
%wix_dir%\light -nologo -o %CPUTYPE%\psqlodbc_%CPUTYPE%.msm %CPUTYPE%\psqlodbcm.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Building psqlODBC installer database...

%wix_dir%\candle.exe -nologo -dPlatform="%CPUTYPE%" -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -o %CPUTYPE%\psqlodbc.wixobj psqlodbc_cpu.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo Linking psqlODBC installer database...
%wix_dir%\light -nologo -ext WixUIExtension -cultures:en-us -o %CPUTYPE%\psqlodbc_%CPUTYPE%.msi %CPUTYPE%\psqlodbc.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Modifying psqlODBC installer database...
cscript modify_msi.vbs %CPUTYPE%\psqlodbc_%CPUTYPE%.msi
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Done!
GOTO EXIT

:ERR_HANDLER
echo.
echo Aborting build!
GOTO EXIT

:EXIT
