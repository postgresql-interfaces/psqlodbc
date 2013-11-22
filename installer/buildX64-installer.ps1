#	build 64bit installer
Param(
[string]$configPath
)
$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
$configInfo = & "$scriptPath\..\winbuild\configuration.ps1" "$configPath"
$VERSION = $configInfo.Configuration.version
$x64info = $configInfo.Configuration.x64
$USE_LIBPQ=$x64info.use_libpq
$USE_GSS=$x64info.use_gss
$USE_SSPI=$x64info.use_sspi
$LIBPQVER=$x64info.libpq.version
if ($LIBPQVER -eq "") {
	$LIBPQVER=$LIBPQ_VERSION
}
if ($USE_LIBPQ -eq "yes")
{
	$LIBPQBINDIR=$x64info.libpq.bin
	if ($LIBPQBINDIR -eq "default") {
		if ($env:PROCESSOR_ARCHITECTURE -eq "x64") {
			$pgmfs = "$env:ProgramFiles"
			$LIBPQBINDIR = "$pgmfs\PostgreSQL\$LIBPQVER\bin"
		}
	} 
}
if ($USE_GSS -eq "yes")
{
	$GSSBINDIR=$x64info.gss.bin
}

Write-Host "VERSION    : $VERSION"
Write-Host "USE LIBPQ  : $USE_LIBPQ ($LIBPQBINDIR)"
Write-Host "USE GSS    : $USE_GSS ($GSSBINDIR)"
Write-Host "USE SSPI   : $USE_SSPI"

$CPUTYPE="x64"

if ($env:WIX -ne "")
{
	$wix = "$env:WIX"
	$env:Path += ";$WIX/bin"
}
# The subdirectory to install into
$SUBLOC=$VERSION.substring(0, 2) + $VERSION.substring(3, 2)

if (!(Test-Path -Path $CPUTYPE)) {
    New-Item -ItemType directory -Path $CPUTYPE
}

<#
	ProductCode History

#>
$PRODUCTID = @{}
$PRODUCTID["09.02.0100"]="3E42F836-9204-4c42-B3C3-8680A0434875"
$PRODUCTID["09.03.0100"]="1F896F2F-5756-4d22-B5A3-040796C9B485"

$PRODUCTCODE=$PRODUCTID[$VERSION]
if ("$PRODUCTCODE" -eq "") {
	Write-Host "`nSpecify the ProductCode for the VERSION $VERSION"
	return
}
Write-Host "PRODUCTCODE: $PRODUCTCODE"

try {
	pushd $scriptPath

	Write-Host ".`nBuilding psqlODBC/$SUBLOC merge module..."

	invoke-expression "candle -nologo -dPlatform=$CPUTYPE `"-dVERSION=$VERSION`" -dSUBLOC=$SUBLOC `"-dLIBPQBINDIR=$LIBPQBINDIR`" `"-dGSSBINDIR=$GSSBINDIR`" -o $CPUTYPE\psqlodbcm.wixobj psqlodbcm_cpu.wxs"

	Write-Host ".`nLinking psqlODBC merge module..."
	invoke-expression "light -nologo -o $CPUTYPE\psqlodbc_$CPUTYPE.msm $CPUTYPE\psqlodbcm.wixobj"

	Write-Host ".`nBuilding psqlODBC installer database..."

	invoke-expression "candle -nologo -dPlatform=$CPUTYPE `"-dVERSION=$VERSION`" -dSUBLOC=$SUBLOC `"-dPRODUCTCODE=$PRODUCTCODE`" -o $CPUTYPE\psqlodbc.wixobj psqlodbc_cpu.wxs"

	Write-Host ".`nLinking psqlODBC installer database..."
	invoke-expression "light -nologo -ext WixUIExtension -cultures:en-us -o $CPUTYPE\psqlodbc_$CPUTYPE.msi $CPUTYPE\psqlodbc.wixobj"

	Write-Host ".`nModifying psqlODBC installer database..."
	invoke-expression "cscript modify_msi.vbs $CPUTYPE\psqlodbc_$CPUTYPE.msi"
	
	Write-Host ".`nDone!"
}
catch {
	Write-Host ".`Aborting build!"
	throw $error[0]
}
finally {
	popd
}
