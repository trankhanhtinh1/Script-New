@echo off
setlocal

REM Try to find MSBuild using vswhere (Visual Studio 2017+)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
        set "MSBUILD=%%i"
    )
)

REM Fallback: Try common Visual Studio paths
if not defined MSBUILD (
    for %%v in (2022 2019 2017) do (
        for %%e in (Enterprise Professional Community BuildTools) do (
            set "TESTPATH=%ProgramFiles%\Microsoft Visual Studio\%%v\%%e\MSBuild\Current\Bin\amd64\MSBuild.exe"
            if exist "!TESTPATH!" (
                set "MSBUILD=!TESTPATH!"
                goto :found
            )
            set "TESTPATH=%ProgramFiles(x86)%\Microsoft Visual Studio\%%v\%%e\MSBuild\Current\Bin\amd64\MSBuild.exe"
            if exist "!TESTPATH!" (
                set "MSBUILD=!TESTPATH!"
                goto :found
            )
        )
    )
)

:found
if not defined MSBUILD (
    echo ERROR: MSBuild not found. Please install Visual Studio 2017 or later.
    exit /b 1
)

echo Using MSBuild: %MSBUILD%
"%MSBUILD%" "%~dp0NightSharp.sln" /p:Configuration=Release /p:Platform=x64 /m
if %errorlevel% neq 0 (
    echo BUILD FAILED
    exit /b %errorlevel%
)
echo BUILD OK