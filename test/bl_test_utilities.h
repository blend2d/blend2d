// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// This file provides utility classes and functions shared between some tests.

#ifndef BLEND2D_TEST_UTILITIES_H_INCLUDED
#define BLEND2D_TEST_UTILITIES_H_INCLUDED

#include <blend2d.h>
#include <stdlib.h>
#include <string.h>

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

  int valueAsInt(const char* key, int defaultValue) const {
    const char* val = valueOf(key, nullptr);
    if (val == nullptr || val[0] == '\0')
      return defaultValue;

    return atoi(val);
  }

  unsigned valueAsUInt(const char* key, unsigned defaultValue) const {
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

class Logger {
public:
  enum class Verbosity {
    Debug,
    Info,
    Silent
  };

  Verbosity _verbosity;

  inline Logger(Verbosity verbosity)
    : _verbosity(verbosity) {}

  inline Verbosity verbossity() const { return _verbosity; }

  inline Verbosity setVerbosity(Verbosity value) {
    Verbosity prev = _verbosity;
    _verbosity = value;
    return prev;
  }

  template<typename... Args>
  inline void print(const char* fmt, Args&&... args) {
    printf(fmt, args...);
  }

  template<typename... Args>
  inline void debug(const char* fmt, Args&&... args) {
    if (_verbosity <= Verbosity::Debug)
      printf(fmt, args...);
  }

  template<typename... Args>
  inline void info(const char* fmt, Args&&... args) {
    if (_verbosity <= Verbosity::Info)
      printf(fmt, args...);
  }
};

class RandomDataGenerator {
public:
  enum class Mode {
    InBounds = 0
  };

  BLRandom _rnd;
  Mode _mode;
  BLBox _bounds;
  BLSize _size;

  RandomDataGenerator()
    : _rnd(0x12345678),
      _mode(Mode::InBounds),
      _bounds(),
      _size() {}

  inline Mode mode() const { return _mode; }
  inline void setMode(Mode mode) { _mode = mode; }

  inline const BLBox& bounds() const { return _bounds; }
  inline void setBounds(const BLBox& bounds) {
    _bounds = bounds;
    _size.reset(_bounds.x1 - _bounds.x0, _bounds.y1 - _bounds.y0);
  }

  inline void seed(uint64_t value) { _rnd.reset(value); }

  inline BLRgba32 nextRgb32() { return BLRgba32(_rnd.nextUInt32() | 0xFF000000u); }

  int nextXCoordI() { return (int)((_rnd.nextDouble() * _size.w) + _bounds.x0); }
  int nextYCoordI() { return (int)((_rnd.nextDouble() * _size.h) + _bounds.y0); };

  double nextXCoordD() { return (_rnd.nextDouble() * _size.w) + _bounds.x0; }
  double nextYCoordD() { return (_rnd.nextDouble() * _size.h) + _bounds.y0; };

  inline BLPoint nextPointD() { return BLPoint(nextXCoordD(), nextYCoordD()); }
  inline BLPoint nextPointI() { return BLPointI(nextXCoordI(), nextYCoordI()); }

  inline BLBox nextBoxD() {
    double x0 = nextXCoordD();
    double y0 = nextYCoordD();
    double x1 = nextXCoordD();
    double y1 = nextYCoordD();
    return BLBox(blMin(x0, x1), blMin(y0, y1), blMax(x0, x1), blMax(y0, y1));
  }

  inline BLBoxI nextBoxI() {
    int x0 = nextXCoordI();
    int y0 = nextYCoordI();
    int x1 = nextXCoordI();
    int y1 = nextYCoordI();

    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);

    if (x0 == x1) x1++;
    if (y0 == y1) y1++;

    return BLBoxI(x0, y0, x1, y1);
  }

  inline BLRectI nextRectI() {
    BLBoxI box = nextBoxI();
    return BLRectI(box.x0, box.y0, box.x1 - box.x0, box.y1 - box.y0);
  }

  inline BLRect nextRectD() {
    BLBox box = nextBoxD();
    return BLRect(box.x0, box.y0, box.x1 - box.x0, box.y1 - box.y0);
  }

  inline BLTriangle nextTriangle() {
    BLTriangle out;
    out.x0 = nextXCoordD();
    out.y0 = nextYCoordD();
    out.x1 = nextXCoordD();
    out.y1 = nextYCoordD();
    out.x2 = nextXCoordD();
    out.y2 = nextYCoordD();
    return out;
  }
};

