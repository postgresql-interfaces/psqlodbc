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
.PARAMETER RedistUCRT
    Specify when you'd like to redistribute Visual C++ 2015(or later) Redistributable.
.PARAMETER NoPDB
    Specify when you'd rather not include PDB files.
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
[switch]$RedistUCRT,
[switch]$NoPDB,
[string]$BuildConfigPath
)

[int]$ucrt_version=14
[String]$str_msvcr="msvcr"
[String]$str_vcrun="vcruntime"
[String]$str_msvcp="msvcp"
[String]$msrun_ptn="msvcr|vcruntime"

function msvcrun([int]$runtime_version)
{
	[String]$str = if ($runtime_version -lt $ucrt_version) {$str_msvcr} else {$str_vcrun}
	return $str
}

function env_vcversion_no()
{
	$viver = $env:VisualStudioVersion
	if ("$viver" -ne "") {
		if ("$viver" -match "(\d+)\.0") {
			return [int]$matches[1]
		}
	}
	return 0
}

function toolset_no_to_runtimeversion([int]$toolset_no)
{
	return [int] ($toolset_no / 10)
}

function runtimeversion_to_toolset_no([int]$runtime_version)
{
	[int]$toolset_no = $runtime_version * 10
	if ($runtime_version -eq 14) {	# possibly be v141
		[int]$vc_ver = 15
		if ((env_vcversion_no) -eq $vc_ver) {	# v141
			$toolseto++
		} elseif ((Find-VSDir $vc_ver) -ne "") {	# v141
			$toolset_no++
		}
	}
	return $toolset_no
}

function toolset_no_to_vcversion([int]$toolset_no)
{
	return [int] ($toolset_no / 10 + $toolset_no % 10)
}

function findRuntime([int]$toolset_no, [String]$pgmvc)
{
	$runtime_version = toolset_no_to_runtimeversion($toolset_no)
	$vcversion_no = toolset_no_to_vcversion($toolset_no)
	# where's the dll? 
	[String]$rt_dllname = (msvcrun $runtime_version) + "${runtime_version}0.dll"
	if ("$pgmvc" -ne "") {
		$dllspecified = "${pgmvc}\${rt_dllname}"
		if (Test-Path -Path $dllspecified) {
			return $dllspecified, ""
		}
	}
	$dllinredist = "${LIBPQBINDIR}\${rt_dllname}"
	if (Test-Path -Path $dllinredist) {
		return $dllinredist, ""
	}
	if ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
		$pgmvc = "$env:ProgramFiles"
	} else {
		$pgmvc = "${env:ProgramFiles(x86)}"
	}
	$dllinredist = ""
	$vsdir = Find-VSDir $vcversion_no
	if ("$vsdir" -ne "") {
		if ($vcversion_no -gt 14) {	# VC15 ~ VC??
			$lslist = @(Get-ChildItem "${vsdir}VC\Redist\MSVC\*\${CPUTYPE}\Microsoft.VC${toolset_no}.CRT\${rt_dllname}" -ErrorAction SilentlyContinue)
			if ($lslist.Count -gt 0) {
				$dllinredist = $lslist[0].FullName
			}
		} else {			# VC10 ~ VC14
			$dllinredist = "${vsdir}VC\redist\${CPUTYPE}\Microsoft.VC${toolset_no}.CRT\${rt_dllname}"
		}
	}
	if (("$dllinredist" -ne "") -and (Test-Path -Path $dllinredist)) {
		return $dllinredist, ""
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
	return "", $rt_dllname
}

