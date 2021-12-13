// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// bl_test_verify_mt
// -----------------
//
// This is a simple test that renders shapes with ST and MT and compares whether
// the results are identical. If not, a diff is created and stored on disk.

#include <blend2d.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bl_test_utilities.h"

class App {
public:
  typedef void FuzzFunc(ContextFuzzer* fuzzer, size_t n);

  bool verbose;
  bool storeImages;
  uint32_t threadCount;
  uint32_t seed;
  uint32_t width;
  uint32_t height;
  uint32_t count;
  uint64_t mismatchCount;

  App() :
    verbose(false),
    storeImages(false),
    threadCount(2),
    seed(1),
    width(513),
    height(513),
    count(1000000),
    mismatchCount(0) {}

  void info() {
    BLRuntimeBuildInfo buildInfo;
    BLRuntime::queryBuildInfo(&buildInfo);

    printf(
      "Blend2D Verify MT [use --help for command line options]\n"
      "  Version    : %u.%u.%u\n"
      "  Build Type : %s\n"
      "  Compiled By: %s\n\n",
      buildInfo.majorVersion,
      buildInfo.minorVersion,
      buildInfo.patchVersion,
      buildInfo.buildType == BL_RUNTIME_BUILD_TYPE_DEBUG ? "Debug" : "Release",
      buildInfo.compilerInfo);
  }

  int help() {
    printf("Usage:\n");
    printf("  bl_test_verify_mt [Options]\n");
    printf("\n");

    printf("Fuzzer Options:\n");
    printf("  --width           - Image width                     [default=513]\n");
    printf("  --height          - Image height                    [default=513]\n");
    printf("  --count           - Count of render commands        [default=1000000]\n");
    printf("  --thread-count    - Number of threads of MT context [default=2]\n");
    printf("  --command         - Specify which command to run    [default=all]\n");
    printf("  --seed            - Random number generator seed    [default=1]\n");
    printf("  --store           - Write resulting images to files [default=false]\n");
    printf("  --verbose         - Debug each render command       [default=false]\n");
    printf("\n");

    printf("Fuzzer Commands:\n");
    printf("  FillRectI      - Fill aligned rectangles\n");
    printf("  FillRectD      - Fill unaligned rectangles\n");
    printf("  FillTriangle   - Fill triangles\n");
    printf("  FillPathQuads  - Fill path having quadratic curves\n");
    printf("  FillPathCubics - Fill path having cubic curves\n");

    return 0;
  }

  void fuzz(const char* fuzzName, ContextFuzzer& aFuzzer, ContextFuzzer& bFuzzer, FuzzFunc fuzzFunc) {
    aFuzzer.clear();
    bFuzzer.clear();

    aFuzzer.seed(seed);
    bFuzzer.seed(seed);

    fuzzFunc(&aFuzzer, count);
    fuzzFunc(&bFuzzer, count);

    if (check(fuzzName, aFuzzer.image(), bFuzzer.image()))
      return;

    findProblem(fuzzName, aFuzzer, bFuzzer, fuzzFunc);
  }

  bool check(const char* fuzzName, const BLImage& aImage, const BLImage& bImage) {
    ImageUtils::DiffInfo diffInfo = ImageUtils::diffInfo(aImage, bImage);
    if (diffInfo.maxDiff == 0)
      return true;

    mismatchCount++;
    BLString fileName;
    fileName.assignFormat("%s-Bug-%05llu.bmp", fuzzName, (unsigned long long)mismatchCount);
    printf("Mismatch: %s\n", fileName.data());
    if (storeImages) {
      BLImage d = ImageUtils::diffImage(aImage, bImage);
      d.writeToFile(fileName.data());
    }
    return false;
  }

