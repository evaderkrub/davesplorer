@echo off
rem Configure, build and test with MSVC + Ninja. Usage:
rem   build.cmd [release|debug] [configure|build|test|all]
setlocal
set PRESET=%1
if "%PRESET%"=="" set PRESET=release
set STEP=%2
if "%STEP%"=="" set STEP=all

rem Locate Visual Studio through vswhere rather than a hard-coded version.
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% set VSWHERE="%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" (
    echo Visual Studio with C++ tools not found.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

if "%STEP%"=="build" goto build
if "%STEP%"=="test" goto test

:configure
cmake --preset %PRESET% || exit /b 1
if "%STEP%"=="configure" exit /b 0
:build
cmake --build --preset %PRESET% || exit /b 1
if "%STEP%"=="build" exit /b 0
:test
ctest --preset %PRESET% || exit /b 1
exit /b 0
