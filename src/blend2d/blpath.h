// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLPATH_H
#define BLEND2D_BLPATH_H

#include "./blarray.h"
#include "./blgeometry.h"
#include "./blvariant.h"

//! \addtogroup blend2d_api_geometry
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Path command.
BL_DEFINE_ENUM(BLPathCmd) {
  //! Move-to command (starts a new figure).
  BL_PATH_CMD_MOVE = 0,
  //! On-path command (interpreted as line-to or the end of a curve).
  BL_PATH_CMD_ON = 1,
  //! Quad-to control point.
  BL_PATH_CMD_QUAD = 2,
  //! Cubic-to control point (always used as a pair of commands).
  BL_PATH_CMD_CUBIC = 3,
  //! Close path.
  BL_PATH_CMD_CLOSE = 4,

  //! Count of path commands.
  BL_PATH_CMD_COUNT = 5
};

//! Path command (never stored in path).
BL_DEFINE_ENUM(BLPathCmdExtra) {
  //! Used by `BLPath::setVertexAt` to preserve the current command value.
  BL_PATH_CMD_PRESERVE = 0xFFFFFFFFu
};

//! Path flags.
BL_DEFINE_ENUM(BLPathFlags) {
  //! Path is empty (no commands or close commands only).
  BL_PATH_FLAG_EMPTY = 0x00000001u,
  //! Path contains multiple figures.
  BL_PATH_FLAG_MULTIPLE = 0x00000002u,
  //! Path contains quad curves (at least one).
  BL_PATH_FLAG_QUADS = 0x00000004u,
  //! Path contains cubic curves (at least one).
  BL_PATH_FLAG_CUBICS = 0x00000008u,
  //! Path is invalid.
  BL_PATH_FLAG_INVALID = 0x40000000u,
  //! Flags are dirty (not reflecting the current status).
  BL_PATH_FLAG_DIRTY = 0x80000000u
};

//! Path reversal mode.
BL_DEFINE_ENUM(BLPathReverseMode) {
  //! Reverse each figure and their order as well (default).
  BL_PATH_REVERSE_MODE_COMPLETE = 0,
  //! Reverse each figure separately (keeps their order).
  BL_PATH_REVERSE_MODE_SEPARATE = 1,

  //! Count of path-reversal modes
  BL_PATH_REVERSE_MODE_COUNT = 2
};

//! Stroke join type.
BL_DEFINE_ENUM(BLStrokeJoin) {
  //! Miter-join possibly clipped at `miterLimit` [default].
  BL_STROKE_JOIN_MITER_CLIP = 0,
  //! Miter-join or bevel-join depending on miterLimit condition.
  BL_STROKE_JOIN_MITER_BEVEL = 1,
  //! Miter-join or round-join depending on miterLimit condition.
  BL_STROKE_JOIN_MITER_ROUND = 2,
  //! Bevel-join.
  BL_STROKE_JOIN_BEVEL = 3,
  //! Round-join.
  BL_STROKE_JOIN_ROUND = 4,

  //! Count of stroke join types.
  BL_STROKE_JOIN_COUNT = 5
};

//! Position of a stroke-cap.
BL_DEFINE_ENUM(BLStrokeCapPosition) {
  //! Start of the path.
  BL_STROKE_CAP_POSITION_START = 0,
  //! End of the path.
  BL_STROKE_CAP_POSITION_END = 1,

  //! Count of stroke position options.
  BL_STROKE_CAP_POSITION_COUNT = 2
};

//! A presentation attribute defining the shape to be used at the end of open subpaths.
BL_DEFINE_ENUM(BLStrokeCap) {
  //! Butt cap [default].
  BL_STROKE_CAP_BUTT = 0,
  //! Square cap.
  BL_STROKE_CAP_SQUARE = 1,
  //! Round cap.
  BL_STROKE_CAP_ROUND = 2,
  //! Round cap reversed.
  BL_STROKE_CAP_ROUND_REV = 3,
  //! Triangle cap.
  BL_STROKE_CAP_TRIANGLE = 4,
  //! Triangle cap reversed.
  BL_STROKE_CAP_TRIANGLE_REV = 5,

  //! Used to catch invalid arguments.
  BL_STROKE_CAP_COUNT = 6
};

