@echo off
rem Local Windows build: MSVC (VS 2022) + VS-bundled CMake/Ninja + bundled SFML 3.0.0.
rem Usage: Scripts\WindowsMSVCBuild.bat [Release|Debug]
rem Output: build\x64-<Config>\MapViewer\SFMLMapViewer.exe (Config.tsv there points at the repo root)

setlocal
cd /d "%~dp0\.."

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if not defined VSDIR (
    echo Visual Studio with the C++ toolset was not found.
    exit /b 1
)

rem vcvars64.bat calls vswhere.exe by bare name, so put the installer directory on PATH first
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

set BUILD_DIR=build\x64-%CONFIG%

cmake -S Projects -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON || exit /b 1
cmake --build "%BUILD_DIR%" %2 %3 %4 || exit /b 1

echo.
echo Built: %BUILD_DIR%\MapViewer\SFMLMapViewer.exe
endlocal
