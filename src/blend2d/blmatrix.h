// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLMATRIX_H
#define BLEND2D_BLMATRIX_H

#include "./blgeometry.h"

//! \addtogroup blend2d_api_geometry
//! \{

// ============================================================================
// [Typedefs]
// ============================================================================

//! A generic function that can be used to transform an array of points that use
//! `double` precision coordinates. This function will be 99.99% of time used with
//! `BLMatrix2D` so the `ctx` would point to a `const BLMatrix2D*` instance.
typedef BLResult (BL_CDECL* BLMapPointDArrayFunc)(const void* ctx, BLPoint* dst, const BLPoint* src, size_t count) BL_NOEXCEPT;

// ============================================================================
// [Constants]
// ============================================================================

//! 2D matrix type that can be obtained by calling `BLMatrix2D::type()`.
//!
//! ```
//!  Identity  Transl.  Scale     Swap    Affine
//!   [1  0]   [1  0]   [.  0]   [0  .]   [.  .]
//!   [0  1]   [0  1]   [0  .]   [.  0]   [.  .]
//!   [0  0]   [.  .]   [.  .]   [.  .]   [.  .]
//! ```
BL_DEFINE_ENUM(BLMatrix2DType) {
  //! Identity matrix.
  BL_MATRIX2D_TYPE_IDENTITY = 0,
  //! Has translation part (the rest is like identity).
  BL_MATRIX2D_TYPE_TRANSLATE = 1,
  //! Has translation and scaling parts.
  BL_MATRIX2D_TYPE_SCALE = 2,
  //! Has translation and scaling parts, however scaling swaps X/Y.
  BL_MATRIX2D_TYPE_SWAP = 3,
  //! Generic affine matrix.
  BL_MATRIX2D_TYPE_AFFINE = 4,
  //! Invalid/degenerate matrix not useful for transformations.
  BL_MATRIX2D_TYPE_INVALID = 5,

  //! Count of matrix types.
  BL_MATRIX2D_TYPE_COUNT = 6
};

//! 2D matrix data index.
BL_DEFINE_ENUM(BLMatrix2DValue) {
  BL_MATRIX2D_VALUE_00 = 0,
  BL_MATRIX2D_VALUE_01 = 1,
  BL_MATRIX2D_VALUE_10 = 2,
  BL_MATRIX2D_VALUE_11 = 3,
  BL_MATRIX2D_VALUE_20 = 4,
  BL_MATRIX2D_VALUE_21 = 5,

  BL_MATRIX2D_VALUE_COUNT = 6
};

//! 2D matrix operation.
BL_DEFINE_ENUM(BLMatrix2DOp) {
  //! Reset matrix to identity (argument ignored, should be nullptr).
  BL_MATRIX2D_OP_RESET = 0,
  //! Assign (copy) the other matrix.
  BL_MATRIX2D_OP_ASSIGN = 1,

  //! Translate the matrix by [x, y].
  BL_MATRIX2D_OP_TRANSLATE = 2,
  //! Scale the matrix by [x, y].
  BL_MATRIX2D_OP_SCALE = 3,
  //! Skew the matrix by [x, y].
  BL_MATRIX2D_OP_SKEW = 4,
  //! Rotate the matrix by the given angle about [0, 0].
  BL_MATRIX2D_OP_ROTATE = 5,
  //! Rotate the matrix by the given angle about [x, y].
  BL_MATRIX2D_OP_ROTATE_PT = 6,
  //! Transform this matrix by other `BLMatrix2D`.
  BL_MATRIX2D_OP_TRANSFORM = 7,

  //! Post-translate the matrix by [x, y].
  BL_MATRIX2D_OP_POST_TRANSLATE = 8,
  //! Post-scale the matrix by [x, y].
  BL_MATRIX2D_OP_POST_SCALE = 9,
  //! Post-skew the matrix by [x, y].
  BL_MATRIX2D_OP_POST_SKEW = 10,
  //! Post-rotate the matrix about [0, 0].
  BL_MATRIX2D_OP_POST_ROTATE = 11,
  //! Post-rotate the matrix about a reference BLPoint.
  BL_MATRIX2D_OP_POST_ROTATE_PT = 12,
  //! Post-transform this matrix by other `BLMatrix2D`.
  BL_MATRIX2D_OP_POST_TRANSFORM = 13,

  //! Count of matrix operations.
  BL_MATRIX2D_OP_COUNT = 14
};

// ============================================================================
// [BLMatrix2D]
// ============================================================================

//! 2D matrix represents an affine transformation matrix that can be used to
//! transform geometry and images.
struct BLMatrix2D {
  union {
    //! Matrix values, use `BL_MATRIX2D_VALUE` indexes to get a particular one.
    double m[BL_MATRIX2D_VALUE_COUNT];
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

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  //! \name Constructors and Destructors
  //! \{

  //! Creates an uninitialized 2D matrix, you must initialize all members before use.
  BL_INLINE BLMatrix2D() noexcept = default;

