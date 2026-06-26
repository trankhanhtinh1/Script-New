@echo off
set MSBUILD=E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe
set SLN=c:\Users\MR THINH\Downloads\New\NightSharp yungblod_ dev\NightSharp.sln
"%MSBUILD%" "%SLN%" /p:Configuration=Release /p:Platform=x64 /m /v:minimal
