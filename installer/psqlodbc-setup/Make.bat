setlocal
set wix_dir=%WIX%bin
set pgmname=psqlodbc
set build_config=Release
set objdir=obj\%build_config%
set bindir=bin\%build_config%

"%wix_dir%\candle.exe" -v ^
-dConfiguration=%build_config% ^
-dOutDir=%bindir%\ -dPlatform=x86 ^
-d"ProjectDir=\\" -dProjectExt=.wixproj ^
-dProjectFileName=%pgmname%.wixproj -dProjectName=%pgmname% ^
-d"ProjectPath=%pgmname%.wixproj" ^
-d"TargetDir=%bindir%\\" -dTargetExt=.exe ^
-dTargetFileName=%pgmname%.exe -dTargetName=%pgmname% ^
-d"TargetPath=%bindir%\%pgmname%.exe" ^
-out %objdir%\ -arch x86 ^
-ext "%wix_dir%\WixUtilExtension.dll" ^
-ext "%wix_dir%\WixBalExtension.dll" ^
Bundle.wxs vcredist.wxs

"%wix_dir%\Light.exe" -out %bindir%\%pgmname%.exe ^
-pdbout %bindir%\%pgmname%.wixpdb ^
-ext "%wix_dir%\\WixUtilExtension.dll" ^
-ext "%wix_dir%\\WixBalExtension.dll" ^
-contentsfile %objdir%\%pgmname%.wixproj.BindContentsFileList.txt ^
-outputsfile %objdir%\%pgmname%.wixproj.BindOutputsFileList.txt ^
-builtoutputsfile %objdir%\%pgmname%.wixproj.BindBuiltOutputsFileList.txt ^
-wixprojectfile %pgmname%.wixproj ^
%objdir%\Bundle.wixobj %objdir%\vcredist.wixobj

endlocal
