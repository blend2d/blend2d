// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// bl_test_fuzzer
// --------------
//
// This is a simple rendering context fuzzer that covers only basic API calls
// at the moment. It will be improved in the future to cover also paths and
// other features like stroking.

#include <blend2d.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// [CmdLine]
// ============================================================================

class CmdLine {
public:
  int _argc;
  const char* const* _argv;

  CmdLine(int argc, const char* const* argv)
    : _argc(argc),
      _argv(argv) {}

  bool hasArg(const char* key) const {
    for (int i = 1; i < _argc; i++)
      if (strcmp(key, _argv[i]) == 0)
        return true;
    return false;
  }

  const char* valueOf(const char* key, const char* defaultValue) const {
    size_t keySize = strlen(key);
    for (int i = 1; i < _argc; i++) {
      const char* val = _argv[i];
      if (strlen(val) >= keySize + 1 && val[keySize] == '=' && memcmp(val, key, keySize) == 0)
        return val + keySize + 1;
    }

    return defaultValue;
  }

  int intValueOf(const char* key, int defaultValue) const {
    const char* val = valueOf(key, nullptr);
    if (val == nullptr || val[0] == '\0')
      return defaultValue;

    return atoi(val);
  }

  unsigned uintValueOf(const char* key, int defaultValue) const {
    const char* val = valueOf(key, nullptr);
    if (val == nullptr || val[0] == '\0')
      return defaultValue;

    int v = atoi(val);
    if (v < 0)
      return defaultValue;
    else
      return unsigned(v);
  }
};

// ============================================================================
// [ContextFuzzer]
// ============================================================================

class ContextFuzzer {
public:
  BLRandom _rnd;
  BLImage _img;
  BLContext _ctx;
  bool _enableLogger;

  ContextFuzzer(bool enableLogger)
    : _enableLogger(enableLogger) {}

  BLResult init(int w, int h, uint32_t format) {
    BL_PROPAGATE(_img.create(w, h, format));
    BL_PROPAGATE(_ctx.begin(_img));

    _ctx.clearAll();
    _ctx.setFillStyle(BLRgba32(0xFFFFFFFF));

    return BL_SUCCESS;
  }

  void seed(uint32_t seed) {
    _rnd.reset(seed);
  }

  void reset() {
    _ctx.reset();
    _img.reset();
  }

  int randIntCoord() {
    return int(_rnd.nextUInt32());
  }

  int randIntLength() {
    return randIntCoord();
  }

  double randDoubleCoord() {
    double x = _rnd.nextDouble() * 2000 - 1000;
    double y = _rnd.nextDouble();
    double v = x + y;

    if (std::isfinite(v))
      return v;
    else
      return 0.0;
  }

  double randDoubleLength() {
    return randDoubleCoord();
  }

  BLPoint randPoint() {
    BLPoint p;
    p.x = randDoubleCoord();
    p.y = randDoubleCoord();
    return p;
  }

  BLRectI randRectI() {
    BLRectI rect;
    rect.x = randIntCoord();
    rect.y = randIntCoord();
    rect.w = randIntLength();
    rect.h = randIntLength();
    return rect;
  }

  BLRect randRectD() {
    BLRect rect;
    rect.x = randDoubleCoord();
    rect.y = randDoubleCoord();
    rect.w = randDoubleLength();
    rect.h = randDoubleLength();
    return rect;
  }

  void fuzzFillRectI(size_t n) {
    printf("Fuzzing FillRectI\n");
    for (size_t i = 0; i < n; i++) {
      BLRectI rect = randRectI();
      if (_enableLogger)
        printf("FillRectI(%d, %d, %d, %d)\n", rect.x, rect.y, rect.w, rect.h);
      _ctx.fillRect(rect);
    }
  }

  void fuzzFillRectD(size_t n) {
    printf("Fuzzing FillRectD\n");
    for (size_t i = 0; i < n; i++) {
      BLRect rect = randRectD();
      if (_enableLogger)
        printf("FillRectD(%g, %g, %g, %g)\n", rect.x, rect.y, rect.w, rect.h);
      _ctx.fillRect(rect);
    }
  }

  void fuzzFillTriangle(size_t n) {
    printf("Fuzzing FillTriangle\n");
    for (size_t i = 0; i < n; i++) {
      BLTriangle t;
      t.x0 = randDoubleCoord();
      t.y0 = randDoubleCoord();
      t.x1 = randDoubleCoord();
      t.y1 = randDoubleCoord();
      t.x2 = randDoubleCoord();
      t.y2 = randDoubleCoord();

      if (_enableLogger)
        printf("FillTriangle(%g, %g, %g, %g, %g, %g)\n", t.x0, t.y0, t.x1, t.y1, t.x2, t.y2);

      _ctx.fillTriangle(t);
    }
  }

