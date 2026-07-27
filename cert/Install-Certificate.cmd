@echo off
setlocal
echo Installing Waypoint Root Certificate Authority...
echo This will allow Windows to trust the Waypoint Driver.
echo.
echo Requesting Administrative Privileges...
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Success: Administrative permissions confirmed.
) else (
    echo Error: Please right-click and run this script as Administrator.
    pause
    exit /b 1
)

set CER_FILE="%~dp0Waypoint.cer"
if not exist %CER_FILE% (
    echo Error: Could not find %CER_FILE%
    pause
    exit /b 1
)

echo Importing Root CA into Local Machine Trusted Root Store...
certutil -addstore -f "Root" %CER_FILE%
if %errorLevel% neq 0 (
    echo Failed to install Root Certificate.
    pause
    exit /b 1
)

echo Importing Root CA into Local Machine Trusted Publishers Store...
certutil -addstore -f "TrustedPublisher" %CER_FILE%
if %errorLevel% neq 0 (
    echo Failed to install Trusted Publisher Certificate.
    pause
    exit /b 1
)

echo.
echo Certificate successfully installed!
echo.
echo NOTE: Since this is a self-signed certificate, you MUST also enable Test Mode.
echo To do this, run: bcdedit /set testsigning on
echo Then reboot your computer.
echo.
pause
