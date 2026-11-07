// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/axl/scopedbuffer_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/bit_array_p.h>
#include <blend2d/core/glyph_buffer_p.h>
#include <blend2d/core/font_p.h>
#include <blend2d/core/font_face_p.h>
#include <blend2d/core/font_feature_settings_p.h>
#include <blend2d/core/matrix.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/path.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/opentype/ot_face_p.h>
#include <blend2d/opentype/ot_layout_p.h>

namespace bl::FontDetail {

// Font - Globals
// ==============

static constinit ObjectLayer::EternalImpl<FontPrivateImpl> null_font = {};

// Font - Internal Utilities
// =========================

static void bl_font_calc_properties(FontPrivateImpl* font_impl, const FontFacePrivateImpl* face_impl, float size) noexcept {
  const BLFontDesignMetrics& dm = face_impl->design_metrics;

  double y_scale = dm.units_per_em ? double(size) / double(dm.units_per_em) : 0.0;
  double x_scale = y_scale;

  font_impl->metrics.size                    = size;
  font_impl->metrics.ascent                  = float(double(dm.ascent                 ) * y_scale);
  font_impl->metrics.descent                 = float(double(dm.descent                ) * y_scale);
  font_impl->metrics.line_gap                = float(double(dm.line_gap               ) * y_scale);
  font_impl->metrics.x_height                = float(double(dm.x_height               ) * y_scale);
  font_impl->metrics.cap_height              = float(double(dm.cap_height             ) * y_scale);
  font_impl->metrics.v_ascent                = float(double(dm.v_ascent               ) * y_scale);
  font_impl->metrics.v_descent               = float(double(dm.v_descent              ) * y_scale);
  font_impl->metrics.x_min                   = float(double(dm.glyph_bounding_box.x0  ) * x_scale);
  font_impl->metrics.y_min                   = float(double(dm.glyph_bounding_box.y0  ) * y_scale);
  font_impl->metrics.x_max                   = float(double(dm.glyph_bounding_box.x1  ) * x_scale);
  font_impl->metrics.y_max                   = float(double(dm.glyph_bounding_box.y1  ) * y_scale);
  font_impl->metrics.underline_position      = float(double(dm.underline_position     ) * y_scale);
  font_impl->metrics.underline_thickness     = float(double(dm.underline_thickness    ) * y_scale);
  font_impl->metrics.strikethrough_position  = float(double(dm.strikethrough_position ) * y_scale);
  font_impl->metrics.strikethrough_thickness = float(double(dm.strikethrough_thickness) * y_scale);
  font_impl->matrix.reset(x_scale, 0.0, 0.0, -y_scale);
}

// Font - Internals - Alloc & Free Impl
// ====================================

static BL_INLINE BLResult font_alloc_impl(BLFontCore* self, const BLFontFaceCore* face, float size) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_impl_t<FontPrivateImpl>(self, BL_OBJECT_TYPE_FONT));

  FontPrivateImpl* impl = ObjectLayer::get_impl(self);
  bl_call_ctor(impl->face.dcast(), face->dcast());
  bl_call_ctor(impl->feature_settings.dcast());
  bl_call_ctor(impl->variation_settings.dcast());
  impl->weight = 0;
  impl->stretch = 0;
  impl->style = 0;
  bl_font_calc_properties(impl, ObjectLayer::get_impl(face), size);
  return BL_SUCCESS;
}

BLResult font_free_impl(BLFontCore* self, FontPrivateImpl* impl, ObjectLayer::MetaByte meta_byte) noexcept {
  axl::maybe_unused(self);

  bl_call_dtor(impl->variation_settings.dcast());
  bl_call_dtor(impl->feature_settings.dcast());
  bl_call_dtor(impl->face.dcast());

  return ObjectLayer::dealloc_impl(impl, meta_byte);
}

// Font - Internals - Make Mutable
// ===============================

static BL_NOINLINE BLResult make_mutable_internal(BLFontCore* self) noexcept {
  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);

  BLFontCore new_o;
  BL_PROPAGATE(ObjectLayer::alloc_impl_t<FontPrivateImpl>(&new_o, BL_OBJECT_TYPE_FONT));

  FontPrivateImpl* new_impl = ObjectLayer::get_impl(&new_o);
  bl_call_ctor(new_impl->face.dcast(), self_impl->face.dcast());
  new_impl->weight = self_impl->weight;
  new_impl->stretch = self_impl->stretch;
  new_impl->style = self_impl->style;
  new_impl->reserved = 0;
  new_impl->metrics = self_impl->metrics;
  new_impl->matrix = self_impl->matrix;
  bl_call_ctor(new_impl->feature_settings.dcast(), self_impl->feature_settings.dcast());
  bl_call_ctor(new_impl->variation_settings.dcast(), self_impl->variation_settings.dcast());

  return ObjectLayer::replace_object(self, &new_o);
}

