@echo off
set "MSBUILD=E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe"
set "SLN=C:\Users\MR THINH\Downloads\Script-New-183d53ac031a5d2815e67ba1d19d27a79e470107\Script-New-183d53ac031a5d2815e67ba1d19d27a79e470107\Nightsharp\NightSharp.sln"

"%MSBUILD%" "%SLN%" /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m /v:minimal
pause