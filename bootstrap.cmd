@echo off
::#
::# bootstrap.cmd - pull, build and install dekaf2 on Windows, with vcpkg
::#
::# usage: bootstrap.cmd [<options>] go
::#
::# with <options>:
::#
::#   -with-debug : build the Debug version as well
::#   -scratch    : clear the build directories of this run before building
::#                 (the vcpkg checkout in build\vcpkg stays)
::#   -no-pull    : do not pull the latest sources before building
::#   -no-klog    : configure without klog
::#   -lto        : force link time optimization for release builds
::#   -fresh      : recreate cmake build with fresh cache
::#   -utests     : build and run the unit tests after each build
::#   -j N        : build with N processes in parallel
::#                 (default is number of cpu cores)
::#
::# The Windows counterpart of "bootstrap -vcpkg": dekaf2 static, with every
::# library from vcpkg (triplet x64-windows-static or arm64-windows-static,
::# static runtime), built with the compiler, CMake and Ninja of Visual Studio
::# into build\<triplet>-Release (and -Debug) and installed into their install
::# subdirectories, where another project takes it from. The architecture is
::# that of an active Visual Studio developer environment, else that of this
::# machine.
::# vcpkg itself is checked out in build\vcpkg, at the builtin-baseline
::# of vcpkg\vcpkg.json. The build is configured with the preset
::# <triplet>-Release (or -Debug) of CMakePresets.json.
::#
::# Needs Visual Studio 2022 or later with the C++ workload, and git.
::#

setlocal EnableDelayedExpansion

rem the path of this script, saved before shift changes the batch parameters
set "BOOTSTRAP=%~f0"
set "DEKAF2=%~dp0"
set "DEKAF2=%DEKAF2:~0,-1%"
set "BUILDDIR=%DEKAF2%\build"
set "VCPKGDIR=%BUILDDIR%\vcpkg"
set "CONFIGS=Release"
set "SCRATCH="
set "NOPULL="
set "EXTRA="
set "JOBS=%NUMBER_OF_PROCESSORS%"
set "UTESTS="
set "GO="

rem ---- arguments ---------------------------------------------------------------
:args
if "%~1"=="" goto argsdone
if /i "%~1"=="-with-debug" (set "CONFIGS=Release Debug" & shift & goto args)
if /i "%~1"=="-scratch"    (set "SCRATCH=1" & shift & goto args)
if /i "%~1"=="-no-pull"    (set "NOPULL=1" & shift & goto args)
if /i "%~1"=="-nopull"     (set "NOPULL=1" & shift & goto args)
if /i "%~1"=="-no-klog"    (set "EXTRA=!EXTRA! -DDEKAF2_WITH_KLOG=OFF" & shift & goto args)
if /i "%~1"=="-lto"        (set "EXTRA=!EXTRA! -DDEKAF2_LINK_TIME_OPTIMIZATION=ON" & shift & goto args)
if /i "%~1"=="-fresh"      (set "EXTRA=!EXTRA! --fresh" & shift & goto args)
if /i "%~1"=="-j"          (set "JOBS=%~2" & shift & shift & goto args)
if /i "%~1"=="-utests"     (set "UTESTS=1" & shift & goto args)
if /i "%~1"=="go"          (set "GO=1" & shift & goto args)
if /i "%~1"=="-go"         (set "GO=1" & shift & goto args)
if /i "%~1"=="-help"       goto usage
if /i "%~1"=="-h"          goto usage
echo error: unknown option: %~1
goto usage
:argsdone
if not defined GO goto usage

rem ---- the target architecture -------------------------------------------------
rem that of an active developer environment, else that of this machine - when this
rem cmd runs emulated, PROCESSOR_ARCHITEW6432 holds the machine's architecture
set "ARCH="
set "INVSENV="
where cl >nul 2>&1
if not errorlevel 1 (
	set "INVSENV=1"
	set "ARCH=%VSCMD_ARG_TGT_ARCH%"
)
if not defined ARCH set "ARCH=%PROCESSOR_ARCHITEW6432%"
if not defined ARCH set "ARCH=%PROCESSOR_ARCHITECTURE%"
if /i "%ARCH%"=="amd64" set "ARCH=x64"
if /i "%ARCH%"=="arm64" set "ARCH=arm64"
if /i not "%ARCH%"=="x64" if /i not "%ARCH%"=="arm64" (
	echo error: unsupported architecture %ARCH%, only x64 and arm64 are supported
	exit /b 1
)
set "TRIPLET=%ARCH%-windows-static"
if "%ARCH%"=="arm64" (
	set "VSTOOLS=Microsoft.VisualStudio.Component.VC.Tools.ARM64"
) else (
	set "VSTOOLS=Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
)
echo triplet:  %TRIPLET%

