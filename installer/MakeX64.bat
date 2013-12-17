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
if "%USE_LIBPQ%" == "" (
	SET USE_LIBPQ=yes
)
if "%USE_GSS%" == "" (
	SET USE_GSS=no
)
if "%USE_SSPI%" == "" (
	SET USE_SSPI=yes
)

::	
::	Please specify the foler name where you placed libpq related dlls.
::	Currently not used.
::
if "%PGVERSION%" == "" SET PGVERSION=9.3
if "%USE_LIBPQ%" == "yes" (
	if "%LIBPQBINDIR%" == "" (
		if "%PROCESSOR_ARCHITECTURE%" == "AMD64" (
			SET LIBPQBINDIR="%ProgramFiles%\PostgreSQL\%PGVERSION%\bin"
		) else (
			echo You are setting USE_LIBPQ=%USE_LIBPQ%
			echo Please specify LIBPQBINDIR variable
			echo Press any key to exit ...
			pause > nul
			exit
		)
	)
	echo LIBPQBINDIR=%LIBPQBINDIR%
)

rem	
rem	Please specify the foler name where you placed GSSAPI related dlls.
rem
if "%USE_GSS%" == "yes" (
	if "%GSSBINDIR%" == "" (
		echo You are setting USE_GSS=%USE_GSS%
		echo Please specify GSSBINDIR variable
		echo Press any key to exit ...
		pause > nul
		exit
	)
	echo GSSBINDIR=%GSSBINDIR%
)

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
SET VERSION="09.03.0100"

echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

if not exist %CPUTYPE%\ mkdir %CPUTYPE%

:: The subdirectory to install into
SET SUBLOC=%VERSION:~1,2%%VERSION:~4,2%

::
::	ProductCode History
::
::
SET PRODUCTID["09.02.0100"]="3E42F836-9204-4c42-B3C3-8680A0434875"
SET PRODUCTID["09.03.0100"]="1F896F2F-5756-4d22-B5A3-040796C9B485"

CALL SET PRODUCTCODE=%%PRODUCTID[%VERSION%]%%

echo.
echo Building psqlODBC/%SUBLOC% merge module...

%wix_dir%\candle.exe -nologo -dPlatform="%CPUTYPE%" -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -dLIBPQBINDIR=%LIBPQBINDIR% -dGSSBINDIR=%GSSBINDIR% -o %CPUTYPE%\psqlodbcm.wixobj psqlodbcm_cpu.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo Linking psqlODBC merge module...
%wix_dir%\light -nologo -o %CPUTYPE%\psqlodbc_%CPUTYPE%.msm %CPUTYPE%\psqlodbcm.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Building psqlODBC installer database...

%wix_dir%\candle.exe -nologo -dPlatform="%CPUTYPE%" -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -dPRODUCTCODE=%PRODUCTCODE% -o %CPUTYPE%\psqlodbc.wixobj psqlodbc_cpu.wxs
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
endlocal
