// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_MATRIX_H_INCLUDED
#define BLEND2D_MATRIX_H_INCLUDED

#include "geometry.h"

//! \addtogroup bl_geometry
//! \{

//! \name BLMatrix Constants
//! \{

//! Transformation matrix type that can be obtained by calling `BLMatrix2D::type()`.
//!
//! ```
//!  Identity  Transl.  Scale     Swap    Affine
//!   [1  0]   [1  0]   [.  0]   [0  .]   [.  .]
//!   [0  1]   [0  1]   [0  .]   [.  0]   [.  .]
//!   [0  0]   [.  .]   [.  .]   [.  .]   [.  .]
//! ```
BL_DEFINE_ENUM(BLTransformType) {
  //! Identity matrix.
  BL_TRANSFORM_TYPE_IDENTITY = 0,
  //! Has translation part (the rest is like identity).
  BL_TRANSFORM_TYPE_TRANSLATE = 1,
  //! Has translation and scaling parts.
  BL_TRANSFORM_TYPE_SCALE = 2,
  //! Has translation and scaling parts, however scaling swaps X/Y.
  BL_TRANSFORM_TYPE_SWAP = 3,
  //! Generic affine matrix.
  BL_TRANSFORM_TYPE_AFFINE = 4,
  //! Invalid/degenerate matrix not useful for transformations.
  BL_TRANSFORM_TYPE_INVALID = 5,

  //! Maximum value of `BLTransformType`.
  BL_TRANSFORM_TYPE_MAX_VALUE = 5

  BL_FORCE_ENUM_UINT32(BL_MATRIX2D_TYPE)
};

//! Transformation matrix operation type.
BL_DEFINE_ENUM(BLTransformOp) {
  //! Reset matrix to identity (argument ignored, should be nullptr).
  BL_TRANSFORM_OP_RESET = 0,
  //! Assign (copy) the other matrix.
  BL_TRANSFORM_OP_ASSIGN = 1,

  //! Translate the matrix by [x, y].
  BL_TRANSFORM_OP_TRANSLATE = 2,
  //! Scale the matrix by [x, y].
  BL_TRANSFORM_OP_SCALE = 3,
  //! Skew the matrix by [x, y].
  BL_TRANSFORM_OP_SKEW = 4,
  //! Rotate the matrix by the given angle about [0, 0].
  BL_TRANSFORM_OP_ROTATE = 5,
  //! Rotate the matrix by the given angle about [x, y].
  BL_TRANSFORM_OP_ROTATE_PT = 6,
  //! Transform this matrix by other \ref BLMatrix2D.
  BL_TRANSFORM_OP_TRANSFORM = 7,

  //! Post-translate the matrix by [x, y].
  BL_TRANSFORM_OP_POST_TRANSLATE = 8,
  //! Post-scale the matrix by [x, y].
  BL_TRANSFORM_OP_POST_SCALE = 9,
  //! Post-skew the matrix by [x, y].
  BL_TRANSFORM_OP_POST_SKEW = 10,
  //! Post-rotate the matrix about [0, 0].
  BL_TRANSFORM_OP_POST_ROTATE = 11,
  //! Post-rotate the matrix about a reference BLPoint.
  BL_TRANSFORM_OP_POST_ROTATE_PT = 12,
  //! Post-transform this matrix by other \ref BLMatrix2D.
  BL_TRANSFORM_OP_POST_TRANSFORM = 13,

  //! Maximum value of `BLTransformOp`.
  BL_TRANSFORM_OP_MAX_VALUE = 13

  BL_FORCE_ENUM_UINT32(BL_TRANSFORM_OP)
};

//! \}
//! \}

//! \addtogroup bl_c_api
//! \{

