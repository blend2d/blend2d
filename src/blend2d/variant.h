// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_VARIANT_H_INCLUDED
#define BLEND2D_VARIANT_H_INCLUDED

#include "./api.h"

//! \addtogroup blend2d_api_globals
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Impl type identifier used by to describe a Blend2D Impl.
BL_DEFINE_ENUM(BLImplType) {
  //! Type is `Null`.
  BL_IMPL_TYPE_NULL = 0,

  //! Type is `BLArray<T>` where `T` is `BLVariant` or other ref-counted type.
  BL_IMPL_TYPE_ARRAY_VAR = 1,
  //! Type is `BLArray<T>` where `T` matches 8-bit signed integral type.
  BL_IMPL_TYPE_ARRAY_I8 = 2,
  //! Type is `BLArray<T>` where `T` matches 8-bit unsigned integral type.
  BL_IMPL_TYPE_ARRAY_U8 = 3,
  //! Type is `BLArray<T>` where `T` matches 16-bit signed integral type.
  BL_IMPL_TYPE_ARRAY_I16 = 4,
  //! Type is `BLArray<T>` where `T` matches 16-bit unsigned integral type.
  BL_IMPL_TYPE_ARRAY_U16 = 5,
  //! Type is `BLArray<T>` where `T` matches 32-bit signed integral type.
  BL_IMPL_TYPE_ARRAY_I32 = 6,
  //! Type is `BLArray<T>` where `T` matches 32-bit unsigned integral type.
  BL_IMPL_TYPE_ARRAY_U32 = 7,
  //! Type is `BLArray<T>` where `T` matches 64-bit signed integral type.
  BL_IMPL_TYPE_ARRAY_I64 = 8,
  //! Type is `BLArray<T>` where `T` matches 64-bit unsigned integral type.
  BL_IMPL_TYPE_ARRAY_U64 = 9,
  //! Type is `BLArray<T>` where `T` matches 32-bit floating point type.
  BL_IMPL_TYPE_ARRAY_F32 = 10,
  //! Type is `BLArray<T>` where `T` matches 64-bit floating point type.
  BL_IMPL_TYPE_ARRAY_F64 = 11,
  //! Type is `BLArray<T>` where `T` is a struct of size 1.
  BL_IMPL_TYPE_ARRAY_STRUCT_1 = 12,
  //! Type is `BLArray<T>` where `T` is a struct of size 2.
  BL_IMPL_TYPE_ARRAY_STRUCT_2 = 13,
  //! Type is `BLArray<T>` where `T` is a struct of size 3.
  BL_IMPL_TYPE_ARRAY_STRUCT_3 = 14,
  //! Type is `BLArray<T>` where `T` is a struct of size 4.
  BL_IMPL_TYPE_ARRAY_STRUCT_4 = 15,
  //! Type is `BLArray<T>` where `T` is a struct of size 6.
  BL_IMPL_TYPE_ARRAY_STRUCT_6 = 16,
  //! Type is `BLArray<T>` where `T` is a struct of size 8.
  BL_IMPL_TYPE_ARRAY_STRUCT_8 = 17,
  //! Type is `BLArray<T>` where `T` is a struct of size 10.
  BL_IMPL_TYPE_ARRAY_STRUCT_10 = 18,
  //! Type is `BLArray<T>` where `T` is a struct of size 12.
  BL_IMPL_TYPE_ARRAY_STRUCT_12 = 19,
  //! Type is `BLArray<T>` where `T` is a struct of size 16.
  BL_IMPL_TYPE_ARRAY_STRUCT_16 = 20,
  //! Type is `BLArray<T>` where `T` is a struct of size 20.
  BL_IMPL_TYPE_ARRAY_STRUCT_20 = 21,
  //! Type is `BLArray<T>` where `T` is a struct of size 24.
  BL_IMPL_TYPE_ARRAY_STRUCT_24 = 22,
  //! Type is `BLArray<T>` where `T` is a struct of size 32.
  BL_IMPL_TYPE_ARRAY_STRUCT_32 = 23,

  //! Type is `BLBitArray`.
  BL_IMPL_TYPE_BIT_ARRAY = 32,
  //! Type is `BLBitSet`.
  BL_IMPL_TYPE_BIT_SET = 33,
  //! Type is `BLString`.
  BL_IMPL_TYPE_STRING = 39,

  //! Type is `BLPath`.
  BL_IMPL_TYPE_PATH = 40,
  //! Type is `BLRegion`.
  BL_IMPL_TYPE_REGION = 43,
  //! Type is `BLImage`.
  BL_IMPL_TYPE_IMAGE = 44,
  //! Type is `BLImageCodec`.
  BL_IMPL_TYPE_IMAGE_CODEC = 45,
  //! Type is `BLImageDecoder`.
  BL_IMPL_TYPE_IMAGE_DECODER = 46,
  //! Type is `BLImageEncoder`.
  BL_IMPL_TYPE_IMAGE_ENCODER = 47,
  //! Type is `BLGradient`.
  BL_IMPL_TYPE_GRADIENT = 48,
  //! Type is `BLPattern`.
  BL_IMPL_TYPE_PATTERN = 49,

  //! Type is `BLContext`.
  BL_IMPL_TYPE_CONTEXT = 55,

  //! Type is `BLFont`.
  BL_IMPL_TYPE_FONT = 56,
  //! Type is `BLFontFace`.
  BL_IMPL_TYPE_FONT_FACE = 57,
  //! Type is `BLFontData`.
  BL_IMPL_TYPE_FONT_DATA = 58,
  //! Type is `BLFontManager`.
  BL_IMPL_TYPE_FONT_MANAGER = 59,

  //! Type is `BLFontFeatureOptions`.
  BL_IMPL_TYPE_FONT_FEATURE_OPTIONS = 60,
  //! Type is `BLFontVariationOptions`.
  BL_IMPL_TYPE_FONT_VARIATION_OPTIONS = 61,

  //! Count of type identifiers including all reserved ones.
  BL_IMPL_TYPE_COUNT = 64
};