static BL_INLINE BLResult font_make_mutable(BLFontCore* self) noexcept {
  if (ObjectLayer::is_object_mutable(self)) {
    return BL_SUCCESS;
  }
  return make_mutable_internal(self);
}

} // {bl::FontDetail}

// Font - Init & Destroy
// =====================

BL_API_IMPL BLResult bl_font_init(BLFontCore* self) noexcept {
  self->_d = bl_object_defaults[BL_OBJECT_TYPE_FONT]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_font_init_move(BLFontCore* self, BLFontCore* other) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_font_init_weak(BLFontCore* self, const BLFontCore* other) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object<ObjectLayer::ObjectMode::kDyn>(self, other);
}

BL_API_IMPL BLResult bl_font_destroy(BLFontCore* self) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object<ObjectLayer::ObjectMode::kDyn>(self);
}

// Font - Reset
// ============

BL_API_IMPL BLResult bl_font_reset(BLFontCore* self) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object<ObjectLayer::ObjectMode::kDyn>(self);
}

// Font - Assign
// =============

BL_API_IMPL BLResult bl_font_assign_move(BLFontCore* self, BLFontCore* other) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_font_assign_weak(BLFontCore* self, const BLFontCore* other) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

// Font - Equality & Comparison
// ============================

BL_API_IMPL bool bl_font_equals(const BLFontCore* a, const BLFontCore* b) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  return a->_d.dyn.impl.ptr == b->_d.dyn.impl.ptr;
}

// Font - Create
// =============

BL_API_IMPL BLResult bl_font_create_from_face(BLFontCore* self, const BLFontFaceCore* face, float size) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(face->_d.is_font_face());

  if (!face->dcast().is_valid()) {
    return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
  }

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  if (ObjectLayer::is_impl_mutable(self_impl)) {
    FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(face);

    self_impl->feature_settings.dcast().clear();
    self_impl->variation_settings.dcast().clear();
    self_impl->weight = 0;
    self_impl->stretch = 0;
    self_impl->style = 0;
    bl_font_calc_properties(self_impl, face_impl, size);

    return ObjectLayer::assign_object_weak(&self_impl->face, face);
  }
  else {
    BLFontCore new_o;
    BL_PROPAGATE(font_alloc_impl(&new_o, face, size));
    return ObjectLayer::replace_object(self, &new_o);
  }
}

BL_API_IMPL BLResult bl_font_create_from_face_with_settings(BLFontCore* self, const BLFontFaceCore* face, float size, const BLFontFeatureSettingsCore* feature_settings, const BLFontVariationSettingsCore* variation_settings) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(face->_d.is_font_face());

  if (feature_settings == nullptr) {
    feature_settings = static_cast<BLFontFeatureSettings*>(&bl_object_defaults[BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS]);
  }

  if (variation_settings == nullptr) {
    variation_settings = static_cast<BLFontVariationSettings*>(&bl_object_defaults[BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS]);
  }

  BL_ASSERT(feature_settings->_d.is_font_feature_settings());
  BL_ASSERT(variation_settings->_d.is_font_variation_settings());

  if (!face->dcast().is_valid()) {
    return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
  }

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  if (ObjectLayer::is_impl_mutable(self_impl)) {
    FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(face);

    self_impl->feature_settings.dcast().assign(feature_settings->dcast());
    self_impl->variation_settings.dcast().assign(variation_settings->dcast());
    self_impl->weight = 0;
    self_impl->stretch = 0;
    self_impl->style = 0;
    bl_font_calc_properties(self_impl, face_impl, size);

    return ObjectLayer::assign_object_weak(&self_impl->face, face);
  }
  else {
    BLFontCore new_o;
    BL_PROPAGATE(font_alloc_impl(&new_o, face, size));

    FontPrivateImpl* new_impl = ObjectLayer::get_impl(&new_o);
    new_impl->feature_settings.dcast().assign(feature_settings->dcast());
    new_impl->variation_settings.dcast().assign(variation_settings->dcast());
    return ObjectLayer::replace_object(self, &new_o);
  }
}

