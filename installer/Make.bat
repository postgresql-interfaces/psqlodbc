@echo off

if NOT "%1"=="" SET VERSION="%1"
if NOT "%1"=="" GOTO GOT_VERSION

SET VERSION="08.01.0200"

echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

echo.
echo Building psqlODBC merge module...

candle -nologo -dVERSION=%VERSION% -dPROGRAMFILES="%ProgramFiles%" psqlodbcm.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

light -nologo -out psqlodbc.msm psqlodbcm.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Building psqlODBC installer database...

candle -nologo -dVERSION=%VERSION% -dPROGRAMFILES="%ProgramFiles%" psqlodbc.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

light -nologo psqlodbc.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Done!
GOTO EXIT

:ERR_HANDLER
echo.
echo Aborting build!
GOTO EXIT

:EXIT
