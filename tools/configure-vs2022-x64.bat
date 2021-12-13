@echo off
cmake .. -B "..\build_vs2022_x64" -G"Visual Studio 17" -A x64 -DBLEND2D_TEST=1
