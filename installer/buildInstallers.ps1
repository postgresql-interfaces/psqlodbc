<#
.SYNOPSIS
    Build all installers of psqlodbc project.
.DESCRIPTION
    Build psqlodbc_x86.msi(msm), psqlodbc_x64.msi(msm).
.PARAMETER cpu
    Specify build cpu type, "both"(default), "x86" or "x64" is
    available.
.PARAMETER AlongWithDrivers
    Specify when you'd like to build drivers before building installers.
.PARAMETER ExcludeRuntime
    Specify when you'd like to exclude a msvc runtime dll from the installer.
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
[switch]$AlongWithDrivers,
[switch]$ExcludeRuntime,
[string]$BuildConfigPath
)

function findRuntime($runtime_version, $pgmvc)
{
	# where's the dll? 
	$rt_dllname="msvcr${runtime_version}0.dll"
	if ("$pgmvc" -ne "") {
		$dllspecified = "${pgmvc}\${rt_dllname}"
		if (Test-Path -Path $dllspecified) {
			$dllspecified
			return ""
		}
	}
	$dllinredist = "${LIBPQBINDIR}\${rt_dllname}"
	if (Test-Path -Path $dllinredist) {
		$dllinredist
		return ""
	}
	if ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
		$pgmvc = "$env:ProgramFiles"
	} else {
		$pgmvc = "${env:ProgramFiles(x86)}"
	}
	$dllinredist = "$pgmvc\Microsoft Visual Studio ${runtime_version}.0\VC\redist\${CPUTYPE}\Microsoft.VC${runtime_version}0.CRT\${rt_dllname}"
	if (Test-Path -Path $dllinredist) {
		$dllinredist
		return ""
	} else {
		$messageSpec = "Please specify Configuration.$CPUTYPE.runtime_folder element of the configuration file where msvc runtime dll $rt_dllname can be found"
		if ($CPUTYPE -eq "x86") {
			if ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
				$pgmvc = "${env:SystemRoot}\system32"
			} else {
				$pgmvc = "${env:SystemRoot}\syswow64"
			}
		} else {
			if ($env:PROCESSOR_ARCHITECTURE -eq "AMD64") {
				$pgmvc = "${env:SystemRoot}\system32"
			} elseif ($env:PROCESSOR_ARCHITEW6432 -eq "AMD64") {
				$pgmvc = "${env:SystemRoot}\sysnative"
			} else {
				throw "${messageSpec}`n$dllinredist doesn't exist unfortunately"
			}
		}
		$dllinsystem = "${pgmvc}\${rt_dllname}"
		if (-not(Test-Path -Path $dllinsystem)) {
			throw "${messageSpec}`nneither $dllinredist nor $dllinsystem exists unfortunately"
		}
	}
	""
	return $rt_dllname
}