rem ---- the Visual Studio developer environment, unless we already are in one --
rem (the variable is set outside the block: its ")" would end the block)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not defined INVSENV (
	if not exist "!VSWHERE!" (
		echo error: vswhere.exe not found - is Visual Studio installed?
		exit /b 1
	)
	for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires !VSTOOLS! -property installationPath`) do set "VSDIR=%%I"
	if "!VSDIR!"=="" (
		echo error: no Visual Studio with the C++ build tools for !ARCH! found
		exit /b 1
	)
	echo VS:       !VSDIR!
	call "!VSDIR!\VC\Auxiliary\Build\vcvarsall.bat" !ARCH! >nul
	if errorlevel 1 (
		echo error: vcvarsall.bat !ARCH! failed
		exit /b 1
	)
)
for %%T in (git cmake ninja) do (
	where %%T >nul 2>&1
	if errorlevel 1 (
		echo error: %%T not found - git comes from git-scm.com, cmake and ninja with the C++ workload of Visual Studio
		exit /b 1
	)
)
rem our checkout, not the one of Visual Studio
set "VCPKG_ROOT=%VCPKGDIR%"

rem ---- the sources ---------------------------------------------------------------
rem cmd.exe does not keep a batch file open: for every further line it opens it anew
rem and continues at the byte position where it stopped. Once git pull has changed
rem this script, that position lies elsewhere in the new version, and cmd.exe would
rem run whatever stands there. A block is read completely before it runs, so the
rem pull, the check for a change of this script and the start of the new version
rem all happen in this one block, and exit /b ends this instance before it reads on.
if not defined NOPULL (
	echo checking for updates..
	set "OLDHEAD="
	for /f "usebackq delims=" %%H in (`git -C "%DEKAF2%" rev-parse HEAD`) do set "OLDHEAD=%%H"
	git -C "%DEKAF2%" pull
	if errorlevel 1 (
		echo error: cannot pull source code changes
		exit /b 1
	)
	git -C "%DEKAF2%" diff --quiet !OLDHEAD! HEAD -- bootstrap.cmd
	if errorlevel 1 (
		echo bootstrap.cmd was updated, starting the new version..
		call "%BOOTSTRAP%" -no-pull %*
		exit /b !errorlevel!
	)
)

rem ---- vcpkg at the builtin-baseline of the manifest ---------------------------------
rem the package hashes include vcpkg's own scripts, so only this state finds the
rem packages again in the binary cache - any other one builds them all anew
set "BASELINE="
for /f "usebackq delims=" %%B in (`powershell -NoProfile -Command "(ConvertFrom-Json (Get-Content -Raw '%DEKAF2%\vcpkg\vcpkg.json')).'builtin-baseline'"`) do set "BASELINE=%%B"
if not defined BASELINE (
	echo error: no builtin-baseline in %DEKAF2%\vcpkg\vcpkg.json
	exit /b 1
)

if not exist "%VCPKGDIR%\.git" (
	echo cloning vcpkg into %VCPKGDIR% ...
	git clone https://github.com/microsoft/vcpkg.git "%VCPKGDIR%"
	if errorlevel 1 (
		echo error: cannot clone vcpkg
		exit /b 1
	)
)

set "HEAD="
for /f "usebackq delims=" %%H in (`git -C "%VCPKGDIR%" rev-parse HEAD`) do set "HEAD=%%H"
if /i not "%HEAD%"=="%BASELINE%" (
	echo checking out vcpkg at %BASELINE% ...
	git -C "%VCPKGDIR%" checkout -q %BASELINE% 2>nul
	if errorlevel 1 (
		git -C "%VCPKGDIR%" fetch origin
		git -C "%VCPKGDIR%" checkout -q %BASELINE%
		if errorlevel 1 (
			echo error: cannot check out vcpkg at %BASELINE%
			exit /b 1
		)
	)
	rem the tool belongs to the checkout
	if exist "%VCPKGDIR%\vcpkg.exe" del "%VCPKGDIR%\vcpkg.exe"
)

if not exist "%VCPKGDIR%\vcpkg.exe" (
	echo bootstrapping vcpkg ...
	call "%VCPKGDIR%\bootstrap-vcpkg.bat" -disableMetrics
	if errorlevel 1 (
		echo error: cannot bootstrap vcpkg
		exit /b 1
	)
)

rem ---- the builds ------------------------------------------------------------------
for %%C in (%CONFIGS%) do (
	call :build %%C
	if errorlevel 1 exit /b 1
)

echo.
echo dekaf2 successfully installed!
exit /b 0

rem ---- configure, build and install one configuration ------------------------------
rem the packages go into the build directory (manifest mode), and are restored
rem from the binary cache as long as vcpkg and the triplet are unchanged
:build
set "CFG=%~1"
rem the configuration of the build is this preset of CMakePresets.json, and its
rem binary directory is build\<preset>
set "PRESET=%TRIPLET%-%CFG%"
set "BUILD=%BUILDDIR%\%PRESET%"
if defined SCRATCH if exist "%BUILD%" (
	rmdir /s /q "%BUILD%"
	echo removed build directory %BUILD%
)
rem the presets are found in the source directory (setlocal restores the directory at the end)
cd /d "%DEKAF2%"
echo.
echo configuring %BUILD% with preset %PRESET% ...
cmake --preset %PRESET% %EXTRA%
if errorlevel 1 (
	echo error: cannot configure %BUILD%
	exit /b 1
)
echo.
echo now building %BUILD% with %JOBS% processes
cmake --build --preset %PRESET% --parallel %JOBS%
if errorlevel 1 (
	echo error: cannot build %BUILD%
	exit /b 1
)
cmake --install "%BUILD%" >nul
if errorlevel 1 (
	echo error: cannot install %BUILD%
	exit /b 1
)
echo installed into %BUILD%\install

rem the unit tests are no part of the default build target
if not defined UTESTS exit /b 0
echo.
echo building the unit tests in %BUILD% ...
cmake --build "%BUILD%" --parallel %JOBS% --target dekaf2-utests
if errorlevel 1 (
	echo error: cannot build the unit tests in %BUILD%
	exit /b 1
)
echo running the unit tests in %BUILD% ...
"%BUILD%\utests\dekaf2-utests.exe"
if errorlevel 1 (
	echo error: the unit tests failed in %BUILD%
	exit /b 1
)
exit /b 0

rem ---- the usage, from the lines on top ----------------------------------------------
:usage
for /f "usebackq delims=" %%L in (`findstr /b /c:"::#" "%~f0"`) do (
	set "LINE=%%L"
	echo(!LINE:~3!
)
exit /b 1