  //! Creates a new matrix initialized to a copy of `src` matrix.
  constexpr BLMatrix2D(const BLMatrix2D& src) noexcept = default;

  //! Creates a new matrix initialized to:
  //!
  //! ```
  //!   [m00 m01]
  //!   [m10 m11]
  //!   [m20 m21]
  //! ```
  constexpr BLMatrix2D(double m00, double m01, double m10, double m11, double m20, double m21) noexcept
    : m00(m00), m01(m01),
      m10(m10), m11(m11),
      m20(m20), m21(m21) {}

  //! \}

  //! \name Static Constructors
  //! \{

  //! Creates a new matrix initialized to identity.
  static constexpr BLMatrix2D makeIdentity() noexcept { return BLMatrix2D(1.0, 0.0, 0.0, 1.0, 0.0, 0.0); }

  //! \overload
  static constexpr BLMatrix2D makeTranslation(double x, double y) noexcept { return BLMatrix2D(1.0, 0.0, 0.0, 1.0, x, y); }
  //! Creates a new matrix initialized to translation.
  static constexpr BLMatrix2D makeTranslation(const BLPointI& p) noexcept { return BLMatrix2D(1.0, 0.0, 0.0, 1.0, double(p.x), double(p.y)); }
  //! \overload
  static constexpr BLMatrix2D makeTranslation(const BLPoint& p) noexcept { return BLMatrix2D(1.0, 0.0, 0.0, 1.0, p.x, p.y); }

  //! \overload
  static constexpr BLMatrix2D makeScaling(double xy) noexcept { return BLMatrix2D(xy, 0.0, 0.0, xy, 0.0, 0.0); }
  //! \overload
  static constexpr BLMatrix2D makeScaling(double x, double y) noexcept { return BLMatrix2D(x, 0.0, 0.0, y, 0.0, 0.0); }
  //! \overload
  static constexpr BLMatrix2D makeScaling(const BLPointI& p) noexcept { return BLMatrix2D(double(p.x), 0.0, 0.0, double(p.y), 0.0, 0.0); }
  //! Creates a new matrix initialized to scaling.
  static constexpr BLMatrix2D makeScaling(const BLPoint& p) noexcept { return BLMatrix2D(p.x, 0.0, 0.0, p.y, 0.0, 0.0); }

  //! Creates a new matrix initialized to rotation.
  static BL_INLINE BLMatrix2D makeRotation(double angle) noexcept {
    BLMatrix2D result;
    result.resetToRotation(angle, 0.0, 0.0);
    return result;
  }

  //! \overload
  static BL_INLINE BLMatrix2D makeRotation(double angle, double x, double y) noexcept {
    BLMatrix2D result;
    result.resetToRotation(angle, x, y);
    return result;
  }

  //! \overload
  static BL_INLINE BLMatrix2D makeRotation(double angle, const BLPoint& p) noexcept {
    BLMatrix2D result;
    result.resetToRotation(angle, p.x, p.y);
    return result;
  }

  //! Create a new skewing matrix.
  static BL_INLINE BLMatrix2D makeSkewing(double x, double y) noexcept {
    BLMatrix2D result;
    result.resetToSkewing(x, y);
    return result;
  }
  //! \overload
  static BL_INLINE BLMatrix2D makeSkewing(const BLPoint& p) noexcept {
    BLMatrix2D result;
    result.resetToSkewing(p.x, p.y);
    return result;
  }

  static BL_INLINE BLMatrix2D makeSinCos(double sin, double cos, double tx = 0.0, double ty = 0.0) noexcept {
    return BLMatrix2D(cos, sin, -sin, cos, tx, ty);
  }

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

