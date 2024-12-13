// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OBJECT_H_INCLUDED
#define BLEND2D_OBJECT_H_INCLUDED

#include "api.h"
#include "rgba.h"

//! \defgroup bl_object Object Model
//! \brief Object Model & Memory Layout
//!
//! Blend2D object model is a foundation of all Blend2D objects. It was designed only for Blend2D and it's not
//! supposed to be used as a foundation of other libraries. The object model provides runtime reflection, small
//! size optimization (SSO), and good performance. In general, it focuses on optimizing memory footprint by taking
//! advantage of SSO storage, however, this makes the implementation more complex compared to a traditional non-SSO
//! model.
//!
//! Blend2D object model used by \ref BLObjectCore consists of 16 bytes that have the following layout:
//!
//! ```
//! union BLObjectDetail {
//!   BLObjectImpl* impl;
//!
//!   char char_data[16];
//!   uint8_t u8_data[16];
//!   uint16_t u16_data[8];
//!   uint32_t u32_data[4];
//!   uint64_t u64_data[2];
//!   float f32_data[4];
//!   double f64_data[2];
//!
//!   struct {
//!     uint32_t u32_data_overlap[2];
//!     uint32_t impl_payload;
//!     BLObjectInfo info; // 32 bits describing object type and its additional payload.
//!   };
//! };
//! ```
//!
//! Which allows to have either static or dynamic instances:
//!
//!   - Static instance stores payload in object detail, `impl` is not a valid pointer and cannot be accessed.
//!   - Dynamic instance has a valid `impl` pointer having a content, which type depends on \ref BLObjectType.
//!
//! The layout was designed to provide the following properties:
//!
//!   - Reflection - any Blend2D object can be casted to a generic \ref BLObjectCore or \ref BLVarCore and
//!     inspected at runtime.
//!   - Small string, container, and value optimization saves memory allocations (\ref BLString, \ref BLArray,
//!     \ref BLBitSet).
//!   - No atomic reference counting operations for small containers and default constructed objects without data.
//!   - It's possible to store a floating point RGBA color (BLRgba) as f32_data, which uses all 16 bytes. The last
//!     value of the color, which describes alpha channel, cannot have a sign bit set (cannot be negative and cannot
//!     be NaN with sign).
//!
//! 32-bit Floating Point is represented the following way (32 bits):
//!
//! ```
//! [--------+--------+--------+--------]
//! [31....24|23....16|15.....8|7......0] (32-bit integer layout)
//! [--------+--------+--------+--------]
//! [Seeeeeee|eQ......|........|........] (32-bit floating point)
//! [--------+--------+--------+--------]
//! ```
//!
//! Where:
//!
//!   - 'S' - Sign bit
//!   - 'e' - Exponent bits (all bits must be '1' to form NaN).
//!   - 'Q' - Mantissa bit that can be used to describe quiet and signaling NaNs, the value is not standardized
//!           (X86 and ARM use '1' for quiet NaN and '0' for signaling NaN).
//!   - '.' - Mantissa bits.
//!
//! Blend2D uses a sign bit to determine whether the data is \ref BLRgba or object compatible. This design has been
//! chosen, because we don't allow alpha values to be negative. When the sign bit is set it means that it's a type
//! inherited from \ref BLObjectCore. When the sign bit is not set the whole payload represents 128-bit \ref BLRgba
//! color, where alpha is not a negative number. It's designed in a way that 31 bits out of 32 can be used as payload
//! that represents object type, object info flags, and additional type-dependent payload.
//!
//! Object info value looks like this (also compared with floating point):
//!
//! ```
//! [--------+--------+--------+--------]
//! [31....24|23....16|15.....8|7......0] Info Layout:
//! [--------+--------+--------+--------]
//! [Seeeeeee|eQ......|........|........] - 32-bit floating-point data view (\ref BLRgba case, 'S' bit (sign bit) set to zero).
//! [MDRttttt|ttaaaaaa|bbbbcccc|pppppppp] - object info fields view 1 (\ref BLObjectCore case, 'M' bit set to one).
//! [MDRttttt|ttaaaaaa|qqqqqqqq|pppppppp] - object info fields view 2 (\ref BLObjectCore case, 'M' bit set to one).
//! [--------+--------+--------+--------]
//!
//! [--------+--------+--------+--------]
//! [31....24|23....16|15.....8|7......0] SSO Layout:
//! [--------+--------+--------+--------]
//! [1DRttttt|ttaaaaaa|bbbbcccc|pppppppp] - BLArray - 'aaaaaa' is size, 'bbbb' is capacity).
//! [1DRttttt|00aaaaaa|bbbbcccc|pppppppp] - BLString - 'aaaaaa' is size ^ kSSOCapacity, the rest can be used as characters).
//! [1DRttttt|ttaaaaaa|bbbbcccc|pppppppp] - BLBitSet - 'R' is used to distinguish between SSO Range and SSO Dense representation.
//! [1DRttttt|ttaaaaaa|bbbbcccc|pppppppp] - BLFontFeatureSettings - 'aaaaaa' is size, 'bbbbcccc|pppppppp' is used to store feature data.
//! [1DRttttt|ttaaaaaa|bbbbcccc|pppppppp] - BLFontVariationSettings - 'aaaaaa' is size, 'bbbbcccc|pppppppp' is used to store variation ids.
//! [--------+--------+--------+--------]
//! ```
//!
//! Where:
//!
//!   - 'M' - Object marker, forms a valid BLObject signature when set to '1'.
//!   - 'D' - Dynamic flag - when set the Impl pointer is valid.
//!           When 'D' == '0' it means the object is in SSO mode, when 'D' == '1' it means it's in Dynamic mode.
//!   - 'R' - Ref counted flag - when set together with 'M' and 'D' it makes it guaranteed that teh Impl pointer is ref-counted.
//!           Otherwise if 'D' is not set, 'R' flag can be used by the SSO representation to store another bit.
//!   - 't' - Object type bits - 'ttttttt' forms a 7-bit type having possible values from 0 to 127, see \ref BLObjectType.
//!
//!   - 'a' - Object 'a' payload (6 bits).
//!   - 'b' - Object 'b' payload (4 bits).
//!   - 'c' - Object 'c' payload (4 bits).
//!   - 'p' - Object 'p' payload (8 bits).
//!   - 'q' - Object 'q' payload (8 bits aliased with 'bbbbcccc' fields).
//!
//! Common meaning of payload fields:
//!
//!   - 'a' - If the object is a container (BLArray, BLString) 'a' field always represents its size in SSO mode.
//!           If the object is a \ref BLBitSet, 'a' field is combined with other fields to store a start word index
//!           or to mark a BitSet, which contains an SSO range instead of dense words.
//!   - 'b' - If the object is a container (BLArray) 'b' field always represents its capacity in SSO mode except
//!           \ref BLString, which doesn't store capacity in 'b' field and uses it as an additional SSO content byte
//!           on little endian targets (SSO capacity is then either 14 on little endian targets or 11 on big endian
//!           targets). This is possible as \ref BL_OBJECT_TYPE_STRING must be identifier that has 2 low bits zero,
//!           which then makes it possible to use 'ttIRaaaa' as null terminator when the string length is 14 characters.
//!   - 'c' - Used freely.
//!   - 'p' - Used freely.
//!   - 'q' - Used freely.
//!
//! If the 'D' flag is '1' the following payload fields are used by the allocator (and thus cannot be used by the object):
//!
//!   - 'a' - Allocation adjustment (4 bits) - At the moment the field describes how many bytes (shifted) to subtract
//!           from Impl to get the real pointer returned by Impl allocator. Object deallocation relies on this offset.
//!
//! Not all object support all defined flags, here is a little overview:
//!
//! ```
//! +--------------------------+---+---+---+---+---+---+---+
//! | Type                     | M |SSO|Dyn|Ext|Imm|Vrt|Ref|
//! +--------------------------+---+---+---+---+---+---+---|
//! | BLVar {Null}             | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 'M'   - Object marker (always used except wrapping BLRgba).
//! | BLVar {Bool}             | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 'SSO' - Small size optimization support (no Impl).
//! | BLVar {Int64}            | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 'Dyn' - Dynamic Impl support.
//! | BLVar {UInt64}           | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 'Ext' - External data support.
//! | BLVar {Double}           | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 'Imm' - Immutable data support.
//! | BLVar {BLRgba}           | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 'Vrt' - Object provides virtual function table.
//! | BLVar {BLRgba32}         | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 'Ref' - Reference counting support.
//! | BLVar {BLRgba64}         | 1 | 1 | 0 | 0 | 0 | 0 | 0 |
//! | BLArray<T>               | 1 | x | x | x | x | 0 | x | '0'   - Never used
//! | BLBitArray               | 1 | x | x | 0 | 0 | 0 | x | '1'   - Always used
//! | BLBitSet                 | 1 | x | x | 0 | 0 | 0 | x | 'x'   - Variable (either used or not)
//! | BLContext                | 1 | 0 | 1 | 0 | 0 | 1 | x |
//! | BLString                 | 1 | x | x | 0 | 0 | 0 | x |
//! | BLPattern                | 1 | 0 | 1 | 0 | 0 | 0 | x |
//! | BLGradient               | 1 | 0 | 1 | 0 | 0 | 0 | x |
//! | BLPath                   | 1 | 0 | 1 | 0 | x | 0 | x |
//! | BLImage                  | 1 | 0 | 1 | x | x | 0 | x |
//! | BLImageCodec             | 1 | 0 | 1 | 0 | x | 1 | x |
//! | BLImageDecoder           | 1 | 0 | 1 | 0 | 0 | 1 | x |
//! | BLImageEncoder           | 1 | 0 | 1 | 0 | 0 | 1 | x |
//! | BLFont                   | 1 | 0 | 1 | 0 | 0 | 0 | x |
//! | BLFontFace               | 1 | 0 | 1 | 0 | x | 1 | x |
//! | BLFontData               | 1 | 0 | 1 | x | x | 1 | x |
//! | BLFontManager            | 1 | 0 | 1 | 0 | x | 1 | x |
//! | BLFontFeatureSettings    | 1 | 1 | 1 | 0 | 0 | 0 | x |
//! | BLFontVariationSettings  | 1 | 1 | 1 | 0 | 0 | 0 | x |
//! +--------------------------+---+---+---+---+---+---+---+
//! ```

