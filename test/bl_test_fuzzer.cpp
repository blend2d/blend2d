// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// bl_test_fuzzer
// --------------
//
// This is a simple rendering context fuzzer that covers only basic API calls
// at the moment. It will be improved in the future to cover also paths and
// other features like stroking.

#include <blend2d.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bl_test_utilities.h"

static int help() {
  printf("Usage:\n");
  printf("  bl_test_fuzzer [Options]\n");
  printf("\n");

  printf("Fuzzer Options:\n");
  printf("  --width           - Image width                     [default=513]\n");
  printf("  --height          - Image height                    [default=513]\n");
  printf("  --count           - Count of render commands        [default=1000000]\n");
  printf("  --thread-count    - Number of threads of MT context [default=0]\n");
  printf("  --command         - Specify which command to run    [default=all]\n");
  printf("  --seed            - Random number generator seed    [default=1]\n");
  printf("  --store           - Write resulting images to files [default=false]\n");
  printf("  --verbose         - Debug each render command       [default=false]\n");
  printf("\n");

  printf("Fuzzer Commands:\n");
  printf("  FillRectI      - Fill aligned rectangles\n");
  printf("  FillRectD      - Fill unaligned rectangles\n");
  printf("  FillTriangle   - Fill triangles\n");
  printf("  FillQuads      - Fill path having quadratic curves\n");
  printf("  FillCubics     - Fill path having cubic curves\n");

  return 0;
}

int main(int argc, char* argv[]) {
  BLRuntimeBuildInfo buildInfo;
  BLRuntime::queryBuildInfo(&buildInfo);

  CmdLine cmdLine(argc, argv);

  // Basic information.
  printf(
    "Blend2D Fuzzer [use --help for command line options]\n"
    "  Version    : %u.%u.%u\n"
    "  Build Type : %s\n"
    "  Compiled By: %s\n\n",
    buildInfo.majorVersion,
    buildInfo.minorVersion,
    buildInfo.patchVersion,
    buildInfo.buildType == BL_RUNTIME_BUILD_TYPE_DEBUG ? "Debug" : "Release",
    buildInfo.compilerInfo);

  if (cmdLine.hasArg("--help"))
    return help();

  // Command line parameters.
  bool verbose = cmdLine.hasArg("--verbose");
  bool storeImages = cmdLine.hasArg("--store");
  uint32_t threadCount = cmdLine.valueAsUInt("--thread-count", 0);
  uint32_t seed = cmdLine.valueAsUInt("--seed", 1);
  uint32_t width = cmdLine.valueAsUInt("--width", 513);
  uint32_t height = cmdLine.valueAsUInt("--height", 513);
  uint32_t count = cmdLine.valueAsUInt("--count", 1000000);

  const char* command = cmdLine.valueOf("--command", "");
  bool all = command[0] == '\0' || StringUtils::strieq(command, "all");

  // Fuzzing...
  ContextFuzzer fuzzer("", verbose ? Logger::Verbosity::Debug : Logger::Verbosity::Info);
  fuzzer.seed(seed);
  fuzzer.setStoreImages(storeImages);

  if (fuzzer.init(int(width), int(height), BL_FORMAT_PRGB32, threadCount) != BL_SUCCESS) {
    printf("Failed to initialize the rendering context\n");
    return 1;
  }

  #define FUZZ(fuzzName) \
    if (all || StringUtils::strieq(command, #fuzzName)) { \
      fuzzer.fuzz##fuzzName(count); \
    }

  FUZZ(FillRectI)
  FUZZ(FillRectD)
  FUZZ(FillTriangle)
  FUZZ(FillPathQuads)
  FUZZ(FillPathCubics)

  #undef FUZZ

  fuzzer.reset();

  printf("Fuzzing finished...\n");
  return 0;
}
