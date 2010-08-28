@echo off
set wix_dir="%WIX%bin"
echo wix_dir=%wix_dir%
REM Values to change include VERSION and SUBLOC, both below.

REM The subdirectory to install into
SET SUBLOC="0900"
SET CPUTYPE="x64"

if "%1" == "clean" GOTO CLEAN

if NOT "%1"=="" SET VERSION="%1"
if NOT "%1"=="" GOTO GOT_VERSION
GOTO NORMAL_EXEC:

:CLEAN
echo.
echo cleaning derived files
echo.
del %CPUTYPE%\psqlodbc*.wix* %CPUTYPE%\psqlodbc*.ms*
GOTO EXIT

:NORMAL_EXEC
REM The full version number of the build in XXXX.XX.XX format
SET VERSION="09.00.0100"

echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

if not exist %CPUTYPE%\ mkdir %CPUTYPE%

echo.
echo Building psqlODBC merge module...

%wix_dir%\candle.exe -nologo -dPlatform="%CPUTYPE%" -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -dSYSTEM32DIR="%SystemRoot%/system32" -o %CPUTYPE%\psqlodbcm.wixobj psqlodbcm_cpu.wxs
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
