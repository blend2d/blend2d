// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH_H_INCLUDED
#define BLEND2D_PATH_H_INCLUDED

#include "array.h"
#include "geometry.h"
#include "object.h"

//! \addtogroup blend2d_api_geometry
//! \{

//! \name BLPath and BLStroke Constants
//! \{

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

  //! Maximum value of `BLPathCmd`.
  BL_PATH_CMD_MAX_VALUE = 4

  BL_FORCE_ENUM_UINT32(BL_PATH_CMD)
};

//! Path command (never stored in path).
BL_DEFINE_ENUM(BLPathCmdExtra) {
  //! Used by `BLPath::setVertexAt` to preserve the current command value.
  BL_PATH_CMD_PRESERVE = 0xFFFFFFFFu
};

//! Path flags.
BL_DEFINE_ENUM(BLPathFlags) {
  //! No flags.
  BL_PATH_NO_FLAGS = 0u,
  //! Path is empty (no commands or close commands only).
  BL_PATH_FLAG_EMPTY = 0x00000001u,
  //! Path contains multiple figures.
  BL_PATH_FLAG_MULTIPLE = 0x00000002u,
  //! Path contains one or more quad curves.
  BL_PATH_FLAG_QUADS = 0x00000004u,
  //! Path contains one or more cubic curves.
  BL_PATH_FLAG_CUBICS = 0x00000008u,
  //! Path is invalid.
  BL_PATH_FLAG_INVALID = 0x40000000u,
  //! Flags are dirty (not reflecting the current status).
  BL_PATH_FLAG_DIRTY = 0x80000000u

  BL_FORCE_ENUM_UINT32(BL_PATH_FLAG)
};

//! Path reversal mode.
BL_DEFINE_ENUM(BLPathReverseMode) {
  //! Reverse each figure and their order as well (default).
  BL_PATH_REVERSE_MODE_COMPLETE = 0,
  //! Reverse each figure separately (keeps their order).
  BL_PATH_REVERSE_MODE_SEPARATE = 1,

  //! Maximum value of `BLPathReverseMode`.
  BL_PATH_REVERSE_MODE_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_PATH_REVERSE_MODE)
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

  //! Maximum value of `BLStrokeJoin`.
  BL_STROKE_JOIN_MAX_VALUE = 4

  BL_FORCE_ENUM_UINT32(BL_STROKE_JOIN)
};

//! Position of a stroke-cap.
BL_DEFINE_ENUM(BLStrokeCapPosition) {
  //! Start of the path.
  BL_STROKE_CAP_POSITION_START = 0,
  //! End of the path.
  BL_STROKE_CAP_POSITION_END = 1,

  //! Maximum value of `BLStrokeCapPosition`.
  BL_STROKE_CAP_POSITION_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_STROKE_CAP_POSITION)
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

  //! Maximum value of `BLStrokeCap`.
  BL_STROKE_CAP_MAX_VALUE = 5

  BL_FORCE_ENUM_UINT32(BL_STROKE_CAP)
};

//! Stroke transform order.
BL_DEFINE_ENUM(BLStrokeTransformOrder) {
  //! Transform after stroke  => `Transform(Stroke(Input))` [default].
  BL_STROKE_TRANSFORM_ORDER_AFTER = 0,
  //! Transform before stroke => `Stroke(Transform(Input))`.
  BL_STROKE_TRANSFORM_ORDER_BEFORE = 1,

  //! Maximum value of `BLStrokeTransformOrder`.
  BL_STROKE_TRANSFORM_ORDER_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_STROKE_TRANSFORM_ORDER)
};

//! Mode that specifies how curves are approximated to line segments.
BL_DEFINE_ENUM(BLFlattenMode) {
  //! Use default mode (decided by Blend2D).
  BL_FLATTEN_MODE_DEFAULT = 0,
  //! Recursive subdivision flattening.
  BL_FLATTEN_MODE_RECURSIVE = 1,

  //! Maximum value of `BLFlattenMode`.
  BL_FLATTEN_MODE_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_FLATTEN_MODE)
};

//! Mode that specifies how to construct offset curves.
BL_DEFINE_ENUM(BLOffsetMode) {
  //! Use default mode (decided by Blend2D).
  BL_OFFSET_MODE_DEFAULT = 0,
  //! Iterative offset construction.
  BL_OFFSET_MODE_ITERATIVE = 1,

  //! Maximum value of `BLOffsetMode`.
  BL_OFFSET_MODE_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_OFFSET_MODE)
};

//! \}

//! \name BLPath and related C/C++ Structs
//! \{