// Font - Accessors
// ================

BL_API_IMPL BLResult bl_font_get_face(const BLFontCore* self, BLFontFaceCore* out) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(out->_d.is_font_face());

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return bl_font_face_assign_weak(out, &self_impl->face);
}

BL_API_IMPL float bl_font_get_size(const BLFontCore* self) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return self_impl->metrics.size;
}

BL_API_IMPL BLResult bl_font_set_size(BLFontCore* self, float size) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (ObjectLayer::get_impl(self)->face.dcast().is_empty()) {
    return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
  }

  BL_PROPAGATE(font_make_mutable(self));
  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);

  bl_font_calc_properties(self_impl, ObjectLayer::get_impl(&self_impl->face), size);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_font_get_metrics(const BLFontCore* self, BLFontMetrics* out) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  *out = self_impl->metrics;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_font_get_matrix(const BLFontCore* self, BLFontMatrix* out) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  *out = self_impl->matrix;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_font_get_design_metrics(const BLFontCore* self, BLFontDesignMetrics* out) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(&self_impl->face);

  *out = face_impl->design_metrics;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_font_get_feature_settings(const BLFontCore* self, BLFontFeatureSettingsCore* out) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(out->_d.is_font_feature_settings());

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return bl_font_feature_settings_assign_weak(out, &self_impl->feature_settings);
}

BL_API_IMPL BLResult bl_font_set_feature_settings(BLFontCore* self, const BLFontFeatureSettingsCore* feature_settings) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(feature_settings->_d.is_font_feature_settings());

  if (ObjectLayer::get_impl(self)->face.dcast().is_empty()) {
    return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
  }

  BL_PROPAGATE(font_make_mutable(self));
  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return bl_font_feature_settings_assign_weak(&self_impl->feature_settings, feature_settings);
}

BL_API_IMPL BLResult bl_font_reset_feature_settings(BLFontCore* self) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // Don't make the font mutable if there are no feature settings set.
  if (ObjectLayer::get_impl(self)->feature_settings.dcast().is_empty()) {
    return BL_SUCCESS;
  }

  BL_PROPAGATE(font_make_mutable(self));
  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return bl_font_feature_settings_reset(&self_impl->feature_settings);
}

BL_API_IMPL BLResult bl_font_get_variation_settings(const BLFontCore* self, BLFontVariationSettingsCore* out) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(out->_d.is_font_variation_settings());

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return bl_font_variation_settings_assign_weak(out, &self_impl->variation_settings);
}

BL_API_IMPL BLResult bl_font_set_variation_settings(BLFontCore* self, const BLFontVariationSettingsCore* variation_settings) noexcept {
  BL_USING_DETAIL(bl::FontDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(variation_settings->_d.is_font_variation_settings());

  if (ObjectLayer::get_impl(self)->face.dcast().is_empty()) {
    return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
  }

  BL_PROPAGATE(font_make_mutable(self));
  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return bl_font_variation_settings_assign_weak(&self_impl->variation_settings, variation_settings);
}

BL_API_IMPL BLResult bl_font_reset_variation_settings(BLFontCore* self) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // Don't make the font mutable if there are no variation settings set.
  if (ObjectLayer::get_impl(self)->variation_settings.dcast().is_empty()) {
    return BL_SUCCESS;
  }

  BL_PROPAGATE(font_make_mutable(self));
  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return bl_font_variation_settings_reset(&self_impl->variation_settings);
}

// Font - Shaping
// ==============

BL_API_IMPL BLResult bl_font_shape(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BL_PROPAGATE(bl_font_map_text_to_glyphs(self, gb, nullptr));

  OpenType::OTFaceImpl* ot_face_impl = ObjectLayer::get_impl<OpenType::OTFaceImpl>(&self->dcast().face());
  if (ot_face_impl->layout.gsub().lookup_count) {
    BLBitArray plan;
    BL_PROPAGATE(OpenType::LayoutImpl::calculate_gsub_plan(ot_face_impl, self->dcast().feature_settings(), &plan));
    BL_PROPAGATE(bl_font_apply_gsub(self, gb, &plan));
  }

  return bl_font_position_glyphs(self, gb);
}

