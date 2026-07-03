@echo off
set "MSBUILD=E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe"
set "SLN=C:\Users\MR THINH\Downloads\New\NightSharp\NightSharp.sln"

"%MSBUILD%" "%SLN%" /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m /v:minimal
pause