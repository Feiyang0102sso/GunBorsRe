@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Use the installed VS build tools without starting the IDE.
pushd "%~dp0" || exit /b 1
set "BUILD_EXIT=1"
set "BUILD_CONFIG=%~1"
if not defined BUILD_CONFIG set "BUILD_CONFIG=All"
if /i not "%BUILD_CONFIG%"=="All" if /i not "%BUILD_CONFIG%"=="Debug" if /i not "%BUILD_CONFIG%"=="Release" goto :usage
if not "%~2"=="" if /i not "%~2"=="/nopause" goto :usage

set "BUILD_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%BUILD_VSWHERE%" (
    echo [build] Visual Studio Installer not found. Install Visual Studio with Desktop development with C++.
    goto :finish
)
set "BUILD_MSBUILD="
for /f "usebackq delims=" %%I in (`"%BUILD_VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find MSBuild\**\Bin\MSBuild.exe`) do (
    if not defined BUILD_MSBUILD set "BUILD_MSBUILD=%%I"
)
if not defined BUILD_MSBUILD (
    echo [build] MSBuild with C++ tools not found. Install Desktop development with C++.
    goto :finish
)
if not exist "obj\" mkdir "obj"

rem The Game target also builds Viewer and Tests using the project rules.
if /i not "%BUILD_CONFIG%"=="All" goto :single
call :build Debug
if errorlevel 1 goto :build_failed
call :build Release
if errorlevel 1 goto :build_failed
goto :success

:single
call :build "%BUILD_CONFIG%"
if errorlevel 1 goto :build_failed

:success
set "BUILD_EXIT=0"
echo [build] All requested builds passed. Executables are in bin\Debug or bin\Release.
goto :finish

:build_failed
set "BUILD_EXIT=%errorlevel%"
echo [build] Build failed. See the errors above and obj\build-*.log.
goto :finish

:usage
set "BUILD_EXIT=2"
echo Usage: build.bat [All^|Debug^|Release] [/nopause]
echo Default: build Debug and Release, then wait for a key.

:finish
echo [build] Exit code: %BUILD_EXIT%
popd
if /i not "%~2"=="/nopause" pause
exit /b %BUILD_EXIT%

:build
echo [build] Building %~1 x64: Game, Viewer, Tests...
rem Explorer may not have PowerShell 7 on PATH. This entry point only compiles.
echo [build] Automatic checks are skipped; the test executable is still built.
"%BUILD_MSBUILD%" "%~dp0GunBrosRe.vcxproj" /t:Build /p:GbProduct=Game /p:Configuration=%~1 /p:Platform=x64 /p:SkipCompanions=false /p:SkipAutoTests=true /m /nologo /v:minimal "/flp:logfile=%~dp0obj\build-%~1.log;verbosity=normal"
exit /b %errorlevel%