//! Options used to describe how geometry is approximated.
//!
//! This struct cannot be simply zeroed and then passed to functions that accept approximation options.
//! Use `blDefaultApproximationOptions` to setup defaults and then alter values you want to change.
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
  //! Specifies how curves are flattened, see `FlattenMode`.
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

//! 2D vector path view provides pointers to vertex and command data along with their size.
struct BLPathView {
  const uint8_t* commandData;
  const BLPoint* vertexData;
  size_t size;

#ifdef __cplusplus
  BL_INLINE void reset() noexcept {
    commandData = nullptr;
    vertexData = nullptr;
    size = 0;
  }

  BL_INLINE void reset(const uint8_t* commandDataIn, const BLPoint* vertexDataIn, size_t sizeIn) noexcept {
    commandData = commandDataIn;
    vertexData = vertexDataIn;
    size = sizeIn;
  }
#endif
};

//! \}

//! 2D vector path [Impl].
struct BLPathImpl BL_CLASS_INHERITS(BLObjectImpl) {
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

  //! Path vertex/command capacity.
  size_t capacity;

  //! Path flags related to caching.
  volatile uint32_t flags;
};

//! \name BLPath - C API
//!
//! \{

//! 2D vector path [C API].
struct BLPathCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
};

//! Stroke options [C API].
struct BLStrokeOptionsCore {
  union {
    struct {
      uint8_t startCap;
      uint8_t endCap;
      uint8_t join;
      uint8_t transformOrder;
      uint8_t reserved[4];
    };
    uint8_t caps[BL_STROKE_CAP_POSITION_MAX_VALUE + 1];
    uint64_t hints;
  };

  double width;
  double miterLimit;
  double dashOffset;
  BL_TYPED_MEMBER(BLArrayCore, BLArray<double>, dashArray);

  BL_HAS_TYPED_MEMBERS(BLStrokeOptionsCore)
};

