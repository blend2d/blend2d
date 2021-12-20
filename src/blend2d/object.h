// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OBJECT_H_INCLUDED
#define BLEND2D_OBJECT_H_INCLUDED

#include "api.h"

//! \defgroup blend2d_api_object Object Model
//! \brief Object Model & Memory Layout
//!
//! Blend2D object model is a foundation of all Blend2D objects. It was designed only for Blend2D and it's not
//! supposed to be used as a foundation of other libraries. The object model provides runtime reflection, small
//! size optimization (SSO), and good performance. In general, it focuses on optimizing memory footprint by
//! taking advantage of SSO storage, however, this makes the implementation more complex compared to a traditional
//! non-SSO model.
//!
//! Blend2D object model used by \ref BLObjectCore consists of 16 bytes that have the following layout:
//!
//! ```
//! union BLObjectDetail {
//!   void* impl;
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
//!   - Static instance stores payload in object layout, Impl is not a valid pointer and cannot be accessed.
//!   - Dynamic instance has a valid Impl pointer having a content, which depends on \ref BLObjectType.
//!
//! The layout was designed to provide the following properties:
//!
//!   - Reflection - any Blend2D object can be casted to a generic \ref BLObjectCore or \ref BLVar and reflected
//!     at runtime.
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
//!   [--------+--------+--------+--------]
//!   [31....24|23....16|15.....8|7......0] (32-bit integer layout)
//!   [--------+--------+--------+--------]
//!   [Seeeeeee|eQ......|........|........] (32-bit floating point)
//!   [--------+--------+--------+--------]
//! ```
//!
//! Where:
//!
//!   - 'S' - Sign bit
//!   - 'e' - Exponent bits (all bits must be '1' to form NaN).
//!   - 'Q' - Fraction bit that can be used to describe quiet and signaling NaNs, the value is not standardized
//!           (X86 and ARM use '1' for quiet NaN and '0' for signaling NaN).
//!   - '.' - Fraction bits.
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
//!   [--------+--------+--------+--------]
//!   [31....24|23....16|15.....8|7......0] (32-bit integer layout)
//!   [--------+--------+--------+--------]
//!   [Seeeeeee|eQ......|........|........] (32-bit floating point) (\ref BLRgba case, 'S' bit (sign bit) set to zero).
//!   [MDTVtttt|ttaaaabb|bbccccpp|pppppXIR] (object info fields view) (\ref BLObjectCore case, 'M' bit set to one).
//!   [--------+--------+--------+--------]
//!   [1D00tttt|ttaaaabb|bbccccpp|pppppXIR] (BLArray  SSO layout - 'a' is an array size, 'bbbb' is capacity)
//!   [1D001000|00aaaabb|bbccccpp|pppppXIR] (BLString SSO layout - 'a' is a string size ^ SSO Capacity, the rest can be used as characters)
//!   [1D10tttt|ttaaaabb|bbccccpp|pppppXIR] (BLBitSet SSO layout - 'ttt|ttaaaabb|bbccccpp|pppppXIR' is a word start or SSO range sentinel)
//!   [--------+--------+--------+--------]
//! ```
//!
//! Where:
//!
//!   - 'M' - Object marker, forms a valid BLObject signature when set to '1'.
//!   - 'T' - Object type MSB bit - when set all other 't' bits are implied to be zero, only used by \ref BLBitSet to
//!           make it possible to index all possible BitWords in SSO mode - this should be considered a special case.
//!   - 'V' - Virtual table flag (implies 'D' == 1) - when set it means that Impl pointer has a virtual function table.
//!           This flag cannot be used in SSO mode as a payload. It can only be set when 'D' == 1. Objects that provide
//!           virtual function table are never SSO, so such objects always have both 'D' and 'V' set to '1'.
//!   - 't' - Object type LSB bits - 'TVtttttt' forms an 8-bit type having possible values from 0 to 128, see
//!           \ref BLObjectType.
//!
//!   - 'D' - Dynamic flag - when set the Impl pointer is valid.
//!           When 'D' == '0' it means the object is in SSO mode, when 'D' == '1' it means it's in Dynamic mode.
//!   - 'X' - External flag (with 'D' == 1) - used by allocator, when set the Impl holds external data.
//!           In SSO mode 'X' flag can be used as a payload.
//!   - 'I' - Immutable flag (with 'D' == 1) - if set the Impl is immutable - could be an ethernal built-in instance.
//!           In SSO mode 'I' flag can be used as a payload.
//!   - 'R' - Reference counted. Must be '1' if the the Impl is reference counted. The reference count still has to be
//!           provided even when 'R' == '0'.
//!           In SSO mode 'R' flag can be used as a payload.
//!
//!   - 'a' - Object 'a' payload (4 bits) - available in SSO mode, used by Impl allocator if 'D' is 1.
//!   - 'b' - Object 'b' payload (4 bits) - available in both SSO and Dynamic modes.
//!   - 'c' - Object 'c' payload (4 bits) - available in both SSO and Dynamic modes.
//!   - 'p' - Object 'p' payload (7 bits) - available in both SSO and Dynamic modes.
//!
//! Common meaning of payload fields:
//!
//!   - 'a' - If the object is a container (BLArray, BLString) 'a' field always represents its size in SSO mode.
//!           If the object is a \ref BLBitSet, 'a' field is combined with other fields to store a start word index
//!           or to mark a BitSet, which contains an SSO range instead of dense words.
//!   - 'b' - If the object is a container (BLArray) 'b' field always represents its capacity in SSO mode except
//!           \ref BLString, which doesn't store capacity in 'b' field and uses it as an additional SSO content byte
//!           on little endian targets (SSO capacity is then either 13 on little endian targets or 11 on big endian
//!           ones). This is possible as \ref BL_OBJECT_TYPE_STRING must be identifier that has 2 low bits zero, which
//!           then makes it possible to use 'ttIRaaaa' as null terminator when the string length is 14 characters.
//!   - 'c' - Used freely.
//!   - 'p' - Used freely.
//!
//! If the 'D' flag is '1' the following payload fields are used by the allocator (and thus cannot be used by the object):
//!
//!   - 'a' - Allocation adjustment (4 bits) - At the moment the field describes how many bytes (shifted) to subtract
//!           from Impl to get the real pointer returned by Impl allocator. Object deallocation relies on this offset.
//!
//! Not all object support all defined flags, here is an overview:
//!
//! ```
//! +---------------------+---+---+---+---+---+---+
//! | Type                |SSO|Dyn|Ext|Imm|Vft|Ref|
//! +---------------------+---+---+---+---+---+---|
//! | BLVar {Null}        | 1 | 0 | 0 | 0 | 0 | 0 | 'SSO' - Small size optimization support
//! | BLVar {Bool}        | 1 | 0 | 0 | 0 | 0 | 0 | 'Dyn' - Dynamic Impl support
//! | BLVar {Int64}       | 1 | 0 | 0 | 0 | 0 | 0 | 'Ext' - External data support
//! | BLVar {UInt64}      | 1 | 0 | 0 | 0 | 0 | 0 | 'Imm' - Immutable data support.
//! | BLVar {Double}      | 1 | 0 | 0 | 0 | 0 | 0 | 'Vft' - Object provides virtual function table.
//! | BLVar {Rgba}        | 1 | 0 | 0 | 0 | 0 | 0 | 'Ref' - Reference counting support.
//! | BLArray<T>          | x | x | x | x | 0 | x |
//! | BLBitSet            | x | x | 0 | 0 | 0 | x |
//! | BLContext           | 0 | 1 | 0 | 0 | 1 | x |
//! | BLString            | x | x | 0 | 0 | 0 | x |
//! | BLPattern           | 0 | 1 | 0 | 0 | 0 | x |
//! | BLGradient          | 0 | 1 | 0 | 0 | 0 | x |
//! | BLPath              | 0 | 1 | 0 | x | 0 | x |
//! | BLImage             | 0 | 1 | x | x | 0 | x |
//! | BLImageCodec        | 0 | 1 | 0 | x | 1 | x |
//! | BLImageDecoder      | 0 | 1 | 0 | 0 | 1 | x |
//! | BLImageEncoder      | 0 | 1 | 0 | 0 | 1 | x |
//! | BLFont              | 0 | 1 | 0 | 0 | 0 | x |
//! | BLFontFace          | 0 | 1 | 0 | x | 1 | x |
//! | BLFontData          | 0 | 1 | x | x | 1 | x |
//! | BLFontManager       | 0 | 1 | 0 | x | 1 | x |
//! +---------------------+---+---+---+---+---+---+
//! ```

