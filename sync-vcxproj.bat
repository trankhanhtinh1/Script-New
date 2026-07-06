@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Nightsharp\sync-vcxproj.ps1" %*
exit /b %errorlevel%
