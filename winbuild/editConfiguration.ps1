# Powershell needs to run in STA mode to display WPF windows
Param(
[string]$configPath
)
if ([Threading.Thread]::CurrentThread.GetApartmentState() -eq "MTA"){
	PowerShell -Sta -File $MyInvocation.MyCommand.Path
	return
}

<#
	Edit the configuration xnl file with WPF
#>

Add-Type -AssemblyName presentationframework

[xml]$XAML = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="MainWindow" Height="470" Width="539" BorderBrush="Black" Margin="30,0,0,0">
    <Grid>
        <StackPanel Height="600" HorizontalAlignment="Left" Margin="42,29,0,0" Name="stackPanel1" VerticalAlignment="Top" Width="431" Opacity="1">
            <StackPanel Orientation="Horizontal" Height="50">
                <Label Content="Windows Build Configuration" Height="28" Name="label25" Margin="30,0,0,0" />
                <Button Content="save" Height="23" Name="buttonSave" Width="75" Margin="30,0,0,0" />
                <Button Content="end" Height="23" Name="buttonEnd" Width="75" Margin="30,0,0,0" />
            </StackPanel>
            <StackPanel Orientation="Horizontal" Height="30" Width="Auto">
                <Label Content="version" Height="28" Name="labelVersion" HorizontalAlignment="Left" />
		<TextBox Height="24" Name="versionBox" HorizontalAlignment="Left" Width="100" />
                <Label Content="vcversion" Height="28" Name="labelVcversion" HorizontalAlignment="Left" />
		<TextBox Height="24" Name="vcversionBox" HorizontalAlignment="Left" Width="50" />
                <Label Content="toolset" Height="28" Name="labelToolset" HorizontalAlignment="Left" />
		<TextBox Height="24" Name="toolsetBox" HorizontalAlignment="Left" Width="50" />
            </StackPanel>
            <StackPanel Orientation="Horizontal">
                <Label Content="x86" Height="26" Name="label1" Width="43" HorizontalContentAlignment="Center" HorizontalAlignment="Left" VerticalAlignment="Top" />
		<TextBox Height="Auto" Name="versionBox1" Width="30" />
            </StackPanel>
            <StackPanel Height="78" Name="stackPanel2" Width="Auto" HorizontalAlignment="Right" Orientation="Horizontal">
                <Label Content="libpq" Height="Auto" HorizontalContentAlignment="Center" Name="label2" VerticalContentAlignment="Center" Width="51" BorderBrush="Black" BorderThickness="1,1,0,0" />
                <StackPanel Height="Auto" Name="stackPanel3" Width="380">
                    <StackPanel Height="26" Name="stackPanel4" Width="Auto" Orientation="Horizontal">
                        <Label Content="include" Height="Auto" Name="label3" Width="56" BorderThickness="1,1,1,0" BorderBrush="Black" />
                        <TextBox Height="24" Name="textBox1" Width="304" />
                        <Button Content="..." Height="23" Name="button1" Width="20" />
                    </StackPanel>
                    <StackPanel Height="26" Name="stackPanel5" Width="Auto" Orientation="Horizontal">
                        <Label Content="lib       " Height="Auto" Name="label4" Width="56" BorderBrush="Black" BorderThickness="1,1,1,0" />
                        <TextBox Height="24" Name="textBox2" Width="304" />
                        <Button Content="..." Height="23" Name="button2" Width="20" />
                    </StackPanel>
                    <StackPanel Height="26" Name="stackPanel6" Width="Auto" Orientation="Horizontal">
                        <Label Content="bin      " Height="Auto" Name="label5" Width="56" BorderBrush="Black" BorderThickness="1,1,1,0" />
                        <TextBox Height="25" Name="textBox3" Width="304" />
                        <Button Content="..." Height="23" Name="button3" Width="20" />
                    </StackPanel>
                </StackPanel>
            </StackPanel>
	    <!-- x86.build_macros -->
            <StackPanel Height="26" Name="stackPanel86vcvars" Orientation="Horizontal" Width="Auto">
                <Label BorderBrush="Black" Content="build__macros" Height="Auto" HorizontalContentAlignment="Center" Name="label86vcvars" VerticalContentAlignment="Center" Width="107" BorderThickness="1,0,1,1" />
                <StackPanel Height="Auto" Name="stackPanel86vcvars_1" Orientation="Horizontal" Width="Auto">
			<TextBox Height="24" Name="textBox86vcvars" Width="304" />
                        <Button Content="..." Height="23" Name="button86vcvars" Width="20" />
                 </StackPanel>
            </StackPanel>
	    <!-- x64 -->
            <StackPanel Orientation="Horizontal">
                <Label Content="x64" Height="26" HorizontalAlignment="Left" HorizontalContentAlignment="Center" Name="label13" VerticalAlignment="Top" Width="43" />
		<TextBox Height="Auto" Name="versionBox2" Width="30" />
            </StackPanel>
            <StackPanel Height="78" Name="stackPanel16" Orientation="Horizontal" Width="Auto">
                <Label BorderBrush="Black" Content="libpq" Height="Auto" HorizontalContentAlignment="Center" Name="label14" VerticalContentAlignment="Center" Width="51" BorderThickness="1,1,0,0" />
                <StackPanel Height="Auto" Name="stackPanel17" Width="380">
                    <StackPanel Height="26" Name="stackPanel18" Orientation="Horizontal" Width="Auto">
                        <Label Content="include" Height="Auto" Name="label15" Width="56" BorderThickness="1,1,1,0" BorderBrush="Black" />
                        <TextBox Height="24" Name="textBox9" Width="304" />
                        <Button Content="..." Height="23" Name="button9" Width="20" />
                    </StackPanel>
                    <StackPanel Height="26" Name="stackPanel19" Orientation="Horizontal" Width="Auto">
                        <Label BorderBrush="Black" Content="lib       " Height="Auto" Name="label16" Width="56" BorderThickness="1,1,1,0" />
                        <TextBox Height="24" Name="textBox10" Width="304" />
                        <Button Content="..." Height="23" Name="button10" Width="20" />
                    </StackPanel>
                    <StackPanel Height="26" Name="stackPanel20" Orientation="Horizontal" Width="Auto">
                        <Label BorderBrush="Black" Content="bin      " Height="Auto" Name="label17" Width="56" BorderThickness="1,1,1,0" />
                        <TextBox Height="25" Name="textBox11" Width="304" />
                        <Button Content="..." Height="23" Name="button11" Width="20" />
                    </StackPanel>
                </StackPanel>
            </StackPanel>
	    <!-- x64.build_macros -->
            <StackPanel Height="26" Name="stackPanel64vcvars" Orientation="Horizontal" Width="Auto">
                <Label BorderBrush="Black" Content="build__macros" Height="Auto" HorizontalContentAlignment="Center" Name="label64vcvars" VerticalContentAlignment="Center" Width="107" BorderThickness="1,0,1,1" />
                <StackPanel Height="Auto" Name="stackPanel64vcvars_1" Orientation="Horizontal" Width="Auto">
			<TextBox Height="24" Name="textBox64vcvars" Width="304" />
                        <Button Content="..." Height="23" Name="button64vcvars" Width="20" />
                 </StackPanel>
            </StackPanel>

        </StackPanel>
    </Grid>
