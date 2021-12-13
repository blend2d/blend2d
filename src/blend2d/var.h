
// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_VAR_H_INCLUDED
#define BLEND2D_VAR_H_INCLUDED

#include "object.h"
#include "rgba.h"

//! \addtogroup blend2d_api_globals
//! \{

//! \name BLVar - C API
//! \{

//! Var [C API].
struct BLVarCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blVarInitType(BLUnknown* self, BLObjectType type) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitNull(BLUnknown* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitBool(BLUnknown* self, bool value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitInt32(BLUnknown* self, int32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitInt64(BLUnknown* self, int64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitUInt32(BLUnknown* self, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitUInt64(BLUnknown* self, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitDouble(BLUnknown* self, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitRgba(BLUnknown* self, const BLRgba* rgba) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitRgba32(BLUnknown* self, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitRgba64(BLUnknown* self, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitMove(BLUnknown* self, BLUnknown* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarInitWeak(BLUnknown* self, const BLUnknown* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarDestroy(BLUnknown* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarReset(BLUnknown* self) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blVarAssignNull(BLUnknown* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignBool(BLUnknown* self, bool value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignInt32(BLUnknown* self, int32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignInt64(BLUnknown* self, int64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignUInt32(BLUnknown* self, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignUInt64(BLUnknown* self, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignDouble(BLUnknown* self, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignRgba(BLUnknown* self, const BLRgba* rgba) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignRgba32(BLUnknown* self, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignRgba64(BLUnknown* self, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignMove(BLUnknown* self, BLUnknown* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarAssignWeak(BLUnknown* self, const BLUnknown* other) BL_NOEXCEPT_C;

BL_API BLObjectType BL_CDECL blVarGetType(const BLUnknown* self) BL_NOEXCEPT_C BL_PURE;

BL_API BLResult BL_CDECL blVarToBool(const BLUnknown* self, bool* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarToInt32(const BLUnknown* self, int32_t* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarToInt64(const BLUnknown* self, int64_t* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarToUInt32(const BLUnknown* self, uint32_t* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarToUInt64(const BLUnknown* self, uint64_t* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarToDouble(const BLUnknown* self, double* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarToRgba(const BLUnknown* self, BLRgba* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarToRgba32(const BLUnknown* self, uint32_t* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blVarToRgba64(const BLUnknown* self, uint64_t* out) BL_NOEXCEPT_C;

BL_API bool BL_CDECL blVarEquals(const BLUnknown* a, const BLUnknown* b) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL blVarEqualsNull(const BLUnknown* self) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL blVarEqualsBool(const BLUnknown* self, bool value) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL blVarEqualsInt64(const BLUnknown* self, int64_t value) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL blVarEqualsUInt64(const BLUnknown* self, uint64_t value) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL blVarEqualsDouble(const BLUnknown* self, double value) BL_NOEXCEPT_C BL_PURE;

BL_API bool BL_CDECL blVarStrictEquals(const BLUnknown* a, const BLUnknown* b) BL_NOEXCEPT_C BL_PURE;

BL_END_C_DECLS

//! \}

//! \name BLVar - C++ API
//! \{

#ifdef __cplusplus

//! \cond INTERNAL
namespace BLInternal {
namespace {

//! Helper class that provides static functions that are called by `BLVar` member functions. There are
//! several specializations for primitive types like int/float and also for special types like `BLRgba`.
template<typename T, uint32_t Category>
struct VarOps;

template<typename T>
struct VarOps<T, kTypeCategoryBool> {
  static BL_INLINE void init(BLVarCore* self, const T& value) noexcept {
    self->_d.initBool(value);
  }

  static BL_INLINE BLResult assign(BLVarCore* self, const T& value) noexcept {
    return blVarAssignBool(self, value);
  }

  static BL_INLINE bool equals(const BLVarCore* self, const T& value) noexcept {
    return blVarEqualsBool(self, value);
  }
};

template<typename T>
struct VarOps<T, kTypeCategoryInt> {
  static BL_INLINE void init(BLVarCore* self, const T& value) noexcept {
    if (std::is_signed<T>::value)
      self->_d.initInt64(int64_t(value));
    else
      self->_d.initUInt64(uint64_t(value));
  }

  static BL_INLINE BLResult assign(BLVarCore* self, const T& value) noexcept {
    if (sizeof(T) <= 4 && std::is_signed<T>::value)
      return blVarAssignInt32(self, int32_t(value));
    else if (sizeof(T) <= 4)
      return blVarAssignUInt32(self, uint32_t(value));
    else if (std::is_signed<T>::value)
      return blVarAssignInt64(self, int64_t(value));
    else
      return blVarAssignUInt64(self, uint64_t(value));
  }

  static BL_INLINE bool equals(const BLVarCore* self, const T& value) noexcept {
    if (std::is_signed<T>::value)
      return blVarEqualsInt64(self, int64_t(value));
    else
      return blVarEqualsUInt64(self, uint64_t(value));
  }
};

template<typename T>
struct VarOps<T, kTypeCategoryFloat> {
  static BL_INLINE void init(BLVarCore* self, const T& value) noexcept {
    self->_d.initDouble(double(value));
  }

  static BL_INLINE BLResult assign(BLVarCore* self, const T& value) noexcept {
    return blVarAssignDouble(self, double(value));
  }

  static BL_INLINE bool equals(const BLVarCore* self, const T& value) noexcept {
    return blVarEqualsDouble(self, double(value));
  }
};

template<typename T>
struct VarOps<T, kTypeCategoryObject> {
  static BL_INLINE void init(BLVarCore* self, T&& value) noexcept { blVarInitMove(self, &value); }
  static BL_INLINE void init(BLVarCore* self, const T& value) noexcept { blVarInitWeak(self, &value); }

  static BL_INLINE BLResult assign(BLVarCore* self, T&& value) noexcept { return blVarAssignMove(self, &value); }
  static BL_INLINE BLResult assign(BLVarCore* self, const T& value) noexcept { return blVarAssignWeak(self, &value); }

  static BL_INLINE bool equals(const BLVarCore* self, const T& value) noexcept { return blVarEquals(self, &value); }
};

template<>
struct VarOps<BLRgba, kTypeCategoryStruct> {
  static BL_INLINE void init(BLVarCore* self, const BLRgba& value) noexcept {
    // Inlined version of `blVarInitRgba()`.
    uint32_t r = blBitCast<uint32_t>(value.r);
    uint32_t g = blBitCast<uint32_t>(value.g);
    uint32_t b = blBitCast<uint32_t>(value.b);
    uint32_t a = blBitCast<uint32_t>(blMax(0.0f, value.a)) & 0x7FFFFFFFu;

    self->_d.initU32x4(r, g, b, a);
  }

  static BL_INLINE BLResult assign(BLVarCore* self, const BLRgba& value) noexcept {
    return blVarAssignRgba(self, &value);
  }

  static BL_INLINE bool equals(const BLVarCore* self, const BLRgba& value) noexcept {
    return blVarStrictEquals(self, &value);
  }
};

template<typename T>
struct VarCastImpl {
  static BL_INLINE T* cast(BLVarCore* self) noexcept { return static_cast<T*>(static_cast<BLObjectCore*>(self)); }
  static BL_INLINE const T* cast(const BLVarCore* self) noexcept { return static_cast<const T*>(static_cast<const BLObjectCore*>(self)); }
};

template<>
struct VarCastImpl<BLRgba> {
  static BL_INLINE BLRgba* cast(BLVarCore* self) noexcept { return reinterpret_cast<BLRgba*>(self); }
  static BL_INLINE const BLRgba* cast(const BLVarCore* self) noexcept { return reinterpret_cast<const BLRgba*>(self); }
};

} // {anonymous}
} // {BLInternal}
//! \endcond

//! Value [C++ API].
class BLVar : public BLVarCore {
public:
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLVar() noexcept { _d.initStatic(BL_OBJECT_TYPE_NULL); }

  BL_INLINE BLVar(BLVar&& other) noexcept {
    _d = other._d;
    other._d.initStatic(BL_OBJECT_TYPE_NULL);
  }

  BL_INLINE BLVar(const BLVar& other) noexcept { blVarInitWeak(this, &other); }

  template<typename T>
  BL_INLINE explicit BLVar(T&& value) noexcept {
    typedef typename std::decay<T>::type DecayT;
    BLInternal::VarOps<DecayT, BLInternal::TypeTraits<DecayT>::kCategory>::init(this, std::forward<T>(value));
  }

  BL_INLINE ~BLVar() noexcept { blVarDestroy(this); }

  //! \}

  //! \name Static Constructors
  //! \{

  BL_INLINE static BLVar null() noexcept { return BLVar(); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  template<typename T>
  BL_INLINE BLVar& operator=(T&& value) noexcept { assign(std::forward<T>(value)); return *this; }

  template<typename T>
  BL_INLINE bool operator==(const T& other) const noexcept { return equals(other); }

  template<typename T>
  BL_INLINE bool operator!=(const T& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blVarReset(this); }
  BL_INLINE void swap(BLVarCore& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Type Accessors
  //! \{

  //! Returns the type of the underlying object.
  BL_NODISCARD
  BL_INLINE BLObjectType type() const noexcept { return _d.getType(); }

  //! Tests whether this `BLVar` instance represents a `BLArray<T>` storing any supported type.
  BL_NODISCARD
  BL_INLINE bool isArray() const noexcept { return _d.isArray(); }

  //! Tests whether this `BLVar` instance represents `BLBitSet`.
  BL_NODISCARD
  BL_INLINE bool isBitSet() const noexcept { return _d.isBitSet(); }

  //! Tests whether this `BLVar` instance represents a boxed `bool` value.
  BL_NODISCARD
  BL_INLINE bool isBool() const noexcept { return _d.isBool(); }

  //! Tests whether this `BLVar` instance represents `BLContext`.
  BL_NODISCARD
  BL_INLINE bool isContext() const noexcept { return _d.isContext(); }

  //! Tests whether this `BLVar` instance represents a boxed `double` value.
  BL_NODISCARD
  BL_INLINE bool isDouble() const noexcept { return _d.isDouble(); }

  //! Tests whether this `BLVar` instance represents `BLFont`.
  BL_NODISCARD
  BL_INLINE bool isFont() const noexcept { return _d.isFont(); }

  //! Tests whether this `BLVar` instance represents `BLFontData`.
  BL_NODISCARD
  BL_INLINE bool isFontData() const noexcept { return _d.isFontData(); }

  //! Tests whether this `BLVar` instance represents `BLFontFace`.
  BL_NODISCARD
  BL_INLINE bool isFontFace() const noexcept { return _d.isFontFace(); }

  //! Tests whether this `BLVar` instance represents `BLFontManager`.
  BL_NODISCARD
  BL_INLINE bool isFontManager() const noexcept { return _d.isFontManager(); }

  //! Tests whether this `BLVar` instance represents `BLGradient`.
  BL_NODISCARD
  BL_INLINE bool isGradient() const noexcept { return _d.isGradient(); }

  //! Tests whether this `BLVar` instance represents `BLImage`.
  BL_NODISCARD
  BL_INLINE bool isImage() const noexcept { return _d.isImage(); }

  //! Tests whether this `BLVar` instance represents `BLImageCodec`.
  BL_NODISCARD
  BL_INLINE bool isImageCodec() const noexcept { return _d.isImageCodec(); }

  //! Tests whether this `BLVar` instance represents `BLImageDecoder`.
  BL_NODISCARD
  BL_INLINE bool isImageDecoder() const noexcept { return _d.isImageDecoder(); }

  //! Tests whether this `BLVar` instance represents `BLImageEncoder`.
  BL_NODISCARD
  BL_INLINE bool isImageEncoder() const noexcept { return _d.isImageEncoder(); }

  //! Tests whether this `BLVar` instance represents a boxed `int64_t` value.
  BL_NODISCARD
  BL_INLINE bool isInt64() const noexcept { return _d.isInt64(); }

  //! Tests whether this `BLVar` instance represents a null value.
  BL_NODISCARD
  BL_INLINE bool isNull() const noexcept { return _d.isNull(); }

  //! Tests whether this `BLVar` instance represents `BLPath`.
  BL_NODISCARD
  BL_INLINE bool isPath() const noexcept { return _d.isPath(); }

  //! Tests whether this `BLVar` instance represents `BLPattern.`
  BL_NODISCARD
  BL_INLINE bool isPattern() const noexcept { return _d.isPattern(); }

  //! Tests whether this `BLVar` instance represents `BLString`.
  BL_NODISCARD
  BL_INLINE bool isString() const noexcept { return _d.isString(); }

  //! Tests whether this `BLVar` instance represents boxed `BLRgba`.
  BL_NODISCARD
  BL_INLINE bool isRgba() const noexcept { return _d.isRgba(); }

  //! Tests whether this `BLVar` instance represents a boxed `uint64_t` value.
  BL_NODISCARD
  BL_INLINE bool isUInt64() const noexcept { return _d.isUInt64(); }

  //! Tests whether this `BLVar` instance is a style that can be used with the rendering context.
  BL_NODISCARD
  BL_INLINE bool isStyle() const noexcept { return _d.isStyle(); }

  //! Converts this value to `bool` and stores the result in `out`.
  BL_INLINE BLResult toBool(bool* out) const noexcept { return blVarToBool(this, out); }
  //! Converts this value to `int32_t` and stores the result in `out`.
  BL_INLINE BLResult toInt32(int32_t* out) const noexcept { return blVarToInt32(this, out); }
  //! Converts this value to `int64_t` and stores the result in `out`.
  BL_INLINE BLResult toInt64(int64_t* out) const noexcept { return blVarToInt64(this, out); }
  //! Converts this value to `uint32_t` and stores the result in `out`.
  BL_INLINE BLResult toUInt32(uint32_t* out) const noexcept { return blVarToUInt32(this, out); }
  //! Converts this value to `uint64_t` and stores the result in `out`.
  BL_INLINE BLResult toUInt64(uint64_t* out) const noexcept { return blVarToUInt64(this, out); }
  //! Converts this value to `double` precision floating point and stores the result in `out`.
  BL_INLINE BLResult toDouble(double* out) const noexcept { return blVarToDouble(this, out); }
  //! Converts this value to `BLRgba` and stores the result in `out`.
  BL_INLINE BLResult toRgba(BLRgba* out) const noexcept { return blVarToRgba(this, out); }
  //! Converts this value to `BLRgba32` and stores the result in `out`.
  BL_INLINE BLResult toRgba32(BLRgba32* out) const noexcept { return blVarToRgba32(this, &out->value); }
  //! Converts this value to `BLRgba64` and stores the result in `out`.
  BL_INLINE BLResult toRgba64(BLRgba64* out) const noexcept { return blVarToRgba64(this, &out->value); }

  //! \}

  //! \name Properties
  //! \{

  BL_DEFINE_OBJECT_PROPERTY_API

  //! \}

  //! \name Casts
  //! \{

  //! Casts this \ref BLVar instance to `T&`.
  template<typename T>
  BL_INLINE T& as() noexcept { return *BLInternal::VarCastImpl<T>::cast(this); }

  //! Casts this \ref BLVar instance to `const T&`.
  template<typename T>
  BL_INLINE const T& as() const noexcept { return *BLInternal::VarCastImpl<T>::cast(this); }

  //! \}

  //! \name Assignment
  //! \{

  template<typename T>
  BL_INLINE BLResult assign(T&& value) noexcept {
    typedef typename std::decay<T>::type DecayT;
    return BLInternal::VarOps<DecayT, BLInternal::TypeTraits<DecayT>::kCategory>::assign(this, std::forward<T>(value));
  }

  //! \}

  //! \name Equality
  //! \{

  template<typename T>
  BL_NODISCARD
  BL_INLINE bool equals(const T& value) const noexcept {
    typedef typename std::decay<T>::type DecayT;
    return BLInternal::VarOps<DecayT, BLInternal::TypeTraits<DecayT>::kCategory>::equals(this, value);
  }

  BL_NODISCARD
  BL_INLINE bool strictEquals(const BLVarCore& other) const noexcept { return blVarStrictEquals(this, &other); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_VAR_H_INCLUDED
