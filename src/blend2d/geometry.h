// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GEOMETRY_H_INCLUDED
#define BLEND2D_GEOMETRY_H_INCLUDED

#include "api.h"

//! \addtogroup bl_geometry
//! \{

//! Direction of a geometry used by geometric primitives and paths.
BL_DEFINE_ENUM(BLGeometryDirection) {
  //! No direction specified.
  BL_GEOMETRY_DIRECTION_NONE = 0,
  //! Clockwise direction.
  BL_GEOMETRY_DIRECTION_CW = 1,
  //! Counter-clockwise direction.
  BL_GEOMETRY_DIRECTION_CCW = 2

  BL_FORCE_ENUM_UINT32(BL_GEOMETRY_DIRECTION)
};

//! Geometry type.
//!
//! Geometry describes a shape or path that can be either rendered or added to a BLPath container. Both \ref BLPath
//! and \ref BLContext provide functionality to work with all geometry types. Please note that each type provided
//! here requires to pass a matching struct or class to the function that consumes a `geometryType` and `geometryData`
//! arguments.
//!
//! \cond INTERNAL
//! \note Always modify `BL_GEOMETRY_TYPE_SIMPLE_LAST` and related functions when adding a new type to `BLGeometryType`
//! enum. Some functions just pass the geometry type and data to another function, but the rendering context must copy
//! simple types to a render job, which means that it must know which type is simple and also sizes of all simple
//! types, see `geometry_p.h` for more details about handling simple types.
//! \endcond
BL_DEFINE_ENUM(BLGeometryType) {
  //! No geometry provided.
  BL_GEOMETRY_TYPE_NONE = 0,
  //! BLBoxI struct.
  BL_GEOMETRY_TYPE_BOXI = 1,
  //! BLBox struct.
  BL_GEOMETRY_TYPE_BOXD = 2,
  //! BLRectI struct.
  BL_GEOMETRY_TYPE_RECTI = 3,
  //! BLRect struct.
  BL_GEOMETRY_TYPE_RECTD = 4,
  //! BLCircle struct.
  BL_GEOMETRY_TYPE_CIRCLE = 5,
  //! BLEllipse struct.
  BL_GEOMETRY_TYPE_ELLIPSE = 6,
  //! BLRoundRect struct.
  BL_GEOMETRY_TYPE_ROUND_RECT = 7,
  //! BLArc struct.
  BL_GEOMETRY_TYPE_ARC = 8,
  //! BLArc struct representing chord.
  BL_GEOMETRY_TYPE_CHORD = 9,
  //! BLArc struct representing pie.
  BL_GEOMETRY_TYPE_PIE = 10,
  //! BLLine struct.
  BL_GEOMETRY_TYPE_LINE = 11,
  //! BLTriangle struct.
  BL_GEOMETRY_TYPE_TRIANGLE = 12,
  //! BLArrayView<BLPointI> representing a polyline.
  BL_GEOMETRY_TYPE_POLYLINEI = 13,
  //! BLArrayView<BLPoint> representing a polyline.
  BL_GEOMETRY_TYPE_POLYLINED = 14,
  //! BLArrayView<BLPointI> representing a polygon.
  BL_GEOMETRY_TYPE_POLYGONI = 15,
  //! BLArrayView<BLPoint> representing a polygon.
  BL_GEOMETRY_TYPE_POLYGOND = 16,
  //! BLArrayView<BLBoxI> struct.
  BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI = 17,
  //! BLArrayView<BLBox> struct.
  BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD = 18,
  //! BLArrayView<BLRectI> struct.
  BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI = 19,
  //! BLArrayView<BLRect> struct.
  BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD = 20,
  //! BLPath (or BLPathCore).
  BL_GEOMETRY_TYPE_PATH = 21,

  //! Maximum value of `BLGeometryType`.
  BL_GEOMETRY_TYPE_MAX_VALUE = 21,

  //! \cond INTERNAL

  //! The last simple type.
  BL_GEOMETRY_TYPE_SIMPLE_LAST = BL_GEOMETRY_TYPE_TRIANGLE

  //! \endcond

  BL_FORCE_ENUM_UINT32(BL_GEOMETRY_TYPE)
};

//! Fill rule.
BL_DEFINE_ENUM(BLFillRule) {
  //! Non-zero fill-rule.
  BL_FILL_RULE_NON_ZERO = 0,
  //! Even-odd fill-rule.
  BL_FILL_RULE_EVEN_ODD = 1,

  //! Maximum value of `BLFillRule`.
  BL_FILL_RULE_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_FILL_RULE)
};

