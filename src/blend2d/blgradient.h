// // [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLGRADIENT_H
#define BLEND2D_BLGRADIENT_H

#include "./blgeometry.h"
#include "./blmatrix.h"
#include "./blrgba.h"
#include "./blvariant.h"

//! \addtogroup blend2d_api_styles
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Gradient type.
BL_DEFINE_ENUM(BLGradientType) {
  //! Linear gradient type.
  BL_GRADIENT_TYPE_LINEAR = 0,
  //! Radial gradient type.
  BL_GRADIENT_TYPE_RADIAL = 1,
  //! Conical gradient type.
  BL_GRADIENT_TYPE_CONICAL = 2,

  //! Count of gradient types.
  BL_GRADIENT_TYPE_COUNT = 3
};

//! Gradient data index.
BL_DEFINE_ENUM(BLGradientValue) {
  //! x0 - start 'x' for Linear/Radial and center 'x' for Conical.
  BL_GRADIENT_VALUE_COMMON_X0 = 0,
  //! y0 - start 'y' for Linear/Radial and center 'y' for Conical.
  BL_GRADIENT_VALUE_COMMON_Y0 = 1,
  //! x1 - end 'x' for Linear/Radial.
  BL_GRADIENT_VALUE_COMMON_X1 = 2,
  //! y1 - end 'y' for Linear/Radial.
  BL_GRADIENT_VALUE_COMMON_Y1 = 3,
  //! Radial gradient r0 radius.
  BL_GRADIENT_VALUE_RADIAL_R0 = 4,
  //! Conical gradient angle.
  BL_GRADIENT_VALUE_CONICAL_ANGLE = 2,

  //! Count of gradient values.
  BL_GRADIENT_VALUE_COUNT = 6
};

// ============================================================================
// [BLGradientStop]
// ============================================================================

//! Defines an `offset` and `rgba` color that us used by `BLGradient` to define
//! a linear transition between colors.
struct BLGradientStop {
  double offset;
  BLRgba64 rgba;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE BLGradientStop() noexcept = default;
  BL_INLINE BLGradientStop(const BLGradientStop& other) noexcept = default;

  BL_INLINE BLGradientStop(double offset, const BLRgba32& rgba32) noexcept
    : offset(offset),
      rgba(rgba32) {}

  BL_INLINE BLGradientStop(double offset, const BLRgba64& rgba64) noexcept
    : offset(offset),
      rgba(rgba64) {}

  BL_INLINE void reset() noexcept {
    this->offset = 0.0;
    this->rgba.reset();
  }

  BL_INLINE void reset(double offset, const BLRgba32& rgba32) noexcept {
    this->offset = offset;
    this->rgba.reset(rgba32);
  }

  BL_INLINE void reset(double offset, const BLRgba64& rgba64) noexcept {
    this->offset = offset;
    this->rgba.reset(rgba64);
  }

  BL_INLINE bool equals(const BLGradientStop& other) const noexcept {
    return blEquals(this->offset, other.offset) &
           blEquals(this->rgba  , other.rgba  ) ;
  }

  BL_INLINE bool operator==(const BLGradientStop& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLGradientStop& other) const noexcept { return !equals(other); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLLinearGradientValues]
// ============================================================================

//! Linear gradient values packed into a structure.
struct BLLinearGradientValues {
  double x0;
  double y0;
  double x1;
  double y1;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus
  BL_INLINE BLLinearGradientValues() noexcept = default;
  BL_INLINE BLLinearGradientValues(const BLLinearGradientValues& other) noexcept = default;

  BL_INLINE BLLinearGradientValues(double x0, double y0, double x1, double y1) noexcept
    : x0(x0),
      y0(y0),
      x1(x1),
      y1(y1) {}

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLRadialGradientValues]
// ============================================================================

//! Radial gradient values packed into a structure.
struct BLRadialGradientValues {
  double x0;
  double y0;
  double x1;
  double y1;
  double r0;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE BLRadialGradientValues() noexcept = default;
  BL_INLINE BLRadialGradientValues(const BLRadialGradientValues& other) noexcept = default;