//! \addtogroup bl_object
//! \{

//! \name BLObject - Constants
//! \{

//! \cond INTERNAL
//! Defines a start offset of each field or flag in object info - the shift can be then used to get/set value from/to
//! info bits.
BL_DEFINE_ENUM(BLObjectInfoShift) {
  BL_OBJECT_INFO_P_SHIFT = 0,
  BL_OBJECT_INFO_Q_SHIFT = 8,
  BL_OBJECT_INFO_C_SHIFT = 8,
  BL_OBJECT_INFO_B_SHIFT = 12,
  BL_OBJECT_INFO_A_SHIFT = 16,
  BL_OBJECT_INFO_TYPE_SHIFT = 22,
  BL_OBJECT_INFO_R_SHIFT = 29,
  BL_OBJECT_INFO_D_SHIFT = 30,
  BL_OBJECT_INFO_M_SHIFT = 31

  BL_FORCE_ENUM_UINT32(BL_OBJECT_INFO_SHIFT)
};
//! \endcond

//! Defines a mask of each field of the object info.
//!
//! \note This is part of the official documentation, however, users should not use these enumerations in any context.
BL_DEFINE_ENUM(BLObjectInfoBits) {
  //! Mask describing 'P' payload (8 bits).
  BL_OBJECT_INFO_P_MASK = 0xFFu << BL_OBJECT_INFO_P_SHIFT,           // [........|........|........|pppppppp]
  //! Mask describing 'Q' payload (8 bits aliased with 'bbbbcccc' bits).
  BL_OBJECT_INFO_Q_MASK = 0xFFu << BL_OBJECT_INFO_Q_SHIFT,           // [........|........|qqqqqqqq|........]
  //! Mask describing 'C' payload (4 bits).
  BL_OBJECT_INFO_C_MASK = 0x0Fu << BL_OBJECT_INFO_C_SHIFT,           // [........|........|....cccc|........]
  //! Mask describing 'B' payload (4 bits).
  BL_OBJECT_INFO_B_MASK = 0x0Fu << BL_OBJECT_INFO_B_SHIFT,           // [........|........|bbbb....|........]
  //! Mask describing 'A' payload (6 bits).
  BL_OBJECT_INFO_A_MASK = 0x3Fu << BL_OBJECT_INFO_A_SHIFT,           // [........|..aaaaaa|........|........]

  //! Mask of all payload fields combined, except 'M', 'T', type identification, and 'R' (RefCounted marker).
  BL_OBJECT_INFO_FIELDS_MASK = 0x003FFFFF,

  //! Mask describing object type (8 bits), see \ref BLObjectType.
  BL_OBJECT_INFO_TYPE_MASK = 0x7Fu << BL_OBJECT_INFO_TYPE_SHIFT,     // [...ttttt|tt......|........|........]
  //! Flag describing a ref-counted object (if set together with 'D' flag)
  //!
  //! \note This flag is free for use by SSO, it has no meaning when 'D' flag is not set).
  BL_OBJECT_INFO_R_FLAG = 0x01u << BL_OBJECT_INFO_R_SHIFT,           // [..R.....|........|........|........]
  //! Flag describing a dynamic object - if this flag is not set, it means the object is in SSO mode.
  BL_OBJECT_INFO_D_FLAG = 0x01u << BL_OBJECT_INFO_D_SHIFT,           // [.D......|........|........|........]
  //! Flag describing a valid object compatible with \ref BLObjectCore interface (otherwise it's most likely \ref BLRgba).
  BL_OBJECT_INFO_M_FLAG = 0x01u << BL_OBJECT_INFO_M_SHIFT,           // [M.......|........|........|........]

  //! A combination of `BL_OBJECT_INFO_M_FLAG` and `BL_OBJECT_INFO_D_FLAG` flags.
  BL_OBJECT_INFO_MD_FLAGS = BL_OBJECT_INFO_M_FLAG | BL_OBJECT_INFO_D_FLAG,
  //! A combination of `BL_OBJECT_INFO_M_FLAG`, `BL_OBJECT_INFO_D_FLAG`, `BL_OBJECT_INFO_R_FLAG` flags.
  BL_OBJECT_INFO_MDR_FLAGS = BL_OBJECT_INFO_M_FLAG | BL_OBJECT_INFO_D_FLAG | BL_OBJECT_INFO_R_FLAG

  BL_FORCE_ENUM_UINT32(BL_OBJECT_INFO_BITS)
};

