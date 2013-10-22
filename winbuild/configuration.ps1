function InitConfiguration
{
	$configInfo = [xml](Get-Content "$scriptPath\configuration_template.xml")
	if ($env:PROCESSOR_ARCHITECTURE -eq "x86")
	{
		$x64info = $configInfo.Configuration.x64
		$x64info.libpq.include = ""	
		$x64info.libpq.lib = ""	
		$x64info.libpq.bin = ""	
	}
	$configInfo.save("$configPath")
}

function global:GetConfiguration
{
	Return  [xml] (Get-Content "$configPath")
}

function global:SaveConfiguration
{
	$configInfo.save("$configPath")
}

$scriptPath = [System.IO.Path]::GetDirectoryName($myInvocation.MyCommand.Definition)
$configPath = "$scriptPath\configuration.xml"
if (!(Test-Path -path $configPath))
{
	InitConfiguration
}
Return
