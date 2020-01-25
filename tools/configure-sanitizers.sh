#!/bin/sh

CURRENT_DIR=`pwd`
BUILD_DIR="build"
BUILD_OPTIONS="-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBLEND2D_TEST=1"

if [ -n "${ASMJIT_DIR}" ]; then
  BUILD_OPTIONS="${BUILD_OPTIONS} -DASMJIT_DIR=\"${ASMJIT_DIR}\""
fi

echo "** Configuring '${BUILD_DIR}_rel_asan' [Sanitize=Address] **"
mkdir -p ../${BUILD_DIR}_rel_asan
cd ../${BUILD_DIR}_rel_asan
eval cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release ${BUILD_OPTIONS} -DBLEND2D_SANITIZE=address
cd ${CURRENT_DIR}

echo "** Configuring '${BUILD_DIR}_rel_ubsan' [Sanitize=Undefined] **"
mkdir -p ../${BUILD_DIR}_rel_ubsan
cd ../${BUILD_DIR}_rel_ubsan
eval cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release ${BUILD_OPTIONS} -DBLEND2D_SANITIZE=undefined
cd ${CURRENT_DIR}