</Window>
'@

$reader=(New-Object System.Xml.XmlNodeReader $xaml)
$window=[Windows.Markup.XamlReader]::Load( $reader )

$buttonEnd = $window.FindName("buttonEnd")
$buttonEnd_clicked = $buttonEnd.add_Click
$buttonEnd_clicked.Invoke({
	Remove-Module Psqlodbc-config
	$window.close()
})

$button_click =
{
    ($sender, $e) = $this, $_
    # sender（$this）
	[void] [Reflection.Assembly]::LoadWithPartialName('System.Windows.Forms')
	$d = New-Object Windows.Forms.FolderBrowserDialog
	if ($d.ShowDialog() -eq "OK") {
        $lname = $sender.Name.substring(6)
		$text = $window.FindName("textBox" + $lname)
		$text.Text = $d.SelectedPath
    }
}

for ($i = 1; $i -lt 17; $i++)
{
	$button = $window.FindName("button" + $i)
	if ($button)
	{
		$button.add_Click($button_click)
	}
}

$button_click2 =
{
    ($sender, $e) = $this, $_
    # sender（$this）
	[void] [Reflection.Assembly]::LoadWithPartialName('System.Windows.Forms')
	$d = New-Object Windows.Forms.OpenFileDialog
	$d.InitialDirectory = $scriptPath
	if ($d.ShowDialog() -eq "OK") {
        	$lname = $sender.Name.substring(6)
		$text = $window.FindName("textBox" + $lname)
		$text.Text = $d.FileName
    	}
}

foreach ($btnname in ("button86vcvars", "button64vcvars"))
{
	$button = $window.FindName($btnname)
	$button.add_Click($button_click2)
}

$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
Import-Module "$scriptPath\Psqlodbc-config.psm1"
$configInfo = LoadConfiguration $configPath $scriptPath

$window.findName("versionBox").Text = $configInfo.Configuration.version
$window.findName("vcversionBox").Text = $configInfo.Configuration.vcversion
$window.findName("toolsetBox").Text = $configInfo.Configuration.toolset

$x86info = $configInfo.Configuration.x86
$window.findName("versionBox1").Text = $x86info.libpq.version
$window.findName("textBox1").Text = $x86info.libpq.include
$window.findName("textBox2").Text = $x86info.libpq.lib
$window.findName("textBox3").Text = $x86info.libpq.bin
$window.findName("textBox86vcvars").Text = $x86info.build_macros

$x64info = $configInfo.Configuration.x64

$window.findName("versionBox2").Text = $x64info.libpq.version
$window.findName("textBox9").Text = $x64info.libpq.include
$window.findName("textBox10").Text = $x64info.libpq.lib
$window.findName("textBox11").Text = $x64info.libpq.bin
$window.findName("textBox64vcvars").Text = $x64info.build_macros

$buttonSave = $window.FindName("buttonSave")
$buttonSave_clicked = $buttonSave.add_Click
$buttonSave_clicked.Invoke({
	$configInfo.Configuration.version = $window.findName("versionBox").Text
	$configInfo.Configuration.vcversion = $window.findName("vcversionBox").Text
	$configInfo.Configuration.toolset = $window.findName("toolsetBox").Text
	$x86info.libpq.version = $window.findName("versionBox1").Text
	$x86info.libpq.include = $window.findName("textBox1").Text
	$x86info.libpq.lib = $window.findName("textBox2").Text
	$x86info.libpq.bin = $window.findName("textBox3").Text
	$x86info.build_macros = $window.findName("textBox86vcvars").Text

	$x64info.libpq.version = $window.findName("versionBox2").Text
	$x64info.libpq.include = $window.findName("textBox9").Text
	$x64info.libpq.lib = $window.findName("textBox10").Text
	$x64info.libpq.bin = $window.findName("textBox11").Text
	$x64info.build_macros = $window.findName("textBox64vcvars").Text

	SaveConfiguration $configInfo
})

$window.ShowDialog() | out-null
