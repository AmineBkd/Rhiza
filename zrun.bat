@echo off
setlocal

rem Runs the last build of the game. Build it first with zbuild.bat.
rem
rem   zrun.bat                  -> Debug
rem   zrun.bat RelWithDebInfo   -> RelWithDebInfo

set "config=%~1"
if "%config%"=="" set "config=Debug"

rem Visual Studio builds into bin\<config>\; other generators straight into bin\.
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
