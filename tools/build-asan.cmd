@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 10.0.26100.0 -vcvars_ver=14.51
if errorlevel 1 exit /b 1
set "PATH=D:\qt\6.12.0\msvc2022_64\bin;%PATH%"
powershell -NoProfile -ExecutionPolicy Bypass -File tools\detect-msvc-prefix.ps1 -BuildDirectory build-asan
if errorlevel 1 exit /b 1
"D:\qt\Tools\CMake_64\bin\cmake.exe" -S . -B build-asan -G Ninja -DCMAKE_MAKE_PROGRAM=D:/qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=D:/qt/6.12.0/msvc2022_64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DPASS_ASAN_SAFETY=ON
if errorlevel 1 exit /b 1
"D:\qt\Tools\CMake_64\bin\cmake.exe" --build build-asan --target pass_estimator_safety
if errorlevel 1 exit /b 1
build-asan\pass_estimator_safety.exe
exit /b %errorlevel%
