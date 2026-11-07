// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/axl/scopedbuffer_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/coverage_set_p.h>
#include <blend2d/core/glyph_buffer_p.h>
#include <blend2d/core/file_system.h>
#include <blend2d/core/font_face_p.h>
#include <blend2d/core/matrix.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/string_p.h>
#include <blend2d/opentype/ot_core_p.h>
#include <blend2d/opentype/ot_face_p.h>
#include <blend2d/threading/unique_id_generator_p.h>
#include <blend2d/unicode/unicode_p.h>

namespace bl {

// FontFace - Null
// ===============

FontFacePrivateFuncs null_font_face_funcs;

static constinit ObjectLayer::EternalVirtImpl<FontFacePrivateImpl, BLFontFaceVirt> null_font_face{};

static BLResult BL_CDECL null_font_face_destroy(
  [[maybe_unused]] BLObjectCore* self
) noexcept {
  return BL_SUCCESS;
}

static BLResult BL_CDECL null_font_face_map_text_to_glyphs(
  [[maybe_unused]] const BLFontFaceImpl* impl,
  [[maybe_unused]] uint32_t* content,
  [[maybe_unused]] size_t count,
  BLGlyphMappingState* state
) noexcept {
  state->reset();
  return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL null_font_face_get_glyph_bounds(
  [[maybe_unused]] const BLFontFaceImpl* impl,
  [[maybe_unused]] const uint32_t* glyph_data,
  [[maybe_unused]] intptr_t glyph_advance,
  [[maybe_unused]] BLBoxI* boxes,
  [[maybe_unused]] size_t count
) noexcept {
  return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL null_font_face_get_glyph_advances(
  [[maybe_unused]] const BLFontFaceImpl* impl,
  [[maybe_unused]] const uint32_t* glyph_data,
  [[maybe_unused]] intptr_t glyph_advance,
  [[maybe_unused]] BLGlyphPlacement* placement_data,
  [[maybe_unused]] size_t count
) noexcept {
  return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL null_font_face_get_glyph_outlines(
  [[maybe_unused]] const BLFontFaceImpl* impl,
  [[maybe_unused]] BLGlyphId glyph_id,
  [[maybe_unused]] const BLMatrix2D* user_transform,
  [[maybe_unused]] BLPath* out,
  [[maybe_unused]] size_t* contour_count_out,
  [[maybe_unused]] ScopedBuffer* tmp_buffer
) noexcept {
  *contour_count_out = 0;
  return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL null_font_face_apply_kern(
  [[maybe_unused]] const BLFontFaceImpl* face_impl,
  [[maybe_unused]] uint32_t* glyph_data,
  [[maybe_unused]] BLGlyphPlacement* placement_data,
  [[maybe_unused]] size_t count
) noexcept {
  return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL null_font_face_apply_gsub(
  [[maybe_unused]] const BLFontFaceImpl* impl,
  [[maybe_unused]] BLGlyphBuffer* gb,
  [[maybe_unused]] const BLBitWord* bit_words,
  [[maybe_unused]] size_t bit_word_count
) noexcept {
  return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL null_font_face_apply_gpos(
  [[maybe_unused]] const BLFontFaceImpl* impl,
  [[maybe_unused]] BLGlyphBuffer* gb,
  [[maybe_unused]] const BLBitWord* bit_words,
  [[maybe_unused]] size_t bit_word_count
) noexcept {
  return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL null_font_face_position_glyphs(
  [[maybe_unused]] const BLFontFaceImpl* impl,
  [[maybe_unused]] uint32_t* glyph_data,
  [[maybe_unused]] BLGlyphPlacement* placement_data,
  [[maybe_unused]] size_t count
) noexcept {
  return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);
}

} // {bl}

// FontFace - Init & Destroy
// =========================

BLResult bl_font_face_init(BLFontFaceCore* self) noexcept {
  using namespace bl;
  return ObjectLayer::init_object(self);
}

BLResult bl_font_face_init_move(BLFontFaceCore* self, BLFontFaceCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BLResult bl_font_face_init_weak(BLFontFaceCore* self, const BLFontFaceCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BLResult bl_font_face_destroy(BLFontFaceCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// FontFace - Reset
// ================

BLResult bl_font_face_reset(BLFontFaceCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// FontFace - Assign
// =================

BLResult bl_font_face_assign_move(BLFontFaceCore* self, BLFontFaceCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BLResult bl_font_face_assign_weak(BLFontFaceCore* self, const BLFontFaceCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

// FontFace - Equality & Comparison
// ================================

bool bl_font_face_equals(const BLFontFaceCore* a, const BLFontFaceCore* b) noexcept {
  using namespace bl;

  BL_ASSERT(a->_d.is_font_face());
  BL_ASSERT(b->_d.is_font_face());

  return a->_d.dyn.impl.ptr == b->_d.dyn.impl.ptr;
}

// FontFace - Create
// =================

#if !defined(BL_BUILD_NO_FILESYSTEM)
BLResult bl_font_face_create_from_file(BLFontFaceCore* self, const char* file_name, BLFileReadFlags read_flags) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLFontData font_data;
  BL_PROPAGATE(font_data.create_from_file(file_name, read_flags));
  return bl_font_face_create_from_data(self, &font_data, 0);
}
#endif // !BL_BUILD_NO_FILESYSTEM

BLResult bl_font_face_create_from_data(BLFontFaceCore* self, const BLFontDataCore* font_data, uint32_t face_index) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(font_data));

  if (BL_UNLIKELY(!font_data->dcast().is_valid()))
    return bl_make_error(BL_ERROR_NOT_INITIALIZED);

  if (face_index >= font_data->dcast().face_count())
    return bl_make_error(BL_ERROR_INVALID_VALUE);

  BLFontFaceCore new_o;
  BL_PROPAGATE(OpenType::create_open_type_face(&new_o, static_cast<const BLFontData*>(font_data), face_index));

  // TODO: Move to OTFace?
  ObjectLayer::get_impl<OpenType::OTFaceImpl>(&new_o)->unique_id = UniqueIdGenerator::generate_id(UniqueIdGenerator::Domain::kAny);

  return ObjectLayer::replace_object(self, &new_o);
}

// FontFace - Accessors
// ====================

BLResult bl_font_face_get_full_name(const BLFontFaceCore* self, BLStringCore* out) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(out));

  FontFacePrivateImpl* impl = ObjectLayer::get_impl(self);
  return bl_string_assign_weak(out, &impl->full_name);
}

BLResult bl_font_face_get_family_name(const BLFontFaceCore* self, BLStringCore* out) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(out));

  FontFacePrivateImpl* impl = ObjectLayer::get_impl(self);
  return bl_string_assign_weak(out, &impl->family_name);
}

BLResult bl_font_face_get_subfamily_name(const BLFontFaceCore* self, BLStringCore* out) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(out));

  FontFacePrivateImpl* impl = ObjectLayer::get_impl(self);
  return bl_string_assign_weak(out, &impl->subfamily_name);
}

BLResult bl_font_face_get_post_script_name(const BLFontFaceCore* self, BLStringCore* out) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(out));

  FontFacePrivateImpl* impl = ObjectLayer::get_impl(self);
  return bl_string_assign_weak(out, &impl->post_script_name);
}

BLResult bl_font_face_get_face_info(const BLFontFaceCore* self, BLFontFaceInfo* out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  *out = self->dcast().face_info();
  return BL_SUCCESS;
}

BLResult bl_font_face_get_design_metrics(const BLFontFaceCore* self, BLFontDesignMetrics* out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  *out = self->dcast().design_metrics();
  return BL_SUCCESS;
}

BLResult bl_font_face_get_coverage_info(const BLFontFaceCore* self, BLFontCoverageInfo* out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  *out = self->dcast().coverage_info();
  return BL_SUCCESS;
}

BLResult bl_font_face_get_panose_info(const BLFontFaceCore* self, BLFontPanoseInfo* out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  *out = self->dcast().panose_info();
  return BL_SUCCESS;
}

BLResult bl_font_face_get_cmap_coverage(const BLFontFaceCore* self, BLCoverageSetCore* out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontFacePrivateImpl* impl = ObjectLayer::get_impl(self);
  BLCoverageSetCore tmp;

  BLResult result = ObjectLayer::atomic_object_get_or_populate(impl->cmap_coverage, tmp, [&](BLCoverageSetCore& out) -> BLResult {
    ObjectLayer::init_object(&out);
    BLResult result = OpenType::CMapImpl::populate_character_coverage(static_cast<OpenType::OTFaceImpl*>(impl), &tmp.dcast());
    tmp.dcast().shrink();
    return result;
  });

  if (result == BL_SUCCESS) {
    ObjectLayer::move_object(out, &tmp);
  }

  return result;
}

bool bl_font_face_has_script_tag(const BLFontFaceCore* self, BLTag script_tag) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const FontFacePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return self_impl->script_tag_set.has_tag(script_tag);
}

