// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_EDGEBUILDER_P_H_INCLUDED
#define BLEND2D_RASTER_EDGEBUILDER_P_H_INCLUDED

#include "../geometry_p.h"
#include "../math_p.h"
#include "../path_p.h"
#include "../pipeline/pipedefs_p.h"
#include "../raster/edgestorage_p.h"
#include "../support/algorithm_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/traits_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

enum ClipShift : uint32_t {
  kClipShiftX0  = 0,
  kClipShiftY0  = 1,
  kClipShiftX1  = 2,
  kClipShiftY1  = 3
};

enum ClipFlags: uint32_t {
  kClipFlagNone = 0u,
  kClipFlagX0   = 1u << kClipShiftX0,
  kClipFlagY0   = 1u << kClipShiftY0,
  kClipFlagX1   = 1u << kClipShiftX1,
  kClipFlagY1   = 1u << kClipShiftY1,

  kClipFlagX0X1 = kClipFlagX0 | kClipFlagX1,
  kClipFlagY0Y1 = kClipFlagY0 | kClipFlagY1,

  kClipFlagX0Y0 = kClipFlagX0 | kClipFlagY0,
  kClipFlagX1Y0 = kClipFlagX1 | kClipFlagY0,

  kClipFlagX0Y1 = kClipFlagX0 | kClipFlagY1,
  kClipFlagX1Y1 = kClipFlagX1 | kClipFlagY1
};

static BL_INLINE uint32_t blClipCalcX0Flags(const BLPoint& pt, const BLBox& box) noexcept { return (uint32_t(!(pt.x >= box.x0)) << kClipShiftX0); }
static BL_INLINE uint32_t blClipCalcX1Flags(const BLPoint& pt, const BLBox& box) noexcept { return (uint32_t(!(pt.x <= box.x1)) << kClipShiftX1); }
static BL_INLINE uint32_t blClipCalcY0Flags(const BLPoint& pt, const BLBox& box) noexcept { return (uint32_t(!(pt.y >= box.y0)) << kClipShiftY0); }
static BL_INLINE uint32_t blClipCalcY1Flags(const BLPoint& pt, const BLBox& box) noexcept { return (uint32_t(!(pt.y <= box.y1)) << kClipShiftY1); }

static BL_INLINE uint32_t blClipCalcXFlags(const BLPoint& pt, const BLBox& box) noexcept { return blClipCalcX0Flags(pt, box) | blClipCalcX1Flags(pt, box); }
static BL_INLINE uint32_t blClipCalcYFlags(const BLPoint& pt, const BLBox& box) noexcept { return blClipCalcY0Flags(pt, box) | blClipCalcY1Flags(pt, box); }
static BL_INLINE uint32_t blClipCalcXYFlags(const BLPoint& pt, const BLBox& box) noexcept { return blClipCalcXFlags(pt, box) | blClipCalcYFlags(pt, box); }

//! \name Edge Transformations
//! \{

class EdgeTransformNone {
public:
  BL_INLINE EdgeTransformNone() noexcept {}
  BL_INLINE void apply(BLPoint& dst, const BLPoint& src) noexcept { dst = src; }
};

class EdgeTransformScale {
public:
  double sx, sy;
  double tx, ty;

  BL_INLINE EdgeTransformScale(const BLMatrix2D& matrix) noexcept
    : sx(matrix.m00),
      sy(matrix.m11),
      tx(matrix.m20),
      ty(matrix.m21) {}
  BL_INLINE EdgeTransformScale(const EdgeTransformScale& other) noexcept = default;

  BL_INLINE void apply(BLPoint& dst, const BLPoint& src) noexcept {
    dst.reset(src.x * sx + tx, src.y * sy + ty);
  }
};

class EdgeTransformAffine {
public:
  BLMatrix2D matrix;

  BL_INLINE EdgeTransformAffine(const BLMatrix2D& matrix) noexcept
    : matrix(matrix) {}
  BL_INLINE EdgeTransformAffine(const EdgeTransformAffine& other) noexcept = default;

  BL_INLINE void apply(BLPoint& dst, const BLPoint& src) noexcept {
    dst = matrix.mapPoint(src);
  }
};

//! \}

//! \name Edge Source Data
//! \{

template<class PointType, class Transform = EdgeTransformNone>
class EdgeSourcePoly {
public:
  Transform _transform;
  const PointType* _srcPtr;
  const PointType* _srcEnd;

  BL_INLINE EdgeSourcePoly(const Transform& transform) noexcept
    : _transform(transform),
      _srcPtr(nullptr),
      _srcEnd(nullptr) {}

  BL_INLINE EdgeSourcePoly(const Transform& transform, const PointType* srcPtr, size_t count) noexcept
    : _transform(transform),
      _srcPtr(srcPtr),
      _srcEnd(srcPtr + count) {}

  BL_INLINE void reset(const PointType* srcPtr, size_t count) noexcept {
    _srcPtr = srcPtr;
    _srcEnd = srcPtr + count;
  }

  BL_INLINE bool begin(BLPoint& initial) noexcept {
    if (_srcPtr == _srcEnd)
      return false;

    _transform.apply(initial, BLPoint(_srcPtr[0].x, _srcPtr[0].y));
    _srcPtr++;
    return true;
  }

  BL_INLINE void beforeNextBegin() noexcept {}

  BL_INLINE constexpr bool isClose() const noexcept { return false; }
  BL_INLINE bool isLineTo() const noexcept { return _srcPtr != _srcEnd; }
  BL_INLINE constexpr bool isQuadTo() const noexcept { return false; }
  BL_INLINE constexpr bool isCubicTo() const noexcept { return false; }

  BL_INLINE void nextLineTo(BLPoint& pt1) noexcept {
    _transform.apply(pt1, BLPoint(_srcPtr[0].x, _srcPtr[0].y));
    _srcPtr++;
  }

  BL_INLINE bool maybeNextLineTo(BLPoint& pt1) noexcept {
    if (_srcPtr == _srcEnd)
      return false;

    nextLineTo(pt1);
    return true;
  }

  BL_INLINE void nextQuadTo(BLPoint&, BLPoint&) noexcept {}
  BL_INLINE bool maybeNextQuadTo(BLPoint&, BLPoint&) noexcept { return false; }

  BL_INLINE void nextCubicTo(BLPoint&, BLPoint&, BLPoint&) noexcept {}
  BL_INLINE bool maybeNextCubicTo(BLPoint&, BLPoint&, BLPoint&) noexcept { return false; }
};

template<class Transform = EdgeTransformNone>
class EdgeSourcePath {
public:
  Transform _transform;
  const BLPoint* _vtxPtr;
  const uint8_t* _cmdPtr;
  const uint8_t* _cmdEnd;
  const uint8_t* _cmdEndMinus2;

  BL_INLINE EdgeSourcePath(const Transform& transform) noexcept
    : _transform(transform),
      _vtxPtr(nullptr),
      _cmdPtr(nullptr),
      _cmdEnd(nullptr),
      _cmdEndMinus2(nullptr) {}

  BL_INLINE EdgeSourcePath(const Transform& transform, const BLPathView& view) noexcept
    : _transform(transform) { reset(view.vertexData, view.commandData, view.size); }

  BL_INLINE EdgeSourcePath(const Transform& transform, const BLPoint* vtxData, const uint8_t* cmdData, size_t count) noexcept
    : _transform(transform) { reset(vtxData, cmdData, count); }

  BL_INLINE void reset(const BLPoint* vtxData, const uint8_t* cmdData, size_t count) noexcept {
    _vtxPtr = vtxData;
    _cmdPtr = cmdData;
    _cmdEnd = cmdData + count;
    _cmdEndMinus2 = _cmdEnd - 2;
  }

  BL_INLINE void reset(const BLPath& path) noexcept {
    reset(path.vertexData(), path.commandData(), path.size());
  }

  BL_INLINE bool begin(BLPoint& initial) noexcept {
    for (;;) {
      if (_cmdPtr == _cmdEnd)
        return false;

      uint32_t cmd = _cmdPtr[0];
      _cmdPtr++;
      _vtxPtr++;

      if (cmd != BL_PATH_CMD_MOVE)
        continue;

      _transform.apply(initial, _vtxPtr[-1]);
      return true;
    }
  }

  BL_INLINE void beforeNextBegin() noexcept {}

  BL_INLINE bool isClose() const noexcept { return _cmdPtr != _cmdEnd && _cmdPtr[0] == BL_PATH_CMD_CLOSE; }
  BL_INLINE bool isLineTo() const noexcept { return _cmdPtr != _cmdEnd && _cmdPtr[0] == BL_PATH_CMD_ON; }
  BL_INLINE bool isQuadTo() const noexcept { return _cmdPtr <= _cmdEndMinus2 && _cmdPtr[0] == BL_PATH_CMD_QUAD; }
  BL_INLINE bool isCubicTo() const noexcept { return _cmdPtr < _cmdEndMinus2 && _cmdPtr[0] == BL_PATH_CMD_CUBIC; }

  BL_INLINE void nextLineTo(BLPoint& pt1) noexcept {
    _transform.apply(pt1, _vtxPtr[0]);
    _cmdPtr++;
    _vtxPtr++;
  }

  BL_INLINE bool maybeNextLineTo(BLPoint& pt1) noexcept {
    if (!isLineTo())
      return false;

    nextLineTo(pt1);
    return true;
  }

  BL_INLINE void nextQuadTo(BLPoint& pt1, BLPoint& pt2) noexcept {
    _transform.apply(pt1, _vtxPtr[0]);
    _transform.apply(pt2, _vtxPtr[1]);
    _cmdPtr += 2;
    _vtxPtr += 2;
  }

  BL_INLINE bool maybeNextQuadTo(BLPoint& pt1, BLPoint& pt2) noexcept {
    if (!isQuadTo())
      return false;

    nextQuadTo(pt1, pt2);
    return true;
  }

  BL_INLINE void nextCubicTo(BLPoint& pt1, BLPoint& pt2, BLPoint& pt3) noexcept {
    _transform.apply(pt1, _vtxPtr[0]);
    _transform.apply(pt2, _vtxPtr[1]);
    _transform.apply(pt3, _vtxPtr[2]);
    _cmdPtr += 3;
    _vtxPtr += 3;
  }

  BL_INLINE bool maybeNextCubicTo(BLPoint& pt1, BLPoint& pt2, BLPoint& pt3) noexcept {
    if (!isCubicTo())
      return false;

    nextCubicTo(pt1, pt2, pt3);
    return true;
  }
};

