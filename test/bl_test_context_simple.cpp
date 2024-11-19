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
    using StringUtils::boolToString;

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

    printCommonOptions(defaultOptions);
    printCommands();
    printFormats();
    printCompOps();
    printOpacityOps();
    printStyleIds();
    printStyleOps();

    fflush(stdout);
    return 0;
  }

  int run(CmdLine cmdLine) {
    printAppInfo("Blend2D Rendering Context Tester", cmdLine.hasArg("--quiet"));

    if (cmdLine.hasArg("--help"))
      return help();

    if (!parseCommonOptions(cmdLine))
      return 1;

    ContextTester tester(testCases, "simple");

    tester.seed(options.seed);
    tester.setFontData(fontData);
    tester.setFlushSync(options.flushSync);

    BLContextCreateInfo cci {};
    cci.threadCount = options.threadCount;

    if (tester.init(int(options.width), int(options.height), options.format, cci) != BL_SUCCESS) {
      printf("Failed to initialize the rendering context\n");
      return 1;
    }

    TestInfo info;
    dispatchRuns([&](CommandId commandId, StyleId styleId, StyleOp styleOp, CompOp compOp, OpacityOp opacityOp) {
      BLString s0;
      s0.appendFormat("%s/%s",
        StringUtils::styleIdToString(styleId),
        StringUtils::styleOpToString(styleOp));

      BLString s1;
      s1.appendFormat("%s/%s",
        StringUtils::compOpToString(compOp),
        StringUtils::opacityOpToString(opacityOp));

      info.name.assignFormat(
          "%-21s | style+api=%-25s| comp+op=%-20s",
          StringUtils::commandIdToString(commandId),
          s0.data(),
          s1.data());

      if (!options.quiet) {
        printf("Running [%s]\n", info.name.data());
      }

      info.id.assignFormat("ctx-simple-%s-%s-%s-%s-%s",
        StringUtils::commandIdToString(commandId),
        StringUtils::styleIdToString(styleId),
        StringUtils::styleOpToString(styleOp),
        StringUtils::compOpToString(compOp),
        StringUtils::opacityOpToString(opacityOp));

      tester.clear();
      tester.setOptions(compOp, opacityOp, styleId, options.styleOp);
      tester.render(commandId, options.count, options);

      if (options.storeImages) {
        storeImage(tester.image(), info.id.data());
      }
    });

    tester.reset();

    printf("Testing finished...\n");
    return 0;
  }
};

} // {ContextTests}

int main(int argc, char* argv[]) {
  BLRuntimeScope rtScope;
  ContextTests::SimpleTestApp app;

  return app.run(CmdLine(argc, argv));
}