//! Stroke transform order.
BL_DEFINE_ENUM(BLStrokeTransformOrder) {
  //! Transform after stroke  => `Transform(Stroke(Input))` [default].
  BL_STROKE_TRANSFORM_ORDER_AFTER = 0,
  //! Transform before stroke => `Stroke(Transform(Input))`.
  BL_STROKE_TRANSFORM_ORDER_BEFORE = 1,

  //! Count of transform order types.
  BL_STROKE_TRANSFORM_ORDER_COUNT = 2
};

//! Mode that specifies how curves are approximated to line segments.
BL_DEFINE_ENUM(BLFlattenMode) {
  //! Use default mode (decided by Blend2D).
  BL_FLATTEN_MODE_DEFAULT = 0,
  //! Recursive subdivision flattening.
  BL_FLATTEN_MODE_RECURSIVE = 1,

  //! Count of flatten modes.
  BL_FLATTEN_MODE_COUNT = 2
};

//! Mode that specifies how to construct offset curves.
BL_DEFINE_ENUM(BLOffsetMode) {
  //! Use default mode (decided by Blend2D).
  BL_OFFSET_MODE_DEFAULT = 0,
  //! Iterative offset construction.
  BL_OFFSET_MODE_ITERATIVE = 1,

  //! Count of offset modes.
  BL_OFFSET_MODE_COUNT = 2
};

// ============================================================================
// [BLApproximationOptions]
// ============================================================================

//! Options used to describe how geometry is approximated.
//!
//! This struct cannot be simply zeroed and then passed to functions that accept
//! approximation options. Use `blDefaultApproximationOptions` to setup defaults
//! and then alter values you want to change.
//!
//! Example of using `BLApproximationOptions`:
//!
//! ```
//! // Initialize with defaults first.
//! BLApproximationOptions approx = blDefaultApproximationOptions;
//!
//! // Override values you want to change.
//! approx.simplifyTolerance = 0.02;
//!
//! // ... now safely use approximation options in your code ...
//! ```
struct BLApproximationOptions {
  //! Specifies how curves are flattened, see `BLFlattenMode`.
  uint8_t flattenMode;
  //! Specifies how curves are offsetted (used by stroking), see `BLOffsetMode`.
  uint8_t offsetMode;
  //! Reserved for future use, must be zero.
  uint8_t reservedFlags[6];

  //! Tolerance used to flatten curves.
  double flattenTolerance;
  //! Tolerance used to approximatecubic curves qith quadratic curves.
  double simplifyTolerance;
  //! Curve offsetting parameter, exact meaning depends on `offsetMode`.
  double offsetParameter;
};

//! Default approximation options used by Blend2D.
BL_API_C const BLApproximationOptions blDefaultApproximationOptions;

// ============================================================================
// [BLStrokeOptions - Core]
// ============================================================================

//! Stroke options [C Interface - Core].
//!
//! This structure may use dynamically allocated memory so it's required to use
//! proper initializers to initialize it and reset it.
struct BLStrokeOptionsCore {
  union {
    struct {
      uint8_t startCap;
      uint8_t endCap;
      uint8_t join;
      uint8_t transformOrder;
      uint8_t reserved[4];
    };
    uint8_t caps[BL_STROKE_CAP_POSITION_COUNT];
    uint64_t hints;
  };

  double width;
  double miterLimit;
  double dashOffset;
  BL_TYPED_MEMBER(BLArrayCore, BLArray<double>, dashArray);

  BL_HAS_TYPED_MEMBERS(BLStrokeOptionsCore)
};

// ============================================================================
// [BLStrokeOptions - C++]
// ============================================================================

#ifdef __cplusplus
//! Stroke options [C++ API].
//!
//! You should use this as a structure and use members of `BLStrokeOptionsCore`
//! directly.
class BLStrokeOptions : public BLStrokeOptionsCore {
public:
  BL_INLINE BLStrokeOptions() noexcept { blStrokeOptionsInit(this); }
  BL_INLINE BLStrokeOptions(BLStrokeOptions&& other) noexcept { blStrokeOptionsInitMove(this, &other); }
  BL_INLINE BLStrokeOptions(const BLStrokeOptions& other) noexcept { blStrokeOptionsInitWeak(this, &other); }
  BL_INLINE ~BLStrokeOptions() noexcept { blStrokeOptionsReset(this); }

