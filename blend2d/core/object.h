// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_OBJECT_H_INCLUDED
#define BLEND2D_CORE_OBJECT_H_INCLUDED

#include <blend2d/core/api.h>
#include <blend2d/core/rgba.h>

//! \defgroup bl_object Object Model
//! \brief Object Model & Memory Layout
//!
//! Blend2D object model is a foundation of all Blend2D objects. It was designed only for Blend2D and it's not
//! supposed to be used as a foundation of other libraries. The object model provides runtime reflection, small
//! size optimization (SSO), and good performance. In general, it focuses on optimizing memory footprint by taking
//! advantage of SSO storage and to exploit SSO storage also in cases in which dynamic Impl objects are necessary.
//!
//! Blend2D object model used by \ref BLObjectCore consists of 32 bytes that have the following layout:
//!
//! ```
//! union BLObjectDetail {
//!   char _sso_data[32];
//!
//!   ... [other data views] ...
//!
//!   struct {
//!     const BLObjectVirt* virt;
//!     BLObjectImpl* impl;
//!
//!     ... [padding] ...
//!
//!     uint8_t type; // Object type 'tttttt' combined with 'Z' and 'V' flags, which form 'ZVtttttt' byte.
//!     uint8_t meta; // Object flags combined with 'aaaaa' field, which form 'DRIXaaaa' byte.
//!   };
//! };
//! ```
//!
//! Which allows to have either SSO or dynamic instances:
//!
//!   - SSO instance stores all data in object detail itself - `impl` is not a valid pointer and cannot be dereferenced.
//!   - Dynamic instance has a valid `impl` pointer having a content, which type depends on \ref BLObjectType.
//!
//! The layout was designed to provide the following properties:
//!
//!   - Reflection - any Blend2D object can be casted to a generic \ref BLObjectCore or \ref BLVarCore and inspected
//!     at runtime. Variants `BLVarCore` have no runtime cost - they are binary compatible with BLObject-derived types.
//!   - SSO and primitive value optimizations save memory allocations of \ref BLString, \ref BLArray, etc...
//!   - No atomic reference counting operations for small containers and default constructed objects without data.
//!
//! Meta Fields:
//!
//!   - 'D' - Dynamic flag - when set the Impl pointer is valid.
//!           When 'D' == '0' it means the object is in SSO mode, when 'D' == '1' it means it's in Dynamic mode (has a valid Impl).
//!   - 'R' - Ref counted flag - can only be set when 'D' flag is set as well, otherwise it must remain '0'.
//!   - 'I' - Used to mean 'immutable', but currently it's not used and it's reserved in Dynamic mode and used as SSO payload bit.
//!   - 'X' - External data flag - only has meaning when 'D' flag is set, otherwise it can be used as SSO payload bit.
//!   - 'a' - If the object is a container (BLArray, BLString) 'a' field always represents its size in SSO mode. In this case
//!           it's accessed as a whole BYTE because both 'R' and 'D' bits would be zero and 'I' and 'X' bits are part of SSO
//!           payload, so no masking is required to get SSO size in ALL cases.
//!
//! If the 'D' flag is '1' the whole byte cannot be touched by the object itself. The following payload fields are used by the
//! implementation:
//!
//!   - 'X' - External data flag - the object holds external data.
//!   - 'a' - Allocation adjustment - at the moment the field describes how many bytes (shifted) to subtract from 'impl' to get a
//!           real pointer returned by the underlying memory allocator. Object deallocation relies on this to compute the correct
//!           pointer to pass to `free()` or other function that is responsible for freeing memory.
//!
//! Type Fields:
//!
//!   - 'Z' - Zero bit - this bit is never set to '1' by Blend2D for an instance that is supposed to be alive. However, Blend2D is
//!           free to set this bit to '1' when an instance is destroyed or when an instance hasn't been created yet (for example in
//!           case there is a lazy-initialization somewhere).
//!   - 'V' - Virtual flag - can only be set when 'D' flag is set as well.
//!   - 't' - Object type bits - see \ref BLObjectType for more details. Object type should always be read as 'ZVtttttt', which forms
//!           an 8-bit quantity (BYTE), which can be read directly from memory without any need to further process it to get a valid
//!           `BLObjectType` - and this is exactly what Blend2D does, because 'Z' is guaranteed to be zero for valid instances and 'V'
//!           is part of the type itself.
//!
//! Not all classes use all defined flags, here is a little overview:
//!
//! ```
//! +--------------------------+---+---+---+---+---+---+
//! | Type                     |SSO|Dyn|Ext|Imm|Vrt|Ref|
//! +--------------------------+---+---+---+---+---+---+
//! | BLVar {Null}             | 1 | 0 | 0 | 0 | 0 | 0 |
//! | BLVar {Bool}             | 1 | 0 | 0 | 0 | 0 | 0 | 'SSO' - General SSO support (optimized when small)
//! | BLVar {Int64}            | 1 | 0 | 0 | 0 | 0 | 0 | 'Dyn' - Dynamic Impl support.
//! | BLVar {UInt64}           | 1 | 0 | 0 | 0 | 0 | 0 | 'Ext' - External data support.
//! | BLVar {Double}           | 1 | 0 | 0 | 0 | 0 | 0 | 'Imm' - Immutable data support.
//! | BLVar {BLRgba}           | 1 | 0 | 0 | 0 | 0 | 0 | 'Vrt' - Object provides virtual function table.
//! | BLVar {BLRgba32}         | 1 | 0 | 0 | 0 | 0 | 0 | 'Ref' - Reference counting support.
//! | BLVar {BLRgba64}         | 1 | 0 | 0 | 0 | 0 | 0 |
//! | BLArray<T>               | x | x | x | x | 0 | x | '0'   - Never used
//! | BLBitArray               | x | x | 0 | 0 | 0 | x | '1'   - Always used
//! | BLContext                | 0 | 1 | 0 | 0 | 1 | x |
//! | BLString                 | x | x | 0 | 0 | 0 | x |
//! | BLPattern                | 0 | 1 | 0 | 0 | 0 | x |
//! | BLGradient               | 0 | 1 | 0 | 0 | 0 | x |
//! | BLPath                   | 0 | 1 | 0 | x | 0 | x |
//! | BLImage                  | 0 | 1 | x | x | 0 | x |
//! | BLImageCodec             | 0 | 1 | 0 | x | 1 | x |
//! | BLImageDecoder           | 0 | 1 | 0 | 0 | 1 | x |
//! | BLImageEncoder           | 0 | 1 | 0 | 0 | 1 | x |
//! | BLFont                   | 0 | 1 | 0 | 0 | 0 | x |
//! | BLFontFace               | 0 | 1 | 0 | x | 1 | x |
//! | BLFontData               | 0 | 1 | x | x | 1 | x |
//! | BLFontManager            | 0 | 1 | 0 | x | 1 | x |
//! | BLFontFeatureSettings    | 1 | 1 | 0 | 0 | 0 | x |
//! | BLFontVariationSettings  | 1 | 1 | 0 | 0 | 0 | x |
//! | BLCoverageSet        | 0 | 1 | 0 | x | 0 | x |
//! +--------------------------+---+---+---+---+---+---+
//! ```

