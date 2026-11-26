@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM Set VCPKG_ROOT if it is not already available in the environment.
if not defined VCPKG_ROOT if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" set "VCPKG_ROOT=C:\vcpkg"
if not defined VCPKG_ROOT (
  echo [ERROR] VCPKG_ROOT is not set. Set it to your vcpkg folder, for example C:\vcpkg.
  pause
  exit /b 1
)
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
  echo [ERROR] vcpkg CMake toolchain was not found at "%VCPKG_ROOT%".
  pause
  exit /b 1
)
REM Initialize Visual Studio 2026 MSVC environment automatically for a normal CMD window.
if not defined VSCMD_VER (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
  )
  if defined VSINSTALL if exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
)
where cl >nul 2>nul || (echo [ERROR] MSVC compiler not found. Install Desktop development with C++ in VS Installer.& pause & exit /b 1)
where cmake >nul 2>nul || (echo [ERROR] CMake not found. Install CMake tools for Windows in VS Installer.& pause & exit /b 1)

set "BUILD_DIR=%CD%\out\build\vs2026-local"
set "PACKAGE_DIR=%CD%\dist\GameControlCenter"

 echo.
echo === Configure CMake + install vcpkg dependencies (first run may take time) ===
cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64 -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_MODE=ON
if errorlevel 1 goto :failed

echo.
echo === Build Release ===
cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 goto :failed

set "EXE=%BUILD_DIR%\Release\GameControlCenter.exe"
if not exist "%EXE%" (echo [ERROR] EXE was not produced: "%EXE%" & goto :failed)

 echo.
echo === Prepare portable folder ===
if exist "%PACKAGE_DIR%" rmdir /s /q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%"
copy /y "%EXE%" "%PACKAGE_DIR%\GameControlCenter.exe" >nul

set "VCPKG_BIN=%BUILD_DIR%\vcpkg_installed\x64-windows\bin"
if not exist "%VCPKG_BIN%" set "VCPKG_BIN=%BUILD_DIR%\vcpkg_installed\x64-windows\debug\bin"
if exist "%VCPKG_BIN%" copy /y "%VCPKG_BIN%\*.dll" "%PACKAGE_DIR%\" >nul

REM Locate windeployqt from the manifest-installed Qt package.
set "WINDEPLOYQT="
for /r "%BUILD_DIR%\vcpkg_installed\x64-windows\tools" %%F in (windeployqt.exe) do if not defined WINDEPLOYQT set "WINDEPLOYQT=%%F"
if defined WINDEPLOYQT (
  echo Deploying Qt plugins using "%WINDEPLOYQT%"
  "%WINDEPLOYQT%" --release --compiler-runtime "%PACKAGE_DIR%\GameControlCenter.exe"
  if errorlevel 1 echo [WARN] windeployqt reported an issue. Check the package folder before running.
) else (
  echo [WARN] windeployqt.exe was not found. The EXE may require Qt DLLs to run.
)
if exist README.md copy /y README.md "%PACKAGE_DIR%\README.txt" >nul

echo.
echo Build completed.
echo EXE: "%PACKAGE_DIR%\GameControlCenter.exe"
echo Open the dist\GameControlCenter folder and run the EXE.
pause
exit /b 0

:failed
echo.
echo Build failed. The error above identifies the missing component or source error.
pause
exit /b 1
