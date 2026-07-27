@echo off
setlocal EnableDelayedExpansion

set MSBUILD_PATH="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if not exist %MSBUILD_PATH% (
    echo [ERROR] MSBuild not found at %MSBUILD_PATH%
    exit /b 1
)

echo [INFO] Building Solution...
%MSBUILD_PATH% Waypoint.sln /p:Configuration=Release /p:Platform=x64 /p:WDKBuildFolder=10.0.26100.0 /p:RunInfVerif=false /p:RunInf2Cat=false /p:SignMode=Off /t:Rebuild
if not exist x64\Release\Waypoint.sys (
    echo [ERROR] MSBuild failed to compile Waypoint.sys.
    exit /b 1
)

set INF2CAT_PATH="C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x86\Inf2Cat.exe"
if not exist %INF2CAT_PATH% (
    set INF2CAT_PATH="C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\Inf2Cat.exe"
)
if not exist %INF2CAT_PATH% (
    echo [ERROR] Inf2Cat.exe not found.
    exit /b 1
)

echo [INFO] Cleaning up MSBuild package dir...
if exist x64\Release\Waypoint\ rmdir /s /q x64\Release\Waypoint\

echo [INFO] Generating Catalog File...
%INF2CAT_PATH% /driver:x64\Release\ /os:10_x64 /USELOCALTIME
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Inf2Cat failed.
    exit /b %ERRORLEVEL%
)

set SIGNTOOL_PATH="C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\signtool.exe"
if not exist %SIGNTOOL_PATH% (
    set SIGNTOOL_PATH="C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
)

echo [INFO] Signing Catalog and Driver using 'Waypoint Certificate'...
%SIGNTOOL_PATH% sign /v /fd sha256 /tr http://timestamp.digicert.com /td sha256 /n "Waypoint Certificate" x64\Release\Waypoint.cat
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to sign Catalog.
    exit /b %ERRORLEVEL%
)

%SIGNTOOL_PATH% sign /v /fd sha256 /tr http://timestamp.digicert.com /td sha256 /n "Waypoint Certificate" x64\Release\Waypoint.sys
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to sign Driver binary.
    exit /b %ERRORLEVEL%
)

%SIGNTOOL_PATH% sign /v /fd sha256 /tr http://timestamp.digicert.com /td sha256 /n "Waypoint Certificate" x64\Release\WaypointCtl.exe
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to sign Controller binary.
    exit /b %ERRORLEVEL%
)

echo.
echo [INFO] Build and Code Signing Successful.

exit /b 0