bool bl_font_face_has_feature_tag(const BLFontFaceCore* self, BLTag feature_tag) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const FontFacePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return self_impl->feature_tag_set.has_tag(feature_tag);
}

bool bl_font_face_has_variation_tag(const BLFontFaceCore* self, BLTag variation_tag) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const FontFacePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return self_impl->variation_tag_set.has_tag(variation_tag);
}

BLResult bl_font_face_get_script_tags(const BLFontFaceCore* self, BLArrayCore* out) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(out->_d.is_array());

  const FontFacePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return self_impl->script_tag_set.flatten_to(out->dcast<BLArray<BLTag>>());
}

BLResult bl_font_face_get_feature_tags(const BLFontFaceCore* self, BLArrayCore* out) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(out->_d.is_array());

  const FontFacePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return self_impl->feature_tag_set.flatten_to(out->dcast<BLArray<BLTag>>());
}

BLResult bl_font_face_get_variation_tags(const BLFontFaceCore* self, BLArrayCore* out) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(out->_d.is_array());

  const FontFacePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return self_impl->variation_tag_set.flatten_to(out->dcast<BLArray<BLTag>>());
}

// FontFace - Runtime Registration
// ===============================

void bl_font_face_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  using namespace bl;
  static constexpr BLObjectType object_type = BLFontFace::Info::kType;

  auto& null_funcs = null_font_face_funcs;
  null_funcs.map_text_to_glyphs = null_font_face_map_text_to_glyphs;
  null_funcs.get_glyph_bounds = null_font_face_get_glyph_bounds;
  null_funcs.get_glyph_advances = null_font_face_get_glyph_advances;
  null_funcs.get_glyph_outlines = null_font_face_get_glyph_outlines;
  null_funcs.apply_kern = null_font_face_apply_kern;
  null_funcs.apply_gsub = null_font_face_apply_gsub;
  null_funcs.apply_gpos = null_font_face_apply_gpos;
  null_funcs.position_glyphs = null_font_face_position_glyphs;

  auto& null_face = null_font_face;
  null_face.virt.base.destroy = null_font_face_destroy;
  null_face.virt.base.get_property = bl_object_impl_get_property;
  null_face.virt.base.set_property = bl_object_impl_set_property;
  bl_font_face_impl_ctor(&null_face.impl, null_funcs);

  ObjectLayer::init_virt(&bl_object_defaults[object_type], object_type, 0u, &null_face.impl, &null_face.virt);
}
