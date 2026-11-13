// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_GLYPH_BUFFER_H_INCLUDED
#define BLEND2D_CORE_GLYPH_BUFFER_H_INCLUDED

#include <blend2d/core/font_defs.h>
#include <blend2d/core/glyph_run.h>
#include <blend2d/core/object_layer.h>

//! \addtogroup bl_c_api
//! \{

//! \name BLGlyphBuffer - C API
//!
//! \{

//! Glyph buffer [Impl].
struct BLGlyphBufferImpl {
  union {
    struct {
      //! Text (UCS4 code-points) or glyph content.
      uint32_t* content;
      //! Glyph placement data.
      BLGlyphPlacement* placement_data;
      //! Number of either code points or glyph indexes in the glyph-buffer.
      size_t size;
      //! Reserved, used exclusively by BLGlyphRun.
      uint32_t reserved;
      //! Flags shared between BLGlyphRun and BLGlyphBuffer.
      uint32_t flags;
    };

    //! Glyph run data that can be passed directly to the rendering context.
    //!
    //! Glyph run shares data with other members like `content`, `placement_data`, `size`, and `flags`. When working
    //! with data it's better to access these members directly as they are typed, whereas \ref BLGlyphRun stores
    //! pointers as `void*` as it offers more flexibility, which \ref BLGlyphBuffer doesn't need.
    BLGlyphRun glyph_run;
  };

  //! Glyph info data - additional information of each code-point or glyph.
  BLGlyphInfo* info_data;
};