//! \addtogroup blend2d_api_object
//! \{

//! \name BLObject - Constants
//! \{

//! \cond INTERNAL
//! Defines a start offset of each field or flag in object info - the shift can be then used to get/set value from/to
//! info bits.
BL_DEFINE_ENUM(BLObjectInfoShift) {
  BL_OBJECT_INFO_REF_COUNTED_SHIFT = 0,
  BL_OBJECT_INFO_IMMUTABLE_SHIFT   = 1,
  BL_OBJECT_INFO_X_SHIFT           = 2,
  BL_OBJECT_INFO_P_SHIFT           = 3,
  BL_OBJECT_INFO_C_SHIFT           = 10,
  BL_OBJECT_INFO_B_SHIFT           = 14,
  BL_OBJECT_INFO_A_SHIFT           = 18,

  BL_OBJECT_INFO_TYPE_SHIFT        = 22,
  BL_OBJECT_INFO_VIRTUAL_SHIFT     = 28,
  BL_OBJECT_INFO_T_MSB_SHIFT       = 29,
  BL_OBJECT_INFO_DYNAMIC_SHIFT     = 30,
  BL_OBJECT_INFO_MARKER_SHIFT      = 31,

  BL_OBJECT_INFO_RC_INIT_SHIFT     = BL_OBJECT_INFO_REF_COUNTED_SHIFT

  BL_FORCE_ENUM_UINT32(BL_OBJECT_INFO_SHIFT)
};
//! \endcond

