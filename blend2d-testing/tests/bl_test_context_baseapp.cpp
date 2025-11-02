// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/blend2d.h>
#include <blend2d-testing/commons/imagediff.h>
#include <blend2d-testing/resources/abeezee_regular_ttf.h>
#include <blend2d-testing/tests/bl_test_context_baseapp.h>

namespace ContextTests {

BaseTestApp::BaseTestApp()
  : default_options{make_default_options()} {}

BaseTestApp::~BaseTestApp() {}

TestOptions BaseTestApp::make_default_options() {
  TestOptions opt {};
  opt.width = 513;
  opt.height = 513;
  opt.format = BL_FORMAT_PRGB32;
  opt.count = 1000;
  opt.thread_count = 0;
  opt.seed = 1;
  opt.style_id = StyleId::kRandom;
  opt.comp_op = CompOp::kRandom;
  opt.opacity_op = OpacityOp::kRandom;
  opt.command = CommandId::kAll;
  opt.font = "built-in";
  opt.font_size = 20;
  opt.face_index = 0;
  opt.quiet = false;
  opt.flush_sync = false;
  opt.store_images = false;
  return opt;
}

bool BaseTestApp::parse_common_options(const CmdLine& cmd_line) {
  using namespace StringUtils;

  options.width = cmd_line.value_as_uint("--width", default_options.width);
  options.height = cmd_line.value_as_uint("--height", default_options.height);
  options.count = cmd_line.value_as_uint("--count", default_options.count);
  options.seed = cmd_line.value_as_uint("--seed", default_options.seed);
  options.style_id = parse_style_id(cmd_line.value_of("--style", style_id_to_string(default_options.style_id)));
  options.style_op = parse_style_op(cmd_line.value_of("--style-op", style_op_to_string(default_options.style_op)));
  options.comp_op = parse_comp_op(cmd_line.value_of("--comp-op", comp_op_to_string(default_options.comp_op)));
  options.opacity_op = parse_opacity_op(cmd_line.value_of("--opacity-op", opacity_op_to_string(default_options.opacity_op)));
  options.command = parse_command_id(cmd_line.value_of("--command", command_id_to_string(default_options.command)));
  options.font = cmd_line.value_of("--font", "built-in");
  options.font_size = cmd_line.value_as_uint("--font-size", default_options.font_size);
  options.face_index = cmd_line.value_as_uint("--face-index", default_options.face_index);
  options.quiet = cmd_line.has_arg("--quiet") || default_options.quiet;
  options.store_images = cmd_line.has_arg("--store") || default_options.store_images;

  const char* format_string = cmd_line.value_of("--format", "all");
  options.format = parse_format(format_string);

  bool format_valid = options.format != BL_FORMAT_NONE;

  if (!format_valid && StringUtils::strieq(format_string, "all")) {
    format_valid = true;
  }

  if (!format_valid ||
      options.command == CommandId::kUnknown ||
      options.style_id == StyleId::kUnknown ||
      options.comp_op == CompOp::kUnknown ||
      options.opacity_op == OpacityOp::kUnknown
  ) {
    printf("Failed to process command line arguments:\n");

    if (!format_valid)
      printf("  Unknown format '%s' - please use --help to list all available pixel formats\n", cmd_line.value_of("--format", ""));

    if (options.comp_op == CompOp::kUnknown)
      printf("  Unknown comp_op '%s' - please use --help to list all available operators\n", cmd_line.value_of("--comp-op", ""));

    if (options.opacity_op == OpacityOp::kUnknown)
      printf("  Unknown opacity_op '%s' - please use --help to list all available options\n", cmd_line.value_of("--opacity-op", ""));

    if (options.style_id == StyleId::kUnknown)
      printf("  Unknown style '%s' - please use --help to list all available styles\n", cmd_line.value_of("--style", ""));

    if (options.style_op == StyleOp::kUnknown)
      printf("  Unknown style-op '%s' - please use --help to list all available style options\n", cmd_line.value_of("--style-op", ""));

    if (options.command == CommandId::kUnknown)
      printf("  Unknown command '%s' - please use --help to list all available commands\n", cmd_line.value_of("--command", ""));

    return false;
  }

  if (strieq(options.font, "built-in")) {
    BLResult result = font_data.create_from_data(
      resource_abeezee_regular_ttf,
      sizeof(resource_abeezee_regular_ttf));

    if (result != BL_SUCCESS) {
      printf("Failed to load built-in font (result=0x%08X)\n", result);
      return false;
    }
  }
  else {
    BLResult result = font_data.create_from_file(options.font);
    if (result != BL_SUCCESS) {
      printf("Failed to load font %s (result=0x%08X)\n", options.font, result);
      return false;
    }
  }

  // Add all choices into the lists that are iterated during testing.
  if (options.format == BL_FORMAT_NONE) {
    test_cases.format_ids.push_back(BL_FORMAT_PRGB32);
    test_cases.format_ids.push_back(BL_FORMAT_A8);
  }
  else {
    test_cases.format_ids.push_back(options.format);
  }

  if (options.command == CommandId::kAll) {
    for (uint32_t i = 0; i < uint32_t(CommandId::kAll); i++) {
      test_cases.command_ids.push_back(CommandId(i));
    }
  }
  else {
    test_cases.command_ids.push_back(options.command);
  }

  if (options.style_id >= StyleId::kRandom) {
    if (options.style_id == StyleId::kRandomStable || options.style_id == StyleId::kAllStable) {
      test_cases.style_ids.push_back(StyleId::kSolid);
      test_cases.style_ids.push_back(StyleId::kSolidOpaque);
      test_cases.style_ids.push_back(StyleId::kGradientLinear);
      test_cases.style_ids.push_back(StyleId::kGradientLinearDither);
      test_cases.style_ids.push_back(StyleId::kPatternAligned);
      test_cases.style_ids.push_back(StyleId::kPatternFx);
      test_cases.style_ids.push_back(StyleId::kPatternFy);
      test_cases.style_ids.push_back(StyleId::kPatternFxFy);
      test_cases.style_ids.push_back(StyleId::kPatternAffineNearest);
      test_cases.style_ids.push_back(StyleId::kPatternAffineBilinear);
    }
    else if (options.style_id == StyleId::kRandomUnstable || options.style_id == StyleId::kAllUnstable) {
      test_cases.style_ids.push_back(StyleId::kGradientRadial);
      test_cases.style_ids.push_back(StyleId::kGradientRadialDither);
      test_cases.style_ids.push_back(StyleId::kGradientConic);
      test_cases.style_ids.push_back(StyleId::kGradientConicDither);
    }
    else {
      for (uint32_t i = 0; i < uint32_t(StyleId::kRandom); i++) {
        test_cases.style_ids.push_back(StyleId(i));
      }
    }
  }
  else {
    test_cases.style_ids.push_back(options.style_id);
  }

  if (options.style_op >= StyleOp::kRandom) {
    for (uint32_t i = 0; i < uint32_t(StyleOp::kRandom); i++) {
      test_cases.style_ops.push_back(StyleOp(i));
    }
  }
  else {
    test_cases.style_ops.push_back(options.style_op);
  }

  if (options.comp_op >= CompOp::kRandom) {
    for (uint32_t i = 0; i < uint32_t(CompOp::kRandom); i++) {
      test_cases.comp_ops.push_back(CompOp(i));
    }
  }
  else {
    test_cases.comp_ops.push_back(options.comp_op);
  }

  if (options.opacity_op >= OpacityOp::kRandom) {
    for (uint32_t i = 0; i < uint32_t(OpacityOp::kRandom); i++) {
      test_cases.opacity_ops.push_back(OpacityOp(i));
    }
  }
  else {
    test_cases.opacity_ops.push_back(options.opacity_op);
  }

  return true;
}

void BaseTestApp::print_app_info(const char* title, bool quiet) const {
  printf("%s [use --help for command line options]\n", title);

  if (!quiet) {
    BLRuntimeBuildInfo build_info;
    BLRuntime::query_build_info(&build_info);
    printf("  Version    : %u.%u.%u\n"
           "  Build Type : %s\n"
           "  Compiled By: %s\n\n",
           build_info.major_version,
           build_info.minor_version,
           build_info.patch_version,
           build_info.build_type == BL_RUNTIME_BUILD_TYPE_DEBUG ? "Debug" : "Release",
           build_info.compiler_info);
  }

  fflush(stdout);
}

void BaseTestApp::print_common_options(const TestOptions& test_options) const {
  using namespace StringUtils;

  printf("Common test options:\n");
  printf("  --width=<uint>          - Image width                       [default=%u]\n", test_options.width);
  printf("  --height=<uint>         - Image height                      [default=%u]\n", test_options.height);
  printf("  --format=<string>       - Image pixel format                [default=%s]\n", format_to_string(test_options.format));
  printf("  --count=<uint>          - Count of render commands          [default=%u]\n", test_options.count);
  printf("  --seed=<uint>           - Random number generator seed      [default=%u]\n", test_options.seed);
  printf("  --style=<string>        - Style to render commands with     [default=%s]\n", style_id_to_string(test_options.style_id));
  printf("  --style-op=<string>     - Configure how to use styles       [default=%s]\n", style_op_to_string(test_options.style_op));
  printf("  --comp-op=<string>      - Composition operator              [default=%s]\n", comp_op_to_string(test_options.comp_op));
  printf("  --opacity-op=<string>   - Opacity option                    [default=%s]\n", opacity_op_to_string(test_options.opacity_op));
  printf("  --command=<string>      - Specify which command to run      [default=%s]\n", command_id_to_string(test_options.command));
  printf("  --font=<string>         - Specify which font to use         [default=%s]\n", test_options.font);
  printf("  --font-size=<uint>      - Font size                         [default=%u]\n", test_options.font_size);
  printf("  --face-index=<uint>     - Face index of a font collection   [default=%u]\n", test_options.face_index);
  printf("  --store                 - Write resulting images to files   [default=%s]\n", bool_to_string(test_options.store_images));
  printf("  --quiet                 - Don't write log unless necessary  [default=%s]\n", bool_to_string(test_options.quiet));
  printf("\n");
}

void BaseTestApp::print_formats() const {
  using namespace StringUtils;

  printf("List of pixel formats:\n");
  printf("  %-23s - Premultiplied 32-bit ARGB\n", format_to_string(BL_FORMAT_PRGB32));
  printf("  %-23s - 32-bit RGB (1 byte unused)\n", format_to_string(BL_FORMAT_XRGB32));
  printf("  %-23s - 8-bit alpha-only format\n", format_to_string(BL_FORMAT_A8));
  printf("\n");
}

void BaseTestApp::print_comp_ops() const {
  using namespace StringUtils;

  printf("List of composition operators:\n");
  printf("  %-23s - Source over\n", comp_op_to_string(CompOp::kSrcOver));
  printf("  %-23s - Source copy\n", comp_op_to_string(CompOp::kSrcCopy));
  printf("  %-23s - Random operator for every call\n", comp_op_to_string(CompOp::kRandom));
  printf("  %-23s - Tests all separately\n", comp_op_to_string(CompOp::kAll));
  printf("\n");
}

void BaseTestApp::print_opacity_ops() const {
  using namespace StringUtils;

  printf("List of opacity options:\n");
  printf("  %-23s - Opacity is set to fully opaque (1)\n", opacity_op_to_string(OpacityOp::kOpaque));
  printf("  %-23s - Opacity is semi-transparent (0..1)\n", opacity_op_to_string(OpacityOp::kSemi));
  printf("  %-23s - Opacity is always zero (fully transparent)\n", opacity_op_to_string(OpacityOp::kTransparent));
  printf("  %-23s - Random opacity for every call\n", opacity_op_to_string(OpacityOp::kRandom));
  printf("  %-23s - Tests all opacity options separately\n", opacity_op_to_string(OpacityOp::kAll));
  printf("\n");
}

void BaseTestApp::print_style_ids() const {
  using namespace StringUtils;

  printf("List of styles:\n");
  printf("  %-23s - Solid color\n", style_id_to_string(StyleId::kSolid));
  printf("  %-23s - Linear gradient\n", style_id_to_string(StyleId::kGradientLinear));
  printf("  %-23s - Linear gradient (dithered)\n", style_id_to_string(StyleId::kGradientLinearDither));
  printf("  %-23s - Radial gradient\n", style_id_to_string(StyleId::kGradientRadial));
  printf("  %-23s - Radial gradient (dithered)\n", style_id_to_string(StyleId::kGradientRadialDither));
  printf("  %-23s - Conic gradient\n", style_id_to_string(StyleId::kGradientConic));
  printf("  %-23s - Conic gradient (dithered)\n", style_id_to_string(StyleId::kGradientConicDither));
  printf("  %-23s - Pattern with aligned translation (no scaling)\n", style_id_to_string(StyleId::kPatternAligned));
  printf("  %-23s - Pattern with fractional x translation\n", style_id_to_string(StyleId::kPatternFx));
  printf("  %-23s - Pattern with fractional y translation\n", style_id_to_string(StyleId::kPatternFy));
  printf("  %-23s - Pattern with fractional x and y translation\n", style_id_to_string(StyleId::kPatternFxFy));
  printf("  %-23s - Pattern with affine transformation (nearest)\n", style_id_to_string(StyleId::kPatternAffineNearest));
  printf("  %-23s - Pattern with affine transformation (bilinear)\n", style_id_to_string(StyleId::kPatternAffineBilinear));
  printf("  %-23s - Random style for every render call\n", style_id_to_string(StyleId::kRandom));
  printf("  %-23s - Like 'random', but only styles that never require --max-diff\n", style_id_to_string(StyleId::kRandomStable));
  printf("  %-23s - Like 'random', but only styles that could require --max-diff\n", style_id_to_string(StyleId::kRandomUnstable));
  printf("  %-23s - Test all styles separately\n", style_id_to_string(StyleId::kAll));
  printf("  %-23s - Like 'all', but only styles that never require --max-diff\n", style_id_to_string(StyleId::kAllStable));
  printf("  %-23s - Like 'all', but only styles that could require --max-diff\n", style_id_to_string(StyleId::kAllUnstable));
  printf("\n");
}

void BaseTestApp::print_style_ops() const {
  using namespace StringUtils;

  printf("List of style options:\n");
  printf("  %-23s - Pass styles directly to render calls\n", style_op_to_string(StyleOp::kExplicit));
  printf("  %-23s - Use set_fill_style() and set_stroke_style()\n", style_op_to_string(StyleOp::kImplicit));
  printf("  %-23s - Random style option for every render call\n", style_op_to_string(StyleOp::kRandom));
  printf("  %-23s - Test all style options separately\n", style_op_to_string(StyleOp::kAll));
  printf("\n");
}

void BaseTestApp::print_commands() const {
  using namespace StringUtils;

  printf("List of commands:\n");
  printf("  %-23s - Fills aligned rectangles (int coordinates)\n", command_id_to_string(CommandId::kFillRectI));
  printf("  %-23s - Fills unaligned rectangles (float coordinates)\n", command_id_to_string(CommandId::kFillRectD));
  printf("  %-23s - Fills multiple rectangles (float coordinates)\n", command_id_to_string(CommandId::kFillMultipleRects));
  printf("  %-23s - Fills rounded rectangles\n", command_id_to_string(CommandId::kFillRound));
  printf("  %-23s - Fills triangles\n", command_id_to_string(CommandId::kFillTriangle));
  printf("  %-23s - Fills a path having quadratic curves\n", command_id_to_string(CommandId::kFillPathQuad));
  printf("  %-23s - Fills a path having cubic curves\n", command_id_to_string(CommandId::kFillPathCubic));
  printf("  %-23s - Fills text runs\n", command_id_to_string(CommandId::kFillText));
  printf("  %-23s - Strokes aligned rectangles (int coordinates)\n", command_id_to_string(CommandId::kStrokeRectI));
  printf("  %-23s - Strokes unaligned rectangles (float coordinates)\n", command_id_to_string(CommandId::kStrokeRectD));
  printf("  %-23s - Strokes multiple rectangles (float coordinates)\n", command_id_to_string(CommandId::kStrokeMultipleRects));
  printf("  %-23s - Strokes rounded rectangles\n", command_id_to_string(CommandId::kStrokeRound));
  printf("  %-23s - Strokes triangles\n", command_id_to_string(CommandId::kStrokeTriangle));
  printf("  %-23s - Strokes a path having quadratic curves\n", command_id_to_string(CommandId::kStrokePathQuad));
  printf("  %-23s - Strokes a path having cubic curves\n", command_id_to_string(CommandId::kStrokePathCubic));
  printf("  %-23s - Strokes text runs\n", command_id_to_string(CommandId::kStrokeText));
  printf("  %-23s - Test all commands separately\n", command_id_to_string(CommandId::kAll));
  printf("\n");
}

bool BaseTestApp::run_multiple(CommandId command_id, const TestInfo& info, ContextTester& a_tester, ContextTester& b_tester, uint32_t max_diff) {
  a_tester.clear();
  a_tester.seed(options.seed);
  a_tester.render(command_id, options.count, options);

  b_tester.clear();
  b_tester.seed(options.seed);
  b_tester.render(command_id, options.count, options);

  if (!check_output(info.id.data(), a_tester, b_tester, max_diff)) {
    find_problem(command_id, info, a_tester, b_tester, max_diff);
    return false;
  }

  return true;
}

void BaseTestApp::find_problem(CommandId command_id, const TestInfo& info, ContextTester& a_tester, ContextTester& b_tester, uint32_t max_diff) {
  // Do a binary search to find exactly the failing command.
  size_t base = 0;
  size_t size = options.count;

  if (options.quiet) {
    // Print the test name so we will know which test actually failed. This is
    // important especially on CI where we want to use quiet mode by default.
    printf("Testing [%s]\n", info.name.data());
  }

  printf("  Bisecting to match the problematic command...\n");

  while (size_t half = size / 2u) {
    size_t middle = base + half;
    size -= half;

    printf("  Verifying range [%zu %zu)\n", base, base + size);

    a_tester.clear();
    b_tester.clear();

    a_tester.seed(options.seed);
    b_tester.seed(options.seed);

    a_tester.render(command_id, base + size, options);
    b_tester.render(command_id, base + size, options);

    check_output(info.id.data(), a_tester, b_tester, max_diff);

    if (ImageUtils::diff_info(a_tester.image(), b_tester.image()).max_diff <= max_diff)
      base = middle;
  }

  printf("  Mismatch command index: %zu\n", base);

  a_tester.clear();
  b_tester.clear();

  a_tester.seed(options.seed);
  b_tester.seed(options.seed);

  if (base) {
    a_tester.render(command_id, base - 1, options);
    b_tester.render(command_id, base - 1, options);
  }

  a_tester.render(command_id, 1, options);
  b_tester.render(command_id, 1, options);

  check_output(info.id.data(), a_tester, b_tester, max_diff);
}

bool BaseTestApp::check_output(const char* test_id, const ContextTester& a_tester, const ContextTester& b_tester, uint32_t max_diff) {
  const BLImage& a_image = a_tester.image();
  const BLImage& b_image = b_tester.image();

  ImageUtils::DiffInfo diff_info = ImageUtils::diff_info(a_image, b_image);
  if (diff_info.max_diff <= max_diff)
    return true;

  mismatch_count++;

  BLString image_name;
  image_name.assign_format("%s-bug-%05llu", test_id, (unsigned long long)mismatch_count);
  printf("  Mismatch: %s (max_diff=%u cumulative=%llu)\n", image_name.data(), diff_info.max_diff, (unsigned long long)diff_info.cumulative_diff);

  if (options.store_images) {
    BLImage d = ImageUtils::diff_image(a_image, b_image);
    store_image(d, image_name.data(), "diff");
    store_image(a_image, image_name.data(), a_tester.prefix());
    store_image(b_image, image_name.data(), b_tester.prefix());
  }

  return false;
}

void BaseTestApp::store_image(const BLImage& image, const char* name, const char* suffix) const {
  BLString s;
  if (suffix)
    s.assign_format("%s-%s.png", name, suffix);
  else
    s.assign_format("%s.png", name);

  if (!options.quiet)
    printf("  Storing %s\n", s.data());

  image.write_to_file(s.data());
}

} // {ContextTests}