class ContextFuzzer {
public:
  RandomDataGenerator _rnd;
  const char* _prefix;
  Logger _logger;
  BLImage _img;
  BLContext _ctx;
  bool _storeImages;

  ContextFuzzer(const char* prefix, Logger::Verbosity verbosity)
    : _prefix(prefix),
      _logger(verbosity),
      _storeImages(false) {}

  BLResult init(int w, int h, BLFormat format, uint32_t threadCount) {
    BLContextCreateInfo createInfo {};
    createInfo.threadCount = threadCount;

    BL_PROPAGATE(_img.create(w, h, format));
    BL_PROPAGATE(_ctx.begin(_img, createInfo));

    double oob = 30;

    _rnd.setBounds(BLBox(0.0 - oob, 0.0 - oob, w + oob, h + oob));
    _ctx.clearAll();
    _ctx.setFillStyle(BLRgba32(0xFFFFFFFF));

    return BL_SUCCESS;
  }

  void seed(uint32_t seed) { _rnd.seed(seed); }
  void setStoreImages(bool value) { _storeImages = value; }

  const BLImage& image() const { return _img; }

  void reset() {
    _ctx.reset();
    _img.reset();
  }

  void started(const char* fuzzName) {
    _logger.info("%sFuzzing: %s\n", _prefix, fuzzName);
  }

  void finished(const char* fuzzName) {
    _ctx.flush(BL_CONTEXT_FLUSH_SYNC);

    if (_storeImages && _img) {
      BLString s;
      s.assignFormat("%s.bmp", fuzzName);
      _logger.info("%sStoring: %s\n", _prefix, s.data());
      _img.writeToFile(s.data());
    }
  }

  void clear() { _ctx.clearAll(); }

  void fuzzFillRectI(size_t n) {
    const char* fuzzName = "FillRectI";

    started(fuzzName);
    for (size_t i = 0; i < n; i++) {
      BLRectI rect = _rnd.nextRectI();
      _logger.debug("%sFillRectI(%d, %d, %d, %d)\n", _prefix, rect.x, rect.y, rect.w, rect.h);
      _ctx.setFillStyle(_rnd.nextRgb32());
      _ctx.fillRect(rect);
    }
    finished(fuzzName);
  }

  void fuzzFillRectD(size_t n) {
    const char* fuzzName = "FillRectD";

    started(fuzzName);
    for (size_t i = 0; i < n; i++) {
      BLRect rect = _rnd.nextRectD();
      _logger.debug("%sFillRectD(%g, %g, %g, %g)\n", _prefix, rect.x, rect.y, rect.w, rect.h);
      _ctx.setFillStyle(_rnd.nextRgb32());
      _ctx.fillRect(rect);
    }
    finished(fuzzName);
  }

  void fuzzFillTriangle(size_t n) {
    const char* fuzzName = "FillTriangle";

    started(fuzzName);
    for (size_t i = 0; i < n; i++) {
      BLTriangle t = _rnd.nextTriangle();
      _logger.debug("%sFillTriangle(%g, %g, %g, %g, %g, %g)\n", _prefix, t.x0, t.y0, t.x1, t.y1, t.x2, t.y2);
      _ctx.setFillStyle(_rnd.nextRgb32());
      _ctx.fillTriangle(t);
    }
    finished(fuzzName);
  }

  void fuzzFillPathQuads(size_t n) {
    const char* fuzzName = "FillPathQuads";

    started(fuzzName);
    for (size_t i = 0; i < n; i++) {
      BLPath path;
      path.moveTo(_rnd.nextPointD());
      path.quadTo(_rnd.nextPointD(), _rnd.nextPointD());
      _ctx.setFillStyle(_rnd.nextRgb32());
      _ctx.fillPath(path);
    }
    finished(fuzzName);
  }

  void fuzzFillPathCubics(size_t n) {
    const char* fuzzName = "FillPathCubics";

    started(fuzzName);
    for (size_t i = 0; i < n; i++) {
      BLPath path;
      path.moveTo(_rnd.nextPointD());
      path.cubicTo(_rnd.nextPointD(), _rnd.nextPointD(), _rnd.nextPointD());
      _ctx.setFillStyle(_rnd.nextRgb32());
      _ctx.fillPath(path);
    }
    finished(fuzzName);
  }
};

namespace StringUtils {

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

} // {StringUtils}