//! Defines a mask of each field of the object info.
//!
//! \note This is part of the official documentation, however, users should not use these enumerations in any context.
BL_DEFINE_ENUM(BLObjectInfoBits) {
  //! Flag describing a reference counted object, which means it has a valid reference count that must be increased/decreased.
  BL_OBJECT_INFO_REF_COUNTED_FLAG  = 0x01u << BL_OBJECT_INFO_REF_COUNTED_SHIFT, // [........|........|........|.......R]
  //! Flag describing an immutable object, which holds immutable data (immutable data is always external).
  BL_OBJECT_INFO_IMMUTABLE_FLAG    = 0x01u << BL_OBJECT_INFO_IMMUTABLE_SHIFT,   // [........|........|........|......I.]
  //! Flag describing 'X' payload value (it's a payload that has a single bit).
  BL_OBJECT_INFO_X_FLAG            = 0x01u << BL_OBJECT_INFO_X_SHIFT,           // [........|........|........|.....X..]
  //! Mask describing 'P' payload (7 bits).
  BL_OBJECT_INFO_P_MASK            = 0x7Fu << BL_OBJECT_INFO_B_SHIFT,           // [........|........|......pp|ppppp...]
  //! Mask describing 'C' payload (4 bits).
  BL_OBJECT_INFO_C_MASK            = 0x0Fu << BL_OBJECT_INFO_C_SHIFT,           // [........|........|..cccc..|........]
  //! Mask describing 'B' payload (4 bits).
  BL_OBJECT_INFO_B_MASK            = 0x0Fu << BL_OBJECT_INFO_B_SHIFT,           // [........|......bb|bb......|........]
  //! Mask describing 'A' payload (4 bits).
  BL_OBJECT_INFO_A_MASK            = 0x0Fu << BL_OBJECT_INFO_A_SHIFT,           // [........|..aaaa..|........|........]
  //! Mask describing object type (8 bits), see \ref BLObjectType.
  BL_OBJECT_INFO_TYPE_MASK         = 0xFFu << BL_OBJECT_INFO_TYPE_SHIFT,        // [..TVtttt|tt......|........|........]
  //! Flag describing a virtual object.
  BL_OBJECT_INFO_VIRTUAL_FLAG      = 0x01u << BL_OBJECT_INFO_VIRTUAL_SHIFT,     // [...V....|........|........|........]
  //! Flag describing the first most significant bit of \ref BLObjectType.
  BL_OBJECT_INFO_T_MSB_FLAG        = 0x01u << BL_OBJECT_INFO_T_MSB_SHIFT,       // [..T.....|........|........|........]
  //! Flag describing a dynamic object - if this flag is not set, it means the object is in SSO mode.
  BL_OBJECT_INFO_DYNAMIC_FLAG      = 0x01u << BL_OBJECT_INFO_DYNAMIC_SHIFT,     // [.D......|........|........|........]
  //! Flag describing a valid object compatible with \ref BLObjectCore interface (otherwise it's most likely \ref BLRgba).
  BL_OBJECT_INFO_MARKER_FLAG       = 0x01u << BL_OBJECT_INFO_MARKER_SHIFT,      // [M.......|........|........|........]
  //! Reference count initializer (combines \ref BL_OBJECT_INFO_REF_COUNTED_FLAG and \ref BL_OBJECT_INFO_IMMUTABLE_FLAG).
  BL_OBJECT_INFO_RC_INIT_MASK      = 0x03u << BL_OBJECT_INFO_RC_INIT_SHIFT      // [........|........|........|......**]

  BL_FORCE_ENUM_UINT32(BL_OBJECT_INFO_BITS)
};

