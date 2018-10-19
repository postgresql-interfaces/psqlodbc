#/bin/bash
##
##	Build installers of psqlodbc project for Windows
##		from WSL or cygwin
##
pscript=installer/buildInstallers.ps1
if [ $# -gt 0 ]; then
	if [ $1 = "/?" ]; then
		powershell.exe Get-Help $pscript -detailed
		exit 0
	elif [ $1 = "-?" ]; then
		shift
		powershell.exe Get-Help $pscript $*
		exit 0
	fi
fi
powershell.exe $pscript $*
