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

class MTTestApp : public BaseTestApp {
public:
  uint32_t failedCount {};
  uint32_t passedCount {};

  MTTestApp() : BaseTestApp() {
    defaultOptions.threadCount = 2u;
  }

  int help() {
    using StringUtils::boolToString;

    printf("Usage:\n");
    printf("  bl_test_context_mt [options] [--help for help]\n");
    printf("\n");

    printf("Purpose:\n");
    printf("  Multi-threaded rendering context tester is designed to verify whether both\n");
    printf("  single-threaded and multi-threaded rendering contexts yield pixel identical\n");
    printf("  output when used with the same input data.\n");
    printf("\n");

    printCommonOptions(defaultOptions);

    printf("Multithreading Options:\n");
    printf("  --flush-sync            - Do occasional syncs between calls [default=%s]\n", boolToString(defaultOptions.flushSync));
    printf("  --thread-count=<uint>   - Number of threads of MT context   [default=%u]\n", defaultOptions.threadCount);
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

  bool parseMTOptions(CmdLine cmdLine) {
    options.flushSync = cmdLine.hasArg("--flush-sync") || defaultOptions.flushSync;
    options.threadCount = cmdLine.valueAsUInt("--thread-count", defaultOptions.threadCount);

    return true;
  }

  int run(CmdLine cmdLine) {
    printAppInfo("Blend2D Multi-Threaded Rendering Context Tester", cmdLine.hasArg("--quiet"));

    if (cmdLine.hasArg("--help"))
      return help();

    if (!parseCommonOptions(cmdLine) || !parseMTOptions(cmdLine))
      return 1;

    for (BLFormat format : testCases.formatIds) {
      ContextTester aTester(testCases, "st");
      ContextTester bTester(testCases, "mt");

      aTester.setFlushSync(options.flushSync);
      bTester.setFlushSync(options.flushSync);

      BLContextCreateInfo aCreateInfo {};
      BLContextCreateInfo bCreateInfo {};

      bCreateInfo.threadCount = options.threadCount;

      if (aTester.init(int(options.width), int(options.height), format, aCreateInfo) != BL_SUCCESS ||
          bTester.init(int(options.width), int(options.height), format, bCreateInfo) != BL_SUCCESS) {
        printf("Failed to initialize rendering contexts\n");
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
            "%-21s | fmt=%-7s| style+api=%-30s| comp+op=%-20s| thread-count=%u",
            StringUtils::commandIdToString(commandId),
            StringUtils::formatToString(format),
            s0.data(),
            s1.data(),
            options.threadCount);

        info.id.assignFormat("ctx-mt-%s-%s-%s-%s-%s-%s-%u",
          StringUtils::formatToString(format),
          StringUtils::commandIdToString(commandId),
          StringUtils::styleIdToString(styleId),
          StringUtils::styleOpToString(styleOp),
          StringUtils::compOpToString(compOp),
          StringUtils::opacityOpToString(opacityOp),
          options.threadCount);

        if (!options.quiet) {
          printf("Running [%s]\n", info.name.data());
        }

        aTester.setOptions(compOp, opacityOp, styleId, styleOp);
        bTester.setOptions(compOp, opacityOp, styleId, styleOp);

        if (runMultiple(commandId, info, aTester, bTester, 0))
          passedCount++;
        else
          failedCount++;
      });

      aTester.reset();
      bTester.reset();
    }

    if (failedCount) {
      printf("[FAILED] %u tests out of %u failed\n", failedCount, passedCount + failedCount);
      return 1;
    }
    else {
      printf("[PASSED] %u tests passed\n", passedCount);
      return 0;
    }

    return failedCount ? 1 : 0;
  }
};

} // {ContextTests}

int main(int argc, char* argv[]) {
  BLRuntimeScope rtScope;
  ContextTests::MTTestApp app;

  return app.run(CmdLine(argc, argv));
}
