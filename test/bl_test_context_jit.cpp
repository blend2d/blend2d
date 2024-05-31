// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bl_test_cmdline.h"
#include "bl_test_context_baseapp.h"
#include "bl_test_context_utilities.h"
#include "bl_test_imageutils.h"

namespace ContextTests {

#if defined(_M_X64) || defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386) || defined(__i386__)
  #define BL_JIT_ARCH_X86
#elif defined(_M_ARM64) || defined(__ARM64__) || defined(__aarch64__)
  #define BL_JIT_ARCH_A64
#endif

class JITTestApp : public BaseTestApp {
public:
  BLString _cpuFeaturesString;

  bool iterateAllJitFeatures = false;
  uint32_t selectedCpuFeatures {};
  uint32_t maximumPixelDifference {};

  uint32_t failedCount {};
  uint32_t passedCount {};

  JITTestApp()
    : BaseTestApp() {}

  int help() {
    using StringUtils::boolToString;

    printf("Usage:\n");
    printf("  bl_test_context_jit [options] [--help for help]\n");
    printf("\n");

    printf("Purpose:\n");
    printf("  JIT rendering context tester is designed to verify whether JIT-compiled\n");
    printf("  pipelines and reference pipelines yield pixel identical output when used\n");
    printf("  with the same input data. In addition, JIT rendering context tester verifies\n");
    printf("  whether all JIT compiled pipelines used by tests are actually compiled\n");
    printf("  successfully.\n");
    printf("\n");
    printf("  Blend2D's JIT compiler provides optimizations for various SIMD levels of the\n");
    printf("  supported architectures. For example X86 SIMD level could vary from SSE2 to\n");
    printf("  AVX-512+VBMI. The purpose of the tester is not just testing a single SIMD\n");
    printf("  level, but to offer to possibly testing ALL of them via command line options.\n");
    printf("\n");
    printf("Remarks:\n");
    printf("  Blend2D tries to use FMA when available, which means that rendering\n");
    printf("  styles that rely on floating point can end up different thanks to\n");
    printf("  rounding (FMA operation rounds only once, mul+add twice). Tests are\n");
    printf("  written in a way to make the difference minimal, but it's still there.\n");
    printf("\n");
    printf("  To counter it, use --max-diff=2 when testing the rendering of radial\n");
    printf("  and conical gradients. Other styles don't use floating point calculations\n");
    printf("  during fetching so they must yield identical results.\n");
    printf("\n");

    printCommonOptions(defaultOptions);

    printf("JIT options:\n");
    printf("  --max-diff=<value>      - Maximum pixel difference allowed  [default=0]\n");
    printf("  --simd-level=<name>     - SIMD level                        [default=native]\n");
    printf("\n");

#if defined(BL_JIT_ARCH_X86)
    printf("JIT SIMD levels (X86 and X86_64):\n");
    printf("  sse2                    - Enables SSE2      (x86 baseline)  [128-bit SIMD]\n");
    printf("  sse3                    - Enables SSE3      (if available)  [128-bit SIMD]\n");
    printf("  ssse3                   - Enables SSSE3     (if available)  [128-bit SIMD]\n");
    printf("  sse4.1                  - Enables SSE4.1    (if available)  [128-bit SIMD]\n");
    printf("  sse4.2                  - Enables SSE4.2    (if available)  [128-bit SIMD]\n");
    printf("  avx                     - Enables AVX       (if available)  [128-bit SIMD]\n");
    printf("  avx2                    - Enables AVX2      (if available)  [256-bit SIMD]\n");
    printf("  avx512                  - Enables AVX512    (F|CD|BW|DQ|VL) [512-bit SIMD]\n");
#elif defined(BL_JIT_ARCH_A64)
    printf("JIT SIMD levels (AArch64):\n");
    printf("  asimd                   - Enables ASIMD     (a64 baseline)  [128-bit SIMD]\n");
#else
    printf("JIT SIMD levels (unknown architecture!):\n");
#endif
    printf("  native                  - Use a native SIMD level as detected by Blend2D\n");
    printf("  all                     - Executes all available SIMD levels\n");
    printf("\n");

    printCommands();
    printFormats();
    printCompOps();
    printOpacityOps();
    printStyleIds();
    printStyleOps();

    fflush(stdout);
    return 0;
  }

