@echo off

if NOT "%1"=="" SET VERSION="%1"
if NOT "%1"=="" GOTO GOT_VERSION

SET VERSION="08.00.0100"
echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

echo.
echo Building psqlODBC merge module...

candle -nologo -dVERSION=%VERSION% psqlodbcm.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

light -nologo -out psqlodbc.msm psqlodbcm.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Building psqlODBC installer database...

candle -nologo -dVERSION=%VERSION% psqlodbc.wxs
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