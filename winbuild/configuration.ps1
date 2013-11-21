function InitConfiguration
{
	$configInfo = [xml](Get-Content "$configTempPath")
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
	$configInfo =  [xml] (Get-Content "$configPath")
	set-variable -name xmlFormatVersion -value "0.1" -option constant
	if ($configInfo.Configuration.formatVersion -ne $xmlFormatVersion)
	{
        	$xmlDoc2 = [xml](Get-Content "$configTempPath")
        	$root2 = $XmlDoc2.get_DocumentElement() 
        	$root1 = $configInfo.get_DocumentElement() 
        	unifyNodes $root1 $root2

		$root1.formatVersion = $xmlFormatVersion
            	$configInfo.save("$configPath")
	}

	return $configInfo
}

function global:SaveConfiguration
{
	$configInfo.save("$configPath")
}

function unifyNodes($node1, $node2)
{
    $attributes2 = $node2.get_Attributes()
    if ($attributes2.Count -gt 0)
    {
        $attributes1 = $node1.get_Attributes()
        foreach ($attrib in $attributes2)
        {
            $attribname = $attrib.name
            if (($attributes1.Count -eq 0) -or ($attributes1.GetNamedItem($attribname) -eq $null))
            {
                Write-Debug " Adding attribute=$attribname"
                $addattr = $node1.OwnerDocument.ImportNode($attrib, $true)
                $added = $attributes1.Append($addattr)
            }
        }
    }
    if (!$node2.get_HasChildNodes()) {
        return;
    }
    foreach ($child2 in $node2.get_ChildNodes())
    {
        $nodename = $child2.get_Name()
        if ($nodename -eq "#text"){
            continue
        }
        $matchnode = $node1.SelectSingleNode($nodename)
        if ($matchnode -eq $null)
        {
                Write-Debug "Adding node=$nodename"
                $addnode = $node1.OwnerDocument.ImportNode($child2, $true)
                $added = $node1.AppendChild($addnode)
                continue
        }
        unifyNodes $matchnode $child2
    }
}

$LIBPQ_VERSION="9.3"
$scriptPath = [System.IO.Path]::GetDirectoryName($myInvocation.MyCommand.Definition)
$configPath = "$scriptPath\configuration.xml"
$configTempPath = "$scriptPath\configuration_template.xml"
if (!(Test-Path -path $configPath))
{
	InitConfiguration
}
Return
