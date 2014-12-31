/********************************************************************

  BuildAll.ps1 - A Windows powershell script to build all dlls for psqlodbc.

  You can also use the same functionality using Windows batch at the parent
  folder (..\). See ..\readme_winbuild.txt.

  1. You have to install one of the following.

   . Visual Studio 2013 non-Express edtion or Express 2013 for Windows
	Desktop
   . Visual Studio 2012 non-Express edtion or Express 2012 for Windows
	Desktop
   . Full Microsoft Visual C++ 2010
   . Windows SDK 7.1

     You have to include x64 development tools (bin, lib, include) as
     well as x86 ones for the installation.

  Note that the Visual Studio 2010 install conflicts with Windows SDK 7.1.
  Visual Studio 2012 conflict with both Windows SDK 7.1 and Visual Studio 2010.
  Finally, Windows SDK 7.1 will fail to install if a newer Visual Studio 2010 
  runtime is already installed on the computer.

  To avoid conflicts, install only one SDK. There is an order in which all
  can be installed safely, but it's cumbersome and there's little benefit.

  Once you have an SDK installed, please start a powershell console and

  2. Set the ExecutionPolicy of Powershell to RemoteSigned or Unrestricted.
     You can get the ExecutionPolicy by typing

	Get-ExecutionPolicy

     When the ExectionPolicy is "Restricted" or "AllSigned" then type e.g.

	Set-ExecutionPolicy RemoteSigned

     To see details about ExecutionPolicy, type

	Get-Help about_Execution_Policies

  3. Please type

	.\editConfiguration(.ps1)

     and edit the setting of your environment especially the folders
     you placed libpq related include/lib files.

  4. Please type

	.\BuildAll(.ps1)

     to invoke build operations.
     If the automatically selected VisualStudioVersion is inappropriate
     and the desired version is e.g. 11.0, type

	.\BuildAll(.ps1) -V(CVersion) 11.0

     or set the value 11.0 to Configuration.vcver attribute of the
     configuration file.

     To see details about the use of BuildAll, type

	Get-Help .\BuildAll(.ps1) [-Detailed | -Examples | -Full]

  5. Outputs

     The build can produce output in up to four directories for each of
     the debug and release configurations:

     - AMD64Release            the Unicode driver, 64-bit
     - AMD64ANSIRelease        the ANSI driver, 64-bit
     - Release                 the ANSI driver, 32-bit
     - MultibyteRelease        the Unicode driver, 32-bit

     For debug builds (-Configuration Debug) the directories are named with
     Debug instead of Release but otherwise the same.

     pgxalib.dll is only built for the multibyte/unicode version, as it is
     the same for both unicode and ansi drivers.

     Dependencies like libpq etc are not copied into the build
     output directories. You must copy them to the target directory yourself.
     Dependency Walker (depends.exe) from http://dependencywalker.com/ can help
     you find what's needed, but in general you'll need to add:

     - libpq (from the PostgreSQL bin dir)
     - libintl (from the PostgreSQL bin dir)

     ... and the Visual Studio runtime redist for the version of Visual Studio
     you compiled with.

Troubleshooting:

Some documentation on dealing with Windows SDK installation issues can be found
on the related pg_build_win page:
https://github.com/2ndQuadrant/pg_build_win#troubleshooting
     

***********************************************************************/
