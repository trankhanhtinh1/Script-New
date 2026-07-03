@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "SLN=%SCRIPT_DIR%NightSharp.sln"
set "CONFIG=%~1"
set "PLATFORM=%~2"
set "TARGET_DIR=%~3"

if "%CONFIG%"=="" set "CONFIG=Release"
if "%PLATFORM%"=="" set "PLATFORM=x64"
if "%TARGET_DIR%"=="" set "TARGET_DIR=C:\Users\weare\Desktop\goat"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "MSBUILD="

if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
        set "MSBUILD=%%i"
        goto :found_msbuild
    )
)

if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    goto :found_msbuild
)

if exist "E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe"
    goto :found_msbuild
)

echo MSBuild not found.
exit /b 1

:found_msbuild
if not exist "%SLN%" (
    echo Solution not found: "%SLN%"
    exit /b 1
)

echo Building "%SLN%" [%CONFIG%^|%PLATFORM%]...
"%MSBUILD%" "%SLN%" /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /m /v:minimal
if errorlevel 1 exit /b %errorlevel%

set "DLL=%SCRIPT_DIR%bin\%CONFIG%\NightSharp.dll"
if not exist "%DLL%" (
    echo Build succeeded but DLL was not found: "%DLL%"
    exit /b 1
)

if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"
if errorlevel 1 exit /b %errorlevel%

copy /Y "%DLL%" "%TARGET_DIR%\NightSharp.dll"
if errorlevel 1 exit /b %errorlevel%

echo Copied "%DLL%" to "%TARGET_DIR%\NightSharp.dll"
endlocal
