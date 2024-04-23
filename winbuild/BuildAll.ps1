<#
.SYNOPSIS
    Build all dlls of psqlodbc project using MSbuild.
.DESCRIPTION
    Build psqlodbc35w.dll, psqlodbc30a.dll, pgenlist.dll, pgenlista.dll
    and pgxalib.dll for both x86 and x64 platforms.
.PARAMETER Target
    Specify the target of MSBuild. "Build"(default), "Rebuild" or
    "Clean" is available.
.PARAMETER VCVersion
    Visual Studio version is determined automatically unless this
    option is specified.
.PARAMETER Platform
    Specify build platforms, "both"(default), "Win32" or "x64" is
    available.
.PARAMETER AlongWithInstallers
    Specify when you'd like to build installers after building drivers.
.PARAMETER Toolset
    MSBuild PlatformToolset is determined automatically unless this
    option is specified. Currently "v90", "v100", "Windows7.1SDK",
    "v110", "v110_xp", "v120", "v120_xp", "v140" , "v140_xp", "v141",
    "v141_xp" or "v142" is available.
.PARAMETER MSToolsVersion
    This option is deprecated. MSBuild ToolsVersion is detemrined
    automatically unless this option is specified. Currently "4.0",
    "12.0" or "14.0" is available.
.PARAMETER Configuration
    Specify "Release"(default) or "Debug".
.PARAMETER BuildConfigPath
    Specify the configuration xml file name if you want to use
    the configuration file other than standard one.
    The relative path is relative to the current directory.
.PARAMETER UseMimalloc
    Whether to use the mimalloc allocator for improved performance.
	Requires a toolset of v141, v142 or later. Specify "yes" or
	"no"(default).
.EXAMPLE
    > .\BuildAll
	Build with default or automatically selected parameters.
.EXAMPLE
    > .\BuildAll Clean
	Clean all generated files.
.EXAMPLE
    > .\BuildAll -V(CVersion) 11.0
	Build using Visual Studio 11.0 environment.
.EXAMPLE
    > .\BuildAll -P(latform) x64
	Build only 64bit dlls.
.EXAMPLE
    > .\BuildAll -A(longWithInstallers)
	Build installers as well after building drivers.
.NOTES
    Author: Hiroshi Inoue
    Date:   Febrary 1, 2014
#>

#
#	build 32bit & 64bit dlls for VC10 or later
#
Param(
[ValidateSet("Build", "Rebuild", "Clean", "info")]
[string]$Target="Build",
[string]$VCVersion,
[ValidateSet("Win32", "x64", "both")]
[string]$Platform="both",
[string]$Toolset,
[ValidateSet("", "4.0", "12.0", "14.0")]
[string]$MSToolsVersion,
[ValidateSet("Debug", "Release")]
[String]$Configuration="Release",
[string]$BuildConfigPath,
[switch]$AlongWithInstallers,
[ValidateSet("yes", "no")]
[string]$UseMimalloc="no"
)

function buildPlatform([xml]$configInfo, [string]$Platform)
{
	if ($Platform -ieq "x64") {
		$platinfo=$configInfo.Configuration.x64
	} else {
		$platinfo=$configInfo.Configuration.x86
	}
	$BUILD_MACROS=$platinfo.build_macros
	$PG_INC=getPGDir $configInfo $Platform "include"
	$PG_LIB=getPGDir $configInfo $Platform "lib"
	$PG_BIN=getPGDir $configInfo $Platform "bin"

	Write-Host "USE LIBPQ  : ($PG_INC $PG_LIB $PG_BIN)"

	if (-not (Test-Path $PG_INC)) {
		throw("`n!!!! include directory $PG_INC does not exist`nplease specify the correct folder name using editConfiguration")
	}
	if (-not (Test-Path $PG_LIB)) {
		throw("`n!!!! lib directory $PG_LIB does not exist`nplease specify the correct folder name using editConfiguration")
	}
	if (-not (Test-Path $PG_BIN)) {
		throw("`n!!!! bin directory $PG_BIN does not exist`nplease specify the correct folder name using editConfiguration")
	}
	
	$useSplit=$true
	if ($useSplit) {
			$macroList = -split $BUILD_MACROS
	} else {
		$BUILD_MACROS = $BUILD_MACROS -replace ';', '`;'
		$BUILD_MACROS = $BUILD_MACROS -replace '"', '`"'
		$macroList = iex "write-output $BUILD_MACROS"
	}

	if ($UseMimalloc -eq "yes") {
		switch ($VCVersion) {
			"10.0"	{ $mimallocIdeDir = "vs2017" }
			"11.0"	{ $mimallocIdeDir = "vs2017" }
			"12.0"	{ $mimallocIdeDir = "vs2017" }
			"14.0"	{ $mimallocIdeDir = "vs2017" }
			"15.0"	{ $mimallocIdeDir = "vs2017" }
			"16.0"	{ $mimallocIdeDir = "vs2019" }
			"17.0"	{ $mimallocIdeDir = "vs2022" }
			default { throw "Unable to resolve mimalloc IDE directory for VC ${VCVersion}."}
		}

		# build mimalloc dependency
		& ${msbuildexe} ..\libs\mimalloc\ide\$mimallocIdeDir\mimalloc.vcxproj /tv:$MSToolsV "/p:Platform=$Platform;Configuration=$Configuration;PlatformToolset=${Toolset}" /t:$target /p:VisualStudioVersion=${VCVersion}
	}

	# build psqlodbc
	& ${msbuildexe} ./platformbuild.vcxproj /tv:$MSToolsV "/p:Platform=$Platform;Configuration=$Configuration;PlatformToolset=${Toolset}" /t:$target /p:VisualStudioVersion=${VCVersion} /p:DRIVERVERSION=$DRIVERVERSION /p:PG_INC=$PG_INC /p:PG_LIB=$PG_LIB /p:PG_BIN=$PG_BIN /p:MIMALLOC=$UseMimalloc $macroList
}

