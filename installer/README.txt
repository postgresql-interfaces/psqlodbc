This directory contains the psqlODBC installer for Windows. To build the
installer, you will need a copy of WiX installed somewhere in your system
path. The installer has been tested with WiX version 3.0.2420 only at the
time writing. WiX may be downloaded from:

  http://sourceforge.net/projects/wix/.

In order to build the installer, first ensure that a suitable binary is in
the $SRC\Release directory, then, from the $SRC\Installer directory run:

C:\psqlODBC\Installer> make 09.01.0200

The version number will default to a value set in the Make.bat batch file if
not specified on the command line. 2 files will be built:

psqlodbc.msm - A merge module for use in other projects.
psqlodbc.msi - A Windows Installer package for standalone use.
