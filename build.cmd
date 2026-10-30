@echo off
setlocal
cd /d "%~dp0"
set "VSLANG=1033"
chcp 65001 >nul
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.26100.0 -vcvars_ver=14.44
if errorlevel 1 exit /b 1
set "PATH=D:\qt\6.12.0\msvc2022_64\bin;%PATH%"
powershell -NoProfile -ExecutionPolicy Bypass -File tools\detect-msvc-prefix.ps1 -BuildDirectory build
if errorlevel 1 exit /b 1
"D:\qt\Tools\CMake_64\bin\cmake.exe" -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM=D:/qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=D:/qt/6.12.0/msvc2022_64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DOPENSSL_ROOT_DIR="D:/Program Files/OpenSSL-Win64" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
if errorlevel 1 exit /b 1
"D:\qt\Tools\CMake_64\bin\cmake.exe" --build build
if errorlevel 1 exit /b 1
"D:\qt\Tools\CMake_64\bin\ctest.exe" --test-dir build --output-on-failure
if errorlevel 1 exit /b 1
echo Build and tests passed.