//! \addtogroup bl_object
//! \{

//! \name BLObject - Constants
//! \{

//! Object type identifier.
BL_DEFINE_ENUM(BLObjectType) {
  //! Object is \ref BLString.
  BL_OBJECT_TYPE_STRING = 0,
  //! Object is \ref BLArray<T> where `T` matches 8-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT8 = 1,
  //! Object is \ref BLArray<T> where `T` matches 8-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT8 = 2,
  //! Object is \ref BLArray<T> where `T` matches 16-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT16 = 3,
  //! Object is \ref BLArray<T> where `T` matches 16-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT16 = 4,
  //! Object is \ref BLArray<T> where `T` matches 32-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT32 = 5,
  //! Object is \ref BLArray<T> where `T` matches 32-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT32 = 6,
  //! Object is \ref BLArray<T> where `T` matches 64-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT64 = 7,
  //! Object is \ref BLArray<T> where `T` matches 64-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT64 = 8,
  //! Object is \ref BLArray<T> where `T` matches 32-bit floating point type.
  BL_OBJECT_TYPE_ARRAY_FLOAT32 = 9,
  //! Object is \ref BLArray<T> where `T` matches 64-bit floating point type.
  BL_OBJECT_TYPE_ARRAY_FLOAT64 = 10,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 1.
  BL_OBJECT_TYPE_ARRAY_STRUCT_1 = 11,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 2.
  BL_OBJECT_TYPE_ARRAY_STRUCT_2 = 12,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 3.
  BL_OBJECT_TYPE_ARRAY_STRUCT_3 = 13,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 4.
  BL_OBJECT_TYPE_ARRAY_STRUCT_4 = 14,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 6.
  BL_OBJECT_TYPE_ARRAY_STRUCT_6 = 15,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 8.
  BL_OBJECT_TYPE_ARRAY_STRUCT_8 = 16,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 10.
  BL_OBJECT_TYPE_ARRAY_STRUCT_10 = 17,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 12.
  BL_OBJECT_TYPE_ARRAY_STRUCT_12 = 18,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 16.
  BL_OBJECT_TYPE_ARRAY_STRUCT_16 = 19,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 20.
  BL_OBJECT_TYPE_ARRAY_STRUCT_20 = 20,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 24.
  BL_OBJECT_TYPE_ARRAY_STRUCT_24 = 21,
  //! Object is \ref BLArray<T> where `T` is a trivially copyable struct of size 32.
  BL_OBJECT_TYPE_ARRAY_STRUCT_32 = 22,
  //! Object is \ref BLArray<T> where `T` is a BLObject-compatible type.
  BL_OBJECT_TYPE_ARRAY_OBJECT = 23,

  //! Object is `Null` (can be used as style).
  BL_OBJECT_TYPE_NULL = 32,
  //! Object represents a \ref BLRgba32 value stored as 32-bit integer in `0xAARRGGBB` form.
  BL_OBJECT_TYPE_RGBA32 = 33,
  //! Object represents a \ref BLRgba64 value stored as 64-bit integer in `0xAAAARRRRGGGGBBBB` form.
  BL_OBJECT_TYPE_RGBA64 = 34,
  //! Object represents a \ref BLRgba value stored as four 32-bit floating point components (can be used as Style).
  BL_OBJECT_TYPE_RGBA = 35,
  //! Object is \ref BLGradient (can be used as style).
  BL_OBJECT_TYPE_GRADIENT = 36,
  //! Object is \ref BLPattern (can be used as style).
  BL_OBJECT_TYPE_PATTERN = 37,

  //! Object represents a boolean value.
  BL_OBJECT_TYPE_BOOL = 40,
  //! Object represents a 64-bit signed integer value.
  BL_OBJECT_TYPE_INT64 = 41,
  //! Object represents a 64-bit unsigned integer value.
  BL_OBJECT_TYPE_UINT64 = 42,
  //! Object represents a 64-bit floating point value.
  BL_OBJECT_TYPE_FLOAT64 = 43,

  //! Object is \ref BLImage.
  BL_OBJECT_TYPE_IMAGE = 45,

  //! Object is \ref BLFont.
  BL_OBJECT_TYPE_FONT = 46,
  //! Object is \ref BLFontFeatureSettings.
  BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS = 47,
  //! Object is \ref BLFontVariationSettings.
  BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS = 48,
  //! Object is \ref BLGlyphBuffer.
  BL_OBJECT_TYPE_GLYPH_BUFFER = 50,

  //! Object is \ref BLBitArray.
  BL_OBJECT_TYPE_BIT_ARRAY = 57,
  //! Object is \ref BLCoverageSet.
  BL_OBJECT_TYPE_COVERAGE_SET = 59,

  //! Object is \ref BLPath.
  BL_OBJECT_TYPE_PATH = 62,

  //! Object is \ref BLStrokeOptions.
  BL_OBJECT_TYPE_STROKE_OPTIONS = 63,

  //! Object is \ref BLContext.
  BL_OBJECT_TYPE_CONTEXT = 64,

  //! Object is \ref BLPathEffect.
  BL_OBJECT_TYPE_PATH_EFFECT = 65,

  //! Object is \ref BLImageCodec.
  BL_OBJECT_TYPE_IMAGE_CODEC = 66,
  //! Object is \ref BLImageDecoder.
  BL_OBJECT_TYPE_IMAGE_DECODER = 67,
  //! Object is \ref BLImageEncoder.
  BL_OBJECT_TYPE_IMAGE_ENCODER = 68,

  //! Object is \ref BLFontData.
  BL_OBJECT_TYPE_FONT_DATA = 69,
  //! Object is \ref BLFontFace.
  BL_OBJECT_TYPE_FONT_FACE = 70,
  //! Object is \ref BLFontManager.
  BL_OBJECT_TYPE_FONT_MANAGER = 71,

  //! Minimum object type identifier of an array object.
  BL_OBJECT_TYPE_MIN_ARRAY = 1,
  //! Maximum object type identifier of an array object.
  BL_OBJECT_TYPE_MAX_ARRAY = 23,

  //! Minimum object type identifier that can be used as a style.
  BL_OBJECT_TYPE_MIN_STYLE = 32,
  //! Maximum object type identifier that can be used as a style.
  BL_OBJECT_TYPE_MAX_STYLE = 37,

  //! Minimum object type identifier of an object with virtual function table.
  BL_OBJECT_TYPE_MIN_VIRTUAL = 64,
  //! Maximum object type identifier of an object with virtual function table.
  BL_OBJECT_TYPE_MAX_VIRTUAL = 127,

  //! Maximum possible value of an object type, including identifiers reserved for the future.
  BL_OBJECT_TYPE_MAX_VALUE = 127,

  //! Unknown object type - this is not a valid object type, it's only used statically in some places.
  BL_OBJECT_TYPE_UNKNOWN = 0x80000000u

  BL_FORCE_ENUM_UINT32(BL_OBJECT_TYPE)
};

