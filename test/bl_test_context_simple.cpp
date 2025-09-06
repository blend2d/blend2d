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

#include "resources/abeezee_regular_ttf.h"

namespace ContextTests {

class SimpleTestApp : public BaseTestApp {
public:
  SimpleTestApp()
    : BaseTestApp() {}

  int help() {
    using StringUtils::bool_to_string;

    printf("Usage:\n");
    printf("  bl_test_context_simple [options] [--help for help]\n");
    printf("\n");

    printf("Purpose:\n");
    printf("  Simple rendering context tester is designed to verify whether the rendering\n");
    printf("  context can process input commands without crashing or causing undefined\n");
    printf("  behavior. It's also designed to be run with instrumentation such as ASAN,\n");
    printf("  UBSAN, MSAN, and Valgrind.\n");
    printf("\n");
    printf("  Simple rendering context tester doesn't do any verification of the rendered\n");
    printf("  output like other testers do, because it's not its purpose.\n");
    printf("\n");

    print_common_options(default_options);
    print_commands();
    print_formats();
    print_comp_ops();
    print_opacity_ops();
    print_style_ids();
    print_style_ops();

    fflush(stdout);
    return 0;
  }

  int run(CmdLine cmd_line) {
    print_app_info("Blend2D Rendering Context Tester", cmd_line.has_arg("--quiet"));

    if (cmd_line.has_arg("--help"))
      return help();

    if (!parse_common_options(cmd_line))
      return 1;

    for (BLFormat format : test_cases.format_ids) {
      ContextTester tester(test_cases, "simple");

      tester.seed(options.seed);
      tester.set_font_data(font_data);
      tester.set_flush_sync(options.flush_sync);

      BLContextCreateInfo cci {};
      cci.thread_count = options.thread_count;

      if (tester.init(int(options.width), int(options.height), format, cci) != BL_SUCCESS) {
        printf("Failed to initialize the rendering context\n");
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
            "%-21s | fmt=%-7s| style+api=%-30s| comp+op=%-20s",
            StringUtils::command_id_to_string(command_id),
            StringUtils::format_to_string(format),
            s0.data(),
            s1.data());

        if (!options.quiet) {
          printf("Running [%s]\n", info.name.data());
        }

        info.id.assign_format("ctx-simple-%s-%s-%s-%s-%s-%s",
          StringUtils::format_to_string(format),
          StringUtils::command_id_to_string(command_id),
          StringUtils::style_id_to_string(style_id),
          StringUtils::style_op_to_string(style_op),
          StringUtils::comp_op_to_string(comp_op),
          StringUtils::opacity_op_to_string(opacity_op));

        tester.clear();
        tester.set_options(comp_op, opacity_op, style_id, options.style_op);
        tester.render(command_id, options.count, options);

        if (options.store_images) {
          store_image(tester.image(), info.id.data());
        }
      });

      tester.reset();
    }

    printf("Testing finished...\n");
    return 0;
  }
};

} // {ContextTests}

int main(int argc, char* argv[]) {
  BLRuntimeScope rt_scope;
  ContextTests::SimpleTestApp app;

  return app.run(CmdLine(argc, argv));
}
