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
  TestOptions default_options {};
  //! Current options (inherited from default options and parsed from command line).
  TestOptions options {};
  //! Tests cases to execute.
  TestCases test_cases;

  //! Font data to use during text rendering tests.
  BLFontData font_data;

  // Statistics from run_multiple().
  uint32_t mismatch_count {};

  template<typename T>
  struct CaseIterator {
    const std::vector<T>& options;
    uint32_t index {};
    uint32_t count {};
    bool randomized {};
    T random_case {};

    inline CaseIterator(const std::vector<T>& options, bool randomized, T random_case = T::kRandom)
      : options(options),
        index(0),
        count(randomized ? uint32_t(1) : uint32_t(options.size())),
        randomized(randomized),
        random_case(random_case) {}

    inline bool valid() const { return index < count; }
    inline T value() const { return randomized ? random_case : options[index]; }

    inline bool next() { return ++index < count; }
  };

  BaseTestApp();
  ~BaseTestApp();

  static TestOptions make_default_options();
  bool parse_common_options(const CmdLine& cmd_line);

  template<typename RunFunc>
  void dispatch_runs(RunFunc&& run) {
    for (CommandId command_id : test_cases.command_ids) {
      CaseIterator<StyleId> style_id_iterator(test_cases.style_ids, is_random_style(options.style_id), options.style_id);
      do {
        CaseIterator<StyleOp> style_op_iterator(test_cases.style_ops, options.style_op == StyleOp::kRandom);
        do {
          CaseIterator<CompOp> comp_op_iterator(test_cases.comp_ops, options.comp_op == CompOp::kRandom);
          do {
            CaseIterator<OpacityOp> opacity_op_iterator(test_cases.opacity_ops, options.opacity_op == OpacityOp::kRandom);
            do {
              run(command_id,
                  style_id_iterator.value(),
                  style_op_iterator.value(),
                  comp_op_iterator.value(),
                  opacity_op_iterator.value());
            } while (opacity_op_iterator.next());
          } while (comp_op_iterator.next());
        } while (style_op_iterator.next());
      } while (style_id_iterator.next());
    }
  }

  void print_app_info(const char* title, bool quiet) const;
  void print_common_options(const TestOptions& default_options) const;
  void print_formats() const;
  void print_comp_ops() const;
  void print_opacity_ops() const;
  void print_style_ids() const;
  void print_style_ops() const;
  void print_commands() const;

  bool run_multiple(CommandId command_id, const TestInfo& info, ContextTester& a_tester, ContextTester& b_tester, uint32_t max_diff);
  void find_problem(CommandId command_id, const TestInfo& info, ContextTester& a_tester, ContextTester& b_tester, uint32_t max_diff);

  bool check_output(const char* test_id, const ContextTester& a_tester, const ContextTester& b_tester, uint32_t max_diff);
  void store_image(const BLImage& image, const char* name, const char* suffix = nullptr) const;
};

} // {ContextTests}

#endif // BLEND2D_TEST_CONTEXT_BASEAPP_H_INCLUDED