//! Object type identifier.
BL_DEFINE_ENUM(BLObjectType) {
  //! Object represents a RGBA value stored as 4 32-bit floating point components (can be used as style).
  BL_OBJECT_TYPE_RGBA = 0,
  //! Object is `Null` (can be used as style).
  BL_OBJECT_TYPE_NULL = 1,
  //! Object is `BLPattern` (style compatible).
  BL_OBJECT_TYPE_PATTERN = 2,
  //! Object is `BLGradient` (style compatible).
  BL_OBJECT_TYPE_GRADIENT = 3,

  //! Object is `BLImage`.
  BL_OBJECT_TYPE_IMAGE = 9,
  //! Object is `BLPath`.
  BL_OBJECT_TYPE_PATH = 10,

  //! Object is `BLFont`.
  BL_OBJECT_TYPE_FONT = 16,
  //! Object is `BLFontFeatureOptions`.
  BL_OBJECT_TYPE_FONT_FEATURE_OPTIONS = 17,
  //! Object is `BLFontVariationOptions`.
  BL_OBJECT_TYPE_FONT_VARIATION_OPTIONS = 18,

  //! Object represents a boolean value.
  BL_OBJECT_TYPE_BOOL = 28,
  //! Object represents a 64-bit signed integer value.
  BL_OBJECT_TYPE_INT64 = 29,
  //! Object represents a 64-bit unsigned integer value.
  BL_OBJECT_TYPE_UINT64 = 30,
  //! Object represents a 64-bit floating point value.
  BL_OBJECT_TYPE_DOUBLE = 31,
  //! Object is `BLString`.
  BL_OBJECT_TYPE_STRING = 32,

  //! Object is `BLArray<T>` where `T` is a `BLObject` compatible type.
  BL_OBJECT_TYPE_ARRAY_OBJECT = 33,
  //! Object is `BLArray<T>` where `T` matches 8-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT8 = 34,
  //! Object is `BLArray<T>` where `T` matches 8-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT8 = 35,
  //! Object is `BLArray<T>` where `T` matches 16-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT16 = 36,
  //! Object is `BLArray<T>` where `T` matches 16-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT16 = 37,
  //! Object is `BLArray<T>` where `T` matches 32-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT32 = 38,
  //! Object is `BLArray<T>` where `T` matches 32-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT32 = 39,
  //! Object is `BLArray<T>` where `T` matches 64-bit signed integral type.
  BL_OBJECT_TYPE_ARRAY_INT64 = 40,
  //! Object is `BLArray<T>` where `T` matches 64-bit unsigned integral type.
  BL_OBJECT_TYPE_ARRAY_UINT64 = 41,
  //! Object is `BLArray<T>` where `T` matches 32-bit floating point type.
  BL_OBJECT_TYPE_ARRAY_FLOAT32 = 42,
  //! Object is `BLArray<T>` where `T` matches 64-bit floating point type.
  BL_OBJECT_TYPE_ARRAY_FLOAT64 = 43,
  //! Object is `BLArray<T>` where `T` is a struct of size 1.
  BL_OBJECT_TYPE_ARRAY_STRUCT_1 = 44,
  //! Object is `BLArray<T>` where `T` is a struct of size 2.
  BL_OBJECT_TYPE_ARRAY_STRUCT_2 = 45,
  //! Object is `BLArray<T>` where `T` is a struct of size 3.
  BL_OBJECT_TYPE_ARRAY_STRUCT_3 = 46,
  //! Object is `BLArray<T>` where `T` is a struct of size 4.
  BL_OBJECT_TYPE_ARRAY_STRUCT_4 = 47,
  //! Object is `BLArray<T>` where `T` is a struct of size 6.
  BL_OBJECT_TYPE_ARRAY_STRUCT_6 = 48,
  //! Object is `BLArray<T>` where `T` is a struct of size 8.
  BL_OBJECT_TYPE_ARRAY_STRUCT_8 = 49,
  //! Object is `BLArray<T>` where `T` is a struct of size 10.
  BL_OBJECT_TYPE_ARRAY_STRUCT_10 = 50,
  //! Object is `BLArray<T>` where `T` is a struct of size 12.
  BL_OBJECT_TYPE_ARRAY_STRUCT_12 = 51,
  //! Object is `BLArray<T>` where `T` is a struct of size 16.
  BL_OBJECT_TYPE_ARRAY_STRUCT_16 = 52,
  //! Object is `BLArray<T>` where `T` is a struct of size 20.
  BL_OBJECT_TYPE_ARRAY_STRUCT_20 = 53,
  //! Object is `BLArray<T>` where `T` is a struct of size 24.
  BL_OBJECT_TYPE_ARRAY_STRUCT_24 = 54,
  //! Object is `BLArray<T>` where `T` is a struct of size 32.
  BL_OBJECT_TYPE_ARRAY_STRUCT_32 = 55,

  //! Object is `BLContext`.
  BL_OBJECT_TYPE_CONTEXT = 64,

  //! Object is `BLImageCodec`.
  BL_OBJECT_TYPE_IMAGE_CODEC = 65,
  //! Object is `BLImageDecoder`.
  BL_OBJECT_TYPE_IMAGE_DECODER = 66,
  //! Object is `BLImageEncoder`.
  BL_OBJECT_TYPE_IMAGE_ENCODER = 67,

  //! Object is `BLFontFace`.
  BL_OBJECT_TYPE_FONT_FACE = 68,
  //! Object is `BLFontData`.
  BL_OBJECT_TYPE_FONT_DATA = 69,
  //! Object is `BLFontManager`.
  BL_OBJECT_TYPE_FONT_MANAGER = 70,

  //! Object is `BLBitSet`.
  BL_OBJECT_TYPE_BIT_SET = 128,

  BL_OBJECT_TYPE_ARRAY_FIRST = 33,
  BL_OBJECT_TYPE_ARRAY_LAST = 55,

  //! Maximum object type identifier that can be used as a style.
  BL_OBJECT_TYPE_MAX_STYLE_VALUE = 3,

  //! Maximum possible value of an object type, including identifiers reserved for the future.
  BL_OBJECT_TYPE_MAX_VALUE = 128

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
    //! Signature of a SSO BitSet, which is in Range mode.
    kSignatureSSOBitSetRange = (BL_OBJECT_INFO_MARKER_FLAG) |
                               (BL_OBJECT_TYPE_BIT_SET << BL_OBJECT_INFO_TYPE_SHIFT) |
                               (0xFFFFFFFFu >> 5)
  };

  //! \}

  //! \name Static Methods for Packing & Unpacking
  //! \{

  //! Packs object type into object info bits.
  static BL_INLINE constexpr BLObjectInfo packType(BLObjectType type) noexcept {
    return BLObjectInfo{uint32_t(type) << BL_OBJECT_INFO_TYPE_SHIFT};
  }

  //! Packs A, B, C, and P fields so they can be combined with other object info bits.
  static BL_INLINE constexpr BLObjectInfo packFields(uint32_t aField, uint32_t bField = 0u, uint32_t cField = 0u, uint32_t pField = 0u) noexcept {
    return BLObjectInfo {
      (aField << BL_OBJECT_INFO_A_SHIFT) |
      (bField << BL_OBJECT_INFO_B_SHIFT) |
      (cField << BL_OBJECT_INFO_C_SHIFT) |
      (pField << BL_OBJECT_INFO_P_SHIFT)
    };
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE constexpr BLObjectInfo operator|(const BLObjectInfo& v) const noexcept { return BLObjectInfo{bits | v.bits}; }
  BL_INLINE constexpr BLObjectInfo operator&(const BLObjectInfo& v) const noexcept { return BLObjectInfo{bits & v.bits}; }
  BL_INLINE constexpr BLObjectInfo operator^(const BLObjectInfo& v) const noexcept { return BLObjectInfo{bits ^ v.bits}; }

  BL_INLINE constexpr BLObjectInfo operator|(const BLObjectInfoBits& v) const noexcept { return BLObjectInfo{bits | v}; }
  BL_INLINE constexpr BLObjectInfo operator&(const BLObjectInfoBits& v) const noexcept { return BLObjectInfo{bits & v}; }
  BL_INLINE constexpr BLObjectInfo operator^(const BLObjectInfoBits& v) const noexcept { return BLObjectInfo{bits ^ v}; }

  BL_INLINE BLObjectInfo& operator|=(const BLObjectInfo& v) noexcept { bits |= v.bits; return *this; }
  BL_INLINE BLObjectInfo& operator&=(const BLObjectInfo& v) noexcept { bits &= v.bits; return *this; }
  BL_INLINE BLObjectInfo& operator^=(const BLObjectInfo& v) noexcept { bits ^= v.bits; return *this; }

  BL_INLINE BLObjectInfo& operator|=(const BLObjectInfoBits& v) noexcept { bits |= v; return *this; }
  BL_INLINE BLObjectInfo& operator&=(const BLObjectInfoBits& v) noexcept { bits &= v; return *this; }
  BL_INLINE BLObjectInfo& operator^=(const BLObjectInfoBits& v) noexcept { bits ^= v; return *this; }

  //! \}

  //! \name Info Data Accessors - Generic
  //! \{

  //! Extracts a field based on Shift and Mask.
  //!
  //! \note It doesn't verify whether the object info is valid, it just extracts the field.
  template<uint32_t Shift, uint32_t Mask>
  BL_INLINE uint32_t getField() const noexcept { return (bits >> Shift) & (Mask >> Shift); }

  template<uint32_t Shift, uint32_t Mask>
  BL_INLINE void setField(uint32_t value) noexcept { bits = (bits & ~Mask) | (value << Shift); }

  BL_INLINE bool sso() const noexcept { return (bits & BL_OBJECT_INFO_DYNAMIC_FLAG) == 0; }
  BL_INLINE bool dynamicFlag() const noexcept { return (bits & BL_OBJECT_INFO_DYNAMIC_FLAG) != 0; }

  BL_INLINE bool xFlag() const noexcept { return (bits & BL_OBJECT_INFO_X_FLAG) != 0; }
  BL_INLINE bool virtualFlag() const noexcept { return (bits & BL_OBJECT_INFO_VIRTUAL_FLAG) != 0; }
  BL_INLINE bool immutableFlag() const noexcept { return (bits & BL_OBJECT_INFO_IMMUTABLE_FLAG) != 0; }
  BL_INLINE bool refCountedFlag() const noexcept { return (bits & BL_OBJECT_INFO_REF_COUNTED_FLAG) != 0; }

  BL_INLINE uint32_t aField() const noexcept { return getField<BL_OBJECT_INFO_A_SHIFT, BL_OBJECT_INFO_A_MASK>(); }
  BL_INLINE uint32_t bField() const noexcept { return getField<BL_OBJECT_INFO_B_SHIFT, BL_OBJECT_INFO_B_MASK>(); }
  BL_INLINE uint32_t cField() const noexcept { return getField<BL_OBJECT_INFO_C_SHIFT, BL_OBJECT_INFO_C_MASK>(); }
  BL_INLINE uint32_t pField() const noexcept { return getField<BL_OBJECT_INFO_P_SHIFT, BL_OBJECT_INFO_P_MASK>(); }

  BL_INLINE void setAField(uint32_t value) noexcept { setField<BL_OBJECT_INFO_A_SHIFT, BL_OBJECT_INFO_A_MASK>(value); }
  BL_INLINE void setBField(uint32_t value) noexcept { setField<BL_OBJECT_INFO_B_SHIFT, BL_OBJECT_INFO_B_MASK>(value); }
  BL_INLINE void setCField(uint32_t value) noexcept { setField<BL_OBJECT_INFO_C_SHIFT, BL_OBJECT_INFO_C_MASK>(value); }
  BL_INLINE void setPField(uint32_t value) noexcept { setField<BL_OBJECT_INFO_P_SHIFT, BL_OBJECT_INFO_P_MASK>(value); }

  //! \}

  //! \name Object Signature Accessors
  //! \{

  //! Tests whether BLObjectInfo describes a valid BLObject and verifies that `additionalBits` match the
  //! given `mask` in BLObjectInfo bits as well. This function is a higher-level function used by others.
  BL_INLINE bool hasObjectSignatureAndFlags(uint32_t mask, uint32_t additionalBits) const noexcept {
    return (bits & (BL_OBJECT_INFO_MARKER_FLAG | mask)) == (BL_OBJECT_INFO_MARKER_FLAG | additionalBits);
  }

  //! Tests whether BLObjectInfo describes a valid BLObject and verifies the the given `flags` are all set.
  BL_INLINE bool hasObjectSignatureAndFlags(uint32_t flags) const noexcept {
    return hasObjectSignatureAndFlags(flags, flags);
  }

  //! Tests whether the object info represents a valid BLObject signature.
  //!
  //! A valid signature describes a \ref BLObjectCore and not an alternative representation used by \ref BLRgba data.
  BL_INLINE bool hasObjectSignature() const noexcept { return hasObjectSignatureAndFlags(0u); }

  //! Tests whether BLObjectInfo describes a valid BLObject of the given `type`.
  BL_INLINE bool checkObjectSignatureAndRawType(BLObjectType type) const noexcept {
    return hasObjectSignatureAndFlags(uint32_t(type) << BL_OBJECT_INFO_TYPE_SHIFT);
  }

  //! \}

  //! \name Object Type Accessors
  //! \{

  //! Tests a whether this \ref BLObjectInfo represents a valid \ref BLObjectCore.
  BL_INLINE bool isObject() const noexcept { return uint32_t((int32_t(bits) >> 31)); }

  //! Returns a whether this \ref BLObjectInfo represents a valid \ref BLObjectCore as a mask (either all zeros or all ones).
  BL_INLINE uint32_t isObjectMask() const noexcept { return uint32_t((int32_t(bits) >> 31)); }

  //! Tests whether the object info represents a valid BLObject, which has a valid Impl field.
  BL_INLINE bool isDynamicObject() const noexcept { return hasObjectSignatureAndFlags(BL_OBJECT_INFO_DYNAMIC_FLAG); }

  //! Tests whether the object info represents a valid BLObject, which has a valid Impl, and has a virtual function table.
  BL_INLINE bool isVirtualObject() const noexcept { return hasObjectSignatureAndFlags(BL_OBJECT_INFO_DYNAMIC_FLAG | BL_OBJECT_INFO_VIRTUAL_FLAG); }

  //! Tests whether the object info represents a valid BLObject, which is reference counted.
  //!
  //! \note reference counted object means that it has a valid Impl (implies \ref BL_OBJECT_INFO_DYNAMIC_FLAG flag).
  BL_INLINE bool isRefCountedObject() const noexcept { return hasObjectSignatureAndFlags(BL_OBJECT_INFO_DYNAMIC_FLAG | BL_OBJECT_INFO_REF_COUNTED_FLAG); }

  //! Returns a RAW \ref BLObjectType read from object info bits without checking for a valid signature.
  //!
  //! This function should only be used in case that the caller knows that the object info is of a valid \ref
  //! BLObjectCore and knows how to handle \ref BLBitSet.
  BL_INLINE BLObjectType rawType() const noexcept { return BLObjectType(getField<BL_OBJECT_INFO_TYPE_SHIFT, BL_OBJECT_INFO_TYPE_MASK>()); }

  //! Returns a corrected \ref BLObjectType read from object info bits.
  //!
  //! The value returned is corrected so the returned value has no special cases to consider.
  BL_INLINE BLObjectType getType() const noexcept { return BLObjectType(blMin(rawType(), BL_OBJECT_TYPE_MAX_VALUE) & isObjectMask()); }

  //! Tests whether the object info represents a `BLArray<T>` storing any supported type.
  BL_INLINE bool isArray() const noexcept { return getType() >= BL_OBJECT_TYPE_ARRAY_FIRST && getType() <= BL_OBJECT_TYPE_ARRAY_LAST; }
  //! Tests whether the object info represents a `BLBitSet`.
  BL_INLINE bool isBitSet() const noexcept { return hasObjectSignatureAndFlags(BL_OBJECT_INFO_T_MSB_FLAG); }
  //! Tests whether the object info represents a boxed `bool` value.
  BL_INLINE bool isBool() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_BOOL); }
  //! Tests whether the object info represents `BLContext`.
  BL_INLINE bool isContext() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_CONTEXT); }
  //! Tests whether the object info represents a boxed `double` value.
  BL_INLINE bool isDouble() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_DOUBLE); }
  //! Tests whether the object info represents `BLFont`.
  BL_INLINE bool isFont() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_FONT); }
  //! Tests whether the object info represents `BLFontData`.
  BL_INLINE bool isFontData() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_FONT_DATA); }
  //! Tests whether the object info represents `BLFontFace`.
  BL_INLINE bool isFontFace() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_FONT_FACE); }
  //! Tests whether the object info represents `BLFontManager`.
  BL_INLINE bool isFontManager() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_FONT_MANAGER); }
  //! Tests whether the object info represents `BLGradient`.
  BL_INLINE bool isGradient() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_GRADIENT); }
  //! Tests whether the object info represents `BLImage`.
  BL_INLINE bool isImage() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_IMAGE); }
  //! Tests whether the object info represents `BLImageCodec`.
  BL_INLINE bool isImageCodec() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_IMAGE_CODEC); }
  //! Tests whether the object info represents `BLImageDecoder`.
  BL_INLINE bool isImageDecoder() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_IMAGE_DECODER); }
  //! Tests whether the object info represents `BLImageEncoder`.
  BL_INLINE bool isImageEncoder() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_IMAGE_ENCODER); }
  //! Tests whether the object info represents a boxed `int64_t` value.
  BL_INLINE bool isInt64() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_INT64); }
  //! Tests whether the object info represents a null value.
  BL_INLINE bool isNull() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_NULL); }
  //! Tests whether the object info represents `BLPath`.
  BL_INLINE bool isPath() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_PATH); }
  //! Tests whether the object info represents `BLPattern.`
  BL_INLINE bool isPattern() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_PATTERN); }
  //! Tests whether the object info represents `BLRgba`.
  BL_INLINE bool isRgba() const noexcept { return !isObject(); }
  //! Tests whether the object info represents `BLString`.
  BL_INLINE bool isString() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_STRING); }
  //! Tests whether the object info represents a boxed `uint64_t` value.
  BL_INLINE bool isUInt64() const noexcept { return checkObjectSignatureAndRawType(BL_OBJECT_TYPE_UINT64); }

  //! Tests whether the object info represents a style that can be passed to the rendering context.
  BL_INLINE bool isStyle() const noexcept { return getType() <= BL_OBJECT_TYPE_MAX_STYLE_VALUE; }

  // \}

  //! \cond INTERNAL
  //! \name Object Type Accessors - Internals
  //! \{

  //! Tests whether the object info represents a `BLBitSet`, which is in SSO range mode.
  //!
  //! \note An empty SSO range [0, 0) is used by default constructed BitSets.
  BL_INLINE bool isBitSetRange() const noexcept { return bits == kSignatureSSOBitSetRange; }

  //! \}
  //! \endcond
