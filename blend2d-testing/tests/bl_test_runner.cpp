// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api-build_test_p.h>
#include <blend2d/core/runtime.h>
#include <blend2d/core/runtimescope.h>

int main(int argc, const char* argv[]) {
  BLRuntimeBuildInfo build_info;
  BLRuntime::query_build_info(&build_info);

  INFO(
    "Blend2D Unit Tests [use --help for command line options]\n"
    "  Version    : %u.%u.%u\n"
    "  Build Type : %s\n"
    "  Compiled By: %s\n\n",
    build_info.major_version,
    build_info.minor_version,
    build_info.patch_version,
    build_info.build_type == BL_RUNTIME_BUILD_TYPE_DEBUG ? "Debug" : "Release",
    build_info.compiler_info);

  BLRuntimeScope rt_scope;
  return BrokenAPI::run(argc, argv);
}
