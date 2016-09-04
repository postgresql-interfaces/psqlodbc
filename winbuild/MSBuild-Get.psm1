function Get-MSBuild([ref]$VCVersion, [ref]$MSToolsVersion, [ref]$Toolset, $configInfo)
{
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
		$msbver = invoke-expression "msbuild /ver /nologo"
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