//! Impl traits that describe some details about a Blend2D `Impl` data.
BL_DEFINE_ENUM(BLImplTraits) {
  //! The data this container holds is mutable if `refCount == 1`.
  BL_IMPL_TRAIT_MUTABLE = 0x01u,
  //! The data this container holds is always immutable.
  BL_IMPL_TRAIT_IMMUTABLE = 0x02u,
  //! Set if the impl uses an external data (data is not part of impl).
  BL_IMPL_TRAIT_EXTERNAL = 0x04u,
  //! Set if the impl was not allocated by `blRuntimeAllocImpl()`.
  BL_IMPL_TRAIT_FOREIGN = 0x08u,
  //! Set if the impl provides a virtual function table (first member).
  BL_IMPL_TRAIT_VIRT = 0x10u,
  //! Set if the impl is a built-in null instance (default constructed).
  BL_IMPL_TRAIT_NULL = 0x80u
};

// ============================================================================
// [BLVariant - Core]
// ============================================================================

//! Variant [C Interface - Impl].
//!
//! Please note that this impl defines just the layout of any Value-based or
//! Object-based Impl. Members not defined by the layout can be used to store
//! any data.
struct BLVariantImpl {
  // IMPL HEADER
  // -----------
  //
  // [32-bit: 16 bytes]
  // [64-bit: 24 bytes]

  //! Union that provides either one `virt` table pointer or an arbitrary header
  //! if the object doesn't have virtual function table.
  union {
    //! Virtual function table (only available to impls with `BL_IMPL_TRAIT_VIRT`
    //! trait).
    const void* virt;
    //! Space reserved for object/value header other than virtual function table.
    //!
    //! This is always `capacity` when the Impl is a container, otherwise it's
    //! implementation dependent what this field is.
    uintptr_t unknownHeaderData;
  };

  //! Reference count.
  volatile size_t refCount;
  //! Impl type, see `BLImplType`.
  uint8_t implType;
  //! Traits of this impl, see `BLImplTraits`.
  uint8_t implTraits;
  //! Memory pool data, zero if not mem-pooled.
  uint16_t memPoolData;

  // IMPL BODY
  // ---------

  //! Reserved data (padding) that is free to be used by the Impl.
  uint8_t reserved[4];
};

//! Variant [C Interface - Core].
struct BLVariantCore {
  BLVariantImpl* impl;
};

#ifdef __cplusplus
extern "C" {
#endif

//! Built-in none objects indexed by `BLImplType`
extern BL_API BLVariantCore blNone[BL_IMPL_TYPE_COUNT];

#ifdef __cplusplus
} // {Extern:C}
#endif

// ============================================================================
// [BLVariant - C++]
// ============================================================================

#ifdef __cplusplus
//! Variant [C++ API].
//!
//! `BLVariant` defines a common interface that can be used to work with both
//! Blend2D values and objects in an abstract way without knowing their type.
//! Since both objects and values share the same common strucutre it's possible
//! to treat them as same at the lowest level (memory and lifetime management).
class BLVariant : public BLVariantCore {
public:
  BL_INLINE BLVariant() noexcept { this->impl = none().impl; }
  BL_INLINE BLVariant(BLVariant&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLVariant(const BLVariant& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLVariant(BLVariantImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLVariant() noexcept { blVariantDestroy(this); }

  BL_INLINE BLVariant& operator=(BLVariant&& other) noexcept { blVariantAssignMove(this, &other); return *this; }
  BL_INLINE BLVariant& operator=(const BLVariant& other) noexcept { blVariantAssignWeak(this, &other); return *this; }

  //! Tests whether the variant is a built-in null instance (of any impl-type).
  BL_NODISCARD
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }

  //! Returns the type of the object, see `BLImplType` for more details.
  BL_NODISCARD
  BL_INLINE uint32_t implType() const noexcept { return impl->implType; }

  BL_INLINE BLResult reset() noexcept { return blVariantReset(this); }

  BL_INLINE void swap(BLVariant& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLVariant&& other) noexcept { return blVariantAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLVariant& other) noexcept { return blVariantAssignWeak(this, &other); }

  BL_NODISCARD
  BL_INLINE bool equals(const BLVariant& other) const noexcept { return blVariantEquals(this, &other); }

  BL_NODISCARD
  static BL_INLINE const BLVariant& none() noexcept { return reinterpret_cast<const BLVariant*>(blNone)[BL_IMPL_TYPE_NULL]; }
};
#endif

//! \}

#endif // BLEND2D_VARIANT_H_INCLUDED
