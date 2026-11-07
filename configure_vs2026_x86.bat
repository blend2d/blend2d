@echo off
cmake . -B build_x86 -G"Visual Studio 18 2026" -A Win32 -DBLEND2D_TEST=ON