BL_API_IMPL BLResult bl_font_map_text_to_glyphs(const BLFontCore* self, BLGlyphBufferCore* gb, BLGlyphMappingState* state_out) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(&self_impl->face);
  GlyphBufferPrivateImpl* gb_impl = bl_glyph_buffer_get_impl(gb);

  if (!gb_impl->size) {
    return BL_SUCCESS;
  }

  if (BL_UNLIKELY(!(gb_impl->flags & BL_GLYPH_RUN_FLAG_UCS4_CONTENT))) {
    return bl_make_error(BL_ERROR_INVALID_STATE);
  }

  BLGlyphMappingState state;
  if (!state_out) {
    state_out = &state;
  }

  BL_PROPAGATE(face_impl->funcs.map_text_to_glyphs(face_impl, gb_impl->content, gb_impl->size, state_out));

  gb_impl->flags = gb_impl->flags & ~BL_GLYPH_RUN_FLAG_UCS4_CONTENT;
  if (state_out->undefined_count > 0) {
    gb_impl->flags |= BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS;
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_font_position_glyphs(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(&self_impl->face);
  GlyphBufferPrivateImpl* gb_impl = bl_glyph_buffer_get_impl(gb);

  if (!gb_impl->size) {
    return BL_SUCCESS;
  }

  if (BL_UNLIKELY(gb_impl->flags & BL_GLYPH_RUN_FLAG_UCS4_CONTENT)) {
    return bl_make_error(BL_ERROR_INVALID_STATE);
  }

  if (!(gb_impl->flags & BL_GLYPH_BUFFER_GLYPH_ADVANCES)) {
    BL_PROPAGATE(gb_impl->ensure_placement());
    face_impl->funcs.get_glyph_advances(face_impl, gb_impl->content, sizeof(uint32_t), gb_impl->placement_data, gb_impl->size);
    gb_impl->glyph_run.placement_type = uint8_t(BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET);
    gb_impl->flags |= BL_GLYPH_BUFFER_GLYPH_ADVANCES;
  }

  using OTFaceImpl = OpenType::OTFaceImpl;
  using OTFaceFlags = OpenType::OTFaceFlags;

  OTFaceImpl* ot_face_i = ObjectLayer::get_impl<OTFaceImpl>(&self->dcast().face());

  if (bl_test_flag(ot_face_i->ot_flags, OTFaceFlags::kGPosLookupList)) {
    BLBitArray plan;
    BL_PROPAGATE(OpenType::LayoutImpl::calculate_gpos_plan(ot_face_i, self->dcast().feature_settings(), &plan));
    BL_PROPAGATE(bl_font_apply_gpos(self, gb, &plan));
  }

  constexpr OTFaceFlags kKernFlags = OTFaceFlags::kGPosKernAvailable | OTFaceFlags::kLegacyKernAvailable;
  if ((ot_face_i->ot_flags & kKernFlags) == OTFaceFlags::kLegacyKernAvailable) {
    if (self_impl->feature_settings.dcast().get_value(BL_MAKE_TAG('k', 'e', 'r', 'n')) != 0u) {
      BL_PROPAGATE(face_impl->funcs.apply_kern(face_impl, gb_impl->content, gb_impl->placement_data, gb_impl->size));
    }
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_font_apply_kerning(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(&self_impl->face);
  GlyphBufferPrivateImpl* gb_impl = bl_glyph_buffer_get_impl(gb);

  if (!gb_impl->size) {
    return BL_SUCCESS;
  }

  if (BL_UNLIKELY(!(gb_impl->placement_data))) {
    return bl_make_error(BL_ERROR_INVALID_STATE);
  }

  return face_impl->funcs.apply_kern(face_impl, gb_impl->content, gb_impl->placement_data, gb_impl->size);
}

BL_API_IMPL BLResult bl_font_apply_gsub(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitArrayCore* lookups) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(&self_impl->face);

  return face_impl->funcs.apply_gsub(face_impl, static_cast<BLGlyphBuffer*>(gb), lookups->dcast().data(), lookups->dcast().size_in_words());
}

BL_API_IMPL BLResult bl_font_apply_gpos(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitArrayCore* lookups) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(&self_impl->face);
  GlyphBufferPrivateImpl* gb_impl = bl_glyph_buffer_get_impl(gb);

  if (!gb_impl->size) {
    return BL_SUCCESS;
  }

  if (BL_UNLIKELY(!(gb_impl->placement_data))) {
    return bl_make_error(BL_ERROR_INVALID_STATE);
  }

  return face_impl->funcs.apply_gpos(face_impl, static_cast<BLGlyphBuffer*>(gb), lookups->dcast().data(), lookups->dcast().size_in_words());
}

BL_API_IMPL BLResult bl_font_get_text_metrics(const BLFontCore* self, BLGlyphBufferCore* gb, BLTextMetrics* out) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  GlyphBufferPrivateImpl* gb_impl = bl_glyph_buffer_get_impl(gb);

  out->reset();
  if (!(gb_impl->flags & BL_GLYPH_BUFFER_GLYPH_ADVANCES)) {
    BL_PROPAGATE(bl_font_shape(self, gb));
    gb_impl = bl_glyph_buffer_get_impl(gb);
  }

  size_t size = gb_impl->size;
  if (!size) {
    return BL_SUCCESS;
  }

  BLPoint advance {};

  const uint32_t* glyph_data = gb_impl->content;
  const BLGlyphPlacement* placement_data = gb_impl->placement_data;

  for (size_t i = 0; i < size; i++) {
    advance += BLPoint(placement_data[i].advance);
  }

  BLBoxI glyph_bounds[2];
  uint32_t border_glyphs[2] = { glyph_data[0], glyph_data[size - 1] };

  BL_PROPAGATE(bl_font_get_glyph_bounds(self, border_glyphs, intptr_t(sizeof(uint32_t)), glyph_bounds, 2));
  out->advance = advance;

  double lsb = glyph_bounds[0].x0;
  double rsb = placement_data[size - 1].advance.x - glyph_bounds[1].x1;

  out->leading_bearing.reset(lsb, 0);
  out->trailing_bearing.reset(rsb, 0);
  out->bounding_box.reset(glyph_bounds[0].x0, 0.0, advance.x - rsb, 0.0);

  const BLFontMatrix& m = self_impl->matrix;
  BLPoint scale_pt = BLPoint(m.m00, m.m11);

  out->advance *= scale_pt;
  out->leading_bearing *= scale_pt;
  out->trailing_bearing *= scale_pt;
  out->bounding_box *= scale_pt;

  return BL_SUCCESS;
}