// Stroke sink never produces invalid paths, thus:
//   - this path will only have a single figure.
//   - we don't have to check whether the path is valid, it was produced by our stroker, which produces valid paths.
template<class Transform = EdgeTransformNone>
class EdgeSourceReversePathFromStrokeSink {
public:
  Transform _transform;
  const BLPoint* _vtxPtr;
  const uint8_t* _cmdPtr;
  const uint8_t* _cmdStart;
  bool _mustClose;

  BL_INLINE EdgeSourceReversePathFromStrokeSink(const Transform& transform) noexcept
    : _transform(transform),
      _vtxPtr(nullptr),
      _cmdPtr(nullptr),
      _cmdStart(nullptr),
      _mustClose(false) {}

  BL_INLINE EdgeSourceReversePathFromStrokeSink(const Transform& transform, const BLPathView& view) noexcept
    : _transform(transform) { reset(view.vertexData, view.commandData, view.size); }

  BL_INLINE EdgeSourceReversePathFromStrokeSink(const Transform& transform, const BLPoint* vtxData, const uint8_t* cmdData, size_t count) noexcept
    : _transform(transform) { reset(vtxData, cmdData, count); }

  BL_INLINE void reset(const BLPoint* vtxData, const uint8_t* cmdData, size_t count) noexcept {
    _vtxPtr = vtxData + count;
    _cmdPtr = cmdData + count;
    _cmdStart = cmdData;
    _mustClose = count > 0 && _cmdPtr[-1] == BL_PATH_CMD_CLOSE;

    _cmdPtr -= size_t(_mustClose);
    _vtxPtr -= size_t(_mustClose);
  }

  BL_INLINE void reset(const BLPath& path) noexcept {
    reset(path.vertexData(), path.commandData(), path.size());
  }

  BL_INLINE bool begin(BLPoint& initial) noexcept {
    if (_cmdPtr == _cmdStart)
      return false;

    // The only check we do - if the path doesn't end with on-point, we won't process the path.
    uint32_t cmd = _cmdPtr[-1];
    if (cmd != BL_PATH_CMD_ON)
      return false;

    _cmdPtr--;
    _vtxPtr--;
    _transform.apply(initial, _vtxPtr[0]);
    return true;
  }

  BL_INLINE bool mustClose() const noexcept { return _mustClose; }

  BL_INLINE void beforeNextBegin() noexcept {}

  BL_INLINE bool isClose() const noexcept { return false; }
  BL_INLINE bool isLineTo() const noexcept { return _cmdPtr != _cmdStart && _cmdPtr[-1] <= BL_PATH_CMD_ON; }
  BL_INLINE bool isQuadTo() const noexcept { return _cmdPtr != _cmdStart && _cmdPtr[-1] == BL_PATH_CMD_QUAD; }
  BL_INLINE bool isCubicTo() const noexcept { return _cmdPtr != _cmdStart && _cmdPtr[-1] == BL_PATH_CMD_CUBIC; }

  BL_INLINE void nextLineTo(BLPoint& pt1) noexcept {
    _cmdPtr--;
    _vtxPtr--;
    _transform.apply(pt1, _vtxPtr[0]);
  }

  BL_INLINE bool maybeNextLineTo(BLPoint& pt1) noexcept {
    if (!isLineTo())
      return false;

    nextLineTo(pt1);
    return true;
  }

  BL_INLINE void nextQuadTo(BLPoint& pt1, BLPoint& pt2) noexcept {
    _cmdPtr -= 2;
    _vtxPtr -= 2;
    _transform.apply(pt1, _vtxPtr[1]);
    _transform.apply(pt2, _vtxPtr[0]);
  }

  BL_INLINE bool maybeNextQuadTo(BLPoint& pt1, BLPoint& pt2) noexcept {
    if (!isQuadTo())
      return false;

    nextQuadTo(pt1, pt2);
    return true;
  }

  BL_INLINE void nextCubicTo(BLPoint& pt1, BLPoint& pt2, BLPoint& pt3) noexcept {
    _cmdPtr -= 3;
    _vtxPtr -= 3;
    _transform.apply(pt1, _vtxPtr[2]);
    _transform.apply(pt2, _vtxPtr[1]);
    _transform.apply(pt3, _vtxPtr[0]);
  }

  BL_INLINE bool maybeNextCubicTo(BLPoint& pt1, BLPoint& pt2, BLPoint& pt3) noexcept {
    if (!isCubicTo())
      return false;

    nextCubicTo(pt1, pt2, pt3);
    return true;
  }
};

template<class PointType>
using EdgeSourcePolyScale = EdgeSourcePoly<PointType, EdgeTransformScale>;

template<class PointType>
using EdgeSourcePolyAffine = EdgeSourcePoly<PointType, EdgeTransformAffine>;

typedef EdgeSourcePath<EdgeTransformScale> EdgeSourcePathScale;
typedef EdgeSourcePath<EdgeTransformAffine> EdgeSourcePathAffine;

typedef EdgeSourceReversePathFromStrokeSink<EdgeTransformScale> EdgeSourceReversePathFromStrokeSinkScale;
typedef EdgeSourceReversePathFromStrokeSink<EdgeTransformAffine> EdgeSourceReversePathFromStrokeSinkAffine;

//! \}

//! \name Edge Flattening
//! \{

//! Base data (mostly stack) used by `FlattenMonoQuad` and `FlattenMonoCubic`.
class FlattenMonoData {
public:
  enum : size_t {
    kRecursionLimit = 32,

    kStackSizeQuad  = kRecursionLimit * 3,
    kStackSizeCubic = kRecursionLimit * 4,
    kStackSizeTotal = kStackSizeCubic
  };

  BLPoint _stack[kStackSizeTotal];
};

//! Helper to flatten a monotonic quad curve.
class FlattenMonoQuad {
public:
  FlattenMonoData& _flattenData;
  double _toleranceSq;
  BLPoint* _stackPtr;
  BLPoint _p0, _p1, _p2;

  struct SplitStep {
    BL_INLINE bool isFinite() const noexcept { return blIsFinite(value); }
    BL_INLINE const BLPoint& midPoint() const noexcept { return p012; }

    double value;
    double limit;

    BLPoint p01;
    BLPoint p12;
    BLPoint p012;
  };

  BL_INLINE explicit FlattenMonoQuad(FlattenMonoData& flattenData, double toleranceSq) noexcept
    : _flattenData(flattenData),
      _toleranceSq(toleranceSq) {}

  BL_INLINE void begin(const BLPoint* src, uint32_t signBit) noexcept {
    _stackPtr = _flattenData._stack;

    if (signBit == 0) {
      _p0 = src[0];
      _p1 = src[1];
      _p2 = src[2];
    }
    else {
      _p0 = src[2];
      _p1 = src[1];
      _p2 = src[0];
    }
  }

  BL_INLINE const BLPoint& first() const noexcept { return _p0; }
  BL_INLINE const BLPoint& last() const noexcept { return _p2; }

  BL_INLINE bool canPop() const noexcept { return _stackPtr != _flattenData._stack; }
  BL_INLINE bool canPush() const noexcept { return _stackPtr != _flattenData._stack + FlattenMonoData::kStackSizeQuad; }

  BL_INLINE bool isLeftToRight() const noexcept { return first().x < last().x; }

  // Caused by floating point inaccuracy, we must bound the control
  // point as we really need monotonic curve that would never outbound
  // the boundary defined by its start/end points.
  BL_INLINE void boundLeftToRight() noexcept {
    _p1.x = blClamp(_p1.x, _p0.x, _p2.x);
    _p1.y = blClamp(_p1.y, _p0.y, _p2.y);
  }

  BL_INLINE void boundRightToLeft() noexcept {
    _p1.x = blClamp(_p1.x, _p2.x, _p0.x);
    _p1.y = blClamp(_p1.y, _p0.y, _p2.y);
  }

  BL_INLINE bool isFlat(SplitStep& step) const noexcept {
    BLPoint v1 = _p1 - _p0;
    BLPoint v2 = _p2 - _p0;

    double d = BLGeometry::cross(v2, v1);
    double lenSq = BLGeometry::lengthSq(v2);

    step.value = d * d;
    step.limit = _toleranceSq * lenSq;

    return step.value <= step.limit;
  }

  BL_INLINE void split(SplitStep& step) const noexcept {
    step.p01 = (_p0 + _p1) * 0.5;
    step.p12 = (_p1 + _p2) * 0.5;
    step.p012 = (step.p01 + step.p12) * 0.5;
  }

  BL_INLINE void push(const SplitStep& step) noexcept {
    // Must be checked before calling `push()`.
    BL_ASSERT(canPush());

    _stackPtr[0].reset(step.p012);
    _stackPtr[1].reset(step.p12);
    _stackPtr[2].reset(_p2);
    _stackPtr += 3;

    _p1 = step.p01;
    _p2 = step.p012;
  }

  BL_INLINE void discardAndAdvance(const SplitStep& step) noexcept {
    _p0 = step.p012;
    _p1 = step.p12;
  }

  BL_INLINE void pop() noexcept {
    _stackPtr -= 3;
    _p0 = _stackPtr[0];
    _p1 = _stackPtr[1];
    _p2 = _stackPtr[2];
  }
};

//! Helper to flatten a monotonic cubic curve.
class FlattenMonoCubic {
public:
  FlattenMonoData& _flattenData;
  double _toleranceSq;
  BLPoint* _stackPtr;
  BLPoint _p0, _p1, _p2, _p3;

  struct SplitStep {
    BL_INLINE bool isFinite() const noexcept { return blIsFinite(value); }
    BL_INLINE const BLPoint& midPoint() const noexcept { return p0123; }

    double value;
    double limit;

    BLPoint p01;
    BLPoint p12;
    BLPoint p23;
    BLPoint p012;
    BLPoint p123;
    BLPoint p0123;
  };

  BL_INLINE explicit FlattenMonoCubic(FlattenMonoData& flattenData, double toleranceSq) noexcept
    : _flattenData(flattenData),
      _toleranceSq(toleranceSq) {}

  BL_INLINE void begin(const BLPoint* src, uint32_t signBit) noexcept {
    _stackPtr = _flattenData._stack;

    if (signBit == 0) {
      _p0 = src[0];
      _p1 = src[1];
      _p2 = src[2];
      _p3 = src[3];
    }
    else {
      _p0 = src[3];
      _p1 = src[2];
      _p2 = src[1];
      _p3 = src[0];
    }
  }

