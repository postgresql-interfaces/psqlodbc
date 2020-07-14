
function Find-MSBuild
{
    [CmdletBinding()]

    Param([Parameter(Mandatory=$true)]
          [ref]$VCVersion,
          [Parameter(Mandatory=$true)]
          $MSToolsVersion,
          [Parameter(Mandatory=$true)]
          [ref]$Toolset,
          [Parameter(Mandatory=$true)]
          [xml]$configInfo)

	$msbuildexe=""
	$VisualStudioVersion=$VCVersion.Value
	$Toolsetv=$Toolset.Value

	$WSDK71Set="Windows7.1SDK"
	Write-Debug "VCVersion=$VCVersionv $env:VisualStudioVersion"
	[int]$toolsnum=0
#
#	Determine VisualStudioVersion
#
	if (("$VisualStudioVersion" -eq "") -And ($configInfo -ne $null)) {
		$VisualStudioVersion=$configInfo.Configuration.vcversion
	}
	if ("$VisualStudioVersion" -eq "") {
		$VisualStudioVersion = $env:VisualStudioVersion # VC11 or later version of C++ command prompt sets this variable
	}
	if ("$VisualStudioVersion" -eq "") {
		if ("${env:WindowsSDKVersionOverride}" -eq "v7.1") { # SDK7.1+ command prompt
			$VisualStudioVersion = "10.0"
		} elseif ("${env:VCInstallDir}" -ne "") { # C++ command prompt
			if ("${env:VCInstallDir}" -match "Visual Studio\s*(\S+)\\VC\\$") {
				$VisualStudioVersion = $matches[1]
			}
		}
	}
	$refnum=""
#	neither C++ nor SDK prompt
	if ("$VisualStudioVersion" -eq "") {
		if ((Find-VSDir 15) -ne "") {	# VC15 is installed (current official)
			$VisualStudioVersion = "15.0"
		} elseif ((Find-VSDir 16) -ne "") {	# VC16 is installed
			$VisualStudioVersion = "16.0"
		} elseif ("${env:VS140COMNTOOLS}" -ne "") { # VC14 is installed
			$VisualStudioVersion = "14.0"
		} elseif ("${env:VS120COMNTOOLS}" -ne "") { # VC12 is installed
			$VisualStudioVersion = "12.0"
		} elseif ("${env:VS100COMNTOOLS}" -ne "") { # VC10 is installed
			$VisualStudioVersion = "10.0"
		} elseif ("${env:VS110COMNTOOLS}" -ne "") { # VC11 is installed
			$VisualStudioVersion = "11.0"
		} else {
			throw "Visual Studio >= 10.0 not found"
		}
	}
	if ([int]::TryParse($VisualStudioVersion, [ref]$refnum)) {
		$VisualStudioVersion="${refnum}.0"
	}
#	Check VisualStudioVersion and prepare for ToolsVersion
	switch ($VisualStudioVersion) {
	 "10.0"	{ $toolsout = 4 }
	 "11.0"	{ $toolsout = 4 }
	 "12.0"	{ $toolsout = 12 }
	 "14.0"	{ $toolsout = 14 }
	 "15.0"	{ $toolsout = 15 }
	 "16.0"	{ $toolsout = 16 }
	 default { throw "Selected Visual Stuidio is Version ${VisualStudioVersion}. Please use VC10 or later"}
	}
#
#	Determine ToolsVersion
#
	[int]$toolsnum=$toolsout
	if ("$MSToolsVersion" -eq "") {
		$MSToolsVersion=$env:ToolsVersion
	}
	if ("$MSToolsVersion" -ne "") {
		if ([int]::TryParse($MSToolsVersion, [ref]$refnum)) {
			$toolsout=[int]$refnum
		}
	}
	if ($toolsnum -lt 12) {
		$toolsnum=$toolsout
	}
#
#	Determine MSBuild executable
#
	Write-Debug "ToolsVersion=$MSToolsVersion VisualStudioVersion=$VisualStudioVersion"
	try {
		$msbver = msbuild.exe /ver /nologo
		if ("$msbver" -match "^(\d+)\.(\d+)") {
			$major1 = [int] $matches[1]
			$minor1 = [int] $matches[2]
			if ($MSToolsVersion -match "^(\d+)\.(\d+)") {
				$bavail = $false
				$major2 = [int] $matches[1]
				$minor2 = [int] $matches[2]
				if ($major1 -gt $major2) {
					Write-Debug "$major1 > $major2"
					$bavail = $true
				}
				elseif ($major1 -eq $major2 -And $minor1 -ge $minor2) {
					Write-Debug "($major1, $minor1) >= ($major2, $minor2)"
					$bavail = $true
				}
				if ($bavail) {
					$msbuildexe = "MSBuild"
				}
			}
		}
	} catch {}
	if ("$msbuildexe" -eq "") {
		if ($toolsnum -gt 14) {	# VC15 ~ VC16
			$msbuildexe = msbfind_15_xx $toolsnum
		} else {			# VC10 ~ VC14
			$msbuildexe = msbfind_10_14 "${toolsnum}.0"
			if ($toolsnum -eq 4) {
				if ((Find-VSDir $VisualStudioVersion) -eq "") {
					throw "MSBuild ${toolsnum}.0 found but VisualStudioVersion $VisualStudioVersion not Found"
				}
			}
		}
		if ("$msbuildexe" -eq "") {
			throw "MSBuild ToolsVersion ${toolsnum}.0 not Found"
		}
	}
#
#	Determine PlatformToolset
#
	if (("$Toolsetv" -eq "") -And ($configInfo -ne $null)) {
		$Toolsetv=$configInfo.Configuration.toolset
	}
	if ("$Toolsetv" -eq "") {
		$Toolsetv=$env:PlatformToolset
	}
	if ("$Toolsetv" -eq "") {
		switch ($VisualStudioVersion) {
		 "10.0"	{
				if (Test-path "HKLM:\Software\Microsoft\Microsoft SDKs\Windows\v7.1") {
					$Toolsetv=$WSDK71Set
				} else {
					$Toolsetv="v100"
				}
			}
		 "11.0"	{$Toolsetv="v110_xp"}
		 "12.0"	{$Toolsetv="v120_xp"}
		 "14.0"	{$Toolsetv="v140_xp"}
		 "15.0"	{$Toolsetv="v141_xp"}
		 "16.0"	{$Toolsetv="v142"}
		}
	}
#	avoid a bug of Windows7.1SDK PlatformToolset
	if ($Toolsetv -eq $WSDK71Set) {
		$env:TARGET_CPU=""
	}
#
	$VCVersion.value=$VisualStudioVersion
	$Toolset.value=$Toolsetv

	if ("$MSToolsVersion" -eq "") {
		if ([int]$toolsout -gt 15) {
			$MSToolsVersion="Current"
		} else {
			$MSToolsVersion="${toolsout}.0"
		}
	}
	return $msbuildexe, $MSToolsVersion
}

