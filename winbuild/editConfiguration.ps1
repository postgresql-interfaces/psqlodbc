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
        Title="MainWindow" Height="640" Width="539" BorderBrush="Black" Margin="30,0,0,0">
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
            </StackPanel>
            <StackPanel Orientation="Horizontal">
                <Label Content="x86" Height="26" Name="label1" Width="43" HorizontalContentAlignment="Center" HorizontalAlignment="Left" VerticalAlignment="Top" />
                <CheckBox Content="libpq" Height="Auto" HorizontalContentAlignment="Center" Name="checkBox1" VerticalContentAlignment="Center" Width="51" BorderBrush="Black" />
		<TextBox Height="Auto" Name="versionBox1" Width="30" />
                <CheckBox Content="gss" Height="Auto" HorizontalContentAlignment="Center" Name="checkBox2" VerticalContentAlignment="Center" Width="51" BorderBrush="Black" />
                <CheckBox Content="sspi" Height="Auto" HorizontalContentAlignment="Center" Name="checkBox3" VerticalContentAlignment="Center" Width="51" BorderBrush="Black" />
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
            <StackPanel Height="78" Name="stackPanel7" Orientation="Horizontal" Width="Auto">
                <Label BorderBrush="Black" Content="gss" Height="Auto" HorizontalContentAlignment="Center" Name="label6" VerticalContentAlignment="Center" Width="51" BorderThickness="1,1,0,0" />
                <StackPanel Height="Auto" Name="stackPanel8" Width="380">
                    <StackPanel Height="26" Name="stackPanel9" Orientation="Horizontal" Width="Auto">
                        <Label Content="include" Height="Auto" Name="label7" Width="56" BorderThickness="1,1,1,0" BorderBrush="Black" />
                        <TextBox Height="24" Name="textBox4" Width="304" />
                        <Button Content="..." Height="23" Name="button4" Width="20" />
                    </StackPanel>
                    <StackPanel Height="26" Name="stackPanel10" Orientation="Horizontal" Width="Auto">
                        <Label BorderBrush="Black" Content="lib       " Height="Auto" Name="label8" Width="56" BorderThickness="1,1,1,0" />
                        <TextBox Height="24" Name="textBox5" Width="304" />
                        <Button Content="..." Height="23" Name="button5" Width="20" />
                    </StackPanel>
                    <StackPanel Height="26" Name="stackPanel11" Orientation="Horizontal" Width="Auto">
                        <Label BorderBrush="Black" Content="bin      " Height="Auto" Name="label9" Width="56" BorderThickness="1,1,1,0" />
                        <TextBox Height="25" Name="textBox6" Width="304" />
                        <Button Content="..." Height="23" Name="button6" Width="20" />
                    </StackPanel>
                </StackPanel>
            </StackPanel>
            <StackPanel Height="52" Name="stackPanel12" Orientation="Horizontal" Width="Auto">
                <Label BorderBrush="Black" Content="ssl" Height="Auto" HorizontalContentAlignment="Center" Name="label10" VerticalContentAlignment="Center" Width="51" BorderThickness="1,1,0,1" />
                <StackPanel Height="Auto" Name="stackPanel13" Width="380">
                    <StackPanel Height="26" Name="stackPanel14" Orientation="Horizontal" Width="Auto">
                        <Label Content="include" Height="Auto" Name="label11" Width="56" BorderThickness="1,1,1,0" BorderBrush="Black" />
                        <TextBox Height="24" Name="textBox7" Width="304" />
                        <Button Content="..." Height="23" Name="button7" Width="20" />
                    </StackPanel>
                    <StackPanel Height="26" Name="stackPanel15" Orientation="Horizontal" Width="Auto">
                        <Label BorderBrush="Black" Content="lib       " Height="Auto" Name="label12" Width="56" BorderThickness="1" />
                        <TextBox Height="24" Name="textBox8" Width="304" />
                        <Button Content="..." Height="23" Name="button8" Width="20" />
                    </StackPanel>
                 </StackPanel>
            </StackPanel>
            <StackPanel Orientation="Horizontal">
                <Label Content="x64" Height="26" HorizontalAlignment="Left" HorizontalContentAlignment="Center" Name="label13" VerticalAlignment="Top" Width="43" />
                <CheckBox BorderBrush="Black" Content="libpq" Height="Auto" HorizontalContentAlignment="Center" Name="checkBox4" VerticalContentAlignment="Center" Width="51" />
		<TextBox Height="Auto" Name="versionBox2" Width="30" />
                <CheckBox BorderBrush="Black" Content="gss" Height="Auto" HorizontalContentAlignment="Center" Name="checkBox5" VerticalContentAlignment="Center" Width="51" />
                <CheckBox BorderBrush="Black" Content="sspi" Height="Auto" HorizontalContentAlignment="Center" Name="checkBox6" VerticalContentAlignment="Center" Width="51" />
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
            <StackPanel Height="78" Name="stackPanel21" Orientation="Horizontal" Width="Auto">
                <Label BorderBrush="Black" Content="gss" Height="Auto" HorizontalContentAlignment="Center" Name="label18" VerticalContentAlignment="Center" Width="51" BorderThickness="1,1,0,0" />
                <StackPanel Height="Auto" Name="stackPanel22" Width="380">
                    <StackPanel Height="26" Name="stackPanel23" Orientation="Horizontal" Width="Auto">
                        <Label Content="include" Height="Auto" Name="label19" Width="56" BorderThickness="1,1,1,0" BorderBrush="Black" />
                        <TextBox Height="24" Name="textBox12" Width="304" />
                        <Button Content="..." Height="23" Name="button12" Width="20" />
                    </StackPanel>
                    <StackPanel Height="26" Name="stackPanel24" Orientation="Horizontal" Width="Auto">
                        <Label BorderBrush="Black" Content="lib       " Height="Auto" Name="label20" Width="56" BorderThickness="1,1,1,0" />
                        <TextBox Height="24" Name="textBox13" Width="304" />
                        <Button Content="..." Height="23" Name="button13" Width="20" />
                    </StackPanel>
                    <StackPanel Height="26" Name="stackPanel25" Orientation="Horizontal" Width="Auto">
                        <Label BorderBrush="Black" Content="bin      " Height="Auto" Name="label21" Width="56" BorderThickness="1,1,1,0" />
                        <TextBox Height="25" Name="textBox14" Width="304" />
                        <Button Content="..." Height="23" Name="button14" Width="20" />
                    </StackPanel>
                </StackPanel>
            </StackPanel>
            <StackPanel Height="52" Name="stackPanel26" Orientation="Horizontal" Width="Auto">
                <Label BorderBrush="Black" Content="ssl" Height="Auto" HorizontalContentAlignment="Center" Name="label22" VerticalContentAlignment="Center" Width="51" BorderThickness="1,1,0,1" />
                <StackPanel Height="Auto" Name="stackPanel27" Width="380">
                    <StackPanel Height="26" Name="stackPanel28" Orientation="Horizontal" Width="Auto">
                        <Label Content="include" Height="Auto" Name="label23" Width="56" BorderThickness="1,1,1,0" BorderBrush="Black" />
                        <TextBox Height="24" Name="textBox15" Width="304" />
                        <Button Content="..." Height="23" Name="button15" Width="20" />
                    </StackPanel>
                    <StackPanel Height="26" Name="stackPanel29" Orientation="Horizontal" Width="Auto">
                        <Label BorderBrush="Black" Content="lib       " Height="Auto" Name="label24" Width="56" BorderThickness="1" />
                        <TextBox Height="24" Name="textBox16" Width="304" />
                        <Button Content="..." Height="23" Name="button16" Width="20" />
                    </StackPanel>
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
	$window.close()
})