#endif
};

//! Defines a BLObject layout that all objects must use.
union BLObjectDetail {
  void* impl;

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

  struct {
    uint32_t u32_data_overlap[2];
    uint32_t impl_payload;
    BLObjectInfo info;
  };

#ifdef __cplusplus
  //! \name Constants
  //! \{

  enum : uint32_t {
    //! Size of object static storage not considring \ref BLObjectInfo.
    kStaticDataSize = 12
  };

  //! \}

  //! \name Initialization
  //! \{

  //! Initializes this BLObjectDetail with object that uses static storage.
  BL_INLINE void initStatic(BLObjectType objectType, BLObjectInfo objectInfo = BLObjectInfo{0}) noexcept {
    u64_data[0] = 0;
    u32_data[2] = 0;
    info = BLObjectInfo::packType(objectType) | objectInfo | BL_OBJECT_INFO_MARKER_FLAG;
  }

  //! Initializes this BLObjectDetail with object that uses dynamic storage (Impl).
  BL_INLINE void initDynamic(BLObjectType objectType, BLObjectInfo objectInfo, void* implInit) noexcept {
    u64_data[0] = 0;
    impl = implInit;

    u32_data[2] = 0;
    info = BLObjectInfo::packType(objectType) | objectInfo | BL_OBJECT_INFO_MARKER_FLAG | BL_OBJECT_INFO_DYNAMIC_FLAG;
  }

