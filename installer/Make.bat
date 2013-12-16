@echo off

:: Values to change include VERSION and SUBLOC, both below.

setlocal
SET X86PROGRAMFILES=%ProgramFiles%
SET X86COMMONFILES=%CommonProgramFiles%

if "%PROCESSOR_ARCHITECTURE%" == "x86" GOTO SET_LINKFILES
SET X86PROGRAMFILES=%ProgramFiles(x86)%
SET X86COMMONFILES=%CommonProgramFiles(x86)%

:SET_LINKFILES
:: echo X86PROGRAMFILES=%X86PROGRAMFILES%
:: echo X86COMMONFILES=%X86COMMONFILES%
::
:: When you reference PG server's libpq related dlls, set the
:: version to the variable PGVERSION (default 9.3) and call 
:: this batch file.
::
if "%PGVERSION%" == "" SET PGVERSION=9.3
::
:: When you placed libpq related dlls in the folder other than
:: the default one, set the folder name to the variable LIBPQBINDIR
:: and call this batch file.
::
if "%LIBPQBINDIR%" == "" (
	SET LINKFILES="%X86PROGRAMFILES%\PostgreSQL\%PGVERSION%\bin"
 ) else (
	SET LINKFILES="%LIBPQBINDIR%"
 )

if NOT "%1"=="" (
       SET VERSION="%1"
       GOTO GOT_VERSION
)

::
::	The ProductCode History
::	    ProductCode must be changed in case of major upgrade
::
SET PRODUCTID["09.02.0100"]="838E187D-8B7A-473d-B93C-C8E970B15D2B"
SET PRODUCTID["09.03.0100"]="D3527FA5-9C2B-4550-A59B-9534A78950F4"

:: The full version number of the build in XXXX.XX.XX format
SET VERSION="09.03.0100"
echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

:: The subdirectory to install into
SET SUBLOC=%VERSION:~1,2%%VERSION:~4,2%

CALL SET PRODUCTCODE=%%PRODUCTID[%VERSION%]%%
echo PRODUCTCODE=%PRODUCTCODE%

echo.
echo Building psqlODBC/%SUBLOC% merge module...

candle -nologo -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -dLINKFILES=%LINKFILES% psqlodbcm.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

light -nologo -out psqlodbc.msm psqlodbcm.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Building psqlODBC installer database...

::SET PROGRAMCOM="%X86COMMONFILES%/Merge Modules"
candle -nologo -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -dPRODUCTCODE="%PRODUCTCODE%" psqlodbc.wxs
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
endlocal