$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
Import-Module ${scriptPath}\Psqlodbc-config.psm1
$configInfo = LoadConfiguration $BuildConfigPath $scriptPath
$DRIVERVERSION=$configInfo.Configuration.version
pushd $scriptPath
$path_save = ${env:PATH}

Import-Module ${scriptPath}\MSProgram-Get.psm1
try {
	$rtnArray=Find-MSBuild ([ref]$VCVersion) ($MSToolsVersion) ([ref]$Toolset) $configInfo
	$msbuildexe=$rtnArray[0]
	$MSToolsV=$rtnArray[1]
} catch [Exception] {
	if ("$_.Exception.Message" -ne "") {
		Write-Host $_.Exception.Message -ForegroundColor Red
	} else {
		echo $_.Exception | Format-List -Force
	}
	popd
	Remove-Module Psqlodbc-config
	return
} finally {
	Remove-Module MSProgram-Get
}

$recordResult = $true
if ($Target -ne "info") {
	$configInfo.Configuration.BuildResult.Date=""
	$configInfo.Configuration.BuildResult.VisualStudioVersion=""
	$configInfo.Configuration.BuildResult.PlatformToolset=""
	$configInfo.Configuration.BuildResult.ToolsVersion=""
	$configInfo.Configuration.BuildResult.Platform=""
}

try {
#
#	build 32bit dlls
#
	if ($Platform -ieq "Win32" -or $Platform -ieq "both") {
		buildPlatform $configInfo "Win32"
		if ($LastExitCode -ne 0) {
			$recordResult = $false
		}
	}
#
#	build 64bit dlls
#
	if ($recordResult -and ($Platform -ieq "x64" -or $Platform -ieq "both")) {
		buildPlatform $configInfo "x64"
		if ($LastExitCode -ne 0) {
			$recordResult = $false
		}
	}
#
#	Write the result to configuration xml
#
	$resultText="successful"
	if ($recordResult) {
		if ($Target -ne "Clean") {
			$configInfo.Configuration.BuildResult.Date=[string](Get-Date)
			$configInfo.Configuration.BuildResult.VisualStudioVersion=$VCVersion
			$configInfo.Configuration.BuildResult.PlatformToolset=$Toolset
			$configInfo.Configuration.BuildResult.ToolsVersion=$MSToolsV
			$configInfo.Configuration.BuildResult.Platform=$Platform
		}
	} else {
		$resultText="failed"
	} 
	SaveConfiguration $configInfo
	Write-Host "VisualStudioVersion=$VCVersion(ToolsVersion=$MSToolsV) PlatformToolset=$Toolset Platform=$Platform $resultText`n"
#
#	build installers as well
#
	if ($AlongWithInstallers) {
		if (-not $recordResult) {
			throw("compilation failed")
		} 
                $cpu = $Platform
                if ($Platform -eq "win32") {
                        $cpu = "x86"
                }
                ..\installer\buildInstallers.ps1 -cpu $cpu -BuildConfigPath $BuildConfigPath
                if ($LASTEXITCODE -ne 0) {
                        throw "Failed to build installers"
                }
	}
} catch [Exception] {
	if ("$_.Exception.Message" -ne "") {
		Write-Host $_.Exception.Message -ForegroundColor Red
	} else {
		echo $_.Exception | Format-List -Force
	}
} finally {
	$env:PATH = $path_save
	popd
	Remove-Module Psqlodbc-config
}