//! \}

//! \name BLObject - Detail
//! \{

//! BLObject [Dynamic Data].
//!
//! This is only used to model inheritance when compiled by a C++ compiler.
#ifdef __cplusplus
struct BLObjectImpl {
  //! \name Common Functionality
  //! \{

  //! Casts this Impl to `T*`.
  template<typename T>
  [[nodiscard]]
  BL_INLINE_NODEBUG T* as() noexcept { return static_cast<T*>(this); }

  //! Casts this Impl to `T*` (const).
  template<typename T>
  [[nodiscard]]
  BL_INLINE_NODEBUG const T* as() const noexcept { return static_cast<const T*>(this); }

  //! \}
};
#endif

//! Base members of \ref BLObjectVirt.
//!
//! The reason for this struct is to make C API the same as C++ API in terms of struct members. In C++ mode we use
//! inheritance so `Virt` structs actually inherit from \ref BLObjectVirt, but in every case all base members are
//! provided by `base`.
struct BLObjectVirtBase {
  BLResult (BL_CDECL* destroy)(BLObjectCore* self) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* get_property)(const BLObjectCore* self, const char* name, size_t name_size, BLVarCore* value_out) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* set_property)(BLObjectCore* self, const char* name, size_t name_size, const BLVarCore* value) BL_NOEXCEPT_C;
};

//! BLObject [Virtual Function Table].
//!
//! Virtual function table is only present when object type is greater than \ref BL_OBJECT_TYPE_MIN_VIRTUAL.
//! Objects can extend the function table, but it has to always start with members defined by `BLObjectVirt`.
struct BLObjectVirt {
  BLObjectVirtBase base;
};

//! Defines a BLObject layout that all objects must use.
union BL_ALIGN_TYPE(BLObjectDetail, 16) {
  uint8_t _sso_data[32];

