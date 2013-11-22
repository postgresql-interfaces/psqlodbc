#	build 32bit installer
Param(
[string]$configPath
)
$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
$configInfo = & "$scriptPath\..\winbuild\configuration.ps1" "$configPath"
$VERSION = $configInfo.Configuration.version
$x86info = $configInfo.Configuration.x86
$USE_LIBPQ=$x86info.use_libpq
$USE_GSS=$x86info.use_gss
$USE_SSPI=$x86info.use_sspi
$LIBPQVER=$x86info.libpq.version
if ($LIBPQVER -eq "") {
	$LIBPQVER=$LIBPQ_VERSION
}
if ($USE_LIBPQ -eq "yes")
{
	$LIBPQBINDIR=$x86info.libpq.bin
	if ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
		$pgmfs = "$env:ProgramFiles"
	} else {
		$pgmfs = "$env:ProgramFiles(X86)"
	}
	if ($LIBPQBINDIR -eq "default") {
		$LIBPQBINDIR = "$pgmfs\PostgreSQL\$LIBPQVER\bin"
	}
}

Write-Host "VERSION    : $VERSION"
Write-Host "USE LIBPQ  : $USE_LIBPQ ($LIBPQBINDIR)"
Write-Host "USE GSS    : $USE_GSS ($GSSBINDIR)"
Write-Host "USE SSPI   : $USE_SSPI"

if ($env:WIX -ne "")
{
	$wix = "$env:WIX"
	$env:Path += ";$WIX/bin"
}

<#
	The ProductCode History
	    ProductCode must be changed in case of major upgrade
#>
$PRODUCTID = @{}
$PRODUCTID["09.02.0100"]="838E187D-8B7A-473d-B93C-C8E970B15D2B"
$PRODUCTID["09.03.0100"]="D3527FA5-9C2B-4550-A59B-9534A78950F4"

# The subdirectory to install into
$SUBLOC=$VERSION.substring(0, 2) + $VERSION.substring(3, 2)

$PRODUCTCODE=$PRODUCTID[$VERSION]
if ("$PRODUCTCODE" -eq "") {
	Write-Host ".`nSpecify the ProductCode for the VERSION $VERSION"
	return
}
Write-Host "PRODUCTCODE: $PRODUCTCODE"

try {
	pushd $scroptPath

	Write-Host ".`nBuilding psqlODBC/$SUBLOC merge module..."

	invoke-expression "candle -nologo `"-dVERSION=$VERSION`" -dSUBLOC=$SUBLOC `"-dLINKFILES=$LIBPQBINDIR`" psqlodbcm.wxs"

	invoke-expression "light -nologo -out psqlodbc.msm psqlodbcm.wixobj"

	Write-Host ".`nBuilding psqlODBC installer database..."

	invoke-expression "candle -nologo `"-dVERSION=$VERSION`" -dSUBLOC=$SUBLOC `"-dPRODUCTCODE=$PRODUCTCODE`" psqlodbc.wxs"

	invoke-expression "light -nologo -ext WixUIExtension -cultures:en-us psqlodbc.wixobj"

	Write-Host ".`nDone!"
}
catch {
	Write-Host ".`nAborting build!"
	throw $error[0]
}
finally {
	popd
}