  BL_INLINE BLStrokeOptions& operator=(BLStrokeOptions&& other) noexcept { blStrokeOptionsAssignMove(this, &other); return *this; }
  BL_INLINE BLStrokeOptions& operator=(const BLStrokeOptions& other) noexcept { blStrokeOptionsAssignWeak(this, &other); return *this; }

  BL_INLINE void setCaps(uint32_t strokeCap) noexcept {
    startCap = uint8_t(strokeCap);
    endCap = uint8_t(strokeCap);
  }
};
#endif

// ============================================================================
// [BLPath - View]
// ============================================================================

//! 2D path view provides pointers to vertex and command data along with their
//! size.
struct BLPathView {
  const uint8_t* commandData;
  const BLPoint* vertexData;
  size_t size;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept {
    this->commandData = nullptr;
    this->vertexData = nullptr;
    this->size = 0;
  }

  BL_INLINE void reset(const uint8_t* commandData, const BLPoint* vertexData, size_t size) noexcept {
    this->commandData = commandData;
    this->vertexData = vertexData;
    this->size = size;
  }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLPath - Core]
// ============================================================================

//! 2D vector path [C Interface - Impl].
struct BLPathImpl {
  //! Union of either raw path-data or their `view`.
  union {
    struct {
      //! Command data
      uint8_t* commandData;
      //! Vertex data.
      BLPoint* vertexData;
      //! Vertex/command count.
      size_t size;
    };
    //! Path data as view.
    BLPathView view;
  };

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Path flags related to caching.
  volatile uint32_t flags;
  //! Path vertex/command capacity.
  size_t capacity;
};

//! 2D vector path [C Interface - Core].
struct BLPathCore {
  BLPathImpl* impl;
};

// ============================================================================
// [BLPath - C++]
// ============================================================================

