// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GLYPHBUFFER_H_INCLUDED
#define BLEND2D_GLYPHBUFFER_H_INCLUDED

#include "fontdefs.h"

//! \addtogroup blend2d_api_text
//! \{

//! \name BLGlyphBuffer - C API
//!
//! \{

//! Glyph buffer [Impl].
//!
//! \note This is not a `BLObjectImpl` compatible Impl.
struct BLGlyphBufferImpl {
  union {
    struct {
      //! Text (UCS4 code-points) or glyph content.
      uint32_t* content;
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
    //!
    //! Glyph run shares data with other members like `content`, `placementData`, `size`, and `flags`. When working
    //! with data it's better to access these members directly as they are typed, whereas `BLGlyphRun` stores pointers
    //! as `const void*` as it offers more flexibility, which `BLGlyphRun` doesn't need.
    BLGlyphRun glyphRun;
  };

  //! Glyph info data - additional information of each code-point or glyph.
  BLGlyphInfo* infoData;
};

//! Glyph buffer [C API].
struct BLGlyphBufferCore {
  BLGlyphBufferImpl* impl;
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blGlyphBufferInit(BLGlyphBufferCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGlyphBufferInitMove(BLGlyphBufferCore* self, BLGlyphBufferCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGlyphBufferDestroy(BLGlyphBufferCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGlyphBufferReset(BLGlyphBufferCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGlyphBufferClear(BLGlyphBufferCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blGlyphBufferGetSize(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API uint32_t BL_CDECL blGlyphBufferGetFlags(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const BLGlyphRun* BL_CDECL blGlyphBufferGetGlyphRun(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const uint32_t* BL_CDECL blGlyphBufferGetContent(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const BLGlyphInfo* BL_CDECL blGlyphBufferGetInfoData(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const BLGlyphPlacement* BL_CDECL blGlyphBufferGetPlacementData(const BLGlyphBufferCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blGlyphBufferSetText(BLGlyphBufferCore* self, const void* textData, size_t size, BLTextEncoding encoding) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGlyphBufferSetGlyphs(BLGlyphBufferCore* self, const uint32_t* glyphData, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blGlyphBufferSetGlyphsFromStruct(BLGlyphBufferCore* self, const void* glyphData, size_t size, size_t glyphIdSize, intptr_t glyphIdAdvance) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}

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
class BLGlyphBuffer : public BLGlyphBufferCore {
public:
  //! \name Constructors & Destructors
  //! \{

  BL_INLINE BLGlyphBuffer(const BLGlyphBuffer&) noexcept = delete;
  BL_INLINE BLGlyphBuffer& operator=(const BLGlyphBuffer&) noexcept = delete;

  BL_INLINE BLGlyphBuffer() noexcept { blGlyphBufferInit(this); }
  BL_INLINE BLGlyphBuffer(BLGlyphBuffer&& other) noexcept { blGlyphBufferInitMove(this, &other); }
  BL_INLINE ~BLGlyphBuffer() noexcept { blGlyphBufferDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !empty(); }

  //! \}

  //! \name Accessors
  //! \{

  BL_NODISCARD
  BL_INLINE bool empty() const noexcept { return impl->glyphRun.empty(); }

  BL_NODISCARD
  BL_INLINE size_t size() const noexcept { return impl->size; }

  BL_NODISCARD
  BL_INLINE uint32_t flags() const noexcept { return impl->flags; }

  BL_NODISCARD
  BL_INLINE uint32_t* content() const noexcept { return impl->content; }

  BL_NODISCARD
  BL_INLINE BLGlyphInfo* infoData() const noexcept { return impl->infoData; }

  BL_NODISCARD
  BL_INLINE BLGlyphPlacement* placementData() const noexcept { return impl->placementData; }

  BL_NODISCARD
  BL_INLINE const BLGlyphRun& glyphRun() const noexcept { return impl->glyphRun; }

  //! Tests whether the glyph-buffer has `flag` set.
  BL_NODISCARD
  BL_INLINE bool hasFlag(uint32_t flag) const noexcept { return (impl->flags & flag) != 0; }

  //! Tests whether the buffer contains unicode data.
  BL_NODISCARD
  BL_INLINE bool hasText() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_UCS4_CONTENT); }

  //! Tests whether the buffer contains glyph-id data.
  BL_NODISCARD
  BL_INLINE bool hasGlyphs() const noexcept { return !hasFlag(BL_GLYPH_RUN_FLAG_UCS4_CONTENT); }

  //! Tests whether the input string contained invalid characters (unicode encoding errors).
  BL_NODISCARD
  BL_INLINE bool hasInvalidChars() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_INVALID_TEXT); }

  //! Tests whether the input string contained undefined characters that weren't mapped properly to glyphs.
  BL_NODISCARD
  BL_INLINE bool hasUndefinedChars() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS); }

  //! Tests whether one or more operation was terminated before completion because of invalid data in a font.
  BL_NODISCARD
  BL_INLINE bool hasInvalidFontData() const noexcept { return hasFlag(BL_GLYPH_RUN_FLAG_INVALID_FONT_DATA); }

  //! \}

  //! \name Operations
  //! \{

  //! Resets the `BLGlyphBuffer` into its construction state. The content will be cleared and allocated memory
  //! released.
  BL_INLINE BLResult reset() noexcept {
    return blGlyphBufferReset(this);
  }

  //! Clears the content of `BLGlyphBuffer` without releasing internal buffers.
  BL_INLINE BLResult clear() noexcept {
    return blGlyphBufferClear(this);
  }

  //! Assigns a text content of this `BLGlyphBuffer`.
  //!
  //! This is a generic function that accepts `void*` data, which is specified by `encoding`. The `size` argument
  //! depends on encoding as well. If the encoding specifies byte string (LATIN1 or UTF8) then it's bytes, if the
  //! encoding specifies UTF16 or UTF32 then it would describe the number of `uint16_t` or `uint32_t` code points,
  //! respectively.
  //!
  //! Null-terminated string can be specified by passing `SIZE_MAX` as `size`.
  BL_INLINE BLResult setText(const void* textData, size_t size, BLTextEncoding encoding) noexcept {
    return blGlyphBufferSetText(this, textData, size, encoding);
  }

  //! Assigns a text content of this `BLGlyphBuffer` from LATIN1 (ISO/IEC 8859-1) string.
  BL_INLINE BLResult setLatin1Text(const char* text, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, text, size, BL_TEXT_ENCODING_LATIN1);
  }

  //! Assigns a text content of this `BLGlyphBuffer` from UTF-8 encoded string. The `size` parameter represents the
  //! length of the `text` in bytes.
  BL_INLINE BLResult setUtf8Text(const char* text, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, text, size, BL_TEXT_ENCODING_UTF8);
  }
  //! \overload
  BL_INLINE BLResult setUtf8Text(const uint8_t* text, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, text, size, BL_TEXT_ENCODING_UTF8);
  }

  //! Assigns a text content of this `BLGlyphBuffer` from UTF-16 encoded string. The `size` parameter represents the
  //! length of the `text` in 16-bit units.
  BL_INLINE BLResult setUtf16Text(const uint16_t* text, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, text, size, BL_TEXT_ENCODING_UTF16);
  }

  //! Assigns a text content of this `BLGlyphBuffer` from UTF-32 encoded string. The `size` parameter represents the
  //! length of the `text` in 32-bit units.
  BL_INLINE BLResult setUtf32Text(const uint32_t* text, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, text, size, BL_TEXT_ENCODING_UTF32);
  }

  //! Assigns a text content of this `BLGlyphBuffer` from `wchar_t` encoded string. The `size` parameter represents
  //! the length of the `text` in `wchar_t` units.
  BL_INLINE BLResult setWCharText(const wchar_t* text, size_t size = SIZE_MAX) noexcept {
    return blGlyphBufferSetText(this, text, size, BL_TEXT_ENCODING_WCHAR);
  }

  //! Assigns a glyph content of this `BLGlyphBuffer` from either the given `glyphData`.
  BL_INLINE BLResult setGlyphs(const uint32_t* glyphData, size_t size) noexcept {
    return blGlyphBufferSetGlyphs(this, glyphData, size);
  }

  //! Assigns a glyph content of this `BLGlyphBuffer` from an array of glyphs or from a foreign structure that contains
  //! glyphs and possibly other members that have to be skipped. The glyph size can be either 16-bit (2) or 32-bit (4).
  //! The last parameter `glyphAdvance` specifies how many bytes to advance after a glyph value is read.
  BL_INLINE BLResult setGlyphsFromStruct(const void* glyphData, size_t size, size_t glyphIdSize, intptr_t glyphAdvance) noexcept {
    return blGlyphBufferSetGlyphsFromStruct(this, glyphData, size, glyphIdSize, glyphAdvance);
  }

//! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_GLYPHBUFFER_H_INCLUDED
