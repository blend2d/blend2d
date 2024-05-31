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

    ContextTester tester("simple");

    tester.seed(options.seed);
    tester.setFontData(fontData);
    tester.setFlushSync(options.flushSync);

    BLContextCreateInfo cci {};
    cci.threadCount = options.threadCount;

    if (tester.init(int(options.width), int(options.height), options.format, cci) != BL_SUCCESS) {
      printf("Failed to initialize the rendering context\n");
      return 1;
    }

    BLString testId;
    dispatchRuns([&](CommandId commandId, CompOp compOp, OpacityOp opacityOp) {
      if (!options.quiet) {
        printf("Testing [%s | %s | %s | %s]:\n",
          StringUtils::commandIdToString(commandId),
          StringUtils::compOpToString(compOp),
          StringUtils::opacityOpToString(opacityOp),
          StringUtils::styleIdToString(options.styleId));
      }

      testId.assignFormat("test-simple-%s-%s-%s-%s",
        StringUtils::commandIdToString(commandId),
        StringUtils::compOpToString(compOp),
        StringUtils::opacityOpToString(opacityOp),
        StringUtils::styleIdToString(options.styleId));

      tester.clear();
      tester.setOptions(compOp, opacityOp, options.styleId, options.styleOp);
      tester.render(commandId, options.count, options);

      if (options.storeImages) {
        storeImage(tester.image(), testId.data());
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
