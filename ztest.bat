@echo off
setlocal

rem Runs the last build's tests. Use zbuild.bat with the same config first.
rem
rem   ztest.bat             -> RelWithDebInfo
rem   ztest.bat Release     -> Release

set "config=%~1"
if "%config%"=="" set "config=RelWithDebInfo"

rem Without a build, ctest finds no tests and would call that a pass.
ctest --test-dir build -C %config% --output-on-failure --no-tests=error
if errorlevel 1 exit /b 1
