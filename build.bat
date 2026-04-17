@echo off

"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" "%~dp0Nightsharp\NightSharp.sln" /p:Configuration=Release /p:Platform=x64 /m
if %errorlevel% neq 0 (
    echo BUILD FAILED
    pause
    exit /b %errorlevel%
)
echo BUILD OK
pause