  BL_INLINE void initNull() noexcept {
    u64_data[0] = 0;
    u32_data[2] = 0;
    info = BLObjectInfo::packType(BL_OBJECT_TYPE_NULL) | BL_OBJECT_INFO_MARKER_FLAG;
  }

  BL_INLINE void initBool(bool value) noexcept {
    u64_data[0] = uint64_t(value);
    u32_data[2] = 0;
    info = BLObjectInfo::packType(BL_OBJECT_TYPE_BOOL) | BL_OBJECT_INFO_MARKER_FLAG;
  }

  BL_INLINE void initInt64(int64_t value) noexcept {
    u64_data[0] = uint64_t(value);
    u32_data[2] = 0;
    info = BLObjectInfo::packType(BL_OBJECT_TYPE_INT64) | BL_OBJECT_INFO_MARKER_FLAG;
  }

  BL_INLINE void initUInt64(uint64_t value) noexcept {
    u64_data[0] = value;
    u32_data[2] = 0;
    info = BLObjectInfo::packType(BL_OBJECT_TYPE_UINT64) | BL_OBJECT_INFO_MARKER_FLAG;
  }

  BL_INLINE void initDouble(double value) noexcept {
    f64_data[0] = value;
    u32_data[2] = 0;
    info = BLObjectInfo::packType(BL_OBJECT_TYPE_DOUBLE) | BL_OBJECT_INFO_MARKER_FLAG;
  }

