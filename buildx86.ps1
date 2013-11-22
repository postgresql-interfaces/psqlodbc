#	build 32bit dll
Param(
[string]$target="ALL",
[string]$configPath
)
$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
$configInfo = & "$scriptPath\winbuild\configuration.ps1" "$configPath"
$x86info = $configInfo.Configuration.x86
pushd $scriptPath
if ($x86info.setvcvars -ne "") {
	$envcmd = [String] $x86info.setvcvars
	Write-Host "setvcvars :" $envcmd
	Invoke-Expression $envcmd
}
$USE_LIBPQ=$x86info.use_libpq
$USE_SSPI=$x86info.use_sspi
$LIBPQVER=$x86info.libpq.version
if ($LIBPQVER -eq "") {
	$LIBPQVER=$LIBPQ_VERSION
}
$PG_INC=$x86info.libpq.include
$PG_LIB=$x86info.libpq.lib
$SSL_INC=$x86info.ssl.include
$SSL_LIB=$x86info.ssl.lib
$BUILD_MACROS=$x86info.build_macros
if ($USE_LIBPQ -eq "yes")
{
	if ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
		$pgmfs = "$env:ProgramFiles"
	} else {
		$pgmfs = "$env:ProgramFiles(X86)"
	}
	if ($PG_INC -eq "default") {
		$PG_INC = "$pgmfs\PostgreSQL\$LIBPQVER\include"
	} 
	if ($PG_LIB -eq "default") {
		$PG_LIB = "$pgmfs\PostgreSQL\$LIBPQVER\lib"
	} 
}
Write-Host "USE LIBPQ  : $USE_LIBPQ ($PG_INC $PG_LIB)"
# Write-Host "USE GSS    : $USE_GSS"
Write-Host "USE SSPI   : $USE_SSPI"
Write-Host "SSL	   : ($SSL_INC $SSL_LIB)"
$MACROS = "USE_LIBPQ=$USE_LIBPQ USE_SSPI=$USE_SSPI PG_LIB=`"$PG_LIB`" PG_INC=`"$PG_INC`" SSL_LIB=`"$SSL_LIB`" SSL_INC=`"$SSL_INC`" $BUILD_MACROS"
invoke-expression "nmake.exe /f win32.mak $MACROS $target"
invoke-expression "nmake.exe /f win32.mak ANSI_VERSION=yes $MACROS $target"
popd