$button_click =
{
    ($sender, $e) = $this, $_
    # senderÅi$thisÅj
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
    $button.add_Click($button_click)
}

$scriptPath = (Split-Path $MyInvocation.MyCommand.Path)
$configInfo = & "$scriptPath\configuration.ps1" "$configPath"

$window.findName("versionBox").Text = $configInfo.Configuration.version

$x86info = $configInfo.Configuration.x86
$window.findName("checkBox1").isChecked = ($x86info.use_libpq -eq "yes")
$window.findName("versionBox1").Text = $x86info.libpq.version
$window.findName("checkBox2").isChecked = ($x86info.use_gss -eq "yes")
$window.findName("checkBox3").isChecked = ($x86info.use_sspi -eq "yes")
$window.findName("textBox1").Text = $x86info.libpq.include
$window.findName("textBox2").Text = $x86info.libpq.lib
$window.findName("textBox3").Text = $x86info.libpq.bin
$window.findName("textBox4").Text = $x86info.gss.include
$window.findName("textBox5").Text = $x86info.gss.lib
$window.findName("textBox6").Text = $x86info.gss.bin
$window.findName("textBox7").Text = $x86info.ssl.include
$window.findName("textBox8").Text = $x86info.ssl.lib

$x64info = $configInfo.Configuration.x64

