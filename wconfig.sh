#/bin/bash
##
##	Start editConfiguration with a mimimized console window
##
cd $(dirname $0)
Powershell.exe -Sta -WindowStyle Minimized winbuild/editConfiguration.ps1 $*
