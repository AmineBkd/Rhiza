@echo off
setlocal

rem Runs the last build's tests. Build it first with zbuild.bat.
rem
rem   ztest.bat                  -> Debug
rem   ztest.bat RelWithDebInfo   -> RelWithDebInfo

set "config=%~1"
if "%config%"=="" set "config=Debug"

rem Without a build, ctest finds no tests and would call that a pass.
ctest --test-dir build -C %config% --output-on-failure --no-tests=error
if errorlevel 1 exit /b 1