  BL_INLINE BLRadialGradientValues(double x0, double y0, double x1, double y1, double r0) noexcept
    : x0(x0),
      y0(y0),
      x1(x1),
      y1(y1),
      r0(r0) {}

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLConicalGradientValues]
// ============================================================================

//! Conical gradient values packed into a structure.
struct BLConicalGradientValues {
  double x0;
  double y0;
  double angle;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE BLConicalGradientValues() noexcept = default;
  BL_INLINE BLConicalGradientValues(const BLConicalGradientValues& other) noexcept = default;

  BL_INLINE BLConicalGradientValues(double x0, double y0, double angle) noexcept
    : x0(x0),
      y0(y0),
      angle(angle) {}

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLGradient - Core]
// ============================================================================

//! Gradient [C Interface - Impl].
struct BLGradientImpl {
  //! Union of either raw `stops` & `size` members or their `view`.
  union {
    struct {
      //! Gradient stop data.
      BLGradientStop* stops;
      //! Gradient stop count.
      size_t size;
    };
    #ifdef __cplusplus
    //! Gradient stop view (C++ only).
    BLArrayView<BLGradientStop> view;
    #endif
  };

  //! Stop capacity.
  size_t capacity;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Gradient type, see `BLGradientType`.
  uint8_t gradientType;
  //! Gradient extend mode, see `BLExtendMode`.
  uint8_t extendMode;
  //! Type of the transformation matrix.
  uint8_t matrixType;
  //! Reserved, must be zero.
  uint8_t reserved[1];

  //! Gradient transformation matrix.
  BLMatrix2D matrix;

  union {
    //! Gradient values (coordinates, radius, angle).
    double values[BL_GRADIENT_VALUE_COUNT];
    //! Linear parameters.
    BLLinearGradientValues linear;
    //! Radial parameters.
    BLRadialGradientValues radial;
    //! Conical parameters.
    BLConicalGradientValues conical;
  };
};

//! Gradient [C Interface - Core].
struct BLGradientCore {
  BLGradientImpl* impl;
};

// ============================================================================
// [BLGradient - C++]
// ============================================================================

#ifdef __cplusplus
//! Gradient [C++ API].
class BLGradient : public BLGradientCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_GRADIENT;
  //! \endcond

  //! \name Constructors and Destructors
  //! \{