//! Object type identifier.
BL_DEFINE_ENUM(BLObjectType) {
  //! Object represents a \ref BLRgba value stored as four 32-bit floating point components (can be used as Style).
  BL_OBJECT_TYPE_RGBA = 0,
  //! Object represents a \ref BLRgba32 value stored as 32-bit integer in `0xAARRGGBB` form.
  BL_OBJECT_TYPE_RGBA32 = 1,
  //! Object represents a \ref BLRgba64 value stored as 64-bit integer in `0xAAAARRRRGGGGBBBB` form.
  BL_OBJECT_TYPE_RGBA64 = 2,
  //! Object is `Null` (can be used as style).
  BL_OBJECT_TYPE_NULL = 3,
  //! Object is \ref BLPattern (can be used as style).
  BL_OBJECT_TYPE_PATTERN = 4,
  //! Object is \ref BLGradient (can be used as style).
  BL_OBJECT_TYPE_GRADIENT = 5,

  //! Object is \ref BLImage.
  BL_OBJECT_TYPE_IMAGE = 9,
  //! Object is \ref BLPath.
  BL_OBJECT_TYPE_PATH = 10,

  //! Object is \ref BLFont.
  BL_OBJECT_TYPE_FONT = 16,
  //! Object is \ref BLFontFeatureSettings.
  BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS = 17,
  //! Object is \ref BLFontVariationSettings.
  BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS = 18,

  //! Object is \ref BLBitArray.
  BL_OBJECT_TYPE_BIT_ARRAY = 25,
  //! Object is \ref BLBitSet.
  BL_OBJECT_TYPE_BIT_SET = 26,

  //! Object represents a boolean value.
  BL_OBJECT_TYPE_BOOL = 28,
  //! Object represents a 64-bit signed integer value.
  BL_OBJECT_TYPE_INT64 = 29,
  //! Object represents a 64-bit unsigned integer value.
  BL_OBJECT_TYPE_UINT64 = 30,
  //! Object represents a 64-bit floating point value.
  BL_OBJECT_TYPE_DOUBLE = 31,
  //! Object is \ref BLString.
  BL_OBJECT_TYPE_STRING = 32,

  //! Object is \ref BLArray<T> where `T` is a `BLObject` compatible type.
  BL_OBJECT_TYPE_ARRAY_OBJECT = 33,
  //! Object is \ref BLArray<T> where `T` matches 8-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT8 = 34,
  //! Object is \ref BLArray<T> where `T` matches 8-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT8 = 35,
  //! Object is \ref BLArray<T> where `T` matches 16-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT16 = 36,
  //! Object is \ref BLArray<T> where `T` matches 16-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT16 = 37,
  //! Object is \ref BLArray<T> where `T` matches 32-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT32 = 38,
  //! Object is \ref BLArray<T> where `T` matches 32-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT32 = 39,
  //! Object is \ref BLArray<T> where `T` matches 64-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT64 = 40,
  //! Object is \ref BLArray<T> where `T` matches 64-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT64 = 41,
  //! Object is \ref BLArray<T> where `T` matches 32-bit floating point type.
  BL_OBJECT_TYPE_ARRAY_FLOAT32 = 42,
  //! Object is \ref BLArray<T> where `T` matches 64-bit floating point type.
  BL_OBJECT_TYPE_ARRAY_FLOAT64 = 43,
  //! Object is \ref BLArray<T> where `T` is a struct of size 1.
  BL_OBJECT_TYPE_ARRAY_STRUCT_1 = 44,
  //! Object is \ref BLArray<T> where `T` is a struct of size 2.
  BL_OBJECT_TYPE_ARRAY_STRUCT_2 = 45,
  //! Object is \ref BLArray<T> where `T` is a struct of size 3.
  BL_OBJECT_TYPE_ARRAY_STRUCT_3 = 46,
  //! Object is \ref BLArray<T> where `T` is a struct of size 4.
  BL_OBJECT_TYPE_ARRAY_STRUCT_4 = 47,
  //! Object is \ref BLArray<T> where `T` is a struct of size 6.
  BL_OBJECT_TYPE_ARRAY_STRUCT_6 = 48,
  //! Object is \ref BLArray<T> where `T` is a struct of size 8.
  BL_OBJECT_TYPE_ARRAY_STRUCT_8 = 49,
  //! Object is \ref BLArray<T> where `T` is a struct of size 10.
  BL_OBJECT_TYPE_ARRAY_STRUCT_10 = 50,
  //! Object is \ref BLArray<T> where `T` is a struct of size 12.
  BL_OBJECT_TYPE_ARRAY_STRUCT_12 = 51,
  //! Object is \ref BLArray<T> where `T` is a struct of size 16.
  BL_OBJECT_TYPE_ARRAY_STRUCT_16 = 52,
  //! Object is \ref BLArray<T> where `T` is a struct of size 20.
  BL_OBJECT_TYPE_ARRAY_STRUCT_20 = 53,
  //! Object is \ref BLArray<T> where `T` is a struct of size 24.
  BL_OBJECT_TYPE_ARRAY_STRUCT_24 = 54,
  //! Object is \ref BLArray<T> where `T` is a struct of size 32.
  BL_OBJECT_TYPE_ARRAY_STRUCT_32 = 55,

  //! Object is \ref BLContext.
  BL_OBJECT_TYPE_CONTEXT = 100,

  //! Object is \ref BLImageCodec.
  BL_OBJECT_TYPE_IMAGE_CODEC = 101,
  //! Object is \ref BLImageDecoder.
  BL_OBJECT_TYPE_IMAGE_DECODER = 102,
  //! Object is \ref BLImageEncoder.
  BL_OBJECT_TYPE_IMAGE_ENCODER = 103,

  //! Object is \ref BLFontFace.
  BL_OBJECT_TYPE_FONT_FACE = 104,
  //! Object is \ref BLFontData.
  BL_OBJECT_TYPE_FONT_DATA = 105,
  //! Object is \ref BLFontManager.
  BL_OBJECT_TYPE_FONT_MANAGER = 106,

  //! Minimum object type of an array object.
  BL_OBJECT_TYPE_MIN_ARRAY = 33,
  //! Maximum object type of an array object.
  BL_OBJECT_TYPE_MAX_ARRAY = 55,

  //! Minimum object type identifier that can be used as a style.
  BL_OBJECT_TYPE_MIN_STYLE = 0,
  //! Maximum object type identifier that can be used as a style.
  BL_OBJECT_TYPE_MAX_STYLE = 5,

  //! Minimum object type of an object with virtual function table.
  BL_OBJECT_TYPE_MIN_VIRTUAL = 100,
  //! Maximum object type of an object with virtual function table.
  BL_OBJECT_TYPE_MAX_VIRTUAL = 127,

  //! Maximum possible value of an object type, including identifiers reserved for the future.
  BL_OBJECT_TYPE_MAX_VALUE = 127

  BL_FORCE_ENUM_UINT32(BL_OBJECT_TYPE)
};

//! \}

//! \name BLObject - Detail
//! \{

//! Information bits used by \ref BLObjectCore and all Blend2D compatible objects inheriting it.
struct BLObjectInfo {
  //! \name Members
  //! \{

  //! Stores all object info bits.
  uint32_t bits;

  //! \}

#ifdef __cplusplus
  //! \name Constants
  //! \{

