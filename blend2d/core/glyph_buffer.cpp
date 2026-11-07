// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/axl/string_utils_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/font_p.h>
#include <blend2d/core/glyph_buffer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/string_p.h>
#include <blend2d/unicode/unicode_p.h>

namespace bl {

// GlyphBuffer - Internals
// =======================

static const constexpr GlyphBufferPrivateImpl null_glyph_buffer {};

static BL_INLINE BLResult glyph_buffer_ensure_data(BLGlyphBufferCore* self, GlyphBufferPrivateImpl** impl) noexcept {
  *impl = bl_glyph_buffer_get_impl(self);
  if (*impl != &null_glyph_buffer)
    return BL_SUCCESS;

  *impl = GlyphBufferPrivateImpl::create();
  if (BL_UNLIKELY(!*impl))
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);

  self->impl = *impl;
  return BL_SUCCESS;
}

// GlyphBuffer - Private API
// =========================

BLResult GlyphBufferPrivateImpl::ensure_buffer(size_t buffer_id, size_t copy_size, size_t min_capacity) noexcept {
  size_t old_capacity = capacity[buffer_id];
  BL_ASSERT(copy_size <= old_capacity);

  if (min_capacity <= old_capacity)
    return BL_SUCCESS;

  size_t new_capacity = min_capacity;
  if (new_capacity < BL_GLYPH_BUFFER_INITIAL_CAPACITY)
    new_capacity = BL_GLYPH_BUFFER_INITIAL_CAPACITY;
  else if (new_capacity < SIZE_MAX - 256)
    new_capacity = axl::align_up(min_capacity, 64);

  axl::OverflowFlag of{};
  size_t data_size = axl::mul_overflow<size_t>(new_capacity, BL_GLYPH_BUFFER_ANY_ITEM_SIZE, of);

  if (BL_UNLIKELY(axl::did_overflow(of)))
    return BL_ERROR_OUT_OF_MEMORY;

  uint8_t* new_data = axl::mem_alloc<uint8_t>(data_size);
  if (BL_UNLIKELY(!new_data)) {
    return BL_ERROR_OUT_OF_MEMORY;
  }

  uint8_t* old_data = static_cast<uint8_t*>(buffer[buffer_id]);
  if (copy_size) {
    memcpy(new_data,
           old_data,
           copy_size * sizeof(BLGlyphId));

    memcpy(new_data + new_capacity * sizeof(BLGlyphId),
           old_data + old_capacity * sizeof(BLGlyphId),
           copy_size * sizeof(BLGlyphInfo));
  }

  axl::mem_free(old_data);
  buffer[buffer_id] = new_data;
  capacity[buffer_id] = new_capacity;

  if (buffer_id == 0)
    get_glyph_data_ptrs(0, &content, &info_data);

  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLGlyphInfo glyph_info_from_cluster(const T& cluster) noexcept {
  return BLGlyphInfo { uint32_t(cluster), 0 };
}

template<typename T>
static BL_INLINE BLResult glyph_buffer_data_set_glyph_ids(GlyphBufferPrivateImpl* d, const T* src, size_t size, intptr_t advance) noexcept {
  uint32_t* glyph_data = d->content;
  BLGlyphInfo* info_data = d->info_data;

  for (size_t i = 0; i < size; i++) {
    glyph_data[i] = uint32_t(src[0]);
    info_data[i] = glyph_info_from_cluster(i);
    src = axl::offset_ptr(src, advance);
  }

  d->size = size;
  d->flags = 0;
  return BL_SUCCESS;
}

static BL_INLINE BLResult glyph_buffer_data_set_latin1_text(GlyphBufferPrivateImpl* d, const char* src, size_t size) noexcept {
  uint32_t* text_data = d->content;
  BLGlyphInfo* info_data = d->info_data;

  for (size_t i = 0; i < size; i++) {
    text_data[i] = uint8_t(src[i]);
    info_data[i] = glyph_info_from_cluster(i);
  }

  d->size = size;
  d->flags = 0;

  if (d->size)
    d->flags |= BL_GLYPH_RUN_FLAG_UCS4_CONTENT;

  return BL_SUCCESS;
}

template<typename Reader, typename CharType>
static BL_INLINE BLResult glyph_buffer_data_set_unicode_text(GlyphBufferPrivateImpl* d, const CharType* src, size_t size) noexcept {
  Reader reader(src, size);

  uint32_t* text_data = d->content;
  BLGlyphInfo* info_data = d->info_data;

  while (reader.has_next()) {
    uint32_t uc;
    uint32_t cluster = uint32_t(reader.native_index(src));
    BLResult result = reader.next(uc);

    *text_data++ = uc;
    *info_data++ = glyph_info_from_cluster(cluster);

    if (BL_LIKELY(result == BL_SUCCESS))
      continue;

    text_data[-1] = Unicode::kCharReplacement;
    d->flags |= BL_GLYPH_RUN_FLAG_INVALID_TEXT;
    reader.skip_one_unit();
  }

  d->size = (size_t)(text_data - d->content);
  d->flags = 0;

  if (d->size)
    d->flags |= BL_GLYPH_RUN_FLAG_UCS4_CONTENT;

  return BL_SUCCESS;
}

} // {bl}

