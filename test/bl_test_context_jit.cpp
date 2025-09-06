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
  BLString _cpu_features_string;

  bool iterate_all_jit_features = false;
  uint32_t selected_cpu_features {};
  uint32_t maximum_pixel_difference = 0xFFFFFFFFu;

  uint32_t failed_count {};
  uint32_t passed_count {};

  JITTestApp()
    : BaseTestApp() {}

  int help() {
    using StringUtils::bool_to_string;

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

    print_common_options(default_options);

    printf("JIT options:\n");
    printf("  --max-diff=<value>      - Maximum pixel difference allowed  [default=auto]\n");
    printf("  --simd-level=<name>     - SIMD level                        [default=all]\n");
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

    print_commands();
    print_formats();
    print_comp_ops();
    print_opacity_ops();
    print_style_ids();
    print_style_ops();

    fflush(stdout);
    return 0;
  }

  void reset_counters() {
    mismatch_count = 0;
  }

  bool parse_jit_options(CmdLine cmd_line) {
    const char* max_diff_value = cmd_line.value_of("--max-diff", "auto");
    if (strcmp(max_diff_value, "auto") == 0) {
      maximum_pixel_difference = 0xFFFFFFFFu;
    }
    else {
      maximum_pixel_difference = cmd_line.value_as_uint("--max-diff", 0);
    }

    const char* simd_level = cmd_line.value_of("--simd-level", "all");
    if (simd_level) {
      if (StringUtils::strieq(simd_level, "native")) {
        // Nothing to do if configured to auto-detect.
      }
      else if (StringUtils::strieq(simd_level, "all")) {
        iterate_all_jit_features = true;
      }
#if defined(BL_JIT_ARCH_X86)
      else if (StringUtils::strieq(simd_level, "sse2")) {
        selected_cpu_features = BL_RUNTIME_CPU_FEATURE_X86_SSE2;
      }
      else if (StringUtils::strieq(simd_level, "sse3")) {
        selected_cpu_features = BL_RUNTIME_CPU_FEATURE_X86_SSE3;
      }
      else if (StringUtils::strieq(simd_level, "ssse3")) {
        selected_cpu_features = BL_RUNTIME_CPU_FEATURE_X86_SSSE3;
      }
      else if (StringUtils::strieq(simd_level, "sse4.1")) {
        selected_cpu_features = BL_RUNTIME_CPU_FEATURE_X86_SSE4_1;
      }
      else if (StringUtils::strieq(simd_level, "sse4.2")) {
        selected_cpu_features = BL_RUNTIME_CPU_FEATURE_X86_SSE4_2;
      }
      else if (StringUtils::strieq(simd_level, "avx")) {
        selected_cpu_features = BL_RUNTIME_CPU_FEATURE_X86_AVX;
      }
      else if (StringUtils::strieq(simd_level, "avx2")) {
        selected_cpu_features = BL_RUNTIME_CPU_FEATURE_X86_AVX2;
      }
      else if (StringUtils::strieq(simd_level, "avx512")) {
        selected_cpu_features = BL_RUNTIME_CPU_FEATURE_X86_AVX512;
      }
#elif defined(BL_JIT_ARCH_A64)
      else if (strcmp(simd_level, "asimd") == 0) {
        // Currently the default...
      }
#endif
      else {
        printf("Failed to process command line arguments:\n");
        printf("  Unknown simd-level '%s' - please use --help to list all available simd levels\n", simd_level);
        return false;
      }
    }

    return true;
  }

  void stringify_feature_id(BLString& out, uint32_t cpu_features) noexcept {
    if (cpu_features)
      out.assign(StringUtils::cpu_x86_feature_to_string(BLRuntimeCpuFeatures(cpu_features)));
    else
      out.assign("native");
  }

  bool run_with_features(BLFormat format, uint32_t cpu_features) {
    reset_counters();
    stringify_feature_id(_cpu_features_string, cpu_features);

    ContextTester a_tester(test_cases, "ref");
    ContextTester b_tester(test_cases, "jit");

    a_tester.set_font_data(font_data);
    b_tester.set_font_data(font_data);

    a_tester.set_flush_sync(options.flush_sync);
    b_tester.set_flush_sync(options.flush_sync);

    BLContextCreateInfo a_create_info {};
    BLContextCreateInfo b_create_info {};

    a_create_info.flags = BL_CONTEXT_CREATE_FLAG_DISABLE_JIT;

    if (cpu_features) {
      b_create_info.flags = BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT_RUNTIME |
                          BL_CONTEXT_CREATE_FLAG_OVERRIDE_CPU_FEATURES;
      b_create_info.cpu_features = cpu_features;
    }

    if (a_tester.init(int(options.width), int(options.height), format, a_create_info) != BL_SUCCESS ||
        b_tester.init(int(options.width), int(options.height), format, b_create_info) != BL_SUCCESS) {
      printf("Failed to initialize rendering contexts\n");
      return 1;
    }

    TestInfo info;
    dispatch_runs([&](CommandId command_id, StyleId style_id, StyleOp style_op, CompOp comp_op, OpacityOp opacity_op) {
      BLString s0;
      s0.append_format("%s/%s",
        StringUtils::style_id_to_string(style_id),
        StringUtils::style_op_to_string(style_op));

      BLString s1;
      s1.append_format("%s/%s",
        StringUtils::comp_op_to_string(comp_op),
        StringUtils::opacity_op_to_string(opacity_op));

      info.name.assign_format(
          "%-21s | fmt=%-7s| style+api=%-30s| comp+op=%-20s| simd-level=%-9s",
          StringUtils::command_id_to_string(command_id),
          StringUtils::format_to_string(format),
          s0.data(),
          s1.data(),
          _cpu_features_string.data());

      info.id.assign_format("ctx-jit-%s-%s-%s-%s-%s-%s-%s",
        StringUtils::format_to_string(format),
        StringUtils::command_id_to_string(command_id),
        StringUtils::style_id_to_string(style_id),
        StringUtils::style_op_to_string(style_op),
        StringUtils::comp_op_to_string(comp_op),
        StringUtils::opacity_op_to_string(opacity_op),
        _cpu_features_string.data());

      if (!options.quiet) {
        printf("Running [%s]\n", info.name.data());
      }

      a_tester.set_options(comp_op, opacity_op, style_id, style_op);
      b_tester.set_options(comp_op, opacity_op, style_id, style_op);

      uint32_t adjusted_max_diff = maximum_pixel_difference;
      if (adjusted_max_diff == 0xFFFFFFFFu) {
        adjusted_max_diff = maximum_pixel_difference_of(style_id);
      }

      if (run_multiple(command_id, info, a_tester, b_tester, adjusted_max_diff))
        passed_count++;
      else
        failed_count++;
    });

    a_tester.reset();
    b_tester.reset();

    if (mismatch_count)
      printf("Found %llu mismatches!\n\n", (unsigned long long)mismatch_count);
    else if (!options.quiet)
      printf("\n");

    return !mismatch_count;
  }

  int run(CmdLine cmd_line) {
    print_app_info("Blend2D JIT Rendering Context Tester", cmd_line.has_arg("--quiet"));

    if (cmd_line.has_arg("--help"))
      return help();

    if (!parse_common_options(cmd_line) || !parse_jit_options(cmd_line))
      return 1;

    if (iterate_all_jit_features) {
#if defined(BL_JIT_ARCH_X86)
      static constexpr uint32_t x86_features_list[] = {
        BL_RUNTIME_CPU_FEATURE_X86_SSE2,
        BL_RUNTIME_CPU_FEATURE_X86_SSE3,
        BL_RUNTIME_CPU_FEATURE_X86_SSSE3,
        BL_RUNTIME_CPU_FEATURE_X86_SSE4_1,
        BL_RUNTIME_CPU_FEATURE_X86_SSE4_2,
        BL_RUNTIME_CPU_FEATURE_X86_AVX,
        BL_RUNTIME_CPU_FEATURE_X86_AVX2,
        BL_RUNTIME_CPU_FEATURE_X86_AVX512
      };

      BLRuntimeSystemInfo system_info {};
      BLRuntime::query_system_info(&system_info);

      for (const uint32_t& feature : x86_features_list) {
        if (!(system_info.cpu_features & feature))
          break;

        if (options.quiet) {
          stringify_feature_id(_cpu_features_string, feature);
          printf("Testing [%s] (quiet mode)\n", _cpu_features_string.data());
        }

        for (BLFormat format : test_cases.format_ids) {
          run_with_features(format, feature);
        }
      }
#endif

      // Now run with all features if everything above has passed.
      for (BLFormat format : test_cases.format_ids) {
        run_with_features(format, 0);
      }
    }
    else {
      for (BLFormat format : test_cases.format_ids) {
        run_with_features(format, selected_cpu_features);
      }
    }

    if (failed_count) {
      printf("[FAILED] %u tests out of %u failed\n", failed_count, passed_count + failed_count);
      return 1;
    }
    else {
      printf("[PASSED] %u tests passed\n", passed_count);
      return 0;
    }
  }
};

} // {ContextTests}

int main(int argc, char* argv[]) {
  BLRuntimeScope rt_scope;
  ContextTests::JITTestApp app;

  return app.run(CmdLine(argc, argv));
}
