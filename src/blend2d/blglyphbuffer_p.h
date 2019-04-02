// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLGLYPHBUFFER_P_H
#define BLEND2D_BLGLYPHBUFFER_P_H

#include "./blapi-internal_p.h"
#include "./blfont.h"
#include "./blglyphbuffer.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLGlyphBuffer - Internal Enums]
// ============================================================================

enum BLGlyphBufferFlags : uint32_t {
  //! Glyph-buffer already contains glyph advances.
  BL_GLYPH_BUFFER_GLYPH_ADVANCES = 0x00000001u,
  //! Glyph-buffer has a calculated bounding box.
  BL_GLYPH_BUFFER_BOUNDING_BOX   = 0x00000002u
};

enum BLGlyphBufferEnums : uint32_t {
  //! Size of either GlyphIdData+GlyphItemData or PlacementData.
  BL_GLYPH_BUFFER_ANY_ITEM_SIZE = 16,

  BL_GLYPH_BUFFER_INITIAL_CAPACITY = 256,
  BL_GLYPH_BUFFER_AGGRESIVE_GROWTH = BL_ALLOC_GROW_LIMIT / BL_GLYPH_BUFFER_ANY_ITEM_SIZE,
};

// ============================================================================
// [BLGlyphBuffer - Internal Data]
// ============================================================================

struct BLInternalGlyphBufferData : public BLGlyphBufferData {
  uint8_t* buffer[2];
  size_t capacity[2];

  // Default-constructed data should not be initialized.
  constexpr BLInternalGlyphBufferData() noexcept
    : BLGlyphBufferData {},
      buffer { nullptr, nullptr },
      capacity { 0, 0 } {}

  static BLInternalGlyphBufferData* create() noexcept {
    BLInternalGlyphBufferData* d = (BLInternalGlyphBufferData*)malloc(sizeof(BLInternalGlyphBufferData));
    if (BL_UNLIKELY(!d))
      return nullptr;

    d->glyphItemData = nullptr;
    d->placementData = nullptr;
    d->size = 0;
    d->glyphRun.glyphIdSize = uint8_t(sizeof(BLGlyphItem));
    d->glyphRun.placementType = BL_GLYPH_PLACEMENT_TYPE_NONE;
    d->glyphRun.glyphIdAdvance = int8_t(sizeof(BLGlyphItem));
    d->glyphRun.placementAdvance = int8_t(sizeof(BLGlyphPlacement));
    d->flags = 0;

    d->glyphInfoData = nullptr;
    d->buffer[0] = nullptr;
    d->buffer[1] = nullptr;
    d->capacity[0] = 0;
    d->capacity[1] = 0;

    return d;
  }

  BL_INLINE void destroy() noexcept {
    resetBuffers();
    free(this);
  }

  BL_INLINE void resetBuffers() noexcept {
    if (buffer[0]) { free(buffer[0]); buffer[0] = nullptr; }
    if (buffer[1]) { free(buffer[1]); buffer[1] = nullptr; }
  }

  BL_INLINE void clear() noexcept {
    size = 0;
    glyphRun.placementType = BL_GLYPH_PLACEMENT_TYPE_NONE;
    glyphRun.flags = 0;
    placementData = nullptr;
    getGlyphDataPtrs(0, &glyphItemData, &glyphInfoData);
  }

  BL_HIDDEN BLResult ensureBuffer(size_t bufferId, size_t copySize, size_t minCapacity) noexcept;

  BL_INLINE BLResult ensurePlacement() noexcept {
    BL_PROPAGATE(ensureBuffer(1, 0, size));
    placementData = reinterpret_cast<BLGlyphPlacement*>(buffer[1]);
    return BL_SUCCESS;
  }

  BL_INLINE void flip() noexcept {
    std::swap(buffer[0], buffer[1]);
    std::swap(capacity[0], capacity[1]);
  }

  BL_INLINE void getGlyphDataPtrs(size_t bufferId, BLGlyphItem** glyphItemOut, BLGlyphInfo** glyphInfoOut) noexcept {
    *glyphItemOut = reinterpret_cast<BLGlyphItem*>(buffer[bufferId]);
    *glyphInfoOut = reinterpret_cast<BLGlyphInfo*>(buffer[bufferId] + capacity[bufferId] * sizeof(BLGlyphItem));
  }
};

template<>
struct BLInternalCastImpl<BLGlyphBufferData> { typedef BLInternalGlyphBufferData Type; };

static BL_INLINE void blCopyGlyphData(BLGlyphItem* itemDst, BLGlyphInfo* infoDst, const BLGlyphItem* itemSrc, const BLGlyphInfo* infoSrc, size_t n) noexcept {
  const BLGlyphItem* itemEnd = itemSrc + n;
  while (itemSrc != itemEnd) {
    *itemDst++ = *itemSrc++;
    *infoDst++ = *infoSrc++;
  }
}

//! \}
//! \endcond

#endif // BLEND2D_BLGLYPHBUFFER_P_H
