// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d.h>

#include "bl_test_cmdline.h"
#include "bl_test_context_baseapp.h"
#include "bl_test_imageutils.h"
#include "resources/abeezee_regular_ttf.h"

namespace ContextTests {

BaseTestApp::BaseTestApp()
  : defaultOptions{makeDefaultOptions()} {}

BaseTestApp::~BaseTestApp() {}

TestOptions BaseTestApp::makeDefaultOptions() {
  TestOptions opt {};
  opt.width = 513;
  opt.height = 513;
  opt.format = BL_FORMAT_PRGB32;
  opt.count = 1000;
  opt.threadCount = 0;
  opt.seed = 1;
  opt.styleId = StyleId::kSolid;
  opt.compOp = CompOp::kSrcOver;
  opt.opacityOp = OpacityOp::kOpaque;
  opt.command = CommandId::kAll;
  opt.font = "built-in";
  opt.fontSize = 20;
  opt.faceIndex = 0;
  opt.quiet = false;
  opt.flushSync = false;
  opt.storeImages = false;
  return opt;
}

bool BaseTestApp::parseCommonOptions(const CmdLine& cmdLine) {
  using namespace StringUtils;

  options.width = cmdLine.valueAsUInt("--width", defaultOptions.width);
  options.height = cmdLine.valueAsUInt("--height", defaultOptions.height);
  options.format = parseFormat(cmdLine.valueOf("--format", formatToString(defaultOptions.format)));
  options.count = cmdLine.valueAsUInt("--count", defaultOptions.count);
  options.seed = cmdLine.valueAsUInt("--seed", defaultOptions.seed);
  options.styleId = parseStyleId(cmdLine.valueOf("--style", styleIdToString(defaultOptions.styleId)));
  options.styleOp = parseStyleOp(cmdLine.valueOf("--style-op", styleOpToString(defaultOptions.styleOp)));
  options.compOp = parseCompOp(cmdLine.valueOf("--comp-op", compOpToString(defaultOptions.compOp)));
  options.opacityOp = parseOpacityOp(cmdLine.valueOf("--opacity-op", opacityOpToString(defaultOptions.opacityOp)));
  options.command = parseCommandId(cmdLine.valueOf("--command", commandIdToString(defaultOptions.command)));
  options.font = cmdLine.valueOf("--font", "built-in");
  options.fontSize = cmdLine.valueAsUInt("--font-size", defaultOptions.fontSize);
  options.faceIndex = cmdLine.valueAsUInt("--face-index", defaultOptions.faceIndex);
  options.quiet = cmdLine.hasArg("--quiet") || defaultOptions.quiet;
  options.storeImages = cmdLine.hasArg("--store") || defaultOptions.storeImages;

  if (options.command == CommandId::kUnknown ||
      options.styleId == StyleId::kUnknown ||
      options.compOp == CompOp::kUnknown ||
      options.opacityOp == OpacityOp::kUnknown) {
    printf("Failed to process command line arguments:\n");

    if (options.compOp == CompOp::kUnknown)
      printf("  Unknown compOp '%s' - please use --help to list all available operators\n", cmdLine.valueOf("--comp-op", ""));

    if (options.opacityOp == OpacityOp::kUnknown)
      printf("  Unknown opacityOp '%s' - please use --help to list all available options\n", cmdLine.valueOf("--opacity-op", ""));

    if (options.styleId == StyleId::kUnknown)
      printf("  Unknown style '%s' - please use --help to list all available styles\n", cmdLine.valueOf("--style", ""));

    if (options.styleOp == StyleOp::kUnknown)
      printf("  Unknown style-op '%s' - please use --help to list all available style options\n", cmdLine.valueOf("--style-op", ""));

    if (options.command == CommandId::kUnknown)
      printf("  Unknown command '%s' - please use --help to list all available commands\n", cmdLine.valueOf("--command", ""));

    return false;
  }

  if (strieq(options.font, "built-in")) {
    BLResult result = fontData.createFromData(
      resource_abeezee_regular_ttf,
      sizeof(resource_abeezee_regular_ttf));

    if (result != BL_SUCCESS) {
      printf("Failed to load built-in font (result=0x%08X)\n", result);
      return false;
    }
  }
  else {
    BLResult result = fontData.createFromFile(options.font);
    if (result != BL_SUCCESS) {
      printf("Failed to load font %s (result=0x%08X)\n", options.font, result);
      return false;
    }
  }

  return true;
}

bool BaseTestApp::shouldRun(CommandId cmd) const {
  return options.command == cmd || options.command == CommandId::kAll;
}

void BaseTestApp::printAppInfo(const char* title, bool quiet) const {
  printf("%s [use --help for command line options]\n", title);

  if (!quiet) {
    BLRuntimeBuildInfo buildInfo;
    BLRuntime::queryBuildInfo(&buildInfo);
    printf("  Version    : %u.%u.%u\n"
           "  Build Type : %s\n"
           "  Compiled By: %s\n\n",
           buildInfo.majorVersion,
           buildInfo.minorVersion,
           buildInfo.patchVersion,
           buildInfo.buildType == BL_RUNTIME_BUILD_TYPE_DEBUG ? "Debug" : "Release",
           buildInfo.compilerInfo);
  }
  fflush(stdout);
}

void BaseTestApp::printCommonOptions(const TestOptions& defaultOptions) const {
  using namespace StringUtils;

  printf("Common test options:\n");
  printf("  --width=<uint>          - Image width                       [default=%u]\n", defaultOptions.width);
  printf("  --height=<uint>         - Image height                      [default=%u]\n", defaultOptions.height);
  printf("  --format=<string>       - Image pixel format                [default=%s]\n", formatToString(defaultOptions.format));
  printf("  --count=<uint>          - Count of render commands          [default=%u]\n", defaultOptions.count);
  printf("  --seed=<uint>           - Random number generator seed      [default=%u]\n", defaultOptions.seed);
  printf("  --style=<string>        - Style to render commands with     [default=%s]\n", styleIdToString(defaultOptions.styleId));
  printf("  --style-op=<string>     - Configure how to use styles       [default=%s]\n", styleOpToString(defaultOptions.styleOp));
  printf("  --comp-op=<string>      - Composition operator              [default=%s]\n", compOpToString(defaultOptions.compOp));
  printf("  --opacity-op=<string>   - Opacity option                    [default=%s]\n", opacityOpToString(defaultOptions.opacityOp));
  printf("  --command=<string>      - Specify which command to run      [default=%s]\n", commandIdToString(defaultOptions.command));
  printf("  --font=<string>         - Specify which font to use         [default=%s]\n", defaultOptions.font);
  printf("  --font-size=<uint>      - Font size                         [default=%u]\n", defaultOptions.fontSize);
  printf("  --face-index=<uint>     - Face index of a font collection   [default=%u]\n", defaultOptions.faceIndex);
  printf("  --store                 - Write resulting images to files   [default=%s]\n", boolToString(defaultOptions.storeImages));
  printf("  --quiet                 - Don't write log unless necessary  [default=%s]\n", boolToString(defaultOptions.quiet));
  printf("\n");
}

void BaseTestApp::printFormats() const {
  using namespace StringUtils;

  printf("List of pixel formats:\n");
  printf("  %-23s - Premultiplied 32-bit ARGB\n", formatToString(BL_FORMAT_PRGB32));
  printf("  %-23s - 32-bit RGB (1 byte unused)\n", formatToString(BL_FORMAT_XRGB32));
  printf("  %-23s - 8-bit alpha-only format\n", formatToString(BL_FORMAT_A8));
  printf("\n");
}

void BaseTestApp::printCompOps() const {
  using namespace StringUtils;

  printf("List of composition operators:\n");
  printf("  %-23s - Source over\n", compOpToString(CompOp::kSrcOver));
  printf("  %-23s - Source copy\n", compOpToString(CompOp::kSrcCopy));
  printf("  %-23s - Random operator for every call\n", compOpToString(CompOp::kRandom));
  printf("  %-23s - Tests all separately\n", compOpToString(CompOp::kAll));
  printf("\n");
}

void BaseTestApp::printOpacityOps() const {
  using namespace StringUtils;

  printf("List of opacity options:\n");
  printf("  %-23s - Opacity is set to fully opaque (1)\n", opacityOpToString(OpacityOp::kOpaque));
  printf("  %-23s - Opacity is semi-transparent (0..1)\n", opacityOpToString(OpacityOp::kSemi));
  printf("  %-23s - Opacity is always zero (fully transparent)\n", opacityOpToString(OpacityOp::kTransparent));
  printf("  %-23s - Random opacity for every call\n", opacityOpToString(OpacityOp::kRandom));
  printf("  %-23s - Tests all opacity options separately\n", opacityOpToString(OpacityOp::kAll));
  printf("\n");
}

void BaseTestApp::printStyleIds() const {
  using namespace StringUtils;

  printf("List of styles:\n");
  printf("  %-23s - Solid color\n", styleIdToString(StyleId::kSolid));
  printf("  %-23s - Linear gradient\n", styleIdToString(StyleId::kGradientLinear));
  printf("  %-23s - Radial gradient\n", styleIdToString(StyleId::kGradientRadial));
  printf("  %-23s - Conic gradient\n", styleIdToString(StyleId::kGradientConic));
  printf("  %-23s - Pattern with aligned translation (no scaling)\n", styleIdToString(StyleId::kPatternAligned));
  printf("  %-23s - Pattern with fractional x translation\n", styleIdToString(StyleId::kPatternFx));
  printf("  %-23s - Pattern with fractional y translation\n", styleIdToString(StyleId::kPatternFy));
  printf("  %-23s - Pattern with fractional x and y translation\n", styleIdToString(StyleId::kPatternFxFy));
  printf("  %-23s - Pattern with affine transformation (nearest)\n", styleIdToString(StyleId::kPatternAffineNearest));
  printf("  %-23s - Pattern with affine transformation (bilinear)\n", styleIdToString(StyleId::kPatternAffineBilinear));
  printf("  %-23s - Random style for every render call\n", styleIdToString(StyleId::kRandom));
  printf("\n");
}

void BaseTestApp::printStyleOps() const {
  using namespace StringUtils;

  printf("List of style options:\n");
  printf("  %-23s - Pass styles directly to render calls\n", styleOpToString(StyleOp::kExplicit));
  printf("  %-23s - Use setFillStyle() and setStrokeStyle()\n", styleOpToString(StyleOp::kImplicit));
  printf("  %-23s - Random style option for every render call\n", styleOpToString(StyleOp::kRandom));
  printf("\n");
}

void BaseTestApp::printCommands() const {
  using namespace StringUtils;

  printf("List of commands:\n");
  printf("  %-23s - Fills aligned rectangles (int coordinates)\n", commandIdToString(CommandId::kFillRectI));
  printf("  %-23s - Fills unaligned rectangles (float coordinates)\n", commandIdToString(CommandId::kFillRectD));
  printf("  %-23s - Fills multiple rectangles (float coordinates)\n", commandIdToString(CommandId::kFillMultipleRects));
  printf("  %-23s - Fills rounded rectangles\n", commandIdToString(CommandId::kFillRound));
  printf("  %-23s - Fills triangles\n", commandIdToString(CommandId::kFillTriangle));
  printf("  %-23s - Fills a path having quadratic curves\n", commandIdToString(CommandId::kFillPathQuad));
  printf("  %-23s - Fills a path having cubic curves\n", commandIdToString(CommandId::kFillPathCubic));
  printf("  %-23s - Fills text runs\n", commandIdToString(CommandId::kFillText));
  printf("  %-23s - Strokes aligned rectangles (int coordinates)\n", commandIdToString(CommandId::kStrokeRectI));
  printf("  %-23s - Strokes unaligned rectangles (float coordinates)\n", commandIdToString(CommandId::kStrokeRectD));
  printf("  %-23s - Strokes multiple rectangles (float coordinates)\n", commandIdToString(CommandId::kStrokeMultipleRects));
  printf("  %-23s - Strokes rounded rectangles\n", commandIdToString(CommandId::kStrokeRound));
  printf("  %-23s - Strokes triangles\n", commandIdToString(CommandId::kStrokeTriangle));
  printf("  %-23s - Strokes a path having quadratic curves\n", commandIdToString(CommandId::kStrokePathQuad));
  printf("  %-23s - Strokes a path having cubic curves\n", commandIdToString(CommandId::kStrokePathCubic));
  printf("  %-23s - Strokes text runs\n", commandIdToString(CommandId::kStrokeText));
  printf("  %-23s - Executes all commands separately\n", commandIdToString(CommandId::kAll));
  printf("\n");
}

bool BaseTestApp::runMultiple(CommandId commandId, const TestInfo& info, ContextTester& aTester, ContextTester& bTester, uint32_t maxDiff) {
  aTester.clear();
  aTester.seed(options.seed);
  aTester.render(commandId, options.count, options);

  bTester.clear();
  bTester.seed(options.seed);
  bTester.render(commandId, options.count, options);

  if (!checkOutput(info.id.data(), aTester, bTester, maxDiff)) {
    findProblem(commandId, info, aTester, bTester, maxDiff);
    return false;
  }

  return true;
}

void BaseTestApp::findProblem(CommandId commandId, const TestInfo& info, ContextTester& aTester, ContextTester& bTester, uint32_t maxDiff) {
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

    aTester.clear();
    bTester.clear();

    aTester.seed(options.seed);
    bTester.seed(options.seed);

    aTester.render(commandId, base + size, options);
    bTester.render(commandId, base + size, options);

    checkOutput(info.id.data(), aTester, bTester, maxDiff);

    if (ImageUtils::diffInfo(aTester.image(), bTester.image()).maxDiff <= maxDiff)
      base = middle;
  }

