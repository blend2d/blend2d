// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONT_P_H_INCLUDED
#define BLEND2D_FONT_P_H_INCLUDED

#include "api-internal_p.h"
#include "array_p.h"
#include "bitset_p.h"
#include "font.h"
#include "matrix_p.h"
#include "object_p.h"
#include "support/scopedbuffer_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLOpenType { struct OTFaceImpl; }

static constexpr uint32_t BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE = 2048;

//! \name Font - Uncategoried Internals
//! \{

//! Returns `true` if the given `tag` is valid. A valid tag consists of 4 ASCII characters within [32..126] range.
static BL_INLINE bool blFontTagIsValid(uint32_t tag) noexcept {
  return (bool)((((tag - 0x20202020u) & 0xFF000000u) < 0x5F000000u) &
                (((tag - 0x20202020u) & 0x00FF0000u) < 0x005F0000u) &
                (((tag - 0x20202020u) & 0x0000FF00u) < 0x00005F00u) &
                (((tag - 0x20202020u) & 0x000000FFu) < 0x0000005Fu));
}

//! Converts `tag` to a null-terminated ASCII string `str`. Characters that are not printable are replaced by '?'.
static BL_INLINE void blFontTagToAscii(char str[5], uint32_t tag) noexcept {
  for (size_t i = 0; i < 4; i++, tag <<= 8) {
    uint32_t c = tag >> 24;
    str[i] = (c < 32 || c > 126) ? char('?') : char(c);
  }
  str[4] = '\0';
}

static BL_INLINE void blFontMatrixMultiply(BLMatrix2D* dst, const BLFontMatrix* a, const BLMatrix2D* b) noexcept {
  dst->reset(a->m00 * b->m00 + a->m01 * b->m10,
             a->m00 * b->m01 + a->m01 * b->m11,
             a->m10 * b->m00 + a->m11 * b->m10,
             a->m10 * b->m01 + a->m11 * b->m11,
             b->m20,
             b->m21);
}

static BL_INLINE void blFontMatrixMultiply(BLMatrix2D* dst, const BLMatrix2D* a, const BLFontMatrix* b) noexcept {
  dst->reset(a->m00 * b->m00 + a->m01 * b->m10,
             a->m00 * b->m01 + a->m01 * b->m11,
             a->m10 * b->m00 + a->m11 * b->m10,
             a->m10 * b->m01 + a->m11 * b->m11,
             a->m20 * b->m00 + a->m21 * b->m10,
             a->m20 * b->m01 + a->m21 * b->m11);
}

//! \}

//! \name Font Data - Internal Font Table Functionality
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

//! \name Font Data - Internal Memory Management
//! \{

struct BLInternalFontDataImpl : public BLFontDataImpl {
  volatile size_t backRefCount;
  BLArray<BLFontFaceImpl*> faceCache;
};

static BL_INLINE BLInternalFontDataImpl* blFontDataGetImpl(const BLFontDataCore* self) noexcept {
  return static_cast<BLInternalFontDataImpl*>(self->_d.impl);
}

static BL_INLINE void blFontDataImplCtor(BLInternalFontDataImpl* impl, BLFontDataVirt* virt) noexcept {
  impl->virt = virt;
  impl->faceCount = 0;
  impl->faceType = BL_FONT_FACE_TYPE_NONE;
  impl->flags = 0;
  impl->backRefCount = 0;
  blCallCtor(impl->faceCache);
};

//! \}

//! \name Font Face - Internal Memory Management
//! \{

struct BLInternalFontFaceFuncs {
  BLResult (BL_CDECL* mapTextToGlyphs)(
    const BLFontFaceImpl* impl,
    uint32_t* content,
    size_t count,
    BLGlyphMappingState* state) BL_NOEXCEPT;

  BLResult (BL_CDECL* getGlyphBounds)(
    const BLFontFaceImpl* impl,
    const uint32_t* glyphData,
    intptr_t glyphAdvance,
    BLBoxI* boxes,
    size_t count) BL_NOEXCEPT;