#ifdef __cplusplus
//! 2D vector path [C++ API].
class BLPath : public BLPathCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_PATH2D;
  //! \endcond

  //! \name Constructors and Destructors
  //! \{

  BL_INLINE BLPath() noexcept { this->impl = none().impl; }
  BL_INLINE BLPath(BLPath&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLPath(const BLPath& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLPath(BLPathImpl* impl) noexcept { this->impl = impl; }

  BL_INLINE ~BLPath() noexcept { blPathReset(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLPath& operator=(BLPath&& other) noexcept { blPathAssignMove(this, &other); return *this; }
  BL_INLINE BLPath& operator=(const BLPath& other) noexcept { blPathAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLPath& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLPath& other) const noexcept { return !equals(other); }

  BL_INLINE explicit operator bool() const noexcept { return impl->size != 0; }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blPathReset(this); }
  BL_INLINE void swap(BLPath& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLPath&& other) noexcept { return blPathAssignMove(this, &other);  }
  BL_INLINE BLResult assign(const BLPath& other) noexcept { return blPathAssignWeak(this, &other);  }
  BL_INLINE BLResult assignDeep(const BLPath& other) noexcept { return blPathAssignDeep(this, &other); }

  //! Gets whether the 2D path is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Gets whether the path is empty (its size equals zero).
  BL_INLINE bool empty() const noexcept { return impl->size == 0; }

  //! Gets whether this path and the `other` path are equal.
  //!
  //! The equality check is deep. The data of both paths is examined and binary
  //! compared (thus a slight difference like -0 and +0 would make the equality
  //! check to fail).
  BL_INLINE bool equals(const BLPath& other) const noexcept { return blPathEquals(this, &other); }

  //! \}

  //! \name Path Content
  //! \{

  //! Gets path size (count of vertices used).
  BL_INLINE size_t size() const noexcept { return impl->size; }
  //! Gets path capacity (count of allocated vertices).
  BL_INLINE size_t capacity() const noexcept { return impl->capacity; }

  //! Gets path's vertex data (read-only).
  BL_INLINE const BLPoint* vertexData() const noexcept { return impl->vertexData; }
  //! Gets end of path's vertex data (read-only).
  BL_INLINE const BLPoint* vertexDataEnd() const noexcept { return impl->vertexData + impl->size; }

  //! Gets path's command data (read-only).
  BL_INLINE const uint8_t* commandData() const noexcept { return impl->commandData; }
  //! Gets end of path's command data (read-only).
  BL_INLINE const uint8_t* commandDataEnd() const noexcept { return impl->commandData + impl->size; }

  //! Gets the path data as a read-only `BLPathView`.
  BL_INLINE const BLPathView& view() const noexcept { return impl->view; }

  //! \}

  //! \name Path Construction
  //! \{

  BL_INLINE BLResult clear() noexcept {
    return blPathClear(this);
  }

  BL_INLINE BLResult shrink() noexcept {
    return blPathShrink(this);
  }

  BL_INLINE BLResult reserve(size_t n) noexcept {
    return blPathReserve(this, n);
  }

  BL_INLINE BLResult modifyOp(uint32_t op, size_t n, uint8_t** cmdDataOut, BLPoint** vtxDataOut) noexcept {
    return blPathModifyOp(this, op, n, cmdDataOut, vtxDataOut);
  }

  BL_INLINE BLResult setVertexAt(size_t index, uint32_t cmd, const BLPoint& pt) noexcept {
    return blPathSetVertexAt(this, index, cmd, pt.x, pt.y);
  }

  BL_INLINE BLResult setVertexAt(size_t index, uint32_t cmd, double x, double y) noexcept {
    return blPathSetVertexAt(this, index, cmd, x, y);
  }

  //! Moves to `p0`.
  BL_INLINE BLResult moveTo(const BLPoint& p0) noexcept {
    return blPathMoveTo(this, p0.x, p0.y);
  }

  //! Moves to `[x0, y0]`.
  BL_INLINE BLResult moveTo(double x0, double y0) noexcept {
    return blPathMoveTo(this, x0, y0);
  }

  //! Adds line to `p1`.
  BL_INLINE BLResult lineTo(const BLPoint& p1) noexcept {
    return blPathLineTo(this, p1.x, p1.y);
  }

  //! Adds line to `[x1, y1]`.
  BL_INLINE BLResult lineTo(double x1, double y1) noexcept {
    return blPathLineTo(this, x1, y1);
  }

  //! Adds a polyline (LineTo) of the given `poly` array of size `count`.
  BL_INLINE BLResult polyTo(const BLPoint* poly, size_t count) noexcept {
    return blPathPolyTo(this, poly, count);
  }

  //! Adds a quadratic curve to `p1` and `p2`.
  BL_INLINE BLResult quadTo(const BLPoint& p1, const BLPoint& p2) noexcept {
    return blPathQuadTo(this, p1.x, p1.y, p2.x, p2.y);
  }

  //! Adds a quadratic curve to `[x1, y1]` and `[x2, y2]`.
  BL_INLINE BLResult quadTo(double x1, double y1, double x2, double y2) noexcept {
    return blPathQuadTo(this, x1, y1, x2, y2);
  }

  //! Adds a cubic curve to `p1`, `p2`, `p3`.
  BL_INLINE BLResult cubicTo(const BLPoint& p1, const BLPoint& p2, const BLPoint& p3) noexcept {
    return blPathCubicTo(this, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
  }

  //! Adds a cubic curve to `[x1, y1]`, `[x2, y2]`, and `[x3, y3]`.
  BL_INLINE BLResult cubicTo(double x1, double y1, double x2, double y2, double x3, double y3) noexcept {
    return blPathCubicTo(this, x1, y1, x2, y2, x3, y3);
  }

  //! Adds a smooth quadratic curve to `p2`, calculating `p1` from last points.
  BL_INLINE BLResult smoothQuadTo(const BLPoint& p2) noexcept {
    return blPathSmoothQuadTo(this, p2.x, p2.y);
  }

  //! Adds a smooth quadratic curve to `[x2, y2]`, calculating `[x1, y1]` from last points.
  BL_INLINE BLResult smoothQuadTo(double x2, double y2) noexcept {
    return blPathSmoothQuadTo(this, x2, y2);
  }

  //! Adds a smooth cubic curve to `p2` and `p3`, calculating `p1` from last points.
  BL_INLINE BLResult smoothCubicTo(const BLPoint& p2, const BLPoint& p3) noexcept {
    return blPathSmoothCubicTo(this, p2.x, p2.y, p3.x, p3.y);
  }

  //! Adds a smooth cubic curve to `[x2, y2]` and `[x3, y3]`, calculating `[x1, y1]` from last points.
  BL_INLINE BLResult smoothCubicTo(double x2, double y2, double x3, double y3) noexcept {
    return blPathSmoothCubicTo(this, x2, y2, x3, y3);
  }

  BL_INLINE BLResult arcTo(const BLPoint& cp, const BLPoint& rp, double start, double sweep, bool forceMoveTo = false) noexcept {
    return blPathArcTo(this, cp.x, cp.y, rp.x, rp.y, start, sweep, forceMoveTo);
  }

  BL_INLINE BLResult arcTo(double cx, double cy, double rx, double ry, double start, double sweep, bool forceMoveTo = false) noexcept {
    return blPathArcTo(this, cx, cy, rx, ry, start, sweep, forceMoveTo);
  }

  BL_INLINE BLResult arcQuadrantTo(const BLPoint& p1, const BLPoint& p2) noexcept {
    return blPathArcQuadrantTo(this, p1.x, p1.y, p2.x, p2.y);
  }

  BL_INLINE BLResult arcQuadrantTo(double x1, double y1, double x2, double y2) noexcept {
    return blPathArcQuadrantTo(this, x1, y1, x2, y2);
  }

  BL_INLINE BLResult ellipticArcTo(const BLPoint& rp, double xAxisRotation, bool largeArcFlag, bool sweepFlag, const BLPoint& p1) noexcept {
    return blPathEllipticArcTo(this, rp.x, rp.y, xAxisRotation, largeArcFlag, sweepFlag, p1.x, p1.y);
  }

  BL_INLINE BLResult ellipticArcTo(double rx, double ry, double xAxisRotation, bool largeArcFlag, bool sweepFlag, double x1, double y1) noexcept {
    return blPathEllipticArcTo(this, rx, ry, xAxisRotation, largeArcFlag, sweepFlag, x1, y1);
  }

  BL_INLINE BLResult close() noexcept { return blPathClose(this); }

  //! \}

  //! \name Adding Figures
  //! \{

  //! Adds a closed rectangle to the path specified by `box`.
  BL_INLINE BLResult addBox(const BLBoxI& box, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return blPathAddBoxI(this, &box, dir);
  }

  //! Adds a closed rectangle to the path specified by `box`.
  BL_INLINE BLResult addBox(const BLBox& box, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return blPathAddBoxD(this, &box, dir);
  }

  //! Adds a closed rectangle to the path specified by `[x0, y0, x1, y1]`.
  BL_INLINE BLResult addBox(double x0, double y0, double x1, double y1, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addBox(BLBox(x0, y0, x1, y1), dir);
  }

  //! Adds a closed rectangle to the path specified by `rect`.
  BL_INLINE BLResult addRect(const BLRectI& rect, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return blPathAddRectI(this, &rect, dir);
  }

  //! Adds a closed rectangle to the path specified by `rect`.
  BL_INLINE BLResult addRect(const BLRect& rect, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return blPathAddRectD(this, &rect, dir);
  }

  //! Adds a closed rectangle to the path specified by `[x, y, w, h]`.
  BL_INLINE BLResult addRect(double x, double y, double w, double h, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addRect(BLRect(x, y, w, h), dir);
  }

  //! Adds a geometry to the path.
  BL_INLINE BLResult addGeometry(uint32_t geometryType, const void* geometryData, const BLMatrix2D* m = nullptr, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return blPathAddGeometry(this, geometryType, geometryData, m, dir);
  }

  //! Adds a closed circle to the path.
  BL_INLINE BLResult addCircle(const BLCircle& circle, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_CIRCLE, &circle, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addCircle(const BLCircle& circle, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_CIRCLE, &circle, &m, dir);
  }

  //! Adds a closed ellipse to the path.
  BL_INLINE BLResult addEllipse(const BLEllipse& ellipse, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ELLIPSE, &ellipse, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addEllipse(const BLEllipse& ellipse, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ELLIPSE, &ellipse, &m, dir);
  }

  //! Adds a closed rounded ractangle to the path.
  BL_INLINE BLResult addRoundRect(const BLRoundRect& rr, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ROUND_RECT, &rr, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addRoundRect(const BLRoundRect& rr, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ROUND_RECT, &rr, &m, dir);
  }

  //! Adds an unclosed arc to the path.
  BL_INLINE BLResult addArc(const BLArc& arc, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARC, &arc, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addArc(const BLArc& arc, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARC, &arc, &m, dir);
  }

  //! Adds a closed chord to the path.
  BL_INLINE BLResult addChord(const BLArc& chord, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_CHORD, &chord, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addChord(const BLArc& chord, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_CHORD, &chord, &m, dir);
  }

  //! Adds a closed pie to the path.
  BL_INLINE BLResult addPie(const BLArc& pie, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_PIE, &pie, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addPie(const BLArc& pie, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_PIE, &pie, &m, dir);
  }

  //! Adds an unclosed line to the path.
  BL_INLINE BLResult addLine(const BLLine& line, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_LINE, &line, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addLine(const BLLine& line, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_LINE, &line, &m, dir);
  }

  //! Adds a closed triangle.
  BL_INLINE BLResult addTriangle(const BLTriangle& triangle, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_TRIANGLE, &triangle, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addTriangle(const BLTriangle& triangle, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_TRIANGLE, &triangle, &m, dir);
  }

  //! Adds a polyline.
  BL_INLINE BLResult addPolyline(const BLArrayView<BLPointI>& poly, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYLINEI, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLArrayView<BLPointI>& poly, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYLINEI, &poly, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLPointI* poly, size_t n, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolyline(BLArrayView<BLPointI>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLPointI* poly, size_t n, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolyline(BLArrayView<BLPointI>{poly, n}, m, dir);
  }

  //! Adds a polyline.
  BL_INLINE BLResult addPolyline(const BLArrayView<BLPoint>& poly, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYLINED, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLArrayView<BLPoint>& poly, const BLMatrix2D& m, uint32_t dir) {
    return addGeometry(BL_GEOMETRY_TYPE_POLYLINED, &poly, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLPoint* poly, size_t n, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolyline(BLArrayView<BLPoint>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLPoint* poly, size_t n, const BLMatrix2D& m, uint32_t dir) {
    return addPolyline(BLArrayView<BLPoint>{poly, n}, m, dir);
  }

  //! Adds a polygon.
  BL_INLINE BLResult addPolygon(const BLArrayView<BLPointI>& poly, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYGONI, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLArrayView<BLPointI>& poly, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYGONI, &poly, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLPointI* poly, size_t n, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolygon(BLArrayView<BLPointI>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLPointI* poly, size_t n, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolygon(BLArrayView<BLPointI>{poly, n}, m, dir);
  }

  //! Adds a polygon.
  BL_INLINE BLResult addPolygon(const BLArrayView<BLPoint>& poly, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYGOND, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLArrayView<BLPoint>& poly, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYGOND, &poly, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLPoint* poly, size_t n, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolygon(BLArrayView<BLPoint>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLPoint* poly, size_t n, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolygon(BLArrayView<BLPoint>{poly, n}, m, dir);
  }

  //! Adds an array of closed boxes.
  BL_INLINE BLResult addBoxArray(const BLArrayView<BLBoxI>& array, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLArrayView<BLBoxI>& array, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI, &array, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLBoxI* data, size_t n, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addBoxArray(BLArrayView<BLBoxI>{data, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLBoxI* data, size_t n, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addBoxArray(BLArrayView<BLBoxI>{data, n}, m, dir);
  }

  //! Adds an array of closed boxes.
  BL_INLINE BLResult addBoxArray(const BLArrayView<BLBox>& array, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLArrayView<BLBox>& array, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD, &array, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLBox* data, size_t n, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addBoxArray(BLArrayView<BLBox>{data, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLBox* data, size_t n, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addBoxArray(BLArrayView<BLBox>{data, n}, m, dir);
  }

  //! Adds an array of closed rectangles.
  BL_INLINE BLResult addRectArray(const BLArrayView<BLRectI>& array, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLArrayView<BLRectI>& array, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI, &array, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLRectI* data, size_t n, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addRectArray(BLArrayView<BLRectI>{data, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLRectI* data, size_t n, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addRectArray(BLArrayView<BLRectI>{data, n}, m, dir);
  }

  //! Adds an array of closed rectangles.
  BL_INLINE BLResult addRectArray(const BLArrayView<BLRect>& array, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLArrayView<BLRect>& array, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD, &array, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLRect* data, size_t n, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addRectArray(BLArrayView<BLRect>{data, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLRect* data, size_t n, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addRectArray(BLArrayView<BLRect>{data, n}, m, dir);
  }

  //! Adds a closed region (converted to set of rectangles).
  BL_INLINE BLResult addRegion(const BLRegion& region, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_REGION, &region, nullptr, dir);
  }

  //! Adds a closed region (converted to set of rectangles).
  BL_INLINE BLResult addRegion(const BLRegion& region, const BLMatrix2D& m, uint32_t dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_REGION, &region, &m, dir);
  }

  //! \}

  //! \name Adding Paths
  //! \{

  //! Adds other `path` to this path.
  BL_INLINE BLResult addPath(const BLPath& path) noexcept {
    return blPathAddPath(this, &path, nullptr);
  }

  //! Adds other `path` sliced by the given `range` to this path.
  BL_INLINE BLResult addPath(const BLPath& path, const BLRange& range) noexcept {
    return blPathAddPath(this, &path, &range);
  }

  //! Adds other `path` translated by `p` to this path.
  BL_INLINE BLResult addPath(const BLPath& path, const BLPoint& p) noexcept {
    return blPathAddTranslatedPath(this, &path, nullptr, &p);
  }

  //! Adds other `path` translated by `p` and sliced by the given `range` to this path.
  BL_INLINE BLResult addPath(const BLPath& path, const BLRange& range, const BLPoint& p) noexcept {
    return blPathAddTranslatedPath(this, &path, &range, &p);
  }

  //! Adds other `path` transformed by `m` to this path.
  BL_INLINE BLResult addPath(const BLPath& path, const BLMatrix2D& m) noexcept {
    return blPathAddTransformedPath(this, &path, nullptr, &m);
  }

  //! Adds other `path` transformed by `m` and sliced by the given `range` to this path.
  BL_INLINE BLResult addPath(const BLPath& path, const BLRange& range, const BLMatrix2D& m) noexcept {
    return blPathAddTransformedPath(this, &path, &range, &m);
  }

  //! Adds other `path`, but reversed.
  BL_INLINE BLResult addReversedPath(const BLPath& path, uint32_t reverseMode) noexcept {
    return blPathAddReversedPath(this, &path, nullptr, reverseMode);
  }

  //! Adds other `path`, but reversed.
  BL_INLINE BLResult addReversedPath(const BLPath& path, const BLRange& range, uint32_t reverseMode) noexcept {
    return blPathAddReversedPath(this, &path, &range, reverseMode);
  }

  //! Adds a stroke of `path` to this path.
  BL_INLINE BLResult addStrokedPath(const BLPath& path, const BLRange& range, const BLStrokeOptions& strokeOptions, const BLApproximationOptions& approximationOptions) noexcept {
    return blPathAddStrokedPath(this, &path, &range, &strokeOptions, &approximationOptions);
  }
  //! \overload
  BL_INLINE BLResult addStrokedPath(const BLPath& path, const BLStrokeOptions& strokeOptions, const BLApproximationOptions& approximationOptions) noexcept {
    return blPathAddStrokedPath(this, &path, nullptr, &strokeOptions, &approximationOptions);
  }

  //! \}

  //! \name Transformations
  //! \{

  //! Translates the whole path by `p`.
  BL_INLINE BLResult translate(const BLPoint& p) noexcept {
    return blPathTranslate(this, nullptr, &p);
  }

  //! Translates a part of the path specified by the given `range` by `p`.
  BL_INLINE BLResult translate(const BLRange& range, const BLPoint& p) noexcept {
    return blPathTranslate(this, &range, &p);
  }

  //! Transforms the whole path by matrix `m`.
  BL_INLINE BLResult transform(const BLMatrix2D& m) noexcept {
    return blPathTransform(this, nullptr, &m);
  }

  //! Transforms a part of the path specified by the given `range` by matrix `m`.
  BL_INLINE BLResult transform(const BLRange& range, const BLMatrix2D& m) noexcept {
    return blPathTransform(this, &range, &m);
  }

  //! Fits the whole path into the given `rect` by taking into account fit flags passed by `fitFlags`.
  BL_INLINE BLResult fitTo(const BLRect& rect, uint32_t fitFlags) noexcept {
    return blPathFitTo(this, nullptr, &rect, fitFlags);
  }

  //! Fits a parh of the path specified by the given `range` into the given `rect` by taking into account fit flags passed by `fitFlags`.
  BL_INLINE BLResult fitTo(const BLRange& range, const BLRect& rect, uint32_t fitFlags) noexcept {
    return blPathFitTo(this, &range, &rect, fitFlags);
  }

  //! \}

  //! \name Path Information
  //! \{

  //! Update a path information if necessary.
  BL_INLINE BLResult getInfoFlags(uint32_t* flagsOut) const noexcept {
    return blPathGetInfoFlags(this, flagsOut);
  }

  //! Stores a bounding box of all vertices and control points to `boxOut`.
  //!
  //! Control box is simply bounds of all vertices the path has without further
  //! processing. It contains both on-path and off-path points. Consider using
  //! `getBoundingBox()` if you need a visual bounding box.
  BL_INLINE BLResult getControlBox(BLBox* boxOut) const noexcept {
    return blPathGetControlBox(this, boxOut);
  }

  //! Stores a bounding box of all on-path vertices and curve extremas to `boxOut`.
  //!
  //! The bounding box stored to `boxOut` could be smaller than a bounding box
  //! obtained by `getControlBox()` as it's calculated by merging only start/end
  //! points and curves at their extremas (not control points). The resulting
  //! bounding box represents a visual bounds of the path.
  BL_INLINE BLResult getBoundingBox(BLBox* boxOut) const noexcept {
    return blPathGetBoundingBox(this, boxOut);
  }

  //! Gets a range describing a figure at the given `index`.
  BL_INLINE BLResult getFigureRange(size_t index, BLRange* rangeOut) const noexcept {
    return blPathGetFigureRange(this, index, rangeOut);
  }

  //! Gets the last vertex of the path and stores it to `vtxOut`. If the very
  //! last command of the path is `BL_PATH_CMD_CLOSE` then the path will be
  //! iterated in reverse order to match the initial vertex of the last figure.
  BL_INLINE BLResult getLastVertex(BLPoint* vtxOut) noexcept {
    return blPathGetLastVertex(this, vtxOut);
  }

  BL_INLINE BLResult getClosestVertex(const BLPoint& p, double maxDistance, size_t* indexOut) const noexcept {
    double distanceOut;
    return blPathGetClosestVertex(this, &p, maxDistance, indexOut, &distanceOut);
  }

  BL_INLINE BLResult getClosestVertex(const BLPoint& p, double maxDistance, size_t* indexOut, double* distanceOut) const noexcept {
    return blPathGetClosestVertex(this, &p, maxDistance, indexOut, distanceOut);
  }

  //! \}

  //! \name Hit Testing
  //! \{

  //! Hit tests the given point `p` by respecting the given `fillRule`.
  BL_INLINE uint32_t hitTest(const BLPoint& p, uint32_t fillRule) const noexcept {
    return blPathHitTest(this, &p, fillRule);
  }

  //! \}

  static BL_INLINE const BLPath& none() noexcept { return reinterpret_cast<const BLPath*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_BLPATH_H
