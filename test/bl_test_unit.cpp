// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include <blend2d/blapi-build_p.h>
#include <blend2d/blruntime.h>

// ============================================================================
// [Main]
// ============================================================================

int main(int argc, const char* argv[]) {
  BLRuntimeBuildInfo buildInfo;
  BLRuntime::queryBuildInfo(&buildInfo);

  INFO("Blend2D Unit-Test (v%u.%u.%u - %s)\n\n",
    buildInfo.majorVersion,
    buildInfo.minorVersion,
    buildInfo.patchVersion,
    buildInfo.buildType == BL_RUNTIME_BUILD_TYPE_RELEASE ? "release" : "debug");

  return BrokenAPI::run(argc, argv);
}