namespace ImageUtils {

struct DiffInfo {
  int maxDiff;
  int cumulativeDiff;
};

static DiffInfo diffInfo(const BLImage& aImage, const BLImage& bImage) noexcept {
  DiffInfo info {};
  BLImageData aData;
  BLImageData bData;

  if (aImage.size() != bImage.size())
    return info;

  size_t w = size_t(aImage.width());
  size_t h = size_t(aImage.height());

  if (aImage.getData(&aData) != BL_SUCCESS)
    return info;

  if (bImage.getData(&bData) != BL_SUCCESS)
    return info;

  intptr_t aStride = aData.stride;
  intptr_t bStride = bData.stride;

  const uint8_t* aLine = static_cast<const uint8_t*>(aData.pixelData);
  const uint8_t* bLine = static_cast<const uint8_t*>(bData.pixelData);

  for (size_t y = 0; y < h; y++) {
    const uint32_t* aPtr = reinterpret_cast<const uint32_t*>(aLine);
    const uint32_t* bPtr = reinterpret_cast<const uint32_t*>(bLine);

    for (size_t x = 0; x < w; x++) {
      uint32_t aVal = aPtr[x];
      uint32_t bVal = bPtr[x];

      if (aVal != bVal) {
        int aDiff = blAbs(int((aVal >> 24) & 0xFF) - int((bVal >> 24) & 0xFF));
        int rDiff = blAbs(int((aVal >> 16) & 0xFF) - int((bVal >> 16) & 0xFF));
        int gDiff = blAbs(int((aVal >>  8) & 0xFF) - int((bVal >>  8) & 0xFF));
        int bDiff = blAbs(int((aVal      ) & 0xFF) - int((bVal      ) & 0xFF));
        int maxDiff = blMax(aDiff, rDiff, gDiff, bDiff);

        info.maxDiff = blMax(info.maxDiff, maxDiff);
        info.cumulativeDiff += maxDiff;
      }
    }

    aLine += aStride;
    bLine += bStride;
  }

  return info;
}

static BLImage diffImage(const BLImage& aImage, const BLImage& bImage) noexcept {
  BLImage result;
  BLImageData rData;
  BLImageData aData;
  BLImageData bData;

  if (aImage.size() != bImage.size())
    return result;

  size_t w = size_t(aImage.width());
  size_t h = size_t(aImage.height());

  if (aImage.getData(&aData) != BL_SUCCESS)
    return result;

  if (bImage.getData(&bData) != BL_SUCCESS)
    return result;

  if (result.create(w, h, BL_FORMAT_XRGB32) != BL_SUCCESS)
    return result;

  if (result.getData(&rData) != BL_SUCCESS)
    return result;

  intptr_t dStride = rData.stride;
  intptr_t aStride = aData.stride;
  intptr_t bStride = bData.stride;

  uint8_t* dLine = static_cast<uint8_t*>(rData.pixelData);
  const uint8_t* aLine = static_cast<const uint8_t*>(aData.pixelData);
  const uint8_t* bLine = static_cast<const uint8_t*>(bData.pixelData);

  for (size_t y = 0; y < h; y++) {
    uint32_t* dPtr = reinterpret_cast<uint32_t*>(dLine);
    const uint32_t* aPtr = reinterpret_cast<const uint32_t*>(aLine);
    const uint32_t* bPtr = reinterpret_cast<const uint32_t*>(bLine);

    for (size_t x = 0; x < w; x++) {
      uint32_t aVal = aPtr[x];
      uint32_t bVal = bPtr[x];

      int aDiff = blAbs(int((aVal >> 24) & 0xFF) - int((bVal >> 24) & 0xFF));
      int rDiff = blAbs(int((aVal >> 16) & 0xFF) - int((bVal >> 16) & 0xFF));
      int gDiff = blAbs(int((aVal >>  8) & 0xFF) - int((bVal >>  8) & 0xFF));
      int bDiff = blAbs(int((aVal      ) & 0xFF) - int((bVal      ) & 0xFF));

      int maxDiff = blMax(aDiff, rDiff, gDiff, bDiff);
      uint32_t dVal = 0xFF000000u;

      if (maxDiff) {
        if (maxDiff <= 4)
          dVal = 0xFF000000u + unsigned((maxDiff * 64 - 1) << 0);
        else if (maxDiff <= 16)
          dVal = 0xFF000000u + unsigned((maxDiff * 16 - 1) << 8);
        else
          dVal = 0xFF000000u + unsigned((127 + maxDiff / 2) << 16);
      }

      dPtr[x] = dVal;
    }

    dLine += dStride;
    aLine += aStride;
    bLine += bStride;
  }

  return result;
}

} // {ImageUtils}

#endif // BLEND2D_TEST_UTILITIES_H_INCLUDED