  BL_INLINE const BLPoint& first() const noexcept { return _p0; }
  BL_INLINE const BLPoint& last() const noexcept { return _p3; }

  BL_INLINE bool canPop() const noexcept { return _stackPtr != _flattenData._stack; }
  BL_INLINE bool canPush() const noexcept { return _stackPtr != _flattenData._stack + FlattenMonoData::kStackSizeCubic; }

  BL_INLINE bool isLeftToRight() const noexcept { return first().x < last().x; }

  // Caused by floating point inaccuracy, we must bound the control
  // point as we really need monotonic curve that would never outbound
  // the boundary defined by its start/end points.
  BL_INLINE void boundLeftToRight() noexcept {
    _p1.x = blClamp(_p1.x, _p0.x, _p3.x);
    _p1.y = blClamp(_p1.y, _p0.y, _p3.y);
    _p2.x = blClamp(_p2.x, _p0.x, _p3.x);
    _p2.y = blClamp(_p2.y, _p0.y, _p3.y);
  }

  BL_INLINE void boundRightToLeft() noexcept {
    _p1.x = blClamp(_p1.x, _p3.x, _p0.x);
    _p1.y = blClamp(_p1.y, _p0.y, _p3.y);
    _p2.x = blClamp(_p2.x, _p3.x, _p0.x);
    _p2.y = blClamp(_p2.y, _p0.y, _p3.y);
  }

  BL_INLINE bool isFlat(SplitStep& step) const noexcept {
    BLPoint v = _p3 - _p0;

    double d1Sq = blSquare(BLGeometry::cross(v, _p1 - _p0));
    double d2Sq = blSquare(BLGeometry::cross(v, _p2 - _p0));
    double lenSq = BLGeometry::lengthSq(v);

    step.value = blMax(d1Sq, d2Sq);
    step.limit = _toleranceSq * lenSq;

    return step.value <= step.limit;
  }

  BL_INLINE void split(SplitStep& step) const noexcept {
    step.p01 = (_p0 + _p1) * 0.5;
    step.p12 = (_p1 + _p2) * 0.5;
    step.p23 = (_p2 + _p3) * 0.5;
    step.p012 = (step.p01 + step.p12 ) * 0.5;
    step.p123 = (step.p12 + step.p23 ) * 0.5;
    step.p0123 = (step.p012 + step.p123) * 0.5;
  }

  BL_INLINE void push(const SplitStep& step) noexcept {
    // Must be checked before calling `push()`.
    BL_ASSERT(canPush());

    _stackPtr[0].reset(step.p0123);
    _stackPtr[1].reset(step.p123);
    _stackPtr[2].reset(step.p23);
    _stackPtr[3].reset(_p3);
    _stackPtr += 4;

    _p1 = step.p01;
    _p2 = step.p012;
    _p3 = step.p0123;
  }

  BL_INLINE void discardAndAdvance(const SplitStep& step) noexcept {
    _p0 = step.p0123;
    _p1 = step.p123;
    _p2 = step.p23;
  }

  BL_INLINE void pop() noexcept {
    _stackPtr -= 4;
    _p0 = _stackPtr[0];
    _p1 = _stackPtr[1];
    _p2 = _stackPtr[2];
    _p3 = _stackPtr[3];
  }
};

//! \}

//! \name Edge Builder
//! \{

template<typename CoordT>
class EdgeBuilder {
public:
  static constexpr uint32_t kEdgeOffset = uint32_t(sizeof(EdgeVector<CoordT>) - sizeof(EdgePoint<CoordT>));
  static constexpr uint32_t kMinEdgeSize = uint32_t(sizeof(EdgeVector<CoordT>) + sizeof(EdgePoint<CoordT>));

  //! \name Edge Storage
  //! \{

  //! Zone memory used to allocate EdgeVector[].
  BLArenaAllocator* _zone;
  //! Edge storage the builder adds edges to.
  EdgeStorage<int>* _storage;

  //! ClipBox already scaled to fixed-point in `double` precision.
  BLBox _clipBoxD;
  //! ClipBox already scaled to fixed-point (integral).
  BLBoxI _clipBoxI;
  //! Curve flattening tolerance
  double _flattenToleranceSq;

  //! \}

  //! \name Shorthands and Working Variables
  //! \{

  //! Bands (shortcut to `_storage->bandEdges()`).
  EdgeList<CoordT>* _bandEdges;
  //! Shift to get bandId from fixed coordinate (shortcut to `_storage->fixedBandHeightShift()`).
  uint32_t _fixedBandHeightShift;
  //! Current point in edge-vector.
  EdgePoint<CoordT>* _ptr;
  //! Last point the builder can go.
  EdgePoint<CoordT>* _end;

  //! Current bounding box, must be flushed.
  BLBoxI _bBoxI;
  double _borderAccX0Y0;
  double _borderAccX0Y1;
  double _borderAccX1Y0;
  double _borderAccX1Y1;

  //! \}

  //! \name State
  //! \{

  //! Working state that is only used during path/poly processing.
  struct State {
    BLPoint a;
    uint32_t aFlags;
    FlattenMonoData flattenData;
  };

  //! \}

  //! \name Appender
  //! \{

  struct Appender {
    BL_INLINE Appender(EdgeBuilder& builder, uint32_t signBit = 0) noexcept
      : _builder(builder),
        _signBit(signBit) {}

    BL_INLINE uint32_t signBit() noexcept { return _signBit; }
    BL_INLINE void setSignBit(uint32_t signBit) noexcept { _signBit = signBit; }

    BL_INLINE BLResult openAt(double x, double y) noexcept {
      int fx = blTruncToInt(x);
      int fy = blTruncToInt(y);

      BL_PROPAGATE(_builder.descendingOpen());
      _builder.descendingAddUnsafe(fx, fy);

      return BL_SUCCESS;
    }

    BL_INLINE BLResult addLine(double x, double y) noexcept {
      int fx = blTruncToInt(x);
      int fy = blTruncToInt(y);

      return _builder.descendingAddChecked(fx, fy, _signBit);
    }

    BL_INLINE BLResult close() noexcept {
      int fy0 = _builder.descendingFirst()->y;
      int fy1 = _builder.descendingLast()->y;

      // Rare but happens, degenerated h-lines make no contribution.
      if (fy0 == fy1) {
        _builder.descendingCancel();
      }
      else {
        _builder._bBoxI.y0 = blMin(_builder._bBoxI.y0, fy0);
        _builder._bBoxI.y1 = blMax(_builder._bBoxI.y1, fy1);
        _builder.descendingClose(_signBit);
      }

      return BL_SUCCESS;
    }

    EdgeBuilder<CoordT>& _builder;
    uint32_t _signBit;
  };

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE EdgeBuilder(BLArenaAllocator* zone, EdgeStorage<int>* storage) noexcept
    : EdgeBuilder(zone, storage, BLBox {}, 0.0) {}

  BL_INLINE EdgeBuilder(BLArenaAllocator* zone, EdgeStorage<int>* storage, const BLBox& clipBox, double toleranceSq) noexcept
    : _zone(zone),
      _storage(storage),
      _clipBoxD(clipBox),
      _clipBoxI(blTruncToInt(clipBox.x0),
                blTruncToInt(clipBox.y0),
                blTruncToInt(clipBox.x1),
                blTruncToInt(clipBox.y1)),
      _flattenToleranceSq(toleranceSq),
      _bandEdges(nullptr),
      _fixedBandHeightShift(0),
      _ptr(nullptr),
      _end(nullptr),
      _bBoxI(BLTraits::maxValue<int>(), BLTraits::maxValue<int>(), BLTraits::minValue<int>(), BLTraits::minValue<int>()),
      _borderAccX0Y0(clipBox.y0),
      _borderAccX0Y1(clipBox.y0),
      _borderAccX1Y0(clipBox.y0),
      _borderAccX1Y1(clipBox.y0) {}

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE void setClipBox(const BLBox& clipBox) noexcept {
    _clipBoxD.reset(clipBox);
    _clipBoxI.reset(blTruncToInt(clipBox.x0),
                    blTruncToInt(clipBox.y0),
                    blTruncToInt(clipBox.x1),
                    blTruncToInt(clipBox.y1));
  }

  BL_INLINE void setFlattenToleranceSq(double toleranceSq) noexcept {
    _flattenToleranceSq = toleranceSq;
  }

  BL_INLINE void mergeBoundingBox() noexcept {
    BLGeometry::bound(_storage->_boundingBox, _bBoxI);
  }

  //! \}

  //! \name Begin & End
  //! \{

  BL_INLINE void begin() noexcept {
    _bandEdges = _storage->bandEdges();
    _fixedBandHeightShift = _storage->fixedBandHeightShift();
    _ptr = nullptr;
    _end = nullptr;
    _bBoxI.reset(BLTraits::maxValue<int>(), BLTraits::maxValue<int>(), BLTraits::minValue<int>(), BLTraits::minValue<int>());
    _borderAccX0Y0 = _clipBoxD.y0;
    _borderAccX0Y1 = _clipBoxD.y0;
    _borderAccX1Y0 = _clipBoxD.y0;
    _borderAccX1Y1 = _clipBoxD.y0;
  }

  BL_INLINE BLResult done() noexcept {
    BL_PROPAGATE(flushBorderAccumulators());
    resetBorderAccumulators();
    mergeBoundingBox();
    return BL_SUCCESS;
  }

  //! \}

  //! \name Begin + Add + End Shortcuts
  //! \{

