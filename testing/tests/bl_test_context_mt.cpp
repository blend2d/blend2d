// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bl_test_context_baseapp.h"
#include "bl_test_context_utilities.h"

#include "../commons/imagediff.h"

namespace ContextTests {

class MTTestApp : public BaseTestApp {
public:
  uint32_t failed_count {};
  uint32_t passed_count {};

  MTTestApp() : BaseTestApp() {
    default_options.thread_count = 2u;
  }

  int help() {
    using StringUtils::bool_to_string;

    printf("Usage:\n");
    printf("  bl_test_context_mt [options] [--help for help]\n");
    printf("\n");

    printf("Purpose:\n");
    printf("  Multi-threaded rendering context tester is designed to verify whether both\n");
    printf("  single-threaded and multi-threaded rendering contexts yield pixel identical\n");
    printf("  output when used with the same input data.\n");
    printf("\n");

    print_common_options(default_options);

    printf("Multithreading Options:\n");
    printf("  --flush-sync            - Do occasional syncs between calls [default=%s]\n", bool_to_string(default_options.flush_sync));
    printf("  --thread-count=<uint>   - Number of threads of MT context   [default=%u]\n", default_options.thread_count);
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

  bool parseMTOptions(CmdLine cmd_line) {
    options.flush_sync = cmd_line.has_arg("--flush-sync") || default_options.flush_sync;
    options.thread_count = cmd_line.value_as_uint("--thread-count", default_options.thread_count);

    return true;
  }

  int run(CmdLine cmd_line) {
    print_app_info("Blend2D Multi-Threaded Rendering Context Tester", cmd_line.has_arg("--quiet"));

    if (cmd_line.has_arg("--help"))
      return help();

    if (!parse_common_options(cmd_line) || !parseMTOptions(cmd_line))
      return 1;

    for (BLFormat format : test_cases.format_ids) {
      ContextTester a_tester(test_cases, "st");
      ContextTester b_tester(test_cases, "mt");

      a_tester.set_flush_sync(options.flush_sync);
      b_tester.set_flush_sync(options.flush_sync);

      BLContextCreateInfo a_create_info {};
      BLContextCreateInfo b_create_info {};

      b_create_info.thread_count = options.thread_count;

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
            "%-21s | fmt=%-7s| style+api=%-30s| comp+op=%-20s| thread-count=%u",
            StringUtils::command_id_to_string(command_id),
            StringUtils::format_to_string(format),
            s0.data(),
            s1.data(),
            options.thread_count);

        info.id.assign_format("ctx-mt-%s-%s-%s-%s-%s-%s-%u",
          StringUtils::format_to_string(format),
          StringUtils::command_id_to_string(command_id),
          StringUtils::style_id_to_string(style_id),
          StringUtils::style_op_to_string(style_op),
          StringUtils::comp_op_to_string(comp_op),
          StringUtils::opacity_op_to_string(opacity_op),
          options.thread_count);

        if (!options.quiet) {
          printf("Running [%s]\n", info.name.data());
        }

        a_tester.set_options(comp_op, opacity_op, style_id, style_op);
        b_tester.set_options(comp_op, opacity_op, style_id, style_op);

        if (run_multiple(command_id, info, a_tester, b_tester, 0))
          passed_count++;
        else
          failed_count++;
      });

      a_tester.reset();
      b_tester.reset();
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
  ContextTests::MTTestApp app;

  return app.run(CmdLine(argc, argv));
}