#	find msbuild.exe for VC10 ~ VC14
function msbfind_10_14
{
    [CmdletBinding()]

    Param([Parameter(Mandatory=$true)]
          [string]$toolsver)

	$msbindir=""
	$regKey="HKLM:\Software\Wow6432Node\Microsoft\MSBuild\ToolsVersions\${toolsver}"
	if (Test-Path -path $regkey) {
		$msbitem=Get-ItemProperty $regKey
		if ($msbitem -ne $null) {
			$msbindir=$msbitem.MSBuildToolsPath
		}
	} else {
		$regKey="HKLM:\Software\Microsoft\MSBuild\ToolsVersions\${toolsver}"
		if (Test-Path -path $regkey) {
			$msbitem=Get-ItemProperty $regKey
			if ($msbitem -ne $null) {
				$msbindir=$msbitem.MSBuildToolsPath
			}
		} else {
			return ""
		}
	}
	return "${msbindir}msbuild"
}

#	find msbuild.exe for VC15 ~ VC16
function msbfind_15_xx
{
    [CmdletBinding()]

    Param([Parameter(Mandatory=$true)]
          [string]$toolsver)

# via vssetup module
	$vsdir = find_vs_installation $toolsver
# vssetup module is unavailable
	return find_default_msbuild_path $toolsver $vsdir
}

