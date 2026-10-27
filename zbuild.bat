@echo off
setlocal

rem Configures and builds, from cmd or PowerShell.
rem
rem   zbuild.bat                  -> Debug
rem   zbuild.bat RelWithDebInfo   -> optimised, with symbols
rem   zbuild.bat Release          -> optimised

set "config=%~1"
if "%config%"=="" set "config=Debug"

cmake --preset windows
if errorlevel 1 exit /b 1

rem Visual Studio picks the config at build time, not configure time.
cmake --build --preset windows --config %config%
if errorlevel 1 exit /b 1

echo zbuild.bat: built %config%
