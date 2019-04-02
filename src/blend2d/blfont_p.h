// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLFONT_P_H
#define BLEND2D_BLFONT_P_H

#include "./blapi-internal_p.h"
#include "./blarray_p.h"
#include "./blfont.h"
#include "./blmatrix_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct BLOTFaceImpl;

// ============================================================================
// [Constants]
// ============================================================================

static constexpr uint32_t BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE = 2048;

// ============================================================================
// [Utilities]
// ============================================================================

//! Returns `true` if the given `tag` is valid. A valid tag consists of 4
//! ASCII characters within [32..126] range (inclusive).
static BL_INLINE bool blFontTagIsValid(uint32_t tag) noexcept {
  return (bool)( (((tag - 0x20202020u) & 0xFF000000u) < 0x5F000000u) &
                 (((tag - 0x20202020u) & 0x00FF0000u) < 0x005F0000u) &
                 (((tag - 0x20202020u) & 0x0000FF00u) < 0x00005F00u) &
                 (((tag - 0x20202020u) & 0x000000FFu) < 0x0000005Fu) );
}

//! Converts `tag` to a null-terminated ASCII string `str`. Characters that are
//! not printable are replaced by '?' character, thus it's not safe to convert
//! the output string back to tag if it was invalid.
static BL_INLINE void blFontTagToAscii(char str[5], uint32_t tag) noexcept {
  for (size_t i = 0; i < 4; i++, tag <<= 8) {
    uint32_t c = tag >> 24;
    str[i] = (c < 32 || c > 127) ? char('?') : char(c);
  }
  str[4] = '\0';
}

BL_INLINE void blFontMatrixMultiply(BLMatrix2D* dst, const BLFontMatrix* a, const BLMatrix2D* b) noexcept {
  dst->reset(a->m00 * b->m00 + a->m01 * b->m10,
             a->m00 * b->m01 + a->m01 * b->m11,
             a->m10 * b->m00 + a->m11 * b->m10,
             a->m10 * b->m01 + a->m11 * b->m11,
             b->m20,
             b->m21);
}

BL_INLINE void blFontMatrixMultiply(BLMatrix2D* dst, const BLMatrix2D* a, const BLFontMatrix* b) noexcept {
  dst->reset(a->m00 * b->m00 + a->m01 * b->m10,
             a->m00 * b->m01 + a->m01 * b->m11,
             a->m10 * b->m00 + a->m11 * b->m10,
             a->m10 * b->m01 + a->m11 * b->m11,
             a->m20 * b->m00 + a->m21 * b->m10,
             a->m20 * b->m01 + a->m21 * b->m11);
}

// ============================================================================
// [BLFontTableT]
// ============================================================================

//! A convenience class that maps `BLFontTable` to a typed table.
template<typename T>
class BLFontTableT : public BLFontTable {
public:
  BL_INLINE BLFontTableT() noexcept = default;
  constexpr BLFontTableT(const BLFontTableT& other) noexcept = default;

  constexpr BLFontTableT(const BLFontTable& other) noexcept
    : BLFontTable(other) {}

  constexpr BLFontTableT(const uint8_t* data, size_t size) noexcept
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

// ============================================================================
// [BLFontFace - Internal]
// ============================================================================

struct BLInternalFontFaceFuncs {
  BLResult (BL_CDECL* mapTextToGlyphs)(
    const BLFontFaceImpl* impl,
    BLGlyphItem* itemData,
    size_t count,
    BLGlyphMappingState* state) BL_NOEXCEPT;

  BLResult (BL_CDECL* getGlyphBounds)(
    const BLFontFaceImpl* impl,
    const BLGlyphId* glyphIdData,
    intptr_t glyphIdAdvance,
    BLBoxI* boxes,
    size_t count) BL_NOEXCEPT;

  BLResult (BL_CDECL* getGlyphAdvances)(
    const BLFontFaceImpl* impl,
    const BLGlyphId* glyphIdData,
    intptr_t glyphIdAdvance,
    BLGlyphPlacement* placementData,
    size_t count) BL_NOEXCEPT;

  BLResult (BL_CDECL* applyKern)(
    const BLFontFaceImpl* faceI,
    BLGlyphItem* itemData,
    BLGlyphPlacement* placementData,
    size_t count) BL_NOEXCEPT;

  BLResult (BL_CDECL* applyGSub)(
    const BLFontFaceImpl* impl,
    BLGlyphBuffer* buf,
    size_t index,
    BLBitWord lookups) BL_NOEXCEPT;

  BLResult (BL_CDECL* applyGPos)(
    const BLFontFaceImpl* impl,
    BLGlyphBuffer* buf,
    size_t index,
    BLBitWord lookups) BL_NOEXCEPT;

  BLResult (BL_CDECL* positionGlyphs)(
    const BLFontFaceImpl* impl,
    BLGlyphItem* itemData,
    BLGlyphPlacement* placementData,
    size_t count) BL_NOEXCEPT;

  BLResult (BL_CDECL* decodeGlyph)(
    const BLFontFaceImpl* impl,
    uint32_t glyphId,
    const BLMatrix2D* userMatrix,
    BLPath* out,
    BLMemBuffer* tmpBuffer,
    BLPathSinkFunc sink, size_t sinkGlyphIndex, void* closure) BL_NOEXCEPT;
};

BL_HIDDEN extern BLInternalFontFaceFuncs blNullFontFaceFuncs;

struct BLInternalFontFaceImpl : public BLFontFaceImpl {
  BLInternalFontFaceFuncs funcs;
};

template<>
struct BLInternalCastImpl<BLFontFaceImpl> { typedef BLInternalFontFaceImpl Type; };

// ============================================================================
// [BLFont - Internal]
// ============================================================================

struct BLInternalFontImpl : public BLFontImpl {};

template<>
struct BLInternalCastImpl<BLFontImpl> { typedef BLInternalFontImpl Type; };

BL_HIDDEN BLResult blFontImplDelete(BLFontImpl* impl_) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_BLFONT_P_H
