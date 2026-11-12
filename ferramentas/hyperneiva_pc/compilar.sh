#!/bin/sh
# Compila o HyperNeivaPC.exe (Windows, sem DLL) com o MinGW do MSYS2.
cd "$(dirname "$0")" || exit 1
PATH=/c/msys64/mingw64/bin:$PATH g++ -O2 -std=c++17 -static -s -Wall -Wno-unused-function -Wno-missing-field-initializers \
  -o HyperNeivaPC.exe HyperNeivaPC.cpp -lws2_32 -lgdi32 -luser32 -lshell32 -lole32 -luuid -lcrypt32 -ladvapi32 -lwinhttp && echo "HyperNeivaPC.exe: $(stat -c %s HyperNeivaPC.exe) bytes"