// Font - Low-Level API
// ====================

BL_API_IMPL BLResult bl_font_get_glyph_bounds(const BLFontCore* self, const uint32_t* glyph_data, intptr_t glyph_advance, BLBoxI* out, size_t count) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(&self_impl->face);

  return face_impl->funcs.get_glyph_bounds(face_impl, glyph_data, glyph_advance, out, count);
}

BL_API_IMPL BLResult bl_font_get_glyph_advances(const BLFontCore* self, const uint32_t* glyph_data, intptr_t glyph_advance, BLGlyphPlacement* out, size_t count) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(&self_impl->face);

  return face_impl->funcs.get_glyph_advances(face_impl, glyph_data, glyph_advance, out, count);
}

// Font - Glyph Outlines
// =====================

static BLResult BL_CDECL bl_font_dummy_path_sink(BLPathCore* path, const void* info, void* user_data) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  axl::maybe_unused(path, info, user_data);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_font_get_glyph_outlines(const BLFontCore* self, BLGlyphId glyph_id, const BLMatrix2D* user_transform, BLPathCore* out, BLPathSinkFunc sink, void* user_data) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(&self_impl->face);

  BLMatrix2D final_transform;
  const BLFontMatrix& font_matrix = self_impl->matrix;

  if (user_transform) {
    font_matrix_multiply(&final_transform, &font_matrix, user_transform);
  }
  else {
    final_transform.reset(font_matrix.m00, font_matrix.m01, font_matrix.m10, font_matrix.m11, 0.0, 0.0);
  }

  ScopedBufferTmp<font_get_glyph_outline_buffer_size> tmp_buffer;
  BLGlyphOutlineSinkInfo sink_info;
  BL_PROPAGATE(face_impl->funcs.get_glyph_outlines(face_impl, glyph_id, &final_transform, static_cast<BLPath*>(out), &sink_info.contour_count, &tmp_buffer));

  if (!sink) {
    return BL_SUCCESS;
  }

  sink_info.glyph_index = 0;
  return sink(out, &sink_info, user_data);
}

