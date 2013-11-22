#	build 64bit dll
Param(
[string]$target="ALL",
[string]$configPath
)
$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
$configInfo = & "$scriptPath\winbuild\configuration.ps1" "$configPath"
$x64info = $configInfo.Configuration.x64
pushd $scriptPath
if ($x64info.setvcvars -ne "") {
	$envcmd = [String] $x64info.setvcvars
	Write-Host "setvcvars :" $envcmd
	Invoke-Expression $envcmd
}

$USE_LIBPQ=$x64info.use_libpq
$LIBPQVER=$x64info.libpq.version
if ($LIBPQVER -eq "") {
	$LIBPQVER = $LIBPQ_VERSION
}
$USE_SSPI=$x64info.use_sspi
$USE_GSS=$x64info.use_gss
$PG_INC=$x64info.libpq.include
$PG_LIB=$x64info.libpq.lib
$SSL_INC=$x64info.ssl.include
$SSL_LIB=$x64info.ssl.lib
$GSS_INC=$x64info.gss.include
$GSS_LIB=$x64info.gss.lib
$BUILD_MACROS=$x64info.build_macros
if ($USE_LIBPQ -eq "yes")
{
	if ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
		$pgmfs = ""
	} else {
		$pgmfs = "$env:ProgramFiles"
	}
	if ($PG_INC -eq "default") {
		$PG_INC = "$pgmfs\PostgreSQL\$LIBPQVER\include"
	} 
	if ($PG_LIB -eq "default") {
		$PG_LIB = "$pgmfs\PostgreSQL\$LIBPQVER\lib"
	} 
}

Write-Host "USE LIBPQ  : $USE_LIBPQ ($PG_INC $PG_LIB)"
Write-Host "USE GSS    : $USE_GSS ($GSS_INC $GSS_LIB)"
Write-Host "USE SSPI   : $USE_SSPI"
Write-Host "SSL DIR    : ($SSL_INC $SSL_LIB)"

$MACROS = "USE_LIBPQ=$USE_LIBPQ USE_SSPI=$USE_SSPI USE_GSS=$USE_GSS PG_LIB=`"$PG_LIB`" PG_INC=`"$PG_INC`" SSL_LIB=`"$SSL_LIB`" SSL_INC=`"$SSL_INC`" GSS_LIB=`"$GSS_LIB`" GSS_INC=`"$GSS_INC`" $BUILD_MACROS"
invoke-expression "nmake.exe /f win64.mak $MACROS $target"
invoke-expression "nmake.exe /f win64.mak ANSI_VERSION=yes $MACROS $target"
popd
