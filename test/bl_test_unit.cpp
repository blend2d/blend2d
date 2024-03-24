// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/api-build_test_p.h>
#include <blend2d/runtime.h>

// There can be numerical issues with testing floating point code in cases in which X87
// is configured with 80 bits of precision. This is only relevant on 32-bit X86 as 64-bit
// code would always use SSE or AVX instructions.
class TestFPUControlScope {
public:
  static constexpr uint16_t kPC_Mask = 0x0300u;
  static constexpr uint16_t kPC_Float = 0x0000u;
  static constexpr uint16_t kPC_Double = 0x0200u;

  uint16_t _cw {};

  BL_NOINLINE TestFPUControlScope() {
#if defined(__GNUC__) && BL_TARGET_ARCH_X86 == 32
    __asm__ __volatile__("fstcw %w0" : "=m" (_cw));
    uint16_t updatedCW = uint16_t((_cw & ~kPC_Mask) | kPC_Double);
    __asm__ __volatile__("fldcw %w0" : : "m" (updatedCW));
#endif // BL_TARGET_ARCH_X86
  }

  BL_NOINLINE ~TestFPUControlScope() {
#if defined(__GNUC__) && BL_TARGET_ARCH_X86 == 32
    __asm__ __volatile__("fldcw %w0" : : "m" (_cw));
#endif // BL_TARGET_ARCH_X86
  }
};

int main(int argc, const char* argv[]) {
  BLRuntimeBuildInfo buildInfo;
  BLRuntime::queryBuildInfo(&buildInfo);

  INFO(
    "Blend2D Unit Tests [use --help for command line options]\n"
    "  Version    : %u.%u.%u\n"
    "  Build Type : %s\n"
    "  Compiled By: %s\n\n",
    buildInfo.majorVersion,
    buildInfo.minorVersion,
    buildInfo.patchVersion,
    buildInfo.buildType == BL_RUNTIME_BUILD_TYPE_DEBUG ? "Debug" : "Release",
    buildInfo.compilerInfo);

  TestFPUControlScope scope;
  return BrokenAPI::run(argc, argv);
}
