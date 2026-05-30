@echo off
setlocal

REM Usage:
REM   deploy_example.bat C:\Tools\x64dbg\release\x64 x64
REM   deploy_example.bat C:\Tools\x64dbg\release\x32 x32

if "%~1"=="" (
  echo Usage: %~nx0 ^<x64dbg-release-arch-dir^> ^<x64^|x32^>
  exit /b 1
)
if "%~2"=="" (
  echo Usage: %~nx0 ^<x64dbg-release-arch-dir^> ^<x64^|x32^>
  exit /b 1
)

set XDBGDIR=%~1
set ARCH=%~2

if /i "%ARCH%"=="x64" (
  set PLUGIN=bin\x64\ScyllaXBridgePlugin.dp64
) else (
  set PLUGIN=bin\x32\ScyllaXBridgePlugin.dp32
)

if not exist "%XDBGDIR%\plugins" mkdir "%XDBGDIR%\plugins"
copy /y "%PLUGIN%" "%XDBGDIR%\plugins\"
copy /y "bin\%ARCH%\ScyllaX.exe" "%XDBGDIR%\plugins\"

echo Done.