$dumpbinexe = ""
$addPath=""

function Find-Dumpbin
{
    [CmdletBinding()]

    Param([int]$CurMaxVC = 16)

	if ("$dumpbinexe" -ne "") {
		if ("$addPath" -ne "") {
			if (${env:PATH}.indexof($addPath) -lt 0) {
				$env:PATH = "${addPath};" + $env:PATH
			}
		}
		return $dumpbinexe
	}
	$addPath=""
	$vsdir=""
	try {
		$dum = dumpbin.exe /NOLOGO
		$dumpbinexe="dumpbin"
	} catch [Exception] {
#		$dumpbinexe="$env:DUMPBINEXE"
		if ($dumpbinexe -eq "") {
			$searching = $true
			for ($i = $CurMaxVc; $searching -and ($i -ge 14); $i--)	# VC15 ~ VC16
			{
				$vsdir = Find-VSDir $i
				if ("$vsdir" -ne "") {
					$lslist = @(Get-ChildItem "${vsdir}VC\Tools\MSVC\*\bin\HostX86\x86\dumpbin.exe" -ErrorAction SilentlyContinue)
					if ($lslist.Count -gt 0) {
						$dumpbinexe=$lslist[0].FullName
						$searching = $false
						break
					}
				}
			}
			for (; $searching -and ($i -ge 10); $i--)	# VC10 ~ VC14
			{
				$vsdir = Find-VSDir $i
				if ("$vsdir" -ne "") {
					$dumpbinexe="${vsdir}VC\bin\dumpbin.exe"
					if (Test-Path -Path $dumpbinexe) {
						$searching = $false
						break
					}
				}
			}
			if ($searching) {
				throw "dumpbin doesn't exist"
			}
			elseif ($i -eq 10) {
				$addPath = "${vsdir}Common7\ide"
			}
		}
	}
	
	Write-Host "Dumpbin=$dumpbinexe"
	Set-Variable -Name dumpbinexe -Value $dumpbinexe -Scope 1
	Set-Variable -Name addPath -Value $addPath -Scope 1
	if ("$addPath" -ne "") {
		if (${env:PATH}.indexof($addPath) -lt 0) {
			$env:PATH = "${addPath};" + $env:PATH
		}
	}
	return $dumpbinexe
}