  //! A convenience function that calls `begin()`, `addPoly()`, and `done()`.
  template<class PointType>
  BL_INLINE BLResult initFromPoly(const PointType* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept {
    begin();
    BL_PROPAGATE(addPoly(pts, size, m, mType));
    return done();
  }

  //! A convenience function that calls `begin()`, `addPath()`, and `done()`.
  BL_INLINE BLResult initFromPath(const BLPathView& view, bool closed, const BLMatrix2D& m, uint32_t mType) noexcept {
    begin();
    BL_PROPAGATE(addPath(view, closed, m, mType));
    return done();
  }

  //! \}

  //! \name Add Geometry
  //! \{

  template<class PointType>
  BL_INLINE BLResult addPoly(const PointType* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept {
    if (mType <= BL_MATRIX2D_TYPE_SCALE)
      return _addPolyScale(pts, size, m);
    else
      return _addPolyAffine(pts, size, m);
  }

  template<class PointType>
  BL_NOINLINE BLResult _addPolyScale(const PointType* pts, size_t size, const BLMatrix2D& m) noexcept {
    EdgeSourcePolyScale<PointType> source(EdgeTransformScale(m), pts, size);
    return addFromSource(source, true);
  }

  template<class PointType>
  BL_NOINLINE BLResult _addPolyAffine(const PointType* pts, size_t size, const BLMatrix2D& m) noexcept {
    EdgeSourcePolyAffine<PointType> source(EdgeTransformAffine(m), pts, size);
    return addFromSource(source, true);
  }

  BL_INLINE BLResult addPath(const BLPathView& view, bool closed, const BLMatrix2D& m, uint32_t mType) noexcept {
    if (mType <= BL_MATRIX2D_TYPE_SCALE)
      return _addPathScale(view, closed, m);
    else
      return _addPathAffine(view, closed, m);
  }

  BL_NOINLINE BLResult _addPathScale(BLPathView view, bool closed, const BLMatrix2D& m) noexcept {
    EdgeSourcePathScale source(EdgeTransformScale(m), view);
    return addFromSource(source, closed);
  }

  BL_NOINLINE BLResult _addPathAffine(BLPathView view, bool closed, const BLMatrix2D& m) noexcept {
    EdgeSourcePathAffine source(EdgeTransformAffine(m), view);
    return addFromSource(source, closed);
  }

  BL_INLINE BLResult addReversePathFromStrokeSink(const BLPathView& view, const BLMatrix2D& m, uint32_t mType) noexcept {
    if (mType <= BL_MATRIX2D_TYPE_SCALE)
      return _addReversePathFromStrokeSinkScale(view, m);
    else
      return _addReversePathFromStrokeSinkAffine(view, m);
  }

  BL_NOINLINE BLResult _addReversePathFromStrokeSinkScale(BLPathView view, const BLMatrix2D& m) noexcept {
    EdgeSourceReversePathFromStrokeSinkScale source(EdgeTransformScale(m), view);
    return addFromSource(source, source.mustClose());
  }

  BL_NOINLINE BLResult _addReversePathFromStrokeSinkAffine(BLPathView view, const BLMatrix2D& m) noexcept {
    EdgeSourceReversePathFromStrokeSinkAffine source(EdgeTransformAffine(m), view);
    return addFromSource(source, source.mustClose());
  }

  template<class Source>
  BL_INLINE BLResult addFromSource(Source& source, bool closed) noexcept {
    State state;
    while (source.begin(state.a)) {
      BLPoint start(state.a);
      BLPoint b;

      bool done = false;
      state.aFlags = blClipCalcXYFlags(state.a, _clipBoxD);

      for (;;) {
        if (source.isLineTo()) {
          source.nextLineTo(b);
LineTo:
          BL_PROPAGATE(lineTo(source, state, b));
          if (done) break;
        }
        else if (source.isQuadTo()) {
          BL_PROPAGATE(quadTo(source, state));
        }
        else if (source.isCubicTo()) {
          BL_PROPAGATE(cubicTo(source, state));
        }
        else {
          b = start;
          done = true;
          if (closed || source.isClose())
            goto LineTo;
          break;
        }
      }
      source.beforeNextBegin();
    }

    return BL_SUCCESS;
  }

  BL_INLINE BLResult addLineSegment(double x0, double y0, double x1, double y1) noexcept {
    int fx0 = blTruncToInt(x0);
    int fy0 = blTruncToInt(y0);
    int fx1 = blTruncToInt(x1);
    int fy1 = blTruncToInt(y1);

    if (fy0 == fy1)
      return BL_SUCCESS;

    if (fy0 < fy1) {
      _bBoxI.y0 = blMin(_bBoxI.y0, fy0);
      _bBoxI.y1 = blMax(_bBoxI.y1, fy1);
      return addClosedLine(fx0, fy0, fx1, fy1, 0);
    }
    else {
      _bBoxI.y0 = blMin(_bBoxI.y0, fy1);
      _bBoxI.y1 = blMax(_bBoxI.y1, fy0);
      return addClosedLine(fx1, fy1, fx0, fy0, 1);
    }
  }

  BL_INLINE BLResult addClosedLine(CoordT x0, CoordT y0, CoordT x1, CoordT y1, uint32_t signBit) noexcept {
    // Must be correct, the rasterizer won't check this.
    BL_ASSERT(y0 < y1);

    EdgeVector<CoordT>* edge = static_cast<EdgeVector<CoordT>*>(_zone->alloc(kMinEdgeSize));
    if (BL_UNLIKELY(!edge))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    edge->pts[0].reset(x0, y0);
    edge->pts[1].reset(x1, y1);
    edge->signBit = signBit;
    edge->count = 2;

    _linkEdge(edge, y0);
    return BL_SUCCESS;
  }

  //! \}

  //! \name Low-Level API - Line To
  //! \{

  // Terminology:
  //
  //   'a' - Line start point.
  //   'b' - List end point.
  //
  //   'd' - Difference between 'b' and 'a'.
  //   'p' - Clipped start point.
  //   'q' - Clipped end point.

  template<class Source>
  BL_INLINE BLResult lineTo(Source& source, State& state, BLPoint b) noexcept {
    BLPoint& a = state.a;
    uint32_t& aFlags = state.aFlags;

    BLPoint p, d;
    uint32_t bFlags;

    // EdgePoint coordinates.
    int fx0, fy0;
    int fx1, fy1;

    do {
      if (!aFlags) {
        // Line - Unclipped
        // ----------------

        bFlags = blClipCalcXYFlags(b, _clipBoxD);
        if (!bFlags) {
          fx0 = blTruncToInt(a.x);
          fy0 = blTruncToInt(a.y);
          fx1 = blTruncToInt(b.x);
          fy1 = blTruncToInt(b.y);

          for (;;) {
            if (fy0 < fy1) {
DescendingLineBegin:
              BL_PROPAGATE(descendingOpen());
              descendingAddUnsafe(fx0, fy0);
              descendingAddUnsafe(fx1, fy1);
              _bBoxI.y0 = blMin(_bBoxI.y0, fy0);

              // Instead of processing one vertex and swapping a/b each time
              // we process two vertices and swap only upon loop termination.
              for (;;) {
DescendingLineLoopA:
                if (!source.maybeNextLineTo(a)) {
                  descendingClose();
                  _bBoxI.y1 = blMax(_bBoxI.y1, fy1);
                  a = b;
                  return BL_SUCCESS;
                }

                bFlags = blClipCalcXYFlags(a, _clipBoxD);
                if (bFlags) {
                  descendingClose();
                  std::swap(a, b);
                  goto BeforeClipEndPoint;
                }

                fx0 = blTruncToInt(a.x);
                fy0 = blTruncToInt(a.y);

                if (fy0 < fy1) {
                  descendingClose();
                  BL_PROPAGATE(ascendingOpen());
                  ascendingAddUnsafe(fx1, fy1);
                  ascendingAddUnsafe(fx0, fy0);
                  _bBoxI.y1 = blMax(_bBoxI.y1, fy1);
                  goto AscendingLineLoopB;
                }
                BL_PROPAGATE(descendingAddChecked(fx0, fy0));

DescendingLineLoopB:
                if (!source.maybeNextLineTo(b)) {
                  descendingClose();
                  _bBoxI.y1 = blMax(_bBoxI.y1, fy0);
                  return BL_SUCCESS;
                }

                bFlags = blClipCalcXYFlags(b, _clipBoxD);
                if (bFlags) {
                  descendingClose();
                  _bBoxI.y1 = blMax(_bBoxI.y1, fy0);
                  goto BeforeClipEndPoint;
                }

                fx1 = blTruncToInt(b.x);
                fy1 = blTruncToInt(b.y);

                if (fy1 < fy0) {
                  descendingClose();
                  BL_PROPAGATE(ascendingOpen());
                  ascendingAddUnsafe(fx0, fy0);
                  ascendingAddUnsafe(fx1, fy1);
                  _bBoxI.y1 = blMax(_bBoxI.y1, fy0);
                  goto AscendingLineLoopA;
                }
                BL_PROPAGATE(descendingAddChecked(fx1, fy1));
              }
              // [NOT REACHED HERE]
            }
            else if (fy0 > fy1) {
AscendingLineBegin:
              BL_PROPAGATE(ascendingOpen());
              ascendingAddUnsafe(fx0, fy0);
              ascendingAddUnsafe(fx1, fy1);
              _bBoxI.y1 = blMax(_bBoxI.y1, fy0);

              // Instead of processing one vertex and swapping a/b each time
              // we process two vertices and swap only upon loop termination.
              for (;;) {
AscendingLineLoopA:
                if (!source.maybeNextLineTo(a)) {
                  ascendingClose();
                  _bBoxI.y0 = blMin(_bBoxI.y0, fy1);
                  a = b;
                  return BL_SUCCESS;
                }

                bFlags = blClipCalcXYFlags(a, _clipBoxD);
                if (bFlags) {
                  ascendingClose();
                  std::swap(a, b);
                  goto BeforeClipEndPoint;
                }

                fx0 = blTruncToInt(a.x);
                fy0 = blTruncToInt(a.y);

                if (fy0 > fy1) {
                  ascendingClose();
                  BL_PROPAGATE(descendingOpen());
                  descendingAddUnsafe(fx1, fy1);
                  descendingAddUnsafe(fx0, fy0);
                  _bBoxI.y0 = blMin(_bBoxI.y0, fy1);
                  goto DescendingLineLoopB;
                }
                BL_PROPAGATE(ascendingAddChecked(fx0, fy0));

AscendingLineLoopB:
                if (!source.maybeNextLineTo(b)) {
                  ascendingClose();
                  _bBoxI.y0 = blMin(_bBoxI.y0, fy0);
                  return BL_SUCCESS;
                }

                bFlags = blClipCalcXYFlags(b, _clipBoxD);
                if (bFlags) {
                  ascendingClose();
                  _bBoxI.y0 = blMin(_bBoxI.y0, fy0);
                  goto BeforeClipEndPoint;
                }

                fx1 = blTruncToInt(b.x);
                fy1 = blTruncToInt(b.y);

                if (fy1 > fy0) {
                  ascendingClose();
                  BL_PROPAGATE(descendingOpen());
                  descendingAddUnsafe(fx0, fy0);
                  descendingAddUnsafe(fx1, fy1);
                  _bBoxI.y0 = blMin(_bBoxI.y0, fy0);
                  goto DescendingLineLoopA;
                }
                BL_PROPAGATE(ascendingAddChecked(fx1, fy1));
              }
              // [NOT REACHED HERE]
            }
            else {
              a = b;
              if (!source.maybeNextLineTo(b))
                return BL_SUCCESS;

              bFlags = blClipCalcXYFlags(b, _clipBoxD);
              if (bFlags) break;

              fx0 = fx1;
              fy0 = fy1;
              fx1 = blTruncToInt(b.x);
              fy1 = blTruncToInt(b.y);
            }
          }
        }

BeforeClipEndPoint:
        p = a;
        d = b - a;
      }
      else {
        // Line - Partically or Completely Clipped
        // ---------------------------------------

        double borY0;
        double borY1;

RestartClipLoop:
        if (aFlags & kClipFlagY0) {
          // Quickly skip all lines that are out of ClipBox or at its border.
          for (;;) {
            if (_clipBoxD.y0 < b.y) break;               // xxxxxxxxxxxxxxxxxxx
            a = b;                                       // .                 .
            if (!source.maybeNextLineTo(b)) {            // .                 .
              aFlags = blClipCalcXFlags(a, _clipBoxD) |  // .                 .
                       blClipCalcY0Flags(a, _clipBoxD);  // .                 .
              return BL_SUCCESS;                         // .                 .
            }                                            // ...................
          }

          // Calculate flags we haven't updated inside the loop.
          aFlags = blClipCalcXFlags(a, _clipBoxD) | blClipCalcY0Flags(a, _clipBoxD);
          bFlags = blClipCalcXFlags(b, _clipBoxD) | blClipCalcY1Flags(b, _clipBoxD);

          borY0 = _clipBoxD.y0;
          uint32_t commonFlags = aFlags & bFlags;

          if (commonFlags) {
            borY1 = blMin(_clipBoxD.y1, b.y);
            if (commonFlags & kClipFlagX0)
              BL_PROPAGATE(accumulateLeftBorder(borY0, borY1));
            else
              BL_PROPAGATE(accumulateRightBorder(borY0, borY1));

            a = b;
            aFlags = bFlags;
            continue;
          }
        }
        else if (aFlags & kClipFlagY1) {
          // Quickly skip all lines that are out of ClipBox or at its border.
          for (;;) {
            if (_clipBoxD.y1 > b.y) break;               // ...................
            a = b;                                       // .                 .
            if (!source.maybeNextLineTo(b)) {            // .                 .
              aFlags = blClipCalcXFlags(a, _clipBoxD) |  // .                 .
                       blClipCalcY1Flags(a, _clipBoxD);  // .                 .
              return BL_SUCCESS;                         // .                 .
            }                                            // xxxxxxxxxxxxxxxxxxx
          }

          // Calculate flags we haven't updated inside the loop.
          aFlags = blClipCalcXFlags(a, _clipBoxD) | blClipCalcY1Flags(a, _clipBoxD);
          bFlags = blClipCalcXFlags(b, _clipBoxD) | blClipCalcY0Flags(b, _clipBoxD);

          borY0 = _clipBoxD.y1;
          uint32_t commonFlags = aFlags & bFlags;

          if (commonFlags) {
            borY1 = blMax(_clipBoxD.y0, b.y);
            if (commonFlags & kClipFlagX0)
              BL_PROPAGATE(accumulateLeftBorder(borY0, borY1));
            else
              BL_PROPAGATE(accumulateRightBorder(borY0, borY1));

            a = b;
            aFlags = bFlags;
            continue;
          }
        }
        else if (aFlags & kClipFlagX0) {
          borY0 = blClamp(a.y, _clipBoxD.y0, _clipBoxD.y1);

          // Quickly skip all lines that are out of ClipBox or at its border.
          for (;;) {                                     // x..................
            if (_clipBoxD.x0 < b.x) break;               // x                 .
            a = b;                                       // x                 .
            if (!source.maybeNextLineTo(b)) {            // x                 .
              aFlags = blClipCalcYFlags(a, _clipBoxD) |  // x                 .
                       blClipCalcX0Flags(a, _clipBoxD);  // x..................
              borY1 = blClamp(a.y, _clipBoxD.y0, _clipBoxD.y1);
              if (borY0 != borY1)
                BL_PROPAGATE(accumulateLeftBorder(borY0, borY1));
              return BL_SUCCESS;
            }
          }

          borY1 = blClamp(a.y, _clipBoxD.y0, _clipBoxD.y1);
          if (borY0 != borY1)
            BL_PROPAGATE(accumulateLeftBorder(borY0, borY1));

          // Calculate flags we haven't updated inside the loop.
          aFlags = blClipCalcX0Flags(a, _clipBoxD) | blClipCalcYFlags(a, _clipBoxD);
          bFlags = blClipCalcX1Flags(b, _clipBoxD) | blClipCalcYFlags(b, _clipBoxD);

          if (aFlags & bFlags)
            goto RestartClipLoop;

          borY0 = borY1;
        }
        else {
          borY0 = blClamp(a.y, _clipBoxD.y0, _clipBoxD.y1);

          // Quickly skip all lines that are out of ClipBox or at its border.
          for (;;) {                                     // ..................x
            if (_clipBoxD.x1 > b.x) break;               // .                 x
            a = b;                                       // .                 x
            if (!source.maybeNextLineTo(b)) {            // .                 x
              aFlags = blClipCalcYFlags(a, _clipBoxD) |  // .                 x
                       blClipCalcX1Flags(a, _clipBoxD);  // ..................x
              borY1 = blClamp(a.y, _clipBoxD.y0, _clipBoxD.y1);
              if (borY0 != borY1)
                BL_PROPAGATE(accumulateRightBorder(borY0, borY1));
              return BL_SUCCESS;
            }
          }

          borY1 = blClamp(a.y, _clipBoxD.y0, _clipBoxD.y1);
          if (borY0 != borY1)
            BL_PROPAGATE(accumulateRightBorder(borY0, borY1));

          // Calculate flags we haven't updated inside the loop.
          aFlags = blClipCalcX1Flags(a, _clipBoxD) | blClipCalcYFlags(a, _clipBoxD);
          bFlags = blClipCalcX0Flags(b, _clipBoxD) | blClipCalcYFlags(b, _clipBoxD);

          if (aFlags & bFlags)
            goto RestartClipLoop;

          borY0 = borY1;
        }

        // Line - Clip Start Point
        // -----------------------

        // The start point of the line requires clipping.
        d = b - a;
        p.x = _clipBoxD.x1;
        p.y = _clipBoxD.y1;

        switch (aFlags) {
          case kClipFlagNone:
            p = a;
            break;

          case kClipFlagX0Y0:
            p.x = _clipBoxD.x0;
            BL_FALLTHROUGH

          case kClipFlagX1Y0:
            p.y = a.y + (p.x - a.x) * d.y / d.x;
            aFlags = blClipCalcYFlags(p, _clipBoxD);

            if (p.y >= _clipBoxD.y0)
              break;
            BL_FALLTHROUGH

          case kClipFlagY0:
            p.y = _clipBoxD.y0;
            p.x = a.x + (p.y - a.y) * d.x / d.y;

            aFlags = blClipCalcXFlags(p, _clipBoxD);
            break;

          case kClipFlagX0Y1:
            p.x = _clipBoxD.x0;
            BL_FALLTHROUGH

          case kClipFlagX1Y1:
            p.y = a.y + (p.x - a.x) * d.y / d.x;
            aFlags = blClipCalcYFlags(p, _clipBoxD);

            if (p.y <= _clipBoxD.y1)
              break;
            BL_FALLTHROUGH

          case kClipFlagY1:
            p.y = _clipBoxD.y1;
            p.x = a.x + (p.y - a.y) * d.x / d.y;

            aFlags = blClipCalcXFlags(p, _clipBoxD);
            break;

          case kClipFlagX0:
            p.x = _clipBoxD.x0;
            BL_FALLTHROUGH

          case kClipFlagX1:
            p.y = a.y + (p.x - a.x) * d.y / d.x;

            aFlags = blClipCalcYFlags(p, _clipBoxD);
            break;

          default:
            // Possibly caused by NaNs.
            return blTraceError(BL_ERROR_INVALID_GEOMETRY);
        }

        if (aFlags) {
          borY1 = blClamp(b.y, _clipBoxD.y0, _clipBoxD.y1);
          if (p.x <= _clipBoxD.x0)
            BL_PROPAGATE(accumulateLeftBorder(borY0, borY1));
          else if (p.x >= _clipBoxD.x1)
            BL_PROPAGATE(accumulateRightBorder(borY0, borY1));

          a = b;
          aFlags = bFlags;
          continue;
        }

        borY1 = blClamp(p.y, _clipBoxD.y0, _clipBoxD.y1);
        if (borY0 != borY1) {
          if (p.x <= _clipBoxD.x0)
            BL_PROPAGATE(accumulateLeftBorder(borY0, borY1));
          else
            BL_PROPAGATE(accumulateRightBorder(borY0, borY1));
        }

        if (!bFlags) {
          a = b;
          aFlags = 0;

          fx0 = blTruncToInt(p.x);
          fy0 = blTruncToInt(p.y);
          fx1 = blTruncToInt(b.x);
          fy1 = blTruncToInt(b.y);

          if (fy0 == fy1)
            continue;

          if (fy0 < fy1)
            goto DescendingLineBegin;
          else
            goto AscendingLineBegin;
        }
      }

      {
        // Line - Clip End Point
        // ---------------------

        BLPoint q(_clipBoxD.x1, _clipBoxD.y1);

        BL_ASSERT(bFlags != 0);
        switch (bFlags) {
          case kClipFlagX0Y0:
            q.x = _clipBoxD.x0;
            BL_FALLTHROUGH

          case kClipFlagX1Y0:
            q.y = a.y + (q.x - a.x) * d.y / d.x;
            if (q.y >= _clipBoxD.y0)
              break;
            BL_FALLTHROUGH

          case kClipFlagY0:
            q.y = _clipBoxD.y0;
            q.x = a.x + (q.y - a.y) * d.x / d.y;
            break;

          case kClipFlagX0Y1:
            q.x = _clipBoxD.x0;
            BL_FALLTHROUGH

          case kClipFlagX1Y1:
            q.y = a.y + (q.x - a.x) * d.y / d.x;
            if (q.y <= _clipBoxD.y1)
              break;
            BL_FALLTHROUGH

          case kClipFlagY1:
            q.y = _clipBoxD.y1;
            q.x = a.x + (q.y - a.y) * d.x / d.y;
            break;

          case kClipFlagX0:
            q.x = _clipBoxD.x0;
            BL_FALLTHROUGH

          case kClipFlagX1:
            q.y = a.y + (q.x - a.x) * d.y / d.x;
            break;

          default:
            // Possibly caused by NaNs.
            return blTraceError(BL_ERROR_INVALID_GEOMETRY);
        }

        BL_PROPAGATE(addLineSegment(p.x, p.y, q.x, q.y));
        double clippedBY = blClamp(b.y, _clipBoxD.y0, _clipBoxD.y1);

        if (q.y != clippedBY) {
          if (q.x == _clipBoxD.x0)
            BL_PROPAGATE(accumulateLeftBorder(q.y, clippedBY));
          else
            BL_PROPAGATE(accumulateRightBorder(q.y, clippedBY));
        }
      }

      a = b;
      aFlags = bFlags;
    } while (source.maybeNextLineTo(b));

    return BL_SUCCESS;
  }

  //! \}

  //! \name Low-Level API - Quad To
  //! \{

  // Terminology:
  //
  //   'p0' - Quad start point.
  //   'p1' - Quad control point.
  //   'p2' - Quad end point.

  template<class Source>
  BL_INLINE_IF_NOT_DEBUG BLResult quadTo(Source& source, State& state) noexcept {
    // 2 extremas and 1 terminating `1.0` value.
    constexpr uint32_t kMaxTCount = 2 + 1;

    BLPoint spline[kMaxTCount * 2 + 1];
    BLPoint& p0 = state.a;
    BLPoint& p1 = spline[1];
    BLPoint& p2 = spline[2];

    uint32_t& p0Flags = state.aFlags;
    source.nextQuadTo(p1, p2);

    for (;;) {
      uint32_t p1Flags = blClipCalcXYFlags(p1, _clipBoxD);
      uint32_t p2Flags = blClipCalcXYFlags(p2, _clipBoxD);
      uint32_t commonFlags = p0Flags & p1Flags & p2Flags;

      // Fast reject.
      if (commonFlags) {
        uint32_t end = 0;

        if (commonFlags & kClipFlagY0) {
          // CLIPPED OUT: Above top (fast).
          for (;;) {
            p0 = p2;
            end = !source.isQuadTo();
            if (end) break;

            source.nextQuadTo(p1, p2);
            if (!((p1.y <= _clipBoxD.y0) & (p2.y <= _clipBoxD.y0)))
              break;
          }
        }
        else if (commonFlags & kClipFlagY1) {
          // CLIPPED OUT: Below bottom (fast).
          for (;;) {
            p0 = p2;
            end = !source.isQuadTo();
            if (end) break;

            source.nextQuadTo(p1, p2);
            if (!((p1.y >= _clipBoxD.y1) & (p2.y >= _clipBoxD.y1)))
              break;
          }
        }
        else {
          // CLIPPED OUT: Before left or after right (border-line required).
          double y0 = blClamp(p0.y, _clipBoxD.y0, _clipBoxD.y1);

          if (commonFlags & kClipFlagX0) {
            for (;;) {
              p0 = p2;
              end = !source.isQuadTo();
              if (end) break;

              source.nextQuadTo(p1, p2);
              if (!((p1.x <= _clipBoxD.x0) & (p2.x <= _clipBoxD.x0)))
                break;
            }

            double y1 = blClamp(p0.y, _clipBoxD.y0, _clipBoxD.y1);
            BL_PROPAGATE(accumulateLeftBorder(y0, y1));
          }
          else {
            for (;;) {
              p0 = p2;
              end = !source.isQuadTo();
              if (end) break;

              source.nextQuadTo(p1, p2);
              if (!((p1.x >= _clipBoxD.x1) & (p2.x >= _clipBoxD.x1)))
                break;
            }

            double y1 = blClamp(p0.y, _clipBoxD.y0, _clipBoxD.y1);
            BL_PROPAGATE(accumulateRightBorder(y0, y1));
          }
        }

        p0Flags = blClipCalcXYFlags(p0, _clipBoxD);
        if (end)
          return BL_SUCCESS;
        continue;
      }

      spline[0] = p0;

      BLPoint* splinePtr = spline;
      BLPoint* splineEnd = BLGeometry::splitQuadToSpline<BLGeometry::SplitQuadOptions::kExtremas>(spline, splinePtr);

      if (splineEnd == splinePtr)
        splineEnd = splinePtr + 2;

      Appender appender(*this);
      FlattenMonoQuad monoCurve(state.flattenData, _flattenToleranceSq);

      uint32_t anyFlags = p0Flags | p1Flags | p2Flags;
      if (anyFlags) {
        // One or more quad may need clipping.
        do {
          uint32_t signBit = splinePtr[0].y > splinePtr[2].y;
          BL_PROPAGATE(
            flattenUnsafeMonoCurve<FlattenMonoQuad>(monoCurve, appender, splinePtr, signBit)
          );
        } while ((splinePtr += 2) != splineEnd);

        p0 = splineEnd[0];
        p0Flags = p2Flags;
      }
      else {
        // No clipping - optimized fast-path.
        do {
          uint32_t signBit = splinePtr[0].y > splinePtr[2].y;
          BL_PROPAGATE(
            flattenSafeMonoCurve<FlattenMonoQuad>(monoCurve, appender, splinePtr, signBit)
          );
        } while ((splinePtr += 2) != splineEnd);

        p0 = splineEnd[0];
      }

      if (!source.maybeNextQuadTo(p1, p2))
        return BL_SUCCESS;
    }
  }

  //! \}

  //! \name Low-Level API - Cubic To
  //! \{

  // Terminology:
  //
  //   'p0' - Cubic start point.
  //   'p1' - Cubic first control point.
  //   'p2' - Cubic second control point.
  //   'p3' - Cubic end point.

  template<class Source>
  BL_INLINE_IF_NOT_DEBUG BLResult cubicTo(Source& source, State& state) noexcept {
    // 4 extremas, 2 inflections, 1 cusp, and 1 terminating `1.0` value.
    constexpr uint32_t kMaxTCount = 4 + 2 + 1 + 1;

    BLPoint spline[kMaxTCount * 3 + 1];
    BLPoint& p0 = state.a;
    BLPoint& p1 = spline[1];
    BLPoint& p2 = spline[2];
    BLPoint& p3 = spline[3];

    uint32_t& p0Flags = state.aFlags;
    source.nextCubicTo(p1, p2, p3);

    for (;;) {
      uint32_t p1Flags = blClipCalcXYFlags(p1, _clipBoxD);
      uint32_t p2Flags = blClipCalcXYFlags(p2, _clipBoxD);
      uint32_t p3Flags = blClipCalcXYFlags(p3, _clipBoxD);
      uint32_t commonFlags = p0Flags & p1Flags & p2Flags & p3Flags;

      // Fast reject.
      if (commonFlags) {
        uint32_t end = 0;

        if (commonFlags & kClipFlagY0) {
          // CLIPPED OUT: Above top (fast).
          for (;;) {
            p0 = p3;
            end = !source.isCubicTo();
            if (end) break;

            source.nextCubicTo(p1, p2, p3);
            if (!((p1.y <= _clipBoxD.y0) & (p2.y <= _clipBoxD.y0) & (p3.y <= _clipBoxD.y0)))
              break;
          }
        }
        else if (commonFlags & kClipFlagY1) {
          // CLIPPED OUT: Below bottom (fast).
          for (;;) {
            p0 = p3;
            end = !source.isCubicTo();
            if (end) break;

            source.nextCubicTo(p1, p2, p3);
            if (!((p1.y >= _clipBoxD.y1) & (p2.y >= _clipBoxD.y1) & (p3.y >= _clipBoxD.y1)))
              break;
          }
        }
        else {
          // CLIPPED OUT: Before left or after right (border-line required).
          double y0 = blClamp(p0.y, _clipBoxD.y0, _clipBoxD.y1);

          if (commonFlags & kClipFlagX0) {
            for (;;) {
              p0 = p3;
              end = !source.isCubicTo();
              if (end) break;

              source.nextCubicTo(p1, p2, p3);
              if (!((p1.x <= _clipBoxD.x0) & (p2.x <= _clipBoxD.x0) & (p3.x <= _clipBoxD.x0)))
                break;
            }

            double y1 = blClamp(p0.y, _clipBoxD.y0, _clipBoxD.y1);
            BL_PROPAGATE(accumulateLeftBorder(y0, y1));
          }
          else {
            for (;;) {
              p0 = p3;
              end = !source.isCubicTo();
              if (end) break;

              source.nextCubicTo(p1, p2, p3);
              if (!((p1.x >= _clipBoxD.x1) & (p2.x >= _clipBoxD.x1) & (p3.x >= _clipBoxD.x1)))
                break;
            }

            double y1 = blClamp(p0.y, _clipBoxD.y0, _clipBoxD.y1);
            BL_PROPAGATE(accumulateRightBorder(y0, y1));
          }
        }

        p0Flags = blClipCalcXYFlags(p0, _clipBoxD);
        if (end)
          return BL_SUCCESS;
        continue;
      }

      spline[0] = p0;

      BLPoint* splinePtr = spline;
      BLPoint* splineEnd = BLGeometry::splitCubicToSpline<BLGeometry::SplitCubicOptions::kExtremasInflectionsCusp>(spline, splinePtr);

      if (splineEnd == splinePtr)
        splineEnd += 3;

      Appender appender(*this);
      FlattenMonoCubic monoCurve(state.flattenData, _flattenToleranceSq);

      uint32_t anyFlags = p0Flags | p1Flags | p2Flags | p3Flags;
      if (anyFlags) {
        // One or more cubic may need clipping.
        do {
          uint32_t signBit = splinePtr[0].y > splinePtr[3].y;
          BL_PROPAGATE(
            flattenUnsafeMonoCurve<FlattenMonoCubic>(monoCurve, appender, splinePtr, signBit)
          );
        } while ((splinePtr += 3) != splineEnd);

        p0 = splineEnd[0];
        p0Flags = p3Flags;
      }
      else {
        // No clipping - optimized fast-path.
        do {
          uint32_t signBit = splinePtr[0].y > splinePtr[3].y;
          BL_PROPAGATE(
            flattenSafeMonoCurve<FlattenMonoCubic>(monoCurve, appender, splinePtr, signBit)
          );
        } while ((splinePtr += 3) != splineEnd);

        p0 = splineEnd[0];
      }

      if (!source.maybeNextCubicTo(p1, p2, p3))
        return BL_SUCCESS;
    }
  }

  //! \}

  //! \name Curve Helpers
  //! \{

  template<typename MonoCurveT>
  BL_INLINE BLResult flattenSafeMonoCurve(MonoCurveT& monoCurve, Appender& appender, const BLPoint* src, uint32_t signBit) noexcept {
    monoCurve.begin(src, signBit);
    appender.setSignBit(signBit);

    if (monoCurve.isLeftToRight())
      monoCurve.boundLeftToRight();
    else
      monoCurve.boundRightToLeft();

    BL_PROPAGATE(appender.openAt(monoCurve.first().x, monoCurve.first().y));
    for (;;) {
      typename MonoCurveT::SplitStep step;
      if (!monoCurve.isFlat(step)) {
        if (monoCurve.canPush()) {
          monoCurve.split(step);
          monoCurve.push(step);
          continue;
        }
        else {
          // The curve is either invalid or the tolerance is too strict. We shouldn't get INF nor NaNs here as
          //  we know we are within the clipBox.
          BL_ASSERT(step.isFinite());
        }
      }

      BL_PROPAGATE(appender.addLine(monoCurve.last().x, monoCurve.last().y));
      if (!monoCurve.canPop())
        break;
      monoCurve.pop();
    }

    appender.close();
    return BL_SUCCESS;
  }

  // Clips and flattens a monotonic curve - works for both quadratics and cubics.
  //
  // The idea behind this function is to quickly subdivide to find the intersection with ClipBox. When the
  // intersection is found the intersecting line is clipped and the subdivision continues until the end of
  // the curve or until another intersection is found, which would be the end of the curve. The algorithm
  // handles all cases and accumulates border lines when necessary.
  template<typename MonoCurveT>
  BL_INLINE BLResult flattenUnsafeMonoCurve(MonoCurveT& monoCurve, Appender& appender, const BLPoint* src, uint32_t signBit) noexcept {
    monoCurve.begin(src, signBit);
    appender.setSignBit(signBit);

    double yStart = monoCurve.first().y;
    double yEnd = blMin(monoCurve.last().y, _clipBoxD.y1);

    if ((yStart >= yEnd) | (yEnd <= _clipBoxD.y0))
      return BL_SUCCESS;

    // This delta should be okay even for 16-bit AA.
    double kDeltaLimit = 0.00001;
    double xDelta = blAbs(monoCurve.first().x - monoCurve.last().x);

    uint32_t completelyOut = 0;
    typename MonoCurveT::SplitStep step;

    if (xDelta < kDeltaLimit) {
      // Straight-Line
      // -------------

      yStart = blMax(yStart, _clipBoxD.y0);

      double xMin = blMin(monoCurve.first().x, monoCurve.last().x);
      double xMax = blMax(monoCurve.first().x, monoCurve.last().x);

      if (xMax <= _clipBoxD.x0) {
        BL_PROPAGATE(accumulateLeftBorder(yStart, yEnd, signBit));
      }
      else if (xMin >= _clipBoxD.x1) {
        BL_PROPAGATE(accumulateRightBorder(yStart, yEnd, signBit));
      }
      else {
        BL_PROPAGATE(appender.openAt(monoCurve.first().x, yStart));
        BL_PROPAGATE(appender.addLine(monoCurve.last().x, yEnd));
        BL_PROPAGATE(appender.close());
      }

      return BL_SUCCESS;
    }
    else if (monoCurve.isLeftToRight()) {
      // Left-To-Right
      // ------------>
      //
      //  ...__
      //       --._
      //           *_
      monoCurve.boundLeftToRight();

      if (yStart < _clipBoxD.y0) {
        yStart = _clipBoxD.y0;
        for (;;) {
          // CLIPPED OUT: Above ClipBox.y0
          // -----------------------------

          completelyOut = (monoCurve.first().x >= _clipBoxD.x1);
          if (completelyOut)
            break;

          if (!monoCurve.isFlat(step)) {
            monoCurve.split(step);

            if (step.midPoint().y <= _clipBoxD.y0) {
              monoCurve.discardAndAdvance(step);
              continue;
            }

            if (monoCurve.canPush()) {
              monoCurve.push(step);
              continue;
            }
          }

          if (monoCurve.last().y > _clipBoxD.y0) {
            // The `completelyOut` value will only be used if there is no
            // curve to be popped from the stack. In that case it's important
            // to be `1` as we have to accumulate the border.
            completelyOut = monoCurve.last().x < _clipBoxD.x0;
            if (completelyOut)
              goto LeftToRight_BeforeX0_Pop;

            double xClipped = monoCurve.first().x + (_clipBoxD.y0 - monoCurve.first().y) * dx_div_dy(monoCurve.last() - monoCurve.first());
            if (xClipped <= _clipBoxD.x0)
              goto LeftToRight_BeforeX0_Clip;

            completelyOut = (xClipped >= _clipBoxD.x1);
            if (completelyOut)
              break;

            BL_PROPAGATE(appender.openAt(xClipped, _clipBoxD.y0));
            goto LeftToRight_AddLine;
          }

          if (!monoCurve.canPop())
            break;

          monoCurve.pop();
        }
        completelyOut <<= kClipShiftX1;
      }
      else if (yStart < _clipBoxD.y1) {
        if (monoCurve.first().x < _clipBoxD.x0) {
          // CLIPPED OUT: Before ClipBox.x0
          // ------------------------------

          for (;;) {
            completelyOut = (monoCurve.first().y >= _clipBoxD.y1);
            if (completelyOut)
              break;

            if (!monoCurve.isFlat(step)) {
              monoCurve.split(step);

              if (step.midPoint().x <= _clipBoxD.x0) {
                monoCurve.discardAndAdvance(step);
                continue;
              }

              if (monoCurve.canPush()) {
                monoCurve.push(step);
                continue;
              }
            }

            if (monoCurve.last().x > _clipBoxD.x0) {
LeftToRight_BeforeX0_Clip:
              double yClipped = monoCurve.first().y + (_clipBoxD.x0 - monoCurve.first().x) * dy_div_dx(monoCurve.last() - monoCurve.first());
              completelyOut = (yClipped >= yEnd);

              if (completelyOut)
                break;

              if (yStart < yClipped)
                BL_PROPAGATE(accumulateLeftBorder(yStart, yClipped, signBit));

              BL_PROPAGATE(appender.openAt(_clipBoxD.x0, yClipped));
              goto LeftToRight_AddLine;
            }

            completelyOut = (monoCurve.last().y >= yEnd);
            if (completelyOut)
              break;

LeftToRight_BeforeX0_Pop:
            if (!monoCurve.canPop())
              break;

            monoCurve.pop();
          }
          completelyOut <<= kClipShiftX0;
        }
        else if (monoCurve.first().x < _clipBoxD.x1) {
          // VISIBLE CASE
          // ------------

          BL_PROPAGATE(appender.openAt(monoCurve.first().x, monoCurve.first().y));
          for (;;) {
            if (!monoCurve.isFlat(step)) {
              monoCurve.split(step);

              if (monoCurve.canPush()) {
                monoCurve.push(step);
                continue;
              }
            }

LeftToRight_AddLine:
            completelyOut = monoCurve.last().x > _clipBoxD.x1;
            if (completelyOut) {
              double yClipped = monoCurve.first().y + (_clipBoxD.x1 - monoCurve.first().x) * dy_div_dx(monoCurve.last() - monoCurve.first());
              if (yClipped <= yEnd) {
                yStart = yClipped;
                BL_PROPAGATE(appender.addLine(_clipBoxD.x1, yClipped));
                break;
              }
            }

            completelyOut = monoCurve.last().y >= _clipBoxD.y1;
            if (completelyOut) {
              double xClipped = blMin(monoCurve.first().x + (_clipBoxD.y1 - monoCurve.first().y) * dx_div_dy(monoCurve.last() - monoCurve.first()), _clipBoxD.x1);
              BL_PROPAGATE(appender.addLine(xClipped, _clipBoxD.y1));

              completelyOut = 0;
              break;
            }

            BL_PROPAGATE(appender.addLine(monoCurve.last().x, monoCurve.last().y));
            if (!monoCurve.canPop())
              break;
            monoCurve.pop();
          }
          appender.close();
          completelyOut <<= kClipShiftX1;
        }
        else {
          completelyOut = kClipFlagX1;
        }
      }
      else {
        // Below bottom or invalid, ignore this part...
      }
    }
    else {
      // Right-To-Left
      // <------------
      //
      //        __...
      //    _.--
      //  _*
      monoCurve.boundRightToLeft();

      if (yStart < _clipBoxD.y0) {
        yStart = _clipBoxD.y0;
        for (;;) {
          // CLIPPED OUT: Above ClipBox.y0
          // -----------------------------

          completelyOut = (monoCurve.first().x <= _clipBoxD.x0);
          if (completelyOut)
            break;

          if (!monoCurve.isFlat(step)) {
            monoCurve.split(step);

            if (step.midPoint().y <= _clipBoxD.y0) {
              monoCurve.discardAndAdvance(step);
              continue;
            }

            if (monoCurve.canPush()) {
              monoCurve.push(step);
              continue;
            }
          }

          if (monoCurve.last().y > _clipBoxD.y0) {
            // The `completelyOut` value will only be used if there is no
            // curve to be popped from the stack. In that case it's important
            // to be `1` as we have to accumulate the border.
            completelyOut = monoCurve.last().x > _clipBoxD.x1;
            if (completelyOut)
              goto RightToLeft_AfterX1_Pop;

            double xClipped = monoCurve.first().x + (_clipBoxD.y0 - monoCurve.first().y) * dx_div_dy(monoCurve.last() - monoCurve.first());
            if (xClipped >= _clipBoxD.x1)
              goto RightToLeft_AfterX1_Clip;

            completelyOut = (xClipped <= _clipBoxD.x0);
            if (completelyOut)
              break;

            BL_PROPAGATE(appender.openAt(xClipped, _clipBoxD.y0));
            goto RightToLeft_AddLine;
          }

          if (!monoCurve.canPop())
            break;

          monoCurve.pop();
        }
        completelyOut <<= kClipShiftX0;
      }
      else if (yStart < _clipBoxD.y1) {
        if (monoCurve.first().x > _clipBoxD.x1) {
          // CLIPPED OUT: After ClipBox.x1
          // ------------------------------

          for (;;) {
            completelyOut = (monoCurve.first().y >= _clipBoxD.y1);
            if (completelyOut)
              break;

            if (!monoCurve.isFlat(step)) {
              monoCurve.split(step);

              if (step.midPoint().x >= _clipBoxD.x1) {
                monoCurve.discardAndAdvance(step);
                continue;
              }

              if (monoCurve.canPush()) {
                monoCurve.push(step);
                continue;
              }
            }

            if (monoCurve.last().x < _clipBoxD.x1) {
RightToLeft_AfterX1_Clip:
              double yClipped = monoCurve.first().y + (_clipBoxD.x1 - monoCurve.first().x) * dy_div_dx(monoCurve.last() - monoCurve.first());
              completelyOut = (yClipped >= yEnd);

              if (completelyOut)
                break;

              if (yStart < yClipped)
                BL_PROPAGATE(accumulateRightBorder(yStart, yClipped, signBit));

              BL_PROPAGATE(appender.openAt(_clipBoxD.x1, yClipped));
              goto RightToLeft_AddLine;
            }

            completelyOut = (monoCurve.last().y >= yEnd);
            if (completelyOut)
              break;

RightToLeft_AfterX1_Pop:
            if (!monoCurve.canPop())
              break;

            monoCurve.pop();
          }
          completelyOut <<= kClipShiftX1;
        }
        else if (monoCurve.first().x > _clipBoxD.x0) {
          // VISIBLE CASE
          // ------------

          BL_PROPAGATE(appender.openAt(monoCurve.first().x, monoCurve.first().y));
          for (;;) {
            if (!monoCurve.isFlat(step)) {
              monoCurve.split(step);

              if (monoCurve.canPush()) {
                monoCurve.push(step);
                continue;
              }
            }

RightToLeft_AddLine:
            completelyOut = monoCurve.last().x < _clipBoxD.x0;
            if (completelyOut) {
              double yClipped = monoCurve.first().y + (_clipBoxD.x0 - monoCurve.first().x) * dy_div_dx(monoCurve.last() - monoCurve.first());
              if (yClipped <= yEnd) {
                yStart = yClipped;
                BL_PROPAGATE(appender.addLine(_clipBoxD.x0, yClipped));
                break;
              }
            }

            completelyOut = monoCurve.last().y >= _clipBoxD.y1;
            if (completelyOut) {
              double xClipped = blMax(monoCurve.first().x + (_clipBoxD.y1 - monoCurve.first().y) * dx_div_dy(monoCurve.last() - monoCurve.first()), _clipBoxD.x0);
              BL_PROPAGATE(appender.addLine(xClipped, _clipBoxD.y1));

              completelyOut = 0;
              break;
            }

            BL_PROPAGATE(appender.addLine(monoCurve.last().x, monoCurve.last().y));
            if (!monoCurve.canPop())
              break;
            monoCurve.pop();
          }
          appender.close();
          completelyOut <<= kClipShiftX0;
        }
        else {
          completelyOut = kClipFlagX0;
        }
      }
      else {
        // Below bottom or invalid, ignore this part...
      }
    }

    if (completelyOut && yStart < yEnd) {
      if (completelyOut & kClipFlagX0)
        BL_PROPAGATE(accumulateLeftBorder(yStart, yEnd, signBit));
      else
        BL_PROPAGATE(accumulateRightBorder(yStart, yEnd, signBit));
    }

    return BL_SUCCESS;
  }

  //! \}

  //! \name Raw Edge Building
  //! \{

  BL_INLINE bool hasSpaceInEdgeVector() const noexcept { return _ptr != _end; }

  BL_INLINE BLResult ascendingOpen() noexcept {
    BL_PROPAGATE(_zone->ensure(kMinEdgeSize));
    _ptr = _zone->end<EdgePoint<CoordT>>();
    _end = _zone->ptr<EdgeVector<CoordT>>()->pts;
    return BL_SUCCESS;
  }

  BL_INLINE void ascendingAddUnsafe(CoordT x, CoordT y) noexcept {
    BL_ASSERT(hasSpaceInEdgeVector());
    _ptr--;
    _ptr->reset(x, y);
  }

  BL_INLINE BLResult ascendingAddChecked(CoordT x, CoordT y, uint32_t signBit = 1) noexcept {
    if (BL_UNLIKELY(!hasSpaceInEdgeVector())) {
      const EdgePoint<CoordT>* last = ascendingLast();
      ascendingClose(signBit);
      BL_PROPAGATE(ascendingOpen());
      _ptr--;
      _ptr->reset(last->x, last->y);
    }

    _ptr--;
    _ptr->reset(x, y);
    return BL_SUCCESS;
  }

  BL_INLINE void ascendingClose(uint32_t signBit = 1) noexcept {
    EdgeVector<CoordT>* edge = reinterpret_cast<EdgeVector<CoordT>*>(reinterpret_cast<uint8_t*>(_ptr) - kEdgeOffset);
    edge->signBit = signBit;
    edge->count = (size_t)(_zone->end<EdgePoint<CoordT>>() - _ptr);

    _zone->setEnd(edge);
    _linkEdge(edge, _ptr[0].y);
  }

  BL_INLINE EdgePoint<CoordT>* ascendingLast() const noexcept { return _ptr; }

  BL_INLINE BLResult descendingOpen() noexcept {
    BL_PROPAGATE(_zone->ensure(kMinEdgeSize));
    _ptr = _zone->ptr<EdgeVector<CoordT>>()->pts;
    _end = _zone->end<EdgePoint<CoordT>>();
    return BL_SUCCESS;
  }

  BL_INLINE void descendingAddUnsafe(CoordT x, CoordT y) noexcept {
    BL_ASSERT(hasSpaceInEdgeVector());
    _ptr->reset(x, y);
    _ptr++;
  }

  BL_INLINE BLResult descendingAddChecked(CoordT x, CoordT y, uint32_t signBit = 0) noexcept {
    BL_ASSERT(_zone->ptr<EdgeVector<CoordT>>()->pts == _ptr || _ptr[-1].y <= y);

    if (BL_UNLIKELY(!hasSpaceInEdgeVector())) {
      const EdgePoint<CoordT>* last = descendingLast();
      descendingClose(signBit);
      BL_PROPAGATE(descendingOpen());
      _ptr->reset(last->x, last->y);
      _ptr++;
    }

    _ptr->reset(x, y);
    _ptr++;
    return BL_SUCCESS;
  }

  BL_INLINE void descendingClose(uint32_t signBit = 0) noexcept {
    EdgeVector<CoordT>* edge = _zone->ptr<EdgeVector<CoordT>>();
    edge->signBit = signBit;
    edge->count = (size_t)(_ptr - edge->pts);

    _zone->setPtr(_ptr);
    _linkEdge(edge, edge->pts[0].y);
  }

  BL_INLINE void descendingCancel() noexcept {
    // Nothing needed here...
  }

  BL_INLINE EdgePoint<CoordT>* descendingFirst() const noexcept { return _zone->ptr<EdgeVector<CoordT>>()->pts; };
  BL_INLINE EdgePoint<CoordT>* descendingLast() const noexcept { return _ptr - 1; }

  BL_INLINE void _linkEdge(EdgeVector<CoordT>* edge, int y0) noexcept {
    size_t bandId = size_t(unsigned(y0) >> _fixedBandHeightShift);
    BL_ASSERT(bandId < _storage->bandCount());
    _bandEdges[bandId].append(edge);
  }

  //! \}

  //! \name Border Accumulation
  //! \{

  BL_INLINE void resetBorderAccumulators() noexcept {
    _borderAccX0Y0 = _borderAccX0Y1;
    _borderAccX1Y0 = _borderAccX1Y1;
  }

  BL_INLINE BLResult flushBorderAccumulators() noexcept {
    return _emitLeftBorder() |
           _emitRightBorder();
  }

  BL_INLINE BLResult accumulateLeftBorder(double y0, double y1) noexcept {
    if (_borderAccX0Y1 == y0) {
      _borderAccX0Y1 = y1;
      return BL_SUCCESS;
    }

    BL_PROPAGATE(_emitLeftBorder());
    _borderAccX0Y0 = y0;
    _borderAccX0Y1 = y1;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult accumulateLeftBorder(double y0, double y1, uint32_t signBit) noexcept {
    if (signBit != 0)
      std::swap(y0, y1);
    return accumulateLeftBorder(y0, y1);
  }

  BL_INLINE BLResult accumulateRightBorder(double y0, double y1) noexcept {
    if (_borderAccX1Y1 == y0) {
      _borderAccX1Y1 = y1;
      return BL_SUCCESS;
    }

    BL_PROPAGATE(_emitRightBorder());
    _borderAccX1Y0 = y0;
    _borderAccX1Y1 = y1;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult accumulateRightBorder(double y0, double y1, uint32_t signBit) noexcept {
    if (signBit != 0)
      std::swap(y0, y1);
    return accumulateRightBorder(y0, y1);
  }

  BL_INLINE BLResult _emitLeftBorder() noexcept {
    int accY0 = blTruncToInt(_borderAccX0Y0);
    int accY1 = blTruncToInt(_borderAccX0Y1);

    if (accY0 == accY1)
      return BL_SUCCESS;

    int minY = blMin(accY0, accY1);
    int maxY = blMax(accY0, accY1);

    _bBoxI.y0 = blMin(_bBoxI.y0, minY);
    _bBoxI.y1 = blMax(_bBoxI.y1, maxY);

    return addClosedLine(_clipBoxI.x0, minY, _clipBoxI.x0, maxY, uint32_t(accY0 > accY1));
  }

  BL_INLINE BLResult _emitRightBorder() noexcept {
    int accY0 = blTruncToInt(_borderAccX1Y0);
    int accY1 = blTruncToInt(_borderAccX1Y1);

    if (accY0 == accY1)
      return BL_SUCCESS;

    int minY = blMin(accY0, accY1);
    int maxY = blMax(accY0, accY1);

    _bBoxI.y0 = blMin(_bBoxI.y0, minY);
    _bBoxI.y1 = blMax(_bBoxI.y1, maxY);

    return addClosedLine(_clipBoxI.x1, minY, _clipBoxI.x1, maxY, uint32_t(accY0 > accY1));
  }

  //! \}

  // TODO: This should go somewhere else, also the name doesn't make much sense...
  static BL_INLINE double dx_div_dy(const BLPoint& d) noexcept { return d.x / d.y; }
  static BL_INLINE double dy_div_dx(const BLPoint& d) noexcept { return d.y / d.x; }
};

//! \}

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_EDGEBUILDER_P_H_INCLUDED