//! Hit-test result.
BL_DEFINE_ENUM(BLHitTest) {
  //! Fully in.
  BL_HIT_TEST_IN = 0,
  //! Partially in/out.
  BL_HIT_TEST_PART = 1,
  //! Fully out.
  BL_HIT_TEST_OUT = 2,

  //! Hit test failed (invalid argument, NaNs, etc).
  BL_HIT_TEST_INVALID = 0xFFFFFFFFu

  BL_FORCE_ENUM_UINT32(BL_HIT_TEST)
};

//! Point specified as [x, y] using `int` as a storage type.
struct BLPointI {
  int x;
  int y;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLPointI() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLPointI(const BLPointI&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLPointI(int x, int y) noexcept
    : x(x),
      y(y) {}

  BL_INLINE_NODEBUG BLPointI& operator=(const BLPointI& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLPointI& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLPointI& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0, 0); }
  BL_INLINE_NODEBUG void reset(const BLPointI& other) noexcept { reset(other.x, other.y); }
  BL_INLINE_NODEBUG void reset(int xValue, int yValue) noexcept {
    x = xValue;
    y = yValue;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLPointI& other) const noexcept {
    return bool(unsigned(blEquals(x, other.x)) &
                unsigned(blEquals(y, other.y)));
  }
#endif
};

//! Size specified as [w, h] using `int` as a storage type.
struct BLSizeI {
  int w;
  int h;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLSizeI() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLSizeI(const BLSizeI&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLSizeI(int w, int h) noexcept
    : w(w),
      h(h) {}

  BL_INLINE_NODEBUG BLSizeI& operator=(const BLSizeI& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLSizeI& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLSizeI& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0, 0); }
  BL_INLINE_NODEBUG void reset(const BLSizeI& other) noexcept { reset(other.w, other.h); }
  BL_INLINE_NODEBUG void reset(int wValue, int hValue) noexcept {
    w = wValue;
    h = hValue;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLSizeI& other) const noexcept {
    return bool(unsigned(blEquals(w, other.w)) &
                unsigned(blEquals(h, other.h)));
  }
#endif
};

