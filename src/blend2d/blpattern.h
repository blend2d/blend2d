// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLPATTERN_H
#define BLEND2D_BLPATTERN_H

#include "./blgeometry.h"
#include "./blimage.h"
#include "./blmatrix.h"
#include "./blvariant.h"

//! \addtogroup blend2d_api_styles
//! \{

// ============================================================================
// [BLPattern - Core]
// ============================================================================

//! Pattern [C Interface - Impl].
struct BLPatternImpl {
  //! Image used by the pattern.
  BL_TYPED_MEMBER(BLImageCore, BLImage, image);
  //! Reserved, must be null.
  void* reservedHeader[2];

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Reserved, must be zero.
  uint8_t patternType;
  //! Pattern extend mode, see `BLExtendMode`.
  uint8_t extendMode;
  //! Type of the transformation matrix.
  uint8_t matrixType;
  //! Reserved, must be zero.
  uint8_t reserved[1];

  //! Pattern transformation matrix.
  BLMatrix2D matrix;
  //! Image area to use.
  BLRectI area;

  BL_HAS_TYPED_MEMBERS(BLPatternImpl)
};

//! Pattern [C Interface - Core].
struct BLPatternCore {
  BLPatternImpl* impl;
};

// ============================================================================
// [BLPattern - C++]
// ============================================================================

#ifdef __cplusplus
//! Pattern [C++ API].
class BLPattern : public BLPatternCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_PATTERN;
  //! \endcond

  //! \name Constructors and Destructors
  //! \{

  BL_INLINE BLPattern() noexcept { this->impl = none().impl; }
  BL_INLINE BLPattern(BLPattern&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLPattern(const BLPattern& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLPattern(BLPatternImpl* impl) noexcept { this->impl = impl; }

  BL_INLINE explicit BLPattern(const BLImage& image, uint32_t extendMode = BL_EXTEND_MODE_REPEAT) noexcept {
    blPatternInitAs(this, &image, nullptr, extendMode, nullptr);
  }

  BL_INLINE BLPattern(const BLImage& image, uint32_t extendMode, const BLMatrix2D& m) noexcept {
    blPatternInitAs(this, &image, nullptr, extendMode, &m);
  }

  BL_INLINE BLPattern(const BLImage& image, const BLRectI& area, uint32_t extendMode = BL_EXTEND_MODE_REPEAT) noexcept {
    blPatternInitAs(this, &image, &area, extendMode, nullptr);
  }

  BL_INLINE BLPattern(const BLImage& image, const BLRectI& area, uint32_t extendMode, const BLMatrix2D& m) noexcept {
    blPatternInitAs(this, &image, &area, extendMode, &m);
  }

  BL_INLINE ~BLPattern() noexcept { blPatternReset(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLPattern& operator=(BLPattern&& other) noexcept { blPatternAssignMove(this, &other); return *this; }
  BL_INLINE BLPattern& operator=(const BLPattern& other) noexcept { blPatternAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLPattern& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLPattern& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blPatternReset(this); }

  BL_INLINE void swap(BLPattern& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLPattern&& other) noexcept { return blPatternAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLPattern& other) noexcept { return blPatternAssignWeak(this, &other); }

  //! Get whether the pattern is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }

  BL_INLINE bool equals(const BLPattern& other) const noexcept { return blPatternEquals(this, &other); }

  //! \}

  //! \name Create Pattern
  //! \{

  BL_INLINE BLResult create(const BLImage& image, uint32_t extendMode = BL_EXTEND_MODE_REPEAT) noexcept {
    return blPatternCreate(this, &image, nullptr, extendMode, nullptr);
  }

  BL_INLINE BLResult create(const BLImage& image, uint32_t extendMode, const BLMatrix2D& m) noexcept {
    return blPatternCreate(this, &image, nullptr, extendMode, &m);
  }

  BL_INLINE BLResult create(const BLImage& image, const BLRectI& area, uint32_t extendMode = BL_EXTEND_MODE_REPEAT) noexcept {
    return blPatternCreate(this, &image, &area, extendMode, nullptr);
  }

  BL_INLINE BLResult create(const BLImage& image, const BLRectI& area, uint32_t extendMode, const BLMatrix2D& m) noexcept {
    return blPatternCreate(this, &image, &area, extendMode, &m);
  }

  //! \}

  //! \name Pattern Source
  //! \{

  BL_INLINE const BLImage& image() const noexcept { return impl->image; }
  BL_INLINE BLResult setImage(const BLImage& image) noexcept { return blPatternSetImage(this, &image, nullptr); }
  BL_INLINE BLResult setImage(const BLImage& image, const BLRectI& area) noexcept { return blPatternSetImage(this, &image, &area); }
  BL_INLINE BLResult resetImage() noexcept { return setImage(BLImage::none()); }

  BL_INLINE const BLRectI& area() const noexcept { return impl->area; }
  BL_INLINE BLResult setArea(const BLRectI& area) noexcept { return blPatternSetArea(this, &area); }
  BL_INLINE BLResult resetArea() noexcept { return setArea(BLRectI(0, 0, 0, 0)); }

  //! \}

  //! \name Pattern Options
  //! \{

  BL_INLINE uint32_t extendMode() const noexcept { return impl->extendMode; }
  BL_INLINE BLResult setExtendMode(uint32_t extendMode) noexcept { return blPatternSetExtendMode(this, extendMode); }
  BL_INLINE BLResult resetExtendMode() noexcept { return setExtendMode(BL_EXTEND_MODE_REPEAT); }

  //! \}

  //! \name Transformations
  //! \{

  BL_INLINE bool hasMatrix() const noexcept { return impl->matrixType != BL_MATRIX2D_TYPE_IDENTITY; }
  BL_INLINE uint32_t matrixType() const noexcept { return impl->matrixType; }
  BL_INLINE const BLMatrix2D& matrix() const noexcept { return impl->matrix; }

  //! Applies a matrix operation to the current transformation matrix (internal).
  BL_INLINE BLResult _applyMatrixOp(uint32_t opType, const void* opData) noexcept {
    return blPatternApplyMatrixOp(this, opType, opData);
  }

  //! \cond INTERNAL
  //! Applies a matrix operation to the current transformation matrix (internal).
  template<typename... Args>
  BL_INLINE BLResult _applyMatrixOpV(uint32_t opType, Args&&... args) noexcept {
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

  static BL_INLINE const BLPattern& none() noexcept { return reinterpret_cast<const BLPattern*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_BLPATTERN_H
