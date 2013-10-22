#	build 64bit dll
. ".\winbuild\configuration.ps1"
$configInfo = GetConfiguration
$x64info = $configInfo.Configuration.x64
Write-Host "setenv :" $x64info.setenv
if ($x64info.setenv -ne "") {
	$envcmd = [String] $x64info.setenv
	if ($envcmd.StartsWith(". ")) {
		. $envcmd.substring(2)
	} else {
		Invoke-Expression $envcmd
	}
}

$USE_LIBPQ=$x64info.use_libpq
$LIBPQVER=$x64info.libpq.version
$USE_SSPI=$x64info.use_sspi
$USE_GSS=$x64info.use_gss
$PG_INC=$x64info.libpq.include
$PG_LIB=$x64info.libpq.lib
$SSL_INC=$x64info.ssl.include
$SSL_LIB=$x64info.ssl.lib
$GSS_INC=$x64info.gss.include
$GSS_LIB=$x64info.gss.lib
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
Write-Host "USE_SSPI   : $USE_SSPI"
Write-Host "SSL DIR    : ($SSL_INC $SSL_LIB)"

$MACROS = "USE_LIBPQ=$USE_LIBPQ USE_SSPI=$USE_SSPI USE_GSS=$USE_GSS PG_LIB=`"$PG_LIB`" PG_INC=`"$PG_INC`" SSL_LIB=`"$SSL_LIB`" SSL_INC=`"$SSL_INC`"GSS_LIB=`"$GSS_LIB`" GSS_INC=`"$GSS_INC`" $args"
invoke-expression "nmake.exe /f win64.mak $MACROS"
invoke-expression "nmake.exe /f win64.mak ANSI_VERSION=yes $MACROS"