function dumpbinRecurs
{
    [CmdletBinding()]

    Param([Parameter(Mandatory=$true)]
          [string]$dllname,
          [Parameter(Mandatory=$true)]
          [string]$dllfolder,
          [array]$instarray)

	$tmem=& ${dumpbinexe} /imports "$dllfolder\${dllname}" | select-string -pattern "^\s*(\S*\.dll)" | foreach-object {$_.Matches.Groups[1].Value} | where-object {test-path ("${dllfolder}\" + $_)}
	if ($LASTEXITCODE -ne 0) {
		throw "Failed to dumpbin ${dllfolder}\${dllname}"
	}
	if ($tmem -eq $Null) {
		return $instarray
	}
	if ($tmem.GetType().Name -eq "String") {
		[String []]$tarray = @($tmem)
	} else {
		$tarray = $tmem
	}
	$iarray=@()
	for ($i=0; $i -lt $tarray.length; $i++) {
		if (-not($instarray -contains $tarray[$i])) {
			$iarray += $tarray[$i]
		}
	}
	if ($iarray.length -eq 0) {
		return $instarray
	}
	$instarray += $iarray
	for ($i=0; $i -lt $iarray.length; $i++) {
		$instarray=dumpbinRecurs $iarray[$i] $dllfolder $instarray
	}
	return $instarray
}

function Get-RelatedDlls
{
    [CmdletBinding()]

    Param([Parameter(Mandatory=$true)]
          [string]$dllname,
          [Parameter(Mandatory=$true)]
          [string]$dllfolder)

	Find-Dumpbin | Out-Null
	$libpqmem=@()
	$libpqmem=dumpbinRecurs $dllname $dllfolder $libpqmem

	return $libpqmem
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

function Find-VSDir
{
    [CmdletBinding()]

    Param([Parameter(Mandatory=$true)]
          [string]$vcversion)

	[int]$vcversion_no = [int]$vcversion
	if ("${vcversion}" -match "^(\d+)") {
		$vcversion_no = $matches[1]
	}
	if ((env_vcversion_no) -eq $vcversion_no) {
		return $env:VSINSTALLDIR
	}
	if ($vcversion_no -gt 14) {	# VC15 ~ VC16
		return find_vsdir_15_xx ${vcversion_no}
	} else {	# VC10 ~ VC14
		$comntools = [environment]::getenvironmentvariable("VS${vcversion_no}0COMNTOOLS")
		if ("$comntools" -eq "") {
			return ""
		}
		return (Split-Path (Split-Path $comntools -Parent) -Parent) + "\"
	}
}

[bool]$vssetup_available = $true
$vssetup = $null
#	find vs installation path for VC15 ~ VC16
function find_vs_installation
{
    [CmdletBinding()]

    Param([Parameter(Mandatory=$true)]
          [string]$toolsver)

	$vsdir = ""
# vssetup module is available?
	if ($vssetup_available -and ($vsseup -eq $null)) {
		try {
			$vssetup = @(Get-VssetupInstance)
		} catch [Exception] {
			$vssetup_available = $false
		}
	}
	$toolsnum = [int]$toolsver
	if ($vssetup -ne $null) {
		$lslist = @($vssetup | where-object { $_.InstallationVersion.Major -eq $toolsnum } | foreach-object { $_.InstallationPath })
		if ($lslist.Count -gt 0) {
			$vsdir = $lslist[0]
		}
	}
	return $vsdir
}

$vsarray = @{VC15 = "2017"; VC16 = "2019"}
#	find VS dir for VC15 ~ VC16
function find_default_msbuild_path
{
    [CmdletBinding()]

    Param([Parameter(Mandatory=$true)]
          [string]$toolsver,
          [string]$vsdir)

	if ($vsdir -eq "")
	{
		$toolsnum = [int]$toolsver
		if ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
			$pgmfs = "$env:ProgramFiles"
		} else {
			$pgmfs = "${env:ProgramFiles(x86)}"
		}
		$vsverdir = $vsarray["VC$toolsnum"]
		$lslist = @(Get-ChildItem "$pgmfs\Microsoft Visual Studio\$vsverdir\*\MSBuild\*\Bin\MSBuild.exe" -ErrorAction SilentlyContinue)
	} else {
		$lslist = @(Get-ChildItem "$vsdir\MSBuild\*\Bin\MSBuild.exe" -ErrorAction SilentlyContinue)
	}
	if ($lslist.Count -gt 0) {
		return $lslist[0].FullName
	}

	return ""
}

#	find VS dir for VC15 ~ VC16
function find_vsdir_15_xx
{
    [CmdletBinding()]

    Param([Parameter(Mandatory=$true)]
          [string]$toolsver)

# via vssetup module
	$vsdir = find_vs_installation $toolsver
	if ($vsdir -ne "") {
		return $vsdir
	}
# vssetup module is unavailable
	$msbpath = find_default_msbuild_path $toolsver $vsdir
	if ($msbpath -ne "") {
		return (Split-Path (Split-Path (Split-Path (Split-Path $msbpath -Parent) -Parent) -Parent) -Parent) + "\"
	}

	return ""
}

Export-ModuleMember -function Find-MSBuild, Find-Dumpbin, Get-RelatedDlls, Find-VSDir
