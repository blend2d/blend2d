// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d.h>
#include <string.h>

#include "bl_test_cmdline.h"
#include "bl_test_imageutils.h"
#include "bl_test_performance_timer.h"

#define ARRAY_SIZE(X) uint32_t(sizeof(X) / sizeof(X[0]))

namespace CodecTests {

struct CodecFeatureNameEntry {
  BLImageCodecFeatures feature;
  char name[12];
};

static constexpr CodecFeatureNameEntry codecFeaturesTable[] = {
  { BL_IMAGE_CODEC_FEATURE_READ       , "read"        },
  { BL_IMAGE_CODEC_FEATURE_WRITE      , "write"       },
  { BL_IMAGE_CODEC_FEATURE_LOSSLESS   , "lossless"    },
  { BL_IMAGE_CODEC_FEATURE_LOSSY      , "lossy"       },
  { BL_IMAGE_CODEC_FEATURE_MULTI_FRAME, "multi-frame" },
  { BL_IMAGE_CODEC_FEATURE_IPTC       , "iptc"        },
  { BL_IMAGE_CODEC_FEATURE_EXIF       , "exif"        },
  { BL_IMAGE_CODEC_FEATURE_XMP        , "xmp"         }
};

struct TestOptions {
  bool quiet {};
  const char* file {};
};

static const char* boolToString(bool value) {
  return value ? "true" : "false";
}

static const char* formatToString(BLFormat format) {
  switch (format) {
    case BL_FORMAT_PRGB32:
      return "prgb32";
    case BL_FORMAT_XRGB32:
      return "xrgb32";
    case BL_FORMAT_A8:
      return "a8";
    default:
      return "unknown";
  }
}

class TestApp {
public:
  TestOptions defaultOptions {};
  TestOptions options {};

  TestApp();
  ~TestApp();

  static TestOptions makeDefaultOptions();

  int help();

  void printAppInfo(const char* title, bool quiet) const;
  void printOptions(const TestOptions& options) const;
  void printBuiltInCodecs() const;

  bool parseOptions(CmdLine cmdLine);

  bool testFile(const char* base_path, const char* fileName);

  int run(CmdLine cmdLine);
};

TestApp::TestApp()
  : defaultOptions(makeDefaultOptions()) {
}

TestApp::~TestApp() {}

TestOptions TestApp::makeDefaultOptions() {
  TestOptions options {};
  return options;
}

int TestApp::help() {
  printf("Usage:\n");
  printf("  bl_test_image_io [options] [--help for help]\n");
  printf("\n");

  printf("Purpose:\n");
  printf("  Verify that image codecs can decode and encode images properly.\n");
  printf("\n");

  printOptions(options);
  printBuiltInCodecs();

  return 0;
}

bool TestApp::parseOptions(CmdLine cmdLine) {
  options.file = cmdLine.valueOf("--file", nullptr);
  options.quiet = cmdLine.hasArg("--quiet") || defaultOptions.quiet;

  if (options.file == nullptr || strcmp(options.file, "") == 0) {
    printf("Failed to process command line arguments:\n");
    printf("  A file to decode must be specified, use --file=<path> to specify it\n");
    return false;
  }

  return true;
}

void TestApp::printAppInfo(const char* title, bool quiet) const {
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

void TestApp::printOptions(const TestOptions& options) const {
  printf("Options:\n");
  printf("  --file                  - Path to a single file to decode   [default=%s]\n", defaultOptions.file ? defaultOptions.file : "<none>");
  printf("  --quiet                 - Don't write log unless necessary  [default=%s]\n", boolToString(defaultOptions.quiet));
  printf("\n");
}

void TestApp::printBuiltInCodecs() const {
  BLArray<BLImageCodec> codecs = BLImageCodec::builtInCodecs();

  printf("List of image codecs:\n");

  for (const BLImageCodec& codec : codecs) {
    BLImageCodecFeatures features = codec.features();
    BLString f;

    for (uint32_t i = 0; i < ARRAY_SIZE(codecFeaturesTable); i++) {
      if ((features & codecFeaturesTable[i].feature) != 0) {
        if (!f.empty())
          f.append("|");
        f.append(codecFeaturesTable[i].name);
      }
    }

    printf("  %-4s (%-7s) - mime=%-12s files=%-22s features=%s\n",
      codec.name().data(),
      codec.vendor().data(),
      codec.mimeType().data(),
      codec.extensions().data(),
      f.data());
  }
}

bool TestApp::testFile(const char* basePath, const char* fileName) {
  BLString fullPath;

  if (basePath) {
    fullPath.append(basePath);
    if (fullPath.size() > 0 && fullPath[fullPath.size() - 1] != '/')
      fullPath.append('/');
    fullPath.append(fileName);
  }
  else {
    fullPath.append(fileName);
  }

  BLImage img;
  PerformanceTimer timer;

  timer.start();
  BLResult result = img.readFromFile(fullPath.data());
  timer.stop();

  if (result != BL_SUCCESS) {
    printf("[%s] Error loading image (result=0x%80u)\n", fileName, result);
    return false;
  }

  printf("[%s] loaded in %0.3f [ms] size=%ux%u format=%s\n", fileName, timer.duration(), img.size().w, img.size().h, formatToString(img.format()));

  return true;
}

int TestApp::run(CmdLine cmdLine) {
  printAppInfo("Blend2D Image Codecs Tester", cmdLine.hasArg("--quiet"));

  if (cmdLine.hasArg("--help")) {
    return help();
  }

  if (!parseOptions(cmdLine)) {
    return 1;
  }

  if (options.file) {
    if (!testFile(nullptr, options.file))
      return 1;
    else
      return 0;
  }

  return 0;
}

} // {CodecTests}

int main(int argc, char* argv[]) {
  BLRuntimeScope rtScope;
  CodecTests::TestApp app;

  return app.run(CmdLine(argc, argv));
}
