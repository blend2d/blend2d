// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#include <blend2d/core/runtime.h>
#include <blend2d/core/runtime_scope.h>

int main(int argc, const char* argv[]) {
  BLRuntimeBuildInfo build_info;
  BLRuntime::query_build_info(&build_info);

  printf("Blend2D Test Runner v%u.%u.%u %s/%s [--help for options]\n\n",
    build_info.major_version,
    build_info.minor_version,
    build_info.patch_version,
    build_info.build_type == BL_RUNTIME_BUILD_TYPE_DEBUG ? "debug" : "release",
    build_info.compiler_info);
  fflush(stdout);

  ZeroTest::TestPrecedence test_precedence[] = {
    // A placeholder for temporary tests that would always run first.
    { "xxx*" },

    // Regular test case order.
    { "axl*" },
    { "simd*" },
    { "tables*" },
    { "math*" },
    { "threading*" },
    { "geometry*" },
    { "core_random*" },
    { "core_rgba*" },
    { "core_transform*" },
    { "core_bit_array*" },
    { "core_coverage_set*" },
    { "core_string*" },
    { "core_array*" },
    { "core_path*" },
    { "core_image*" },
    { "core_image_codec*" },
    { "core_pattern*" },
    { "core_gradient*" },
    { "core_font_tag*" },
    { "core_font*" },
    { "core*" },
    { "core_context*" },
    { "compression_checksum_*" },
    { "compression_*" },
    { "codec*" },
    { "opentype*" },
    { "rendering*" }
  };

  ZeroTest::TestRunner test_runner(argc, argv);
  test_runner.apply_precedence(test_precedence);

  auto message = [&]([[maybe_unused]] ZeroTest::MessageType msg_type, const char* msg, size_t msg_size) {
    fwrite(msg, 1, msg_size, stdout);
    fflush(stdout);
  };

  test_runner.on_message(message);

  BLRuntimeScope rt_scope;
  return test_runner.run_tests();
}
