::
::	regression test using MSBuild
::
@echo off
if "%1" == "/?" (
	powershell Get-Help '%~dp0\winbuild\regress.ps1' -detailed
) else if "%1" == "-?" (
	powershell Get-Help '%~dp0\winbuild\regress.ps1' %2 %3 %4 %5 %6 %7 %8 %9
) else (
	powershell "& '%~dp0\winbuild\regress.ps1' %*"
)