//! Glyph buffer [C API].
struct BLGlyphBufferCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT(BLGlyphBuffer, bl::ObjectLayer::ObjectInfo<
    BL_OBJECT_TYPE_GLYPH_BUFFER,
    bl::ObjectLayer::ObjectMode::kDyn,
    bl::ObjectLayer::ObjectLayout::kUnknown>
  )
};

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_glyph_buffer_init(BLGlyphBufferCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_glyph_buffer_init_move(BLGlyphBufferCore* self, BLGlyphBufferCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_glyph_buffer_init_weak(BLGlyphBufferCore* self, const BLGlyphBufferCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_glyph_buffer_destroy(BLGlyphBufferCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_glyph_buffer_reset(BLGlyphBufferCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_glyph_buffer_clear(BLGlyphBufferCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_glyph_buffer_assign_move(BLGlyphBufferCore* self, BLGlyphBufferCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_glyph_buffer_assign_weak(BLGlyphBufferCore* self, const BLGlyphBufferCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_glyph_buffer_make_mutable(BLGlyphBufferCore* self, uint32_t** content_out, BLGlyphInfo** info_data_out, BLGlyphPlacement** placement_data_out) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_glyph_buffer_get_size(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API uint32_t BL_CDECL bl_glyph_buffer_get_flags(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const BLGlyphRun* BL_CDECL bl_glyph_buffer_get_glyph_run(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const uint32_t* BL_CDECL bl_glyph_buffer_get_content(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const BLGlyphInfo* BL_CDECL bl_glyph_buffer_get_info_data(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const BLGlyphPlacement* BL_CDECL bl_glyph_buffer_get_placement_data(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL bl_glyph_buffer_set_text(BLGlyphBufferCore* self, const void* text_data, size_t size, BLTextEncoding encoding) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_glyph_buffer_set_glyphs(BLGlyphBufferCore* self, const uint32_t* glyph_data, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_glyph_buffer_set_glyphs_from_struct(BLGlyphBufferCore* self, const void* glyph_data, size_t size, size_t glyph_id_size, intptr_t glyph_id_advance) BL_NOEXCEPT_C;

BL_EXTERN_C_END

//! \}
//! \}

//! \addtogroup bl_text
//! \{

//! \name BLGlyphBuffer - C++ API
//!
//! \{
#ifdef __cplusplus

//! Glyph buffer [C++ API].
//!
//! Can hold either text or glyphs and provides basic memory management that is used for text shaping, character to
//! glyph mapping, glyph substitution, and glyph positioning.
//!
//! Glyph buffer provides two separate buffers called 'primary' and 'secondary' that serve different purposes during
//! processing. Primary buffer always holds actual text/glyph array, and secondary buffer is either used as a scratch
//! buffer during glyph substitution or to hold glyph positions after the processing is complete and glyph positions
//! were calculated.
//!
//! Glyph buffers are implicitly shared. Modifying a shared buffer detaches its content, glyph information, and
//! placement data. Use \ref make_mutable() to obtain writable pointers before modifying the data directly.
class BLGlyphBuffer final : public BLGlyphBufferCore {
public:
  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLGlyphBuffer() noexcept { bl_glyph_buffer_init(this); }
  BL_INLINE_NODEBUG BLGlyphBuffer(BLGlyphBuffer&& other) noexcept { bl_glyph_buffer_init_move(this, &other); }
  BL_INLINE_NODEBUG BLGlyphBuffer(const BLGlyphBuffer& other) noexcept { bl_glyph_buffer_init_weak(this, &other); }
  BL_INLINE_NODEBUG ~BLGlyphBuffer() noexcept {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_glyph_buffer_destroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return !is_empty(); }

  BL_INLINE_NODEBUG BLGlyphBuffer& operator=(const BLGlyphBuffer& other) noexcept { bl_glyph_buffer_assign_weak(this, &other); return *this; }
  BL_INLINE_NODEBUG BLGlyphBuffer& operator=(BLGlyphBuffer&& other) noexcept { bl_glyph_buffer_assign_move(this, &other); return *this; }

  //! \}

  //! \name Accessors
  //! \{

  [[nodiscard]] BL_INLINE_NODEBUG bool is_empty() const noexcept { return _impl()->glyph_run.is_empty(); }

  [[nodiscard]] BL_INLINE_NODEBUG size_t size() const noexcept { return _impl()->size; }

  [[nodiscard]] BL_INLINE_NODEBUG uint32_t flags() const noexcept { return _impl()->flags; }

  [[nodiscard]] BL_INLINE_NODEBUG const uint32_t* content() const noexcept { return _impl()->content; }

  [[nodiscard]] BL_INLINE_NODEBUG const BLGlyphInfo* info_data() const noexcept { return _impl()->info_data; }

  [[nodiscard]] BL_INLINE_NODEBUG const BLGlyphPlacement* placement_data() const noexcept { return _impl()->placement_data; }

  //! Returns a view valid until this buffer is modified or destroyed. Treat the referenced data as read-only.
  [[nodiscard]] BL_INLINE_NODEBUG const BLGlyphRun& glyph_run() const noexcept { return _impl()->glyph_run; }

  //! Tests whether the glyph-buffer has `flag` set.
  [[nodiscard]] BL_INLINE_NODEBUG bool has_flag(uint32_t flag) const noexcept { return (_impl()->flags & flag) != 0; }

  //! Tests whether the buffer contains unicode data.
  [[nodiscard]] BL_INLINE_NODEBUG bool has_text() const noexcept { return has_flag(BL_GLYPH_RUN_FLAG_UCS4_CONTENT); }

  //! Tests whether the buffer contains glyph-id data.
  [[nodiscard]] BL_INLINE_NODEBUG bool has_glyphs() const noexcept { return !has_flag(BL_GLYPH_RUN_FLAG_UCS4_CONTENT); }

  //! Tests whether the input string contained invalid characters (unicode encoding errors).
  [[nodiscard]] BL_INLINE_NODEBUG bool has_invalid_chars() const noexcept { return has_flag(BL_GLYPH_RUN_FLAG_INVALID_TEXT); }

  //! Tests whether the input string contained undefined characters that weren't mapped properly to glyphs.
  [[nodiscard]] BL_INLINE_NODEBUG bool has_undefined_chars() const noexcept { return has_flag(BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS); }

  //! Tests whether one or more operation was terminated before completion because of invalid data in a font.
  [[nodiscard]] BL_INLINE_NODEBUG bool has_invalid_font_data() const noexcept { return has_flag(BL_GLYPH_RUN_FLAG_INVALID_FONT_DATA); }

  //! \}

  //! \name Operations
  //! \{

  //! Resets the \ref BLGlyphBuffer into its default constructed state. The content will be cleared and allocated
  //! memory released.
  BL_INLINE_NODEBUG BLResult reset() noexcept { return bl_glyph_buffer_reset(this); }

  //! Clears the content of \ref BLGlyphBuffer without releasing internal buffers.
  BL_INLINE_NODEBUG BLResult clear() noexcept { return bl_glyph_buffer_clear(this); }

  //! Swaps this glyph buffer with `other`.
  BL_INLINE_NODEBUG void swap(BLGlyphBuffer& other) noexcept { _d.swap(other._d); }

  //! Assigns a weak copy of `other` to this glyph buffer.
  BL_INLINE_NODEBUG BLResult assign(const BLGlyphBuffer& other) noexcept { return bl_glyph_buffer_assign_weak(this, &other); }

  //! Moves `other` into this glyph buffer and resets `other`.
  BL_INLINE_NODEBUG BLResult assign(BLGlyphBuffer&& other) noexcept { return bl_glyph_buffer_assign_move(this, &other); }

  //! Detaches shared data and optionally returns writable pointers to the existing data.
  //!
  //! Returned pointers remain valid until the buffer is modified or destroyed. Do not modify the data after
  //! sharing the buffer with another object; call `make_mutable()` again to obtain writable pointers.
  BL_INLINE_NODEBUG BLResult make_mutable(uint32_t** content_out = nullptr, BLGlyphInfo** info_data_out = nullptr, BLGlyphPlacement** placement_data_out = nullptr) noexcept {
    return bl_glyph_buffer_make_mutable(this, content_out, info_data_out, placement_data_out);
  }

  //! Assigns a text content of this \ref BLGlyphBuffer.
  //!
  //! This is a generic function that accepts `void*` data, which is specified by `encoding`. The `size` argument
  //! depends on encoding as well. If the encoding specifies byte string (LATIN1 or UTF8) then it's bytes, if the
  //! encoding specifies UTF16 or UTF32 then it would describe the number of `uint16_t` or `uint32_t` code points,
  //! respectively.
  //!
  //! Null-terminated string can be specified by passing `SIZE_MAX` as `size`.
  BL_INLINE_NODEBUG BLResult set_text(const void* text_data, size_t size, BLTextEncoding encoding) noexcept {
    return bl_glyph_buffer_set_text(this, text_data, size, encoding);
  }

  //! Assigns a text content of this \ref BLGlyphBuffer from LATIN1 (ISO/IEC 8859-1) string.
  BL_INLINE_NODEBUG BLResult set_latin1_text(const char* text, size_t size = SIZE_MAX) noexcept {
    return bl_glyph_buffer_set_text(this, text, size, BL_TEXT_ENCODING_LATIN1);
  }

  //! Assigns a text content of this \ref BLGlyphBuffer from UTF-8 encoded string. The `size` parameter represents the
  //! length of the `text` in bytes.
  BL_INLINE_NODEBUG BLResult set_utf8_text(const char* text, size_t size = SIZE_MAX) noexcept {
    return bl_glyph_buffer_set_text(this, text, size, BL_TEXT_ENCODING_UTF8);
  }
  //! \overload
  BL_INLINE_NODEBUG BLResult set_utf8_text(const uint8_t* text, size_t size = SIZE_MAX) noexcept {
    return bl_glyph_buffer_set_text(this, text, size, BL_TEXT_ENCODING_UTF8);
  }

  //! Assigns a text content of this \ref BLGlyphBuffer from UTF-16 encoded string. The `size` parameter represents the
  //! length of the `text` in 16-bit units.
  BL_INLINE_NODEBUG BLResult set_utf16_text(const uint16_t* text, size_t size = SIZE_MAX) noexcept {
    return bl_glyph_buffer_set_text(this, text, size, BL_TEXT_ENCODING_UTF16);
  }

  //! Assigns a text content of this \ref BLGlyphBuffer from UTF-32 encoded string. The `size` parameter represents the
  //! length of the `text` in 32-bit units.
  BL_INLINE_NODEBUG BLResult set_utf32_text(const uint32_t* text, size_t size = SIZE_MAX) noexcept {
    return bl_glyph_buffer_set_text(this, text, size, BL_TEXT_ENCODING_UTF32);
  }

  //! Assigns a text content of this \ref BLGlyphBuffer from `wchar_t` encoded string. The `size` parameter represents
  //! the length of the `text` in `wchar_t` units.
  BL_INLINE_NODEBUG BLResult set_wchar_text(const wchar_t* text, size_t size = SIZE_MAX) noexcept {
    return bl_glyph_buffer_set_text(this, text, size, BL_TEXT_ENCODING_WCHAR);
  }

  //! Assigns a glyph content of this \ref BLGlyphBuffer from either the given `glyph_data`.
  BL_INLINE_NODEBUG BLResult set_glyphs(const uint32_t* glyph_data, size_t size) noexcept {
    return bl_glyph_buffer_set_glyphs(this, glyph_data, size);
  }

  //! Assigns a glyph content of this \ref BLGlyphBuffer from an array of glyphs or from a foreign structure that
  //! contains glyphs and possibly other members that have to be skipped. The glyph size can be either 16-bit (2)
  //! or 32-bit (4). The last parameter `glyph_advance` specifies how many bytes to advance after a glyph value is
  //! read.
  BL_INLINE_NODEBUG BLResult set_glyphs_from_struct(const void* glyph_data, size_t size, size_t glyph_id_size, intptr_t glyph_advance) noexcept {
    return bl_glyph_buffer_set_glyphs_from_struct(this, glyph_data, size, glyph_id_size, glyph_advance);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_CORE_GLYPH_BUFFER_H_INCLUDED