  void resetCounters() {
    mismatchCount = 0;
  }

  bool parseJITOptions(CmdLine cmdLine) {
    maximumPixelDifference = cmdLine.valueAsUInt("--max-diff", 0);
    const char* simdLevel = cmdLine.valueOf("--simd-level", "native");

    if (simdLevel) {
      if (StringUtils::strieq(simdLevel, "native")) {
        // Nothing to do if configured to auto-detect.
      }
      else if (StringUtils::strieq(simdLevel, "all")) {
        iterateAllJitFeatures = true;
      }
#if defined(BL_JIT_ARCH_X86)
      else if (StringUtils::strieq(simdLevel, "sse2")) {
        selectedCpuFeatures = BL_RUNTIME_CPU_FEATURE_X86_SSE2;
      }
      else if (StringUtils::strieq(simdLevel, "sse3")) {
        selectedCpuFeatures = BL_RUNTIME_CPU_FEATURE_X86_SSE3;
      }
      else if (StringUtils::strieq(simdLevel, "ssse3")) {
        selectedCpuFeatures = BL_RUNTIME_CPU_FEATURE_X86_SSSE3;
      }
      else if (StringUtils::strieq(simdLevel, "sse4.1")) {
        selectedCpuFeatures = BL_RUNTIME_CPU_FEATURE_X86_SSE4_1;
      }
      else if (StringUtils::strieq(simdLevel, "sse4.2")) {
        selectedCpuFeatures = BL_RUNTIME_CPU_FEATURE_X86_SSE4_2;
      }
      else if (StringUtils::strieq(simdLevel, "avx")) {
        selectedCpuFeatures = BL_RUNTIME_CPU_FEATURE_X86_AVX;
      }
      else if (StringUtils::strieq(simdLevel, "avx2")) {
        selectedCpuFeatures = BL_RUNTIME_CPU_FEATURE_X86_AVX2;
      }
      else if (StringUtils::strieq(simdLevel, "avx512")) {
        selectedCpuFeatures = BL_RUNTIME_CPU_FEATURE_X86_AVX512;
      }
#elif defined(BL_JIT_ARCH_A64)
      else if (strcmp(simdLevel, "asimd") == 0) {
        // Currently the default...
      }
#endif
      else {
        printf("Failed to process command line arguments:\n");
        printf("  Unknown simd-level '%s' - please use --help to list all available simd levels\n", simdLevel);
        return false;
      }
    }

    return true;
  }

  void stringifyFeatureId(BLString& out, uint32_t cpuFeatures) noexcept {
    if (cpuFeatures)
      out.assign(StringUtils::cpuX86FeatureToString(BLRuntimeCpuFeatures(cpuFeatures)));
    else
      out.assign("native");
  }

