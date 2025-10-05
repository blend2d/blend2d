#!/bin/bash

BUILD_OPTIONS="-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBLEND2D_TEST=ON"

if [ -n "${ASMJIT_DIR}" ]; then
  BUILD_OPTIONS="${BUILD_OPTIONS} -DASMJIT_DIR=\"${ASMJIT_DIR}\""
fi

echo "== [Configuring Build - Release_ASAN] =="
eval cmake . -B build/Release_ASAN ${BUILD_OPTIONS} -DCMAKE_BUILD_TYPE=Release -DBLEND2D_SANITIZE=address "$@"
echo ""

echo "== [Configuring Build - Release_MSAN] =="
eval cmake . -B build/Release_MSAN ${BUILD_OPTIONS} -DCMAKE_BUILD_TYPE=Release -DBLEND2D_SANITIZE=memory -DBLEND2D_NO_JIT=ON "$@"
echo ""

echo "== [Configuring Build - Release_TSAN] =="
eval cmake . -B build/Release_TSAN ${BUILD_OPTIONS} -DCMAKE_BUILD_TYPE=Release -DBLEND2D_SANITIZE=thread "$@"
echo ""

echo "== [Configuring Build - Release_UBSAN] =="
eval cmake . -B build/Release_UBSAN ${BUILD_OPTIONS} -DCMAKE_BUILD_TYPE=Release -DBLEND2D_SANITIZE=undefined "$@"
echo ""