function buildInstaller($CPUTYPE)
{
	$VERSION = $configInfo.Configuration.version

	$LIBPQBINDIR=getPGDir $configInfo $CPUTYPE "bin"
	# msvc runtime psqlodbc links
	$PODBCMSVCDLL = ""
	$PODBCMSVPDLL = ""
	$PODBCMSVCSYS = ""
	$PODBCMSVPSYS = ""
	# msvc runtime libpq links
	$LIBPQMSVCDLL = ""
	$LIBPQMSVCSYS = ""
	$pgmvc = $configInfo.Configuration.$CPUTYPE.runtime_folder
	if (-not $ExcludeRuntime) {
		$toolset = $configInfo.Configuration.BuildResult.PlatformToolset
		if ($toolset -match "^v(\d+)0") {
			$runtime_version0 = $matches[1]
		} else {
			$runtime_version0 = "10"
		}
		# where's the msvc runtime dll psqlodbc links?
		if ([int] $runtime_version0 -lt 14) { 
			$dlls=findRuntime $runtime_version0 $pgmvc
			$PODBCMSVCDLL=$dlls[0]
			$PODBCMSVPDLL=$PODBCMSVCDLL.Replace("msvcr", "msvcp")
			$PODBCMSVCSYS=$dlls[1]
			$PODBCMSVPSYS=$PODBCMSVCSYS.Replace("msvcr", "msvcp")
		}
		# where's the runtime dll libpq links? 
		$msvclist=invoke-expression -command "& `"${dumpbinexe}`" /imports `"$LIBPQBINDIR\libpq.dll`""| select-string -pattern "^\s*msvcr(\d+)0\.dll" | % {$_.matches[0].Groups[1].Value}
		if ($msvclist -ne $Null -and $msvclist.length -gt 0) {
			if ($msvclist.GetType().Name -eq "String") {
				$runtime_version1=$msvclist
			} else {
				$runtime_version1=$msvclist[0]
			}
			if ($runtime_version1 -ne $runtime_version0) {
				$dlls=findRuntime $runtime_version1 $pgmvc
				$LIBPQMSVCDLL=$dlls[0]
				$LIBPQMSVCSYS=$dlls[1]
				Write-Host "LIBPQ requires msvcr${runtime_version1}0.dll"
			}
		}
	}

	Write-Host "CPUTYPE    : $CPUTYPE"
	Write-Host "VERSION    : $VERSION"
	Write-Host "LIBPQBINDIR: $LIBPQBINDIR"

	if ($env:WIX -ne "")
	{
		$wix = "$env:WIX"
		$env:Path += ";$WIX/bin"
	}
	# The subdirectory to install into
	$SUBLOC=$VERSION.substring(0, 2) + $VERSION.substring(3, 2)

	#
	$maxmem=10
	$libpqmem=Get-RelatedDlls "libpq.dll" $LIBPQBINDIR
	for ($i=0; $i -lt $libpqmem.length; ) {
		if ($libpqmem[$i] -match "^msvcr\d+0.dll") {
			$libpqmem[$i]=$Null	
		} else {
			$i++
		}
	}
	if ($libpqmem.length -gt $maxmem) {
		throw("number of libpq related dlls exceeds $maxmem")
	}
	for ($i=$libpqmem.length; $i -lt $maxmem; $i++) {
		$libpqmem += ""
	}
	$addpara = " `"-dLIBPQMEM0=" + $libpqmem[0] + `
		"`" `"-dLIBPQMEM1=" + $libpqmem[1] + `
		"`" `"-dLIBPQMEM2=" + $libpqmem[2] + `
		"`" `"-dLIBPQMEM3=" + $libpqmem[3] + `
		"`" `"-dLIBPQMEM4=" + $libpqmem[4] + `
		"`" `"-dLIBPQMEM5=" + $libpqmem[5] + `
		"`" `"-dLIBPQMEM6=" + $libpqmem[6] + `
		"`" `"-dLIBPQMEM7=" + $libpqmem[7] + `
		"`" `"-dLIBPQMEM8=" + $libpqmem[8] + `
		"`" `"-dLIBPQMEM9=" + $libpqmem[9] + "`""

	if (-not(Test-Path -Path $CPUTYPE)) {
		New-Item -ItemType directory -Path $CPUTYPE | Out-Null
	}

	$PRODUCTCODE = [GUID]::NewGuid();
	Write-Host "PRODUCTCODE: $PRODUCTCODE"

	try {
		pushd "$scriptPath"

		Write-Host ".`nBuilding psqlODBC/$SUBLOC merge module..."

		invoke-expression "candle -nologo -dPlatform=$CPUTYPE `"-dVERSION=$VERSION`" -dSUBLOC=$SUBLOC `"-dLIBPQBINDIR=$LIBPQBINDIR`" `"-dLIBPQMSVCDLL=$LIBPQMSVCDLL`" `"-dLIBPQMSVCSYS=$LIBPQMSVCSYS`" `"-dPODBCMSVCDLL=$PODBCMSVCDLL`" `"-dPODBCMSVPDLL=$PODBCMSVPDLL`" `"-dPODBCMSVCSYS=$PODBCMSVCSYS`" `"-dPODBCMSVPSYS=$PODBCMSVPSYS`" $addpara -o $CPUTYPE\psqlodbcm.wixobj psqlodbcm_cpu.wxs"
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

		Write-Host ".`nDone!`n"
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

if ($AlongWithDrivers) {
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

Import-Module ${scriptPath}\..\winbuild\MSBuild-Get.psm1
try {
	$dumpbinexe = Find-Dumpbin

	if ($cpu -eq "both") {
		buildInstaller "x86"
		buildInstaller "x64"
		$VERSION = $configInfo.Configuration.version
		try {
			pushd "$scriptPath"
			invoke-expression "psqlodbc-setup\buildBootstrapper.ps1 -version $VERSION" -ErrorAction Stop
			if ($LASTEXITCODE -ne 0) {
				throw "Failed to build bootstrapper"
			}
		} catch [Exception] {
			throw $error[0]
		} finally {
			popd
		} 
	}
	else {
		buildInstaller $cpu
	}
} catch [Exception] {
	throw $error[0]
}