  BL_INLINE BLGradient() noexcept { this->impl = none().impl; }
  BL_INLINE BLGradient(BLGradient&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLGradient(const BLGradient& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLGradient(BLGradientImpl* impl) noexcept { this->impl = impl; }

  BL_INLINE explicit BLGradient(uint32_t type, const double* values = nullptr) noexcept {
    blGradientInitAs(this, type, values, BL_EXTEND_MODE_PAD, nullptr, 0, nullptr);
  }

  BL_INLINE explicit BLGradient(const BLLinearGradientValues& values, uint32_t extendMode = BL_EXTEND_MODE_PAD) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE explicit BLGradient(const BLRadialGradientValues& values, uint32_t extendMode = BL_EXTEND_MODE_PAD) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE explicit BLGradient(const BLConicalGradientValues& values, uint32_t extendMode = BL_EXTEND_MODE_PAD) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE BLGradient(const BLLinearGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLGradient(const BLRadialGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLGradient(const BLConicalGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLGradient(const BLLinearGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, stops, n, &m);
  }

  BL_INLINE BLGradient(const BLRadialGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, stops, n, &m);
  }

  BL_INLINE BLGradient(const BLConicalGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, stops, n, &m);
  }

  BL_INLINE ~BLGradient() noexcept { blGradientReset(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLGradient& operator=(BLGradient&& other) noexcept { blGradientAssignMove(this, &other); return *this; }
  BL_INLINE BLGradient& operator=(const BLGradient& other) noexcept { blGradientAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLGradient& other) const noexcept { return  blGradientEquals(this, &other); }
  BL_INLINE bool operator!=(const BLGradient& other) const noexcept { return !blGradientEquals(this, &other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blGradientReset(this); }
  BL_INLINE void swap(BLGradient& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLGradient&& other) noexcept { return blGradientAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLGradient& other) noexcept { return blGradientAssignWeak(this, &other); }

  //! Get whether the gradient is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }

  BL_INLINE bool equals(const BLGradient& other) const noexcept { return blGradientEquals(this, &other); }

  //! \}

  //! \name Create Gradient
  //! \{

  BL_INLINE BLResult create(const BLLinearGradientValues& values, uint32_t extendMode = BL_EXTEND_MODE_PAD) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE BLResult create(const BLRadialGradientValues& values, uint32_t extendMode = BL_EXTEND_MODE_PAD) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE BLResult create(const BLConicalGradientValues& values, uint32_t extendMode = BL_EXTEND_MODE_PAD) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE BLResult create(const BLLinearGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLResult create(const BLRadialGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLResult create(const BLConicalGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLResult create(const BLLinearGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, stops, n, &m);
  }

  BL_INLINE BLResult create(const BLRadialGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, stops, n, &m);
  }

  BL_INLINE BLResult create(const BLConicalGradientValues& values, uint32_t extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, stops, n, &m);
  }

  //! \}

  //! \name Gradient Options
  //! \{

  //! Gets the type of the gradient, see `BLGradientType`.
  BL_INLINE uint32_t type() const noexcept { return impl->gradientType; }
  //! Sets the gradient type, see `BLGradientType`.
  BL_INLINE BLResult setType(uint32_t type) noexcept { return blGradientSetType(this, type); }

  //! Gets extend mode, see `BLExtendMode`.
  BL_INLINE uint32_t extendMode() const noexcept { return impl->extendMode; }
  //! Set extend mode, see `BLExtendMode`.
  BL_INLINE BLResult setExtendMode(uint32_t extendMode) noexcept { return blGradientSetExtendMode(this, extendMode); }
  //! Reset extend mode to `BL_EXTEND_MODE_PAD`.
  BL_INLINE BLResult resetExtendMode() noexcept { return blGradientSetExtendMode(this, BL_EXTEND_MODE_PAD); }

  BL_INLINE double value(size_t index) const noexcept {
    BL_ASSERT(index < BL_GRADIENT_VALUE_COUNT);
    return impl->values[index];
  }

  BL_INLINE const BLLinearGradientValues& linear() const noexcept { return impl->linear; }
  BL_INLINE const BLRadialGradientValues& radial() const noexcept { return impl->radial; }
  BL_INLINE const BLConicalGradientValues& conical() const noexcept { return impl->conical; }

  BL_INLINE BLResult setValue(size_t index, double value) noexcept { return blGradientSetValue(this, index, value); }
  BL_INLINE BLResult setValues(size_t index, const double* values, size_t n) noexcept { return blGradientSetValues(this, index, values, n); }

  BL_INLINE BLResult setValues(const BLLinearGradientValues& values) noexcept { return setValues(0, (const double*)&values, sizeof(BLLinearGradientValues) / sizeof(double)); }
  BL_INLINE BLResult setValues(const BLRadialGradientValues& values) noexcept { return setValues(0, (const double*)&values, sizeof(BLRadialGradientValues) / sizeof(double)); }
  BL_INLINE BLResult setValues(const BLConicalGradientValues& values) noexcept { return setValues(0, (const double*)&values, sizeof(BLConicalGradientValues) / sizeof(double)); }

  BL_INLINE double x0() const noexcept { return impl->values[BL_GRADIENT_VALUE_COMMON_X0]; }
  BL_INLINE double y0() const noexcept { return impl->values[BL_GRADIENT_VALUE_COMMON_Y0]; }
  BL_INLINE double x1() const noexcept { return impl->values[BL_GRADIENT_VALUE_COMMON_X1]; }
  BL_INLINE double y1() const noexcept { return impl->values[BL_GRADIENT_VALUE_COMMON_Y1]; }
  BL_INLINE double r0() const noexcept { return impl->values[BL_GRADIENT_VALUE_RADIAL_R0]; }
  BL_INLINE double angle() const noexcept { return impl->values[BL_GRADIENT_VALUE_CONICAL_ANGLE]; }

  BL_INLINE BLResult setX0(double value) noexcept { return setValue(BL_GRADIENT_VALUE_COMMON_X0, value); }
  BL_INLINE BLResult setY0(double value) noexcept { return setValue(BL_GRADIENT_VALUE_COMMON_Y0, value); }
  BL_INLINE BLResult setX1(double value) noexcept { return setValue(BL_GRADIENT_VALUE_COMMON_X1, value); }
  BL_INLINE BLResult setY1(double value) noexcept { return setValue(BL_GRADIENT_VALUE_COMMON_Y1, value); }
  BL_INLINE BLResult setR0(double value) noexcept { return setValue(BL_GRADIENT_VALUE_RADIAL_R0, value); }
  BL_INLINE BLResult setAngle(double value) noexcept { return setValue(BL_GRADIENT_VALUE_CONICAL_ANGLE, value); }

  //! \}

  //! \name Gradient Stops
  //! \{

  BL_INLINE bool empty() const noexcept { return impl->size == 0; }
  BL_INLINE size_t size() const noexcept { return impl->size; }
  BL_INLINE size_t capacity() const noexcept { return impl->capacity; }

  //! Reserve the capacity of gradient stops for at least `n` stops.
  BL_INLINE BLResult reserve(size_t n) noexcept { return blGradientReserve(this, n); }
  //! Shrink the capacity of gradient stops to fit the current usage.
  BL_INLINE BLResult shrink() noexcept { return blGradientShrink(this); }

  BL_INLINE const BLGradientStop* stops() const noexcept { return impl->stops; }

  BL_INLINE const BLGradientStop& stopAt(size_t i) const noexcept {
    BL_ASSERT(i < impl->size);
    return impl->stops[i];
  }

  BL_INLINE BLResult resetStops() noexcept { return blGradientResetStops(this); }
  BL_INLINE BLResult assignStops(const BLGradientStop* stops, size_t n) noexcept { return blGradientAssignStops(this, stops, n); }
  BL_INLINE BLResult addStop(double offset, const BLRgba32& rgba32) noexcept { return blGradientAddStopRgba32(this, offset, rgba32.value); }
  BL_INLINE BLResult addStop(double offset, const BLRgba64& rgba64) noexcept { return blGradientAddStopRgba64(this, offset, rgba64.value); }
  BL_INLINE BLResult removeStop(size_t index) noexcept { return blGradientRemoveStop(this, index); }
  BL_INLINE BLResult removeStopByOffset(double offset, bool all = true) noexcept { return blGradientRemoveStopByOffset(this, offset, all); }
  BL_INLINE BLResult removeStops(const BLRange& range) noexcept { return blGradientRemoveStops(this, &range); }
  BL_INLINE BLResult removeStopsByOffset(double offsetMin, double offsetMax) noexcept { return blGradientRemoveStopsFromTo(this, offsetMin, offsetMax); }
  BL_INLINE BLResult replaceStop(size_t index, double offset, const BLRgba32& rgba32) noexcept { return blGradientReplaceStopRgba32(this, index, offset, rgba32.value); }
  BL_INLINE BLResult replaceStop(size_t index, double offset, const BLRgba64& rgba64) noexcept { return blGradientReplaceStopRgba64(this, index, offset, rgba64.value); }
  BL_INLINE size_t indexOfStop(double offset) const noexcept { return blGradientIndexOfStop(this, offset) ;}

  //! \}

  //! \name Transformations
  //! \{

  BL_INLINE bool hasMatrix() const noexcept { return impl->matrixType != BL_MATRIX2D_TYPE_IDENTITY; }
  BL_INLINE uint32_t matrixType() const noexcept { return impl->matrixType; }
  BL_INLINE const BLMatrix2D& matrix() const noexcept { return impl->matrix; }

  //! Applies a matrix operation to the current transformation matrix (internal).
  BL_INLINE BLResult _applyMatrixOp(uint32_t opType, const void* opData) noexcept {
    return blGradientApplyMatrixOp(this, opType, opData);
  }

  //! \cond INTERNAL
  //! Applies a matrix operation to the current transformation matrix (internal).
  template<typename... Args>
  BL_INLINE BLResult _applyMatrixOpV(uint32_t opType, Args&&... args) noexcept {
    double opData[] = { double(args)... };
    return blGradientApplyMatrixOp(this, opType, opData);
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

  static BL_INLINE const BLGradient& none() noexcept { return reinterpret_cast<const BLGradient*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_BLGRADIENT_H
