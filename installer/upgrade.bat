@echo off
cls
echo This file will upgrade your psqlODBC installation.
echo.
echo You must have psqlODBC 09.01.xxxx installed 
echo from the official MSI installation to use this upgrade path.
echo.
echo If psqlODBC or any of it's components are in use
echo a reboot will be required once the upgrade is completed.
echo.
echo.
echo Press Ctrl-C to abort the upgrade or
pause

REM Parameters described:
REM  /i psqlodbc.msi           - pick MSI file to install. All properties
REM                              will be read from existing installation.
REM  REINSTALLMODE=vamus       - reinstall all files, regardless of version.
REM                              This makes sure documentation and other
REM                              non-versioned files are updated.
REM  REINSTALL=ALL             - Reinstall all features that were previously
REM                              installed with the new version.
msiexec /i psqlodbc.msi REINSTALLMODE=vamus REINSTALL=ALL /qr