  // TODO: Possibly only make available for debugging?
#if 0
  uint8_t _u8_data[32];
  int16_t _i16_data[16];
  uint16_t _u16_data[16];
  int32_t _i32_data[8];
  uint32_t _u32_data[8];
  int64_t _i64_data[4];
  uint64_t _u64_data[4];
  float _f32_data[8];
  double _f64_data[4];
  size_t _size_data[32u / sizeof(size_t)];
#endif

#ifdef __cplusplus
  //! \name Constants
  //! \{

  static constexpr size_t kPL0ByteIndex = 28u;
  static constexpr size_t kPL1ByteIndex = 29u;
  static constexpr size_t kTypeByteIndex = 30u;
  static constexpr size_t kMetaByteIndex = 31u;

  // 'ZV_tttttt' fields of `_sso_data[kTypeByteIndex]`.
  static constexpr uint8_t kTypeFlagZ = 0x80u;
  static constexpr uint8_t kTypeFlagV = 0x40u;

  // 'RD_a[X|a]aaaa' fields of `_sso_data[kMetaByteIndex]`.
  static constexpr uint8_t kMetaFlagD = 0x80u;
  static constexpr uint8_t kMetaFlagR = 0x40u;
  static constexpr uint8_t kMetaFlagX = 0x10u;
  static constexpr uint8_t kMetaFlagDR = uint8_t(kMetaFlagD | kMetaFlagR);
  static constexpr uint8_t kMetaFlagDX = uint8_t(kMetaFlagD | kMetaFlagX);
  static constexpr uint8_t kMetaFlagDRX = uint8_t(kMetaFlagD | kMetaFlagR | kMetaFlagX);

  //! Size of object static storage not considering the last 2 bytes - `meta_byte` and `type_byte`.
  static constexpr size_t kSSOCapacity = 32u - 2u;

  //! \}

  //! \name Types
  //! \{

  template<typename T>
  struct Ptr64 {
    T* ptr;

#if BL_TARGET_ARCH_BITS < 64
    //! Padding to ensure that `ptr` has always the same size regardless of bit-width of the target architecture.
    //!
    //! \remarks This field should be zero.
    uint32_t padding;
#endif
  };

  struct Dyn {
    //! Data or virtual function table pointer.
    //!
    //! \remarks This member always start at offset #0 of BLObjectDetail regardless of the target pointer width.
    Ptr64<void> data_or_virt;

    //! Impl pointer.
    //!
    //! \remarks This member always starts at offset #8 of BLObjectDetail regardless of the target pointer width. In case
    //! that the dynamic Impl is trivial and the object is a contiguous container `_impl` would have the same address as
    //! `_data_or_virt`.
    Ptr64<BLObjectImpl> impl;

#if BL_TARGET_ARCH_BITS < 64
    union {
      size_t size_u32;
      void* extra_ptr;
    };
    size_t capacity_u32;
    uint32_t payload_u32;
    uint16_t payload_u16;
    uint8_t type_byte;
    uint8_t meta_byte;
#else
    union {
      uint64_t size_u64;
      void* extra_ptr;
      struct {
        uint32_t size_u32;
        uint32_t capacity_u32;
      };
    };

