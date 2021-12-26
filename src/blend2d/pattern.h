// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATTERN_H_INCLUDED
#define BLEND2D_PATTERN_H_INCLUDED

#include "geometry.h"
#include "image.h"
#include "matrix.h"
#include "object.h"

//! \addtogroup blend2d_api_styling
//! \{

//! \name BLPattern - C API
//! \{

//! Pattern [Impl].
//!
//! The following properties are stored in BLObjectInfo:
//!
//!   - Pattern extend mode is stored in BLObjectInfo's 'b' field.
//!   - Pattern matrix type is stored in BLObjectInfo's 'c' field.
struct BLPatternImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Image used by the pattern.
  BL_TYPED_MEMBER(BLImageCore, BLImage, image);

  //! Image area to use.
  BLRectI area;
  //! Pattern transformation matrix.
  BLMatrix2D matrix;

  BL_HAS_TYPED_MEMBERS(BLPatternImpl)
};

//! Pattern [C API].
struct BLPatternCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blPatternInit(BLPatternCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternInitMove(BLPatternCore* self, BLPatternCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternInitWeak(BLPatternCore* self, const BLPatternCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternInitAs(BLPatternCore* self, const BLImageCore* image, const BLRectI* area, BLExtendMode extendMode, const BLMatrix2D* m) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternDestroy(BLPatternCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternReset(BLPatternCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternAssignMove(BLPatternCore* self, BLPatternCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternAssignWeak(BLPatternCore* self, const BLPatternCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternAssignDeep(BLPatternCore* self, const BLPatternCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternCreate(BLPatternCore* self, const BLImageCore* image, const BLRectI* area, BLExtendMode extendMode, const BLMatrix2D* m) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blPatternGetImage(const BLPatternCore* self, BLImageCore* image) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternSetImage(BLPatternCore* self, const BLImageCore* image, const BLRectI* area) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternResetImage(BLPatternCore* self) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blPatternGetArea(const BLPatternCore* self, BLRectI* areaOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternSetArea(BLPatternCore* self, const BLRectI* area) BL_NOEXCEPT_C;

BL_API BLExtendMode BL_CDECL blPatternGetExtendMode(const BLPatternCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blPatternSetExtendMode(BLPatternCore* self, BLExtendMode extendMode) BL_NOEXCEPT_C;

BL_API BLMatrix2DType BL_CDECL blPatternGetMatrixType(const BLPatternCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blPatternGetMatrix(const BLPatternCore* self, BLMatrix2D* matrixOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blPatternApplyMatrixOp(BLPatternCore* self, BLMatrix2DOp opType, const void* opData) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blPatternEquals(const BLPatternCore* a, const BLPatternCore* b) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}

//! \name BLPattern - C++ API
//! \{
#ifdef __cplusplus

//! Pattern [C++ API].
class BLPattern : public BLPatternCore {
public:
  //! \cond INTERNAL
  BL_INLINE BLPatternImpl* _impl() const noexcept { return static_cast<BLPatternImpl*>(_d.impl); }
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLPattern() noexcept { blPatternInit(this); }
  BL_INLINE BLPattern(BLPattern&& other) noexcept { blPatternInitMove(this, &other); }
  BL_INLINE BLPattern(const BLPattern& other) noexcept { blPatternInitWeak(this, &other); }

  BL_INLINE explicit BLPattern(const BLImage& image, BLExtendMode extendMode = BL_EXTEND_MODE_REPEAT) noexcept {
    blPatternInitAs(this, &image, nullptr, extendMode, nullptr);
  }

  BL_INLINE BLPattern(const BLImage& image, BLExtendMode extendMode, const BLMatrix2D& m) noexcept {
    blPatternInitAs(this, &image, nullptr, extendMode, &m);
  }

  BL_INLINE BLPattern(const BLImage& image, const BLRectI& area, BLExtendMode extendMode = BL_EXTEND_MODE_REPEAT) noexcept {
    blPatternInitAs(this, &image, &area, extendMode, nullptr);
  }

  BL_INLINE BLPattern(const BLImage& image, const BLRectI& area, BLExtendMode extendMode, const BLMatrix2D& m) noexcept {
    blPatternInitAs(this, &image, &area, extendMode, &m);
  }

  BL_INLINE ~BLPattern() noexcept { blPatternDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLPattern& operator=(BLPattern&& other) noexcept { blPatternAssignMove(this, &other); return *this; }
  BL_INLINE BLPattern& operator=(const BLPattern& other) noexcept { blPatternAssignWeak(this, &other); return *this; }

  BL_NODISCARD BL_INLINE bool operator==(const BLPattern& other) const noexcept { return  equals(other); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLPattern& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blPatternReset(this); }

  BL_INLINE void swap(BLPattern& other) noexcept { _d.swap(other._d); }

  BL_INLINE BLResult assign(BLPattern&& other) noexcept { return blPatternAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLPattern& other) noexcept { return blPatternAssignWeak(this, &other); }

  BL_NODISCARD
  BL_INLINE bool equals(const BLPattern& other) const noexcept { return blPatternEquals(this, &other); }

  //! \}

  //! \name Create Pattern
  //! \{

  BL_INLINE BLResult create(const BLImage& image, BLExtendMode extendMode = BL_EXTEND_MODE_REPEAT) noexcept {
    return blPatternCreate(this, &image, nullptr, extendMode, nullptr);
  }

  BL_INLINE BLResult create(const BLImage& image, BLExtendMode extendMode, const BLMatrix2D& m) noexcept {
    return blPatternCreate(this, &image, nullptr, extendMode, &m);
  }

  BL_INLINE BLResult create(const BLImage& image, const BLRectI& area, BLExtendMode extendMode = BL_EXTEND_MODE_REPEAT) noexcept {
    return blPatternCreate(this, &image, &area, extendMode, nullptr);
  }

  BL_INLINE BLResult create(const BLImage& image, const BLRectI& area, BLExtendMode extendMode, const BLMatrix2D& m) noexcept {
    return blPatternCreate(this, &image, &area, extendMode, &m);
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_NODISCARD
  BL_INLINE BLImage getImage() const noexcept {
    BLImage imageOut;
    blPatternGetImage(this, &imageOut);
    return imageOut;
  }

  BL_NODISCARD
  BL_INLINE BLRectI area() const noexcept {
    BLRectI areaOut;
    blPatternGetArea(this, &areaOut);
    return areaOut;
  }

  BL_INLINE BLResult setImage(const BLImageCore& image) noexcept { return blPatternSetImage(this, &image, nullptr); }
  BL_INLINE BLResult setImage(const BLImageCore& image, const BLRectI& area) noexcept { return blPatternSetImage(this, &image, &area); }
  BL_INLINE BLResult resetImage() noexcept { return blPatternResetImage(this); }

  BL_INLINE BLResult setArea(const BLRectI& area) noexcept { return blPatternSetArea(this, &area); }
  BL_INLINE BLResult resetArea() noexcept { return setArea(BLRectI(0, 0, 0, 0)); }

  BL_NODISCARD
  BL_INLINE BLExtendMode extendMode() const noexcept { return (BLExtendMode)_d.bField(); }

  BL_INLINE BLResult setExtendMode(BLExtendMode extendMode) noexcept {
    if (BL_UNLIKELY(extendMode > BL_EXTEND_MODE_COMPLEX_MAX_VALUE))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    _d.info.setBField(uint32_t(extendMode));
    return BL_SUCCESS;
  }

  BL_INLINE BLResult resetExtendMode() noexcept {
    _d.info.setBField(uint32_t(BL_EXTEND_MODE_REPEAT));
    return BL_SUCCESS;
  }

  //! \}

  //! \name Transformations
  //! \{

  BL_NODISCARD
  BL_INLINE bool hasMatrix() const noexcept { return _d.cField() != BL_MATRIX2D_TYPE_IDENTITY; }

  BL_NODISCARD
  BL_INLINE BLMatrix2DType matrixType() const noexcept { return (BLMatrix2DType)_d.cField(); }

  BL_NODISCARD
  BL_INLINE BLMatrix2D matrix() const noexcept {
    BLMatrix2D matrixOut;
    blPatternGetMatrix(this, &matrixOut);
    return matrixOut;
  }

  BL_INLINE BLResult getMatrix(BLMatrix2D* matrixOut) const noexcept {
    return blPatternGetMatrix(this, matrixOut);
  }

  //! Applies a matrix operation to the current transformation matrix (internal).
  BL_INLINE BLResult _applyMatrixOp(BLMatrix2DOp opType, const void* opData) noexcept {
    return blPatternApplyMatrixOp(this, opType, opData);
  }

  //! \cond INTERNAL
  //! Applies a matrix operation to the current transformation matrix (internal).
  template<typename... Args>
  BL_INLINE BLResult _applyMatrixOpV(BLMatrix2DOp opType, Args&&... args) noexcept {
    double opData[] = { double(args)... };
    return blPatternApplyMatrixOp(this, opType, opData);
  }
  //! \endcond

  BL_INLINE BLResult setMatrix(const BLMatrix2D& m) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_ASSIGN, &m); }
  BL_INLINE BLResult resetMatrix() noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_RESET, nullptr); }

  BL_INLINE BLResult translate(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_TRANSLATE, x, y); }
  BL_INLINE BLResult translate(const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_TRANSLATE, p.x, p.y); }
  BL_INLINE BLResult translate(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_TRANSLATE, &p); }
  BL_INLINE BLResult scale(double xy) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_SCALE, xy, xy); }
  BL_INLINE BLResult scale(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_SCALE, x, y); }
  BL_INLINE BLResult scale(const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_SCALE, p.x, p.y); }
  BL_INLINE BLResult scale(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_SCALE, &p); }
  BL_INLINE BLResult skew(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_SKEW, x, y); }
  BL_INLINE BLResult skew(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_SKEW, &p); }
  BL_INLINE BLResult rotate(double angle) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_ROTATE, &angle); }
  BL_INLINE BLResult rotate(double angle, double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_ROTATE_PT, angle, x, y); }
  BL_INLINE BLResult rotate(double angle, const BLPoint& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_ROTATE_PT, angle, p.x, p.y); }
  BL_INLINE BLResult rotate(double angle, const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_ROTATE_PT, angle, p.x, p.y); }
  BL_INLINE BLResult transform(const BLMatrix2D& m) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_TRANSFORM, &m); }

  BL_INLINE BLResult postTranslate(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_TRANSLATE, x, y); }
  BL_INLINE BLResult postTranslate(const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_TRANSLATE, p.x, p.y); }
  BL_INLINE BLResult postTranslate(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_POST_TRANSLATE, &p); }
  BL_INLINE BLResult postScale(double xy) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_SCALE, xy, xy); }
  BL_INLINE BLResult postScale(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_SCALE, x, y); }
  BL_INLINE BLResult postScale(const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_SCALE, p.x, p.y); }
  BL_INLINE BLResult postScale(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_POST_SCALE, &p); }
  BL_INLINE BLResult postSkew(double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_SKEW, x, y); }
  BL_INLINE BLResult postSkew(const BLPoint& p) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_POST_SKEW, &p); }
  BL_INLINE BLResult postRotate(double angle) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_POST_ROTATE, &angle); }
  BL_INLINE BLResult postRotate(double angle, double x, double y) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_ROTATE_PT, angle, x, y); }
  BL_INLINE BLResult postRotate(double angle, const BLPoint& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_ROTATE_PT, angle, p.x, p.y); }
  BL_INLINE BLResult postRotate(double angle, const BLPointI& p) noexcept { return _applyMatrixOpV(BL_MATRIX2D_OP_POST_ROTATE_PT, angle, p.x, p.y); }
  BL_INLINE BLResult postTransform(const BLMatrix2D& m) noexcept { return _applyMatrixOp(BL_MATRIX2D_OP_POST_TRANSFORM, &m); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_PATTERN_H_INCLUDED