//! Optional callback that can be used to consume a path data.
typedef BLResult (BL_CDECL* BLPathSinkFunc)(BLPathCore* path, const void* info, void* closure) BL_NOEXCEPT;

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blPathInit(BLPathCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathInitMove(BLPathCore* self, BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathInitWeak(BLPathCore* self, const BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathDestroy(BLPathCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathReset(BLPathCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blPathGetSize(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL blPathGetCapacity(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const uint8_t* BL_CDECL blPathGetCommandData(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const BLPoint* BL_CDECL blPathGetVertexData(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blPathClear(BLPathCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathShrink(BLPathCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathReserve(BLPathCore* self, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathModifyOp(BLPathCore* self, BLModifyOp op, size_t n, uint8_t** cmdDataOut, BLPoint** vtxDataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAssignMove(BLPathCore* self, BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAssignWeak(BLPathCore* self, const BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAssignDeep(BLPathCore* self, const BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathSetVertexAt(BLPathCore* self, size_t index, uint32_t cmd, double x, double y) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathMoveTo(BLPathCore* self, double x0, double y0) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathLineTo(BLPathCore* self, double x1, double y1) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathPolyTo(BLPathCore* self, const BLPoint* poly, size_t count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathQuadTo(BLPathCore* self, double x1, double y1, double x2, double y2) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathCubicTo(BLPathCore* self, double x1, double y1, double x2, double y2, double x3, double y3) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathSmoothQuadTo(BLPathCore* self, double x2, double y2) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathSmoothCubicTo(BLPathCore* self, double x2, double y2, double x3, double y3) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathArcTo(BLPathCore* self, double x, double y, double rx, double ry, double start, double sweep, bool forceMoveTo) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathArcQuadrantTo(BLPathCore* self, double x1, double y1, double x2, double y2) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathEllipticArcTo(BLPathCore* self, double rx, double ry, double xAxisRotation, bool largeArcFlag, bool sweepFlag, double x1, double y1) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathClose(BLPathCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAddGeometry(BLPathCore* self, BLGeometryType geometryType, const void* geometryData, const BLMatrix2D* m, BLGeometryDirection dir) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAddBoxI(BLPathCore* self, const BLBoxI* box, BLGeometryDirection dir) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAddBoxD(BLPathCore* self, const BLBox* box, BLGeometryDirection dir) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAddRectI(BLPathCore* self, const BLRectI* rect, BLGeometryDirection dir) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAddRectD(BLPathCore* self, const BLRect* rect, BLGeometryDirection dir) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAddPath(BLPathCore* self, const BLPathCore* other, const BLRange* range) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAddTranslatedPath(BLPathCore* self, const BLPathCore* other, const BLRange* range, const BLPoint* p) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAddTransformedPath(BLPathCore* self, const BLPathCore* other, const BLRange* range, const BLMatrix2D* m) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAddReversedPath(BLPathCore* self, const BLPathCore* other, const BLRange* range, BLPathReverseMode reverseMode) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathAddStrokedPath(BLPathCore* self, const BLPathCore* other, const BLRange* range, const BLStrokeOptionsCore* options, const BLApproximationOptions* approx) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathRemoveRange(BLPathCore* self, const BLRange* range) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathTranslate(BLPathCore* self, const BLRange* range, const BLPoint* p) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathTransform(BLPathCore* self, const BLRange* range, const BLMatrix2D* m) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathFitTo(BLPathCore* self, const BLRange* range, const BLRect* rect, uint32_t fitFlags) BL_NOEXCEPT_C;
BL_API bool     BL_CDECL blPathEquals(const BLPathCore* a, const BLPathCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathGetInfoFlags(const BLPathCore* self, uint32_t* flagsOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathGetControlBox(const BLPathCore* self, BLBox* boxOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathGetBoundingBox(const BLPathCore* self, BLBox* boxOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathGetFigureRange(const BLPathCore* self, size_t index, BLRange* rangeOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathGetLastVertex(const BLPathCore* self, BLPoint* vtxOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPathGetClosestVertex(const BLPathCore* self, const BLPoint* p, double maxDistance, size_t* indexOut, double* distanceOut) BL_NOEXCEPT_C;
BL_API BLHitTest BL_CDECL blPathHitTest(const BLPathCore* self, const BLPoint* p, BLFillRule fillRule) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blStrokeOptionsInit(BLStrokeOptionsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStrokeOptionsInitMove(BLStrokeOptionsCore* self, BLStrokeOptionsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStrokeOptionsInitWeak(BLStrokeOptionsCore* self, const BLStrokeOptionsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStrokeOptionsDestroy(BLStrokeOptionsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStrokeOptionsReset(BLStrokeOptionsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStrokeOptionsAssignMove(BLStrokeOptionsCore* self, BLStrokeOptionsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStrokeOptionsAssignWeak(BLStrokeOptionsCore* self, const BLStrokeOptionsCore* other) BL_NOEXCEPT_C;

BL_END_C_DECLS
//! \}

//! \name BLPath and Related Globals
//!
//! 2D path functionality is provided by \ref BLPathCore in C API and wrapped by \ref BLPath in C++ API.
//!
//! \{
BL_BEGIN_C_DECLS

//! Default approximation options used by Blend2D.
extern BL_API const BLApproximationOptions blDefaultApproximationOptions;

BL_END_C_DECLS
//! \}

//! \name BLPath - C++ API
//! \{
#ifdef __cplusplus

// Prevents the following:
//   Base class XXX should be explicitly initialized in the copy constructor [-Wextra]
BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_EXTRA_WARNINGS)

//! Stroke options [C++ API].
//!
//! You should use this as a structure and use members of `BLStrokeOptionsCore` directly.
class BLStrokeOptions : public BLStrokeOptionsCore {
public:
  BL_INLINE BLStrokeOptions() noexcept { blStrokeOptionsInit(this); }
  BL_INLINE BLStrokeOptions(BLStrokeOptions&& other) noexcept { blStrokeOptionsInitMove(this, &other); }
  BL_INLINE BLStrokeOptions(const BLStrokeOptions& other) noexcept { blStrokeOptionsInitWeak(this, &other); }
  BL_INLINE ~BLStrokeOptions() noexcept { blStrokeOptionsDestroy(this); }

  BL_INLINE BLStrokeOptions& operator=(BLStrokeOptions&& other) noexcept { blStrokeOptionsAssignMove(this, &other); return *this; }
  BL_INLINE BLStrokeOptions& operator=(const BLStrokeOptions& other) noexcept { blStrokeOptionsAssignWeak(this, &other); return *this; }

  BL_INLINE BLResult reset() noexcept { return blStrokeOptionsReset(this); }

  BL_INLINE BLResult assign(BLStrokeOptions&& other) noexcept { return blStrokeOptionsAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLStrokeOptions& other) noexcept { return blStrokeOptionsAssignWeak(this, &other); }

  BL_INLINE void setCaps(BLStrokeCap strokeCap) noexcept {
    startCap = uint8_t(strokeCap);
    endCap = uint8_t(strokeCap);
  }
};

BL_DIAGNOSTIC_POP

//! 2D vector path [C++ API].
class BLPath : public BLPathCore {
public:
  //! \cond INTERNAL
  BL_INLINE BLPathImpl* _impl() const noexcept { return static_cast<BLPathImpl*>(_d.impl); }
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLPath() noexcept { blPathInit(this); }
  BL_INLINE BLPath(BLPath&& other) noexcept { blPathInitMove(this, &other); }
  BL_INLINE BLPath(const BLPath& other) noexcept { blPathInitWeak(this, &other); }
  BL_INLINE ~BLPath() noexcept { blPathDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !empty(); }

  BL_INLINE BLPath& operator=(BLPath&& other) noexcept { blPathAssignMove(this, &other); return *this; }
  BL_INLINE BLPath& operator=(const BLPath& other) noexcept { blPathAssignWeak(this, &other); return *this; }

  BL_NODISCARD BL_INLINE bool operator==(const BLPath& other) const noexcept { return  equals(other); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLPath& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blPathReset(this); }
  BL_INLINE void swap(BLPathCore& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the path is empty, which means its size equals to zero.
  BL_NODISCARD
  BL_INLINE bool empty() const noexcept { return size() == 0; }

  //! Returns path size (count of vertices used).
  BL_NODISCARD
  BL_INLINE size_t size() const noexcept { return _impl()->size; }

  //! Returns path capacity (count of allocated vertices).
  BL_NODISCARD
  BL_INLINE size_t capacity() const noexcept { return _impl()->capacity; }

  //! Returns path's vertex data (read-only).
  BL_NODISCARD
  BL_INLINE const BLPoint* vertexData() const noexcept { return _impl()->vertexData; }

  //! Returns the end of path's vertex data (read-only).
  BL_NODISCARD
  BL_INLINE const BLPoint* vertexDataEnd() const noexcept { return _impl()->vertexData + _impl()->size; }

  //! Returns path's command data (read-only).
  BL_NODISCARD
  BL_INLINE const uint8_t* commandData() const noexcept { return _impl()->commandData; }

  //! Returns the end of path's command data (read-only).
  BL_NODISCARD
  BL_INLINE const uint8_t* commandDataEnd() const noexcept { return _impl()->commandData + _impl()->size; }

  //! Returns a read-only path data as `BLPathView`.
  BL_NODISCARD
  BL_INLINE BLPathView view() const noexcept { return _impl()->view; }

  //! \}

  //! \name Path Construction
  //! \{

  //! Clears the content of the path.
  BL_INLINE BLResult clear() noexcept {
    return blPathClear(this);
  }

  //! Shrinks the capacity of the path to fit the current usage.
  BL_INLINE BLResult shrink() noexcept {
    return blPathShrink(this);
  }

  //! Reserves the capacity of the path for at least `n` vertices and commands.
  BL_INLINE BLResult reserve(size_t n) noexcept {
    return blPathReserve(this, n);
  }

  BL_INLINE BLResult modifyOp(BLModifyOp op, size_t n, uint8_t** cmdDataOut, BLPoint** vtxDataOut) noexcept {
    return blPathModifyOp(this, op, n, cmdDataOut, vtxDataOut);
  }

  BL_INLINE BLResult assign(BLPathCore&& other) noexcept { return blPathAssignMove(this, &other);  }
  BL_INLINE BLResult assign(const BLPathCore& other) noexcept { return blPathAssignWeak(this, &other);  }
  BL_INLINE BLResult assignDeep(const BLPathCore& other) noexcept { return blPathAssignDeep(this, &other); }

  //! Sets vertex at `index` to `cmd` and `pt`.
  //!
  //! Pass `BL_PATH_CMD_PRESERVE` in `cmd` to preserve the current command.
  BL_INLINE BLResult setVertexAt(size_t index, uint32_t cmd, const BLPoint& pt) noexcept {
    return blPathSetVertexAt(this, index, cmd, pt.x, pt.y);
  }

  //! Sets vertex at `index` to `cmd` and `[x, y]`.
  //!
  //! Pass `BL_PATH_CMD_PRESERVE` in `cmd` to preserve the current command.
  BL_INLINE BLResult setVertexAt(size_t index, uint32_t cmd, double x, double y) noexcept {
    return blPathSetVertexAt(this, index, cmd, x, y);
  }

  //! Moves to `p0`.
  //!
  //! Appends `BL_PATH_CMD_MOVE[p0]` command to the path.
  BL_INLINE BLResult moveTo(const BLPoint& p0) noexcept {
    return blPathMoveTo(this, p0.x, p0.y);
  }

  //! Moves to `[x0, y0]`.
  //!
  //! Appends `BL_PATH_CMD_MOVE[x0, y0]` command to the path.
  BL_INLINE BLResult moveTo(double x0, double y0) noexcept {
    return blPathMoveTo(this, x0, y0);
  }

  //! Adds line to `p1`.
  //!
  //! Appends `BL_PATH_CMD_ON[p1]` command to the path.
  BL_INLINE BLResult lineTo(const BLPoint& p1) noexcept {
    return blPathLineTo(this, p1.x, p1.y);
  }

  //! Adds line to `[x1, y1]`.
  //!
  //! Appends `BL_PATH_CMD_ON[x1, y1]` command to the path.
  BL_INLINE BLResult lineTo(double x1, double y1) noexcept {
    return blPathLineTo(this, x1, y1);
  }

  //! Adds a polyline (LineTo) of the given `poly` array of size `count`.
  //!
  //! Appends multiple `BL_PATH_CMD_ON[x[i], y[i]]` commands to the path depending on `count` parameter.
  BL_INLINE BLResult polyTo(const BLPoint* poly, size_t count) noexcept {
    return blPathPolyTo(this, poly, count);
  }

  //! Adds a quadratic curve to `p1` and `p2`.
  //!
  //! Appends the following commands to the path:
  //!   - `BL_PATH_CMD_QUAD[p1]`
  //!   - `BL_PATH_CMD_ON[p2]`
  //!
  //! Matches SVG 'Q' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataQuadraticBezierCommands
  BL_INLINE BLResult quadTo(const BLPoint& p1, const BLPoint& p2) noexcept {
    return blPathQuadTo(this, p1.x, p1.y, p2.x, p2.y);
  }

  //! Adds a quadratic curve to `[x1, y1]` and `[x2, y2]`.
  //!
  //! Appends the following commands to the path:
  //!   - `BL_PATH_CMD_QUAD[x1, y1]`
  //!   - `BL_PATH_CMD_ON[x2, y2]`
  //!
  //! Matches SVG 'Q' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataQuadraticBezierCommands
  BL_INLINE BLResult quadTo(double x1, double y1, double x2, double y2) noexcept {
    return blPathQuadTo(this, x1, y1, x2, y2);
  }

  //! Adds a cubic curve to `p1`, `p2`, `p3`.
  //!
  //! Appends the following commands to the path:
  //!   - `BL_PATH_CMD_CUBIC[p1]`
  //!   - `BL_PATH_CMD_CUBIC[p2]`
  //!   - `BL_PATH_CMD_ON[p3]`
  //!
  //! Matches SVG 'C' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataCubicBezierCommands
  BL_INLINE BLResult cubicTo(const BLPoint& p1, const BLPoint& p2, const BLPoint& p3) noexcept {
    return blPathCubicTo(this, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
  }

  //! Adds a cubic curve to `[x1, y1]`, `[x2, y2]`, and `[x3, y3]`.
  //!
  //! Appends the following commands to the path:
  //!   - `BL_PATH_CMD_CUBIC[x1, y1]`
  //!   - `BL_PATH_CMD_CUBIC[x2, y2]`
  //!   - `BL_PATH_CMD_ON[x3, y3]`
  //!
  //! Matches SVG 'C' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataCubicBezierCommands
  BL_INLINE BLResult cubicTo(double x1, double y1, double x2, double y2, double x3, double y3) noexcept {
    return blPathCubicTo(this, x1, y1, x2, y2, x3, y3);
  }

  //! Adds a smooth quadratic curve to `p2`, calculating `p1` from last points.
  //!
  //! Appends the following commands to the path:
  //!   - `BL_PATH_CMD_QUAD[calculated]`
  //!   - `BL_PATH_CMD_ON[p2]`
  //!
  //! Matches SVG 'T' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataQuadraticBezierCommands
  BL_INLINE BLResult smoothQuadTo(const BLPoint& p2) noexcept {
    return blPathSmoothQuadTo(this, p2.x, p2.y);
  }

  //! Adds a smooth quadratic curve to `[x2, y2]`, calculating `[x1, y1]` from last points.
  //!
  //! Appends the following commands to the path:
  //!   - `BL_PATH_CMD_QUAD[calculated]`
  //!   - `BL_PATH_CMD_ON[x2, y2]`
  //!
  //! Matches SVG 'T' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataQuadraticBezierCommands
  BL_INLINE BLResult smoothQuadTo(double x2, double y2) noexcept {
    return blPathSmoothQuadTo(this, x2, y2);
  }

  //! Adds a smooth cubic curve to `p2` and `p3`, calculating `p1` from last points.
  //!
  //! Appends the following commands to the path:
  //!   - `BL_PATH_CMD_CUBIC[calculated]`
  //!   - `BL_PATH_CMD_CUBIC[p2]`
  //!   - `BL_PATH_CMD_ON[p3]`
  //!
  //! Matches SVG 'S' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataCubicBezierCommands
  BL_INLINE BLResult smoothCubicTo(const BLPoint& p2, const BLPoint& p3) noexcept {
    return blPathSmoothCubicTo(this, p2.x, p2.y, p3.x, p3.y);
  }

  //! Adds a smooth cubic curve to `[x2, y2]` and `[x3, y3]`, calculating `[x1, y1]` from last points.
  //!
  //! Appends the following commands to the path:
  //!   - `BL_PATH_CMD_CUBIC[calculated]`
  //!   - `BL_PATH_CMD_CUBIC[x2, y2]`
  //!   - `BL_PATH_CMD_ON[x3, y3]`
  //!
  //! Matches SVG 'S' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataCubicBezierCommands
  BL_INLINE BLResult smoothCubicTo(double x2, double y2, double x3, double y3) noexcept {
    return blPathSmoothCubicTo(this, x2, y2, x3, y3);
  }

  //! Adds an arc to the path.
  //!
  //! The center of the arc is specified by `c` and radius by `r`. Both `start` and `sweep` angles are in radians.
  //! If the last vertex doesn't match the start of the arc then a `lineTo()` would be emitted before adding the arc.
  //! Pass `true` in `forceMoveTo` to always emit `moveTo()` at the beginning of the arc, which starts a new figure.
  BL_INLINE BLResult arcTo(const BLPoint& c, const BLPoint& r, double start, double sweep, bool forceMoveTo = false) noexcept {
    return blPathArcTo(this, c.x, c.y, r.x, r.y, start, sweep, forceMoveTo);
  }

  //! \overload
  BL_INLINE BLResult arcTo(double cx, double cy, double rx, double ry, double start, double sweep, bool forceMoveTo = false) noexcept {
    return blPathArcTo(this, cx, cy, rx, ry, start, sweep, forceMoveTo);
  }

  //! Adds an arc quadrant (90deg) to the path. The first point `p1` specifies
  //! the quadrant corner and the last point `p2` specifies the end point.
  BL_INLINE BLResult arcQuadrantTo(const BLPoint& p1, const BLPoint& p2) noexcept {
    return blPathArcQuadrantTo(this, p1.x, p1.y, p2.x, p2.y);
  }

  //! \overload
  BL_INLINE BLResult arcQuadrantTo(double x1, double y1, double x2, double y2) noexcept {
    return blPathArcQuadrantTo(this, x1, y1, x2, y2);
  }

  //! Adds an elliptic arc to the path that follows the SVG specification.
  //!
  //! Matches SVG 'A' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataEllipticalArcCommands
  BL_INLINE BLResult ellipticArcTo(const BLPoint& rp, double xAxisRotation, bool largeArcFlag, bool sweepFlag, const BLPoint& p1) noexcept {
    return blPathEllipticArcTo(this, rp.x, rp.y, xAxisRotation, largeArcFlag, sweepFlag, p1.x, p1.y);
  }

  //! \overload
  BL_INLINE BLResult ellipticArcTo(double rx, double ry, double xAxisRotation, bool largeArcFlag, bool sweepFlag, double x1, double y1) noexcept {
    return blPathEllipticArcTo(this, rx, ry, xAxisRotation, largeArcFlag, sweepFlag, x1, y1);
  }

  //! Closes the current figure.
  //!
  //! Appends `BL_PATH_CMD_CLOSE` to the path.
  //!
  //! Matches SVG 'Z' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataClosePathCommand
  BL_INLINE BLResult close() noexcept { return blPathClose(this); }

  //! \}

  //! \name Adding Figures
  //! \{

  //! Adds a closed rectangle to the path specified by `box`.
  BL_INLINE BLResult addBox(const BLBoxI& box, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return blPathAddBoxI(this, &box, dir);
  }

  //! Adds a closed rectangle to the path specified by `box`.
  BL_INLINE BLResult addBox(const BLBox& box, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return blPathAddBoxD(this, &box, dir);
  }

  //! Adds a closed rectangle to the path specified by `[x0, y0, x1, y1]`.
  BL_INLINE BLResult addBox(double x0, double y0, double x1, double y1, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addBox(BLBox(x0, y0, x1, y1), dir);
  }

  //! Adds a closed rectangle to the path specified by `rect`.
  BL_INLINE BLResult addRect(const BLRectI& rect, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return blPathAddRectI(this, &rect, dir);
  }

  //! Adds a closed rectangle to the path specified by `rect`.
  BL_INLINE BLResult addRect(const BLRect& rect, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return blPathAddRectD(this, &rect, dir);
  }

  //! Adds a closed rectangle to the path specified by `[x, y, w, h]`.
  BL_INLINE BLResult addRect(double x, double y, double w, double h, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addRect(BLRect(x, y, w, h), dir);
  }

  //! Adds a geometry to the path.
  BL_INLINE BLResult addGeometry(BLGeometryType geometryType, const void* geometryData, const BLMatrix2D* m = nullptr, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return blPathAddGeometry(this, geometryType, geometryData, m, dir);
  }

  //! Adds a closed circle to the path.
  BL_INLINE BLResult addCircle(const BLCircle& circle, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_CIRCLE, &circle, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addCircle(const BLCircle& circle, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_CIRCLE, &circle, &m, dir);
  }

  //! Adds a closed ellipse to the path.
  BL_INLINE BLResult addEllipse(const BLEllipse& ellipse, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ELLIPSE, &ellipse, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addEllipse(const BLEllipse& ellipse, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ELLIPSE, &ellipse, &m, dir);
  }

  //! Adds a closed rounded ractangle to the path.
  BL_INLINE BLResult addRoundRect(const BLRoundRect& rr, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ROUND_RECT, &rr, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addRoundRect(const BLRoundRect& rr, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ROUND_RECT, &rr, &m, dir);
  }

  //! Adds an unclosed arc to the path.
  BL_INLINE BLResult addArc(const BLArc& arc, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARC, &arc, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addArc(const BLArc& arc, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARC, &arc, &m, dir);
  }

  //! Adds a closed chord to the path.
  BL_INLINE BLResult addChord(const BLArc& chord, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_CHORD, &chord, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addChord(const BLArc& chord, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_CHORD, &chord, &m, dir);
  }

  //! Adds a closed pie to the path.
  BL_INLINE BLResult addPie(const BLArc& pie, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_PIE, &pie, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addPie(const BLArc& pie, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_PIE, &pie, &m, dir);
  }

  //! Adds an unclosed line to the path.
  BL_INLINE BLResult addLine(const BLLine& line, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_LINE, &line, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addLine(const BLLine& line, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_LINE, &line, &m, dir);
  }

  //! Adds a closed triangle.
  BL_INLINE BLResult addTriangle(const BLTriangle& triangle, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_TRIANGLE, &triangle, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addTriangle(const BLTriangle& triangle, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_TRIANGLE, &triangle, &m, dir);
  }

  //! Adds a polyline.
  BL_INLINE BLResult addPolyline(const BLArrayView<BLPointI>& poly, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYLINEI, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLArrayView<BLPointI>& poly, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYLINEI, &poly, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLPointI* poly, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolyline(BLArrayView<BLPointI>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLPointI* poly, size_t n, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolyline(BLArrayView<BLPointI>{poly, n}, m, dir);
  }

  //! Adds a polyline.
  BL_INLINE BLResult addPolyline(const BLArrayView<BLPoint>& poly, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYLINED, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLArrayView<BLPoint>& poly, const BLMatrix2D& m, BLGeometryDirection dir) {
    return addGeometry(BL_GEOMETRY_TYPE_POLYLINED, &poly, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLPoint* poly, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolyline(BLArrayView<BLPoint>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolyline(const BLPoint* poly, size_t n, const BLMatrix2D& m, BLGeometryDirection dir) {
    return addPolyline(BLArrayView<BLPoint>{poly, n}, m, dir);
  }

  //! Adds a polygon.
  BL_INLINE BLResult addPolygon(const BLArrayView<BLPointI>& poly, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYGONI, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLArrayView<BLPointI>& poly, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYGONI, &poly, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLPointI* poly, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolygon(BLArrayView<BLPointI>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLPointI* poly, size_t n, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolygon(BLArrayView<BLPointI>{poly, n}, m, dir);
  }

  //! Adds a polygon.
  BL_INLINE BLResult addPolygon(const BLArrayView<BLPoint>& poly, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYGOND, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLArrayView<BLPoint>& poly, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_POLYGOND, &poly, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLPoint* poly, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolygon(BLArrayView<BLPoint>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addPolygon(const BLPoint* poly, size_t n, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addPolygon(BLArrayView<BLPoint>{poly, n}, m, dir);
  }

  //! Adds an array of closed boxes.
  BL_INLINE BLResult addBoxArray(const BLArrayView<BLBoxI>& array, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLArrayView<BLBoxI>& array, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI, &array, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLBoxI* data, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addBoxArray(BLArrayView<BLBoxI>{data, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLBoxI* data, size_t n, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addBoxArray(BLArrayView<BLBoxI>{data, n}, m, dir);
  }

  //! Adds an array of closed boxes.
  BL_INLINE BLResult addBoxArray(const BLArrayView<BLBox>& array, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLArrayView<BLBox>& array, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD, &array, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLBox* data, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addBoxArray(BLArrayView<BLBox>{data, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addBoxArray(const BLBox* data, size_t n, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addBoxArray(BLArrayView<BLBox>{data, n}, m, dir);
  }

  //! Adds an array of closed rectangles.
  BL_INLINE BLResult addRectArray(const BLArrayView<BLRectI>& array, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLArrayView<BLRectI>& array, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI, &array, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLRectI* data, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addRectArray(BLArrayView<BLRectI>{data, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLRectI* data, size_t n, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addRectArray(BLArrayView<BLRectI>{data, n}, m, dir);
  }

  //! Adds an array of closed rectangles.
  BL_INLINE BLResult addRectArray(const BLArrayView<BLRect>& array, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLArrayView<BLRect>& array, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addGeometry(BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD, &array, &m, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLRect* data, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addRectArray(BLArrayView<BLRect>{data, n}, dir);
  }

  //! \overload
  BL_INLINE BLResult addRectArray(const BLRect* data, size_t n, const BLMatrix2D& m, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return addRectArray(BLArrayView<BLRect>{data, n}, m, dir);
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
  BL_INLINE BLResult addReversedPath(const BLPath& path, BLPathReverseMode reverseMode) noexcept {
    return blPathAddReversedPath(this, &path, nullptr, reverseMode);
  }

  //! Adds other `path`, but reversed.
  BL_INLINE BLResult addReversedPath(const BLPath& path, const BLRange& range, BLPathReverseMode reverseMode) noexcept {
    return blPathAddReversedPath(this, &path, &range, reverseMode);
  }

  //! Adds a stroke of `path` to this path.
  BL_INLINE BLResult addStrokedPath(const BLPath& path, const BLRange& range, const BLStrokeOptionsCore& strokeOptions, const BLApproximationOptions& approximationOptions) noexcept {
    return blPathAddStrokedPath(this, &path, &range, &strokeOptions, &approximationOptions);
  }
  //! \overload
  BL_INLINE BLResult addStrokedPath(const BLPath& path, const BLStrokeOptionsCore& strokeOptions, const BLApproximationOptions& approximationOptions) noexcept {
    return blPathAddStrokedPath(this, &path, nullptr, &strokeOptions, &approximationOptions);
  }

  //! \}

  //! \name Manipulation
  //! \{

  BL_INLINE BLResult removeRange(const BLRange& range) noexcept {
    return blPathRemoveRange(this, &range);
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

  //! Fits a parh of the path specified by the given `range` into the given `rect` by taking into account fit flags
  //! passed by `fitFlags`.
  BL_INLINE BLResult fitTo(const BLRange& range, const BLRect& rect, uint32_t fitFlags) noexcept {
    return blPathFitTo(this, &range, &rect, fitFlags);
  }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Tests whether this path and the `other` path are equal.
  //!
  //! The equality check is deep. The data of both paths is examined and binary compared (thus a slight difference
  //! like -0 and +0 would make the equality check to fail).
  BL_NODISCARD
  BL_INLINE bool equals(const BLPath& other) const noexcept { return blPathEquals(this, &other); }

  //! \}

  //! \name Path Information
  //! \{

  //! Update a path information if necessary.
  BL_INLINE BLResult getInfoFlags(uint32_t* flagsOut) const noexcept {
    return blPathGetInfoFlags(this, flagsOut);
  }

  //! Stores a bounding box of all vertices and control points to `boxOut`.
  //!
  //! Control box is simply bounds of all vertices the path has without further processing. It contains both on-path
  //! and off-path points. Consider using `getBoundingBox()` if you need a visual bounding box.
  BL_INLINE BLResult getControlBox(BLBox* boxOut) const noexcept {
    return blPathGetControlBox(this, boxOut);
  }

  //! Stores a bounding box of all on-path vertices and curve extremas to `boxOut`.
  //!
  //! The bounding box stored to `boxOut` could be smaller than a bounding box obtained by `getControlBox()` as it's
  //! calculated by merging only start/end points and curves at their extremas (not control points). The resulting
  //! bounding box represents a visual bounds of the path.
  BL_INLINE BLResult getBoundingBox(BLBox* boxOut) const noexcept {
    return blPathGetBoundingBox(this, boxOut);
  }

  //! Returns the range describing a figure at the given `index`.
  BL_INLINE BLResult getFigureRange(size_t index, BLRange* rangeOut) const noexcept {
    return blPathGetFigureRange(this, index, rangeOut);
  }

  //! Returns the last vertex of the path and stores it to `vtxOut`. If the very last command of the path is
  //! `BL_PATH_CMD_CLOSE` then the path will be iterated in reverse order to match the initial vertex of the last
  //! figure.
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
  BL_NODISCARD
  BL_INLINE BLHitTest hitTest(const BLPoint& p, BLFillRule fillRule) const noexcept {
    return blPathHitTest(this, &p, fillRule);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_PATH_H_INCLUDED
