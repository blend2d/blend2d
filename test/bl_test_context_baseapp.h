// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// This file provides utility classes and functions shared between some tests.

#ifndef BLEND2D_TEST_FUZZ_BASEAPP_H_INCLUDED
#define BLEND2D_TEST_FUZZ_BASEAPP_H_INCLUDED

#include <blend2d.h>

#include "bl_test_cmdline.h"
#include "bl_test_context_utilities.h"

namespace ContextTests {

class BaseTestApp {
public:
  struct TestInfo {
    BLString name;
    BLString id;
  };

  TestOptions defaultOptions {};
  TestOptions options {};
  BLFontData fontData;

  // Statistics from runMultiple().
  uint32_t mismatchCount {};

  BaseTestApp();
  ~BaseTestApp();

  static TestOptions makeDefaultOptions();
  bool parseCommonOptions(const CmdLine& cmdLine);

  bool shouldRun(CommandId cmd) const;

  template<typename RunFunc>
  void dispatchRuns(RunFunc&& run) {
    uint32_t compOpCount = options.compOp <= CompOp::kRandom ? 1 : uint32_t(CompOp::kRandom);
    uint32_t opacityOpCount = options.opacityOp <= OpacityOp::kRandom ? 1 : uint32_t(OpacityOp::kRandom);

    for (uint32_t i = 0; i < uint32_t(CommandId::kAll); i++) {
      CommandId commandId = CommandId(i);
      if (shouldRun(commandId)) {
        for (uint32_t j = 0; j < compOpCount; j++) {
          CompOp compOp = compOpCount != 1 ? CompOp(j) : options.compOp;
          for (uint32_t k = 0; k < opacityOpCount; k++) {
            OpacityOp opacityOp = opacityOpCount != 1 ? OpacityOp(k) : options.opacityOp;
            run(commandId, compOp, opacityOp);
          }
        }
      }
    }
  }

  void printAppInfo(const char* title, bool quiet) const;
  void printCommonOptions(const TestOptions& defaultOptions) const;
  void printFormats() const;
  void printCompOps() const;
  void printOpacityOps() const;
  void printStyleIds() const;
  void printStyleOps() const;
  void printCommands() const;

  bool runMultiple(CommandId commandId, const TestInfo& info, ContextTester& aTester, ContextTester& bTester, uint32_t maxDiff);
  void findProblem(CommandId commandId, const TestInfo& info, ContextTester& aTester, ContextTester& bTester, uint32_t maxDiff);

  bool checkOutput(const char* testId, const ContextTester& aTester, const ContextTester& bTester, uint32_t maxDiff);
  void storeImage(const BLImage& image, const char* name, const char* suffix = nullptr) const;
};

} // {ContextTests}

#endif // BLEND2D_TEST_FUZZ_BASEAPP_H_INCLUDED
