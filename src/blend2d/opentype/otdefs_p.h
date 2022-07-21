// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OTDEFS_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTDEFS_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../fontdata_p.h"
#include "../fontface_p.h"
#include "../support/memops_p.h"
#include "../support/ptrops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

//! \namespace BLOpenType
//! Low-level OpenType functionality, not exposed to users directly.

namespace BLOpenType {

struct OTFaceImpl;

//! A range that specifies offset and size of a data table or some part of it.
struct DataRange {
  uint32_t offset;
  uint32_t size;

  BL_INLINE void reset() noexcept { reset(0, 0); }
  BL_INLINE void reset(uint32_t offset_, uint32_t size_) noexcept {
    this->offset = offset_;
    this->size = size_;
  }
};

template<size_t Size>
struct DataAccess {};

template<>
struct DataAccess<1> {
  template<uint32_t ByteOrder, size_t Alignment>
  static BL_INLINE uint32_t readValue(const uint8_t* data) noexcept { return BLMemOps::readU8(data); }

  template<uint32_t ByteOrder, size_t Alignment>
  static BL_INLINE void writeValue(uint8_t* data, uint32_t value) noexcept { BLMemOps::writeU8(data, value); }
};

template<>
struct DataAccess<2> {
  template<uint32_t ByteOrder, size_t Alignment>
  static BL_INLINE uint32_t readValue(const uint8_t* data) noexcept { return BLMemOps::readU16<ByteOrder, Alignment>(data); }

  template<uint32_t ByteOrder, size_t Alignment>
  static BL_INLINE void writeValue(uint8_t* data, uint32_t value) noexcept { BLMemOps::writeU16<ByteOrder, Alignment>(data, value); }
};

template<>
struct DataAccess<3> {
  template<uint32_t ByteOrder, size_t Alignment>
  static BL_INLINE uint32_t readValue(const uint8_t* data) noexcept { return BLMemOps::readU24u<ByteOrder>(data); }

  template<uint32_t ByteOrder, size_t Alignment>
  static BL_INLINE void writeValue(uint8_t* data, uint32_t value) noexcept { BLMemOps::writeU24u<ByteOrder>(data, value); }
};

template<>
struct DataAccess<4> {
  template<uint32_t ByteOrder, size_t Alignment>
  static BL_INLINE uint32_t readValue(const uint8_t* data) noexcept { return BLMemOps::readU32<ByteOrder, Alignment>(data); }

  template<uint32_t ByteOrder, size_t Alignment>
  static BL_INLINE void writeValue(uint8_t* data, uint32_t value) noexcept { BLMemOps::writeU32<ByteOrder, Alignment>(data, value); }
};

template<>
struct DataAccess<8> {
  template<uint32_t ByteOrder, size_t Alignment>
  static BL_INLINE uint64_t readValue(const uint8_t* data) noexcept { return BLMemOps::readU64<ByteOrder, Alignment>(data); }

  template<uint32_t ByteOrder, size_t Alignment>
  static BL_INLINE void writeValue(uint8_t* data, uint64_t value) noexcept { BLMemOps::writeU64<ByteOrder, Alignment>(data, value); }
};

#pragma pack(push, 1)
template<typename T, uint32_t ByteOrder, size_t Size>
struct DataType {
  uint8_t data[Size];

  BL_INLINE DataType(const DataType& other) noexcept = default;

  template<size_t Alignment = 1>
  BL_INLINE T value() const noexcept { return T(DataAccess<Size>::template readValue<ByteOrder, Alignment>(data));  }

  template<size_t Alignment = 1>
  BL_INLINE T rawValue() const noexcept { return T(DataAccess<Size>::template readValue<BL_BYTE_ORDER_NATIVE, Alignment>(data)); }

  template<size_t Alignment = 1>
  BL_INLINE void setValue(T value) noexcept {
    typedef typename std::make_unsigned<T>::type U;
    DataAccess<Size>::template writeValue<ByteOrder, Alignment>(data, U(value));
  }

  template<size_t Alignment = 1>
  BL_INLINE void setRawValue(T value) noexcept {
    typedef typename std::make_unsigned<T>::type U;
    DataAccess<Size>::template writeValue<BL_BYTE_ORDER_NATIVE, Alignment>(data, U(value));
  }

  BL_INLINE T operator()() const noexcept { return value(); }

  BL_INLINE DataType& operator=(const DataType& other) noexcept = default;
  BL_INLINE DataType& operator=(T other) noexcept { setValue(other); return *this; }
};
#pragma pack(pop)

// Everything in OpenType is big-endian.
typedef DataType<int8_t  , BL_BYTE_ORDER_BE, 1> Int8;
typedef DataType<int16_t , BL_BYTE_ORDER_BE, 2> Int16;
typedef DataType<int32_t , BL_BYTE_ORDER_BE, 4> Int32;
typedef DataType<int64_t , BL_BYTE_ORDER_BE, 8> Int64;

typedef DataType<uint8_t , BL_BYTE_ORDER_BE, 1> UInt8;
typedef DataType<uint16_t, BL_BYTE_ORDER_BE, 2> UInt16;
typedef DataType<uint32_t, BL_BYTE_ORDER_BE, 3> UInt24;
typedef DataType<uint32_t, BL_BYTE_ORDER_BE, 4> UInt32;
typedef DataType<uint64_t, BL_BYTE_ORDER_BE, 8> UInt64;

typedef UInt16 Offset16;
typedef UInt32 Offset32;

typedef Int16 FWord;
typedef UInt16 UFWord;
typedef UInt16 F2x14;
typedef UInt32 F16x16;
typedef UInt32 CheckSum;
typedef Int64 DateTime;

template<typename T>
struct Array16 {
  enum : uint32_t { kMinSize = 2 };

  UInt16 count;
  BL_INLINE const T* array() const noexcept { return BLPtrOps::offset<const T>(this, 2); }
};

template<typename T>
struct Array32 {
  enum : uint32_t { kMinSize = 4 };

  UInt32 count;
  BL_INLINE const T* array() const noexcept { return BLPtrOps::offset<const T>(this, 4); }
};

//! Tag and offset.
//!
//! Replaces a lot of OpenType tables that use this structure (GDEF|GPOS|GSUB).
struct TagRef16 {
  UInt32 tag;
  Offset16 offset;
};

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTDEFS_P_H_INCLUDED
