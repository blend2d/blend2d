// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_FONT_P_H_INCLUDED
#define BLEND2D_CORE_FONT_P_H_INCLUDED

#include <blend2d/axl/scopedbuffer_p.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/coverage_set_p.h>
#include <blend2d/core/font.h>
#include <blend2d/core/matrix_p.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

struct FontPrivateImpl : public BLFontImpl {};

} // {bl}

namespace bl::FontDetail {

static constexpr uint32_t font_get_glyph_outline_buffer_size = 2048;

BLResult font_free_impl(BLFontCore* self, FontPrivateImpl* impl) noexcept;

static BL_INLINE void font_impl_ctor(FontPrivateImpl* impl) noexcept {
  impl->face._d = bl_object_defaults[BL_OBJECT_TYPE_FONT_FACE]._d;
  bl_call_ctor(impl->feature_settings.dcast());
  bl_call_ctor(impl->variation_settings.dcast());
}


static BL_INLINE void font_matrix_multiply(BLMatrix2D* dst, const BLFontMatrix* a, const BLMatrix2D* b) noexcept {
  dst->reset(a->m00 * b->m00 + a->m01 * b->m10,
             a->m00 * b->m01 + a->m01 * b->m11,
             a->m10 * b->m00 + a->m11 * b->m10,
             a->m10 * b->m01 + a->m11 * b->m11,
             b->m20,
             b->m21);
}

static BL_INLINE void font_matrix_multiply(BLMatrix2D* dst, const BLMatrix2D* a, const BLFontMatrix* b) noexcept {
  dst->reset(a->m00 * b->m00 + a->m01 * b->m10,
             a->m00 * b->m01 + a->m01 * b->m11,
             a->m10 * b->m00 + a->m11 * b->m10,
             a->m10 * b->m01 + a->m11 * b->m11,
             a->m20 * b->m00 + a->m21 * b->m10,
             a->m20 * b->m01 + a->m21 * b->m11);
}

} // {bl::FontDetail}

namespace bl {

template<>
struct ObjectDispatchInfo<BLFontCore> {
  using ImplType = FontPrivateImpl;

  static constexpr bool kHasInitSSO = false;

  static BL_INLINE BLResult free_object(BLFontCore* self, ImplType* impl) noexcept {
    return FontDetail::font_free_impl(self, impl);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_FONT_P_H_INCLUDED