  BL_INLINE void initU32x4(uint32_t u0, uint32_t u1, uint32_t u2, uint32_t u3) noexcept {
    u32_data[0] = u0;
    u32_data[1] = u1;
    u32_data[2] = u2;
    u32_data[3] = u3;
  }

  BL_INLINE void initF32x4(float f0, float f1, float f2, float f3) noexcept {
    f32_data[0] = f0;
    f32_data[1] = f1;
    f32_data[2] = f2;
    f32_data[3] = f3;
  }

  BL_INLINE void clearStaticData() noexcept {
    u64_data[0] = 0;
    u32_data[2] = 0;
  }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Swaps this BLObjectDetail with `other`.
  BL_INLINE void swap(BLObjectDetail& other) noexcept {
    BLObjectDetail a = *this;
    BLObjectDetail b = other;

    *this = b;
    other = a;
  }

  //! \}

  //! \name Object Data Accessors
  //! \{

  template<typename T>
  BL_INLINE T* dataAs() noexcept { return reinterpret_cast<T*>(char_data); }

  template<typename T>
  BL_INLINE const T* dataAs() const noexcept { return reinterpret_cast<const T*>(char_data); }

  //! \}

  //! \name Object Info Accessors - Generic
  //! \{

  BL_INLINE bool sso() const noexcept { return info.sso(); }
  BL_INLINE bool dynamicFlag() const noexcept { return info.dynamicFlag(); }

  BL_INLINE bool xFlag() const noexcept { return info.xFlag(); }
  BL_INLINE bool virtualFlag() const noexcept { return info.virtualFlag(); }
  BL_INLINE bool immutableFlag() const noexcept { return info.immutableFlag(); }
  BL_INLINE bool refCountedFlag() const noexcept { return info.refCountedFlag(); }

  BL_INLINE uint32_t aField() const noexcept { return info.aField(); }
  BL_INLINE uint32_t bField() const noexcept { return info.bField(); }
  BL_INLINE uint32_t cField() const noexcept { return info.cField(); }
  BL_INLINE uint32_t pField() const noexcept { return info.pField(); }

  //! \}

  //! \name Object Data Accessors - BitSet
  //! \{

  BL_INLINE bool isBitSetRange() const noexcept { return info.isBitSetRange(); }

  //! \}

  //! \name Object Type Accessors
  //! \{

  //! Tests whether the object info of this BLObjectDetail contains a valid BLObject signature.
  BL_INLINE bool hasObjectSignature() const noexcept { return info.hasObjectSignature(); }

  //! Tests whether the object info of this BLObjectDetail contains a valid BLObject, which has a valid Impl field.
  BL_INLINE bool isDynamicObject() const noexcept { return info.isDynamicObject(); }
  //! Tests whether the object info of this BLObjectDetail represents a valid BLObject, with Impl and Virtual function table.
  BL_INLINE bool isVirtualObject() const noexcept { return info.isVirtualObject(); }

  //! Tests whether the object info of this BLObjectDetail contains a valid BLObject, which is reference counted.
  BL_INLINE bool isRefCountedObject() const noexcept { return info.isRefCountedObject(); }

  //! Returns a RAW type read from object info data.
  BL_INLINE BLObjectType rawType() const noexcept { return info.rawType(); }
  //! Returns the type of this object.
  BL_INLINE BLObjectType getType() const noexcept { return info.getType(); }

  //! Tests whether this BLObjectDetail represents a `BLArray<T>` storing any supported type.
  BL_INLINE bool isArray() const noexcept { return info.isArray(); }
  //! Tests whether this BLObjectDetail represents a `BLBitSet`.
  BL_INLINE bool isBitSet() const noexcept { return info.isBitSet(); }
  //! Tests whether this BLObjectDetail represents a boxed `bool` value.
  BL_INLINE bool isBool() const noexcept { return info.isBool(); }
  //! Tests whether this BLObjectDetail represents `BLContext`.
  BL_INLINE bool isContext() const noexcept { return info.isContext(); }
  //! Tests whether this BLObjectDetail represents a boxed `double` value.
  BL_INLINE bool isDouble() const noexcept { return info.isDouble(); }
  //! Tests whether this BLObjectDetail represents `BLFont`.
  BL_INLINE bool isFont() const noexcept { return info.isFont(); }
  //! Tests whether this BLObjectDetail represents `BLFontData`.
  BL_INLINE bool isFontData() const noexcept { return info.isFontData(); }
  //! Tests whether this BLObjectDetail represents `BLFontFace`.
  BL_INLINE bool isFontFace() const noexcept { return info.isFontFace(); }
  //! Tests whether this BLObjectDetail represents `BLFontManager`.
  BL_INLINE bool isFontManager() const noexcept { return info.isFontManager(); }
  //! Tests whether this BLObjectDetail represents `BLGradient`.
  BL_INLINE bool isGradient() const noexcept { return info.isGradient(); }
  //! Tests whether this BLObjectDetail represents `BLImage`.
  BL_INLINE bool isImage() const noexcept { return info.isImage(); }
  //! Tests whether this BLObjectDetail represents `BLImageCodec`.
  BL_INLINE bool isImageCodec() const noexcept { return info.isImageCodec(); }
  //! Tests whether this BLObjectDetail represents `BLImageDecoder`.
  BL_INLINE bool isImageDecoder() const noexcept { return info.isImageDecoder(); }
  //! Tests whether this BLObjectDetail represents `BLImageEncoder`.
  BL_INLINE bool isImageEncoder() const noexcept { return info.isImageEncoder(); }
  //! Tests whether this BLObjectDetail represents a boxed `int64_t` value.
  BL_INLINE bool isInt64() const noexcept { return info.isInt64(); }
  //! Tests whether this BLObjectDetail represents a null value.
  BL_INLINE bool isNull() const noexcept { return info.isNull(); }
  //! Tests whether this BLObjectDetail represents `BLPath`.
  BL_INLINE bool isPath() const noexcept { return info.isPath(); }
  //! Tests whether this BLObjectDetail represents `BLPattern.`
  BL_INLINE bool isPattern() const noexcept { return info.isPattern(); }
  //! Tests whether this BLObjectDetail represents boxed `BLRgba`.
  BL_INLINE bool isRgba() const noexcept { return info.isRgba(); }
  //! Tests whether this BLObjectDetail represents `BLString`.
  BL_INLINE bool isString() const noexcept { return info.isString(); }
  //! Tests whether this BLObjectDetail represents a boxed `uint64_t` value.
  BL_INLINE bool isUInt64() const noexcept { return info.isUInt64(); }