BL_API_IMPL BLResult bl_font_get_glyph_run_outlines(const BLFontCore* self, const BLGlyphRun* glyph_run, const BLMatrix2D* user_transform, BLPathCore* out, BLPathSinkFunc sink, void* user_data) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(&self_impl->face);

  if (!glyph_run->size) {
    return BL_SUCCESS;
  }

  BLMatrix2D final_transform;
  const BLFontMatrix& fMat = self_impl->matrix;

  if (user_transform) {
    font_matrix_multiply(&final_transform, &fMat, user_transform);
  }
  else {
    user_transform = &TransformDetail::identity_transform;
    final_transform.reset(fMat.m00, fMat.m01, fMat.m10, fMat.m11, 0.0, 0.0);
  }

  if (!sink) {
    sink = bl_font_dummy_path_sink;
  }

  ScopedBufferTmp<font_get_glyph_outline_buffer_size> tmp_buffer;
  BLGlyphOutlineSinkInfo sink_info;

  uint32_t placement_type = glyph_run->placement_type;
  BLGlyphRunIterator it(*glyph_run);
  auto get_glyph_outlines_func = face_impl->funcs.get_glyph_outlines;

  if (it.has_placement() && placement_type != BL_GLYPH_PLACEMENT_TYPE_NONE) {
    BLMatrix2D offset_transform(1.0, 0.0, 0.0, 1.0, final_transform.m20, final_transform.m21);

    switch (placement_type) {
      case BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET:
      case BL_GLYPH_PLACEMENT_TYPE_DESIGN_UNITS:
        offset_transform.m00 = final_transform.m00;
        offset_transform.m01 = final_transform.m01;
        offset_transform.m10 = final_transform.m10;
        offset_transform.m11 = final_transform.m11;
        break;

      case BL_GLYPH_PLACEMENT_TYPE_USER_UNITS:
        offset_transform.m00 = user_transform->m00;
        offset_transform.m01 = user_transform->m01;
        offset_transform.m10 = user_transform->m10;
        offset_transform.m11 = user_transform->m11;
        break;
    }

    if (placement_type == BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET) {
      double ox = final_transform.m20;
      double oy = final_transform.m21;

      while (!it.at_end()) {
        const BLGlyphPlacement& pos = it.placement<BLGlyphPlacement>();

        double px = double(pos.placement.x);
        double py = double(pos.placement.y);
        final_transform.m20 = Math::madd(px, offset_transform.m00, Math::madd(py, offset_transform.m10, ox));
        final_transform.m21 = Math::madd(px, offset_transform.m01, Math::madd(py, offset_transform.m11, oy));

        sink_info.glyph_index = it.index;
        BL_PROPAGATE(get_glyph_outlines_func(face_impl, it.glyph_id(), &final_transform, static_cast<BLPath*>(out), &sink_info.contour_count, &tmp_buffer));
        BL_PROPAGATE(sink(out, &sink_info, user_data));
        it.advance();

        double ax = double(pos.advance.x);
        double ay = double(pos.advance.y);
        ox = Math::madd(ax, offset_transform.m00, Math::madd(ay, offset_transform.m10, ox));
        oy = Math::madd(ax, offset_transform.m01, Math::madd(ay, offset_transform.m11, oy));
      }
    }
    else {
      while (!it.at_end()) {
        const BLPoint& placement = it.placement<BLPoint>();
        final_transform.m20 = Math::madd(placement.x, offset_transform.m00, Math::madd(placement.y, offset_transform.m10, offset_transform.m20));
        final_transform.m21 = Math::madd(placement.x, offset_transform.m01, Math::madd(placement.y, offset_transform.m11, offset_transform.m21));

        sink_info.glyph_index = it.index;
        BL_PROPAGATE(get_glyph_outlines_func(face_impl, it.glyph_id(), &final_transform, static_cast<BLPath*>(out), &sink_info.contour_count, &tmp_buffer));
        BL_PROPAGATE(sink(out, &sink_info, user_data));
        it.advance();
      }
    }
  }
  else {
    while (!it.at_end()) {
      sink_info.glyph_index = it.index;
      BL_PROPAGATE(get_glyph_outlines_func(face_impl, it.glyph_id(), &final_transform, static_cast<BLPath*>(out), &sink_info.contour_count, &tmp_buffer));
      BL_PROPAGATE(sink(out, &sink_info, user_data));
      it.advance();
    }
  }

  return BL_SUCCESS;
}

// Font - Runtime Registration
// ===========================

void bl_font_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::FontDetail);
  static constexpr BLObjectType object_type = BLFont::Info::kType;

  font_impl_ctor(&null_font.impl);
  ObjectLayer::init_dyn(&bl_object_defaults[object_type], object_type, 0u, &null_font.impl, nullptr);
}