//! Box specified as [x0, y0, x1, y1] using `int` as a storage type.
struct BLBoxI {
  int x0;
  int y0;
  int x1;
  int y1;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLBoxI() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLBoxI(const BLBoxI&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLBoxI(int x0, int y0, int x1, int y1) noexcept
    : x0(x0),
      y0(y0),
      x1(x1),
      y1(y1) {}

  BL_INLINE_NODEBUG BLBoxI& operator=(const BLBoxI& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLBoxI& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLBoxI& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0, 0, 0, 0); }
  BL_INLINE_NODEBUG void reset(const BLBoxI& other) noexcept { reset(other.x0, other.y0, other.x1, other.y1); }
  BL_INLINE_NODEBUG void reset(int x0Value, int y0Value, int x1Value, int y1Value) noexcept {
    x0 = x0Value;
    y0 = y0Value;
    x1 = x1Value;
    y1 = y1Value;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLBoxI& other) const noexcept {
    return bool(unsigned(blEquals(x0, other.x0)) &
                unsigned(blEquals(y0, other.y0)) &
                unsigned(blEquals(x1, other.x1)) &
                unsigned(blEquals(y1, other.y1)));
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool contains(int x, int y) const noexcept {
    return bool(unsigned(x >= x0) &
                unsigned(y >= y0) &
                unsigned(x <  x1) &
                unsigned(y <  y1));
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool contains(const BLPointI& pt) const noexcept { return contains(pt.x, pt.y); }
#endif
};

//! Rectangle specified as [x, y, w, h] using `int` as a storage type.
struct BLRectI {
  int x;
  int y;
  int w;
  int h;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLRectI() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLRectI(const BLRectI&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLRectI(int x, int y, int w, int h) noexcept
    : x(x),
      y(y),
      w(w),
      h(h) {}

  BL_INLINE_NODEBUG BLRectI& operator=(const BLRectI& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLRectI& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLRectI& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0, 0, 0, 0); }
  BL_INLINE_NODEBUG void reset(const BLRectI& other) noexcept { reset(other.x, other.y, other.w, other.h); }
  BL_INLINE_NODEBUG void reset(int xValue, int yValue, int wValue, int hValue) noexcept {
    x = xValue;
    y = yValue;
    w = wValue;
    h = hValue;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLRectI& other) const noexcept {
    return bool(unsigned(blEquals(x, other.x)) &
                unsigned(blEquals(y, other.y)) &
                unsigned(blEquals(w, other.w)) &
                unsigned(blEquals(h, other.h)));
  }
#endif
};

//! Point specified as [x, y] using `double` as a storage type.
struct BLPoint {
  double x;
  double y;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLPoint() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLPoint(const BLPoint&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLPoint(const BLPointI& other) noexcept
    : x(other.x),
      y(other.y) {}

  BL_INLINE_NODEBUG constexpr BLPoint(double x, double y) noexcept
    : x(x),
      y(y) {}

  BL_INLINE_NODEBUG BLPoint& operator=(const BLPoint& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLPoint& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLPoint& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0, 0); }
  BL_INLINE_NODEBUG void reset(const BLPoint& other) noexcept { reset(other.x, other.y); }
  BL_INLINE_NODEBUG void reset(double xValue, double yValue) noexcept {
    x = xValue;
    y = yValue;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLPoint& other) const noexcept {
    return bool(unsigned(blEquals(x, other.x)) &
                unsigned(blEquals(y, other.y)));
  }
#endif
};

//! Size specified as [w, h] using `double` as a storage type.
struct BLSize {
  double w;
  double h;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLSize() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLSize(const BLSize&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLSize(double w, double h) noexcept
    : w(w),
      h(h) {}

  BL_INLINE_NODEBUG constexpr BLSize(const BLSizeI& other) noexcept
    : w(other.w),
      h(other.h) {}

  BL_INLINE_NODEBUG BLSize& operator=(const BLSize& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLSize& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLSize& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0, 0); }
  BL_INLINE_NODEBUG void reset(const BLSize& other) noexcept { reset(other.w, other.h); }
  BL_INLINE_NODEBUG void reset(double wValue, double hValue) noexcept {
    w = wValue;
    h = hValue;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLSize& other) const noexcept {
    return bool(unsigned(blEquals(w, other.w)) &
                unsigned(blEquals(h, other.h)));
  }
#endif
};

//! Box specified as [x0, y0, x1, y1] using `double` as a storage type.
struct BLBox {
  double x0;
  double y0;
  double x1;
  double y1;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLBox() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLBox(const BLBox&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLBox(const BLBoxI& other) noexcept
    : x0(other.x0),
      y0(other.y0),
      x1(other.x1),
      y1(other.y1) {}

  BL_INLINE_NODEBUG constexpr BLBox(double x0, double y0, double x1, double y1) noexcept
    : x0(x0),
      y0(y0),
      x1(x1),
      y1(y1) {}

  BL_INLINE_NODEBUG BLBox& operator=(const BLBox& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLBox& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLBox& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0.0, 0.0, 0.0, 0.0); }
  BL_INLINE_NODEBUG void reset(const BLBox& other) noexcept { reset(other.x0, other.y0, other.x1, other.y1); }
  BL_INLINE_NODEBUG void reset(double x0Value, double y0Value, double x1Value, double y1Value) noexcept {
    x0 = x0Value;
    y0 = y0Value;
    x1 = x1Value;
    y1 = y1Value;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLBox& other) const noexcept {
    return bool(unsigned(blEquals(x0, other.x0)) &
                unsigned(blEquals(y0, other.y0)) &
                unsigned(blEquals(x1, other.x1)) &
                unsigned(blEquals(y1, other.y1)));
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool contains(double x, double y) const noexcept {
    return bool(unsigned(x >= x0) &
                unsigned(y >= y0) &
                unsigned(x <  x1) &
                unsigned(y <  y1));
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool contains(const BLPoint& pt) const noexcept { return contains(pt.x, pt.y); }
#endif
};

//! Rectangle specified as [x, y, w, h] using `double` as a storage type.
struct BLRect {
  double x;
  double y;
  double w;
  double h;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLRect() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLRect(const BLRect&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLRect(const BLRectI& other) noexcept
    : x(other.x),
      y(other.y),
      w(other.w),
      h(other.h) {}

  BL_INLINE_NODEBUG constexpr BLRect(double x, double y, double w, double h) noexcept
    : x(x), y(y), w(w), h(h) {}

  BL_INLINE_NODEBUG BLRect& operator=(const BLRect& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLRect& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLRect& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0.0, 0.0, 0.0, 0.0); }
  BL_INLINE_NODEBUG void reset(const BLRect& other) noexcept { reset(other.x, other.y, other.w, other.h); }
  BL_INLINE_NODEBUG void reset(double xValue, double yValue, double wValue, double hValue) noexcept {
    x = xValue;
    y = yValue;
    w = wValue;
    h = hValue;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLRect& other) const noexcept {
    return bool(unsigned(blEquals(x, other.x)) &
                unsigned(blEquals(y, other.y)) &
                unsigned(blEquals(w, other.w)) &
                unsigned(blEquals(h, other.h)));
  }
#endif
};

//! Line specified as [x0, y0, x1, y1] using `double` as a storage type.
struct BLLine {
  double x0, y0;
  double x1, y1;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLLine() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLLine(const BLLine&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLLine(double x0, double y0, double x1, double y1) noexcept
    : x0(x0), y0(y0), x1(x1), y1(y1) {}

  BL_INLINE_NODEBUG BLLine& operator=(const BLLine& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLLine& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLLine& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0.0, 0.0, 0.0, 0.0); }
  BL_INLINE_NODEBUG void reset(const BLLine& other) noexcept { reset(other.x0, other.y0, other.x1, other.y1); }
  BL_INLINE_NODEBUG void reset(double x0Value, double y0Value, double x1Value, double y1Value) noexcept {
    x0 = x0Value;
    y0 = y0Value;
    x1 = x1Value;
    y1 = y1Value;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLLine& other) const noexcept {
    return bool(unsigned(x0 == other.x0) &
                unsigned(y0 == other.y0) &
                unsigned(x1 == other.x1) &
                unsigned(y1 == other.y1));
  }
#endif
};

//! Triangle data specified as [x0, y0, x1, y1, x2, y2] using `double` as a storage type.
struct BLTriangle {
  double x0, y0;
  double x1, y1;
  double x2, y2;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLTriangle() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLTriangle(const BLTriangle&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLTriangle(double x0, double y0, double x1, double y1, double x2, double y2) noexcept
    : x0(x0), y0(y0), x1(x1), y1(y1), x2(x2), y2(y2) {}

  BL_INLINE_NODEBUG BLTriangle& operator=(const BLTriangle& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLTriangle& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLTriangle& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); }
  BL_INLINE_NODEBUG void reset(const BLTriangle& other) noexcept { reset(other.x0, other.y0, other.x1, other.y1, other.x2, other.y2); }
  BL_INLINE_NODEBUG void reset(double x0Value, double y0Value, double x1Value, double y1Value, double x2Value, double y2Value) noexcept {
    x0 = x0Value;
    y0 = y0Value;
    x1 = x1Value;
    y1 = y1Value;
    x2 = x2Value;
    y2 = y2Value;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLTriangle& other) const noexcept {
    return bool(unsigned(x0 == other.x0) & unsigned(y0 == other.y0) &
                unsigned(x1 == other.x1) & unsigned(y1 == other.y1) &
                unsigned(x2 == other.x2) & unsigned(y2 == other.y2));
  }
#endif
};

//! Rounded rectangle specified as [x, y, w, h, rx, ry] using `double` as a storage type.
struct BLRoundRect {
  double x, y, w, h;
  double rx, ry;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLRoundRect() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLRoundRect(const BLRoundRect&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLRoundRect(const BLRect& rect, double r) noexcept
    : x(rect.x), y(rect.y), w(rect.w), h(rect.h), rx(r), ry(r) {}

  BL_INLINE_NODEBUG constexpr BLRoundRect(const BLRect& rect, double rx, double ry) noexcept
    : x(rect.x), y(rect.y), w(rect.w), h(rect.h), rx(rx), ry(ry) {}

  BL_INLINE_NODEBUG constexpr BLRoundRect(double x, double y, double w, double h, double r) noexcept
    : x(x), y(y), w(w), h(h), rx(r), ry(r) {}

  BL_INLINE_NODEBUG constexpr BLRoundRect(double x, double y, double w, double h, double rx, double ry) noexcept
    : x(x), y(y), w(w), h(h), rx(rx), ry(ry) {}

  BL_INLINE_NODEBUG BLRoundRect& operator=(const BLRoundRect& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLRoundRect& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLRoundRect& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); }

  BL_INLINE_NODEBUG void reset(const BLRoundRect& other) noexcept {
    reset(other.x, other.y, other.w, other.h, other.rx, other.ry);
  }

  BL_INLINE_NODEBUG void reset(double xValue, double yValue, double wValue, double hValue, double rValue) noexcept {
    reset(xValue, yValue, wValue, hValue, rValue, rValue);
  }

  BL_INLINE_NODEBUG void reset(double xValue, double yValue, double wValue, double hValue, double rxValue, double ryValue) noexcept {
    x = xValue;
    y = yValue;
    w = wValue;
    h = hValue;
    rx = rxValue;
    ry = ryValue;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLRoundRect& other) const noexcept {
    return bool(unsigned(x  == other.x ) & unsigned(y  == other.y ) &
                unsigned(w  == other.w ) & unsigned(h  == other.h ) &
                unsigned(rx == other.rx) & unsigned(rx == other.ry));
  }
#endif
};

//! Circle specified as [cx, cy, r] using `double` as a storage type.
struct BLCircle {
  double cx, cy;
  double r;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLCircle() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLCircle(const BLCircle&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLCircle(double cx, double cy, double r) noexcept
    : cx(cx), cy(cy), r(r) {}

  BL_INLINE_NODEBUG BLCircle& operator=(const BLCircle& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLCircle& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLCircle& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0.0, 0.0, 0.0); }
  BL_INLINE_NODEBUG void reset(const BLCircle& other) noexcept { reset(other.cx, other.cy, other.r); }
  BL_INLINE_NODEBUG void reset(double cxValue, double cyValue, double rValue) noexcept {
    cx = cxValue;
    cy = cyValue;
    r = rValue;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLCircle& other) const noexcept {
    return bool(unsigned(cx == other.cx) &
                unsigned(cy == other.cy) &
                unsigned(r == other.r));
  }
#endif
};

//! Ellipse specified as [cx, cy, rx, ry] using `double` as a storage type.
struct BLEllipse {
  double cx, cy;
  double rx, ry;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLEllipse() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLEllipse(const BLEllipse&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLEllipse(double cx, double cy, double r) noexcept
    : cx(cx), cy(cy), rx(r), ry(r) {}

  BL_INLINE_NODEBUG constexpr BLEllipse(double cx, double cy, double rx, double ry) noexcept
    : cx(cx), cy(cy), rx(rx), ry(ry) {}

  BL_INLINE_NODEBUG BLEllipse& operator=(const BLEllipse& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLEllipse& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLEllipse& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0.0, 0.0, 0.0, 0.0); }
  BL_INLINE_NODEBUG void reset(const BLEllipse& other) noexcept { reset(other.cx, other.cy, other.rx, other.ry); }
  BL_INLINE_NODEBUG void reset(double cx, double cy, double r) noexcept { reset(cx, cy, r, r); }

  BL_INLINE_NODEBUG void reset(double cxValue, double cyValue, double rxValue, double ryValue) noexcept {
    cx = cxValue;
    cy = cyValue;
    rx = rxValue;
    ry = ryValue;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLEllipse& other) const noexcept {
    return bool(unsigned(cx == other.cx) &
                unsigned(cy == other.cy) &
                unsigned(rx == other.rx) &
                unsigned(ry == other.ry));
  }
#endif
};

//! Arc specified as [cx, cy, rx, ry, start, sweep] using `double` as a storage type.
struct BLArc {
  double cx, cy;
  double rx, ry;
  double start;
  double sweep;

#ifdef __cplusplus
  BL_INLINE_NODEBUG BLArc() noexcept = default;
  BL_INLINE_NODEBUG constexpr BLArc(const BLArc&) noexcept = default;

  BL_INLINE_NODEBUG constexpr BLArc(double cx, double cy, double rx, double ry, double start, double sweep) noexcept
    : cx(cx), cy(cy), rx(rx), ry(ry), start(start), sweep(sweep) {}

  BL_INLINE_NODEBUG BLArc& operator=(const BLArc& other) noexcept = default;

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator==(const BLArc& other) const noexcept { return  equals(other); }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool operator!=(const BLArc& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG void reset() noexcept { reset(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); }

  BL_INLINE_NODEBUG void reset(const BLArc& other) noexcept {
    reset(other.cx, other.cy, other.rx, other.ry, other.start, other.sweep);
  }

  BL_INLINE_NODEBUG void reset(double cxValue, double cyValue, double rxValue, double ryValue, double startValue, double sweepValue) noexcept {
    cx = cxValue;
    cy = cyValue;
    rx = rxValue;
    ry = ryValue;
    start = startValue;
    sweep = sweepValue;
  }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLArc& other) const noexcept {
    return bool(unsigned(cx    == other.cx   ) &
                unsigned(cy    == other.cy   ) &
                unsigned(rx    == other.rx   ) &
                unsigned(ry    == other.ry   ) &
                unsigned(start == other.start) &
                unsigned(sweep == other.sweep));
  }
#endif
};

//! \}

#ifdef __cplusplus
//! \addtogroup bl_geometry
//! \{

//! \name Global Specializations
//! \{

template<> BL_INLINE_NODEBUG constexpr BLPoint blAbs(const BLPoint& a) noexcept { return BLPoint(blAbs(a.x), blAbs(a.y)); }
template<> BL_INLINE_NODEBUG constexpr BLPoint blMin(const BLPoint& a, const BLPoint& b) noexcept { return BLPoint(blMin(a.x, b.x), blMin(a.y, b.y)); }
template<> BL_INLINE_NODEBUG constexpr BLPoint blMax(const BLPoint& a, const BLPoint& b) noexcept { return BLPoint(blMax(a.x, b.x), blMax(a.y, b.y)); }

template<> BL_INLINE_NODEBUG constexpr BLSize blAbs(const BLSize& a) noexcept { return BLSize(blAbs(a.w), blAbs(a.h)); }
template<> BL_INLINE_NODEBUG constexpr BLSize blMin(const BLSize& a, const BLSize& b) noexcept { return BLSize(blMin(a.w, b.w), blMin(a.h, b.h)); }
template<> BL_INLINE_NODEBUG constexpr BLSize blMax(const BLSize& a, const BLSize& b) noexcept { return BLSize(blMax(a.w, b.w), blMax(a.h, b.h)); }

static BL_INLINE_NODEBUG constexpr BLPoint blMin(const BLPoint& a, double b) noexcept { return BLPoint(blMin(a.x, b), blMin(a.y, b)); }
static BL_INLINE_NODEBUG constexpr BLPoint blMin(double a, const BLPoint& b) noexcept { return BLPoint(blMin(a, b.x), blMin(a, b.y)); }

static BL_INLINE_NODEBUG constexpr BLPoint blMax(const BLPoint& a, double b) noexcept { return BLPoint(blMax(a.x, b), blMax(a.y, b)); }
static BL_INLINE_NODEBUG constexpr BLPoint blMax(double a, const BLPoint& b) noexcept { return BLPoint(blMax(a, b.x), blMax(a, b.y)); }

static BL_INLINE_NODEBUG constexpr BLPoint blClamp(const BLPoint& a, double b, double c) noexcept { return blMin(c, blMax(b, a)); }

//! \}

//! \name Overloaded Operators
//! \{

static BL_INLINE_NODEBUG constexpr BLPointI operator-(const BLPointI& self) noexcept { return BLPointI(-self.x, -self.y); }

static BL_INLINE_NODEBUG constexpr BLPointI operator+(const BLPointI& a, int b) noexcept { return BLPointI(a.x + b, a.y + b); }
static BL_INLINE_NODEBUG constexpr BLPointI operator-(const BLPointI& a, int b) noexcept { return BLPointI(a.x - b, a.y - b); }
static BL_INLINE_NODEBUG constexpr BLPointI operator*(const BLPointI& a, int b) noexcept { return BLPointI(a.x * b, a.y * b); }

static BL_INLINE_NODEBUG constexpr BLPointI operator+(int a, const BLPointI& b) noexcept { return BLPointI(a + b.x, a + b.y); }
static BL_INLINE_NODEBUG constexpr BLPointI operator-(int a, const BLPointI& b) noexcept { return BLPointI(a - b.x, a - b.y); }
static BL_INLINE_NODEBUG constexpr BLPointI operator*(int a, const BLPointI& b) noexcept { return BLPointI(a * b.x, a * b.y); }

static BL_INLINE_NODEBUG constexpr BLPointI operator+(const BLPointI& a, const BLPointI& b) noexcept { return BLPointI(a.x + b.x, a.y + b.y); }
static BL_INLINE_NODEBUG constexpr BLPointI operator-(const BLPointI& a, const BLPointI& b) noexcept { return BLPointI(a.x - b.x, a.y - b.y); }
static BL_INLINE_NODEBUG constexpr BLPointI operator*(const BLPointI& a, const BLPointI& b) noexcept { return BLPointI(a.x * b.x, a.y * b.y); }

static BL_INLINE_NODEBUG BLPointI& operator+=(BLPointI& a, int b) noexcept { a.reset(a.x + b, a.y + b); return a; }
static BL_INLINE_NODEBUG BLPointI& operator-=(BLPointI& a, int b) noexcept { a.reset(a.x - b, a.y - b); return a; }
static BL_INLINE_NODEBUG BLPointI& operator*=(BLPointI& a, int b) noexcept { a.reset(a.x * b, a.y * b); return a; }
static BL_INLINE_NODEBUG BLPointI& operator/=(BLPointI& a, int b) noexcept { a.reset(a.x / b, a.y / b); return a; }

static BL_INLINE_NODEBUG BLPointI& operator+=(BLPointI& a, const BLPointI& b) noexcept { a.reset(a.x + b.x, a.y + b.y); return a; }
static BL_INLINE_NODEBUG BLPointI& operator-=(BLPointI& a, const BLPointI& b) noexcept { a.reset(a.x - b.x, a.y - b.y); return a; }
static BL_INLINE_NODEBUG BLPointI& operator*=(BLPointI& a, const BLPointI& b) noexcept { a.reset(a.x * b.x, a.y * b.y); return a; }
static BL_INLINE_NODEBUG BLPointI& operator/=(BLPointI& a, const BLPointI& b) noexcept { a.reset(a.x / b.x, a.y / b.y); return a; }

static BL_INLINE_NODEBUG constexpr BLPoint operator-(const BLPoint& a) noexcept { return BLPoint(-a.x, -a.y); }

static BL_INLINE_NODEBUG constexpr BLPoint operator+(const BLPoint& a, double b) noexcept { return BLPoint(a.x + b, a.y + b); }
static BL_INLINE_NODEBUG constexpr BLPoint operator-(const BLPoint& a, double b) noexcept { return BLPoint(a.x - b, a.y - b); }
static BL_INLINE_NODEBUG constexpr BLPoint operator*(const BLPoint& a, double b) noexcept { return BLPoint(a.x * b, a.y * b); }
static BL_INLINE_NODEBUG constexpr BLPoint operator/(const BLPoint& a, double b) noexcept { return BLPoint(a.x / b, a.y / b); }

static BL_INLINE_NODEBUG constexpr BLPoint operator+(double a, const BLPoint& b) noexcept { return BLPoint(a + b.x, a + b.y); }
static BL_INLINE_NODEBUG constexpr BLPoint operator-(double a, const BLPoint& b) noexcept { return BLPoint(a - b.x, a - b.y); }
static BL_INLINE_NODEBUG constexpr BLPoint operator*(double a, const BLPoint& b) noexcept { return BLPoint(a * b.x, a * b.y); }
static BL_INLINE_NODEBUG constexpr BLPoint operator/(double a, const BLPoint& b) noexcept { return BLPoint(a / b.x, a / b.y); }

static BL_INLINE_NODEBUG constexpr BLPoint operator+(const BLPoint& a, const BLPoint& b) noexcept { return BLPoint(a.x + b.x, a.y + b.y); }
static BL_INLINE_NODEBUG constexpr BLPoint operator-(const BLPoint& a, const BLPoint& b) noexcept { return BLPoint(a.x - b.x, a.y - b.y); }
static BL_INLINE_NODEBUG constexpr BLPoint operator*(const BLPoint& a, const BLPoint& b) noexcept { return BLPoint(a.x * b.x, a.y * b.y); }
static BL_INLINE_NODEBUG constexpr BLPoint operator/(const BLPoint& a, const BLPoint& b) noexcept { return BLPoint(a.x / b.x, a.y / b.y); }

static BL_INLINE_NODEBUG BLPoint& operator+=(BLPoint& a, double b) noexcept { a.reset(a.x + b, a.y + b); return a; }
static BL_INLINE_NODEBUG BLPoint& operator-=(BLPoint& a, double b) noexcept { a.reset(a.x - b, a.y - b); return a; }
static BL_INLINE_NODEBUG BLPoint& operator*=(BLPoint& a, double b) noexcept { a.reset(a.x * b, a.y * b); return a; }
static BL_INLINE_NODEBUG BLPoint& operator/=(BLPoint& a, double b) noexcept { a.reset(a.x / b, a.y / b); return a; }

static BL_INLINE_NODEBUG BLPoint& operator+=(BLPoint& a, const BLPoint& b) noexcept { a.reset(a.x + b.x, a.y + b.y); return a; }
static BL_INLINE_NODEBUG BLPoint& operator-=(BLPoint& a, const BLPoint& b) noexcept { a.reset(a.x - b.x, a.y - b.y); return a; }
static BL_INLINE_NODEBUG BLPoint& operator*=(BLPoint& a, const BLPoint& b) noexcept { a.reset(a.x * b.x, a.y * b.y); return a; }
static BL_INLINE_NODEBUG BLPoint& operator/=(BLPoint& a, const BLPoint& b) noexcept { a.reset(a.x / b.x, a.y / b.y); return a; }

static BL_INLINE_NODEBUG constexpr BLBox operator+(double a, const BLBox& b) noexcept { return BLBox(a + b.x0, a + b.y0, a + b.x1, a + b.y1); }
static BL_INLINE_NODEBUG constexpr BLBox operator-(double a, const BLBox& b) noexcept { return BLBox(a - b.x0, a - b.y0, a - b.x1, a - b.y1); }
static BL_INLINE_NODEBUG constexpr BLBox operator*(double a, const BLBox& b) noexcept { return BLBox(a * b.x0, a * b.y0, a * b.x1, a * b.y1); }
static BL_INLINE_NODEBUG constexpr BLBox operator/(double a, const BLBox& b) noexcept { return BLBox(a / b.x0, a / b.y0, a / b.x1, a / b.y1); }

static BL_INLINE_NODEBUG constexpr BLBox operator+(const BLBox& a, double b) noexcept { return BLBox(a.x0 + b, a.y0 + b, a.x1 + b, a.y1 + b); }
static BL_INLINE_NODEBUG constexpr BLBox operator-(const BLBox& a, double b) noexcept { return BLBox(a.x0 - b, a.y0 - b, a.x1 - b, a.y1 - b); }
static BL_INLINE_NODEBUG constexpr BLBox operator*(const BLBox& a, double b) noexcept { return BLBox(a.x0 * b, a.y0 * b, a.x1 * b, a.y1 * b); }
static BL_INLINE_NODEBUG constexpr BLBox operator/(const BLBox& a, double b) noexcept { return BLBox(a.x0 / b, a.y0 / b, a.x1 / b, a.y1 / b); }

static BL_INLINE_NODEBUG constexpr BLBox operator+(const BLPoint& a, const BLBox& b) noexcept { return BLBox(a.x + b.x0, a.y + b.y0, a.x + b.x1, a.y + b.y1); }
static BL_INLINE_NODEBUG constexpr BLBox operator-(const BLPoint& a, const BLBox& b) noexcept { return BLBox(a.x - b.x0, a.y - b.y0, a.x - b.x1, a.y - b.y1); }
static BL_INLINE_NODEBUG constexpr BLBox operator*(const BLPoint& a, const BLBox& b) noexcept { return BLBox(a.x * b.x0, a.y * b.y0, a.x * b.x1, a.y * b.y1); }
static BL_INLINE_NODEBUG constexpr BLBox operator/(const BLPoint& a, const BLBox& b) noexcept { return BLBox(a.x / b.x0, a.y / b.y0, a.x / b.x1, a.y / b.y1); }

static BL_INLINE_NODEBUG constexpr BLBox operator+(const BLBox& a, const BLPoint& b) noexcept { return BLBox(a.x0 + b.x, a.y0 + b.y, a.x1 + b.x, a.y1 + b.y); }
static BL_INLINE_NODEBUG constexpr BLBox operator-(const BLBox& a, const BLPoint& b) noexcept { return BLBox(a.x0 - b.x, a.y0 - b.y, a.x1 - b.x, a.y1 - b.y); }
static BL_INLINE_NODEBUG constexpr BLBox operator*(const BLBox& a, const BLPoint& b) noexcept { return BLBox(a.x0 * b.x, a.y0 * b.y, a.x1 * b.x, a.y1 * b.y); }
static BL_INLINE_NODEBUG constexpr BLBox operator/(const BLBox& a, const BLPoint& b) noexcept { return BLBox(a.x0 / b.x, a.y0 / b.y, a.x1 / b.x, a.y1 / b.y); }

static BL_INLINE_NODEBUG BLBox& operator+=(BLBox& a, double b) noexcept { a.reset(a.x0 + b, a.y0 + b, a.x1 + b, a.y1 + b); return a; }
static BL_INLINE_NODEBUG BLBox& operator-=(BLBox& a, double b) noexcept { a.reset(a.x0 - b, a.y0 - b, a.x1 - b, a.y1 - b); return a; }
static BL_INLINE_NODEBUG BLBox& operator*=(BLBox& a, double b) noexcept { a.reset(a.x0 * b, a.y0 * b, a.x1 * b, a.y1 * b); return a; }
static BL_INLINE_NODEBUG BLBox& operator/=(BLBox& a, double b) noexcept { a.reset(a.x0 / b, a.y0 / b, a.x1 / b, a.y1 / b); return a; }

static BL_INLINE_NODEBUG BLBox& operator+=(BLBox& a, const BLPoint& b) noexcept { a.reset(a.x0 + b.x, a.y0 + b.y, a.x1 + b.x, a.y1 + b.y); return a; }
static BL_INLINE_NODEBUG BLBox& operator-=(BLBox& a, const BLPoint& b) noexcept { a.reset(a.x0 - b.x, a.y0 - b.y, a.x1 - b.x, a.y1 - b.y); return a; }
static BL_INLINE_NODEBUG BLBox& operator*=(BLBox& a, const BLPoint& b) noexcept { a.reset(a.x0 * b.x, a.y0 * b.y, a.x1 * b.x, a.y1 * b.y); return a; }
static BL_INLINE_NODEBUG BLBox& operator/=(BLBox& a, const BLPoint& b) noexcept { a.reset(a.x0 / b.x, a.y0 / b.y, a.x1 / b.x, a.y1 / b.y); return a; }

//! \}

//! \}
#endif

#endif // BLEND2D_GEOMETRY_H_INCLUDED
