<#
.SYNOPSIS
    Run regression test on Windows.
.DESCRIPTION
    Build test programs and run them.
.PARAMETER Target
    Specify the target of MSBuild. "Build&Go"(default), "Build" or
    "Clean" is available.
.PARAMETER TestList
    Specify the list of test cases. If this parameter isn't specified(default),
    all test cases are executed.
.PARAMETER Ansi
    Specify this switch in case of testing Ansi drivers.
.PARAMETER DeclareFetch
    Specify Use Declare/Fetch mode. "On"(default), "off" or "both" is available.
.PARAMETER DsnInfo
	Specify the dsn info "SERVER=${server}|DATABASE=${database}|PORT=${port}|UID=${uid}|PWD=${passwd}"	
.PARAMETER VCVersion
    Used Visual Studio version is determined automatically unless this
    option is specified.
.PARAMETER Platform
    Specify platforms to test. "x64"(default), "Win32" or "both" is available.
.PARAMETER Toolset
    MSBuild PlatformToolset is determined automatically unless this
    option is specified. Currently "v100", "Windows7.1SDK", "v110",
    "v110_xp", "v120", "v120_xp", "v140" or "v140_xp" is available.
.PARAMETER MSToolsVersion
    This option is deprecated. MSBuild ToolsVersion is determined
    automatically unless this option is specified.  Currently "4.0",
    "12.0" or "14.0" is available.
.PARAMETER Configuration
    Specify the configuration used to build executables for the regression tests. "Release"(default) or "Debug".
.PARAMETER DriverConfiguration
    Specify the configuration which was used to build the driver dlls to test. "Release"(default) or "Debug".
.PARAMETER BuildConfigPath
    Specify the configuration xml file name if you want to use
    the configuration file other than standard one.
    The relative path is relative to the current directory.
.PARAMETER ReinstallDriver
    Reinstall the driver in any case.
.PARAMETER ExpectMimalloc
    Specify whether usage of the mimalloc allocator is expected.
.EXAMPLE
    > .\regress
	Build with default or automatically selected parameters
	and run tests.
.EXAMPLE
    > .\regress Clean
	Clean all generated files.
.EXAMPLE
    > .\regress -TestList connect, deprecated
	Build and run connect-test and deprecated-test.
.EXAMPLE
    > .\regress -Ansi
	Build and run with ANSI version of drivers.
.EXAMPLE
    > .\regress -V(CVersion) 14.0
	Build using Visual Studio 14.0 environment and run tests.
.EXAMPLE
    > .\regress -P(latform) x64
	Build 64bit test programs and run them.
.NOTES
    Author: Hiroshi Inoue
    Date:   August 2, 2016
#>

#
#	build 32bit & 64bit dlls for VC10 or later
#
Param(
[ValidateSet("Build&Go", "Build", "Clean")]
[string]$Target="Build&Go",
[string[]]$TestList,
[switch]$Ansi,
[string]$VCVersion,
[ValidateSet("Win32", "x64", "both")]
[string]$Platform="both",
[string]$Toolset,
[ValidateSet("", "4.0", "12.0", "14.0")]
[string]$MSToolsVersion,
[ValidateSet("Debug", "Release")]
[String]$Configuration="Release",
[ValidateSet("Debug", "Release")]
[String]$DriverConfiguration="Release",
[string]$BuildConfigPath,
[ValidateSet("off", "on", "both")]
[string]$DeclareFetch="on",
[string]$DsnInfo,
[string]$SpecificDsn,
[switch]$ReinstallDriver,
[switch]$ExpectMimalloc
)


