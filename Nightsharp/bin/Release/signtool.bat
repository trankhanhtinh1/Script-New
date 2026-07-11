@echo off
:: Ensure the script runs from its own folder
cd /d "%~dp0"

:: 1. Change system date to 2023 (keeps current month and day)
echo Adjusting system date to 2023...
for /f "tokens=2-4 delims=/ " %%a in ('date /t') do (
    date %%a-%%b-2023
)

:: 2. Run the signing command (Removed timestamp server to enforce the 2023 date)
echo Signing nightsharp.dll...
signtool sign /f msi.pfx /p 2466 /fd SHA256 nightsharp.dll

:: 3. Force Windows to resync time automatically over the internet
echo Resyncing clock to automatic time...
net start w32time >nul 2>&1
w32tm /resync /force

pause
