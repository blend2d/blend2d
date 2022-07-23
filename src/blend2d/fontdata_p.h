// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTDATA_P_H_INCLUDED
#define BLEND2D_FONTDATA_P_H_INCLUDED

#include "api-internal_p.h"
#include "fontdata.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name BLFontData - Internal Font Table Functionality
//! \{

//! A convenience class that maps `BLFontTable` to a typed table.
template<typename T>
class BLFontTableT : public BLFontTable {
public:
  BL_INLINE BLFontTableT() noexcept = default;
  BL_INLINE constexpr BLFontTableT(const BLFontTableT& other) noexcept = default;

  BL_INLINE constexpr BLFontTableT(const BLFontTable& other) noexcept
    : BLFontTable(other) {}

  BL_INLINE constexpr BLFontTableT(const uint8_t* data, size_t size) noexcept
    : BLFontTable { data, size } {}

  BL_INLINE BLFontTableT& operator=(const BLFontTableT& other) noexcept = default;
  BL_INLINE const T* operator->() const noexcept { return dataAs<T>(); }
};

static BL_INLINE bool blFontTableFitsN(const BLFontTable& table, size_t requiredSize, size_t offset = 0) noexcept {
  return (table.size - offset) >= requiredSize;
}

template<typename T>
static BL_INLINE bool blFontTableFitsT(const BLFontTable& table, size_t offset = 0) noexcept {
  return blFontTableFitsN(table, T::kMinSize, offset);
}

static BL_INLINE BLFontTable blFontSubTable(const BLFontTable& table, size_t offset) noexcept {
  BL_ASSERT(offset <= table.size);
  return BLFontTable { table.data + offset, table.size - offset };
}

static BL_INLINE BLFontTable blFontSubTableChecked(const BLFontTable& table, size_t offset) noexcept {
  return blFontSubTable(table, blMin(table.size, offset));
}

template<typename T>
static BL_INLINE BLFontTableT<T> blFontSubTableT(const BLFontTable& table, size_t offset) noexcept {
  BL_ASSERT(offset <= table.size);
  return BLFontTableT<T> { table.data + offset, table.size - offset };
}

template<typename T>
static BL_INLINE BLFontTableT<T> blFontSubTableCheckedT(const BLFontTable& table, size_t offset) noexcept {
  return blFontSubTableT<T>(table, blMin(table.size, offset));
}

//! \}

//! \name BLFontData - Impl
//! \{

struct BLFontDataPrivateImpl : public BLFontDataImpl {
  volatile size_t backRefCount;
  BLArray<BLFontFaceImpl*> faceCache;
};

static BL_INLINE BLFontDataPrivateImpl* blFontDataGetImpl(const BLFontDataCore* self) noexcept {
  return static_cast<BLFontDataPrivateImpl*>(self->_d.impl);
}

static BL_INLINE void blFontDataImplCtor(BLFontDataPrivateImpl* impl, BLFontDataVirt* virt) noexcept {
  impl->virt = virt;
  impl->faceCount = 0;
  impl->faceType = BL_FONT_FACE_TYPE_NONE;
  impl->flags = 0;
  impl->backRefCount = 0;
  blCallCtor(impl->faceCache);
};

//! \}

//! \}
//! \endcond

#endif // BLEND2D_FONTDATA_P_H_INCLUDED