  enum : uint32_t {
    kSignatureMinDynamicObject =
      BL_OBJECT_INFO_M_FLAG |
      BL_OBJECT_INFO_D_FLAG,

    kSignatureMinVirtualObject =
      BL_OBJECT_INFO_M_FLAG |
      BL_OBJECT_INFO_D_FLAG |
      (BL_OBJECT_TYPE_MIN_VIRTUAL << BL_OBJECT_INFO_TYPE_SHIFT),

    //! Signature of a SSO BitSet, which is in Range mode.
    kSignatureSSOBitSetRange = (BL_OBJECT_INFO_M_FLAG) |
                               (BL_OBJECT_TYPE_BIT_SET << BL_OBJECT_INFO_TYPE_SHIFT) |
                               (BL_OBJECT_INFO_R_FLAG)
  };

  //! \}

  //! \name Static Methods for Packing & Unpacking
  //! \{

  //! Packs object type into object info bits.
  static BL_INLINE_NODEBUG constexpr uint32_t packType(BLObjectType type) noexcept {
    return uint32_t(type) << BL_OBJECT_INFO_TYPE_SHIFT;
  }

  //! Packs object type and M flag into object info bits.
  static BL_INLINE_NODEBUG constexpr uint32_t packTypeWithMarker(BLObjectType type) noexcept {
    return (uint32_t(type) << BL_OBJECT_INFO_TYPE_SHIFT) | BL_OBJECT_INFO_M_FLAG;
  }

  //! Packs A, B, C, and P fields so they can be combined with other object info bits.
  static BL_INLINE_NODEBUG constexpr uint32_t packAbcp(uint32_t aField, uint32_t bField = 0u, uint32_t cField = 0u, uint32_t pField = 0u) noexcept {
    return (aField << BL_OBJECT_INFO_A_SHIFT) |
           (bField << BL_OBJECT_INFO_B_SHIFT) |
           (cField << BL_OBJECT_INFO_C_SHIFT) |
           (pField << BL_OBJECT_INFO_P_SHIFT) ;
  }

  static BL_INLINE_NODEBUG constexpr BLObjectInfo fromType(BLObjectType type) noexcept {
    return BLObjectInfo{packType(type)};
  }

  static BL_INLINE_NODEBUG constexpr BLObjectInfo fromTypeWithMarker(BLObjectType type) noexcept {
    return BLObjectInfo{packTypeWithMarker(type)};
  }

