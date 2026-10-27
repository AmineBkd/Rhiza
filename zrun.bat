@echo off
setlocal

rem Runs the last build of the game. Build it first with zbuild.bat.
rem
rem   zrun.bat                  -> Debug
rem   zrun.bat RelWithDebInfo   -> RelWithDebInfo

set "config=%~1"
if "%config%"=="" set "config=Debug"

if not exist "bin\%config%\Game.exe" (
    echo zrun.bat: no Game.exe in bin\%config%\ - run zbuild.bat %config% first 1>&2
    exit /b 1
)
"bin\%config%\Game.exe"
