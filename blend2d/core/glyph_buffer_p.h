// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_GLYPH_BUFFER_P_H_INCLUDED
#define BLEND2D_CORE_GLYPH_BUFFER_P_H_INCLUDED

#include <blend2d/axl/mem_alloc.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/font.h>
#include <blend2d/core/glyph_buffer.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

enum GlyphBufferFlags : uint32_t {
  //! Glyph-buffer already contains glyph advances.
  BL_GLYPH_BUFFER_GLYPH_ADVANCES = 0x00000001u,
  //! Glyph-buffer has a calculated bounding box.
  BL_GLYPH_BUFFER_BOUNDING_BOX   = 0x00000002u
};

//! Size of either GlyphIdData+GlyphItemData or PlacementData.
static constexpr uint32_t BL_GLYPH_BUFFER_ANY_ITEM_SIZE = 16;

static constexpr uint32_t BL_GLYPH_BUFFER_INITIAL_CAPACITY = 256;
static constexpr uint32_t BL_GLYPH_BUFFER_AGGRESSIVE_GROWTH = BL_ALLOC_GROWTH_LIMIT_50 / BL_GLYPH_BUFFER_ANY_ITEM_SIZE;

struct GlyphBufferPrivateImpl : public BLGlyphBufferImpl {
  uint8_t* buffer[2];
  size_t capacity[2];

  BL_INLINE constexpr GlyphBufferPrivateImpl() noexcept
    : BLGlyphBufferImpl {},
      buffer { nullptr, nullptr },
      capacity { 0, 0 } {
    glyph_run = BLGlyphRun {
      nullptr, nullptr, 0, 0, BL_GLYPH_PLACEMENT_TYPE_NONE,
      int8_t(sizeof(uint32_t)), int8_t(sizeof(BLGlyphPlacement)), 0
    };
  }

  BL_INLINE void reset_buffers() noexcept {
    free_buffer(buffer[0]);
    free_buffer(buffer[1]);

    buffer[0] = nullptr;
    buffer[1] = nullptr;
  }

  BL_INLINE uint8_t* primary_buffer() noexcept {
    return reinterpret_cast<uint8_t*>(this + 1);
  }

  BL_INLINE void free_buffer(uint8_t* data) noexcept {
    // The primary allocation owns Impl even after its buffer is flipped or outgrown.
    if (data != primary_buffer()) {
      axl::mem_free(data);
    }
  }

  BL_INLINE void clear() noexcept {
    size = 0;
    glyph_run.placement_type = BL_GLYPH_PLACEMENT_TYPE_NONE;
    glyph_run.flags = 0;
    placement_data = nullptr;
    get_glyph_data_ptrs(0, &content, &info_data);
  }

  BL_HIDDEN BLResult ensure_buffer(size_t buffer_id, size_t copy_size, size_t min_capacity) noexcept;

  BL_INLINE BLResult ensure_placement() noexcept {
    BL_PROPAGATE(ensure_buffer(1, 0, size));
    placement_data = reinterpret_cast<BLGlyphPlacement*>(buffer[1]);
    return BL_SUCCESS;
  }

  BL_INLINE void flip() noexcept {
    bl::Internal::swap(buffer[0], buffer[1]);
    bl::Internal::swap(capacity[0], capacity[1]);
    get_glyph_data_ptrs(0, &content, &info_data);
  }

  BL_INLINE void get_glyph_data_ptrs(size_t buffer_id, uint32_t** glyph_data_out, BLGlyphInfo** info_data_out) noexcept {
    *glyph_data_out = reinterpret_cast<uint32_t*>(buffer[buffer_id]);
    *info_data_out = buffer[buffer_id]
      ? reinterpret_cast<BLGlyphInfo*>(buffer[buffer_id] + capacity[buffer_id] * sizeof(uint32_t))
      : nullptr;
  }
};

template<>
struct ObjectDispatchInfo<BLGlyphBufferCore> {
  using ImplType = GlyphBufferPrivateImpl;

  static constexpr bool kHasInitSSO = false;

  static BL_INLINE BLResult free_object(BLGlyphBufferCore* self, ImplType* impl) noexcept {
    axl::maybe_unused(self);
    impl->reset_buffers();
    return ObjectLayer::dealloc_impl(impl);
  }
};

BL_HIDDEN BLResult glyph_buffer_make_mutable_internal(BLGlyphBufferCore* self, GlyphBufferPrivateImpl** impl_out, size_t min_capacity) noexcept;

static BL_INLINE BLResult glyph_buffer_make_mutable(BLGlyphBufferCore* self, GlyphBufferPrivateImpl** impl_out, size_t min_capacity = 0) noexcept {
  if (ObjectLayer::is_object_mutable(self)) {
    *impl_out = ObjectLayer::get_impl(self);
    return BL_SUCCESS;
  }
  return glyph_buffer_make_mutable_internal(self, impl_out, min_capacity);
}

static BL_INLINE void bl_copy_glyph_data(uint32_t* glyph_dst, BLGlyphInfo* info_dst, const uint32_t* glyph_src, const BLGlyphInfo* info_src, size_t n) noexcept {
  for (size_t i = 0; i < n; i++) {
    glyph_dst[i] = glyph_src[i];
    info_dst[i] = info_src[i];
  }
}

} // {bl}

static BL_INLINE bl::GlyphBufferPrivateImpl* bl_glyph_buffer_get_impl(const BLGlyphBufferCore* self) noexcept {
  return bl::ObjectLayer::get_impl(self);
}

//! \}
//! \endcond

#endif // BLEND2D_CORE_GLYPH_BUFFER_P_H_INCLUDED