  void fuzzFillQuads(size_t n) {
    printf("Fuzzing FillPathQuads\n");
    for (size_t i = 0; i < n; i++) {
      BLPath path;
      path.moveTo(randPoint());
      path.quadTo(randPoint(), randPoint());
      _ctx.fillPath(path);
    }
  }

  void fuzzFillCubics(size_t n) {
    printf("Fuzzing FillPathCubics\n");
    for (size_t i = 0; i < n; i++) {
      BLPath path;
      path.moveTo(randPoint());
      path.cubicTo(randPoint(), randPoint(), randPoint());
      _ctx.fillPath(path);
    }
  }
};

// ============================================================================
// [Main]
// ============================================================================

void help() {
  printf("Usage:\n");
  printf("  bl_test_fuzzer [Options]\n");
  printf("\n");

  printf("Fuzzer Options:\n");
  printf("  --log          - Debug each render command    [default=false]\n");
  printf("  --seed         - Random number generator seed [default=1]\n");
  printf("  --width        - Image width                  [default=513]\n");
  printf("  --height       - Image height                 [default=513]\n");
  printf("  --count        - Count of render commands     [default=1000000]\n");
  printf("  --command      - Debug each render command    [default=All]\n");
  printf("\n");

  printf("Fuzzer Commands:\n");
  printf("  FillRectI      - Fill aligned rectangles\n");
  printf("  FillRectD      - Fill unaligned rectangles\n");
  printf("  FillTriangle   - Fill triangles\n");
  printf("  FillQuads      - Fill path having quadratic curves\n");
  printf("  FillCubics     - Fill path having cubic curves\n");
}

static bool strieq(const char* a, const char* b) {
  size_t aLen = strlen(a);
  size_t bLen = strlen(b);

  if (aLen != bLen)
    return false;

  for (size_t i = 0; i < aLen; i++) {
    unsigned ac = (unsigned char)a[i];
    unsigned bc = (unsigned char)b[i];

    if (ac >= 'A' && ac <= 'Z') ac += 'A' - 'a';
    if (bc >= 'A' && bc <= 'Z') bc += 'A' - 'a';

    if (ac != bc)
      return false;
  }

  return true;
}

int main(int argc, char* argv[]) {
  BLRuntimeBuildInfo buildInfo;
  BLRuntime::queryBuildInfo(&buildInfo);

  CmdLine cmdLine(argc, argv);

  // Basic information.
  printf(
    "Blend2D Fuzzer [use --help for command line options]\n"
    "  Version    : %u.%u.%u\n"
    "  Build Type : %s\n"
    "  Compiled By: %s\n\n",
    buildInfo.majorVersion,
    buildInfo.minorVersion,
    buildInfo.patchVersion,
    buildInfo.buildType == BL_RUNTIME_BUILD_TYPE_DEBUG ? "Debug" : "Release",
    buildInfo.compilerInfo);

  if (cmdLine.hasArg("--help")) {
    help();
    return 0;
  }

  // Command line parameters.
  bool enableLogger = cmdLine.hasArg("--log");
  uint32_t seed = cmdLine.uintValueOf("--seed", 1);
  uint32_t width = cmdLine.uintValueOf("--width", 513);
  uint32_t height = cmdLine.uintValueOf("--height", 513);
  uint32_t count = cmdLine.uintValueOf("--count", 1000000);

  const char* command = cmdLine.valueOf("--command", "");
  bool all = command[0] == '\0' || strcmp(command, "all") == 0;

  // Fuzzing...
  ContextFuzzer fuzzer(enableLogger);
  fuzzer.seed(seed);

  if (fuzzer.init(int(width), int(height), BL_FORMAT_PRGB32) != BL_SUCCESS) {
    printf("Failed to initialize the rendering context\n");
    return 1;
  }

  if (all || strieq(command, "FillRectI"))
    fuzzer.fuzzFillRectI(count);

  if (all || strieq(command, "FillRectD"))
    fuzzer.fuzzFillRectD(count);

  if (all || strieq(command, "FillTriangle"))
    fuzzer.fuzzFillTriangle(count);

  if (all || strieq(command, "FillPathQuads"))
    fuzzer.fuzzFillQuads(count);

  if (all || strieq(command, "FillPathCubics"))
    fuzzer.fuzzFillCubics(count);

  fuzzer.reset();

  printf("Fuzzing finished...\n");
  return 0;
}
