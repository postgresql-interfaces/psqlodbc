function Find-MSBuild([ref]$VCVersion, [ref]$MSToolsVersion, [ref]$Toolset, $configInfo)
{
	$msbuildexe=""
	$VisualStudioVersion=$VCVersion.Value
	$MSToolsVersionv=$MSToolsVersion.Value
	$Toolsetv=$Toolset.Value

	$WSDK71Set="Windows7.1SDK"
	$refnum=""
	Write-Debug "VCVersion=$VCVersionv $env:VisualStudioVersion"
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
#	neither C++ nor SDK prompt
	if ("$VisualStudioVersion" -eq "") {
		if ("${env:VS120COMNTOOLS}" -ne "") { # VC12 is installed (current official)
			$VisualStudioVersion = "12.0"
		} elseif ("${env:VS100COMNTOOLS}" -ne "") { # VC10 is installed
			$VisualStudioVersion = "10.0"
		} elseif ("${env:VS140COMNTOOLS}" -ne "") { # VC14 is installed
			$VisualStudioVersion = "14.0"
		} elseif ("${env:VS110COMNTOOLS}" -ne "") { # VC11 is installed
			$VisualStudioVersion = "11.0"
		} else {
			Write-Error "Visual Studio >= 10.0 not found" -Category InvalidArgument;return
		}
	} elseif ([int]::TryParse($VisualStudioVersion, [ref]$refnum)) {
		$VisualStudioVersion="${refnum}.0"
	}
#	Check VisualStudioVersion and prepare for ToolsVersion
	switch ($VisualStudioVersion) {
	 "10.0"	{ $tv = "4.0" }
	 "11.0"	{ $tv = "4.0" }
	 "12.0"	{ $tv = "12.0" }
	 "14.0"	{ $tv = "14.0" }
	 default { Write-Error "Selected Visual Stuidio is Version ${VisualStudioVersion}. Please use VC10 or later" -Category InvalidArgument; return }
	}
#
#	Determine ToolsVersion
#
	if ("$MSToolsVersionv" -eq "") {
		$MSToolsVersionv=$env:ToolsVersion
	}
	if ("$MSToolsVersionv" -eq "") {
		 $MSToolsVersionv = $tv
	} elseif ([int]::TryParse($MSToolsVersionv, [ref]$refnum)) {
		$MSToolsVersionv="${refnum}.0"
	}
#
#	Determine MSBuild executable
#
	Write-Debug "ToolsVersion=$MSToolsVersionv VisualStudioVersion=$VisualStudioVersion"
	try {
		$msbver = msbuild.exe /ver /nologo
		if ("$msbver" -match "^(\d+)\.(\d+)") {
			$major1 = [int] $matches[1]
			$minor1 = [int] $matches[2]
			if ($MSToolsVersionv -match "^(\d+)\.(\d+)") {
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
		$msbindir=""
		$regKey="HKLM:\Software\Wow6432Node\Microsoft\MSBuild\ToolsVersions\${MSToolsVersionv}"
		if (Test-Path -path $regkey) {
			$msbitem=Get-ItemProperty $regKey
			if ($msbitem -ne $null) {
				$msbindir=$msbitem.MSBuildToolsPath
			}
		} else {
			$regKey="HKLM:\Software\Microsoft\MSBuild\ToolsVersions\${MSToolsVersionv}"
			if (Test-Path -path $regkey) {
				$msbitem=Get-ItemProperty $regKey
				if ($msbitem -ne $null) {
					$msbindir=$msbitem.MSBuildToolsPath
				}
			} else {
				Write-Error "MSBuild ToolsVersion $MSToolsVersionv not Found" -Category NotInstalled; return
			}
		}
		$msbuildexe = "$msbindir\msbuild"
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
		}
	}
#	avoid a bug of Windows7.1SDK PlatformToolset
	if ($Toolsetv -eq $WSDK71Set) {
		$env:TARGET_CPU=""
	}
#
	$VCVersion.value=$VisualStudioVersion
	$MSToolsVersion.value=$MSToolsVersionv
	$Toolset.value=$Toolsetv

	return $msbuildexe
}

$dumpbinexe = ""
$addPath=""

function Find-Dumpbin($CurMaxVC = 14)
{
	if ("$dumpbinexe" -ne "") {
		if ("$addPath" -ne "") {
			if (${env:PATH}.indexof($addPath) -lt 0) {
				$env:PATH = "${addPath};" + $env:PATH
			}
		}
		return $dumpbinexe
	}
	$addPath=""
	try {
		$dum = dumpbin.exe /NOLOGO
		$dumpbinexe="dumpbin"
	} catch [Exception] {
#		$dumpbinexe="$env:DUMPBINEXE"
		if ($dumpbinexe -eq "") {
			if ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
				$pgmfs = "$env:ProgramFiles"
			} else {
				$pgmfs = "${env:ProgramFiles(x86)}"
			}
			for ($i = $CurMaxVc; $i -lt 20; )
			{
				$dumpbinexe="$pgmfs\Microsoft Visual Studio ${i}.0\VC\bin\dumpbin.exe"
				if (Test-Path -Path $dumpbinexe) {
					break
				}
				if ($i -le 10) {
					$i = $CurMaxVc + 1
				} elseif ($i -le $CurMaxVc) {
					$i--
				} else {
					$i++
				}
			}
			if ($i -ge 20) {
				throw "dumpbin doesn't exist"
			}
			elseif ($i -eq 10) {
				$addPath = "$pgmfs\Microsoft Visual Studio ${i}.0\Common7\ide"
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

function dumpbinRecurs($dllname, $dllfolder, $instarray)
{
	$tmem=& ${dumpbinexe} /imports "$dllfolder\${dllname}" | select-string -pattern "^\s*(\S*\.dll)" | % {$_.matches[0].Groups[1].Value} | where-object {test-path ("${dllfolder}\" + $_)}
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

function Get-RelatedDlls($dllname, $dllfolder)
{
	Find-Dumpbin | Out-Null
	$libpqmem=@()
	$libpqmem=dumpbinRecurs $dllname $dllfolder $libpqmem

	return $libpqmem
}

Export-ModuleMember -function Find-MSBuild, Find-Dumpbin, Get-RelatedDlls
