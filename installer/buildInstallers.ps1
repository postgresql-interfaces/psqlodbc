<#
.SYNOPSIS
    Build all installers of psqlodbc project.
.DESCRIPTION
    Build psqlodbc_x86.msi(msm), psqlodbc_x64.msi(msm).
.PARAMETER cpu
    Specify build cpu type, "both"(default), "x86" or "x64" is
    available.
.PARAMETER BuildBinaries
    Specify wherther build binary dlls or not, $FALSE(default) or $TRUE is
    available.
.PARAMETER BuildConfigPath
    Specify the configuration xml file name if you want to use
    the configuration file other than standard one.
    The relative path is relative to the current directory.
.EXAMPLE
    > .\buildInstallers
	Build 32bit and 64bit installers.
.EXAMPLE
    > .\buildInstallers x86
	Build 32bit installers.
.NOTES
    Author: Hiroshi Inoue
    Date:   July 4, 2014
#>
#	build 32bit and/or 64bit installers
#
Param(
[ValidateSet("x86", "x64", "both")]
[string]$cpu="both",
[Boolean]$BuildBinaries=$FALSE,
[string]$BuildConfigPath
)

function buildInstaller($CPUTYPE)
{
	$VERSION = $configInfo.Configuration.version

	$archinfo = $configInfo.Configuration.$CPUTYPE

	$LIBPQVER=$archinfo.libpq.version
	if ($LIBPQVER -eq "") {
		$LIBPQVER=$LIBPQ_VERSION
	}

	$USE_LIBPQ=$archinfo.use_libpq

	if ($CPUTYPE -eq "x64")
	{
		if ($USE_LIBPQ -eq "yes")
		{
			$LIBPQBINDIR=$archinfo.libpq.bin
			if ($LIBPQBINDIR -eq "default") {
				if ($env:PROCESSOR_ARCHITECTURE -ne "x86") {
					$pgmfs = "$env:ProgramFiles"
					$LIBPQBINDIR = "$pgmfs\PostgreSQL\$LIBPQVER\bin"
				}
				elseif ("${env:ProgramW6432}" -ne "") {
					$pgmfs = "$env:ProgramW6432"
					$LIBPQBINDIR = "$pgmfs\PostgreSQL\$LIBPQVER\bin"
				}
			}
		}
	
	}
	elseif ($CPUTYPE -eq "x86")
	{
		if ($USE_LIBPQ -eq "yes")
		{
			$LIBPQBINDIR=$archinfo.libpq.bin
			if ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
				$pgmfs = "$env:ProgramFiles"
			} else {
				$pgmfs = "${env:ProgramFiles(x86)}"
			}
			if ($LIBPQBINDIR -eq "default") {
				$LIBPQBINDIR = "$pgmfs\PostgreSQL\$LIBPQVER\bin"
			}
		}

	}
	else
	{
		throw "Unknown CPU type $CPUTYPE";
	}

	$USE_SSPI=$archinfo.use_sspi

	$USE_GSS=$archinfo.use_gss
	if ($USE_GSS -eq "yes")
	{
		$GSSBINDIR=$archinfo.gss.bin
	}

	Write-Host "CPUTYPE    : $CPUTYPE"
	Write-Host "VERSION    : $VERSION"
	Write-Host "USE LIBPQ  : $USE_LIBPQ ($LIBPQBINDIR)"
	Write-Host "USE GSS    : $USE_GSS ($GSSBINDIR)"
	Write-Host "USE SSPI   : $USE_SSPI"

	if ($env:WIX -ne "")
	{
		$wix = "$env:WIX"
		$env:Path += ";$WIX/bin"
	}
	# The subdirectory to install into
	$SUBLOC=$VERSION.substring(0, 2) + $VERSION.substring(3, 2)

	if (-not(Test-Path -Path $CPUTYPE)) {
    		New-Item -ItemType directory -Path $CPUTYPE
	}

	$PRODUCTCODE = [GUID]::NewGuid();
	Write-Host "PRODUCTCODE: $PRODUCTCODE"

	try {
		pushd "$scriptPath"

		Write-Host ".`nBuilding psqlODBC/$SUBLOC merge module..."

		invoke-expression "candle -nologo -dPlatform=$CPUTYPE `"-dVERSION=$VERSION`" -dSUBLOC=$SUBLOC `"-dLIBPQBINDIR=$LIBPQBINDIR`" `"-dGSSBINDIR=$GSSBINDIR`" -o $CPUTYPE\psqlodbcm.wixobj psqlodbcm_cpu.wxs"
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to build merge module"
		}

		Write-Host ".`nLinking psqlODBC merge module..."
		invoke-expression "light -nologo -o $CPUTYPE\psqlodbc_$CPUTYPE.msm $CPUTYPE\psqlodbcm.wixobj"
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to link merge module"
		}

		Write-Host ".`nBuilding psqlODBC installer database..."

		invoke-expression "candle -nologo -dPlatform=$CPUTYPE `"-dVERSION=$VERSION`" -dSUBLOC=$SUBLOC `"-dPRODUCTCODE=$PRODUCTCODE`" -o $CPUTYPE\psqlodbc.wixobj psqlodbc_cpu.wxs"
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to build installer database"
		}

		Write-Host ".`nLinking psqlODBC installer database..."
		invoke-expression "light -nologo -ext WixUIExtension -cultures:en-us -o $CPUTYPE\psqlodbc_$CPUTYPE.msi $CPUTYPE\psqlodbc.wixobj"
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to link installer database"
		}

		Write-Host ".`nModifying psqlODBC installer database..."
		invoke-expression "cscript modify_msi.vbs $CPUTYPE\psqlodbc_$CPUTYPE.msi"
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to modify installer database"
		}

		Write-Host ".`nDone!"
	}
	catch [Exception] {
		Write-Host ".`Aborting build!"
		throw $error[0]
	}
	finally {
		popd
	}
}

$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
$configInfo = & "$scriptPath\..\winbuild\configuration.ps1" "$BuildConfigPath"

if ($BuildBinaries) {
	try {
		pushd "$scriptpath"
		$platform = $cpu
		if ($cpu -eq "x86") {
			$platform = "win32"
		}
		invoke-expression "..\winbuild\BuildAll.ps1 -Platform $platform -BuildConfigPath `"$BuildConfigPath`"" -ErrorAction Stop
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to build binaries"
		}
	} catch [Exception] {
		throw $error[0]
	} finally {
		popd
	} 
}

if ($cpu -eq "both") {
	buildInstaller "x86"
	buildInstaller "x64"
}
else {
	buildInstaller $cpu
}
