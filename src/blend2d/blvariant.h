// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLVARIANT_H
#define BLEND2D_BLVARIANT_H

#include "./blapi.h"

//! \addtogroup blend2d_api_globals
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Impl type identifier used by to describe a Blend2D Impl.
BL_DEFINE_ENUM(BLImplType) {
  //! Type is `Null`.
  BL_IMPL_TYPE_NULL = 0,
  //! Type is `BLBitArray`.
  BL_IMPL_TYPE_BIT_ARRAY = 1,
  //! Type is `BLString`.
  BL_IMPL_TYPE_STRING = 2,
  //! Type is `BLArray<T>` where `T` is `BLVariant` or other ref-counted type.
  BL_IMPL_TYPE_ARRAY_VAR = 3,
  //! Type is `BLArray<T>` where `T` matches 8-bit signed integral type.
  BL_IMPL_TYPE_ARRAY_I8 = 4,
  //! Type is `BLArray<T>` where `T` matches 8-bit unsigned integral type.
  BL_IMPL_TYPE_ARRAY_U8 = 5,
  //! Type is `BLArray<T>` where `T` matches 16-bit signed integral type.
  BL_IMPL_TYPE_ARRAY_I16 = 6,
  //! Type is `BLArray<T>` where `T` matches 16-bit unsigned integral type.
  BL_IMPL_TYPE_ARRAY_U16 = 7,
  //! Type is `BLArray<T>` where `T` matches 32-bit signed integral type.
  BL_IMPL_TYPE_ARRAY_I32 = 8,
  //! Type is `BLArray<T>` where `T` matches 32-bit unsigned integral type.
  BL_IMPL_TYPE_ARRAY_U32 = 9,
  //! Type is `BLArray<T>` where `T` matches 64-bit signed integral type.
  BL_IMPL_TYPE_ARRAY_I64 = 10,
  //! Type is `BLArray<T>` where `T` matches 64-bit unsigned integral type.
  BL_IMPL_TYPE_ARRAY_U64 = 11,
  //! Type is `BLArray<T>` where `T` matches 32-bit floating point type.
  BL_IMPL_TYPE_ARRAY_F32 = 12,
  //! Type is `BLArray<T>` where `T` matches 64-bit floating point type.
  BL_IMPL_TYPE_ARRAY_F64 = 13,
  //! Type is `BLArray<T>` where `T` is a struct of size 1.
  BL_IMPL_TYPE_ARRAY_STRUCT_1 = 14,
  //! Type is `BLArray<T>` where `T` is a struct of size 2.
  BL_IMPL_TYPE_ARRAY_STRUCT_2 = 15,
  //! Type is `BLArray<T>` where `T` is a struct of size 3.
  BL_IMPL_TYPE_ARRAY_STRUCT_3 = 16,
  //! Type is `BLArray<T>` where `T` is a struct of size 4.
  BL_IMPL_TYPE_ARRAY_STRUCT_4 = 17,
  //! Type is `BLArray<T>` where `T` is a struct of size 6.
  BL_IMPL_TYPE_ARRAY_STRUCT_6 = 18,
  //! Type is `BLArray<T>` where `T` is a struct of size 8.
  BL_IMPL_TYPE_ARRAY_STRUCT_8 = 19,
  //! Type is `BLArray<T>` where `T` is a struct of size 10.
  BL_IMPL_TYPE_ARRAY_STRUCT_10 = 20,
  //! Type is `BLArray<T>` where `T` is a struct of size 12.
  BL_IMPL_TYPE_ARRAY_STRUCT_12 = 21,
  //! Type is `BLArray<T>` where `T` is a struct of size 16.
  BL_IMPL_TYPE_ARRAY_STRUCT_16 = 22,
  //! Type is `BLArray<T>` where `T` is a struct of size 20.
  BL_IMPL_TYPE_ARRAY_STRUCT_20 = 23,
  //! Type is `BLArray<T>` where `T` is a struct of size 24.
  BL_IMPL_TYPE_ARRAY_STRUCT_24 = 24,
  //! Type is `BLArray<T>` where `T` is a struct of size 32.
  BL_IMPL_TYPE_ARRAY_STRUCT_32 = 25,
  //! Type is `BLPath`.
  BL_IMPL_TYPE_PATH2D = 32,
  //! Type is `BLRegion`.
  BL_IMPL_TYPE_REGION = 33,
  //! Type is `BLImage`.
  BL_IMPL_TYPE_IMAGE = 34,
  //! Type is `BLImageCodec`.
  BL_IMPL_TYPE_IMAGE_CODEC = 35,
  //! Type is `BLImageDecoder`.
  BL_IMPL_TYPE_IMAGE_DECODER = 36,
  //! Type is `BLImageEncoder`.
  BL_IMPL_TYPE_IMAGE_ENCODER = 37,
  //! Type is `BLGradient`.
  BL_IMPL_TYPE_GRADIENT = 38,
  //! Type is `BLPattern`.
  BL_IMPL_TYPE_PATTERN = 39,
  //! Type is `BLContext`.
  BL_IMPL_TYPE_CONTEXT = 40,
  //! Type is `BLFont`.
  BL_IMPL_TYPE_FONT = 50,
  //! Type is `BLFontFace`.
  BL_IMPL_TYPE_FONT_FACE = 51,
  //! Type is `BLFontData`.
  BL_IMPL_TYPE_FONT_DATA = 52,
  //! Type is `BLFontLoader`.
  BL_IMPL_TYPE_FONT_LOADER = 53,
  //! Type is `BLFontFeatureOptions`.
  BL_IMPL_TYPE_FONT_FEATURE_OPTIONS = 54,
  //! Type is `BLFontVariationOptions`.
  BL_IMPL_TYPE_FONT_VARIATION_OPTIONS = 55,

  //! Count of type identifiers including all reserved ones.
  BL_IMPL_TYPE_COUNT = 64
};

