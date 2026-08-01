@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "REPO_DIR=%~dp0"
set "NIGHTSHARP_DIR=%REPO_DIR%Nightsharp"
set "PROJECT=%NIGHTSHARP_DIR%\NightSharp.vcxproj"
set "FAILED=0"
set "REMOVED_DIRS=0"
set "REMOVED_FILES=0"

if not exist "%PROJECT%" (
    echo ERROR: NightSharp project not found: "%PROJECT%"
    exit /b 1
)

if /i not "%~1"=="/y" if /i not "%~1"=="-y" (
    echo This will remove generated NightSharp build artifacts:
    echo   - Nightsharp\bin
    echo   - Nightsharp\obj
    echo   - Nightsharp\x64
    echo   - Nightsharp\build
    echo   - generated files under Nightsharp\tests
    echo.
    choice /C YN /N /M "Continue? [Y/N] "
    if errorlevel 2 (
        echo Clean cancelled.
        exit /b 0
    )
)

for %%D in (
    "%NIGHTSHARP_DIR%\bin"
    "%NIGHTSHARP_DIR%\obj"
    "%NIGHTSHARP_DIR%\x64"
    "%NIGHTSHARP_DIR%\build"
) do (
    if exist "%%~D" (
        echo Removing "%%~D"
        rmdir /S /Q "%%~D"
        if errorlevel 1 (
            echo ERROR: Could not remove "%%~D"
            set "FAILED=1"
        ) else (
            set /A REMOVED_DIRS+=1
        )
    )
)

for %%F in (
    "%NIGHTSHARP_DIR%\tests\*.obj"
    "%NIGHTSHARP_DIR%\tests\*.exe"
    "%NIGHTSHARP_DIR%\tests\*.dll"
    "%NIGHTSHARP_DIR%\tests\*.pdb"
    "%NIGHTSHARP_DIR%\tests\*.ilk"
    "%NIGHTSHARP_DIR%\tests\*.lib"
    "%NIGHTSHARP_DIR%\tests\*.exp"
    "%NIGHTSHARP_DIR%\tests\*.pch"
) do (
    if exist "%%~F" (
        echo Removing "%%~F"
        del /Q /F "%%~F"
        if errorlevel 1 (
            echo ERROR: Could not remove "%%~F"
            set "FAILED=1"
        ) else (
            set /A REMOVED_FILES+=1
        )
    )
)

if "!FAILED!"=="1" (
    echo CLEAN FAILED
    exit /b 1
)

echo Clean complete: !REMOVED_DIRS! output folder(s), !REMOVED_FILES! generated test file(s) removed.
exit /b 0