  static BL_INLINE_NODEBUG constexpr BLObjectInfo fromAbcp(uint32_t aField, uint32_t bField = 0u, uint32_t cField = 0u, uint32_t pField = 0u) noexcept {
    return BLObjectInfo{packAbcp(aField, bField, cField, pField)};
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG constexpr bool operator==(const BLObjectInfo& other) const noexcept { return bits == other.bits; }
  BL_INLINE_NODEBUG constexpr bool operator!=(const BLObjectInfo& other) const noexcept { return bits != other.bits; }

  BL_INLINE_NODEBUG constexpr BLObjectInfo operator|(const BLObjectInfo& v) const noexcept { return BLObjectInfo{bits | v.bits}; }
  BL_INLINE_NODEBUG constexpr BLObjectInfo operator&(const BLObjectInfo& v) const noexcept { return BLObjectInfo{bits & v.bits}; }
  BL_INLINE_NODEBUG constexpr BLObjectInfo operator^(const BLObjectInfo& v) const noexcept { return BLObjectInfo{bits ^ v.bits}; }

  BL_INLINE_NODEBUG constexpr BLObjectInfo operator|(const BLObjectInfoBits& v) const noexcept { return BLObjectInfo{bits | v}; }
  BL_INLINE_NODEBUG constexpr BLObjectInfo operator&(const BLObjectInfoBits& v) const noexcept { return BLObjectInfo{bits & v}; }
  BL_INLINE_NODEBUG constexpr BLObjectInfo operator^(const BLObjectInfoBits& v) const noexcept { return BLObjectInfo{bits ^ v}; }

  BL_INLINE_NODEBUG BLObjectInfo& operator|=(const BLObjectInfo& v) noexcept { bits |= v.bits; return *this; }
  BL_INLINE_NODEBUG BLObjectInfo& operator&=(const BLObjectInfo& v) noexcept { bits &= v.bits; return *this; }
  BL_INLINE_NODEBUG BLObjectInfo& operator^=(const BLObjectInfo& v) noexcept { bits ^= v.bits; return *this; }

  BL_INLINE_NODEBUG BLObjectInfo& operator|=(const BLObjectInfoBits& v) noexcept { bits |= v; return *this; }
  BL_INLINE_NODEBUG BLObjectInfo& operator&=(const BLObjectInfoBits& v) noexcept { bits &= v; return *this; }
  BL_INLINE_NODEBUG BLObjectInfo& operator^=(const BLObjectInfoBits& v) noexcept { bits ^= v; return *this; }

  //! \}

  //! \name Info Data Accessors - Generic
  //! \{

  //! Extracts a field based on Shift and Mask.
  //!
  //! \note It doesn't verify whether the object info is valid, it just extracts the field.
  template<uint32_t Shift, uint32_t Mask>
  BL_INLINE_NODEBUG constexpr uint32_t getField() const noexcept { return (bits >> Shift) & (Mask >> Shift); }

  template<uint32_t Shift, uint32_t Mask>
  BL_INLINE_NODEBUG void setField(uint32_t value) noexcept { bits = (bits & ~Mask) | (value << Shift); }

  BL_INLINE_NODEBUG constexpr bool sso() const noexcept { return (bits & BL_OBJECT_INFO_D_FLAG) == 0; }
  BL_INLINE_NODEBUG constexpr bool dynamicFlag() const noexcept { return (bits & BL_OBJECT_INFO_D_FLAG) != 0; }

  BL_INLINE_NODEBUG constexpr uint32_t aField() const noexcept { return getField<BL_OBJECT_INFO_A_SHIFT, BL_OBJECT_INFO_A_MASK>(); }
  BL_INLINE_NODEBUG constexpr uint32_t bField() const noexcept { return getField<BL_OBJECT_INFO_B_SHIFT, BL_OBJECT_INFO_B_MASK>(); }
  BL_INLINE_NODEBUG constexpr uint32_t cField() const noexcept { return getField<BL_OBJECT_INFO_C_SHIFT, BL_OBJECT_INFO_C_MASK>(); }
  BL_INLINE_NODEBUG constexpr uint32_t pField() const noexcept { return getField<BL_OBJECT_INFO_P_SHIFT, BL_OBJECT_INFO_P_MASK>(); }
  BL_INLINE_NODEBUG constexpr uint32_t qField() const noexcept { return getField<BL_OBJECT_INFO_Q_SHIFT, BL_OBJECT_INFO_Q_MASK>(); }
  BL_INLINE_NODEBUG constexpr uint32_t fields() const noexcept { return bits & BL_OBJECT_INFO_FIELDS_MASK; }

  BL_INLINE_NODEBUG void setAField(uint32_t value) noexcept { setField<BL_OBJECT_INFO_A_SHIFT, BL_OBJECT_INFO_A_MASK>(value); }
  BL_INLINE_NODEBUG void setBField(uint32_t value) noexcept { setField<BL_OBJECT_INFO_B_SHIFT, BL_OBJECT_INFO_B_MASK>(value); }
  BL_INLINE_NODEBUG void setCField(uint32_t value) noexcept { setField<BL_OBJECT_INFO_C_SHIFT, BL_OBJECT_INFO_C_MASK>(value); }
  BL_INLINE_NODEBUG void setPField(uint32_t value) noexcept { setField<BL_OBJECT_INFO_P_SHIFT, BL_OBJECT_INFO_P_MASK>(value); }
  BL_INLINE_NODEBUG void setQField(uint32_t value) noexcept { setField<BL_OBJECT_INFO_Q_SHIFT, BL_OBJECT_INFO_Q_MASK>(value); }
  BL_INLINE_NODEBUG void setFields(uint32_t value) noexcept { setField<0, BL_OBJECT_INFO_FIELDS_MASK>(value); }

  //! \}

  //! \name BLObject Signature Accessors
  //! \{

  //! Tests whether BLObjectInfo describes a valid BLObject and verifies that `additionalBits` match the
  //! given `mask` in BLObjectInfo bits as well. This function is a higher-level function used by others.
  BL_INLINE_NODEBUG constexpr bool hasObjectSignatureAndFlags(uint32_t mask, uint32_t check) const noexcept {
    return (bits & (BL_OBJECT_INFO_M_FLAG | mask)) == (BL_OBJECT_INFO_M_FLAG | check);
  }

  //! Tests whether BLObjectInfo describes a valid BLObject and verifies the the given `flags` are all set.
  BL_INLINE_NODEBUG constexpr bool hasObjectSignatureAndFlags(uint32_t flags) const noexcept {
    return hasObjectSignatureAndFlags(flags, flags);
  }

  //! Tests whether the object info represents a valid BLObject signature.
  //!
  //! A valid signature describes a \ref BLObjectCore and not an alternative representation used by \ref BLRgba data.
  BL_INLINE_NODEBUG constexpr bool hasObjectSignature() const noexcept { return hasObjectSignatureAndFlags(0u); }

  //! Tests whether BLObjectInfo describes a valid BLObject of the given `type`.
  BL_INLINE_NODEBUG constexpr bool checkObjectSignatureAndRawType(BLObjectType type) const noexcept {
    return hasObjectSignatureAndFlags(uint32_t(type) << BL_OBJECT_INFO_TYPE_SHIFT);
  }

  //! \}

  //! \name BLObject Type Accessors
  //! \{

  //! Tests a whether this \ref BLObjectInfo represents a valid \ref BLObjectCore.
  BL_INLINE_NODEBUG constexpr bool isObject() const noexcept {
    return (bits & BL_OBJECT_INFO_M_FLAG) != 0;
  }

  //! Returns a whether this \ref BLObjectInfo represents a valid \ref BLObjectCore as a mask (either all zeros or all ones).
  BL_INLINE_NODEBUG constexpr uint32_t isObjectMask() const noexcept {
    return uint32_t((int32_t(bits) >> 31));
  }

  //! Tests whether the object info represents a valid BLObject, which has a valid Impl field.
  BL_INLINE_NODEBUG constexpr bool isDynamicObject() const noexcept {
    return bits >= uint32_t(BL_OBJECT_INFO_MD_FLAGS);
  }

  //! Tests whether the object info represents a valid BLObject, which has a valid Impl, and is reference counted.
  BL_INLINE_NODEBUG constexpr bool isRefCountedObject() const noexcept {
    return bits >= uint32_t(BL_OBJECT_INFO_MDR_FLAGS);
  }

  //! Tests whether the object info represents a valid BLObject, which has a valid Impl, and has a virtual function table.
  BL_INLINE_NODEBUG constexpr bool isVirtualObject() const noexcept {
    return (bits & (BL_OBJECT_INFO_MD_FLAGS | BL_OBJECT_INFO_TYPE_MASK)) >= uint32_t(kSignatureMinVirtualObject);
  }

  //! Returns a RAW \ref BLObjectType read from object info bits without checking for a 'M' object marker.
  //!
  //! This function should only be used in case that the caller knows that the object info is of a valid \ref
  //! BLObjectCore. In any other case the use of \ref getType() is preferred and would always provide a correct
  //! type.
  BL_INLINE_NODEBUG constexpr BLObjectType rawType() const noexcept {
    return BLObjectType(getField<BL_OBJECT_INFO_TYPE_SHIFT, BL_OBJECT_INFO_TYPE_MASK>());
  }

  //! Returns a corrected \ref BLObjectType read from object info bits.
  //!
  //! If the object marker bit 'M' is not set, 0 will be returned, which represents \ref BL_OBJECT_TYPE_RGBA.
  BL_INLINE_NODEBUG constexpr BLObjectType getType() const noexcept { return BLObjectType(uint32_t(rawType()) & isObjectMask()); }

  //! Tests whether the object info represents a `BLArray<T>` storing any supported type.
  BL_INLINE_NODEBUG constexpr bool isArray() const noexcept { return getType() >= BL_OBJECT_TYPE_MIN_ARRAY && getType() <= BL_OBJECT_TYPE_MAX_ARRAY; }
  //! Tests whether the object info represents a `BLBitArray`.
  BL_INLINE_NODEBUG constexpr bool isBitArray() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_BIT_ARRAY); }
  //! Tests whether the object info represents a `BLBitSet`.
  BL_INLINE_NODEBUG constexpr bool isBitSet() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_BIT_SET); }
  //! Tests whether the object info represents a boxed `bool` value.
  BL_INLINE_NODEBUG constexpr bool isBool() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_BOOL); }
  //! Tests whether the object info represents `BLContext`.
  BL_INLINE_NODEBUG constexpr bool isContext() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_CONTEXT); }
  //! Tests whether the object info represents a boxed `double` value.
  BL_INLINE_NODEBUG constexpr bool isDouble() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_DOUBLE); }
  //! Tests whether the object info represents `BLFont`.
  BL_INLINE_NODEBUG constexpr bool isFont() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_FONT); }
  //! Tests whether the object info represents `BLFontData`.
  BL_INLINE_NODEBUG constexpr bool isFontData() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_FONT_DATA); }
  //! Tests whether the object info represents `BLFontFace`.
  BL_INLINE_NODEBUG constexpr bool isFontFace() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_FONT_FACE); }
  //! Tests whether the object info represents `BLFontFeatureSettings`.
  BL_INLINE_NODEBUG constexpr bool isFontFeatureSettings() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS); }
  //! Tests whether the object info represents `BLFontManager`.
  BL_INLINE_NODEBUG constexpr bool isFontManager() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_FONT_MANAGER); }
  //! Tests whether the object info represents `BLFontVariationSettings`.
  BL_INLINE_NODEBUG constexpr bool isFontVariationSettings() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS); }
  //! Tests whether the object info represents `BLGradient`.
  BL_INLINE_NODEBUG constexpr bool isGradient() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_GRADIENT); }
  //! Tests whether the object info represents `BLImage`.
  BL_INLINE_NODEBUG constexpr bool isImage() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_IMAGE); }
  //! Tests whether the object info represents `BLImageCodec`.
  BL_INLINE_NODEBUG constexpr bool isImageCodec() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_IMAGE_CODEC); }
  //! Tests whether the object info represents `BLImageDecoder`.
  BL_INLINE_NODEBUG constexpr bool isImageDecoder() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_IMAGE_DECODER); }
  //! Tests whether the object info represents `BLImageEncoder`.
  BL_INLINE_NODEBUG constexpr bool isImageEncoder() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_IMAGE_ENCODER); }
  //! Tests whether the object info represents a boxed `int64_t` value.
  BL_INLINE_NODEBUG constexpr bool isInt64() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_INT64); }
  //! Tests whether the object info represents a null value.
  BL_INLINE_NODEBUG constexpr bool isNull() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_NULL); }
  //! Tests whether the object info represents `BLPath`.
  BL_INLINE_NODEBUG constexpr bool isPath() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_PATH); }
  //! Tests whether the object info represents `BLPattern.`
  BL_INLINE_NODEBUG constexpr bool isPattern() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_PATTERN); }
  //! Tests whether the object info represents `BLRgba`.
  BL_INLINE_NODEBUG constexpr bool isRgba() const noexcept { return !isObject(); }
  //! Tests whether the object info represents `BLRgba32`.
  BL_INLINE_NODEBUG constexpr bool isRgba32() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_RGBA32); }
  //! Tests whether the object info represents `BLRgba64`.
  BL_INLINE_NODEBUG constexpr bool isRgba64() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_RGBA64); }
  //! Tests whether the object info represents `BLString`.
  BL_INLINE_NODEBUG constexpr bool isString() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_STRING); }
  //! Tests whether the object info represents a boxed `uint64_t` value.
  BL_INLINE_NODEBUG constexpr bool isUInt64() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_UINT64); }

  //! Tests whether the object info represents a style that can be passed to the rendering context.
  BL_INLINE_NODEBUG constexpr bool isStyle() const noexcept { return getType() <= BL_OBJECT_TYPE_MAX_STYLE; }

  // \}

  //! \cond INTERNAL
  //! \name BLObject Type Accessors - Object Specific
  //! \{

  //! Tests whether the object info represents a `BLBitSet`, which is in SSO range mode.
  //!
  //! \note An empty SSO range [0, 0) is used by default constructed BitSets.
  BL_INLINE_NODEBUG constexpr bool isBitSetRange() const noexcept { return bits == kSignatureSSOBitSetRange; }

  //! \}
  //! \endcond
