pushd "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\"
cmd /c "vcvars32.bat&set" |
 foreach {
   if ($_ -match "=") {
     $v = $_.split("="); set-item -force -path "ENV:\$($v[0])"  -value "$($v[1])"
   }
 }
 popd