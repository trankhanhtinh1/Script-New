@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "PROJECT=%SCRIPT_DIR%NightSharp.vcxproj"
set "PLATFORM=x64"
set "CONFIG=%~1"

if not defined CONFIG set "CONFIG=DevFast"
if /i "%CONFIG%"=="/?" goto :usage
if /i "%CONFIG%"=="help" goto :usage

set "PROFILE=0"
set "NO_COPY=0"
for %%A in (%*) do (
    if /i "%%~A"=="profile" set "PROFILE=1"
    if /i "%%~A"=="nocopy" set "NO_COPY=1"
)

if not exist "%PROJECT%" (
    echo ERROR: Project not found: "%PROJECT%"
    goto :failed
)

call :find_msbuild
if not defined MSBUILD (
    echo ERROR: MSBuild was not found.
    goto :failed
)

if "%PROFILE%"=="1" (
    echo Profiling compiler and linker timings.
    set "CL=/Bt+ !CL!"
    set "LINK=/time !LINK!"
)

echo Building NightSharp %CONFIG% ^(%PLATFORM%^) with incremental MSBuild...
echo MSBuild: "%MSBUILD%"
"%MSBUILD%" "%PROJECT%" /m /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /v:minimal
if errorlevel 1 goto :failed

if "%NO_COPY%"=="1" goto :success

set "COPY_TARGET=%NIGHTSHARP_COPY_TARGET%"
if not defined COPY_TARGET set "COPY_TARGET=%USERPROFILE%\Desktop\gg"
set "OUTPUT=%SCRIPT_DIR%bin\%CONFIG%\NightSharp.dll"
if not exist "%OUTPUT%" (
    echo ERROR: Build output not found: "%OUTPUT%"
    goto :failed
)
if not exist "%COPY_TARGET%" (
    echo Copy skipped: target does not exist: "%COPY_TARGET%"
    goto :success
)

copy /Y "%OUTPUT%" "%COPY_TARGET%\" >nul
if errorlevel 1 (
    echo ERROR: Could not copy NightSharp.dll to "%COPY_TARGET%"
    goto :failed
)
echo Copied NightSharp.dll to "%COPY_TARGET%"
goto :success

:find_msbuild
set "MSBUILD="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\amd64\MSBuild.exe`) do if not defined MSBUILD set "MSBUILD=%%I"
)
if defined MSBUILD goto :eof

for %%P in (
    "%ProgramFiles%\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
) do if not defined MSBUILD if exist "%%~P" set "MSBUILD=%%~P"
goto :eof

:usage
echo Usage:
echo   mybuild.bat [DevFast^|Debug^|Release] [profile] [nocopy]
echo.
echo Defaults to DevFast and copies the DLL to %%USERPROFILE%%\Desktop\gg.
echo Set NIGHTSHARP_COPY_TARGET to override the copy destination.
exit /b 0

:success
echo BUILD OK
exit /b 0

:failed
echo BUILD FAILED
if "%NIGHTSHARP_PAUSE%"=="1" pause
exit /b 1
