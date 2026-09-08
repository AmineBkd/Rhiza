@echo off
setlocal

rem Configures and builds, for cmd/PowerShell where .sh is not runnable.
rem zbuild.sh already covers Git Bash / MSYS2 via its MINGW case.
rem
rem The Visual Studio generator is multi-config: the configuration is chosen
rem at build time rather than configure time, so it has to be passed to
rem --build. Without it MSBuild picks Debug, which then needs the _d-suffixed
rem OgreNext libraries - see RHIZA_DEBUG_BUILD in engine\src\render\Renderer.cpp.
rem
rem   zbuild.bat            -> Debug
rem   zbuild.bat Release    -> Release

set "config=%~1"
if "%config%"=="" set "config=Debug"

cmake --preset windows
if errorlevel 1 exit /b 1

cmake --build --preset windows --config %config%
if errorlevel 1 exit /b 1

echo zbuild.bat: built %config%