function testlist_make($testsf)
{
	$testbins=@()
	$testnames=@()
	$dirnames=@()
	$testexes=@()
	$f = (Get-Content -Path $testsf) -as [string[]]
	$nstart=$false
	foreach ($l in $f) {
		if ($l[0] -eq "#") {
			continue
		}
		$sary=-split $l
		if ($sary[0] -eq "#") {
			continue
		}
		if ($sary[0] -eq "TESTBINS") {
			$nstart=$true
			$sary[0]=$null
			if ($sary[1] -eq "=") {
				$sary[1]=$null
			}
		}
		if ($nstart) {
			if ($sary[$sary.length - 1] -eq "\") {
				$sary[$sary.length - 1] = $null
			} else {
				$nstart=$false
			}
			$testbins+=$sary
			if (-not $nstart) {
				break
			}
		}
	}
	for ($i=0; $i -lt $testbins.length; $i++) {
		Write-Debug "$i : $testbins[$i]"
	}

	foreach ($testbin in $testbins) {
		if ("$testbin" -eq "") {
			continue
		}
		$sary=$testbin.split("/")
		$testname=$sary[$sary.length -1]
		$dirname=""
		for ($i=0;$i -lt $sary.length - 1;$i++) {
			$dirname+=($sary[$i]+"`\")
		}
		Write-Debug "testbin=$testbin => testname=$testname dirname=$dirname"
		$dirnames += $dirname
		$testexes+=($dirname+$testname+".exe")
		$testnames+=$testname.Replace("-test","")
	}

	return $testexes, $testnames, $dirnames
}

function vcxfile_make($testnames, $dirnames, $vcxfile)
{
# here-string
	@'
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
    <!--
	 This file is automatically generated by regress.ps1
	 and used by MSBuild.
    -->
    <PropertyGroup>
	<scriptPath>.</scriptPath>
    </PropertyGroup>
    <PropertyGroup>
	<Configuration>Release</Configuration>
	<srcPath>$(scriptPath)\..\test\src\</srcPath>
    </PropertyGroup>
    <Target Name="Build">
        <MSBuild Projects="$(scriptPath)\regress_one.vcxproj"
	  Targets="ClCompile"
	  Properties="TestName=common;Configuration=$(Configuration);srcPath=$(srcPath)"/>
'@ > $vcxfile

	for ($i=0; $i -lt $testnames.length; $i++) {
		$testname=$testnames[$i]
		$dirname=$dirnames[$i]
		$testname+="-test"
# here-string
		@"
        <MSBuild Projects="${scriptPath}\regress_one.vcxproj"
	  Targets="Build"
	  Properties="TestName=$testname;Configuration=`$(Configuration);srcPath=`$(srcPath);SubDir=$dirname"/>
"@ >> $vcxfile
	}
# here-string
	@'
        <MSBuild Projects="$(scriptPath)\regress_one.vcxproj"
	  Targets="Build"
	  Properties="TestName=runsuite;Configuration=$(Configuration);srcPath=$(srcPath)..\"/>
        <MSBuild Projects="$(scriptPath)\regress_one.vcxproj"
	  Targets="Build"
	  Properties="TestName=RegisterRegdsn;Configuration=$(Configuration);srcPath=$(srcPath)..\"/>
        <!-- MSBuild Projects="$(scriptPath)\regress_one.vcxproj"
	  Targets="Build"
	  Properties="TestName=ConfigDsn;Configuration=$(Configuration);srcPath=$(srcPath)..\"/-->
        <MSBuild Projects="$(scriptPath)\regress_one.vcxproj"
	  Targets="Build"
	  Properties="TestName=reset-db;Configuration=$(Configuration);srcPath=$(srcPath)..\"/>
    </Target>
    <Target Name="Clean">
        <MSBuild Projects="$(scriptPath)\regress_one.vcxproj"
	  Targets="Clean"
	  Properties="Configuration=$(Configuration);srcPath=$(srcPath)"/>
    </Target>
</Project>
'@ >> $vcxfile

}

function RunTest($scriptPath, $Platform, $testexes)
{
	# Run regression tests
	if ($Platform -eq "x64") {
		$targetdir="test_x64"
	} else {
		$targetdir="test_x86"
	}
	$revsdir=$scriptPath
	$origdir="${revsdir}\..\test"

	try {
		$regdiff="regression.diffs"
		$RESDIR="results"
		if (Test-Path $regdiff) {
			Remove-Item $regdiff
		}
		New-Item $RESDIR -ItemType Directory -Force > $null
		Get-Content "${origdir}\sampletables.sql" | .\reset-db
		if ($LASTEXITCODE -ne 0) {
			throw "`treset_db error"
		}
		$env:MIMALLOC_VERBOSE = 1
		$cnstr = @()
		switch ($DeclareFetch) {
			"off"	{ $cnstr += "UseDeclareFetch=0" }
			"on"	{ $cnstr += "UseDeclareFetch=1" }
			"both"	{ $cnstr += "UseDeclareFetch=0"
				  $cnstr += "UseDeclareFetch=1" }
		}
		if ($cnstr.length -eq 0) {
			$cnstr += $null
		}
		for ($i = 0; $i -lt $cnstr.length; $i++)
		{
			$env:COMMON_CONNECTION_STRING_FOR_REGRESSION_TEST = $cnstr[$i]
			if ("$SpecificDsn" -ne "") {
				$env:COMMON_CONNECTION_STRING_FOR_REGRESSION_TEST += ";Database=contrib_regression;ConnSettings={set lc_messages='C'}"
			}
			write-host "`n`tSetting by env variable:$env:COMMON_CONNECTION_STRING_FOR_REGRESSION_TEST"
			.\runsuite $testexes --inputdir=$origdir 2>&1 | Tee-Object -Variable runsuiteOutput

			# Check whether mimalloc ran by searching for a message printed by the MIMALLOC_VERBOSE option
			if ($ExpectMimalloc -xor ($runsuiteOutput -match "mimalloc: process done")) {
				throw "`tmimalloc usage was expected to be $ExpectMimalloc"
			}
		}
	} catch [Exception] {
		throw $error[0]
	} finally {
		$env:MIMALLOC_VERBOSE = $null
		$env:COMMON_CONNECTION_STRING_FOR_REGRESSION_TEST = $null
	}
}

function SpecialDsn($testdsn, $testdriver, $dsninfo)
{
	function input-dsninfo($server="localhost", $uid="postgres", $passwd="postgres", $port="5432", $database="contrib_regression")
	{
		$in = read-host "Server [$server]"
		if ("$in" -ne "") {
			$server = $in
		}
		$in = read-host "Port [$port]"
		if ("$in" -ne "") {
			$port = $in
		}
		$in = read-host "Username [$uid]"
		if ("$in" -ne "") {
			$uid = $in
		}
		$in = read-host -assecurestring "Password [$passwd]"
		if ($in.Length -ne 0) {
			$ptr = [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR($in)
			$passwd = [System.Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
		}
		return "SERVER=${server}|DATABASE=${database}|PORT=${port}|UID=${uid}|PWD=${passwd}"
	}

	$reinst = $ReinstallDriver
	$regProgram = "./RegisterRegdsn.exe"
	& $regProgram "check_dsn" $testdsn
	switch ($LastExitCode) { 
	 -1 {
		Write-Host "`tAdding System DSN=$testdsn Driver=$testdriver"
		if ($dsninfo.Length -eq 0) {
			$prop = input-dsninfo
		} else {
			$prop = $dsninfo
		}
		$prop += "|Debug=0|Commlog=0|ConnSettings=set+lc_messages='C'"
		$proc = Start-Process $regProgram -Verb runas -Wait -PassThru -ArgumentList "register_dsn $testdriver $testdsn $prop `"$dlldir`" Driver=${dllname}|Setup=${setup}"
		if ($proc.ExitCode -ne 0) {
			throw "`tAddDsn $testdsn error"
		}
		}
	 -2 {
		$reinst = $true
		<# Write-Host "`tReinstalling Driver=$testdriver"
		$proc = Start-Process $regProgram -Verb runas -Wait -PassThru -ArgumentList "reinstall_driver $testdriver `"$dlldir`" Driver=${dllname}|Setup=${setup}" #>
		}
	 0 {}
	 default {
		throw "$regProgram error"
		}
	}
	if ($reinst) {
		Write-Host "`tReinstalling Driver=$testdriver"
		$proc = Start-Process $regProgram -Verb runas -Wait -PassThru -ArgumentList "reinstall_driver $testdriver `"$dlldir`" Driver=${dllname}|Setup=${setup}"

	}
}

$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
$usingExe=$true
$testsf="$scriptPath\..\test\tests"
Write-Debug testsf=$testsf

$arrays=testlist_make $testsf
if ($null -eq $TestList) {
	$TESTEXES=$arrays[0]
	$TESTNAMES=$arrays[1]
	$DIRNAMES=$arrays[2]
} else {
	$err=$false
	$TESTNAMES=$TestList
	$TESTEXES=@()
	$DIRNAMES=@()
	foreach ($l in $TestList) {
		for ($i=0;$i -lt $arrays[1].length;$i++) {
			if ($l -eq $arrays[1][$i]) {
				$TESTEXES+=$arrays[0][$i]
				$DIRNAMES+=$arrays[2][$i]
				break
			}
		}
<#		if ($i -ge $arrays[1].length) {
			Write-Host "!! test case $l doesn't exist"
			$err=$true
		} #>
	}
	if ($err) {
		return
	}
}

Import-Module "$scriptPath\Psqlodbc-config.psm1"
$configInfo = LoadConfiguration $BuildConfigPath $scriptPath
$objbase = GetObjbase "$scriptPath\.."
$pushdir = GetObjbase "$scriptPath"

Import-Module ${scriptPath}\MSProgram-Get.psm1
$rtnArray=Find-MSBuild ([ref]$VCVersion) ($MSToolsVersion) ([ref]$Toolset) $configInfo
$msbuildexe=$rtnArray[0]
$MSToolsV=$rtnArray[1]
write-host "vcversion=$VCVersion toolset=$Toolset"

Remove-Module MSProgram-Get
Remove-Module Psqlodbc-config

$vcxfile="$objbase\generated_regress.vcxproj"
vcxfile_make $TESTNAMES $DIRNAMES $vcxfile

if ($Platform -ieq "both") {
	$pary = @("Win32", "x64")
} else {
	$pary = @($Platform)
}

$vcx_target=$target
if ($target -ieq "Build&Go") {
	$vcx_target="Build"
}
if ($Ansi) {
	write-host ** testing Ansi driver **
	$testdriver="postgres_deva"
	$testdsn="psqlodbc_test_dsn_ansi"
	$ansi_dir_part="ANSI"
	$dllname="psqlsetupa.dll"
	$setup="psqlsetupa.dll"
} else {
	write-host ** testing unicode driver **
	$testdriver="postgres_devw"
	$testdsn="psqlodbc_test_dsn"
	$ansi_dir_part="Unicode"
	$dllname="psqlsetup.dll"
	$setup="psqlsetup.dll"
}
if ($DriverConfiguration -ieq "Debug") {
	$testdriver += "_debug"
	$testdsn += "_debug"
}
if ("$DsnInfo" -ne "") {
	Write-Host "`tDsn Info=$DsnInfo"
	$dsninfo=$DsnInfo
}
if ("$SpecificDsn" -ne "") {
	Write-Host "`tSpecific DSN=$SpecificDsn"
	$testdsn=$SpecificDsn
}
foreach ($pl in $pary) {
	cd $scriptPath
	& ${msbuildexe} ${vcxfile} /tv:$MSToolsV "/p:Platform=$pl;Configuration=$Configuration;PlatformToolset=${Toolset}" /t:$vcx_target /p:VisualStudioVersion=${VCVersion} /p:scriptPath=${scriptPath} /Verbosity:minimal
	if ($LASTEXITCODE -ne 0) {
		throw "`nCompile error"
	}

	if (($target -ieq "Clean") -or ($target -ieq "Build")) {
		continue
	}

	switch ($pl) {
	 "Win32" {
			$targetdir="test_x86"
			$bit="32-bit"
			$dlldir="$objbase\x86_${ansi_dir_part}_$DriverConfiguration"
		}
	 default {
			$targetdir="test_x64"
			$bit="64-bit"
			$dlldir="$objbase\x64_${ansi_dir_part}_$DriverConfiguration"
		}
	}
	pushd $pushdir\$targetdir

	$env:PSQLODBC_TEST_DSN = $testdsn
	try {
		SpecialDsn $testdsn $testdriver $dsninfo
		RunTest $scriptPath $pl $TESTEXES
	} catch [Exception] {
		throw $error[0]
	} finally {
		popd
		$env:PSQLODBC_TEST_DSN = $null
	}
}
