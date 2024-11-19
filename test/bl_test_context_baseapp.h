// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// This file provides utility classes and functions shared between some tests.

#ifndef BLEND2D_TEST_CONTEXT_BASEAPP_H_INCLUDED
#define BLEND2D_TEST_CONTEXT_BASEAPP_H_INCLUDED

#include <blend2d.h>
#include <vector>

#include "bl_test_cmdline.h"
#include "bl_test_context_utilities.h"

namespace ContextTests {

class BaseTestApp {
public:
  struct TestInfo {
    BLString name;
    BLString id;
  };

  //! Default options.
  TestOptions defaultOptions {};
  //! Current options (inherited from default options and parsed from command line).
  TestOptions options {};
  //! Tests cases to execute.
  TestCases testCases;

  //! Font data to use during text rendering tests.
  BLFontData fontData;

  // Statistics from runMultiple().
  uint32_t mismatchCount {};

  template<typename T>
  struct CaseIterator {
    const std::vector<T>& options;
    uint32_t index {};
    uint32_t count {};
    bool randomized {};
    T randomCase {};

    inline CaseIterator(const std::vector<T>& options, bool randomized, T randomCase = T::kRandom)
      : options(options),
        index(0),
        count(randomized ? uint32_t(1) : uint32_t(options.size())),
        randomized(randomized),
        randomCase(randomCase) {}

    inline bool valid() const { return index < count; }
    inline T value() const { return randomized ? randomCase : options[index]; }

    inline bool next() { return ++index < count; }
  };

  BaseTestApp();
  ~BaseTestApp();

  static TestOptions makeDefaultOptions();
  bool parseCommonOptions(const CmdLine& cmdLine);

  template<typename RunFunc>
  void dispatchRuns(RunFunc&& run) {
    for (CommandId commandId : testCases.commandIds) {
      CaseIterator<StyleId> styleIdIterator(testCases.styleIds, isRandomStyle(options.styleId), options.styleId);
      do {
        CaseIterator<StyleOp> styleOpIterator(testCases.styleOps, options.styleOp == StyleOp::kRandom);
        do {
          CaseIterator<CompOp> compOpIterator(testCases.compOps, options.compOp == CompOp::kRandom);
          do {
            CaseIterator<OpacityOp> opacityOpIterator(testCases.opacityOps, options.opacityOp == OpacityOp::kRandom);
            do {
              run(commandId,
                  styleIdIterator.value(),
                  styleOpIterator.value(),
                  compOpIterator.value(),
                  opacityOpIterator.value());
            } while (opacityOpIterator.next());
          } while (compOpIterator.next());
        } while (styleOpIterator.next());
      } while (styleIdIterator.next());
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

#endif // BLEND2D_TEST_CONTEXT_BASEAPP_H_INCLUDED
