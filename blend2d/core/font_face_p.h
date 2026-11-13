// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_FONT_FACE_P_H_INCLUDED
#define BLEND2D_CORE_FONT_FACE_P_H_INCLUDED

#include <blend2d/axl/scopedbuffer_p.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/coverage_set_p.h>
#include <blend2d/core/font.h>
#include <blend2d/core/font_tag_set_p.h>
#include <blend2d/core/matrix_p.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL

namespace bl {

//! \addtogroup blend2d_internal
//! \{

//! \name BLFontFace - Internal Memory Management
//! \{

struct FontFacePrivateFuncs {
  BLResult (BL_CDECL* map_text_to_glyphs)(
    const BLFontFaceImpl* impl,
    uint32_t* content,
    size_t count,
    BLGlyphMappingState* state) noexcept;

  BLResult (BL_CDECL* get_glyph_bounds)(
    const BLFontFaceImpl* impl,
    const uint32_t* glyph_data,
    intptr_t glyph_advance,
    BLBoxI* boxes,
    size_t count) noexcept;

  BLResult (BL_CDECL* get_glyph_advances)(
    const BLFontFaceImpl* impl,
    const uint32_t* glyph_data,
    intptr_t glyph_advance,
    BLGlyphPlacement* placement_data,
    size_t count) noexcept;

  BLResult (BL_CDECL* get_glyph_outlines)(
    const BLFontFaceImpl* impl,
    BLGlyphId glyph_id,
    const BLMatrix2D* user_transform,
    BLPath* out,
    size_t* contour_count_out,
    bl::ScopedBuffer* tmp_buffer) noexcept;

  BLResult (BL_CDECL* apply_kern)(
    const BLFontFaceImpl* face_impl,
    uint32_t* glyph_data,
    BLGlyphPlacement* placement_data,
    size_t count) noexcept;

  BLResult (BL_CDECL* apply_gsub)(
    const BLFontFaceImpl* impl,
    BLGlyphBuffer* gb,
    const BLBitWord* bit_words,
    size_t bit_word_count) noexcept;

  BLResult (BL_CDECL* apply_gpos)(
    const BLFontFaceImpl* impl,
    BLGlyphBuffer* gb,
    const BLBitWord* bit_words,
    size_t bit_word_count) noexcept;

  BLResult (BL_CDECL* position_glyphs)(
    const BLFontFaceImpl* impl,
    uint32_t* glyph_data,
    BLGlyphPlacement* placement_data,
    size_t count) noexcept;
};

BL_HIDDEN extern FontFacePrivateFuncs null_font_face_funcs;

struct FontFacePrivateImpl : public BLFontFaceImpl {
  FontFacePrivateFuncs funcs;
  bl::FontTagData::ScriptTagSet script_tag_set;
  bl::FontTagData::FeatureTagSet feature_tag_set;
  bl::FontTagData::VariationTagSet variation_tag_set;
  bl::ObjectLayer::AtomicObject<BLCoverageSetCore> cmap_coverage;
};

static BL_INLINE void bl_font_face_impl_ctor(FontFacePrivateImpl* impl, FontFacePrivateFuncs& funcs) noexcept {
  impl->funcs = funcs;
  bl_call_ctor(impl->data.dcast());
  bl_call_ctor(impl->full_name.dcast());
  bl_call_ctor(impl->family_name.dcast());
  bl_call_ctor(impl->subfamily_name.dcast());
  bl_call_ctor(impl->post_script_name.dcast());
  bl_call_ctor(impl->script_tag_set);
  bl_call_ctor(impl->feature_tag_set);
  bl_call_ctor(impl->variation_tag_set);
  ObjectLayer::atomic_object_init_as_unpopulated(impl->cmap_coverage);
}

static BL_INLINE void bl_font_face_impl_dtor(FontFacePrivateImpl* impl) noexcept {
  ObjectLayer::atomic_object_destroy_populated(impl->cmap_coverage);
  bl_call_dtor(impl->variation_tag_set);
  bl_call_dtor(impl->feature_tag_set);
  bl_call_dtor(impl->script_tag_set);
  bl_call_dtor(impl->post_script_name.dcast());
  bl_call_dtor(impl->subfamily_name.dcast());
  bl_call_dtor(impl->family_name.dcast());
  bl_call_dtor(impl->full_name.dcast());
  bl_call_dtor(impl->data.dcast());
}

template<>
struct ObjectDispatchInfo<BLFontFaceCore> {
  using ImplType = FontFacePrivateImpl;

  static constexpr bool kHasInitSSO = false;

  static BL_INLINE BLResult free_object(BLFontFaceCore* self, ImplType* impl) noexcept {
    axl::maybe_unused(impl);
    return self->_virt()->base.destroy(self);
  }
};

//! \}
//! \}

} // {bl}

//! \endcond

#endif // BLEND2D_CORE_FONT_FACE_P_H_INCLUDED
