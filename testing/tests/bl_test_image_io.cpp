// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d.h>
#include <string.h>

#include "../commons/cmdline.h"
#include "../commons/imagediff.h"
#include "../commons/performance_timer.h"

namespace CodecTests {

static bool is_absolute_path(const char* s) {
  size_t len = strlen(s);
  return len > 0 && s[0] == '/';
}

struct CodecFeatureNameEntry {
  BLImageCodecFeatures feature;
  char name[12];
};

static constexpr CodecFeatureNameEntry codec_features_table[] = {
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
  TestKind test_kind = TestKind::kNone;
  bool quiet {};
  const char* base_dir {};
  const char* file1 {};
  const char* file2 {};
};

struct LoadedImage {
  BLResult result;
  double duration;
  BLImage image;
};

static const char* bool_to_string(bool value) {
  return value ? "true" : "false";
}

static const char* format_to_string(BLFormat format) {
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
  TestOptions default_options {};
  TestOptions options {};

  TestApp();
  ~TestApp();

  static TestOptions make_default_options();

  int help();

  void print_app_info(const char* title, bool quiet) const;
  void print_options() const;
  void print_built_in_codecs() const;

  bool parse_options(CmdLine cmd_line);

  LoadedImage load_image(const char* base_dir, const char* file_name);

  bool test_single_file(const char* base_dir, const char* file_name);
  bool compare_files(const char* base_dir, const char* fileName1, const char* fileName2);

  int run(CmdLine cmd_line);
};

TestApp::TestApp()
  : default_options(make_default_options()) {
}

TestApp::~TestApp() {}

TestOptions TestApp::make_default_options() {
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

  print_options();
  print_built_in_codecs();

  return 0;
}

bool TestApp::parse_options(CmdLine cmd_line) {
  options.base_dir = cmd_line.value_of("--base-dir", nullptr);
  options.quiet = cmd_line.has_arg("--quiet") || default_options.quiet;

  TestKind kind = TestKind::kNone;

  if (cmd_line.value_of("--file", nullptr)) {
    kind = TestKind::kSingleImage;
  }
  else if (cmd_line.has_arg("--compare")) {
    kind = TestKind::kCompareImages;
  }

  switch (kind) {
    case TestKind::kSingleImage: {
      options.file1 = cmd_line.value_of("--file", nullptr);
      break;
    }

    case TestKind::kCompareImages: {
      int index = cmd_line.find_arg("--compare");

      if (index + 3 > cmd_line.count()) {
        printf("Failed to process command line arguments: Invalid --compare <path1> <path2> (missing arguments)\n");
        return false;
      }

      options.file1 = cmd_line.args()[index + 1];
      options.file2 = cmd_line.args()[index + 2];
      break;
    }

    default:
      break;
  }

  options.test_kind = kind;
  return true;
}

void TestApp::print_app_info(const char* title, bool quiet) const {
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

void TestApp::print_options() const {
  printf("Options:\n");
  printf("  --base-dir=<string>         - Base working directory                [default=<none>]\n");
  printf("  --file=<string>             - Path to a single file to decode       [default=<none>]\n");
  printf("  --compare <string> <string> - Path to two files to decode & compare [default=<none>]\n");
  printf("  --quiet                     - Don't write log unless necessary      [default=%s]\n", bool_to_string(options.quiet));
  printf("\n");
}

void TestApp::print_built_in_codecs() const {
  BLArray<BLImageCodec> codecs = BLImageCodec::built_in_codecs();

  printf("List of image codecs:\n");

  for (const BLImageCodec& codec : codecs) {
    BLImageCodecFeatures features = codec.features();
    BLString f;

    for (const CodecFeatureNameEntry& entry : codec_features_table) {
      if ((features & entry.feature) != 0) {
        if (!f.is_empty())
          f.append("|");
        f.append(entry.name);
      }
    }

    printf("  %-4s (%-7s) - mime=%-12s files=%-22s features=%s\n",
      codec.name().data(),
      codec.vendor().data(),
      codec.mime_type().data(),
      codec.extensions().data(),
      f.data());
  }
}

LoadedImage TestApp::load_image(const char* base_dir, const char* file_name) {
  BLString full_path;

  if (base_dir && !is_absolute_path(file_name)) {
    full_path.append(base_dir);
    if (full_path.size() > 0 && full_path[full_path.size() - 1] != '/')
      full_path.append('/');
    full_path.append(file_name);
  }
  else {
    full_path.append(file_name);
  }

  BLImage img;
  PerformanceTimer timer;

  timer.start();
  BLResult result = img.read_from_file(full_path.data());
  timer.stop();

  return LoadedImage{result, timer.duration(), img};
}

bool TestApp::test_single_file(const char* base_dir, const char* file_name) {
  LoadedImage i = load_image(base_dir, file_name);

  if (i.result != BL_SUCCESS) {
    printf("[%s] Error loading image (result=0x%80u)\n", file_name, i.result);
    return false;
  }

  printf("[%s] loaded in %0.3f [ms] size=%ux%u format=%s\n", file_name, i.duration, i.image.size().w, i.image.size().h, format_to_string(i.image.format()));
  return true;
}

bool TestApp::compare_files(const char* base_dir, const char* fileName1, const char* fileName2) {
  LoadedImage i1 = load_image(base_dir, fileName1);
  LoadedImage i2 = load_image(base_dir, fileName2);

  BLImage& img1 = i1.image;
  BLImage& img2 = i2.image;

  if (i1.result != BL_SUCCESS) {
    printf("[%s] Error loading first image (result=0x%80u)\n", fileName1, i1.result);
    return false;
  }

  printf("[%s] loaded in %0.3f [ms] size=%ux%u format=%s\n", fileName1, i1.duration, img1.size().w, img1.size().h, format_to_string(img1.format()));

  if (i2.result != BL_SUCCESS) {
    printf("[%s] Error loading second image (result=0x%80u)\n", fileName2, i2.result);
    return false;
  }

  printf("[%s] loaded in %0.3f [ms] size=%ux%u format=%s\n", fileName2, i2.duration, img2.size().w, img2.size().h, format_to_string(img2.format()));

  if (img1.size() != img2.size()) {
    printf("Image sizes don't match!\n");
    return false;
  }

  ImageUtils::DiffInfo diff = ImageUtils::diff_info(img1, img2);
  if (diff.max_diff == 0xFFFFFFFFu) {
    if (img1.format() != img2.format()) {
      printf("Image formats don't match!\n");
      return false;
    }
    else {
      printf("Unknown error happened during image comparison!\n");
      return false;
    }
  }

  if (diff.cumulative_diff) {
    printf("Images don't match:\n"
           "  MaximumDifference=%llu\n"
           "  CumulativeDifference=%llu\n",
           (unsigned long long)diff.max_diff,
           (unsigned long long)diff.cumulative_diff
    );
    return false;
  }

  printf("Images match!\n");
  return true;
}

int TestApp::run(CmdLine cmd_line) {
  print_app_info("Blend2D Image Codecs Tester", cmd_line.has_arg("--quiet"));

  if (cmd_line.has_arg("--help")) {
    return help();
  }

  if (!parse_options(cmd_line)) {
    return 1;
  }

  switch (options.test_kind) {
    case TestKind::kNone: {
      return help();
    }

    case TestKind::kSingleImage: {
      if (!test_single_file(options.base_dir, options.file1))
        return 1;
      else
        return 0;
    }

    case TestKind::kCompareImages: {
      if (!compare_files(options.base_dir, options.file1, options.file2))
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
  BLRuntimeScope rt_scope;
  CodecTests::TestApp app;

  return app.run(CmdLine(argc, argv));
}