  void findProblem(const char* fuzzName, ContextFuzzer& aFuzzer, ContextFuzzer& bFuzzer, FuzzFunc fuzzFunc) {
    // Do a binary search to find exactly the failing command.
    size_t base = 0;
    size_t size = count;

    Logger& logger = aFuzzer._logger;
    logger.print("Bisecting to match the problematic command...\n");

    Logger::Verbosity aLoggerVerbosity = aFuzzer._logger.setVerbosity(Logger::Verbosity::Silent);
    Logger::Verbosity bLoggerVerbosity = bFuzzer._logger.setVerbosity(Logger::Verbosity::Silent);

    while (size_t half = size / 2u) {
      size_t middle = base + half;
      size -= half;

      logger.print("  Verifying range [%zu %zu)\n", base, size);

      aFuzzer.clear();
      bFuzzer.clear();

      aFuzzer.seed(seed);
      bFuzzer.seed(seed);

      fuzzFunc(&aFuzzer, base + size);
      fuzzFunc(&bFuzzer, base + size);

      check(fuzzName, aFuzzer.image(), bFuzzer.image());

      if (ImageUtils::diffInfo(aFuzzer.image(), bFuzzer.image()).maxDiff == 0)
        base = middle;
    }

    logger.print("  Mismatch command index: %zu\n", base);

    aFuzzer.clear();
    bFuzzer.clear();

    aFuzzer.seed(seed);
    bFuzzer.seed(seed);

    if (base) {
      fuzzFunc(&aFuzzer, base - 1);
      fuzzFunc(&bFuzzer, base - 1);
    }

    aFuzzer._logger.setVerbosity(Logger::Verbosity::Debug);
    bFuzzer._logger.setVerbosity(Logger::Verbosity::Debug);

    fuzzFunc(&aFuzzer, 1);
    fuzzFunc(&bFuzzer, 1);

    aFuzzer._logger.setVerbosity(aLoggerVerbosity);
    bFuzzer._logger.setVerbosity(aLoggerVerbosity);

    check(fuzzName, aFuzzer.image(), bFuzzer.image());
  }

  int run(int argc, char* argv[]) {
    CmdLine cmdLine(argc, argv);

    info();
    if (cmdLine.hasArg("--help"))
      return help();

    verbose = cmdLine.hasArg("--verbose");
    storeImages = cmdLine.hasArg("--store");
    threadCount = cmdLine.valueAsUInt("--thread-count", 2);
    seed = cmdLine.valueAsUInt("--seed", seed);
    width = cmdLine.valueAsUInt("--width", width);
    height = cmdLine.valueAsUInt("--height", height);
    count = cmdLine.valueAsUInt("--count", count);

    const char* command = cmdLine.valueOf("--command", "");
    bool all = command[0] == '\0' || StringUtils::strieq(command, "all");

    ContextFuzzer aFuzzer("[ST] ", verbose ? Logger::Verbosity::Debug : Logger::Verbosity::Info);
    ContextFuzzer bFuzzer("[MT] ", Logger::Verbosity::Info);

    if (aFuzzer.init(int(width), int(height), BL_FORMAT_PRGB32, 0          ) != BL_SUCCESS ||
        bFuzzer.init(int(width), int(height), BL_FORMAT_PRGB32, threadCount) != BL_SUCCESS) {
      printf("Failed to initialize rendering contexts\n");
      return 1;
    }

    #define FUZZ(fuzzName) \
      if (all || StringUtils::strieq(command, #fuzzName)) { \
        fuzz(#fuzzName, aFuzzer, bFuzzer, [](ContextFuzzer* fuzzer, size_t n) { \
          fuzzer->fuzz##fuzzName(n); } \
        ); \
      }

    FUZZ(FillRectI)
    FUZZ(FillRectD)
    FUZZ(FillTriangle)
    FUZZ(FillPathQuads)
    FUZZ(FillPathCubics)

    #undef FUZZ

    aFuzzer.reset();
    bFuzzer.reset();

    printf("Fuzzing finished...\n");

    if (mismatchCount)
      printf("Found %llu mismatches!\n", (unsigned long long)mismatchCount);
    else
      printf("No mismatches found!\n");

    return mismatchCount ? 1 : 0;
  }
};

int main(int argc, char* argv[]) {
  return App().run(argc, argv);
}