  //! Resets matrix to [`m00`, `m01`, `m10`, `m11`, `m20`, `m21`].
  BL_INLINE void reset(double m00, double m01, double m10, double m11, double m20, double m21) noexcept {
    this->m00 = m00;
    this->m01 = m01;
    this->m10 = m10;
    this->m11 = m11;
    this->m20 = m20;
    this->m21 = m21;
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
  BL_INLINE void resetToRotation(double angle, const BLPoint& p) noexcept { blMatrix2DSetRotation(this, angle, p.x, p.y); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE bool operator==(const BLMatrix2D& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLMatrix2D& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE bool equals(const BLMatrix2D& other) const noexcept {
    return blEquals(this->m00, other.m00) &
           blEquals(this->m01, other.m01) &
           blEquals(this->m10, other.m10) &
           blEquals(this->m11, other.m11) &
           blEquals(this->m20, other.m20) &
           blEquals(this->m21, other.m21) ;
  }

  //! \}

  //! \name Matrix Properties
  //! \{

  //! Gets matrix type, see `BLMatrix2DType`.
  BL_INLINE uint32_t type() const noexcept { return blMatrix2DGetType(this); }

  //! Gets matrix determinant.
  BL_INLINE double determinant() noexcept { return this->m00 * this->m11 - this->m01 * this->m10; }

  //! \}

  //! \name Matrix Operations
  //! \{

  BL_INLINE BLResult translate(double x, double y) noexcept {
    this->m20 += x * this->m00 + y * this->m10;
    this->m21 += x * this->m01 + y * this->m11;

    return BL_SUCCESS;
  }

  BL_INLINE BLResult translate(const BLPointI& p) noexcept { return translate(BLPoint(p)); }
  BL_INLINE BLResult translate(const BLPoint& p) noexcept { return translate(p.x, p.y); }

  BL_INLINE BLResult scale(double xy) noexcept { return scale(xy, xy); }
  BL_INLINE BLResult scale(double x, double y) noexcept {
    this->m00 *= x;
    this->m01 *= x;
    this->m10 *= y;
    this->m11 *= y;

    return BL_SUCCESS;
  }

  BL_INLINE BLResult scale(const BLPointI& p) noexcept { return scale(BLPoint(p)); }
  BL_INLINE BLResult scale(const BLPoint& p) noexcept { return scale(p.x, p.y); }

  BL_INLINE BLResult skew(double x, double y) noexcept { return skew(BLPoint(x, y)); }
  BL_INLINE BLResult skew(const BLPoint& p) noexcept { return blMatrix2DApplyOp(this, BL_MATRIX2D_OP_SKEW, &p); }

  BL_INLINE BLResult rotate(double angle) noexcept { return blMatrix2DApplyOp(this, BL_MATRIX2D_OP_ROTATE, &angle); }
  BL_INLINE BLResult rotate(double angle, double x, double y) noexcept {
    double opData[3] = { angle, x, y };
    return blMatrix2DApplyOp(this, BL_MATRIX2D_OP_ROTATE_PT, opData);
  }

  BL_INLINE BLResult rotate(double angle, const BLPointI& p) noexcept { return rotate(angle, double(p.x), double(p.y)); }
  BL_INLINE BLResult rotate(double angle, const BLPoint& p) noexcept { return rotate(angle, p.x, p.y); }

  BL_INLINE BLResult transform(const BLMatrix2D& m) noexcept {
    return blMatrix2DApplyOp(this, BL_MATRIX2D_OP_TRANSFORM, &m);
  }

  BL_INLINE BLResult postTranslate(double x, double y) noexcept {
    this->m20 += x;
    this->m21 += y;

    return BL_SUCCESS;
  }

  BL_INLINE BLResult postTranslate(const BLPointI& p) noexcept { return postTranslate(BLPoint(p)); }
  BL_INLINE BLResult postTranslate(const BLPoint& p) noexcept { return postTranslate(p.x, p.y); }

  BL_INLINE BLResult postScale(double xy) noexcept { return postScale(xy, xy); }
  BL_INLINE BLResult postScale(double x, double y) noexcept {
    this->m00 *= x;
    this->m01 *= y;
    this->m10 *= x;
    this->m11 *= y;
    this->m20 *= x;
    this->m21 *= y;

    return BL_SUCCESS;
  }
  BL_INLINE BLResult postScale(const BLPointI& p) noexcept { return postScale(BLPoint(p)); }
  BL_INLINE BLResult postScale(const BLPoint& p) noexcept { return postScale(p.x, p.y); }

  BL_INLINE BLResult postSkew(double x, double y) noexcept { return postSkew(BLPoint(x, y)); }
  BL_INLINE BLResult postSkew(const BLPoint& p) noexcept { return blMatrix2DApplyOp(this, BL_MATRIX2D_OP_POST_SKEW, &p); }

  BL_INLINE BLResult postRotate(double angle) noexcept { return blMatrix2DApplyOp(this, BL_MATRIX2D_OP_POST_ROTATE, &angle); }
  BL_INLINE BLResult postRotate(double angle, double x, double y) noexcept {
    double params[3] = { angle, x, y };
    return blMatrix2DApplyOp(this, BL_MATRIX2D_OP_POST_ROTATE_PT, params);
  }

  BL_INLINE BLResult postRotate(double angle, const BLPointI& p) noexcept { return postRotate(angle, BLPoint(p)); }
  BL_INLINE BLResult postRotate(double angle, const BLPoint& p) noexcept { return postRotate(angle, p.x, p.y); }

  BL_INLINE BLResult postTransform(const BLMatrix2D& m) noexcept { return blMatrix2DApplyOp(this, BL_MATRIX2D_OP_POST_TRANSFORM, &m); }

  //! Inverts the matrix, returns `BL_SUCCESS` if the matrix has been inverted successfully.
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
  // --------------------------------------------------------------------------
};

//! Array of functions for transforming points indexed by `BLMatrixType`. Each
//! function is optimized for the respective type. This is mostly used internally,
//! but exported for users that can take advantage of Blend2D SIMD optimziations.
BL_API_C BLMapPointDArrayFunc blMatrix2DMapPointDArrayFuncs[BL_MATRIX2D_TYPE_COUNT];

//! \}

#endif // BLEND2D_BLMATRIX_H