#endif
};

//! Defines a BLObject layout that all objects must use.
union BLObjectDetail {
  BLObjectImpl* impl;

  char char_data[16];
  int8_t i8_data[16];
  uint8_t u8_data[16];
  int16_t i16_data[8];
  uint16_t u16_data[8];
  int32_t i32_data[4];
  uint32_t u32_data[4];
  int64_t i64_data[2];
  uint64_t u64_data[2];
  float f32_data[4];
  double f64_data[2];

  BLRgba rgba;
  BLRgba32 rgba32;
  BLRgba64 rgba64;

  struct {
    uint32_t u32_data_overlap[2];
    uint32_t impl_payload;
    BLObjectInfo info;
  };

#ifdef __cplusplus
  //! \name Constants
  //! \{

  enum : uint32_t {
    //! Size of object static storage not considering \ref BLObjectInfo.
    kStaticDataSize = 12
  };

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the given objects are binary equivalent.
  //!
  //! Binary equality is used by some equality implementations as a quick check. This can be used by both SSO and
  //! Dynamic instances.
  BL_INLINE_NODEBUG bool equals(const BLObjectDetail& other) const noexcept {
    return bool(unsigned(u64_data[0] == other.u64_data[0]) & unsigned(u64_data[1] == other.u64_data[1]));
  }

  BL_INLINE_NODEBUG bool operator==(const BLObjectDetail& other) const noexcept { return  equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLObjectDetail& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Initialization
  //! \{

  //! Initializes this BLObjectDetail with object that uses static storage.
  BL_INLINE_NODEBUG void initStatic(BLObjectInfo objectInfo) noexcept {
    u64_data[0] = 0;
    u32_data[2] = 0;
    info.bits = objectInfo.bits;
  }

  //! Initializes this BLObjectDetail with object that uses dynamic storage (Impl).
  BL_INLINE_NODEBUG void initDynamic(BLObjectInfo objectInfo, BLObjectImpl* implInit) noexcept {
    u64_data[0] = 0;
    impl = implInit;

    u32_data[2] = 0;
    info.bits = objectInfo.bits | BL_OBJECT_INFO_M_FLAG | BL_OBJECT_INFO_D_FLAG;
  }

  BL_INLINE_NODEBUG void initNull() noexcept {
    u64_data[0] = 0;
    u32_data[2] = 0;
    info.bits = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_NULL);
  }

