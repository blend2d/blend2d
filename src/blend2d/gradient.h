// // This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GRADIENT_H_INCLUDED
#define BLEND2D_GRADIENT_H_INCLUDED

#include "geometry.h"
#include "matrix.h"
#include "object.h"
#include "rgba.h"

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_SHADOW)

//! \addtogroup blend2d_api_styling
//! \{

//! \name BLGradient - Constants
//! \{

//! Gradient type.
BL_DEFINE_ENUM(BLGradientType) {
  //! Linear gradient type.
  BL_GRADIENT_TYPE_LINEAR = 0,
  //! Radial gradient type.
  BL_GRADIENT_TYPE_RADIAL = 1,
  //! Conical gradient type.
  BL_GRADIENT_TYPE_CONICAL = 2,

  //! Maximum value of `BLGradientType`.
  BL_GRADIENT_TYPE_MAX_VALUE = 2

  BL_FORCE_ENUM_UINT32(BL_GRADIENT_TYPE)
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

  //! Maximum value of `BLGradientValue`.
  BL_GRADIENT_VALUE_MAX_VALUE = 5

  BL_FORCE_ENUM_UINT32(BL_GRADIENT_VALUE)
};

//! \}

//! \name BLGradient - Structs
//! \{

//! Defines an `offset` and `rgba` color that us used by `BLGradient` to define
//! a linear transition between colors.
struct BLGradientStop {
  double offset;
  BLRgba64 rgba;

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLGradientStop() noexcept = default;
  BL_INLINE BLGradientStop(const BLGradientStop& other) noexcept = default;

  BL_INLINE BLGradientStop(double offset, const BLRgba32& rgba32) noexcept
    : offset(offset),
      rgba(rgba32) {}

  BL_INLINE BLGradientStop(double offset, const BLRgba64& rgba64) noexcept
    : offset(offset),
      rgba(rgba64) {}

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLGradientStop& operator=(const BLGradientStop& other) noexcept = default;

  BL_INLINE bool operator==(const BLGradientStop& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLGradientStop& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

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

  //! \}
#endif
};

//! Linear gradient values packed into a structure.
struct BLLinearGradientValues {
  double x0;
  double y0;
  double x1;
  double y1;

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLLinearGradientValues() noexcept = default;
  BL_INLINE BLLinearGradientValues(const BLLinearGradientValues& other) noexcept = default;

  BL_INLINE BLLinearGradientValues(double x0, double y0, double x1, double y1) noexcept
    : x0(x0),
      y0(y0),
      x1(x1),
      y1(y1) {}

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  //! \}
#endif
};

//! Radial gradient values packed into a structure.
struct BLRadialGradientValues {
  double x0;
  double y0;
  double x1;
  double y1;
  double r0;

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLRadialGradientValues() noexcept = default;
  BL_INLINE BLRadialGradientValues(const BLRadialGradientValues& other) noexcept = default;

  BL_INLINE BLRadialGradientValues(double x0, double y0, double x1, double y1, double r0) noexcept
    : x0(x0),
      y0(y0),
      x1(x1),
      y1(y1),
      r0(r0) {}

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  //! \}
#endif
};

//! Conical gradient values packed into a structure.
struct BLConicalGradientValues {
  double x0;
  double y0;
  double angle;

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLConicalGradientValues() noexcept = default;
  BL_INLINE BLConicalGradientValues(const BLConicalGradientValues& other) noexcept = default;

  BL_INLINE BLConicalGradientValues(double x0, double y0, double angle) noexcept
    : x0(x0),
      y0(y0),
      angle(angle) {}

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  //! \}
#endif
};

//! \}

//! \name BLGradient - C API
//!
//! \{

//! \cond INTERNAL
//! Gradient [Impl].
struct BLGradientImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Gradient stop data.
  BLGradientStop* stops;
  //! Gradient stop count.
  size_t size;
  //! Stop capacity.
  size_t capacity;

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
    double values[BL_GRADIENT_VALUE_MAX_VALUE + 1];
    //! Linear parameters.
    BLLinearGradientValues linear;
    //! Radial parameters.
    BLRadialGradientValues radial;
    //! Conical parameters.
    BLConicalGradientValues conical;
  };
};
//! \endcond

