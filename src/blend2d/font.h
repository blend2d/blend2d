// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONT_H_INCLUDED
#define BLEND2D_FONT_H_INCLUDED

#include "array.h"
#include "bitset.h"
#include "filesystem.h"
#include "fontdata.h"
#include "fontdefs.h"
#include "fontface.h"
#include "fontfeaturesettings.h"
#include "fontvariationsettings.h"
#include "geometry.h"
#include "glyphbuffer.h"
#include "glyphrun.h"
#include "object.h"
#include "path.h"
#include "string.h"

//! \addtogroup blend2d_api_text
//! \{

//! \name BLFont - C API
//! \{

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blFontInit(BLFontCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontInitMove(BLFontCore* self, BLFontCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontInitWeak(BLFontCore* self, const BLFontCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDestroy(BLFontCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontReset(BLFontCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontAssignMove(BLFontCore* self, BLFontCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontAssignWeak(BLFontCore* self, const BLFontCore* other) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontEquals(const BLFontCore* a, const BLFontCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontCreateFromFace(BLFontCore* self, const BLFontFaceCore* face, float size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontCreateFromFaceWithSettings(BLFontCore* self, const BLFontFaceCore* face, float size, const BLFontFeatureSettingsCore* featureSettings, const BLFontVariationSettingsCore* variationSettings) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetFace(const BLFontCore* self, BLFontFaceCore* out) BL_NOEXCEPT_C;
BL_API float BL_CDECL blFontGetSize(const BLFontCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontSetSize(BLFontCore* self, float size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetMetrics(const BLFontCore* self, BLFontMetrics* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetMatrix(const BLFontCore* self, BLFontMatrix* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetDesignMetrics(const BLFontCore* self, BLFontDesignMetrics* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetFeatureSettings(const BLFontCore* self, BLFontFeatureSettingsCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontSetFeatureSettings(BLFontCore* self, const BLFontFeatureSettingsCore* featureSettings) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontResetFeatureSettings(BLFontCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetVariationSettings(const BLFontCore* self, BLFontVariationSettingsCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontSetVariationSettings(BLFontCore* self, const BLFontVariationSettingsCore* variationSettings) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontResetVariationSettings(BLFontCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontShape(const BLFontCore* self, BLGlyphBufferCore* gb) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontMapTextToGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb, BLGlyphMappingState* stateOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontPositionGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontApplyKerning(const BLFontCore* self, BLGlyphBufferCore* gb) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontApplyGSub(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitArrayCore* lookups) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontApplyGPos(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitArrayCore* lookups) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetTextMetrics(const BLFontCore* self, BLGlyphBufferCore* gb, BLTextMetrics* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetGlyphBounds(const BLFontCore* self, const uint32_t* glyphData, intptr_t glyphAdvance, BLBoxI* out, size_t count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetGlyphAdvances(const BLFontCore* self, const uint32_t* glyphData, intptr_t glyphAdvance, BLGlyphPlacement* out, size_t count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetGlyphOutlines(const BLFontCore* self, BLGlyphId glyphId, const BLMatrix2D* userTransform, BLPathCore* out, BLPathSinkFunc sink, void* userData) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetGlyphRunOutlines(const BLFontCore* self, const BLGlyphRun* glyphRun, const BLMatrix2D* userTransform, BLPathCore* out, BLPathSinkFunc sink, void* userData) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! Font [C API].
struct BLFontCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLFont)
};

//! \}

//! \cond INTERNAL
//! \name BLFont - Internals
//! \{

//! Font [Impl].
struct BLFontImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Font face used by this font.
  BLFontFaceCore face;

  //! Font width (1..1000) [0 if the font is not initialized].
  uint16_t weight;
  //! Font stretch (1..9) [0 if the font is not initialized].
  uint8_t stretch;
  //! Font style.
  uint8_t style;
  //! Reserved for future use.
  uint32_t reserved;
  //! Font metrics.
  BLFontMetrics metrics;
  //! Font matrix.
  BLFontMatrix matrix;

  //! Assigned font features (key/value pairs).
  BLFontFeatureSettingsCore featureSettings;
  //! Assigned font variations (key/value pairs).
  BLFontVariationSettingsCore variationSettings;
};

//! \}
//! \endcond

//! \name BLFont - C++ API
//! \{

#ifdef __cplusplus

//! Font [C++ API].
class BLFont final : public BLFontCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  BL_INLINE_NODEBUG BLFontImpl* _impl() const noexcept { return static_cast<BLFontImpl*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLFont() noexcept { blFontInit(this); }
  BL_INLINE_NODEBUG BLFont(BLFont&& other) noexcept { blFontInitMove(this, &other); }
  BL_INLINE_NODEBUG BLFont(const BLFont& other) noexcept { blFontInitWeak(this, &other); }

  BL_INLINE_NODEBUG ~BLFont() noexcept {
    if (BLInternal::objectNeedsCleanup(_d.info.bits))
      blFontDestroy(this);
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE_NODEBUG BLFont& operator=(BLFont&& other) noexcept { blFontAssignMove(this, &other); return *this; }
  BL_INLINE_NODEBUG BLFont& operator=(const BLFont& other) noexcept { blFontAssignWeak(this, &other); return *this; }

  BL_INLINE_NODEBUG bool operator==(const BLFont& other) const noexcept { return  equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLFont& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept { return blFontReset(this); }
  BL_INLINE_NODEBUG void swap(BLFont& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLFont&& other) noexcept { return blFontAssignMove(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLFont& other) noexcept { return blFontAssignWeak(this, &other); }

  //! Tests whether the font is a valid instance.
  BL_INLINE_NODEBUG bool isValid() const noexcept { return _impl()->face.dcast().isValid(); }
  //! Tests whether the font is empty, which is the same as `!isValid()`.
  BL_INLINE_NODEBUG bool empty() const noexcept { return !isValid(); }

  BL_INLINE_NODEBUG bool equals(const BLFontCore& other) const noexcept { return blFontEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Creates a new font from the existing font `face` scaled to the given `size`.
  BL_INLINE_NODEBUG BLResult createFromFace(const BLFontFaceCore& face, float size) noexcept {
    return blFontCreateFromFace(this, &face, size);
  }

  //! Creates a new font from the existing font `face` scaled to the given `size`.
  //!
  //! This is an overloaded function that takes additional argument `featureSettings.
  BL_INLINE_NODEBUG BLResult createFromFace(const BLFontFaceCore& face, float size, const BLFontFeatureSettingsCore& featureSettings) noexcept {
    return blFontCreateFromFaceWithSettings(this, &face, size, &featureSettings, nullptr);
  }

  //! Creates a new font from the existing font `face` scaled to the given `size`.
  //!
  //! This is an overloaded function that takes additional arguments, which are used to override font `featureSettings`
  //! and font `variationSettings`.
  BL_INLINE_NODEBUG BLResult createFromFace(const BLFontFaceCore& face, float size, const BLFontFeatureSettingsCore& featureSettings, const BLFontVariationSettingsCore& variationSettings) noexcept {
    return blFontCreateFromFaceWithSettings(this, &face, size, &featureSettings, &variationSettings);
  }

  //! \}

  //! \name Accessors
  //! \{

  //! Returns the type of the font's associated font face.
  BL_INLINE_NODEBUG BLFontFaceType faceType() const noexcept { return face().faceType(); }
  //! Returns the flags of the font.
  BL_INLINE_NODEBUG BLFontFaceFlags faceFlags() const noexcept { return face().faceFlags(); }

  //! Returns the size of the font (as float).
  BL_INLINE_NODEBUG float size() const noexcept { return _impl()->metrics.size; }
  //! Sets the font size to `size`.
  BL_INLINE_NODEBUG BLResult setSize(float size) noexcept { return blFontSetSize(this, size); }

  //! Returns the font's associated font face.
  //!
  //! Returns the same font face, which was passed to `createFromFace()`.
  BL_INLINE_NODEBUG const BLFontFace& face() const noexcept { return _impl()->face.dcast(); }

  //! Returns the weight of the font.
  BL_INLINE_NODEBUG uint32_t weight() const noexcept { return _impl()->weight; }
  //! Returns the stretch of the font.
  BL_INLINE_NODEBUG uint32_t stretch() const noexcept { return _impl()->stretch; }
  //! Returns the style of the font.
  BL_INLINE_NODEBUG uint32_t style() const noexcept { return _impl()->style; }

  //! Returns the "units per em" (UPEM) of the font's associated font face.
  BL_INLINE_NODEBUG int unitsPerEm() const noexcept { return face().unitsPerEm(); }

  //! Returns a 2x2 matrix of the font.
  //!
  //! The returned `BLFontMatrix` is used to scale fonts from design units into user units. The matrix
  //! usually has a negative `m11` member as fonts use a different coordinate system than Blend2D.
  BL_INLINE_NODEBUG const BLFontMatrix& matrix() const noexcept { return _impl()->matrix; }

  //! Returns the scaled metrics of the font.
  //!
  //! The returned metrics is a scale of design metrics that match the font size and its options.
  BL_INLINE_NODEBUG const BLFontMetrics& metrics() const noexcept { return _impl()->metrics; }

  //! Returns the design metrics of the font.
  //!
  //! The returned metrics is compatible with the metrics of `BLFontFace` associated with this font.
  BL_INLINE_NODEBUG const BLFontDesignMetrics& designMetrics() const noexcept { return face().designMetrics(); }

  //! Returns font feature settings.
  BL_INLINE_NODEBUG const BLFontFeatureSettings& featureSettings() const noexcept { return _impl()->featureSettings.dcast(); }
  //! Sets font feature settings to `featureSettings`.
  BL_INLINE_NODEBUG BLResult setFeatureSettings(const BLFontFeatureSettingsCore& featureSettings) noexcept { return blFontSetFeatureSettings(this, &featureSettings); }
  //! Resets font feature settings.
  BL_INLINE_NODEBUG BLResult resetFeatureSettings() noexcept { return blFontResetFeatureSettings(this); }

  //! Returns font variation settings.
  BL_INLINE_NODEBUG const BLFontVariationSettings& variationSettings() const noexcept { return _impl()->variationSettings.dcast(); }
  //! Sets font variation settings to `variationSettings`.
  BL_INLINE_NODEBUG BLResult setVariationSettings(const BLFontVariationSettingsCore& variationSettings) noexcept { return blFontSetVariationSettings(this, &variationSettings); }
  //! Resets font variation settings.
  BL_INLINE_NODEBUG BLResult resetVariationSettings() noexcept { return blFontResetVariationSettings(this); }

  //! \}

  //! \name Glyphs & Text
  //! \{

  BL_INLINE_NODEBUG BLResult shape(BLGlyphBufferCore& gb) const noexcept {
    return blFontShape(this, &gb);
  }

  BL_INLINE_NODEBUG BLResult mapTextToGlyphs(BLGlyphBufferCore& gb) const noexcept {
    return blFontMapTextToGlyphs(this, &gb, nullptr);
  }

  BL_INLINE_NODEBUG BLResult mapTextToGlyphs(BLGlyphBufferCore& gb, BLGlyphMappingState& stateOut) const noexcept {
    return blFontMapTextToGlyphs(this, &gb, &stateOut);
  }

  BL_INLINE_NODEBUG BLResult positionGlyphs(BLGlyphBufferCore& gb) const noexcept {
    return blFontPositionGlyphs(this, &gb);
  }

  BL_INLINE_NODEBUG BLResult applyKerning(BLGlyphBufferCore& gb) const noexcept {
    return blFontApplyKerning(this, &gb);
  }

  BL_INLINE_NODEBUG BLResult applyGSub(BLGlyphBufferCore& gb, const BLBitArrayCore& lookups) const noexcept {
    return blFontApplyGSub(this, &gb, &lookups);
  }

  BL_INLINE_NODEBUG BLResult applyGPos(BLGlyphBufferCore& gb, const BLBitArrayCore& lookups) const noexcept {
    return blFontApplyGPos(this, &gb, &lookups);
  }

  BL_INLINE_NODEBUG BLResult getTextMetrics(BLGlyphBufferCore& gb, BLTextMetrics& out) const noexcept {
    return blFontGetTextMetrics(this, &gb, &out);
  }

  BL_INLINE_NODEBUG BLResult getGlyphBounds(const uint32_t* glyphData, intptr_t glyphAdvance, BLBoxI* out, size_t count) const noexcept {
    return blFontGetGlyphBounds(this, glyphData, glyphAdvance, out, count);
  }

  BL_INLINE_NODEBUG BLResult getGlyphAdvances(const uint32_t* glyphData, intptr_t glyphAdvance, BLGlyphPlacement* out, size_t count) const noexcept {
    return blFontGetGlyphAdvances(this, glyphData, glyphAdvance, out, count);
  }

  BL_INLINE_NODEBUG BLResult getGlyphOutlines(BLGlyphId glyphId, BLPathCore& out, BLPathSinkFunc sink = nullptr, void* userData = nullptr) const noexcept {
    return blFontGetGlyphOutlines(this, glyphId, nullptr, &out, sink, userData);
  }

  BL_INLINE_NODEBUG BLResult getGlyphOutlines(BLGlyphId glyphId, const BLMatrix2D& userTransform, BLPathCore& out, BLPathSinkFunc sink = nullptr, void* userData = nullptr) const noexcept {
    return blFontGetGlyphOutlines(this, glyphId, &userTransform, &out, sink, userData);
  }

  BL_INLINE_NODEBUG BLResult getGlyphRunOutlines(const BLGlyphRun& glyphRun, BLPathCore& out, BLPathSinkFunc sink = nullptr, void* userData = nullptr) const noexcept {
    return blFontGetGlyphRunOutlines(this, &glyphRun, nullptr, &out, sink, userData);
  }

  BL_INLINE_NODEBUG BLResult getGlyphRunOutlines(const BLGlyphRun& glyphRun, const BLMatrix2D& userTransform, BLPathCore& out, BLPathSinkFunc sink = nullptr, void* userData = nullptr) const noexcept {
    return blFontGetGlyphRunOutlines(this, &glyphRun, &userTransform, &out, sink, userData);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_FONT_H_INCLUDED
