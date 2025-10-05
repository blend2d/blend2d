// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <stdio.h>
#include <string.h>

#include <cmath>
#include <limits>
#include <type_traits>
#include <tuple>

#include "bl_bench_app.h"
#include "bl_bench_backend.h"

#include "images_data.h"

#define ARRAY_SIZE(X) uint32_t(sizeof(X) / sizeof(X[0]))

namespace blbench {

static constexpr uint32_t supported_backends_mask =
#if defined(BL_BENCH_ENABLE_AGG)
  (1u << uint32_t(BackendKind::kAGG)) |
#endif
#if defined(BL_BENCH_ENABLE_CAIRO)
  (1u << uint32_t(BackendKind::kCairo)) |
#endif
#if defined(BL_BENCH_ENABLE_QT)
  (1u << uint32_t(BackendKind::kQt)) |
#endif
#if defined(BL_BENCH_ENABLE_SKIA)
  (1u << uint32_t(BackendKind::kSkia)) |
#endif
#if defined(BL_BENCH_ENABLE_JUCE)
  (1u << uint32_t(BackendKind::kJUCE)) |
#endif
#if defined(BL_BENCH_ENABLE_COREGRAPHICS)
  (1u << uint32_t(BackendKind::kCoreGraphics)) |
#endif
  (1u << uint32_t(BackendKind::kBlend2D));

static const char* backend_kind_name_table[] = {
  "Blend2D",
  "AGG",
  "Cairo",
  "Qt",
  "Skia",
  "JUCE",
  "CoreGraphics"
};

static const char* test_kind_name_table[] = {
  "FillRectA",
  "FillRectU",
  "FillRectRot",
  "FillRoundU",
  "FillRoundRot",
  "FillTriangle",
  "FillPolyNZi10",
  "FillPolyEOi10",
  "FillPolyNZi20",
  "FillPolyEOi20",
  "FillPolyNZi40",
  "FillPolyEOi40",
  "FillButterfly",
  "FillFish",
  "FillDragon",
  "FillWorld",
  "StrokeRectA",
  "StrokeRectU",
  "StrokeRectRot",
  "StrokeRoundU",
  "StrokeRoundRot",
  "StrokeTriangle",
  "StrokePoly10",
  "StrokePoly20",
  "StrokePoly40",
  "StrokeButterfly",
  "StrokeFish",
  "StrokeDragon",
  "StrokeWorld"
};

static const char* comp_op_name_table[] = {
  "SrcOver",
  "SrcCopy",
  "SrcIn",
  "SrcOut",
  "SrcAtop",
  "DstOver",
  "DstCopy",
  "DstIn",
  "DstOut",
  "DstAtop",
  "Xor",
  "Clear",
  "Plus",
  "Minus",
  "Modulate",
  "Multiply",
  "Screen",
  "Overlay",
  "Darken",
  "Lighten",
  "ColorDodge",
  "ColorBurn",
  "LinearBurn",
  "LinearLight",
  "PinLight",
  "HardLight",
  "SoftLight",
  "Difference",
  "Exclusion"
};

static const char* style_kind_name_table[] = {
  "Solid",
  "Linear@Pad",
  "Linear@Repeat",
  "Linear@Reflect",
  "Radial@Pad",
  "Radial@Repeat",
  "Radial@Reflect",
  "Conic",
  "Pattern_NN",
  "Pattern_BI"
};

static const uint32_t bench_shape_size_table[kBenchShapeSizeCount] = {
  8, 16, 32, 64, 128, 256
};

const char bench_border_str[] = "+--------------------+-------------+---------------+----------+----------+----------+----------+----------+----------+\n";
const char bench_header_str[] = "|%-20s"             "| CompOp      | Style         | 8x8      | 16x16    | 32x32    | 64x64    | 128x128  | 256x256  |\n";
const char bench_fata_fmt_str[]   = "|%-20s"             "| %-12s"     "| %-14s"       "| %-9s"   "| %-9s"   "| %-9s"   "| %-9s"   "| %-9s"   "| %-9s"   "|\n";

static const char* get_os_string() {
#if defined(__ANDROID__)
  return "android";
#elif defined(__linux__)
  return "linux";
#elif defined(__APPLE__) && defined(__MACH__)
  return "osx";
#elif defined(__APPLE__)
  return "apple";
#elif defined(__DragonFly__)
  return "dragonflybsd";
#elif defined(__FreeBSD__)
  return "freebsd";
#elif defined(__NetBSD__)
  return "netbsd";
#elif defined(__OpenBSD__)
  return "openbsd";
#elif defined(__HAIKU__)
  return "haiku";
#elif defined(_WIN32)
  return "windows";
#else
  return "unknown";
#endif
}

static const char* get_cpu_arch_string() {
#if defined(_M_X64) || defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__)
  return "x86_64";
#elif defined(_M_IX86) || defined(__i386) || defined(__i386__)
  return "x86";
#elif defined(_M_ARM64) || defined(__ARM64__) || defined(__aarch64__)
  return "aarch64";
#elif defined(_M_ARM) || defined(_M_ARMT) || defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
  return "aarch32";
#elif defined(_MIPS_ARCH_MIPS64) || defined(__mips64)
  return "mips64";
#elif defined(_MIPS_ARCH_MIPS32) || defined(_M_MRX000) || defined(__mips) || defined(__mips__)
  return "mips32";
#elif defined(__riscv) || defined(__riscv__)
  return sizeof(void*) >= 8 ? "riscv64" : "riscv32";
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
  return "ppc64";
#elif defined(_LOONGARCH_ARCH) || defined(__loongarch_arch) || defined(__loongarch64)
  return "la64";
#else
  return "unknown";
#endif
}

static const char* get_format_string(BLFormat format) {
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

static bool strieq(const char* a, const char* b) {
  size_t aLen = strlen(a);
  size_t bLen = strlen(b);

  if (aLen != bLen)
    return false;

  for (size_t i = 0; i < aLen; i++) {
    unsigned ac = (unsigned char)a[i];
    unsigned bc = (unsigned char)b[i];

    if (ac >= 'A' && ac <= 'Z') ac += uint32_t('a' - 'A');
    if (bc >= 'A' && bc <= 'Z') bc += uint32_t('a' - 'A');

    if (ac != bc)
      return false;
  }

  return true;
}

static uint32_t search_string_list(const char** list_data, size_t list_size, const char* key) {
  for (size_t i = 0; i < list_size; i++)
    if (strieq(list_data[i], key))
      return uint32_t(i);
  return 0xFFFFFFFFu;
}

static void spacesToUnderscores(char* s) {
  while (*s) {
    if (*s == ' ')
      *s = '_';
    s++;
  }
}

static BLArray<BLString> split_string(const char* s) {
  BLArray<BLString> arr;
  while (*s) {
    const char* end = strchr(s, ',');
    if (!end) {
      arr.append(BLString(s));
      break;
    }
    else {
      BLString part(BLStringView{s, (size_t)(end - s)});
      arr.append(part);
      s = end + 1;
    }
  }
  return arr;
}

static std::tuple<int, uint32_t> parse_list(const char** list_data, size_t list_size, const char* input_list, const char* parse_error_msg) {
  int listOp = 0;
  uint32_t parsedMask = 0;
  BLArray<BLString> parts = split_string(input_list);

  for (const BLString& part : parts) {
    if (part.is_empty())
      continue;

    const char* p = part.data();
    int partOp = 0;

    if (p[0] == '-') {
      p++;
      partOp = -1;
    }
    else {
      partOp = 1;
    }

    if (listOp == 0) {
      listOp = partOp;
    }
    else if (listOp != partOp) {
      printf("ERROR: %s [%s]: specify either additive or subtractive list, but not both\n", parse_error_msg, input_list);
      return std::tuple<int, uint32_t>(-2, 0);
    }

    uint32_t backend_index = search_string_list(list_data, list_size, p);
    if (backend_index == 0xFFFFFFFFu) {
      printf("ERROR: %s [%s]: couldn't recognize '%s' part\n", parse_error_msg, input_list, p);
      return std::tuple<int, uint32_t>(-2, 0);
    }

    parsedMask |= 1u << backend_index;
  }

  return std::tuple<int, uint32_t>(listOp, parsedMask);
}

struct DurationFormat {
  char data[64];

  inline void format(double cpms) {
    if (cpms <= 0.1) {
      snprintf(data, 64, "%0.4f", cpms);
    }
    else if (cpms <= 1.0) {
      snprintf(data, 64, "%0.3f", cpms);
    }
    else if (cpms < 10.0) {
      snprintf(data, 64, "%0.2f", cpms);
    }
    else if (cpms < 100.0) {
      snprintf(data, 64, "%0.1f", cpms);
    }
    else {
      snprintf(data, 64, "%llu", (unsigned long long)std::round(cpms));
    }
  }
};

BenchApp::BenchApp(int argc, char** argv)
  : _cmd_line(argc, argv),
    _backends(supported_backends_mask) {}
BenchApp::~BenchApp() {}

void BenchApp::print_app_info() const {
  BLRuntimeBuildInfo build_info;
  BLRuntime::query_build_info(&build_info);

  printf(
    "Blend2D Benchmarking Tool\n"
    "\n"
    "Blend2D Information:\n"
    "  Version    : %u.%u.%u\n"
    "  Build Type : %s\n"
    "  Compiled By: %s\n"
    "\n",
    build_info.major_version,
    build_info.minor_version,
    build_info.patch_version,
    build_info.build_type == BL_RUNTIME_BUILD_TYPE_DEBUG ? "Debug" : "Release",
    build_info.compiler_info);

  fflush(stdout);
}

void BenchApp::print_options() const {
  const char no_yes[][4] = { "no", "yes" };

  printf(
    "The following options are supported / used:\n"
    "  --width=N         [%u] Canvas width to use for rendering\n"
    "  --height=N        [%u] Canvas height to use for rendering\n"
    "  --quantity=N      [%d] Render calls per test (0 = adjust depending on test duration)\n"
    "  --size-count=N    [%u] Number of size iterations (1=8x8 -> 6=8x8..256x256)\n"
    "  --comp-op=<list>  [%s] Benchmark a specific composition operator\n"
    "  --repeat=N        [%d] Number of repeats of each test to select the best time\n"
    "  --backends=<list> [%s] Backends to use (use 'a,b' to select few, '-xxx' to disable)\n"
    "  --save-images     [%s] Save each generated image independently (use with --quantity)\n"
    "  --save-overview   [%s] Save generated images grouped by sizes  (use with --quantity)\n"
    "  --deep            [%s] More tests that use gradients and textures\n"
    "  --isolated        [%s] Use Blend2D isolated context (useful for development only)\n"
    "\n",
    _width,
    _height,
    _quantity,
    _size_count,
    _comp_op == 0xFFFFFFFF ? "all" : comp_op_name_table[_comp_op],
    _repeat,
    _backends == supported_backends_mask ? "all" : "...",
    no_yes[_save_images],
    no_yes[_save_overview],
    no_yes[_deep_bench],
    no_yes[_isolated]
  );

  fflush(stdout);
}

void BenchApp::print_backends() const {
  printf("Backends supported (by default all supported backends are enabled by tests unless overridden):\n");
  const char disabled_enabled[][16] = { "disabled", "enabled" };

  for (uint32_t backend_index = 0; backend_index < kBackendKindCount; backend_index++) {
    uint32_t backend_mask = 1u << backend_index;

    if (backend_mask & supported_backends_mask) {
      printf("  - %-15s [%s]\n",
        backend_kind_name_table[backend_index],
        disabled_enabled[(_backends & backend_mask) != 0]);
    }
    else {
      printf("  - %-15s [unsupported]\n",
        backend_kind_name_table[backend_index]);
    }
  }

  printf("\n");
  fflush(stdout);
}

bool BenchApp::parse_command_line() {
  _width = _cmd_line.value_as_uint("--width", _width);
  _height = _cmd_line.value_as_uint("--height", _height);
  _comp_op = 0xFFFFFFFFu;
  _size_count = _cmd_line.value_as_uint("--size-count", _size_count);
  _quantity = _cmd_line.value_as_uint("--quantity", _quantity);
  _repeat = _cmd_line.value_as_uint("--repeat", _repeat);

  _save_images = _cmd_line.has_arg("--save-images");
  _save_overview = _cmd_line.has_arg("--save-overview");
  _deep_bench = _cmd_line.has_arg("--deep");
  _isolated = _cmd_line.has_arg("--isolated");

  const char* comp_op_string = _cmd_line.value_of("--comp_op", nullptr);
  const char* backend_string = _cmd_line.value_of("--backend", nullptr);

  if (_width < 10|| _width > 4096) {
    printf("ERROR: Invalid --width=%u specified\n", _width);
    return false;
  }

  if (_height < 10|| _height > 4096) {
    printf("ERROR: Invalid --height=%u specified\n", _height);
    return false;
  }

  if (_size_count == 0 || _size_count > kBenchShapeSizeCount) {
    printf("ERROR: Invalid --size-count=%u specified\n", _size_count);
    return false;
  }

  if (_quantity > 100000u) {
    printf("ERROR: Invalid --quantity=%u specified\n", _quantity);
    return false;
  }

  if (_repeat <= 0 || _repeat > 100) {
    printf("ERROR: Invalid --repeat=%u specified\n", _repeat);
    return false;
  }

  if (_save_images && !_quantity) {
    printf("ERROR: Missing --quantity argument; it must be provided when --save-images is used\n");
    return false;
  }

  if (_save_overview && !_quantity) {
    printf("ERROR: Missing --quantity argument; it must be provided when --save-overview is used\n");
    return false;
  }

  if (comp_op_string && strcmp(comp_op_string, "all") != 0) {
    _comp_op = search_string_list(comp_op_name_table, ARRAY_SIZE(comp_op_name_table), comp_op_string);
    if (_comp_op == 0xFFFFFFFF) {
      printf("ERROR: Invalid composition operator [%s] specified\n", comp_op_string);
      return false;
    }
  }

  if (backend_string && strcmp(backend_string, "all") != 0) {
    std::tuple<int, uint32_t> v = parse_list(backend_kind_name_table, kBackendKindCount, backend_string, "Invalid --backend list");

    if (std::get<0>(v) == -2) {
      return false;
    }

    if (std::get<0>(v) == 1) {
      _backends = std::get<1>(v);
    }
    else if (std::get<0>(v) == -1) {
      _backends &= ~std::get<1>(v);
    }
  }

  return true;
}

bool BenchApp::init() {
  if (_cmd_line.has_arg("--help")) {
    info();
    exit(0);
  }

  if (!parse_command_line()) {
    info();
    exit(1);
  }

  return read_image(_sprite_data[0], "#0", _resource_babelfish_png, sizeof(_resource_babelfish_png)) &&
         read_image(_sprite_data[1], "#1", _resource_ksplash_png  , sizeof(_resource_ksplash_png  )) &&
         read_image(_sprite_data[2], "#2", _resource_ktip_png     , sizeof(_resource_ktip_png     )) &&
         read_image(_sprite_data[3], "#3", _resource_firewall_png , sizeof(_resource_firewall_png ));
}

void BenchApp::info() {
  BLRuntimeBuildInfo build_info;
  BLRuntime::query_build_info(&build_info);

  print_app_info();
  print_options();
  print_backends();
}

bool BenchApp::read_image(BLImage& image, const char* name, const void* data, size_t size) noexcept {
  BLResult result = image.read_from_data(data, size);
  if (result != BL_SUCCESS) {
    printf("Failed to read an image '%s' used for benchmarking\n", name);
    return false;
  }
  else {
    return true;
  }
}

BLImage BenchApp::get_scaled_sprite(uint32_t id, uint32_t size) const {
  auto it = _scaled_sprites.find(size);
  if (it != _scaled_sprites.end()) {
    return it->second[id];
  }

  SpriteData scaled;
  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    BLImage::scale(
      scaled[i],
      _sprite_data[i],
      BLSizeI(int(size), int(size)), BL_IMAGE_SCALE_FILTER_BILINEAR);
  }
  _scaled_sprites.emplace(size, scaled);
  return scaled[id];
}

bool BenchApp::is_backend_enabled(BackendKind backend_kind) const {
  return (_backends & (1u << uint32_t(backend_kind))) != 0;
}

bool BenchApp::is_style_enabled(StyleKind style) const {
  if (_deep_bench)
    return true;

  // If this is not a deep run we just select the following styles to be tested:
  return style == StyleKind::kSolid     ||
         style == StyleKind::kLinearPad ||
         style == StyleKind::kRadialPad ||
         style == StyleKind::kConic     ||
         style == StyleKind::kPatternNN ||
         style == StyleKind::kPatternBI ;
}

void BenchApp::serialize_system_info(JSONBuilder& json) const {
  BLRuntimeSystemInfo system_info;
  BLRuntime::query_system_info(&system_info);

  json.before_record().add_key("environment").open_object();
  json.before_record().add_key("os").add_string(get_os_string());
  json.close_object(true);

  json.before_record().add_key("cpu").open_object();
  json.before_record().add_key("arch").add_string(get_cpu_arch_string());
  json.before_record().add_key("vendor").add_string(system_info.cpu_vendor);
  json.before_record().add_key("brand").add_string(system_info.cpu_brand);
  json.close_object(true);
}

void BenchApp::serialize_params(JSONBuilder& json, const BenchParams& params) const {
  json.before_record().add_key("screen").open_object();
  json.before_record().add_key("width").add_uint(params.screen_w);
  json.before_record().add_key("height").add_uint(params.screen_h);
  json.before_record().add_key("format").add_string(get_format_string(params.format));
  json.close_object(true);
}

void BenchApp::serialize_options(JSONBuilder& json, const BenchParams& params) const {
  json.before_record().add_key("options").open_object();
  json.before_record().add_key("quantity").add_uint(params.quantity);
  json.before_record().add_key("sizes").open_array();
  for (uint32_t size_index = 0; size_index < _size_count; size_index++) {
    json.add_stringf("%ux%u", bench_shape_size_table[size_index], bench_shape_size_table[size_index]);
  }
  json.close_array();
  json.before_record().add_key("repeat").add_uint(_repeat);
  json.close_object(true);
}

int BenchApp::run() {
  BenchParams params{};
  params.screen_w = _width;
  params.screen_h = _height;
  params.format = BL_FORMAT_PRGB32;
  params.stroke_width = 2.0;

  BLString json_content;
  JSONBuilder json(&json_content);

  json.open_object();

  serialize_system_info(json);
  serialize_params(json, params);
  serialize_options(json, params);

  json.before_record().add_key("runs").open_array();

  if (_isolated) {
    BLRuntimeSystemInfo si;
    BLRuntime::query_system_info(&si);

    // Only use features that could actually make a difference.
    static const uint32_t x86_features[] = {
      BL_RUNTIME_CPU_FEATURE_X86_SSE2,
      BL_RUNTIME_CPU_FEATURE_X86_SSE3,
      BL_RUNTIME_CPU_FEATURE_X86_SSSE3,
      BL_RUNTIME_CPU_FEATURE_X86_SSE4_1,
      BL_RUNTIME_CPU_FEATURE_X86_SSE4_2,
      BL_RUNTIME_CPU_FEATURE_X86_AVX,
      BL_RUNTIME_CPU_FEATURE_X86_AVX2,
      BL_RUNTIME_CPU_FEATURE_X86_AVX512
    };

    const uint32_t* features = x86_features;
    uint32_t feature_count = ARRAY_SIZE(x86_features);

    for (uint32_t i = 0; i < feature_count; i++) {
      if ((si.cpu_features & features[i]) == features[i]) {
        Backend* backend = create_blend2d_backend(0, features[i]);
        run_backend_tests(*backend, params, json);
        delete backend;
      }
    }
  }
  else {
    if (is_backend_enabled(BackendKind::kBlend2D)) {
      Backend* backend = create_blend2d_backend(0);
      run_backend_tests(*backend, params, json);
      delete backend;

      backend = create_blend2d_backend(2);
      run_backend_tests(*backend, params, json);
      delete backend;

      backend = create_blend2d_backend(4);
      run_backend_tests(*backend, params, json);
      delete backend;
    }

#if defined(BL_BENCH_ENABLE_AGG)
    if (is_backend_enabled(BackendKind::kAGG)) {
      Backend* backend = create_agg_backend();
      run_backend_tests(*backend, params, json);
      delete backend;
    }
#endif

#if defined(BL_BENCH_ENABLE_CAIRO)
    if (is_backend_enabled(BackendKind::kCairo)) {
      Backend* backend = create_cairo_backend();
      run_backend_tests(*backend, params, json);
      delete backend;
    }
#endif

#if defined(BL_BENCH_ENABLE_QT)
    if (is_backend_enabled(BackendKind::kQt)) {
      Backend* backend = create_qt_backend();
      run_backend_tests(*backend, params, json);
      delete backend;
    }
#endif

#if defined(BL_BENCH_ENABLE_SKIA)
    if (is_backend_enabled(BackendKind::kSkia)) {
      Backend* backend = create_skia_backend();
      run_backend_tests(*backend, params, json);
      delete backend;
    }
#endif

#if defined(BL_BENCH_ENABLE_JUCE)
    if (is_backend_enabled(BackendKind::kJUCE)) {
      Backend* backend = create_juce_backend();
      run_backend_tests(*backend, params, json);
      delete backend;
    }
#endif

#if defined(BL_BENCH_ENABLE_COREGRAPHICS)
    if (is_backend_enabled(BackendKind::kCoreGraphics)) {
      Backend* backend = create_cg_backend();
      run_backend_tests(*backend, params, json);
      delete backend;
    }
#endif
  }

  json.close_array(true);
  json.close_object(true);
  json.nl();

  printf("\n");
  fputs(json_content.data(), stdout);

  return 0;
}

int BenchApp::run_backend_tests(Backend& backend, BenchParams& params, JSONBuilder& json) {
  char file_name[256];
  BLString style_string;

  BLImage overview_image;
  BLContext overview_ctx;

  if (_save_overview) {
    overview_image.create(int(1u + (_width + 1u) * _size_count), int(_height + 2u), BL_FORMAT_XRGB32);
    overview_ctx.begin(overview_image);
  }

  double cpms[kBenchShapeSizeCount] {};
  double cpms_total[kBenchShapeSizeCount] {};
  DurationFormat fmt[kBenchShapeSizeCount] {};

  uint32_t comp_op_first = BL_COMP_OP_SRC_OVER;
  uint32_t comp_op_last  = BL_COMP_OP_SRC_COPY;

  if (_comp_op != 0xFFFFFFFFu) {
    comp_op_first = comp_op_last = _comp_op;
  }

  json.before_record().open_object();
  json.before_record().add_key("name").add_string(backend.name());
  backend.serialize_info(json);
  json.before_record().add_key("records").open_array();

  for (uint32_t comp_op = comp_op_first; comp_op <= comp_op_last; comp_op++) {
    params.comp_op = BLCompOp(comp_op);
    if (!backend.supports_comp_op(params.comp_op))
      continue;

    for (uint32_t style_index = 0; style_index < kStyleKindCount; style_index++) {
      StyleKind style = StyleKind(style_index);
      if (!is_style_enabled(style) || !backend.supports_style(style)) {
        continue;
      }

      params.style = style;

      // Remove '@' from the style name if not running a deep benchmark.
      style_string.assign(style_kind_name_table[style_index]);

      if (!_deep_bench) {
        size_t idx = style_string.index_of('@');
        if (idx != SIZE_MAX) {
          style_string.truncate(idx);
        }
      }

      memset(cpms_total, 0, sizeof(cpms_total));

      printf(bench_border_str);
      printf(bench_header_str, backend._name);
      printf(bench_border_str);

      for (uint32_t test_index = 0; test_index < kTestKindCount; test_index++) {
        params.testKind = TestKind(test_index);

        if (_save_overview) {
          overview_ctx.fill_all(BLRgba32(0xFF000000u));
          overview_ctx.stroke_rect(BLRect(0.5, 0.5, overview_image.width() - 1, overview_image.height() - 1), BLRgba32(0xFFFFFFFF));
        }

        for (uint32_t size_index = 0; size_index < _size_count; size_index++) {
          params.shape_size = bench_shape_size_table[size_index];
          uint64_t duration = run_single_test(backend, params);

          cpms[size_index] = double(params.quantity) * double(1000) / double(duration);
          cpms_total[size_index] += cpms[size_index];

          if (_save_overview) {
            overview_ctx.blit_image(
              BLPointI(int(1u + (size_index * (_width + 1u))), 1),
              backend._surface);

            overview_ctx.fill_rect(
              BLRectI(int(1u + (size_index * (_width + 1u)) + _width), 1, 1, int(_height)),
              BLRgba32(0xFFFFFFFF));

            if (size_index == _size_count - 1) {
              snprintf(file_name, 256, "%s-%s-%s-%s.png",
                backend._name,
                test_kind_name_table[uint32_t(params.testKind)],
                comp_op_name_table[uint32_t(params.comp_op)],
                style_string.data());
              spacesToUnderscores(file_name);
              overview_image.write_to_file(file_name);
            }
          }

          if (_save_images) {
            // Save only the last two as these are easier to compare visually.
            if (size_index >= _size_count - 2) {
              snprintf(file_name, 256, "%s-%s-%s-%s-%c.png",
                backend._name,
                test_kind_name_table[uint32_t(params.testKind)],
                comp_op_name_table[uint32_t(params.comp_op)],
                style_string.data(),
                'A' + size_index);
              spacesToUnderscores(file_name);
              backend._surface.write_to_file(file_name);
            }
          }
        }

        for (uint32_t size_index = 0; size_index < _size_count; size_index++) {
          fmt[size_index].format(cpms[size_index]);
        }

        printf(bench_fata_fmt_str,
          test_kind_name_table[uint32_t(params.testKind)],
          comp_op_name_table[uint32_t(params.comp_op)],
          style_string.data(),
          fmt[0].data,
          fmt[1].data,
          fmt[2].data,
          fmt[3].data,
          fmt[4].data,
          fmt[5].data);

        json.before_record()
            .open_object()
            .add_key("test").add_string(test_kind_name_table[uint32_t(params.testKind)])
            .comma().align_to(36).add_key("compOp").add_string(comp_op_name_table[uint32_t(params.comp_op)])
            .comma().align_to(58).add_key("style").add_string(style_string.data());

        json.add_key("rcpms").open_array();
        for (uint32_t size_index = 0; size_index < _size_count; size_index++) {
          json.add_string_no_quotes(fmt[size_index].data);
        }
        json.close_array();

        json.close_object();
      }

      for (uint32_t size_index = 0; size_index < _size_count; size_index++) {
        fmt[size_index].format(cpms_total[size_index]);
      }

      printf(bench_border_str);
      printf(bench_fata_fmt_str,
        "Total",
        comp_op_name_table[uint32_t(params.comp_op)],
        style_string.data(),
        fmt[0].data,
        fmt[1].data,
        fmt[2].data,
        fmt[3].data,
        fmt[4].data,
        fmt[5].data);
      printf(bench_border_str);
      printf("\n");
    }
  }

  json.close_array(true);
  json.close_object(true);

  return 0;
}

uint64_t BenchApp::run_single_test(Backend& backend, BenchParams& params) {
  constexpr uint32_t initial_quantity = 25;
  constexpr uint32_t minimum_duration_in_us = 1000;
  constexpr uint32_t max_repeat_if_no_improvement = 10;

  uint32_t attempt = 0;
  uint64_t duration = std::numeric_limits<uint64_t>::max();
  uint32_t no_improvement = 0;

  params.quantity = _quantity;

  if (_quantity == 0u) {
    // If quantity is zero it means to deduce it based on execution time of each test.
    params.quantity = initial_quantity;
    for (;;) {
      backend.run(*this, params);
      if (backend._duration >= minimum_duration_in_us) {
        // Make this the first attempt to reduce the time of benchmarking.
        attempt = 1;
        duration = backend._duration;
        break;
      }

      if (backend._duration < 100) {
        params.quantity *= 10;
      }
      else if (backend._duration < 500) {
        params.quantity *= 3;
      }
      else {
        params.quantity *= 2;
      }
    }
  }

  while (attempt < _repeat) {
    backend.run(*this, params);

    if (duration > backend._duration) {
      duration = backend._duration;
    }
    else {
      no_improvement++;
    }

    if (no_improvement >= max_repeat_if_no_improvement)
      break;

    attempt++;
  }

  return duration;
}

} // {blbench}

int main(int argc, char* argv[]) {
  blbench::BenchApp app(argc, argv);

  if (!app.init()) {
    printf("Failed to initialize bl_bench.\n");
    return 1;
  }

  return app.run();
}
