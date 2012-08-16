@echo off

REM Values to change include VERSION and SUBLOC, both below.

REM The subdirectory to install into
SET SUBLOC="0901"

if NOT "%1"=="" SET VERSION="%1"
if NOT "%1"=="" GOTO GOT_VERSION

REM The full version number of the build in XXXX.XX.XX format
SET VERSION="09.01.0200"

echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

echo.
echo Building psqlODBC merge module...

candle -nologo -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -dPROGRAMFILES="%ProgramFiles%" -dSYSTEM32DIR="%SystemRoot%/system32" psqlodbcm.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

light -nologo -out psqlodbc.msm psqlodbcm.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Building psqlODBC installer database...

candle -nologo -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -dPROGRAMFILES="%ProgramFiles%" -dPROGRAMCOM="%ProgramFiles%/Common Files/Merge Modules" psqlodbc.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

light -nologo -ext WixUIExtension -cultures:en-us psqlodbc.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Done!
GOTO EXIT

:ERR_HANDLER
echo.
echo Aborting build!
GOTO EXIT

:EXIT
