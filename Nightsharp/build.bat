@echo off
set MSBUILD=E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe
set SLN=C:\Users\MR THINH\Desktop\Script-New\Nightsharp\NightSharp.sln
"%MSBUILD%" "%SLN%" /p:Configuration=Release /p:Platform=x64 /m /v:minimal