//! \name BLMatrix2D - C API
//!
//! Functions that initialize and manipulate \ref BLMatrix2D content.
//!
//! \{

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blMatrix2DSetIdentity(BLMatrix2D* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blMatrix2DSetTranslation(BLMatrix2D* self, double x, double y) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blMatrix2DSetScaling(BLMatrix2D* self, double x, double y) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blMatrix2DSetSkewing(BLMatrix2D* self, double x, double y) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blMatrix2DSetRotation(BLMatrix2D* self, double angle, double cx, double cy) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blMatrix2DApplyOp(BLMatrix2D* self, BLTransformOp opType, const void* opData) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blMatrix2DInvert(BLMatrix2D* dst, const BLMatrix2D* src) BL_NOEXCEPT_C;
BL_API BLTransformType BL_CDECL blMatrix2DGetType(const BLMatrix2D* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blMatrix2DMapPointDArray(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t count) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}
//! \}

//! \addtogroup bl_geometry
//! \{

//! \name BLMatrix C/C++ API
//! \{

//! 2D matrix represents an affine transformation matrix that can be used to transform geometry and images.
struct BLMatrix2D {
  // TODO: Remove the union, keep only m[] array.
  union {
    //! Matrix values stored in array.
    double m[6];
    //! Matrix values that map `m` to named values that can be used directly.
    struct {
      double m00;
      double m01;
      double m10;
      double m11;
      double m20;
      double m21;
    };
  };

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  //! Creates an uninitialized 2D matrix, you must initialize all members before use.
  BL_INLINE BLMatrix2D() noexcept = default;

  //! Creates a new matrix initialized to a copy of `src` matrix.
  BL_INLINE constexpr BLMatrix2D(const BLMatrix2D& src) noexcept = default;

  //! Creates a new matrix initialized to:
  //!
  //! ```
  //!   [m00 m01]
  //!   [m10 m11]
  //!   [m20 m21]
  //! ```
  BL_INLINE constexpr BLMatrix2D(double m00Value, double m01Value, double m10Value, double m11Value, double m20Value, double m21Value) noexcept
    : m00(m00Value), m01(m01Value),
      m10(m10Value), m11(m11Value),
      m20(m20Value), m21(m21Value) {}

  //! \}

  //! \name Static Construction
  //! \{

  //! Creates a new matrix initialized to identity.
  BL_NODISCARD
  static BL_INLINE constexpr BLMatrix2D makeIdentity() noexcept { return BLMatrix2D(1.0, 0.0, 0.0, 1.0, 0.0, 0.0); }

  //! \overload
  BL_NODISCARD
  static BL_INLINE constexpr BLMatrix2D makeTranslation(double x, double y) noexcept { return BLMatrix2D(1.0, 0.0, 0.0, 1.0, x, y); }

  //! Creates a new matrix initialized to translation.
  BL_NODISCARD
  static BL_INLINE constexpr BLMatrix2D makeTranslation(const BLPointI& p) noexcept { return BLMatrix2D(1.0, 0.0, 0.0, 1.0, double(p.x), double(p.y)); }

  //! \overload
  BL_NODISCARD
  static BL_INLINE constexpr BLMatrix2D makeTranslation(const BLPoint& p) noexcept { return BLMatrix2D(1.0, 0.0, 0.0, 1.0, p.x, p.y); }

  //! Creates a new matrix initialized to scaling.
  BL_NODISCARD
  static BL_INLINE constexpr BLMatrix2D makeScaling(double xy) noexcept { return BLMatrix2D(xy, 0.0, 0.0, xy, 0.0, 0.0); }

  //! \overload
  BL_NODISCARD
  static BL_INLINE constexpr BLMatrix2D makeScaling(double x, double y) noexcept { return BLMatrix2D(x, 0.0, 0.0, y, 0.0, 0.0); }

  //! \overload
  BL_NODISCARD
  static BL_INLINE constexpr BLMatrix2D makeScaling(const BLPointI& p) noexcept { return BLMatrix2D(double(p.x), 0.0, 0.0, double(p.y), 0.0, 0.0); }

  //! \overload
  BL_NODISCARD
  static BL_INLINE constexpr BLMatrix2D makeScaling(const BLPoint& p) noexcept { return BLMatrix2D(p.x, 0.0, 0.0, p.y, 0.0, 0.0); }

  //! Creates a new matrix initialized to rotation.
  BL_NODISCARD
  static BL_INLINE BLMatrix2D makeRotation(double angle) noexcept {
    BLMatrix2D result;
    result.resetToRotation(angle, 0.0, 0.0);
    return result;
  }

  //! \overload
  BL_NODISCARD
  static BL_INLINE BLMatrix2D makeRotation(double angle, double x, double y) noexcept {
    BLMatrix2D result;
    result.resetToRotation(angle, x, y);
    return result;
  }

  //! \overload
  BL_NODISCARD
  static BL_INLINE BLMatrix2D makeRotation(double angle, const BLPoint& origin) noexcept {
    BLMatrix2D result;
    result.resetToRotation(angle, origin.x, origin.y);
    return result;
  }

  //! Create a new skewing matrix.
  BL_NODISCARD
  static BL_INLINE BLMatrix2D makeSkewing(double x, double y) noexcept {
    BLMatrix2D result;
    result.resetToSkewing(x, y);
    return result;
  }

  //! \overload
  BL_NODISCARD
  static BL_INLINE BLMatrix2D makeSkewing(const BLPoint& p) noexcept {
    BLMatrix2D result;
    result.resetToSkewing(p.x, p.y);
    return result;
  }

  BL_NODISCARD
  static BL_INLINE BLMatrix2D makeSinCos(double sin, double cos, double tx = 0.0, double ty = 0.0) noexcept {
    return BLMatrix2D(cos, sin, -sin, cos, tx, ty);
  }

  BL_NODISCARD
  static BL_INLINE BLMatrix2D makeSinCos(double sin, double cos, const BLPoint& t) noexcept {
    return makeSinCos(sin, cos, t.x, t.y);
  }

  //! \}

  //! \name Reset Matrix
  //! \{

  //! Resets matrix to identity.
  BL_INLINE void reset() noexcept {
    reset(1.0, 0.0,
          0.0, 1.0,
          0.0, 0.0);
  }

  //! Resets matrix to `other` (copy its content to this matrix).
  BL_INLINE void reset(const BLMatrix2D& other) noexcept {
    reset(other.m00, other.m01,
          other.m10, other.m11,
          other.m20, other.m21);
  }

  //! Resets matrix to [`m00Value`, `m01Value`, `m10Value`, `m11Value`, `m20Value`, `m21Value`].
  BL_INLINE void reset(double m00Value, double m01Value, double m10Value, double m11Value, double m20Value, double m21Value) noexcept {
    m00 = m00Value;
    m01 = m01Value;
    m10 = m10Value;
    m11 = m11Value;
    m20 = m20Value;
    m21 = m21Value;
  }

  //! Resets matrix to translation.
  BL_INLINE void resetToTranslation(double x, double y) noexcept { reset(1.0, 0.0, 0.0, 1.0, x, y); }
  //! Resets matrix to translation.
  BL_INLINE void resetToTranslation(const BLPointI& p) noexcept { resetToTranslation(BLPoint(p)); }
  //! Resets matrix to translation.
  BL_INLINE void resetToTranslation(const BLPoint& p) noexcept { resetToTranslation(p.x, p.y); }

  //! Resets matrix to scaling.
  BL_INLINE void resetToScaling(double xy) noexcept { resetToScaling(xy, xy); }
  //! Resets matrix to scaling.
  BL_INLINE void resetToScaling(double x, double y) noexcept { reset(x, 0.0, 0.0, y, 0.0, 0.0); }
  //! Resets matrix to scaling.
  BL_INLINE void resetToScaling(const BLPointI& p) noexcept { resetToScaling(BLPoint(p)); }
  //! Resets matrix to scaling.
  BL_INLINE void resetToScaling(const BLPoint& p) noexcept { resetToScaling(p.x, p.y); }

  //! Resets matrix to skewing.
  BL_INLINE void resetToSkewing(double x, double y) noexcept { blMatrix2DSetSkewing(this, x, y); }
  //! Resets matrix to skewing.
  BL_INLINE void resetToSkewing(const BLPoint& p) noexcept { blMatrix2DSetSkewing(this, p.x, p.y); }

  //! Resets matrix to rotation specified by `sin` and `cos` and optional translation `tx` and `ty`.
  BL_INLINE void resetToSinCos(double sin, double cos, double tx = 0.0, double ty = 0.0) noexcept { reset(cos, sin, -sin, cos, tx, ty); }
  //! Resets matrix to rotation specified by `sin` and `cos` and optional translation `t`.
  BL_INLINE void resetToSinCos(double sin, double cos, const BLPoint& t) noexcept { resetToSinCos(sin, cos, t.x, t.y); }

  //! Resets matrix to rotation.
  BL_INLINE void resetToRotation(double angle) noexcept { blMatrix2DSetRotation(this, angle, 0.0, 0.0); }
  //! Resets matrix to rotation around a point `[x, y]`.
  BL_INLINE void resetToRotation(double angle, double x, double y) noexcept { blMatrix2DSetRotation(this, angle, x, y); }
  //! Resets matrix to rotation around a point `p`.
  BL_INLINE void resetToRotation(double angle, const BLPoint& origin) noexcept { blMatrix2DSetRotation(this, angle, origin.x, origin.y); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLMatrix2D& operator=(const BLMatrix2D& other) noexcept = default;

  BL_NODISCARD BL_INLINE bool operator==(const BLMatrix2D& other) const noexcept { return  equals(other); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLMatrix2D& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_NODISCARD
  BL_INLINE bool equals(const BLMatrix2D& other) const noexcept {
    return bool(unsigned(blEquals(m00, other.m00)) &
                unsigned(blEquals(m01, other.m01)) &
                unsigned(blEquals(m10, other.m10)) &
                unsigned(blEquals(m11, other.m11)) &
                unsigned(blEquals(m20, other.m20)) &
                unsigned(blEquals(m21, other.m21)));
  }

  //! \}

  //! \name Matrix Properties
  //! \{

  //! Returns the matrix type, see \ref BLTransformType.
  BL_NODISCARD
  BL_INLINE BLTransformType type() const noexcept { return blMatrix2DGetType(this); }

  //! Calculates the matrix determinant.
  BL_NODISCARD
  BL_INLINE double determinant() const noexcept { return m00 * m11 - m01 * m10; }

  //! \}

  //! \name Matrix Operations
  //! \{

  BL_INLINE BLResult translate(double x, double y) noexcept {
    m20 += x * m00 + y * m10;
    m21 += x * m01 + y * m11;

    return BL_SUCCESS;
  }

  BL_INLINE BLResult translate(const BLPointI& p) noexcept { return translate(double(p.x), double(p.y)); }
  BL_INLINE BLResult translate(const BLPoint& p) noexcept { return translate(p.x, p.y); }

  BL_INLINE BLResult scale(double xy) noexcept { return scale(xy, xy); }
  BL_INLINE BLResult scale(double x, double y) noexcept {
    m00 *= x;
    m01 *= x;
    m10 *= y;
    m11 *= y;

    return BL_SUCCESS;
  }

  BL_INLINE BLResult scale(const BLPointI& p) noexcept { return scale(double(p.x), double(p.y)); }
  BL_INLINE BLResult scale(const BLPoint& p) noexcept { return scale(p.x, p.y); }

  BL_INLINE BLResult skew(double x, double y) noexcept { return skew(BLPoint(x, y)); }
  BL_INLINE BLResult skew(const BLPoint& p) noexcept { return blMatrix2DApplyOp(this, BL_TRANSFORM_OP_SKEW, &p); }

  BL_INLINE BLResult rotate(double angle) noexcept { return blMatrix2DApplyOp(this, BL_TRANSFORM_OP_ROTATE, &angle); }
  BL_INLINE BLResult rotate(double angle, double x, double y) noexcept {
    double opData[3] = { angle, x, y };
    return blMatrix2DApplyOp(this, BL_TRANSFORM_OP_ROTATE_PT, opData);
  }

  BL_INLINE BLResult rotate(double angle, const BLPointI& origin) noexcept { return rotate(angle, double(origin.x), double(origin.y)); }
  BL_INLINE BLResult rotate(double angle, const BLPoint& origin) noexcept { return rotate(angle, origin.x, origin.y); }

  BL_INLINE BLResult transform(const BLMatrix2D& m) noexcept {
    return blMatrix2DApplyOp(this, BL_TRANSFORM_OP_TRANSFORM, &m);
  }

  BL_INLINE BLResult postTranslate(double x, double y) noexcept {
    m20 += x;
    m21 += y;

    return BL_SUCCESS;
  }

  BL_INLINE BLResult postTranslate(const BLPointI& p) noexcept { return postTranslate(double(p.x), double(p.y)); }
  BL_INLINE BLResult postTranslate(const BLPoint& p) noexcept { return postTranslate(p.x, p.y); }

  BL_INLINE BLResult postScale(double xy) noexcept { return postScale(xy, xy); }
  BL_INLINE BLResult postScale(double x, double y) noexcept {
    m00 *= x;
    m01 *= y;
    m10 *= x;
    m11 *= y;
    m20 *= x;
    m21 *= y;

    return BL_SUCCESS;
  }

  BL_INLINE BLResult postScale(const BLPointI& p) noexcept { return postScale(double(p.x), double(p.y)); }
  BL_INLINE BLResult postScale(const BLPoint& p) noexcept { return postScale(p.x, p.y); }

  BL_INLINE BLResult postSkew(double x, double y) noexcept { return postSkew(BLPoint(x, y)); }
  BL_INLINE BLResult postSkew(const BLPoint& p) noexcept { return blMatrix2DApplyOp(this, BL_TRANSFORM_OP_POST_SKEW, &p); }

  BL_INLINE BLResult postRotate(double angle) noexcept { return blMatrix2DApplyOp(this, BL_TRANSFORM_OP_POST_ROTATE, &angle); }

  BL_INLINE BLResult postRotate(double angle, double x, double y) noexcept {
    double params[3] = { angle, x, y };
    return blMatrix2DApplyOp(this, BL_TRANSFORM_OP_POST_ROTATE_PT, params);
  }

  BL_INLINE BLResult postRotate(double angle, const BLPointI& origin) noexcept { return postRotate(angle, double(origin.x), double(origin.y)); }
  BL_INLINE BLResult postRotate(double angle, const BLPoint& origin) noexcept { return postRotate(angle, origin.x, origin.y); }

  BL_INLINE BLResult postTransform(const BLMatrix2D& m) noexcept { return blMatrix2DApplyOp(this, BL_TRANSFORM_OP_POST_TRANSFORM, &m); }

  //! Inverts the matrix, returns \ref BL_SUCCESS if the matrix has been inverted successfully.
  BL_INLINE BLResult invert() noexcept { return blMatrix2DInvert(this, this); }

  //! \}

  //! \name Map Points and Primitives
  //! \{

  BL_INLINE BLPoint mapPoint(double x, double y) const noexcept { return BLPoint(x * m00 + y * m10 + m20, x * m01 + y * m11 + m21); }
  BL_INLINE BLPoint mapPoint(const BLPoint& p) const noexcept { return mapPoint(p.x, p.y); }

  BL_INLINE BLPoint mapVector(double x, double y) const noexcept { return BLPoint(x * m00 + y * m10, x * m01 + y * m11); }
  BL_INLINE BLPoint mapVector(const BLPoint& v) const noexcept { return mapVector(v.x, v.y); }

  //! \}

  //! \name Static Operations
  //! \{

  //! Inverts `src` matrix and stores the result in `dst.
  //!
  //! \overload
  static BL_INLINE BLResult invert(BLMatrix2D& dst, const BLMatrix2D& src) noexcept { return blMatrix2DInvert(&dst, &src); }

  //! \}
#endif
};

//! \}

//! \}

#endif // BLEND2D_MATRIX_H_INCLUDED