  printf("  Mismatch command index: %zu\n", base);

  aTester.clear();
  bTester.clear();

  aTester.seed(options.seed);
  bTester.seed(options.seed);

  if (base) {
    aTester.render(commandId, base - 1, options);
    bTester.render(commandId, base - 1, options);
  }

  aTester.render(commandId, 1, options);
  bTester.render(commandId, 1, options);

  checkOutput(info.id.data(), aTester, bTester, maxDiff);
}

bool BaseTestApp::checkOutput(const char* testId, const ContextTester& aTester, const ContextTester& bTester, uint32_t maxDiff) {
  const BLImage& aImage = aTester.image();
  const BLImage& bImage = bTester.image();

  ImageUtils::DiffInfo diffInfo = ImageUtils::diffInfo(aImage, bImage);
  if (diffInfo.maxDiff <= maxDiff)
    return true;

  mismatchCount++;

  BLString imageName;
  imageName.assignFormat("%s-bug-%05llu", testId, (unsigned long long)mismatchCount);
  printf("  Mismatch: %s (maxDiff=%u cumulative=%llu)\n", imageName.data(), diffInfo.maxDiff, (unsigned long long)diffInfo.cumulativeDiff);

  if (options.storeImages) {
    BLImage d = ImageUtils::diffImage(aImage, bImage);
    storeImage(d, imageName.data(), "diff");
    storeImage(aImage, imageName.data(), aTester.prefix());
    storeImage(bImage, imageName.data(), bTester.prefix());
  }

  return false;
}

void BaseTestApp::storeImage(const BLImage& image, const char* name, const char* suffix) const {
  BLString s;
  if (suffix)
    s.assignFormat("%s-%s.png", name, suffix);
  else
    s.assignFormat("%s.png", name);

  if (!options.quiet)
    printf("  Storing %s\n", s.data());

  image.writeToFile(s.data());
}

} // {ContextTests}
