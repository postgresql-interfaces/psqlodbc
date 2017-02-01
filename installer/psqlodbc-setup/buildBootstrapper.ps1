<#
.SYNOPSIS
    Build a bootstrapper application of psqlodbc drivers.
.DESCRIPTION
    Build psqlodbc-setup.exe
.PARAMETER version
    Specify when you'd like to specify the version explicitly.
.PARAMETER UI
    Specify when you'd like to show UIs of each MSIs.
.PARAMETER withRedist
    Specify when you have to include vcredist.
.PARAMETER BuildConfigPath
    Specify the configuration xml file name if you want to use
    the configuration file other than standard one.
    The relative path is relative to the current directory.
.EXAMPLE
    > .\buildBootstrapper
        Build the bootstrapper.
.EXAMPLE
    > .\buildBootstrapper -w(ithRedist)
        Build the bootstrapper with vcredist.
.NOTES
    Author: Hiroshi Inoue
    Date:   October 8, 2014
#>
#       build bootstrapper of psqlodbc drivers
#
Param(
[string]$version,
[switch]$UI,
[switch]$withRedist,
[string]$BuildConfigPath
)

write-host "Building bootstrapper program`n"

$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
if ("$version" -eq "") {
	# $configInfo = & "$scriptPath\..\..\winbuild\configuration.ps1" "$BuildConfigPath"
	$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
	$modulePath="${scriptPath}\..\..\winbuild"
	Import-Module ${modulePath}\Psqlodbc-config.psm1
	$defaultConfigDir=$modulePath
	$configInfo = LoadConfiguration $BuildConfigPath $defaultConfigDir
	$version = GetPackageVersion $configInfo "$scriptPath/../.."
	Remove-Module Psqlodbc-config
}

if ("$env:WIX" -eq "") {
	throw "Please install WIX"
}

$wix_dir="${env:WIX}bin"
$pgmname="psqlodbc-setup"
$build_config="Release"
$objdir="obj\${build_config}"
$bindir="bin\${build_config}"

$modules=@("Bundle.wxs")
$wRedist="no"
$objs=@("${objdir}\Bundle.wixobj")
if ($withRedist) {
	$modules += "vcredist.wxs"
	$objs += "${objdir}\vcredist.wixobj"
	$wRedist = "yes"
	write-host "with Redistributable"
}
$wUI = "no"
if ($UI) {
	$wUI = "yes"
}

try {
	pushd "$scriptPath"

	& ${wix_dir}\candle.exe -v "-dVERSION=$version" "-dwithRedist=$wRedist" "-dwithUI=$wUI" "-dConfiguration=${build_config}" "-dOutDir=${bindir}\" -dPlatform=x86 "-dProjectDir=.\" "-dProjectExt=.wixproj" "-dProjectFileName=${pgmname}.wixproj" "-dProjectName=${pgmname}" "-dProjectPath=${pgmname}.wixproj" "-dTargetDir=${bindir}\" "-dTargetExt=.exe" "-dTargetFileName=${pgmname}.exe" "-dTargetName=${pgmname}" "-dTargetPath=${bindir}\${pgmname}.exe" -out "${objdir}\" -arch x86 -ext ${wix_dir}\WixUtilExtension.dll -ext ${wix_dir}\WixBalExtension.dll $modules
	# $candle_cmd = "& `"${wix_dir}\candle.exe`" -v `"-dVERSION=$version`" -dwithRedist=$wRedist -dwithUI=$wUI -dConfiguration=${build_config} `"-dOutDir=${bindir}\`" -dPlatform=x86 `"-dProjectDir=.\`" `"-dProjectExt=.wixproj`" `"-dProjectFileName=${pgmname}.wixproj`" -dProjectName=${pgmname} `"-dProjectPath=${pgmname}.wixproj`" -dTargetDir=${bindir}\ `"-dTargetExt=.exe`" `"-dTargetFileName=${pgmname}.exe`" -dTargetName=${pgmname} `"-dTargetPath=${bindir}\${pgmname}.exe`" -out `"${objdir}\`" -arch x86 -ext `"${wix_dir}\WixUtilExtension.dll`" -ext `"${wix_dir}\WixBalExtension.dll`" $modules"
	#write-debug "candle_cmd = ${candle_cmd}"
	# compile
	#invoke-expression $candle_cmd
	if ($LASTEXITCODE -ne 0) {
		throw "Failed to compile $modules"
	}
	# link
	# invoke-expression "& `"${wix_dir}\Light.exe`" -out ${bindir}\${pgmname}.exe -pdbout ${bindir}\${pgmname}.wixpdb -ext `"${wix_dir}\\WixUtilExtension.dll`" -ext `"${wix_dir}\\WixBalExtension.dll`" -contentsfile ${objdir}\${pgmname}.wixproj.BindContentsFileList.txt -outputsfile ${objdir}\${pgmname}.wixproj.BindOutputsFileList.txt -builtoutputsfile ${objdir}\${pgmname}.wixproj.BindBuiltOutputsFileList.txt -wixprojectfile ${pgmname}.wixproj  ${objs}"
	& ${wix_dir}\Light.exe -out ${bindir}\${pgmname}.exe -pdbout ${bindir}\${pgmname}.wixpdb -ext ${wix_dir}\\WixUtilExtension.dll -ext ${wix_dir}\\WixBalExtension.dll -contentsfile ${objdir}\${pgmname}.wixproj.BindContentsFileList.txt -outputsfile ${objdir}\${pgmname}.wixproj.BindOutputsFileList.txt -builtoutputsfile ${objdir}\${pgmname}.wixproj.BindBuiltOutputsFileList.txt -wixprojectfile "${pgmname}.wixproj" ${objs}
	if ($LASTEXITCODE -ne 0) {
		throw "Failed to link bootstrapper"
	}
}
catch [Exception] {
       	Write-Host ".`Aborting build!"
       	throw $error[0]
}
finally {
       	popd
}
