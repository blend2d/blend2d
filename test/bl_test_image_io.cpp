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

static bool isAbsolutePath(const char* s) {
  size_t len = strlen(s);
  return len > 0 && s[0] == '/';
}

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

enum class TestKind : uint8_t {
  kNone,
  kSingleImage,
  kCompareImages
};

struct TestOptions {
  TestKind testKind = TestKind::kNone;
  bool quiet {};
  const char* baseDir {};
  const char* file1 {};
  const char* file2 {};
};

struct LoadedImage {
  BLResult result;
  double duration;
  BLImage image;
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

  LoadedImage loadImage(const char* baseDir, const char* fileName);

  bool testSingleFile(const char* baseDir, const char* fileName);
  bool compareFiles(const char* baseDir, const char* fileName1, const char* fileName2);

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
  printf("  bl_test_image_io [options] --<file|compare> [--help for help]\n");
  printf("\n");

  printf("Purpose:\n");
  printf("  Verify that image codecs can decode and encode images properly.\n");
  printf("\n");

  printOptions(options);
  printBuiltInCodecs();

  return 0;
}

bool TestApp::parseOptions(CmdLine cmdLine) {
  options.baseDir = cmdLine.valueOf("--base-dir", nullptr);
  options.quiet = cmdLine.hasArg("--quiet") || defaultOptions.quiet;

  TestKind kind = TestKind::kNone;

  if (cmdLine.valueOf("--file", nullptr)) {
    kind = TestKind::kSingleImage;
  }
  else if (cmdLine.hasArg("--compare")) {
    kind = TestKind::kCompareImages;
  }

  switch (kind) {
    case TestKind::kSingleImage: {
      options.file1 = cmdLine.valueOf("--file", nullptr);
      break;
    }

    case TestKind::kCompareImages: {
      int index = cmdLine.findArg("--compare");

      if (index + 3 > cmdLine.count()) {
        printf("Failed to process command line arguments: Invalid --compare <path1> <path2> (missing arguments)\n");
        return false;
      }

      options.file1 = cmdLine.args()[index + 1];
      options.file2 = cmdLine.args()[index + 2];
      break;
    }

    default:
      break;
  }

  options.testKind = kind;
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
  printf("  --base-dir=<string>         - Base working directory                [default=<none>]\n");
  printf("  --file=<string>             - Path to a single file to decode       [default=<none>]\n");
  printf("  --compare <string> <string> - Path to two files to decode & compare [default=<none>]\n");
  printf("  --quiet                     - Don't write log unless necessary      [default=%s]\n", boolToString(defaultOptions.quiet));
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

LoadedImage TestApp::loadImage(const char* baseDir, const char* fileName) {
  BLString fullPath;

  if (baseDir && !isAbsolutePath(fileName)) {
    fullPath.append(baseDir);
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

  return LoadedImage{result, timer.duration(), img};
}

bool TestApp::testSingleFile(const char* baseDir, const char* fileName) {
  LoadedImage i = loadImage(baseDir, fileName);

  if (i.result != BL_SUCCESS) {
    printf("[%s] Error loading image (result=0x%80u)\n", fileName, i.result);
    return false;
  }

  printf("[%s] loaded in %0.3f [ms] size=%ux%u format=%s\n", fileName, i.duration, i.image.size().w, i.image.size().h, formatToString(i.image.format()));
  return true;
}

bool TestApp::compareFiles(const char* baseDir, const char* fileName1, const char* fileName2) {
  LoadedImage i1 = loadImage(baseDir, fileName1);
  LoadedImage i2 = loadImage(baseDir, fileName2);

  BLImage& img1 = i1.image;
  BLImage& img2 = i2.image;

  if (i1.result != BL_SUCCESS) {
    printf("[%s] Error loading first image (result=0x%80u)\n", fileName1, i1.result);
    return false;
  }

  printf("[%s] loaded in %0.3f [ms] size=%ux%u format=%s\n", fileName1, i1.duration, img1.size().w, img1.size().h, formatToString(img1.format()));

  if (i2.result != BL_SUCCESS) {
    printf("[%s] Error loading second image (result=0x%80u)\n", fileName2, i2.result);
    return false;
  }

  printf("[%s] loaded in %0.3f [ms] size=%ux%u format=%s\n", fileName2, i2.duration, img2.size().w, img2.size().h, formatToString(img2.format()));

  if (img1.size() != img2.size()) {
    printf("Image sizes don't match!\n");
    return false;
  }

  ImageUtils::DiffInfo diff = ImageUtils::diffInfo(img1, img2);
  if (diff.maxDiff == 0xFFFFFFFFu) {
    if (img1.format() != img2.format()) {
      printf("Image formats don't match!\n");
      return false;
    }
    else {
      printf("Unknown error happened during image comparison!\n");
      return false;
    }
  }

  if (diff.cumulativeDiff) {
    printf("Images don't match:\n"
           "  MaximumDifference=%llu\n"
           "  CumulativeDifference=%llu\n",
           (unsigned long long)diff.maxDiff,
           (unsigned long long)diff.cumulativeDiff
    );
    return false;
  }

  printf("Images match!\n");
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

  switch (options.testKind) {
    case TestKind::kNone: {
      return help();
    }

    case TestKind::kSingleImage: {
      if (!testSingleFile(options.baseDir, options.file1))
        return 1;
      else
        return 0;
    }

    case TestKind::kCompareImages: {
      if (!compareFiles(options.baseDir, options.file1, options.file2))
        return 1;
      else
        return 0;
    }

    default:
      return 1;
  }
}

} // {CodecTests}

int main(int argc, char* argv[]) {
  BLRuntimeScope rtScope;
  CodecTests::TestApp app;

  return app.run(CmdLine(argc, argv));
}