  //! Tests whether this BLObjectDetail represents a style that can be passed to the rendering context.
  BL_INLINE bool isStyle() const noexcept { return info.isStyle(); }

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

//! Provides information necessary to release external data that Impl holds. This information is purely optional.
//! If present, the `destroy()` function will be called when the BLObjectImpl's reference count goes to zero. If
//! not present, it acts like the destroy() function did nothing (it just won't call it).
struct BLObjectExternalInfo {
  //! Destroy callback to be called when Impl holding the external data is being destroyed.
  BLDestroyExternalDataFunc destroyFunc;
  //! Data provided by the user to identify the external data, passed to destroyFunc() as `userData`.
  void* userData;
};

//! \}

//! \name BLObject - C API
//! \{

BL_BEGIN_C_DECLS

BL_API void* BL_CDECL blObjectDetailAllocImpl(BLObjectDetail* d, uint32_t info, size_t implSize, size_t* implSizeOut) BL_NOEXCEPT_C;
BL_API void* BL_CDECL blObjectDetailAllocImplExternal(BLObjectDetail* d, uint32_t info, size_t implSize, BLObjectExternalInfo** externalInfoOut, void** externalOptDataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blObjectDetailFreeImpl(void* impl, uint32_t info) BL_NOEXCEPT_C;

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

//! BLObject [Impl].
//!
//! This is only used to model inheritance when compiled by a C++ compiler.
#ifdef __cplusplus
struct BLObjectImpl {
  //! \name Common Functionality
  //! \{

  //! Casts this Impl to `T*`.
  template<typename T>
  BL_INLINE T* as() noexcept { return static_cast<T*>(this); }

  //! Casts this Impl to `T*` (const).
  template<typename T>
  BL_INLINE const T* as() const noexcept { return static_cast<const T*>(this); }

  //! \}
};
#endif

//! Base members of \ref BLObjectVirt.
//!
//! The reason for this struct is to make C API the same as C++ API in terms of struct members. In C++ mode we use
//! inheritance so `Virt` structs actually inherit from \ref BLObjectVirt, but in every case all base members are
//! provided by `base`.
struct BLObjectVirtBase {
  BLResult (BL_CDECL* destroy)(BLObjectImpl* impl, uint32_t info) BL_NOEXCEPT;
  BLResult (BL_CDECL* getProperty)(const BLObjectImpl* impl, const char* name, size_t nameSize, BLVarCore* valueOut) BL_NOEXCEPT;
  BLResult (BL_CDECL* setProperty)(BLObjectImpl* impl, const char* name, size_t nameSize, const BLVarCore* value) BL_NOEXCEPT;
};

//! BLObject [Virtual Function Table].
//!
//! Virtual function table is only present when BLObjectDetail has `BL_OBJECT_INFO_VIRTUAL_FLAG` set. Objects can
//! extend the function table, but it has to always start with members defined by `BLObjectVirt`.
struct BLObjectVirt {
  BLObjectVirtBase base;
};

//! Base class used by all Blend2D objects.
struct BLObjectCore {
  BLObjectDetail _d;
};

//! \}

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
  #define BL_DEFINE_VIRT_BASE
  #define BL_DEFINE_OBJECT_PROPERTY_API                                                            \
    /** Gets a property of the given `name` and assigns it to an initialized `valueOut`. */        \
    BL_INLINE BLResult getProperty(const char* name, BLVarCore& valueOut) const noexcept {         \
      return blObjectGetProperty(this, name, SIZE_MAX, &valueOut);                                 \
    }                                                                                              \
                                                                                                   \
    /** \overload */                                                                               \
    BL_INLINE BLResult getProperty(const BLStringView& name, BLVarCore& valueOut) const noexcept { \
      return blObjectGetProperty(this, name.data, name.size, &valueOut);                           \
    }                                                                                              \
                                                                                                   \
    /** Sets a property of the given `name` to `value`. */                                         \
    BL_INLINE BLResult setProperty(const char* name, const BLObjectCore& value) noexcept {         \
      return blObjectSetProperty(this, name, SIZE_MAX, &value);                                    \
    }                                                                                              \
                                                                                                   \
    /** \overload */                                                                               \
    BL_INLINE BLResult setProperty(const BLStringView& name, const BLObjectCore& value) noexcept { \
      return blObjectSetProperty(this, name.data, name.size, &value);                              \
    }
#else
  #define BL_CLASS_INHERITS(BASE)
  #define BL_DEFINE_OBJECT_DETAIL BLObjectDetail _d;
  #define BL_DEFINE_VIRT_BASE BLObjectVirtBase base;
  #define BL_DEFINE_OBJECT_PROPERTY_API
#endif

//! \}

#endif // BLEND2D_OBJECT_H_INCLUDED
