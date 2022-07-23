// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GLYPHBUFFER_P_H_INCLUDED
#define BLEND2D_GLYPHBUFFER_P_H_INCLUDED

#include "api-internal_p.h"
#include "font.h"
#include "glyphbuffer.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

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

struct BLGlyphBufferPrivateImpl : public BLGlyphBufferImpl {
  uint8_t* buffer[2];
  size_t capacity[2];

  // Default-constructed data should not be initialized.
  BL_INLINE constexpr BLGlyphBufferPrivateImpl() noexcept
    : BLGlyphBufferImpl {},
      buffer { nullptr, nullptr },
      capacity { 0, 0 } {}

  static BLGlyphBufferPrivateImpl* create() noexcept {
    BLGlyphBufferPrivateImpl* d = (BLGlyphBufferPrivateImpl*)malloc(sizeof(BLGlyphBufferPrivateImpl));
    if (BL_UNLIKELY(!d))
      return nullptr;

    d->content = nullptr;
    d->placementData = nullptr;
    d->size = 0;
    d->glyphRun.glyphSize = uint8_t(sizeof(uint32_t));
    d->glyphRun.placementType = BL_GLYPH_PLACEMENT_TYPE_NONE;
    d->glyphRun.glyphAdvance = int8_t(sizeof(uint32_t));
    d->glyphRun.placementAdvance = int8_t(sizeof(BLGlyphPlacement));
    d->flags = 0;

    d->infoData = nullptr;
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
    free(buffer[0]);
    free(buffer[1]);

    buffer[0] = nullptr;
    buffer[1] = nullptr;
  }

  BL_INLINE void clear() noexcept {
    size = 0;
    glyphRun.placementType = BL_GLYPH_PLACEMENT_TYPE_NONE;
    glyphRun.flags = 0;
    placementData = nullptr;
    getGlyphDataPtrs(0, &content, &infoData);
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

  BL_INLINE void getGlyphDataPtrs(size_t bufferId, uint32_t** glyphDataOut, BLGlyphInfo** infoDataOut) noexcept {
    *glyphDataOut = reinterpret_cast<uint32_t*>(buffer[bufferId]);
    *infoDataOut = reinterpret_cast<BLGlyphInfo*>(buffer[bufferId] + capacity[bufferId] * sizeof(uint32_t));
  }
};

static BL_INLINE BLGlyphBufferPrivateImpl* blGlyphBufferGetImpl(const BLGlyphBufferCore* self) noexcept {
  return static_cast<BLGlyphBufferPrivateImpl*>(self->impl);
}

static BL_INLINE void blCopyGlyphData(uint32_t* glyphDst, BLGlyphInfo* infoDst, const uint32_t* glyphSrc, const BLGlyphInfo* infoSrc, size_t n) noexcept {
  for (size_t i = 0; i < n; i++) {
    glyphDst[i] = glyphSrc[i];
    infoDst[i] = infoSrc[i];
  }
}

//! \}
//! \endcond

#endif // BLEND2D_GLYPHBUFFER_P_H_INCLUDED
