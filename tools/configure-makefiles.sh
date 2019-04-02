#!/bin/sh

CURRENT_DIR=`pwd`
BUILD_DIR="build"
BUILD_OPTIONS="-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBLEND2D_BUILD_TEST=1"

if [ -n "${ASMJIT_DIR}" ]; then
  BUILD_OPTIONS="${BUILD_OPTIONS} -DASMJIT_DIR=\"${ASMJIT_DIR}\""
fi

echo "** Configuring '${BUILD_DIR}_dbg' [Debug Build] **"
mkdir -p ../${BUILD_DIR}_dbg
cd ../${BUILD_DIR}_dbg
eval cmake .. -DCMAKE_BUILD_TYPE=Debug ${BUILD_OPTIONS} -DBLEND2D_BUILD_SANITIZE=1
cd ${CURRENT_DIR}

echo "** Configuring '${BUILD_DIR}_rel' [Release Build] **"
mkdir -p ../${BUILD_DIR}_rel
cd ../${BUILD_DIR}_rel
eval cmake .. -DCMAKE_BUILD_TYPE=Release ${BUILD_OPTIONS}
cd ${CURRENT_DIR}