$window.findName("checkBox4").isChecked = ($x64info.use_libpq -eq "yes")
$window.findName("versionBox2").Text = $x64info.libpq.version
$window.findName("checkBox5").isChecked = ($x64info.use_gss -eq "yes")
$window.findName("checkBox6").isChecked = ($x64info.use_sspi -eq "yes")
$window.findName("textBox9").Text = $x64info.libpq.include
$window.findName("textBox10").Text = $x64info.libpq.lib
$window.findName("textBox11").Text = $x64info.libpq.bin
$window.findName("textBox12").Text = $x64info.gss.include
$window.findName("textBox13").Text = $x64info.gss.lib
$window.findName("textBox14").Text = $x64info.gss.bin
$window.findName("textBox15").Text = $x64info.ssl.include
$window.findName("textBox16").Text = $x64info.ssl.lib

$buttonSave = $window.FindName("buttonSave")
$buttonSave_clicked = $buttonSave.add_Click
$buttonSave_clicked.Invoke({
	$configInfo.Configuration.version = $window.findName("versionBox").Text  
	$x86info.use_libpq = $(if ($window.findName("checkBox1").isChecked) {"yes"} else {"no"})
	$x86info.use_gss = $(if ($window.findName("checkBox2").isChecked) {"yes"} else {"no"})
	$x86info.use_sspi = $(if ($window.findName("checkBox3").isChecked) {"yes"} else {"no"})
	$x86info.libpq.include = $window.findName("textBox1").Text  
	$x86info.libpq.lib = $window.findName("textBox2").Text 
	$x86info.libpq.bin = $window.findName("textBox3").Text
	$x86info.gss.include = $window.findName("textBox4").Text
	$x86info.gss.lib = $window.findName("textBox5").Text
	$x86info.gss.bin = $window.findName("textBox6").Text
	$x86info.ssl.include = $window.findName("textBox7").Text
	$x86info.ssl.lib = $window.findName("textBox8").Text
	

	$x64info.use_libpq = $(if ($window.findName("checkBox4").isChecked) {"yes"} else {"no"})
	$x64info.use_gss = $(if ($window.findName("checkBox5").isChecked) {"yes"} else {"no"})
	$x64info.use_sspi = $(if ($window.findName("checkBox6").isChecked) {"yes"} else {"no"})
	$x64info.libpq.include = $window.findName("textBox9").Text
	$x64info.libpq.lib = $window.findName("textBox10").Text
	$x64info.libpq.bin = $window.findName("textBox11").Text
	$x64info.gss.include = $window.findName("textBox12").Text
	$x64info.gss.lib = $window.findName("textBox13").Text
	$x64info.gss.bin = $window.findName("textBox14").Text
	$x64info.ssl.include = $window.findName("textBox15").Text
	$x64info.ssl.lib = $window.findName("textBox16").Text

	SaveConfiguration $configInfo
})

$window.ShowDialog() | out-null