  BL_INLINE_NODEBUG void initBool(bool value) noexcept {
    u64_data[0] = uint64_t(value);
    u32_data[2] = 0;
    info.bits = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_BOOL);
  }

  BL_INLINE_NODEBUG void initRgba32(uint32_t rgba32) noexcept {
    u32_data[0] = rgba32;
    u32_data[1] = 0;
    u32_data[2] = 0;
    info.bits = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_RGBA32);
  }

  BL_INLINE_NODEBUG void initRgba64(uint64_t rgba64) noexcept {
    u64_data[0] = rgba64;
    u32_data[2] = 0;
    info.bits = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_RGBA64);
  }

  BL_INLINE_NODEBUG void initInt64(int64_t value) noexcept {
    u64_data[0] = uint64_t(value);
    u32_data[2] = 0;
    info.bits = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_INT64);
  }

  BL_INLINE_NODEBUG void initUInt64(uint64_t value) noexcept {
    u64_data[0] = value;
    u32_data[2] = 0;
    info.bits = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_UINT64);
  }

  BL_INLINE_NODEBUG void initDouble(double value) noexcept {
    f64_data[0] = value;
    u32_data[2] = 0;
    info.bits = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_DOUBLE);
  }

  BL_INLINE_NODEBUG void initU32x4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3) noexcept {
    u32_data[0] = u0;
    u32_data[1] = u1;
    u32_data[2] = u2;
    u32_data[3] = u3;
  }

  BL_INLINE_NODEBUG void initF32x4(float f0, float f1, float f2, float f3) noexcept {
    f32_data[0] = f0;
    f32_data[1] = f1;
    f32_data[2] = f2;
    f32_data[3] = f3;
  }

  BL_INLINE_NODEBUG void clearStaticData() noexcept {
    u64_data[0] = 0;
    u32_data[2] = 0;
  }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Swaps this BLObjectDetail with `other`.
  BL_INLINE_NODEBUG void swap(BLObjectDetail& other) noexcept {
    BLObjectDetail a = *this;
    BLObjectDetail b = other;

    *this = b;
    other = a;
  }

  //! \}

  //! \name BLObject Data Accessors
  //! \{

  template<typename T>
  BL_INLINE_NODEBUG T* dataAs() noexcept { return reinterpret_cast<T*>(char_data); }

  template<typename T>
  BL_INLINE_NODEBUG const T* dataAs() const noexcept { return reinterpret_cast<const T*>(char_data); }

  //! \}

  //! \name BLObject Info Accessors - Generic
  //! \{

  BL_INLINE_NODEBUG bool sso() const noexcept { return info.sso(); }
  BL_INLINE_NODEBUG bool dynamicFlag() const noexcept { return info.dynamicFlag(); }

  BL_INLINE_NODEBUG uint32_t aField() const noexcept { return info.aField(); }
  BL_INLINE_NODEBUG uint32_t bField() const noexcept { return info.bField(); }
  BL_INLINE_NODEBUG uint32_t cField() const noexcept { return info.cField(); }
  BL_INLINE_NODEBUG uint32_t pField() const noexcept { return info.pField(); }
  BL_INLINE_NODEBUG uint32_t qField() const noexcept { return info.qField(); }
  BL_INLINE_NODEBUG uint32_t fields() const noexcept { return info.fields(); }

  //! \}

  //! \name BLObject Type Accessors
  //! \{

  //! Tests whether the object info of this BLObjectDetail contains a valid BLObject signature.
  BL_INLINE_NODEBUG bool hasObjectSignature() const noexcept { return info.hasObjectSignature(); }

  //! Tests whether the object info of this BLObjectDetail contains a valid BLObject, which has a valid Impl field.
  BL_INLINE_NODEBUG bool isDynamicObject() const noexcept { return info.isDynamicObject(); }
  //! Tests whether the object info of this BLObjectDetail represents a valid BLObject, with Impl and Virtual function table.
  BL_INLINE_NODEBUG bool isVirtualObject() const noexcept { return info.isVirtualObject(); }
  //! Tests whether the object info represents a valid BLObject, which has a valid Impl, and is reference counted.
  BL_INLINE_NODEBUG bool isRefCountedObject() const noexcept { return info.isRefCountedObject(); }

  //! Returns a RAW type read from object info data.
  BL_INLINE_NODEBUG BLObjectType rawType() const noexcept { return info.rawType(); }
  //! Returns the type of this object.
  BL_INLINE_NODEBUG BLObjectType getType() const noexcept { return info.getType(); }

  //! Tests whether this BLObjectDetail represents a `BLArray<T>` storing any supported type.
  BL_INLINE_NODEBUG bool isArray() const noexcept { return info.isArray(); }
  //! Tests whether this BLObjectDetail represents a `BLBitArray`.
  BL_INLINE_NODEBUG bool isBitArray() const noexcept { return info.isBitArray(); }
  //! Tests whether this BLObjectDetail represents a `BLBitSet`.
  BL_INLINE_NODEBUG bool isBitSet() const noexcept { return info.isBitSet(); }
  //! Tests whether this BLObjectDetail represents a boxed `bool` value.
  BL_INLINE_NODEBUG bool isBool() const noexcept { return info.isBool(); }
  //! Tests whether this BLObjectDetail represents `BLContext`.
  BL_INLINE_NODEBUG bool isContext() const noexcept { return info.isContext(); }
  //! Tests whether this BLObjectDetail represents a boxed `double` value.
  BL_INLINE_NODEBUG bool isDouble() const noexcept { return info.isDouble(); }
  //! Tests whether this BLObjectDetail represents `BLFont`.
  BL_INLINE_NODEBUG bool isFont() const noexcept { return info.isFont(); }
  //! Tests whether this BLObjectDetail represents `BLFontData`.
  BL_INLINE_NODEBUG bool isFontData() const noexcept { return info.isFontData(); }
  //! Tests whether this BLObjectDetail represents `BLFontFace`.
  BL_INLINE_NODEBUG bool isFontFace() const noexcept { return info.isFontFace(); }
  //! Tests whether this BLObjectDetail represents `BLFontFeatureSettings`.
  BL_INLINE_NODEBUG bool isFontFeatureSettings() const noexcept { return info.isFontFeatureSettings(); }
  //! Tests whether this BLObjectDetail represents `BLFontManager`.
  BL_INLINE_NODEBUG bool isFontManager() const noexcept { return info.isFontManager(); }
  //! Tests whether this BLObjectDetail represents `BLFontVariationSettings`.
  BL_INLINE_NODEBUG bool isFontVariationSettings() const noexcept { return info.isFontVariationSettings(); }
  //! Tests whether this BLObjectDetail represents `BLGradient`.
  BL_INLINE_NODEBUG bool isGradient() const noexcept { return info.isGradient(); }
  //! Tests whether this BLObjectDetail represents `BLImage`.
  BL_INLINE_NODEBUG bool isImage() const noexcept { return info.isImage(); }
  //! Tests whether this BLObjectDetail represents `BLImageCodec`.
  BL_INLINE_NODEBUG bool isImageCodec() const noexcept { return info.isImageCodec(); }
  //! Tests whether this BLObjectDetail represents `BLImageDecoder`.
  BL_INLINE_NODEBUG bool isImageDecoder() const noexcept { return info.isImageDecoder(); }
  //! Tests whether this BLObjectDetail represents `BLImageEncoder`.
  BL_INLINE_NODEBUG bool isImageEncoder() const noexcept { return info.isImageEncoder(); }
  //! Tests whether this BLObjectDetail represents a boxed `int64_t` value.
  BL_INLINE_NODEBUG bool isInt64() const noexcept { return info.isInt64(); }
  //! Tests whether this BLObjectDetail represents a null value.
  BL_INLINE_NODEBUG bool isNull() const noexcept { return info.isNull(); }
  //! Tests whether this BLObjectDetail represents `BLPath`.
  BL_INLINE_NODEBUG bool isPath() const noexcept { return info.isPath(); }
  //! Tests whether this BLObjectDetail represents `BLPattern.`
  BL_INLINE_NODEBUG bool isPattern() const noexcept { return info.isPattern(); }
  //! Tests whether this BLObjectDetail represents boxed `BLRgba`.
  BL_INLINE_NODEBUG bool isRgba() const noexcept { return info.isRgba(); }
  //! Tests whether this BLObjectDetail represents boxed `BLRgba32`.
  BL_INLINE_NODEBUG bool isRgba32() const noexcept { return info.isRgba32(); }
  //! Tests whether this BLObjectDetail represents boxed `BLRgba64`.
  BL_INLINE_NODEBUG bool isRgba64() const noexcept { return info.isRgba64(); }
  //! Tests whether this BLObjectDetail represents `BLString`.
  BL_INLINE_NODEBUG bool isString() const noexcept { return info.isString(); }
  //! Tests whether this BLObjectDetail represents a boxed `uint64_t` value.
  BL_INLINE_NODEBUG bool isUInt64() const noexcept { return info.isUInt64(); }

  //! Tests whether this BLObjectDetail represents a style that can be passed to the rendering context.
  BL_INLINE_NODEBUG bool isStyle() const noexcept { return info.isStyle(); }

  //! \}

  //! \name BLObject Type Accessors - Object Specific
  //! \{

  BL_INLINE_NODEBUG bool isBitSetRange() const noexcept { return info.isBitSetRange(); }

  //! \}
#endif
};

#ifdef __cplusplus
static_assert(sizeof(BLObjectDetail) == 16, "BLObjectDetail must be exactly 16 bytes long");
#endif

//! \}

//! \name BLObject - External Data
//! \{

//! A function callback that is called when an Impl that holds external data is going to be destroyed. It's
//! often used as a notification that a data passed to a certain Impl is no longer in use by Blend2D.
typedef void (BL_CDECL* BLDestroyExternalDataFunc)(void* impl, void* externalData, void* userData) BL_NOEXCEPT;

//! \}
//! \}

//! \addtogroup bl_c_api
//! \{

//! \name BLObject - C API
//! \{

//! BLObject [Impl].
//!
//! This is only used to model inheritance when compiled by a C++ compiler.
#ifdef __cplusplus
struct BLObjectImpl {
  //! \name Common Functionality
  //! \{

