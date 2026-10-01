@echo off
setlocal

rem Runs the last build. Use zbuild.bat to build first.
rem
rem   zrun.bat              -> RelWithDebInfo
rem   zrun.bat Release      -> Release

set "config=%~1"
if "%config%"=="" set "config=RelWithDebInfo"

rem The Visual Studio generator appends the configuration to
rem CMAKE_RUNTIME_OUTPUT_DIRECTORY, so the exe lands in bin\<config>\.
rem A single-config generator puts it straight in bin\, so try both.
if exist "bin\%config%\Game.exe" (
    "bin\%config%\Game.exe"
    goto :eof
)

if exist "bin\Game.exe" (
    "bin\Game.exe"
    goto :eof
)

echo zrun.bat: no Game.exe in bin\%config%\ or bin\ - run zbuild.bat %config% first 1>&2
exit /b 1
