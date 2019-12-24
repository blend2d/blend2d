// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_GLYPHBUFFER_H
#define BLEND2D_GLYPHBUFFER_H

#include "./fontdefs.h"

//! \addtogroup blend2d_api_text
//! \{

// ============================================================================
// [BLGlyphBuffer - Core]
// ============================================================================

//! Glyph buffer [C Interface - Impl].
//!
//! \note This is not a `BLVariantImpl` compatible Impl.
struct BLGlyphBufferImpl {
  union {
    struct {
      //! Glyph item data.
      BLGlyphItem* glyphItemData;
      //! Glyph placement data.
      BLGlyphPlacement* placementData;
      //! Number of either code points or glyph indexes in the glyph-buffer.
      size_t size;
      //! Reserved, used exclusively by BLGlyphRun.
      uint32_t reserved;
      //! Flags shared between BLGlyphRun and BLGlyphBuffer.
      uint32_t flags;
    };

    //! Glyph run data that can be passed directly to the rendering context.
    BLGlyphRun glyphRun;
  };

  //! Glyph info data - additional information of each `BLGlyphItem`.
  BLGlyphInfo* glyphInfoData;
};

//! Glyph buffer [C Interface - Core].
struct BLGlyphBufferCore {
  BLGlyphBufferImpl* impl;
};

// ============================================================================
// [BLGlyphBuffer - C++]
// ============================================================================

#ifdef __cplusplus
//! Glyph buffer [C++ API].
//!
//! Can hold either text or glyphs and provides basic memory management that is
//! used for text shaping, character to glyph mapping, glyph substitution, and
//! glyph positioning.
//!
//! Glyph buffer provides two separate buffers called 'primary' and 'secondary'
//! that serve different purposes during processing. Primary buffer always holds
//! actualy `BLGlyphItem` array, and secondary buffer is either used as a scratch
//! buffer during glyph substitution or hold glyph positions after the processing
//! is complete and glyph positions were calculated.
class BLGlyphBuffer : public BLGlyphBufferCore {
public:
  //! \name Constructors & Destructors
  //! \{

  BL_INLINE BLGlyphBuffer(const BLGlyphBuffer&) noexcept = delete;
  BL_INLINE BLGlyphBuffer& operator=(const BLGlyphBuffer&) noexcept = delete;

  BL_INLINE BLGlyphBuffer() noexcept { blGlyphBufferInit(this); }
  BL_INLINE BLGlyphBuffer(BLGlyphBuffer&& other) noexcept { blGlyphBufferInitMove(this, &other); }
  BL_INLINE ~BLGlyphBuffer() noexcept { blGlyphBufferReset(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !empty(); }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE bool empty() const noexcept { return impl->glyphRun.empty(); }

  BL_INLINE size_t size() const noexcept { return impl->size; }
  BL_INLINE uint32_t flags() const noexcept { return impl->flags; }

  BL_INLINE BLGlyphItem* glyphItemData() const noexcept { return impl->glyphItemData; }
  BL_INLINE BLGlyphPlacement* placementData() const noexcept { return impl->placementData; }
  BL_INLINE BLGlyphInfo* glyphInfoData() const noexcept { return impl->glyphInfoData; }

  BL_INLINE const BLGlyphRun& glyphRun() const noexcept { return impl->glyphRun; }

  //! Tests whether the glyph-buffer has `flag` set.
  BL_INLINE bool hasFlag(uint32_t flag) const noexcept { return (impl->flags & flag) != 0; }
  //! Tests whether the buffer contains unicode data.
  BL_INLINE bool hasText() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_UCS4_CONTENT); }
  //! Tests whether the buffer contains glyph-id data.
  BL_INLINE bool hasGlyphs() const noexcept { return !hasFlag(BL_GLYPH_RUN_FLAG_UCS4_CONTENT); }

  //! Tests whether the input string contained invalid characters (unicode encoding errors).
  BL_INLINE bool hasInvalidChars() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_INVALID_TEXT); }
  //! Tests whether the input string contained undefined characters that weren't mapped properly to glyphs.
  BL_INLINE bool hasUndefinedChars() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS); }
  //! Tests whether one or more operation was terminated before completion because of invalid data in a font.
  BL_INLINE bool hasInvalidFontData() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_INVALID_FONT_DATA); }

  //! \}

  //! \name Operations
  //! \{

  //! Resets the `BLGlyphBuffer` into its construction state. The content will
  //! be cleared and allocated memory released.
  BL_INLINE BLResult reset() noexcept {
    return blGlyphBufferReset(this);
  }

  //! Clears the content of `BLGlyphBuffer` without releasing internal buffers.
  BL_INLINE BLResult clear() noexcept {
    return blGlyphBufferClear(this);
  }

  //! Sets the text content of this `BLGlyphBuffer`.
  //!
  //! This is a generic function that accepts `void*` data, which is specified
  //! by `encoding`. The `size` argument depends on encoding as well. If the
  //! encoding specifies byte string (LATIN1 or UTF8) then it's bytes, if the
  //! encoding specifies UTF16 or UTF32 then it would describe the number of
  //! `uint16_t` or `uint32_t` code points, respectively.
  //!
  //! Null-terminated string can be specified by passing `SIZE_MAX` as `size`.
  BL_INLINE BLResult setText(const void* data, size_t size, uint32_t encoding) noexcept {
    return blGlyphBufferSetText(this, data, size, encoding);
  }

  //! Sets the text content of this `BLGlyphBuffer` from LATIN1 (ISO/IEC 8859-1)
  //! string.
  BL_INLINE BLResult setLatin1Text(const char* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_LATIN1);
  }

  //! Sets the text content of this `BLGlyphBuffer` from UTF-8 encoded string.
  //! The `size` parameter represents the length of the `data` in bytes.
  BL_INLINE BLResult setUtf8Text(const char* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_UTF8);
  }
  //! \overload
  BL_INLINE BLResult setUtf8Text(const uint8_t* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_UTF8);
  }

  //! Sets the text content of this `BLGlyphBuffer` from UTF-16 encoded string.
  //! The `size` parameter represents the length of the `data` in 16-bit units.
  BL_INLINE BLResult setUtf16Text(const uint16_t* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_UTF16);
  }

  //! Sets the text content of this `BLGlyphBuffer` from UTF-32 encoded string.
  //! The `size` parameter represents the length of the `data` in 32-bit units.
  BL_INLINE BLResult setUtf32Text(const uint32_t* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_UTF32);
  }

  //! Sets the text content of this `BLGlyphBuffer` from `wchar_t` encoded string.
  //! The `size` parameter represents the length of the `data` in `wchar_t` units.
  BL_INLINE BLResult setWCharText(const wchar_t* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_WCHAR);
  }

  //! Sets the glyph content of this `BLGlyphBuffer` from either `uint16_t` array
  //! or any structure that contains 16-bit glyph ids. The `advance` parameter
  //! specifies the size of such structure (set to sizeof(uint16_t) for uint16_t
  //! array) and `size` specifies how many glyph ids the passed `data` represents.
  BL_INLINE BLResult setGlyphIds(const void* data, intptr_t advance, size_t size) noexcept {
    return blGlyphBufferSetGlyphIds(this, data, advance, size);
  }

  //! \}
};
#endif

//! \}

#endif // BLEND2D_GLYPHBUFFER_H