  //! Casts this Impl to `T*`.
  template<typename T>
  BL_INLINE_NODEBUG T* as() noexcept { return static_cast<T*>(this); }

  //! Casts this Impl to `T*` (const).
  template<typename T>
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
  BLResult (BL_CDECL* destroy)(BLObjectImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* getProperty)(const BLObjectImpl* impl, const char* name, size_t nameSize, BLVarCore* valueOut) BL_NOEXCEPT;
  BLResult (BL_CDECL* setProperty)(BLObjectImpl* impl, const char* name, size_t nameSize, const BLVarCore* value) BL_NOEXCEPT;
};

//! BLObject [Virtual Function Table].
//!
//! Virtual function table is only present when object type is greater than \ref BL_OBJECT_TYPE_MIN_VIRTUAL.
//! Objects can extend the function table, but it has to always start with members defined by `BLObjectVirt`.
struct BLObjectVirt {
  BLObjectVirtBase base;
};

//! Base class used by all Blend2D objects.
struct BLObjectCore {
  BLObjectDetail _d;
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blObjectAllocImpl(BLObjectCore* self, uint32_t objectInfo, size_t implSize) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectAllocImplAligned(BLObjectCore* self, uint32_t objectInfo, size_t implSize, size_t implAlignment) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectAllocImplExternal(BLObjectCore* self, uint32_t objectInfo, size_t implSize, bool immutable, BLDestroyExternalDataFunc destroyFunc, void* userData) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectFreeImpl(BLObjectImpl* impl) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blObjectInitMove(BLUnknown* self, BLUnknown* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectInitWeak(BLUnknown* self, const BLUnknown* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectReset(BLUnknown* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectAssignMove(BLUnknown* self, BLUnknown* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectAssignWeak(BLUnknown* self, const BLUnknown* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectGetProperty(const BLUnknown* self, const char* name, size_t nameSize, BLVarCore* valueOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectGetPropertyBool(const BLUnknown* self, const char* name, size_t nameSize, bool* valueOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectGetPropertyInt32(const BLUnknown* self, const char* name, size_t nameSize, int32_t* valueOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectGetPropertyInt64(const BLUnknown* self, const char* name, size_t nameSize, int64_t* valueOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectGetPropertyUInt32(const BLUnknown* self, const char* name, size_t nameSize, uint32_t* valueOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectGetPropertyUInt64(const BLUnknown* self, const char* name, size_t nameSize, uint64_t* valueOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectGetPropertyDouble(const BLUnknown* self, const char* name, size_t nameSize, double* valueOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectSetProperty(BLUnknown* self, const char* name, size_t nameSize, const BLUnknown* value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectSetPropertyBool(BLUnknown* self, const char* name, size_t nameSize, bool value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectSetPropertyInt32(BLUnknown* self, const char* name, size_t nameSize, int32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectSetPropertyInt64(BLUnknown* self, const char* name, size_t nameSize, int64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectSetPropertyUInt32(BLUnknown* self, const char* name, size_t nameSize, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectSetPropertyUInt64(BLUnknown* self, const char* name, size_t nameSize, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectSetPropertyDouble(BLUnknown* self, const char* name, size_t nameSize, double value) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}
//! \}

//! \addtogroup bl_object
//! \{

//! \cond INTERNAL
//! \name BLObject - Macros
//! \{

//! \def BL_CLASS_INHERITS(BASE)
//!
//! Defines an inheritance of a core struct compatible with \ref BLObjectCore.
//!
//! The purpose of this macro is to make Blend2D C++ API use an inheritance model, but not pure C API. This macro
//! is used together with \ref BL_DEFINE_OBJECT_DETAIL, which follows and defines the content of the struct body.

//! \def BL_DEFINE_OBJECT_DETAIL
//!
//! Defines a detail (in a body) of a struct defined by \ref BL_CLASS_INHERITS.

//! \def BL_DEFINE_OBJECT_PROPERTY_API
//!
//! Defines a detail (in a body) of a struct defined by \ref BL_CLASS_INHERITS.

//! \}
//! \endcond

#ifdef __cplusplus
  #define BL_CLASS_INHERITS(BASE) : public BASE
  #define BL_DEFINE_OBJECT_DETAIL
  #define BL_DEFINE_OBJECT_DCAST(DERIVED_TYPE)                                                     \
    /*! \cond INTERNAL */                                                                          \
    template<typename T = DERIVED_TYPE>                                                            \
    BL_NODISCARD                                                                                   \
    BL_INLINE_NODEBUG T& dcast() noexcept { return static_cast<T&>(*this); }                       \
                                                                                                   \
    template<typename T = DERIVED_TYPE>                                                            \
    BL_NODISCARD                                                                                   \
    BL_INLINE_NODEBUG const T& dcast() const noexcept { return static_cast<const T&>(*this); }     \
    /*! \endcond */

  #define BL_DEFINE_VIRT_BASE
  #define BL_DEFINE_OBJECT_PROPERTY_API                                                            \
    /*! Gets a property of the given `name` and assigns it to an initialized `valueOut`. */        \
    BL_INLINE_NODEBUG BLResult getProperty(const char* name, BLVarCore& valueOut) const noexcept { \
      return blObjectGetProperty(this, name, SIZE_MAX, &valueOut);                                 \
    }                                                                                              \
                                                                                                   \
    /*! \overload */                                                                               \
    BL_INLINE_NODEBUG BLResult getProperty(BLStringView name, BLVarCore& valueOut) const noexcept {\
      return blObjectGetProperty(this, name.data, name.size, &valueOut);                           \
    }                                                                                              \
                                                                                                   \
    /*! Sets a property of the given `name` to `value`. */                                         \
    BL_INLINE_NODEBUG BLResult setProperty(const char* name, const BLObjectCore& value) noexcept { \
      return blObjectSetProperty(this, name, SIZE_MAX, &value);                                    \
    }                                                                                              \
                                                                                                   \
    /*! \overload */                                                                               \
    BL_INLINE_NODEBUG BLResult setProperty(BLStringView name, const BLObjectCore& value) noexcept {\
      return blObjectSetProperty(this, name.data, name.size, &value);                              \
    }
#else
  #define BL_CLASS_INHERITS(BASE)
  #define BL_DEFINE_OBJECT_DETAIL BLObjectDetail _d;
  #define BL_DEFINE_OBJECT_DCAST(TO)
  #define BL_DEFINE_VIRT_BASE BLObjectVirtBase base;
  #define BL_DEFINE_OBJECT_PROPERTY_API
#endif

#ifdef __cplusplus
namespace BLInternal {

//! Internal helper function that can be used to optimize out calling a function that would only
//! need to be called in case that the object is dynamic and reference counted. At the moment it's
//! used to determine whether a type inherited from BLObject requires to call a destructor.
#if defined(__GNUC__)
static BL_INLINE_NODEBUG bool objectNeedsCleanup(uint32_t infoBits) noexcept {
  return __builtin_constant_p(infoBits) ? infoBits >= uint32_t(BL_OBJECT_INFO_MDR_FLAGS) : true;
}
#else
static BL_INLINE_NODEBUG constexpr bool objectNeedsCleanup(uint32_t) noexcept { return true; }
#endif

} // {BLInternal}
#endif // __cplusplus

//! \}

#endif // BLEND2D_OBJECT_H_INCLUDED
