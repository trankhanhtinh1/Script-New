@echo off

set BUILD_FILE=%~dp0build_number.txt
set VERSION_H=%~dp0Nightsharp\version.h

set /p BUILD_NUM=<%BUILD_FILE%
set /a BUILD_NUM=BUILD_NUM+1
echo %BUILD_NUM%>%BUILD_FILE%

(
    echo #pragma once
    echo #define NS_BUILD_NUMBER %BUILD_NUM%
    echo #define NS_VERSION_STRING "1.0.%BUILD_NUM%"
    echo #define NS_HEADER_STRING "NightSharp 1.0.%BUILD_NUM%"
) > %VERSION_H%

echo Build number: 1.0.%BUILD_NUM%

"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" "%~dp0Nightsharp\NightSharp.sln" /p:Configuration=Release /p:Platform=x64 /m
if %errorlevel% neq 0 (
    echo BUILD FAILED
    pause
    exit /b %errorlevel%
)
echo BUILD OK - Version 1.0.%BUILD_NUM%
pause
