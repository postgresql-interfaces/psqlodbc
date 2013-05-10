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
:: version to the variable PGVERSION (default 9.2) and call 
:: this batch file.
::
if "%PGVERSION%" == "" SET PGVERSION=9.2
::
:: When you placed libpq related dlls in the folder other than
:: the default one, set the folder name to the variable LIBPQBINDIR
:: and call this batch file.
::
if "%LIBPQBINDIR%" == "" (
	SET LINKFILES=%X86PROGRAMFILES%\PostgreSQL\%PGVERSION%\bin
 ) else (
	SET LINKFILES=%LIBPQBINDIR%
 )

if NOT "%1"=="" (
       SET VERSION="%1"
       GOTO GOT_VERSION
)

:: The full version number of the build in XXXX.XX.XX format
SET VERSION="09.01.0200"
echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

:: The subdirectory to install into
SET SUBLOC=%VERSION:~1,2%%VERSION:~4,2%

echo.
echo Building psqlODBC/%SUBLOC% merge module...

candle -nologo -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -dLINKFILES="%LINKFILES%" psqlodbcm.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

light -nologo -out psqlodbc.msm psqlodbcm.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Building psqlODBC installer database...

candle -nologo -dVERSION=%VERSION% -dSUBLOC=%SUBLOC% -dPROGRAMFILES="%X86PROGRAMFILES%" -dPROGRAMCOM="%X86COMMONFILES%\Merge Modules" psqlodbc.wxs
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