//! Gradient [C API].
struct BLGradientCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blGradientInit(BLGradientCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientInitMove(BLGradientCore* self, BLGradientCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientInitWeak(BLGradientCore* self, const BLGradientCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientInitAs(BLGradientCore* self, BLGradientType type, const void* values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D* m) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientDestroy(BLGradientCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientReset(BLGradientCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientAssignMove(BLGradientCore* self, BLGradientCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientAssignWeak(BLGradientCore* self, const BLGradientCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientCreate(BLGradientCore* self, BLGradientType type, const void* values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D* m) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientShrink(BLGradientCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientReserve(BLGradientCore* self, size_t n) BL_NOEXCEPT_C;
BL_API BLGradientType BL_CDECL blGradientGetType(const BLGradientCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blGradientSetType(BLGradientCore* self, BLGradientType type) BL_NOEXCEPT_C;
BL_API BLExtendMode BL_CDECL blGradientGetExtendMode(BLGradientCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blGradientSetExtendMode(BLGradientCore* self, BLExtendMode extendMode) BL_NOEXCEPT_C;
BL_API double BL_CDECL blGradientGetValue(const BLGradientCore* self, size_t index) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blGradientSetValue(BLGradientCore* self, size_t index, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientSetValues(BLGradientCore* self, size_t index, const double* values, size_t n) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blGradientGetSize(const BLGradientCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL blGradientGetCapacity(const BLGradientCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const BLGradientStop* BL_CDECL blGradientGetStops(const BLGradientCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blGradientResetStops(BLGradientCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientAssignStops(BLGradientCore* self, const BLGradientStop* stops, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientAddStopRgba32(BLGradientCore* self, double offset, uint32_t argb32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientAddStopRgba64(BLGradientCore* self, double offset, uint64_t argb64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientRemoveStop(BLGradientCore* self, size_t index) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientRemoveStopByOffset(BLGradientCore* self, double offset, uint32_t all) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientRemoveStopsByIndex(BLGradientCore* self, size_t rStart, size_t rEnd) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientRemoveStopsByOffset(BLGradientCore* self, double offsetMin, double offsetMax) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientReplaceStopRgba32(BLGradientCore* self, size_t index, double offset, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGradientReplaceStopRgba64(BLGradientCore* self, size_t index, double offset, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blGradientIndexOfStop(const BLGradientCore* self, double offset) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blGradientApplyMatrixOp(BLGradientCore* self, BLMatrix2DOp opType, const void* opData) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blGradientEquals(const BLGradientCore* a, const BLGradientCore* b) BL_NOEXCEPT_C BL_PURE;

BL_END_C_DECLS

//! \}

//! \name BLGradient - C++ API
//!
//! \{
#ifdef __cplusplus

//! Gradient [C++ API].
class BLGradient : public BLGradientCore {
public:
  //! \cond INTERNAL
  BL_INLINE BLGradientImpl* _impl() const noexcept { return static_cast<BLGradientImpl*>(_d.impl); }
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLGradient() noexcept { blGradientInit(this); }
  BL_INLINE BLGradient(BLGradient&& other) noexcept { blGradientInitMove(this, &other); }
  BL_INLINE BLGradient(const BLGradient& other) noexcept { blGradientInitWeak(this, &other); }

  BL_INLINE explicit BLGradient(BLGradientType type, const double* values = nullptr) noexcept {
    blGradientInitAs(this, type, values, BL_EXTEND_MODE_PAD, nullptr, 0, nullptr);
  }

  BL_INLINE explicit BLGradient(const BLLinearGradientValues& values, BLExtendMode extendMode = BL_EXTEND_MODE_PAD) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE explicit BLGradient(const BLRadialGradientValues& values, BLExtendMode extendMode = BL_EXTEND_MODE_PAD) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE explicit BLGradient(const BLConicalGradientValues& values, BLExtendMode extendMode = BL_EXTEND_MODE_PAD) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE BLGradient(const BLLinearGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLGradient(const BLRadialGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLGradient(const BLConicalGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLGradient(const BLLinearGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, stops, n, &m);
  }

  BL_INLINE BLGradient(const BLRadialGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, stops, n, &m);
  }

  BL_INLINE BLGradient(const BLConicalGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    blGradientInitAs(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, stops, n, &m);
  }

  BL_INLINE ~BLGradient() noexcept { blGradientDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLGradient& operator=(BLGradient&& other) noexcept { blGradientAssignMove(this, &other); return *this; }
  BL_INLINE BLGradient& operator=(const BLGradient& other) noexcept { blGradientAssignWeak(this, &other); return *this; }

  BL_NODISCARD BL_INLINE bool operator==(const BLGradient& other) const noexcept { return  blGradientEquals(this, &other); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLGradient& other) const noexcept { return !blGradientEquals(this, &other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blGradientReset(this); }
  BL_INLINE void swap(BLGradient& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Create Gradient
  //! \{

  BL_INLINE BLResult create(const BLLinearGradientValues& values, BLExtendMode extendMode = BL_EXTEND_MODE_PAD) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE BLResult create(const BLRadialGradientValues& values, BLExtendMode extendMode = BL_EXTEND_MODE_PAD) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE BLResult create(const BLConicalGradientValues& values, BLExtendMode extendMode = BL_EXTEND_MODE_PAD) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, nullptr, 0, nullptr);
  }

  BL_INLINE BLResult create(const BLLinearGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLResult create(const BLRadialGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLResult create(const BLConicalGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, stops, n, nullptr);
  }

  BL_INLINE BLResult create(const BLLinearGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_LINEAR, &values, extendMode, stops, n, &m);
  }

  BL_INLINE BLResult create(const BLRadialGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_RADIAL, &values, extendMode, stops, n, &m);
  }

  BL_INLINE BLResult create(const BLConicalGradientValues& values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D& m) noexcept {
    return blGradientCreate(this, BL_GRADIENT_TYPE_CONICAL, &values, extendMode, stops, n, &m);
  }

  //! \}

  //! \name Accessors
  //! \{

  //! Returns the type of the gradient.
  BL_NODISCARD
  BL_INLINE BLGradientType type() const noexcept { return (BLGradientType)_impl()->gradientType; }

  //! Sets the type of the gradient.
  BL_INLINE BLResult setType(BLGradientType type) noexcept { return blGradientSetType(this, type); }

  //! Returns the gradient extend mode, see `BLExtendMode`.
  BL_NODISCARD
  BL_INLINE BLExtendMode extendMode() const noexcept { return (BLExtendMode)_impl()->extendMode; }

  //! Set the gradient extend mode, see `BLExtendMode`.
  BL_INLINE BLResult setExtendMode(BLExtendMode extendMode) noexcept { return blGradientSetExtendMode(this, extendMode); }
  //! Resets the gradient extend mode to `BL_EXTEND_MODE_PAD`.
  BL_INLINE BLResult resetExtendMode() noexcept { return blGradientSetExtendMode(this, BL_EXTEND_MODE_PAD); }

  BL_NODISCARD
  BL_INLINE double value(size_t index) const noexcept {
    BL_ASSERT(index <= BL_GRADIENT_VALUE_MAX_VALUE);
    return _impl()->values[index];
  }

  BL_NODISCARD
  BL_INLINE const BLLinearGradientValues& linear() const noexcept { return _impl()->linear; }

  BL_NODISCARD
  BL_INLINE const BLRadialGradientValues& radial() const noexcept { return _impl()->radial; }

  BL_NODISCARD
  BL_INLINE const BLConicalGradientValues& conical() const noexcept { return _impl()->conical; }

  BL_INLINE BLResult setValue(size_t index, double value) noexcept { return blGradientSetValue(this, index, value); }
  BL_INLINE BLResult setValues(size_t index, const double* values, size_t n) noexcept { return blGradientSetValues(this, index, values, n); }

  BL_INLINE BLResult setValues(const BLLinearGradientValues& values) noexcept { return setValues(0, (const double*)&values, sizeof(BLLinearGradientValues) / sizeof(double)); }
  BL_INLINE BLResult setValues(const BLRadialGradientValues& values) noexcept { return setValues(0, (const double*)&values, sizeof(BLRadialGradientValues) / sizeof(double)); }
  BL_INLINE BLResult setValues(const BLConicalGradientValues& values) noexcept { return setValues(0, (const double*)&values, sizeof(BLConicalGradientValues) / sizeof(double)); }

  BL_NODISCARD
  BL_INLINE double x0() const noexcept { return _impl()->values[BL_GRADIENT_VALUE_COMMON_X0]; }

  BL_NODISCARD
  BL_INLINE double y0() const noexcept { return _impl()->values[BL_GRADIENT_VALUE_COMMON_Y0]; }

  BL_NODISCARD
  BL_INLINE double x1() const noexcept { return _impl()->values[BL_GRADIENT_VALUE_COMMON_X1]; }

  BL_NODISCARD
  BL_INLINE double y1() const noexcept { return _impl()->values[BL_GRADIENT_VALUE_COMMON_Y1]; }

  BL_NODISCARD
  BL_INLINE double r0() const noexcept { return _impl()->values[BL_GRADIENT_VALUE_RADIAL_R0]; }

  BL_NODISCARD
  BL_INLINE double angle() const noexcept { return _impl()->values[BL_GRADIENT_VALUE_CONICAL_ANGLE]; }

  BL_INLINE BLResult setX0(double value) noexcept { return setValue(BL_GRADIENT_VALUE_COMMON_X0, value); }
  BL_INLINE BLResult setY0(double value) noexcept { return setValue(BL_GRADIENT_VALUE_COMMON_Y0, value); }
  BL_INLINE BLResult setX1(double value) noexcept { return setValue(BL_GRADIENT_VALUE_COMMON_X1, value); }
  BL_INLINE BLResult setY1(double value) noexcept { return setValue(BL_GRADIENT_VALUE_COMMON_Y1, value); }
  BL_INLINE BLResult setR0(double value) noexcept { return setValue(BL_GRADIENT_VALUE_RADIAL_R0, value); }
  BL_INLINE BLResult setAngle(double value) noexcept { return setValue(BL_GRADIENT_VALUE_CONICAL_ANGLE, value); }

  //! \}

  //! \name Gradient Stops
  //! \{

  //! Tests whether the gradient is empty.
  //!
  //! Empty gradient is considered any gradient that has no stops.
  BL_NODISCARD
  BL_INLINE bool empty() const noexcept { return _impl()->size == 0; }

  //! Returns the number of stops the gradient has.
  BL_NODISCARD
  BL_INLINE size_t size() const noexcept { return _impl()->size; }

  //! Returns the gradient capacity [in stops].
  BL_NODISCARD
  BL_INLINE size_t capacity() const noexcept { return _impl()->capacity; }

  //! Reserves the capacity of gradient for at least `n` stops.
  BL_INLINE BLResult reserve(size_t n) noexcept { return blGradientReserve(this, n); }
  //! Shrinks the capacity of gradient stops to fit the current use.
  BL_INLINE BLResult shrink() noexcept { return blGradientShrink(this); }

  //! Returns the gradient stop data.
  BL_NODISCARD
  BL_INLINE const BLGradientStop* stops() const noexcept { return _impl()->stops; }

  //! Returns a gradient stop at `i`.
  BL_NODISCARD
  BL_INLINE const BLGradientStop& stopAt(size_t i) const noexcept {
    BL_ASSERT(i < size());
    return _impl()->stops[i];
  }

  //! \}

  //! \name Content Manipulation
  //! \{

  BL_INLINE BLResult assign(BLGradient&& other) noexcept { return blGradientAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLGradient& other) noexcept { return blGradientAssignWeak(this, &other); }

  BL_INLINE BLResult resetStops() noexcept { return blGradientResetStops(this); }
  BL_INLINE BLResult assignStops(const BLGradientStop* stops, size_t n) noexcept { return blGradientAssignStops(this, stops, n); }
  BL_INLINE BLResult addStop(double offset, const BLRgba32& rgba32) noexcept { return blGradientAddStopRgba32(this, offset, rgba32.value); }
  BL_INLINE BLResult addStop(double offset, const BLRgba64& rgba64) noexcept { return blGradientAddStopRgba64(this, offset, rgba64.value); }
  BL_INLINE BLResult removeStop(size_t index) noexcept { return blGradientRemoveStop(this, index); }
  BL_INLINE BLResult removeStopByOffset(double offset, bool all = true) noexcept { return blGradientRemoveStopByOffset(this, offset, all); }
  BL_INLINE BLResult removeStops(const BLRange& range) noexcept { return blGradientRemoveStopsByIndex(this, range.start, range.end); }
  BL_INLINE BLResult removeStopsByOffset(double offsetMin, double offsetMax) noexcept { return blGradientRemoveStopsByOffset(this, offsetMin, offsetMax); }
  BL_INLINE BLResult replaceStop(size_t index, double offset, const BLRgba32& rgba32) noexcept { return blGradientReplaceStopRgba32(this, index, offset, rgba32.value); }
  BL_INLINE BLResult replaceStop(size_t index, double offset, const BLRgba64& rgba64) noexcept { return blGradientReplaceStopRgba64(this, index, offset, rgba64.value); }

  BL_NODISCARD
  BL_INLINE size_t indexOfStop(double offset) const noexcept { return blGradientIndexOfStop(this, offset) ;}

  //! \}

  //! \name Equality & Comparison
  //! \{

  BL_NODISCARD
  BL_INLINE bool equals(const BLGradient& other) const noexcept { return blGradientEquals(this, &other); }

  //! \}

  //! \name Transformations
  //! \{

  BL_NODISCARD
  BL_INLINE bool hasMatrix() const noexcept { return _impl()->matrixType != BL_MATRIX2D_TYPE_IDENTITY; }

  BL_NODISCARD
  BL_INLINE BLMatrix2DType matrixType() const noexcept { return (BLMatrix2DType)_impl()->matrixType; }

  BL_NODISCARD
  BL_INLINE const BLMatrix2D& matrix() const noexcept { return _impl()->matrix; }

  //! Applies a matrix operation to the current transformation matrix (internal).
  BL_INLINE BLResult _applyMatrixOp(BLMatrix2DOp opType, const void* opData) noexcept {
    return blGradientApplyMatrixOp(this, opType, opData);
  }

  //! \cond INTERNAL
  //! Applies a matrix operation to the current transformation matrix (internal).
  template<typename... Args>
  BL_INLINE BLResult _applyMatrixOpV(BLMatrix2DOp opType, Args&&... args) noexcept {
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
};

#endif
//! \}

//! \}

BL_DIAGNOSTIC_POP

#endif // BLEND2D_GRADIENT_H_INCLUDED
