@echo off
rem Builds the headless batch runner (Projects/BatchSimulator) on its own, outside the upstream CMake tree.
rem Output: build\BatchSimulator\PAXS_BatchSimulator.exe

setlocal
cd /d "%~dp0\.."

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if not defined VSDIR (
    echo Visual Studio with the C++ toolset was not found.
    exit /b 1
)
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

cmake -S Projects/BatchSimulator -B build/BatchSimulator -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build/BatchSimulator || exit /b 1
echo Built: build\BatchSimulator\PAXS_BatchSimulator.exe
endlocal