  BLResult (BL_CDECL* getGlyphAdvances)(
    const BLFontFaceImpl* impl,
    const uint32_t* glyphData,
    intptr_t glyphAdvance,
    BLGlyphPlacement* placementData,
    size_t count) BL_NOEXCEPT;

  BLResult (BL_CDECL* getGlyphOutlines)(
    const BLFontFaceImpl* impl,
    uint32_t glyphId,
    const BLMatrix2D* userMatrix,
    BLPath* out,
    size_t* contourCountOut,
    BLScopedBuffer* tmpBuffer) BL_NOEXCEPT;

  BLResult (BL_CDECL* applyKern)(
    const BLFontFaceImpl* faceI,
    uint32_t* glyphData,
    BLGlyphPlacement* placementData,
    size_t count) BL_NOEXCEPT;

  BLResult (BL_CDECL* applyGSub)(
    const BLFontFaceImpl* impl,
    BLGlyphBuffer* gb,
    const BLBitSetCore* lookups) BL_NOEXCEPT;

  BLResult (BL_CDECL* applyGPos)(
    const BLFontFaceImpl* impl,
    BLGlyphBuffer* gb,
    const BLBitSetCore* lookups) BL_NOEXCEPT;

  BLResult (BL_CDECL* positionGlyphs)(
    const BLFontFaceImpl* impl,
    uint32_t* glyphData,
    BLGlyphPlacement* placementData,
    size_t count) BL_NOEXCEPT;
};

BL_HIDDEN extern BLInternalFontFaceFuncs blNullFontFaceFuncs;

struct BLInternalFontFaceImpl : public BLFontFaceImpl {
  BLBitSetCore characterCoverage;
  BLInternalFontFaceFuncs funcs;
};

template<typename T = BLInternalFontFaceImpl>
static BL_INLINE T* blFontFaceGetImpl(const BLFontFaceCore* self) noexcept {
  return static_cast<T*>(static_cast<BLInternalFontFaceImpl*>(self->_d.impl));
}

static BL_INLINE void blFontFaceImplCtor(BLInternalFontFaceImpl* impl, BLFontFaceVirt* virt, BLInternalFontFaceFuncs& funcs) noexcept {
  impl->virt = virt;
  blCallCtor(impl->data);
  blCallCtor(impl->fullName);
  blCallCtor(impl->familyName);
  blCallCtor(impl->subfamilyName);
  blCallCtor(impl->postScriptName);
  blObjectAtomicContentInit(&impl->characterCoverage);
  impl->funcs = funcs;
}

static BL_INLINE void blFontFaceImplDtor(BLInternalFontFaceImpl* impl) noexcept {
  if (blObjectAtomicContentTest(&impl->characterCoverage))
    blCallDtor(impl->characterCoverage);

  blCallDtor(impl->postScriptName);
  blCallDtor(impl->subfamilyName);
  blCallDtor(impl->familyName);
  blCallDtor(impl->fullName);
  blCallDtor(impl->data);
}

//! \}

//! \name Font - Internal Memory Management
//! \{

struct BLInternalFontImpl : public BLFontImpl {};

static BL_INLINE BLInternalFontImpl* blFontGetImpl(const BLFontCore* self) noexcept {
  return static_cast<BLInternalFontImpl*>(self->_d.impl);
}

BL_HIDDEN BLResult blFontImplFree(BLInternalFontImpl* impl, BLObjectInfo info) noexcept;

static BL_INLINE bool blFontPrivateIsMutable(const BLFontCore* self) noexcept {
  const size_t* refCountPtr = blObjectImplGetRefCountPtr(self->_d.impl);
  return *refCountPtr == 1;
}

static BL_INLINE void blFontImplCtor(BLInternalFontImpl* impl) noexcept {
  impl->face._d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d;
  blCallCtor(impl->features);
  blCallCtor(impl->variations);
}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_FONT_P_H_INCLUDED
