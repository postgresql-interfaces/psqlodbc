/********************************************************************

  Development using Powershell and MSBuild instead of Batch and Nmake

  Currently 4 Windows Powershell scripts are provided for developpers.

  BuildAll.ps1          - build all dlls for psqlodbc using MSBuild.
  editConfiguration.ps1 - a GUI tool to set Build environment
  regress.ps1           - build regression test programs and run
  buildInstallers.ps1   - build installers(.msi or setup.exe)

  You can invoke these scripts from Powershell console or you can also 
  use the same functionality from Command prompt using Windows helper 
  batch at the parent folder (..\). See ..\readme_winbuild.txt.

  1. Please start a powershell console and set the ExecutionPolicy of
     Powershell to RemoteSigned or Unrestricted.

     You can get the ExecutionPolicy by typing

	Get-ExecutionPolicy

     When the ExectionPolicy is "Restricted" or "AllSigned" then type e.g.

	Set-ExecutionPolicy RemoteSigned

     To see details about ExecutionPolicy, type

	Get-Help about_Execution_Policies

  2. You have to install one of the following.

   . Visual Studio 2015 non-Express edtion or Express 2015 for Windows
	Desktop
   . Visual Studio 2013 non-Express edtion or Express 2013 for Windows
	Desktop
   . Visual Studio 2012 non-Express edtion or Express 2012 for Windows
	Desktop
   . Full Microsoft Visual C++ 2010
   . Windows SDK 7.1

     You have to include x64 development tools (bin, lib, include) as
     well as x86 ones for the installation.

     You can install multiple versions of VC++ and use them.
     You can easily switch by specifying VCVersion parameter.

  3. Setup Build environment

     Please type

	.\editConfiguration(.ps1)

     and edit the setting of your environment especially the folders
     you placed libpq related include/lib/bin files.

  4. Build

     Please type

	.\BuildAll(.ps1)

     to invoke build operations.

     If you installed both VC10 and VC12 and you'd like to compile
     under VC10 environment, type

        .\BuildAll(.ps1) -V(CVersion) 10.0

     or set the value 10.0 to Configuration.vcver attribute of the
     configuration file.

     To see details about the use of BuildAll, type

	Get-Help .\BuildAll(.ps1) [-Detailed | -Examples | -Full]

  5. Regression test

     Please type

	.\regress(.ps1)

     By default, build 32-bit binaries from test sources and run the tests.
     If you'd like to test 64-bit version, please type

	.\regress(.ps1) -p(latform) x64
	
  6. Outputs

     The build can produce output in up to four directories for each of
     the debug and release configurations:

     - x64_Unicode_Release     the Unicode driver, 64-bit
     - x86_ANSI_Release        the ANSI driver, 64-bit
     - x86_Unicode_Release     the ANSI driver, 32-bit
     - x86_ANSI_Release        the Unicode driver, 32-bit

     For debug builds (-Configuration Debug) the directories are named with
     Debug instead of Release but otherwise the same.

     pgxalib.dll is only built for the multibyte/unicode version, as it is
     the same for both unicode and ansi drivers.

     Dependencies like libpq etc are not copied into the build
     output directories. You must copy them to the target directory yourself.
     Dependency Walker (depends.exe) from http://dependencywalker.com/ can help
     you find what's needed, but in general you'll need to add:

     - libpq (from the PostgreSQL bin dir)
     - ssleay32 (from the PostgreSQL bin dir)
     - libeay32 (from the PostgreSQL bin dir)
     - libintl (from the PostgreSQL bin dir)

     ... and the Visual Studio runtime redist for the version of Visual Studio
     you compiled with.

  7. Installer

     See ..\installers\README.txt.

Troubleshooting:

Some documentation on dealing with Windows SDK installation issues can be found
on the related pg_build_win page:
https://github.com/2ndQuadrant/pg_build_win#troubleshooting
     

***********************************************************************/
