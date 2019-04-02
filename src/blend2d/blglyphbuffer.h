// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLGLYPHBUFFER_H
#define BLEND2D_BLGLYPHBUFFER_H

#include "./blfontdefs.h"

//! \addtogroup blend2d_api_text
//! \{

// ============================================================================
// [BLGlyphBuffer - Core]
// ============================================================================

//! Glyph buffer [C Data].
struct BLGlyphBufferData {
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
  BLGlyphBufferData* data;
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
  BL_INLINE BLGlyphBuffer(const BLGlyphBuffer&) noexcept = delete;
  BL_INLINE BLGlyphBuffer& operator=(const BLGlyphBuffer&) noexcept = delete;

  BL_INLINE BLGlyphBuffer() noexcept { blGlyphBufferInit(this); }
  BL_INLINE ~BLGlyphBuffer() noexcept { blGlyphBufferReset(this); }

  BL_INLINE explicit operator bool() const noexcept { return !empty(); }
  BL_INLINE bool empty() const noexcept { return data->glyphRun.empty(); }

  BL_INLINE size_t size() const noexcept { return data->size; }
  BL_INLINE uint32_t flags() const noexcept { return data->flags; }

  BL_INLINE BLGlyphItem* glyphItemData() const noexcept { return data->glyphItemData; }
  BL_INLINE BLGlyphPlacement* placementData() const noexcept { return data->placementData; }
  BL_INLINE BLGlyphInfo* glyphInfoData() const noexcept { return data->glyphInfoData; }

  BL_INLINE const BLGlyphRun& glyphRun() const noexcept { return data->glyphRun; }

  //! Gets whether the glyph-buffer has `flag` set.
  BL_INLINE bool hasFlag(uint32_t flag) const noexcept { return (data->flags & flag) != 0; }
  //! Get whether this buffer contains unicode data.
  BL_INLINE bool hasText() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_UCS4_CONTENT); }
  //! Get whether this buffer contains glyph-id data.
  BL_INLINE bool hasGlyphs() const noexcept { return !hasFlag(BL_GLYPH_RUN_FLAG_UCS4_CONTENT); }

  //! Gets whether the input string contained invalid characters (unicode encoding errors).
  BL_INLINE bool hasInvalidChars() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_INVALID_TEXT); }
  //! Gets whether the input string contained undefined characters that weren't mapped properly to glyphs.
  BL_INLINE bool hasUndefinedChars() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS); }
  //! Gets whether one or more operation was terminated before completion because of invalid data in a font.
  BL_INLINE bool hasInvalidFontData() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_INVALID_FONT_DATA); }

  //! Resets the `BLGlyphBuffer` into its construction state. The content will
  //! be cleared and allocated memory released.
  BL_INLINE BLResult reset() noexcept {
    return blGlyphBufferReset(this);
  }

  //! Clears the content of `BLGlyphBuffer` without releasing internal buffers.
  BL_INLINE BLResult clear() noexcept {
    return blGlyphBufferClear(this);
  }

  //! Sets text content of this `BLGlyphBuffer`.
  //!
  //! This is a generic function that accepts `void*` data, whicih is specified
  //! by `encoding`. The `size` argument depends on encoding as well. If the
  //! encoding specifies byte string (LATIN1 or UTF8) then it's bytes, if the
  //! encoding specifies UTF16 or UTF32 then it would describe the number of
  //! `uint16_t` or `uint32_t` code points, respectively.
  //!
  //! Null-terminated string can be specified by passing `SIZE_MAX` as `size`.
  BL_INLINE BLResult setText(const void* data, size_t size, uint32_t encoding) noexcept {
    return blGlyphBufferSetText(this, data, size, encoding);
  }

  BL_INLINE BLResult setLatin1Text(const char* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_LATIN1);
  }

  BL_INLINE BLResult setUtf8Text(const char* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_UTF8);
  }

  BL_INLINE BLResult setUtf8Text(const uint8_t* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_UTF8);
  }

  BL_INLINE BLResult setUtf16Text(const uint16_t* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_UTF16);
  }

  BL_INLINE BLResult setUtf32Text(const uint32_t* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_UTF32);
  }

  BL_INLINE BLResult setWCharText(const wchar_t* data, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, data, size, BL_TEXT_ENCODING_WCHAR);
  }

  BL_INLINE BLResult setGlyphIds(const void* data, intptr_t advance, size_t size) noexcept {
    return blGlyphBufferSetGlyphIds(this, data, advance, size);
  }
};
#endif

//! \}

#endif // BLEND2D_BLGLYPHBUFFER_H
