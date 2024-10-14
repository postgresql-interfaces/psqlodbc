This directory contains the psqlODBC installer for Windows. To build the
installer, you will need a copy of WiX installed. The installer has been
tested with WiX version 5.0.2 at the time of writing.

WiX may be downloaded from:

  http://wixtoolset.org/

In addition to the base package, the UI extension is required:

  dotnet tool install --global wix
  wix extension add --global WixToolset.UI.wixext


HOW TO BUILD
----------

Ensure that suitable binaries are in the parent directory Release build outputs
(see ..\winbuild for that). 

  .\buildInstallers.ps1

For help:

  Get-Help .\buildInstallers.ps1

If you get execution policy errors:

  Set-ExecutionPolicy RemoteSigned

and try again.

Note that these installer generators use the configuration file prepared by the
PowerShell scripts in ..\winbuild, defaulting to ..\winbuild\configuration.xml,
so you can't just mix them with SDK-based compilation.