function buildInstaller([string]$CPUTYPE)
{
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
	$runtime_list = @()
	if (-not $ExcludeRuntime) {
		$toolset = $configInfo.Configuration.BuildResult.PlatformToolset
		if ($toolset -match "^v(\d+)") {
			$toolset_no0 = [int]$matches[1]
		} else {
			$toolset_no0 = 100
		}
		$runtime_version0 = toolset_no_to_runtimeversion($toolset_no0)
		# where's the msvc runtime dll psqlodbc links?
		if ($runtime_version0 -ge $ucrt_version -and $RedistUCRT) {
			$script:wRedist=$true
		} else {
			$dlls=findRuntime $toolset_no0 $pgmvc
			$PODBCMSVCDLL=$dlls[0]
			if ("$PODBCMSVCDLL" -ne "") {
				Write-Host "psqlodbc picks $PODBCMSVCDLL"
				$runtime_list += $PODBCMSVCDLL
			}
			$PODBCMSVCSYS=$dlls[1]
			if ("$PODBCMSVCSYS" -ne "") {
				Write-Host "psqlodbc picks system $PODBCMSVCSYS"
				$runtime_list += $PODBCMSVCSYS
			}
			$PODBCMSVPDLL=$PODBCMSVCDLL.Replace((msvcrun $runtime_version0), $str_msvcp)
			if ("$PODBCMSVPDLL" -ne "") {
				$runtime_list += $PODBCMSVPDLL
			}
			$PODBCMSVPSYS=$PODBCMSVCSYS.Replace((msvcrun $runtime_version0), $str_msvcp)
			if ("$PODBCMSVPSYS" -ne "") {
				$runtime_list += $PODBCMSVPSYS
			}
		}
		# where's the runtime dll libpq links? 
		$msvclist=& ${dumpbinexe} /imports $LIBPQBINDIR\libpq.dll | select-string -pattern "^\s*($msrun_ptn)(\d+)0\.dll" | % {$_.Matches.Groups[2].Value}
		if ($msvclist -ne $Null -and $msvclist.length -gt 0) {
			if ($msvclist.GetType().Name -eq "String") {
				$runtime_version1=[int]$msvclist
			} else {
				$runtime_version1=[int]$msvclist[0]
			}
			if ($runtime_version1 -eq $runtime_version0) {
				$toolset_no1 = $toolset_no0
			} else {
				$toolset_no1 = runtimeversion_to_toolset_no($runtime_version1)
			}
			if ($runtime_version1 -ge $ucrt_version -and $RedistUCRT) {
				$script:wRedist=$true
			} elseif ($runtime_version1 -ne $runtime_version0) {
				$dlls=findRuntime $toolset_no1 $pgmvc
				$LIBPQMSVCDLL=$dlls[0]
				if ("$LIBPQMSVCDLL" -ne "") {
					Write-Host "LIBPQ picks $LIBPQMSVCDLL"
					$runtime_list += $LIBPQMSVCDLL
				}
				$LIBPQMSVCSYS=$dlls[1]
				if ("$LIBPQMSVCSYS" -ne "") {
					Write-Host "LIBPQ picks system $LIBPQMSVCSYS"
					$runtime_list += $LIBPQMSVCSYS
				}
			}
		} else {
			$script:wRedist=$true
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
		if ($runtime_list -contains $libpqmem[$i]) {
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

	[string []]$libpqRelArgs=@()
	for ($i=0; $i -lt $maxmem; $i++) {
		$libpqRelArgs += ("-dLIBPQMEM$i=" + $libpqmem[$i])
	}

	if (-not(Test-Path -Path $CPUTYPE)) {
		New-Item -ItemType directory -Path $CPUTYPE | Out-Null
	}

	$PRODUCTCODE = [GUID]::NewGuid()
	Write-Host "PRODUCTCODE: $PRODUCTCODE"

	try {
		pushd "$scriptPath"

		Write-Host ".`nBuilding psqlODBC/$SUBLOC merge module..."
		$BINBASE = GetObjbase ".."
		$INSTBASE = GetObjbase ".\$CPUTYPE" "installer\$CPUTYPE"
		candle -nologo $libpqRelArgs "-dPlatform=$CPUTYPE" "-dVERSION=$VERSION" "-dSUBLOC=$SUBLOC" "-dLIBPQBINDIR=$LIBPQBINDIR" "-dLIBPQMSVCDLL=$LIBPQMSVCDLL" "-dLIBPQMSVCSYS=$LIBPQMSVCSYS" "-dPODBCMSVCDLL=$PODBCMSVCDLL" "-dPODBCMSVPDLL=$PODBCMSVPDLL" "-dPODBCMSVCSYS=$PODBCMSVCSYS" "-dPODBCMSVPSYS=$PODBCMSVPSYS" "-dNoPDB=$NoPDB" "-dBINBASE=$BINBASE" -o $INSTBASE\psqlodbcm.wixobj psqlodbcm_cpu.wxs
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to build merge module"
		}

		Write-Host ".`nLinking psqlODBC merge module..."
		light -nologo -o $INSTBASE\psqlodbc_$CPUTYPE.msm $INSTBASE\psqlodbcm.wixobj
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to link merge module"
		}

		Write-Host ".`nBuilding psqlODBC installer database..."

		candle -nologo "-dPlatform=$CPUTYPE" "-dVERSION=$VERSION" "-dSUBLOC=$SUBLOC" "-dPRODUCTCODE=$PRODUCTCODE" "-dINSTBASE=$INSTBASE" -o $INSTBASE\psqlodbc.wixobj psqlodbc_cpu.wxs
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to build installer database"
		}

		Write-Host ".`nLinking psqlODBC installer database..."
		light -nologo -ext WixUIExtension -cultures:en-us -o $INSTBASE\psqlodbc_$CPUTYPE.msi $INSTBASE\psqlodbc.wixobj
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to link installer database"
		}

		Write-Host ".`nModifying psqlODBC installer database..."
		cscript modify_msi.vbs $INSTBASE\psqlodbc_$CPUTYPE.msi
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
$modulePath="${scriptPath}\..\winbuild"
Import-Module ${modulePath}\Psqlodbc-config.psm1

$defaultConfigDir=$modulePath
$configInfo = LoadConfiguration $BuildConfigPath $defaultConfigDir

if ($AlongWithDrivers) {
	try {
		pushd "$scriptpath"
		$platform = $cpu
		if ($cpu -eq "x86") {
			$platform = "win32"
		}
		..\winbuild\BuildAll.ps1 -Platform $platform -BuildConfigPath "$BuildConfigPath"
		if ($LASTEXITCODE -ne 0) {
			throw "Failed to build binaries"
		}
	} catch [Exception] {
		if ("$_.Exception.Message" -ne "") {
			Write-Host ("Error: " + $_.Exception.Message) -ForegroundColor Red
		} else {
			echo $_.Exception | Format-List -Force
		}
		Remove-Module Psqlodbc-config
		return
	} finally {
		popd
	} 
}

Import-Module ${scriptPath}\..\winbuild\MSProgram-Get.psm1
try {
	if ($configInfo.Configuration.BuildResult.Date -eq "") {
		Write-Host "!! Driver dlls haven't been built yet !!"
		Write-Host "!! Please build driver dlls first !!"
		return
	}

	$dumpbinexe = Find-Dumpbin

	$wRedist=$false
	$VERSION = GetPackageVersion $configInfo "$scriptPath/.."
	if ($cpu -eq "both") {
		buildInstaller "x86"
		buildInstaller "x64"
		Write-Host "wRedist=$wRedist"
		Remove-Module Psqlodbc-config
		try {
			pushd "$scriptPath"
			psqlodbc-setup\buildBootstrapper.ps1 -version $VERSION -withRedist:$wRedist
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
		Remove-Module Psqlodbc-config
	}
} catch [Exception] {
	if ("$_.Exception.Message" -ne "") {
		Write-Host ("Error: " + $_.Exception.Message) -ForegroundColor Red
	} else {
		echo $_.Exception | Format-List -Force
	}
	return
} finally {
	Remove-Module MSProgram-Get
	if (Get-Module Psqlodbc-config) {
		Remove-Module Psqlodbc-config
	}
}
