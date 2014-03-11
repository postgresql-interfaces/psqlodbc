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


  Please start a powershell console and

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
     you placed libpq/Openssl related include/lib files.

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

***********************************************************************/