  bool runWithFeatures(uint32_t cpuFeatures) {
    resetCounters();
    stringifyFeatureId(_cpuFeaturesString, cpuFeatures);

    BLString aTesterName;
    BLString bTesterName;

    ContextTester aTester("ref");
    ContextTester bTester("jit");

    aTester.setFontData(fontData);
    bTester.setFontData(fontData);

    aTester.setFlushSync(options.flushSync);
    bTester.setFlushSync(options.flushSync);

    BLContextCreateInfo aCreateInfo {};
    BLContextCreateInfo bCreateInfo {};

    aCreateInfo.flags = BL_CONTEXT_CREATE_FLAG_DISABLE_JIT;

    if (cpuFeatures) {
      bCreateInfo.flags = BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT_RUNTIME |
                          BL_CONTEXT_CREATE_FLAG_OVERRIDE_CPU_FEATURES;
      bCreateInfo.cpuFeatures = cpuFeatures;
    }

    if (aTester.init(int(options.width), int(options.height), options.format, aCreateInfo) != BL_SUCCESS ||
        bTester.init(int(options.width), int(options.height), options.format, bCreateInfo) != BL_SUCCESS) {
      printf("Failed to initialize rendering contexts\n");
      return 1;
    }

    TestInfo info;
    dispatchRuns([&](CommandId commandId, CompOp compOp, OpacityOp opacityOp) {
      info.name.assignFormat(
          "%s | comp-op=%s | opacity=%s | style=%s | simd-level=%s",
          StringUtils::commandIdToString(commandId),
          StringUtils::compOpToString(compOp),
          StringUtils::opacityOpToString(opacityOp),
          StringUtils::styleIdToString(options.styleId),
          _cpuFeaturesString.data());

      info.id.assignFormat("%s-%s-%s-%s-%s",
        StringUtils::commandIdToString(commandId),
        StringUtils::compOpToString(compOp),
        StringUtils::opacityOpToString(opacityOp),
        StringUtils::styleIdToString(options.styleId),
        _cpuFeaturesString.data());

      if (!options.quiet) {
        printf("Testing [%s]:\n", info.name.data());
      }

      aTester.setOptions(compOp, opacityOp, options.styleId, options.styleOp);
      bTester.setOptions(compOp, opacityOp, options.styleId, options.styleOp);

      if (runMultiple(commandId, info, aTester, bTester, maximumPixelDifference))
        passedCount++;
      else
        failedCount++;
    });

    aTester.reset();
    bTester.reset();

    if (mismatchCount)
      printf("Found %llu mismatches!\n\n", (unsigned long long)mismatchCount);
    else if (!options.quiet)
      printf("\n");

    return !mismatchCount;
  }

  int run(CmdLine cmdLine) {
    printAppInfo("Blend2D JIT Rendering Context Tester", cmdLine.hasArg("--quiet"));

    if (cmdLine.hasArg("--help"))
      return help();

    if (!parseCommonOptions(cmdLine) || !parseJITOptions(cmdLine))
      return 1;

    if (iterateAllJitFeatures) {
#if defined(BL_JIT_ARCH_X86)
      static constexpr uint32_t x86FeaturesList[] = {
        BL_RUNTIME_CPU_FEATURE_X86_SSE2,
        BL_RUNTIME_CPU_FEATURE_X86_SSE3,
        BL_RUNTIME_CPU_FEATURE_X86_SSSE3,
        BL_RUNTIME_CPU_FEATURE_X86_SSE4_1,
        BL_RUNTIME_CPU_FEATURE_X86_SSE4_2,
        BL_RUNTIME_CPU_FEATURE_X86_AVX,
        BL_RUNTIME_CPU_FEATURE_X86_AVX2,
        BL_RUNTIME_CPU_FEATURE_X86_AVX512
      };

      BLRuntimeSystemInfo systemInfo {};
      BLRuntime::querySystemInfo(&systemInfo);

      for (const uint32_t& feature : x86FeaturesList) {
        if (!(systemInfo.cpuFeatures & feature))
          break;

        if (options.quiet) {
          stringifyFeatureId(_cpuFeaturesString, feature);
          printf("Testing [%s] (quiet mode)\n", _cpuFeaturesString.data());
        }

        runWithFeatures(feature);
      }
#endif

      // Now run with all features if everything above has passed.
      runWithFeatures(0);
    }
    else {
      runWithFeatures(selectedCpuFeatures);
    }

    if (failedCount) {
      printf("[FAILED] %u tests out of %u failed\n", failedCount, passedCount + failedCount);
      return 1;
    }
    else {
      printf("[PASSED] %u tests passed\n", passedCount);
      return 0;
    }
  }
};

} // {ContextTests}

int main(int argc, char* argv[]) {
  BLRuntimeScope rtScope;
  ContextTests::JITTestApp app;

  return app.run(CmdLine(argc, argv));
}