    union {
      uint64_t capacity_u48;
      struct {
        uint32_t payload_u32;
        uint16_t payload_u16;
        uint8_t type_byte;
        uint8_t meta_byte;
      };
    };
#endif
  } dyn;

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the given objects are binary equivalent.
  //!
  //! Binary equality is used by some equality implementations as a quick check. This can be used by both SSO and
  //! Dynamic instances.
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const BLObjectDetail& other) const noexcept {
    return memcmp(_sso_data, other._sso_data, sizeof(BLObjectDetail)) == 0;
  }

  [[nodiscard]] BL_INLINE_NODEBUG bool operator==(const BLObjectDetail& other) const noexcept { return equals(other); }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator!=(const BLObjectDetail& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Accessors
  //! \{

  [[nodiscard]] BL_INLINE_CONSTEXPR uint8_t pl0_byte() const noexcept { return _sso_data[kPL0ByteIndex]; }
  [[nodiscard]] BL_INLINE_CONSTEXPR uint8_t pl1_byte() const noexcept { return _sso_data[kPL1ByteIndex]; }
  [[nodiscard]] BL_INLINE_CONSTEXPR uint8_t type_byte() const noexcept { return _sso_data[kTypeByteIndex]; }
  [[nodiscard]] BL_INLINE_CONSTEXPR uint8_t meta_byte() const noexcept { return _sso_data[kMetaByteIndex]; }

  BL_INLINE_CONSTEXPR void set_pl0_byte(uint8_t v) noexcept { _sso_data[kPL0ByteIndex] = v; }
  BL_INLINE_CONSTEXPR void set_pl1_byte(uint8_t v) noexcept { _sso_data[kPL1ByteIndex] = v; }
  BL_INLINE_CONSTEXPR void set_type_byte(uint8_t v) noexcept { _sso_data[kTypeByteIndex] = v; }
  BL_INLINE_CONSTEXPR void set_meta_byte(uint8_t v) noexcept { _sso_data[kMetaByteIndex] = v; }

  //! Tests whether the object-info represents an instance that only uses static storage (SSO).
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_sso() const noexcept { return (meta_byte() & kMetaFlagD) == 0u; }
  //! Tests whether the object-info represents an instance that uses dynamic storage (the same as `!is_sso()`).
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_dynamic() const noexcept { return (meta_byte() & kMetaFlagD) != 0u; }
  //! Tests whether the object info represents a dynamic and reference-counted instance.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_external() const noexcept { return (meta_byte() & kMetaFlagDX) == kMetaFlagDX; }
  //! Tests whether the object info represents a dynamic and reference-counted instance.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_ref_counted() const noexcept { return (meta_byte() & kMetaFlagR) != 0u; }
  //! Tests whether the object info represents a valid BLObject, which has a valid Impl, and has a virtual function table.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_virtual() const noexcept { return (type_byte() & kTypeFlagV) != 0u; }

  //! Returns a RAW type read from object detail data.
  [[nodiscard]] BL_INLINE_CONSTEXPR BLObjectType type() const noexcept { return BLObjectType(type_byte()); }
  //! Tests whether the object detail represents a `BLArray<T>` storing any supported type.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_type_between(BLObjectType a, BLObjectType b) const noexcept { return type() >= a && type() <= b; }

  //! Tests whether the object detail represents `BLString`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_string() const noexcept { return type() == BL_OBJECT_TYPE_STRING; }
  //! Tests whether the object detail represents a `BLBitArray`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_bit_array() const noexcept { return type() == BL_OBJECT_TYPE_BIT_ARRAY; }

  //! Tests whether the object detail represents a `BLArray<int8_t>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_i8() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_INT8; }
  //! Tests whether the object detail represents a `BLArray<uint8_t>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_u8() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_UINT8; }
  //! Tests whether the object detail represents a `BLArray<int16_t>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_i16() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_INT16; }
  //! Tests whether the object detail represents a `BLArray<uint16_t>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_u16() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_UINT16; }
  //! Tests whether the object detail represents a `BLArray<int32_t>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_i32() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_INT32; }
  //! Tests whether the object detail represents a `BLArray<uint32_t>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_u32() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_UINT32; }
  //! Tests whether the object detail represents a `BLArray<int64_t>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_i64() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_INT64; }
  //! Tests whether the object detail represents a `BLArray<uint64_t>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_u64() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_UINT64; }
  //! Tests whether the object detail represents a `BLArray<float>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_f32() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_FLOAT32; }
  //! Tests whether the object detail represents a `BLArray<double>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_f64() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_FLOAT64; }
  //! Tests whether the object detail represents a `BLArray<BLObject|BLVar>`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array_obj() const noexcept { return type() == BL_OBJECT_TYPE_ARRAY_OBJECT; }

  //! Tests whether the object detail represents a boxed `bool` value.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_bool() const noexcept { return type() == BL_OBJECT_TYPE_BOOL; }
  //! Tests whether the object detail represents a boxed `int64_t` value.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_int64() const noexcept { return type() == BL_OBJECT_TYPE_INT64; }
  //! Tests whether the object detail represents a boxed `uint64_t` value.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_uint64() const noexcept { return type() == BL_OBJECT_TYPE_UINT64; }
  //! Tests whether the object detail represents a boxed `double` value.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_float64() const noexcept { return type() == BL_OBJECT_TYPE_FLOAT64; }

  //! Tests whether the object detail represents `BLFont`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_font() const noexcept { return type() == BL_OBJECT_TYPE_FONT; }
  //! Tests whether the object detail represents `BLFontFeatureSettings`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_font_feature_settings() const noexcept { return type() == BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS; }
  //! Tests whether the object detail represents `BLFontVariationSettings`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_font_variation_settings() const noexcept { return type() == BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS; }
  //! Tests whether the object detail represents `BLGlyphBuffer`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_glyph_buffer() const noexcept { return type() == BL_OBJECT_TYPE_GLYPH_BUFFER; }
  //! Tests whether the object detail represents a `BLCoverageSet`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_coverage_set() const noexcept { return type() == BL_OBJECT_TYPE_COVERAGE_SET; }

  //! Tests whether the object detail represents a null value.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_null() const noexcept { return type() == BL_OBJECT_TYPE_NULL; }
  //! Tests whether the object detail represents `BLRgba32`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_rgba32() const noexcept { return type() == BL_OBJECT_TYPE_RGBA32; }
  //! Tests whether the object detail represents `BLRgba64`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_rgba64() const noexcept { return type() == BL_OBJECT_TYPE_RGBA64; }
  //! Tests whether the object detail represents `BLRgba`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_rgba() const noexcept { return type() == BL_OBJECT_TYPE_RGBA; }
  //! Tests whether the object detail represents `BLGradient`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_gradient() const noexcept { return type() == BL_OBJECT_TYPE_GRADIENT; }
  //! Tests whether the object detail represents `BLPattern.`
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_pattern() const noexcept { return type() == BL_OBJECT_TYPE_PATTERN; }

  //! Tests whether the object detail represents `BLImage`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_image() const noexcept { return type() == BL_OBJECT_TYPE_IMAGE; }

  //! Tests whether the object detail represents `BLPath`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_path() const noexcept { return type() == BL_OBJECT_TYPE_PATH; }
  //! Tests whether the object detail represents `BLPathEffect`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_path_effect() const noexcept { return type() == BL_OBJECT_TYPE_PATH_EFFECT; }
  //! Tests whether the object detail represents `BLStrokeOptions`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_stroke_options() const noexcept { return type() == BL_OBJECT_TYPE_STROKE_OPTIONS; }

  //! Tests whether the object detail represents `BLContext`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_context() const noexcept { return type() == BL_OBJECT_TYPE_CONTEXT; }
  //! Tests whether the object detail represents `BLImageCodec`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_image_codec() const noexcept { return type() == BL_OBJECT_TYPE_IMAGE_CODEC; }
  //! Tests whether the object detail represents `BLImageDecoder`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_image_decoder() const noexcept { return type() == BL_OBJECT_TYPE_IMAGE_DECODER; }
  //! Tests whether the object detail represents `BLImageEncoder`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_image_encoder() const noexcept { return type() == BL_OBJECT_TYPE_IMAGE_ENCODER; }
  //! Tests whether the object detail represents `BLFontData`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_font_data() const noexcept { return type() == BL_OBJECT_TYPE_FONT_DATA; }
  //! Tests whether the object detail represents `BLFontFace`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_font_face() const noexcept { return type() == BL_OBJECT_TYPE_FONT_FACE; }
  //! Tests whether the object detail represents `BLFontManager`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_font_manager() const noexcept { return type() == BL_OBJECT_TYPE_FONT_MANAGER; }

  //! Tests whether the object detail represents a `BLArray<T>` storing any supported type.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_array() const noexcept { return is_type_between(BL_OBJECT_TYPE_MIN_ARRAY, BL_OBJECT_TYPE_MAX_ARRAY); }
  //! Tests whether the object detail represents a style that can be passed to the rendering context.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_style() const noexcept { return is_type_between(BL_OBJECT_TYPE_MIN_STYLE, BL_OBJECT_TYPE_MAX_STYLE); }

  template<typename T>
  [[nodiscard]] BL_INLINE_NODEBUG T& layout() noexcept { return *static_cast<T*>(static_cast<void*>(_sso_data)); }

  template<typename T>
  [[nodiscard]] BL_INLINE_NODEBUG std::add_const_t<T>& layout() const noexcept { return *static_cast<std::add_const_t<T>*>(static_cast<const void*>(_sso_data)); }

  template<typename T = BLObjectImpl>
  [[nodiscard]] BL_INLINE_NODEBUG T* impl() const noexcept { return static_cast<T*>(dyn.impl.ptr); }

  template<typename T = BLObjectVirt>
  [[nodiscard]] BL_INLINE_NODEBUG const T* virt() const noexcept { return static_cast<const T*>(dyn.data_or_virt.ptr); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Swaps this BLObjectDetail with `other`.
  BL_INLINE_NODEBUG void swap(BLObjectDetail& other) noexcept {
    BLObjectDetail tmp = *this;
    *this = other;
    other = tmp;
  }

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE_NODEBUG void zero_all() noexcept { memset(_sso_data, 0, sizeof(_sso_data)); }
  BL_INLINE_NODEBUG void zero_storage() noexcept { memset(_sso_data, 0, kSSOCapacity); }

  //! \}

#endif
};

#ifdef __cplusplus
static_assert(sizeof(BLObjectDetail) == 32, "BLObjectDetail must be exactly 32 bytes long");
#endif

//! \}

//! \name BLObject - Object Layer
//! \{

#ifdef __cplusplus
namespace bl::ObjectLayer {

//! Object mode describes variations the object could use (SSO vs Dynamic).
enum class ObjectMode : uint8_t {
  //! Object is always in SSO mode.
  kSSO = 0,

  //! Object is in any mode.
  kAny,

  //! Object is always in dynamic mode (it's never SSO, even when default constructed).
  kDyn,

  //! Object is always reference counted.
  //!
  //! \remarks This is only internally used by the implementation in some cases - it's never stored in Object::Info.
  kRC
};

//! Object layout tag is used to define how the given object uses its 32-bytes inherited from BLObjectCore.
enum class ObjectLayout : uint8_t {
  //! The layout is not known or is special - in general this means a generic container API cannot be used.
  kUnknown = 0u,

  //! Native layout that uses `{data_ptr, impl, size, capacity}` fields where both `size` and `capacity` fields
  //! are of `uint64_t` type. However, `capacity` is represented by 48 bits in 64-bit mode due to using 2 last
  //! bytes by `BLObjectDetail` itself.
  //!
  //! \remarks This layout can only used on 64-bit targets, 32-bit targets always use Native32.
  kNative64 = 1,

  //! Native layout that uses `{data_ptr, impl, size, capacity}` fields where both `size` and `capacity` fields
  //! are of `uint32_t` type. This layout is typically used by containers where large size makes no sense (for
  //! example \ref BLGradient never needs to store large number of stops) so both `size` and `capacity` fields are
  //! traded for more space in the object itself (in 64-bit mode this layout saves 6 bytes in `BLObjectDetail`).
  kNative32 = 2,

  //! A layout similar to \ref Native32, however, the data member is omitted as it's aliased with `impl` member.
  //!
  //! \remarks This layout cannot be used by containers that can hold external data, because there is no `data`
  //! field. Data is considered to always be a part of the `impl` itself.
  kCompact32 = 3,

  //! A layout, which is only used by vector paths, which need two size components instead of 1. One size describes
  //! vertex data and the other control data. The size of the path is them the sum of the two.
  kPath = 4,

  //! Native layout that uses `{data_ptr, impl, size, capacity}` fields where both `size` and `capacity` fields
  //! are compatible with `size_t` type.
  kNative = sizeof(void*) >= 8 ? kNative64 : kNative32
};

template<BLObjectType type, ObjectMode mode, ObjectLayout layout = ObjectLayout::kUnknown>
struct ObjectInfo {
  static constexpr BLObjectType kType = type;
  static constexpr ObjectMode kMode = mode;
  static constexpr ObjectLayout kLayout = layout;

  static constexpr bool kSSOAvailable = kMode <= ObjectMode::kAny;
};

template<BLObjectType type, ObjectMode mode, ObjectLayout layout, size_t sso_capacity, size_t sso_data_offset, size_t sso_size_byte>
struct ContainerInfo {
  static constexpr BLObjectType kType = type;
  static constexpr ObjectMode kMode = mode;
  static constexpr ObjectLayout kLayout = layout;

  static constexpr bool kSSOAvailable = kMode <= ObjectMode::kAny;
  static constexpr size_t kSSOCapacity = sso_capacity;
  static constexpr size_t kSSODataOffset = sso_data_offset;
  static constexpr size_t kSSOSizeByte = sso_size_byte;
};

} // {bl}
#endif

//! \}

//! \name BLObject - External Data
//! \{

//! A function callback that is called when an Impl that holds external data is going to be destroyed. It's
//! often used as a notification that a data passed to a certain Impl is no longer in use by Blend2D.
typedef void (BL_CDECL* BLDestroyExternalDataFunc)(BLObjectCore* object, void* external_data, void* user_data) BL_NOEXCEPT_C;

//! \}
//! \}

//! \addtogroup bl_c_api
//! \{

//! \name BLObject - C API
//! \{

//! Base class used by all Blend2D objects.
struct BLObjectCore {
  BLObjectDetail _d;

#ifdef __cplusplus
  //! \cond INTERNAL
  using CoreType = BLObjectCore;
  using ImplType = BLObjectImpl;
  using Info = bl::ObjectLayer::ObjectInfo<
    BL_OBJECT_TYPE_UNKNOWN,
    bl::ObjectLayer::ObjectMode::kAny,
    bl::ObjectLayer::ObjectLayout::kUnknown>;

  template<typename T = ImplType>
  [[nodiscard]] BL_INLINE_NODEBUG T* _impl() const noexcept { return _d.impl<T>(); }
  //! \endcond
#endif
};

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_object_init_move(BLObjectCore* self, BLObjectCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_init_weak(BLObjectCore* self, const BLObjectCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_destroy(BLObjectCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_reset(BLObjectCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_assign_move(BLObjectCore* self, BLObjectCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_assign_weak(BLObjectCore* self, const BLObjectCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_get_property(const BLObjectCore* self, const char* name, size_t name_size, BLVarCore* value_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_get_property_bool(const BLObjectCore* self, const char* name, size_t name_size, bool* value_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_get_property_int32(const BLObjectCore* self, const char* name, size_t name_size, int32_t* value_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_get_property_int64(const BLObjectCore* self, const char* name, size_t name_size, int64_t* value_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_get_property_uint32(const BLObjectCore* self, const char* name, size_t name_size, uint32_t* value_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_get_property_uint64(const BLObjectCore* self, const char* name, size_t name_size, uint64_t* value_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_get_property_double(const BLObjectCore* self, const char* name, size_t name_size, double* value_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_set_property(BLObjectCore* self, const char* name, size_t name_size, const BLObjectCore* value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_set_property_bool(BLObjectCore* self, const char* name, size_t name_size, bool value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_set_property_int32(BLObjectCore* self, const char* name, size_t name_size, int32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_set_property_int64(BLObjectCore* self, const char* name, size_t name_size, int64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_set_property_uint32(BLObjectCore* self, const char* name, size_t name_size, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_set_property_uint64(BLObjectCore* self, const char* name, size_t name_size, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_object_set_property_double(BLObjectCore* self, const char* name, size_t name_size, double value) BL_NOEXCEPT_C;

BL_EXTERN_C_END

//! \}
//! \}

//! \addtogroup bl_object
//! \{

//! \cond INTERNAL
//! \name BLObject - Macros
//! \{

//! \def BL_INHERITS_FROM(BASE)
//!
//! Defines an inheritance of a core struct compatible with \ref BLObjectCore.
//!
//! The purpose of this macro is to make Blend2D C++ API use an inheritance model, but not pure C API. This macro
//! is used before \ref BL_DEFINE_OBJECT, which follows next and defines the category of the object (or
//! the body of the struct when compiling by a C compiler).

//! \def BL_DEFINE_OBJECT(cxx_type, object_info)
//!
//! Makes an instance inherited from \ref BLObjectCore usable in Blend2D by adding the required properties to it.

//! \def BL_DEFINE_OBJECT_PROPERTY_API
//!
//! Defines a detail (in a body) of a struct defined by \ref BL_INHERITS_FROM.

//! \}
//! \endcond

#ifdef __cplusplus
  #define BL_INHERITS_FROM(base_type) : public base_type
  #define BL_DEFINE_OBJECT(cxx_type, ...)                                                                    \
    /*! \cond INTERNAL */                                                                                    \
    using CoreType = cxx_type##Core;                                                                         \
    using ImplType = cxx_type##Impl;                                                                         \
                                                                                                             \
    using Info = __VA_ARGS__;                                                                                \
                                                                                                             \
    template<typename T = cxx_type>                                                                          \
    [[nodiscard]] BL_INLINE_NODEBUG T& dcast() noexcept { return static_cast<T&>(*this); }                   \
                                                                                                             \
    template<typename T = cxx_type>                                                                          \
    [[nodiscard]] BL_INLINE_NODEBUG const T& dcast() const noexcept { return static_cast<const T&>(*this); } \
                                                                                                             \
    template<typename T = ImplType>                                                                          \
    [[nodiscard]] BL_INLINE_NODEBUG T* _impl() const noexcept { return static_cast<T*>(_d.dyn.impl.ptr); }   \
                                                                                                             \
    /*! \endcond */

  #define BL_DEFINE_OBJECT_VIRT(cxx_type, ...)                                                               \
    BL_DEFINE_OBJECT(cxx_type, __VA_ARGS__)                                                                  \
    using VirtType = cxx_type##Virt;                                                                         \
                                                                                                             \
    /*! \cond INTERNAL */                                                                                    \
    template<typename T = VirtType>                                                                          \
    [[nodiscard]] BL_INLINE_NODEBUG const T* _virt() const noexcept { return _d.virt<T>(); }                 \
    /*! \endcond */

  #define BL_DEFINE_OBJECT_DCAST(cxx_type)                                                                   \
    /*! \cond INTERNAL */                                                                                    \
    template<typename T = cxx_type>                                                                          \
    [[nodiscard]] BL_INLINE_NODEBUG T& dcast() noexcept { return static_cast<T&>(*this); }                   \
                                                                                                             \
    template<typename T = cxx_type>                                                                          \
    [[nodiscard]] BL_INLINE_NODEBUG const T& dcast() const noexcept { return static_cast<const T&>(*this); } \
    /*! \endcond */

  #define BL_DEFINE_VIRT_BASE
  #define BL_DEFINE_OBJECT_PROPERTY_API                                                                      \
    /*! Gets a property of the given `name` and assigns it to an initialized `value_out`. */                 \
    BL_INLINE_NODEBUG BLResult get_property(const char* name, BLVarCore& value_out) const noexcept {         \
      return bl_object_get_property(this, name, SIZE_MAX, &value_out);                                       \
    }                                                                                                        \
                                                                                                             \
    /*! \overload */                                                                                         \
    BL_INLINE_NODEBUG BLResult get_property(BLStringView name, BLVarCore& value_out) const noexcept {        \
      return bl_object_get_property(this, name.data(), name.size(), &value_out);                             \
    }                                                                                                        \
                                                                                                             \
    /*! Sets a property of the given `name` to `value`. */                                                   \
    BL_INLINE_NODEBUG BLResult set_property(const char* name, auto&& value) noexcept {                       \
      return set_property(BLStringView{name, SIZE_MAX}, bl::Internal::forward<decltype(value)>(value));      \
    }                                                                                                        \
                                                                                                             \
    /*! \overload */                                                                                         \
    BL_INLINE_NODEBUG BLResult set_property(BLStringView name, auto&& value) noexcept {                      \
      using T = std::remove_cvref_t<decltype(value)>;                                                        \
      if constexpr (std::is_same_v<T, bool>) {                                                               \
        return bl_object_set_property_bool(this, name.data(), name.size(), value);                           \
      }                                                                                                      \
      else if constexpr (std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) <= 4) {                   \
        return bl_object_set_property_int32(this, name.data(), name.size(), static_cast<int32_t>(value));    \
      }                                                                                                      \
      else if constexpr (std::is_integral_v<T> && std::is_signed_v<T> && sizeof(T) <= 8) {                   \
        return bl_object_set_property_int64(this, name.data(), name.size(), static_cast<int64_t>(value));    \
      }                                                                                                      \
      else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) <= 4) {                 \
        return bl_object_set_property_uint64(this, name.data(), name.size(), static_cast<uint32_t>(value));  \
      }                                                                                                      \
      else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) <= 8) {                 \
        return bl_object_set_property_uint64(this, name.data(), name.size(), static_cast<uint64_t>(value));  \
      }                                                                                                      \
      else if constexpr (std::is_floating_point_v<T> && sizeof(T) <= 8) {                                    \
        return bl_object_set_property_double(this, name.data(), name.size(), static_cast<double>(value));    \
      }                                                                                                      \
      else {                                                                                                 \
        return bl_object_set_property(this, name.data(), name.size(), &value);                               \
      }                                                                                                      \
    }
#else
  #define BL_INHERITS_FROM(base_type)
  #define BL_DEFINE_OBJECT(cxx_type, ...) BLObjectDetail _d;
  #define BL_DEFINE_OBJECT_VIRT(cxx_type, ...) BLObjectDetail _d;
  #define BL_DEFINE_OBJECT_DCAST(cxx_type)
  #define BL_DEFINE_VIRT_BASE BLObjectVirtBase base;
  #define BL_DEFINE_OBJECT_PROPERTY_API
#endif

//! \}

#endif // BLEND2D_CORE_OBJECT_H_INCLUDED
