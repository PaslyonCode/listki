@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
where g++ >nul 2>nul
if errorlevel 1 (
  echo Install MinGW-w64 and add its bin folder to PATH.
  exit /b 1
)
windres -I src src\Listki.rc -O coff -o build\resources.o
if errorlevel 1 exit /b 1
g++ -std=c++17 -O2 -municode -mwindows -static -static-libgcc -static-libstdc++ src\app.cpp build\resources.o -o build\Listki.exe -lcomctl32 -lcomdlg32 -lgdiplus -lgdi32 -lshell32 -lole32 -luuid -ladvapi32 -luser32 -Wl,--dynamicbase,--nxcompat,--no-insert-timestamp
if errorlevel 1 exit /b 1
strip build\Listki.exe
echo Built: build\Listki.exe
