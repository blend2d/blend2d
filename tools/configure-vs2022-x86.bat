@echo off
cmake .. -B "..\build_vs2022_x86" -G"Visual Studio 17" -A Win32 -DBLEND2D_TEST=1