//! Impl traits that describe some details about a Blend2D `Impl` data.
BL_DEFINE_ENUM(BLImplTraits) {
  //! Set if the impl is a built-in null instance (default constructed).
  BL_IMPL_TRAIT_NULL = 0x01u,
  //! Set if the impl provides a virtual function table (first member).
  BL_IMPL_TRAIT_VIRT = 0x02u,
  //! Set if the impl uses an external data (data is not part of impl).
  BL_IMPL_TRAIT_EXTERNAL = 0x10u,
  //! Set if the impl was not allocated by `blRuntimeAllocImpl()`.
  BL_IMPL_TRAIT_FOREIGN = 0x80u
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
  // [32-bit: 12 bytes]
  // [64-bit: 24 bytes]

  //! Union that provides either one `virt` table pointer and two reserved
  //! fields at index [1] and [2] in case of object or 3 reserved fields in
  //! case of value.
  union {
    //! Virtual function table (only available to impls with BL_IMPL_TRAIT_VIRT trait).
    const void* virt;
    //! Space reserved for object/value header (must be array-view if the impl is container).
    uintptr_t header[3];
  };

  // IMPL COMMON
  // -----------
  //
  // [32-bit: 8  bytes]
  // [64-bit: 12 bytes]

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

  //! Reserved data, free to be used by the impl (padding for us).
  uint8_t reserved[4];
};

//! Variant [C Interface - Core].
struct BLVariantCore {
  BLVariantImpl* impl;
};

//! Built-in none objects indexed by `BLImplType`
BL_API_C BLVariantCore blNone[BL_IMPL_TYPE_COUNT];

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
  BL_INLINE ~BLVariant() noexcept { blVariantReset(this); }

  BL_INLINE BLVariant& operator=(BLVariant&& other) noexcept { blVariantAssignMove(this, &other); return *this; }
  BL_INLINE BLVariant& operator=(const BLVariant& other) noexcept { blVariantAssignWeak(this, &other); return *this; }

  //! Get whether the variant is a built-in null instance (of any impl-type).
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }

  BL_INLINE BLResult reset() noexcept { return blVariantReset(this); }

  BL_INLINE void swap(BLVariant& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLVariant&& other) noexcept { return blVariantAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLVariant& other) noexcept { return blVariantAssignWeak(this, &other); }

  BL_INLINE bool equals(const BLVariant& other) const noexcept { return blVariantEquals(this, &other); }

  static BL_INLINE const BLVariant& none() noexcept { return reinterpret_cast<const BLVariant*>(blNone)[BL_IMPL_TYPE_NULL]; }
};
#endif

//! \}

#endif // BLEND2D_BLVARIANT_H