// GlyphBuffer - Init & Destroy
// ============================

BL_API_IMPL BLResult bl_glyph_buffer_init(BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  self->impl = const_cast<GlyphBufferPrivateImpl*>(&null_glyph_buffer);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_glyph_buffer_init_move(BLGlyphBufferCore* self, BLGlyphBufferCore* other) noexcept {
  using namespace bl;

  GlyphBufferPrivateImpl* impl = bl_glyph_buffer_get_impl(other);
  other->impl = const_cast<GlyphBufferPrivateImpl*>(&null_glyph_buffer);
  self->impl = impl;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_glyph_buffer_destroy(BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  GlyphBufferPrivateImpl* impl = bl_glyph_buffer_get_impl(self);
  self->impl = nullptr;

  if (impl != &null_glyph_buffer) {
    impl->destroy();
  }
  return BL_SUCCESS;
}

// GlyphBuffer - Reset
// ===================

BL_API_IMPL BLResult bl_glyph_buffer_reset(BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  GlyphBufferPrivateImpl* impl = bl_glyph_buffer_get_impl(self);
  self->impl = const_cast<GlyphBufferPrivateImpl*>(&null_glyph_buffer);

  if (impl != &null_glyph_buffer) {
    impl->destroy();
  }
  return BL_SUCCESS;
}

// GlyphBuffer - Content
// =====================

BL_API_IMPL BLResult bl_glyph_buffer_clear(BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  GlyphBufferPrivateImpl* self_impl = bl_glyph_buffer_get_impl(self);

  // Would be true if the glyph-buffer is built-in 'none' instance or the data
  // is allocated, but empty.
  if (self_impl->size == 0) {
    return BL_SUCCESS;
  }

  self_impl->clear();
  return BL_SUCCESS;
}

BL_API_IMPL size_t bl_glyph_buffer_get_size(const BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  GlyphBufferPrivateImpl* self_impl = bl_glyph_buffer_get_impl(self);
  return self_impl->size;
}

BL_API_IMPL uint32_t bl_glyph_buffer_get_flags(const BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  GlyphBufferPrivateImpl* self_impl = bl_glyph_buffer_get_impl(self);
  return self_impl->flags;
}

BL_API_IMPL const BLGlyphRun* bl_glyph_buffer_get_glyph_run(const BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  GlyphBufferPrivateImpl* self_impl = bl_glyph_buffer_get_impl(self);
  return &self_impl->glyph_run;
}

BL_API_IMPL const uint32_t* bl_glyph_buffer_get_content(const BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  GlyphBufferPrivateImpl* self_impl = bl_glyph_buffer_get_impl(self);
  return self_impl->content;
}

BL_API_IMPL const BLGlyphInfo* bl_glyph_buffer_get_info_data(const BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  GlyphBufferPrivateImpl* self_impl = bl_glyph_buffer_get_impl(self);
  return self_impl->info_data;
}

BL_API_IMPL const BLGlyphPlacement* bl_glyph_buffer_get_placement_data(const BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  GlyphBufferPrivateImpl* self_impl = bl_glyph_buffer_get_impl(self);
  return self_impl->placement_data;
}

BL_API_IMPL BLResult bl_glyph_buffer_set_text(BLGlyphBufferCore* self, const void* text_data, size_t size, BLTextEncoding encoding) noexcept {
  using namespace bl;

  if (BL_UNLIKELY(uint32_t(encoding) > BL_TEXT_ENCODING_MAX_VALUE))
    return bl_make_error(BL_ERROR_INVALID_VALUE);

  GlyphBufferPrivateImpl* d;
  BL_PROPAGATE(glyph_buffer_ensure_data(self, &d));

  switch (encoding) {
    case BL_TEXT_ENCODING_LATIN1:
      if (size == SIZE_MAX) {
        size = strlen(static_cast<const char*>(text_data));
      }

      BL_PROPAGATE(d->ensure_buffer(0, 0, size));
      return glyph_buffer_data_set_latin1_text(d, static_cast<const char*>(text_data), size);

    case BL_TEXT_ENCODING_UTF8:
      if (size == SIZE_MAX) {
        size = strlen(static_cast<const char*>(text_data));
      }

      BL_PROPAGATE(d->ensure_buffer(0, 0, size));
      return glyph_buffer_data_set_unicode_text<Unicode::Utf8Reader>(d, static_cast<const uint8_t*>(text_data), size);

    case BL_TEXT_ENCODING_UTF16:
      if (size == SIZE_MAX) {
        size = axl::str_len(static_cast<const uint16_t*>(text_data));
      }

      BL_PROPAGATE(d->ensure_buffer(0, 0, size));
      return glyph_buffer_data_set_unicode_text<Unicode::Utf16Reader>(d, static_cast<const uint16_t*>(text_data), size * 2u);

    case BL_TEXT_ENCODING_UTF32:
      if (size == SIZE_MAX) {
        size = axl::str_len(static_cast<const uint32_t*>(text_data));
      }

      BL_PROPAGATE(d->ensure_buffer(0, 0, size));
      return glyph_buffer_data_set_unicode_text<Unicode::Utf32Reader>(d, static_cast<const uint32_t*>(text_data), size * 4u);

    default:
      // Avoids a compile-time warning, should never be reached.
      return bl_make_error(BL_ERROR_INVALID_VALUE);
  }
}

BL_API_IMPL BLResult bl_glyph_buffer_set_glyphs(BLGlyphBufferCore* self, const uint32_t* glyph_data, size_t size) noexcept {
  using namespace bl;

  if (BL_UNLIKELY(sizeof(size_t) > 4 && size > 0xFFFFFFFFu))
    return bl_make_error(BL_ERROR_DATA_TOO_LARGE);

  GlyphBufferPrivateImpl* d;

  BL_PROPAGATE(glyph_buffer_ensure_data(self, &d));
  BL_PROPAGATE(d->ensure_buffer(0, 0, size));

  return glyph_buffer_data_set_glyph_ids(d, glyph_data, size, intptr_t(sizeof(uint16_t)));
}

BL_API_IMPL BLResult bl_glyph_buffer_set_glyphs_from_struct(BLGlyphBufferCore* self, const void* glyph_data, size_t size, size_t glyph_id_size, intptr_t glyph_id_advance) noexcept {
  using namespace bl;

  if (BL_UNLIKELY(glyph_id_size != 2 && glyph_id_size != 4))
    return bl_make_error(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(sizeof(size_t) > 4 && size > 0xFFFFFFFFu))
    return bl_make_error(BL_ERROR_DATA_TOO_LARGE);

  GlyphBufferPrivateImpl* d;

  BL_PROPAGATE(glyph_buffer_ensure_data(self, &d));
  BL_PROPAGATE(d->ensure_buffer(0, 0, size));

  if (glyph_id_size == 2)
    return glyph_buffer_data_set_glyph_ids(d, static_cast<const uint16_t*>(glyph_data), size, glyph_id_advance);
  else
    return glyph_buffer_data_set_glyph_ids(d, static_cast<const uint32_t*>(glyph_data), size, glyph_id_advance);
}

BL_API_IMPL BLResult bl_glyph_buffer_set_debug_sink(BLGlyphBufferCore* self, BLDebugMessageSinkFunc sink, void* user_data) noexcept {
  using namespace bl;

  if (!sink)
    return bl_glyph_buffer_reset_debug_sink(self);

  GlyphBufferPrivateImpl* d;
  BL_PROPAGATE(glyph_buffer_ensure_data(self, &d));

  d->debug_sink = sink;
  d->debug_sink_user_data = user_data;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_glyph_buffer_reset_debug_sink(BLGlyphBufferCore* self) noexcept {
  using namespace bl;

  if (!bl_glyph_buffer_get_impl(self)->debug_sink)
    return BL_SUCCESS;

  GlyphBufferPrivateImpl* d;
  BL_PROPAGATE(glyph_buffer_ensure_data(self, &d));

  d->debug_sink = nullptr;
  d->debug_sink_user_data = nullptr;

  return BL_SUCCESS;
}
