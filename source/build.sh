#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
CXX="${CXX:-x86_64-w64-mingw32-g++}"
"$WINDRES" -I src src/Listki.rc -O coff -o build/resources.o
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wno-misleading-indentation -Wno-cast-function-type -municode -mwindows -static -static-libgcc -static-libstdc++ src/app.cpp build/resources.o -o build/Listki.exe -lcomctl32 -lcomdlg32 -lgdiplus -lgdi32 -lshell32 -lole32 -luuid -ladvapi32 -luser32 -Wl,--dynamicbase,--nxcompat,--no-insert-timestamp
x86_64-w64-mingw32-strip build/Listki.exe
