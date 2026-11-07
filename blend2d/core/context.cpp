// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#include <blend2d/core/context_p.h>
#include <blend2d/core/gradient_p.h>
#include <blend2d/core/image_p.h>
#include <blend2d/core/pattern_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/raster/raster_context_p.h>

namespace bl {

// Context - Null
// ==============

// TODO: This should be constexpr - no need for runtime initialization if we do some tweaks here and there
//       as StrokeOptions is SSO now, thus this whole struct can be read-only and initialized at compile-time.
static axl::Wrap<BLContextState> null_state;

static constinit ObjectLayer::EternalVirtImpl<BLContextImpl, BLContextVirt> null_context{};

static constexpr BLContextCreateInfo context_null_create_info {};

// NullContext implementation does nothing. These functions consistently return `BL_ERROR_INVALID_STATE` to inform the
// caller that the context is not usable. We don't want to mark every unused parameter by `axl::maybe_unused()` in this
// case so the warning is temporarily turned off by BL_DIAGNOSTIC_PUSH/POP.
BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL null_context_destroy(BLObjectCore* self) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_flush(BLContextCore* self, BLContextFlushFlags flags) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_no_args(BLContextCore* self) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_float(BLContextCore* self, float) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_double(BLContextCore* self, double) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_comp_op(BLContextCore* self, BLCompOp) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_fill_rule(BLContextCore* self, BLFillRule) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_save(BLContextCore* self, BLContextCookie*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_restore(BLContextCore* self, const BLContextCookie*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_get_style(const BLContextCore* self, BLContextStyleSlot, bool, BLVarCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_style(BLContextCore* self, BLContextStyleSlot, const BLObjectCore*, BLContextStyleTransformMode) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_disable_style(BLContextCore* self, BLContextStyleSlot) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_style_rgba(BLContextCore* self, BLContextStyleSlot, const BLRgba*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_style_rgba32(BLContextCore* self, BLContextStyleSlot, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_style_rgba64(BLContextCore* self, BLContextStyleSlot, uint64_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_style_alpha(BLContextCore* self, BLContextStyleSlot, double) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_swap_styles(BLContextCore* self, BLContextStyleSwapMode mode) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_set_hint(BLContextCore* self, BLContextHint, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_hints(BLContextCore* self, const BLContextHints*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_flatten_mode(BLContextCore* self, BLFlattenMode) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_approximation_options(BLContextCore* self, const BLApproximationOptions*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_stroke_transform_order(BLContextCore* self, BLStrokeTransformOrder) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_stroke_dash_array_object(BLContextCore* self, const BLArrayCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_stroke_dash_array_data(BLContextCore* self, const float*, size_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_stroke_cap(BLContextCore* self, BLStrokeCapPosition, BLStrokeCap) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_stroke_caps(BLContextCore* self, BLStrokeCap) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_stroke_join(BLContextCore* self, BLStrokeJoin) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_set_stroke_options(BLContextCore* self, const BLStrokeOptionsCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_apply_transform_op(BLContextCore* self, BLTransformOp, const void*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_fill_all(BLContextCore* self) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_fill_all_rgba32(BLContextCore* self, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_fill_all_ext(BLContextCore* self, const BLObjectCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_do_rect_i(BLContextCore* self, const BLRectI*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_rect_i_rgba32(BLContextCore* self, const BLRectI*, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_rect_i_ext(BLContextCore* self, const BLRectI*, const BLObjectCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_do_rect_d(BLContextCore* self, const BLRect*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_rect_d_rgba32(BLContextCore* self, const BLRect*, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_rect_d_ext(BLContextCore* self, const BLRect*, const BLObjectCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_do_path_d(BLContextCore* self, const BLPoint*, const BLPathCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_path_d_rgba32(BLContextCore* self, const BLPoint*, const BLPathCore*, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_path_d_ext(BLContextCore* self, const BLPoint*, const BLPathCore*, const BLObjectCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_do_geometry(BLContextCore* self, BLGeometryType, const void*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_geometry_rgba32(BLContextCore* self, BLGeometryType, const void*, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_geometry_ext(BLContextCore* self, BLGeometryType, const void*, const BLObjectCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_do_text_op_i(BLContextCore* self, const BLPointI*, const BLFontCore*, BLContextRenderTextOp, const void*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_text_op_i_rgba32(BLContextCore* self, const BLPointI*, const BLFontCore*, BLContextRenderTextOp, const void*, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_text_op_i_ext(BLContextCore* self, const BLPointI*, const BLFontCore*, BLContextRenderTextOp, const void*, const BLObjectCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_do_text_op_d(BLContextCore* self, const BLPoint*, const BLFontCore*, BLContextRenderTextOp, const void*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_text_op_d_rgba32(BLContextCore* self, const BLPoint*, const BLFontCore*, BLContextRenderTextOp, const void*, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_text_op_d_ext(BLContextCore* self, const BLPoint*, const BLFontCore*, BLContextRenderTextOp, const void*, const BLObjectCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_do_mask_i(BLContextCore* self, const BLPointI*, const BLImageCore*, const BLRectI*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_mask_i_rgba32(BLContextCore* self, const BLPointI*, const BLImageCore*, const BLRectI*, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_mask_i_ext(BLContextCore* self, const BLPointI*, const BLImageCore*, const BLRectI*, const BLObjectCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_do_mask_d(BLContextCore* self, const BLPoint*, const BLImageCore*, const BLRectI*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_mask_d_rgba32(BLContextCore* self, const BLPoint*, const BLImageCore*, const BLRectI*, uint32_t) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_do_mask_d_ext(BLContextCore* self, const BLPoint*, const BLImageCore*, const BLRectI*, const BLObjectCore*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL null_context_blit_image_i(BLContextCore* self, const BLPointI*, const BLImageCore*, const BLRectI*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_blit_image_d(BLContextCore* self, const BLPoint*, const BLImageCore*, const BLRectI*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_blit_scaled_image_i(BLContextCore* self, const BLRectI*, const BLImageCore*, const BLRectI*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL null_context_blit_scaled_image_d(BLContextCore* self, const BLRect*, const BLImageCore*, const BLRectI*) noexcept { return bl_make_error(BL_ERROR_INVALID_STATE); }

BL_DIAGNOSTIC_POP

static void init_null_context_virt(BLContextVirt* virt) noexcept {
  virt->base.destroy                 = null_context_destroy;
  virt->base.get_property            = bl_object_impl_get_property;
  virt->base.set_property            = bl_object_impl_set_property;
  virt->flush                        = null_context_flush;

  virt->save                         = null_context_save;
  virt->restore                      = null_context_restore;

  virt->user_to_meta                 = null_context_no_args;
  virt->apply_transform_op           = null_context_apply_transform_op;

  virt->set_hint                     = null_context_set_hint;
  virt->set_hints                    = null_context_set_hints;

  virt->set_flatten_mode             = null_context_set_flatten_mode;
  virt->set_flatten_tolerance        = null_context_set_float;
  virt->set_approximation_options    = null_context_set_approximation_options;

  virt->get_style                    = null_context_get_style;
  virt->set_style                    = null_context_set_style;
  virt->disable_style                = null_context_disable_style;
  virt->set_style_rgba               = null_context_set_style_rgba;
  virt->set_style_rgba32             = null_context_set_style_rgba32;
  virt->set_style_rgba64             = null_context_set_style_rgba64;
  virt->set_style_alpha              = null_context_set_style_alpha;
  virt->swap_styles                  = null_context_swap_styles;

  virt->set_global_alpha             = null_context_set_double;
  virt->set_comp_op                  = null_context_set_comp_op;

  virt->set_fill_rule                = null_context_set_fill_rule;

  virt->set_stroke_width             = null_context_set_float;
  virt->set_stroke_miter_limit       = null_context_set_float;
  virt->set_stroke_cap               = null_context_set_stroke_cap;
  virt->set_stroke_caps              = null_context_set_stroke_caps;
  virt->set_stroke_join              = null_context_set_stroke_join;
  virt->set_stroke_transform_order   = null_context_set_stroke_transform_order;
  virt->set_stroke_dash_offset       = null_context_set_float;
  virt->set_stroke_dash_array_object = null_context_set_stroke_dash_array_object;
  virt->set_stroke_dash_array_data   = null_context_set_stroke_dash_array_data;
  virt->set_stroke_options           = null_context_set_stroke_options;

  virt->clip_to_rect_i               = null_context_do_rect_i;
  virt->clip_to_rect_d               = null_context_do_rect_d;
  virt->restore_clipping             = null_context_no_args;

  virt->clear_all                    = null_context_no_args;
  virt->clear_rect_i                 = null_context_do_rect_i;
  virt->clear_rect_d                 = null_context_do_rect_d;

  virt->fill_all                     = null_context_fill_all;
  virt->fill_all_rgba32              = null_context_fill_all_rgba32;
  virt->fill_all_ext                 = null_context_fill_all_ext;

  virt->fill_rect_i                  = null_context_do_rect_i;
  virt->fill_rect_i_rgba32           = null_context_do_rect_i_rgba32;
  virt->fill_rect_i_ext              = null_context_do_rect_i_ext;

  virt->fill_rect_d                  = null_context_do_rect_d;
  virt->fill_rect_d_rgba32           = null_context_do_rect_d_rgba32;
  virt->fill_rect_d_ext              = null_context_do_rect_d_ext;

  virt->fill_path_d                  = null_context_do_path_d;
  virt->fill_path_d_rgba32           = null_context_do_path_d_rgba32;
  virt->fill_path_d_ext              = null_context_do_path_d_ext;

  virt->fill_geometry                = null_context_do_geometry;
  virt->fill_geometry_rgba32         = null_context_do_geometry_rgba32;
  virt->fill_geometry_ext            = null_context_do_geometry_ext;

  virt->fill_text_op_i               = null_context_do_text_op_i;
  virt->fill_text_op_i_rgba32        = null_context_do_text_op_i_rgba32;
  virt->fill_text_op_i_ext           = null_context_do_text_op_i_ext;

  virt->fill_text_op_d               = null_context_do_text_op_d;
  virt->fill_text_op_d_rgba32        = null_context_do_text_op_d_rgba32;
  virt->fill_text_op_d_ext           = null_context_do_text_op_d_ext;

  virt->fill_mask_i                  = null_context_do_mask_i;
  virt->fill_mask_i_rgba32           = null_context_do_mask_i_rgba32;
  virt->fill_mask_i_ext              = null_context_do_mask_i_ext;

  virt->fill_mask_d                  = null_context_do_mask_d;
  virt->fill_mask_d_Rgba32           = null_context_do_mask_d_rgba32;
  virt->fill_mask_d_ext              = null_context_do_mask_d_ext;

  virt->stroke_path_d                = null_context_do_path_d;
  virt->stroke_path_d_rgba32         = null_context_do_path_d_rgba32;
  virt->stroke_path_d_ext            = null_context_do_path_d_ext;

  virt->stroke_geometry              = null_context_do_geometry;
  virt->stroke_geometry_rgba32       = null_context_do_geometry_rgba32;
  virt->stroke_geometry_ext          = null_context_do_geometry_ext;

  virt->stroke_text_op_i             = null_context_do_text_op_i;
  virt->stroke_text_op_i_rgba32      = null_context_do_text_op_i_rgba32;
  virt->stroke_text_op_i_ext         = null_context_do_text_op_i_ext;

  virt->stroke_text_op_d             = null_context_do_text_op_d;
  virt->stroke_text_op_d_rgba32      = null_context_do_text_op_d_rgba32;
  virt->stroke_text_op_d_ext         = null_context_do_text_op_d_ext;

  virt->blit_image_i                 = null_context_blit_image_i;
  virt->blit_image_d                 = null_context_blit_image_d;

  virt->blit_scaled_image_i          = null_context_blit_scaled_image_i;
  virt->blit_scaled_image_d          = null_context_blit_scaled_image_d;
}

} // {bl}

// Context - API - Init & Destroy
// ===============================

BL_API_IMPL BLResult bl_context_init(BLContextCore* self) noexcept {
  self->_d = bl_object_defaults[BL_OBJECT_TYPE_CONTEXT]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_context_init_move(BLContextCore* self, BLContextCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_context_init_weak(BLContextCore* self, const BLContextCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_context_init_as(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* cci) noexcept {
  using namespace bl;

  self->_d = bl_object_defaults[BL_OBJECT_TYPE_CONTEXT]._d;
  return bl_context_begin(self, image, cci);
}

BL_API_IMPL BLResult bl_context_destroy(BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// Context - API - Reset
// ======================

BL_API_IMPL BLResult bl_context_reset(BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// Context - API - Assign
// =======================

BL_API_IMPL BLResult bl_context_assign_move(BLContextCore* self, BLContextCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_context_assign_weak(BLContextCore* self, const BLContextCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

// Context - API - Accessors
// ==========================

BL_API_IMPL BLContextType bl_context_get_type(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return BLContextType(self->_impl()->context_type);
}

BL_API_IMPL BLResult bl_context_get_target_size(const BLContextCore* self, BLSize* target_size_out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  *target_size_out = state->target_size;
  return BL_SUCCESS;
}

BL_API_IMPL BLImageCore* bl_context_get_target_image(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return state->target_image;
}

// Context - API - Begin & End
// ============================

BL_API_IMPL BLResult bl_context_begin(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* cci) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // Reject empty images.
  if (image->dcast().is_empty())
    return bl_make_error(BL_ERROR_INVALID_VALUE);

  if (!cci) {
    cci = &context_null_create_info;
  }

  BLContextCore new_o;
  BL_PROPAGATE(bl_raster_context_init_impl(&new_o, image, cci));

  return bl::ObjectLayer::replace_object(self, &new_o);
}

BL_API_IMPL BLResult bl_context_end(BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // Currently mapped to `BLContext::reset()`.
  return bl_context_reset(self);
}

// Context - API - Flush
// ======================

BL_API_IMPL BLResult bl_context_flush(BLContextCore* self, BLContextFlushFlags flags) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->flush(self, flags);
}

// Context - API - Save & Restore
// ===============================

BL_API_IMPL BLResult bl_context_save(BLContextCore* self, BLContextCookie* cookie) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->save(self, cookie);
}

BL_API_IMPL BLResult bl_context_restore(BLContextCore* self, const BLContextCookie* cookie) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->restore(self, cookie);
}

// Context - API - Transformations
// ================================

BL_API_IMPL BLResult bl_context_get_meta_transform(const BLContextCore* self, BLMatrix2D* transform_out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  *transform_out = state->meta_transform;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_context_get_user_transform(const BLContextCore* self, BLMatrix2D* transform_out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  *transform_out = state->user_transform;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_context_get_final_transform(const BLContextCore* self, BLMatrix2D* transform_out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  *transform_out = state->final_transform;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_context_user_to_meta(BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->user_to_meta(self);
}

BL_API_IMPL BLResult bl_context_apply_transform_op(BLContextCore* self, BLTransformOp op_type, const void* op_data) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->apply_transform_op(self, op_type, op_data);
}

// Context - API - Rendering Hints
// ================================

BL_API_IMPL uint32_t bl_context_get_hint(const BLContextCore* self, BLContextHint hint_type) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(uint32_t(hint_type) > uint32_t(BL_CONTEXT_HINT_MAX_VALUE))) {
    return 0;
  }

  const BLContextState* state = context_get_state(self);
  return state->hints.hints[hint_type];
}

BL_API_IMPL BLResult bl_context_set_hint(BLContextCore* self, BLContextHint hint_type, uint32_t value) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_hint(self, hint_type, value);
}

BL_API_IMPL BLResult bl_context_get_hints(const BLContextCore* self, BLContextHints* hints_out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  *hints_out = state->hints;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_context_set_hints(BLContextCore* self, const BLContextHints* hints) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_hints(self, hints);
}

// Context - API - Approximation Options
// ======================================

BL_API_IMPL BLResult bl_context_set_flatten_mode(BLContextCore* self, BLFlattenMode mode) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_flatten_mode(self, mode);
}

BL_API_IMPL BLResult bl_context_set_flatten_tolerance(BLContextCore* self, float tolerance) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_flatten_tolerance(self, tolerance);
}

BL_API_IMPL BLResult bl_context_set_approximation_options(BLContextCore* self, const BLApproximationOptions* options) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_approximation_options(self, options);
}

// Context - API - Fill Style & Alpha
// ===================================

BL_API_IMPL BLResult bl_context_get_fill_style(const BLContextCore* self, BLVarCore* style_out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->get_style(self, BL_CONTEXT_STYLE_SLOT_FILL, false, style_out);
}

BL_API_IMPL BLResult bl_context_get_transformed_fill_style(const BLContextCore* self, BLVarCore* style_out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->get_style(self, BL_CONTEXT_STYLE_SLOT_FILL, true, style_out);
}

BL_API_IMPL BLResult bl_context_set_fill_style(BLContextCore* self, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style(self, BL_CONTEXT_STYLE_SLOT_FILL, static_cast<const BLObjectCore*>(style), BL_CONTEXT_STYLE_TRANSFORM_MODE_USER);
}

BL_API_IMPL BLResult bl_context_set_fill_style_with_mode(BLContextCore* self, const BLUnknown* style, BLContextStyleTransformMode transform_mode) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style(self, BL_CONTEXT_STYLE_SLOT_FILL, static_cast<const BLObjectCore*>(style), transform_mode);
}

BL_API_IMPL BLResult bl_context_set_fill_style_rgba(BLContextCore* self, const BLRgba* rgba) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style_rgba(self, BL_CONTEXT_STYLE_SLOT_FILL, rgba);
}

BL_API_IMPL BLResult bl_context_set_fill_style_rgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style_rgba32(self, BL_CONTEXT_STYLE_SLOT_FILL, rgba32);
}

BL_API_IMPL BLResult bl_context_set_fill_style_rgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style_rgba64(self, BL_CONTEXT_STYLE_SLOT_FILL, rgba64);
}

BL_API_IMPL BLResult bl_context_disable_fill_style(BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->disable_style(self, BL_CONTEXT_STYLE_SLOT_FILL);
}

BL_API_IMPL double bl_context_get_fill_alpha(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return state->style_alpha[BL_CONTEXT_STYLE_SLOT_FILL];
}

BL_API_IMPL BLResult bl_context_set_fill_alpha(BLContextCore* self, double alpha) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style_alpha(self, BL_CONTEXT_STYLE_SLOT_FILL, alpha);
}

// Context - API - Stroke Style & Alpha
// =====================================

BL_API_IMPL BLResult bl_context_get_stroke_style(const BLContextCore* self, BLVarCore* style_out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->get_style(self, BL_CONTEXT_STYLE_SLOT_STROKE, false, style_out);
}

BL_API_IMPL BLResult bl_context_get_transformed_stroke_style(const BLContextCore* self, BLVarCore* style_out) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->get_style(self, BL_CONTEXT_STYLE_SLOT_STROKE, true, style_out);
}

BL_API_IMPL BLResult bl_context_set_stroke_style(BLContextCore* self, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style(self, BL_CONTEXT_STYLE_SLOT_STROKE, static_cast<const BLObjectCore*>(style), BL_CONTEXT_STYLE_TRANSFORM_MODE_USER);
}

BL_API_IMPL BLResult bl_context_set_stroke_style_with_mode(BLContextCore* self, const BLUnknown* style, BLContextStyleTransformMode transform_mode) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style(self, BL_CONTEXT_STYLE_SLOT_STROKE, static_cast<const BLObjectCore*>(style), transform_mode);
}

BL_API_IMPL BLResult bl_context_set_stroke_style_rgba(BLContextCore* self, const BLRgba* rgba) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style_rgba(self, BL_CONTEXT_STYLE_SLOT_STROKE, rgba);
}

BL_API_IMPL BLResult bl_context_set_stroke_style_rgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style_rgba32(self, BL_CONTEXT_STYLE_SLOT_STROKE, rgba32);
}

BL_API_IMPL BLResult bl_context_set_stroke_style_rgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style_rgba64(self, BL_CONTEXT_STYLE_SLOT_STROKE, rgba64);
}

BL_API_IMPL BLResult bl_context_disable_stroke_style(BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->disable_style(self, BL_CONTEXT_STYLE_SLOT_STROKE);
}

BL_API_IMPL double bl_context_get_stroke_alpha(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return state->style_alpha[BL_CONTEXT_STYLE_SLOT_STROKE];
}

BL_API_IMPL BLResult bl_context_set_stroke_alpha(BLContextCore* self, double alpha) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_style_alpha(self, BL_CONTEXT_STYLE_SLOT_STROKE, alpha);
}

BL_API_IMPL BLResult bl_context_swap_styles(BLContextCore* self, BLContextStyleSwapMode mode) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->swap_styles(self, mode);
}

// Context - API - Composition Options
// ====================================

BL_API_IMPL double bl_context_get_global_alpha(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return state->global_alpha;
}

BL_API_IMPL BLResult bl_context_set_global_alpha(BLContextCore* self, double alpha) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_global_alpha(self, alpha);
}

BL_API_IMPL BLCompOp bl_context_get_comp_op(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return (BLCompOp)state->comp_op;
}

BL_API_IMPL BLResult bl_context_set_comp_op(BLContextCore* self, BLCompOp comp_op) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_comp_op(self, comp_op);
}

// Context - API - Fill Options
// =============================

BL_API_IMPL BLFillRule bl_context_get_fill_rule(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return (BLFillRule)state->fill_rule;
}

BL_API_IMPL BLResult bl_context_set_fill_rule(BLContextCore* self, BLFillRule fill_rule) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_fill_rule(self, fill_rule);
}

// Context - API - Stroke Options
// ===============================

BL_API_IMPL float bl_context_get_stroke_width(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return state->stroke_options.dcast().width();
}

BL_API_IMPL BLResult bl_context_set_stroke_width(BLContextCore* self, float width) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  return self->_virt()->set_stroke_width(self, width);
}

BL_API_IMPL float bl_context_get_stroke_miter_limit(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return state->stroke_options.dcast().miter_limit();
}

BL_API_IMPL BLResult bl_context_set_stroke_miter_limit(BLContextCore* self, float miter_limit) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_stroke_miter_limit(self, miter_limit);
}

BL_API_IMPL BLStrokeCap bl_context_get_stroke_cap(const BLContextCore* self, BLStrokeCapPosition position) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(uint32_t(position) > uint32_t(BL_STROKE_CAP_POSITION_MAX_VALUE))) {
    return (BLStrokeCap)0;
  }

  const BLContextState* state = context_get_state(self);
  return (BLStrokeCap)state->stroke_options.dcast().cap_by_position(position);
}

BL_API_IMPL BLResult bl_context_set_stroke_cap(BLContextCore* self, BLStrokeCapPosition position, BLStrokeCap stroke_cap) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_stroke_cap(self, position, stroke_cap);
}

BL_API_IMPL BLResult bl_context_set_stroke_caps(BLContextCore* self, BLStrokeCap stroke_cap) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_stroke_caps(self, stroke_cap);
}

BL_API_IMPL BLStrokeJoin bl_context_get_stroke_join(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return (BLStrokeJoin)state->stroke_options.dcast().join();
}

BL_API_IMPL BLResult bl_context_set_stroke_join(BLContextCore* self, BLStrokeJoin stroke_join) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_stroke_join(self, stroke_join);
}

BL_API_IMPL BLStrokeTransformOrder bl_context_get_stroke_transform_order(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return (BLStrokeTransformOrder)state->stroke_options.dcast().transform_order();
}

BL_API_IMPL BLResult bl_context_set_stroke_transform_order(BLContextCore* self, BLStrokeTransformOrder transform_order) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_stroke_transform_order(self, transform_order);
}

BL_API_IMPL float bl_context_get_stroke_dash_offset(const BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return state->stroke_options.dcast().dash_offset();
}

BL_API_IMPL BLResult bl_context_set_stroke_dash_offset(BLContextCore* self, float dash_offset) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_stroke_dash_offset(self, dash_offset);
}

BL_API_IMPL BLResult bl_context_get_stroke_dash_array(const BLContextCore* self, BLArrayCore* dash_array_out) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(dash_array_out->_d.is_array_f32());

  const BLStrokeOptionsCore& stroke_options = context_get_state(self)->stroke_options;
  if (stroke_options._d.is_ref_counted()) {
    // TRANSMUTE: BLStrokeOptions data (dash-array) into BLArray's data - Impls are the same.
    BLArrayCore tmp;
    ObjectLayer::init_dyn(&tmp, BL_OBJECT_TYPE_ARRAY_FLOAT32, BLObjectDetail::kMetaFlagDR, stroke_options._d.dyn.impl.ptr, static_cast<void*>(stroke_options._d.dyn.impl.ptr));
    ObjectLayer::set_dyn_size(&tmp, ObjectLayer::dyn_size(&stroke_options));
    ObjectLayer::set_dyn_capacity(&tmp, ObjectLayer::dyn_capacity(&stroke_options));
    return ObjectLayer::replace_object(dash_array_out, &tmp);
  }
  else {
    return dash_array_out->dcast<BLArray<float>>().assign_data(stroke_options.dcast().dash_array());
  }
}

BL_API_IMPL BLResult bl_context_set_stroke_dash_array_object(BLContextCore* self, const BLArrayCore* dash_array) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_stroke_dash_array_object(self, dash_array);
}

BL_API_IMPL BLResult bl_context_set_stroke_dash_array_data(BLContextCore* self, const float* data, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_stroke_dash_array_data(self, data, size);
}

BL_API_IMPL BLResult bl_context_get_stroke_options(const BLContextCore* self, BLStrokeOptionsCore* options) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const BLContextState* state = context_get_state(self);
  return bl_stroke_options_assign_weak(options, &state->stroke_options);
}

BL_API_IMPL BLResult bl_context_set_stroke_options(BLContextCore* self, const BLStrokeOptionsCore* options) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->set_stroke_options(self, options);
}

// Context - API - Clip Operations
// ================================

BL_API_IMPL BLResult bl_context_clip_to_rect_i(BLContextCore* self, const BLRectI* rect) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->clip_to_rect_i(self, rect);
}

BL_API_IMPL BLResult bl_context_clip_to_rect_d(BLContextCore* self, const BLRect* rect) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->clip_to_rect_d(self, rect);
}

BL_API_IMPL BLResult bl_context_restore_clipping(BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->restore_clipping(self);
}

// Context - API - Clear Geometry Operations
// ==========================================

BL_API_IMPL BLResult bl_context_clear_all(BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->clear_all(self);
}

BL_API_IMPL BLResult bl_context_clear_rect_i(BLContextCore* self, const BLRectI* rect) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->clear_rect_i(self, rect);
}

BL_API_IMPL BLResult bl_context_clear_rect_d(BLContextCore* self, const BLRect* rect) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->clear_rect_d(self, rect);
}

// Context - API - Fill All Operations
// ====================================

BL_API_IMPL BLResult bl_context_fill_all(BLContextCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_all(self);
}

BL_API_IMPL BLResult bl_context_fill_all_rgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_all_rgba32(self, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_all_rgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->fill_all_ext(self, &style);
}

BL_API_IMPL BLResult bl_context_fill_all_ext(BLContextCore* self, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_all_ext(self, static_cast<const BLObjectCore*>(style));
}

// Context - API - Fill Rect Operations
// =====================================

BL_API_IMPL BLResult bl_context_fill_rect_i(BLContextCore* self, const BLRectI* rect) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_rect_i(self, rect);
}

BL_API_IMPL BLResult bl_context_fill_rect_i_rgba32(BLContextCore* self, const BLRectI* rect, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_rect_i_rgba32(self, rect, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_rect_i_rgba64(BLContextCore* self, const BLRectI* rect, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->fill_rect_i_ext(self, rect, &style);
}

BL_API_IMPL BLResult bl_context_fill_rect_i_ext(BLContextCore* self, const BLRectI* rect, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_rect_i_ext(self, rect, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_fill_rect_d(BLContextCore* self, const BLRect* rect) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_rect_d(self, rect);
}

BL_API_IMPL BLResult bl_context_fill_rect_d_rgba32(BLContextCore* self, const BLRect* rect, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_rect_d_rgba32(self, rect, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_rect_d_rgba64(BLContextCore* self, const BLRect* rect, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->fill_rect_d_ext(self, rect, &style);
}

BL_API_IMPL BLResult bl_context_fill_rect_d_ext(BLContextCore* self, const BLRect* rect, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_rect_d_ext(self, rect, static_cast<const BLObjectCore*>(style));
}

// Context - API - Fill Path Operations
// =====================================

BL_API_IMPL BLResult bl_context_fill_path_d(BLContextCore* self, const BLPoint* origin, const BLPathCore* path) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_path_d(self, origin, path);
}

BL_API_IMPL BLResult bl_context_fill_path_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_path_d_rgba32(self, origin, path, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_path_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->fill_path_d_ext(self, origin, path, &style);
}

BL_API_IMPL BLResult bl_context_fill_path_d_ext(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_path_d_ext(self, origin, path, static_cast<const BLObjectCore*>(style));
}

// Context - API - Fill Geometry Operations
// =========================================

BL_API_IMPL BLResult bl_context_fill_geometry(BLContextCore* self, BLGeometryType type, const void* data) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_geometry(self, type, data);
}

BL_API_IMPL BLResult bl_context_fill_geometry_rgba32(BLContextCore* self, BLGeometryType type, const void* data, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_geometry_rgba32(self, type, data, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_geometry_rgba64(BLContextCore* self, BLGeometryType type, const void* data, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->fill_geometry_ext(self, type, data, &style);
}

BL_API_IMPL BLResult bl_context_fill_geometry_ext(BLContextCore* self, BLGeometryType type, const void* data, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_geometry_ext(self, type, data, static_cast<const BLObjectCore*>(style));
}

// Context - API - Fill UTF-8 Text Operations
// ===========================================

BL_API_IMPL BLResult bl_context_fill_utf8_text_i(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->fill_text_op_i(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span);
}

BL_API_IMPL BLResult bl_context_fill_utf8_text_i_rgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->fill_text_op_i_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_utf8_text_i_rgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLStringView span{text, size};
  return self->_virt()->fill_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, &style);
}

BL_API_IMPL BLResult bl_context_fill_utf8_text_i_ext(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->fill_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_fill_utf8_text_d(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->fill_text_op_d(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span);
}

BL_API_IMPL BLResult bl_context_fill_utf8_text_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->fill_text_op_d_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_utf8_text_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLStringView span{text, size};
  return self->_virt()->fill_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, &style);
}

BL_API_IMPL BLResult bl_context_fill_utf8_text_d_ext(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->fill_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, static_cast<const BLObjectCore*>(style));
}

// Context - API - Fill UTF-16 Text Operations
// ============================================

BL_API_IMPL BLResult bl_context_fill_utf16_text_i(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->fill_text_op_i(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span);
}

BL_API_IMPL BLResult bl_context_fill_utf16_text_i_rgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->fill_text_op_i_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_utf16_text_i_rgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->fill_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, &style);
}

BL_API_IMPL BLResult bl_context_fill_utf16_text_i_ext(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->fill_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_fill_utf16_text_d(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->fill_text_op_d(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span);
}

BL_API_IMPL BLResult bl_context_fill_utf16_text_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->fill_text_op_d_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_utf16_text_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->fill_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, &style);
}

BL_API_IMPL BLResult bl_context_fill_utf16_text_d_ext(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->fill_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, static_cast<const BLObjectCore*>(style));
}

// Context - API - Fill UTF-32 Text Operations
// ============================================

BL_API_IMPL BLResult bl_context_fill_utf32_text_i(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->fill_text_op_i(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span);
}

BL_API_IMPL BLResult bl_context_fill_utf32_text_i_rgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->fill_text_op_i_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_utf32_text_i_rgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->fill_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, &style);
}

BL_API_IMPL BLResult bl_context_fill_utf32_text_i_ext(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->fill_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_fill_utf32_text_d(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->fill_text_op_d(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span);
}

BL_API_IMPL BLResult bl_context_fill_utf32_text_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->fill_text_op_d_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_utf32_text_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->fill_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, &style);
}

BL_API_IMPL BLResult bl_context_fill_utf32_text_d_ext(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->fill_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, static_cast<const BLObjectCore*>(style));
}

// Context - API - Fill Glyph Run Operations
// ==========================================

BL_API_IMPL BLResult bl_context_fill_glyph_run_i(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyph_run) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_text_op_i(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run);
}

BL_API_IMPL BLResult bl_context_fill_glyph_run_i_rgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_text_op_i_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_glyph_run_i_rgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->fill_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, &style);
}

BL_API_IMPL BLResult bl_context_fill_glyph_run_i_ext(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_fill_glyph_run_d(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyph_run) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_text_op_d(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run);
}

BL_API_IMPL BLResult bl_context_fill_glyph_run_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_text_op_d_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_glyph_run_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->fill_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, &style);
}

BL_API_IMPL BLResult bl_context_fill_glyph_run_d_ext(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, static_cast<const BLObjectCore*>(style));
}

// Context - API - Fill Mask Operations
// =====================================

BL_API_IMPL BLResult bl_context_fill_mask_i(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* mask_area) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_mask_i(self, origin, mask, mask_area);
}

BL_API_IMPL BLResult bl_context_fill_mask_i_rgba32(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* mask_area, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_mask_i_rgba32(self, origin, mask, mask_area, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_mask_i_rgba64(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* mask_area, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->fill_mask_i_ext(self, origin, mask, mask_area, &style);
}

BL_API_IMPL BLResult bl_context_fill_mask_i_ext(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* mask_area, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_mask_i_ext(self, origin, mask, mask_area, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_fill_mask_d(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* mask_area) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_mask_d(self, origin, mask, mask_area);
}

BL_API_IMPL BLResult bl_context_fill_mask_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* mask_area, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_mask_d_Rgba32(self, origin, mask, mask_area, rgba32);
}

BL_API_IMPL BLResult bl_context_fill_mask_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* mask_area, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->fill_mask_d_ext(self, origin, mask, mask_area, &style);
}

BL_API_IMPL BLResult bl_context_fill_mask_d_ext(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* mask_area, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->fill_mask_d_ext(self, origin, mask, mask_area, static_cast<const BLObjectCore*>(style));
}

// Context - API - Stroke Rect Operations
// =======================================

BL_API_IMPL BLResult bl_context_stroke_rect_i(BLContextCore* self, const BLRectI* rect) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_geometry(self, BL_GEOMETRY_TYPE_RECT_I, rect);
}

BL_API_IMPL BLResult bl_context_stroke_rect_i_rgba32(BLContextCore* self, const BLRectI* rect, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_geometry_rgba32(self, BL_GEOMETRY_TYPE_RECT_I, rect, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_rect_i_rgba64(BLContextCore* self, const BLRectI* rect, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->stroke_geometry_ext(self, BL_GEOMETRY_TYPE_RECT_I, rect, &style);
}

BL_API_IMPL BLResult bl_context_stroke_rect_i_ext(BLContextCore* self, const BLRectI* rect, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_geometry_ext(self, BL_GEOMETRY_TYPE_RECT_I, rect, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_stroke_rect_d(BLContextCore* self, const BLRect* rect) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_geometry(self, BL_GEOMETRY_TYPE_RECT_D, rect);
}

BL_API_IMPL BLResult bl_context_stroke_rect_d_rgba32(BLContextCore* self, const BLRect* rect, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_geometry_rgba32(self, BL_GEOMETRY_TYPE_RECT_D, rect, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_rect_d_rgba64(BLContextCore* self, const BLRect* rect, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->stroke_geometry_ext(self, BL_GEOMETRY_TYPE_RECT_D, rect, &style);
}

BL_API_IMPL BLResult bl_context_stroke_rect_d_ext(BLContextCore* self, const BLRect* rect, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_geometry_ext(self, BL_GEOMETRY_TYPE_RECT_D, rect, static_cast<const BLObjectCore*>(style));
}

// Context - API - Stroke Path Operations
// =======================================

BL_API_IMPL BLResult bl_context_stroke_path_d(BLContextCore* self, const BLPoint* origin, const BLPathCore* path) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_path_d(self, origin, path);
}

BL_API_IMPL BLResult bl_context_stroke_path_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_path_d_rgba32(self, origin, path, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_path_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->stroke_path_d_ext(self, origin, path, &style);
}

BL_API_IMPL BLResult bl_context_stroke_path_d_ext(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_path_d_ext(self, origin, path, static_cast<const BLObjectCore*>(style));
}

// Context - API - Stroke Geometry Operations
// ===========================================

BL_API_IMPL BLResult bl_context_stroke_geometry(BLContextCore* self, BLGeometryType type, const void* data) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_geometry(self, type, data);
}

BL_API_IMPL BLResult bl_context_stroke_geometry_rgba32(BLContextCore* self, BLGeometryType type, const void* data, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_geometry_rgba32(self, type, data, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_geometry_rgba64(BLContextCore* self, BLGeometryType type, const void* data, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->stroke_geometry_ext(self, type, data, &style);
}

BL_API_IMPL BLResult bl_context_stroke_geometry_ext(BLContextCore* self, BLGeometryType type, const void* data, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_geometry_ext(self, type, data, static_cast<const BLObjectCore*>(style));
}

// Context - API - Stroke UTF-8 Text Operations
// =============================================

BL_API_IMPL BLResult bl_context_stroke_utf8_text_i(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->stroke_text_op_i(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span);
}

BL_API_IMPL BLResult bl_context_stroke_utf8_text_i_rgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->stroke_text_op_i_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_utf8_text_i_rgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLStringView span{text, size};
  return self->_virt()->stroke_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, &style);
}

BL_API_IMPL BLResult bl_context_stroke_utf8_text_i_ext(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->stroke_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_stroke_utf8_text_d(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->stroke_text_op_d(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span);
}

BL_API_IMPL BLResult bl_context_stroke_utf8_text_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->stroke_text_op_d_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_utf8_text_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLStringView span{text, size};
  return self->_virt()->stroke_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, &style);
}

BL_API_IMPL BLResult bl_context_stroke_utf8_text_d_ext(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringView span{text, size};
  return self->_virt()->stroke_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &span, static_cast<const BLObjectCore*>(style));
}

// Context - API - Stroke UTF-16 Text Operations
// ==============================================

BL_API_IMPL BLResult bl_context_stroke_utf16_text_i(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->stroke_text_op_i(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span);
}

BL_API_IMPL BLResult bl_context_stroke_utf16_text_i_rgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->stroke_text_op_i_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_utf16_text_i_rgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->stroke_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, &style);
}

BL_API_IMPL BLResult bl_context_stroke_utf16_text_i_ext(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->stroke_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_stroke_utf16_text_d(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->stroke_text_op_d(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span);
}

BL_API_IMPL BLResult bl_context_stroke_utf16_text_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->stroke_text_op_d_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_utf16_text_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->stroke_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, &style);
}

BL_API_IMPL BLResult bl_context_stroke_utf16_text_d_ext(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint16_t> span{text, size};
  return self->_virt()->stroke_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &span, static_cast<const BLObjectCore*>(style));
}

// Context - API - Stroke UTF-32 Text Operations
// ==============================================

BL_API_IMPL BLResult bl_context_stroke_utf32_text_i(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->stroke_text_op_i(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span);
}

BL_API_IMPL BLResult bl_context_stroke_utf32_text_i_rgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->stroke_text_op_i_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_utf32_text_i_rgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->stroke_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, &style);
}

BL_API_IMPL BLResult bl_context_stroke_utf32_text_i_ext(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->stroke_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_stroke_utf32_text_d(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->stroke_text_op_d(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span);
}

BL_API_IMPL BLResult bl_context_stroke_utf32_text_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->stroke_text_op_d_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_utf32_text_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->stroke_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, &style);
}

BL_API_IMPL BLResult bl_context_stroke_utf32_text_d_ext(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const uint32_t> span{text, size};
  return self->_virt()->stroke_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &span, static_cast<const BLObjectCore*>(style));
}

// Context - API - Stroke Glyph Run Operations
// ============================================

BL_API_IMPL BLResult bl_context_stroke_glyph_run_i(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyph_run) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_text_op_i(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run);
}

BL_API_IMPL BLResult bl_context_stroke_glyph_run_i_rgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_text_op_i_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_glyph_run_i_rgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->stroke_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, &style);
}

BL_API_IMPL BLResult bl_context_stroke_glyph_run_i_ext(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_text_op_i_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult bl_context_stroke_glyph_run_d(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyph_run) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_text_op_d(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run);
}

BL_API_IMPL BLResult bl_context_stroke_glyph_run_d_rgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, uint32_t rgba32) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_text_op_d_rgba32(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, rgba32);
}

BL_API_IMPL BLResult bl_context_stroke_glyph_run_d_rgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, uint64_t rgba64) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLVarCore style = Internal::make_inline_style(BLRgba64(rgba64));
  return self->_virt()->stroke_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, &style);
}

BL_API_IMPL BLResult bl_context_stroke_glyph_run_d_ext(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyph_run, const BLUnknown* style) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->stroke_text_op_d_ext(self, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyph_run, static_cast<const BLObjectCore*>(style));
}

// Context - API - Blit Operations
// ================================

BL_API_IMPL BLResult bl_context_blit_image_i(BLContextCore* self, const BLPointI* pt, const BLImageCore* img, const BLRectI* img_area) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->blit_image_i(self, pt, img, img_area);
}

BL_API_IMPL BLResult bl_context_blit_image_d(BLContextCore* self, const BLPoint* pt, const BLImageCore* img, const BLRectI* img_area) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->blit_image_d(self, pt, img, img_area);
}

BL_API_IMPL BLResult bl_context_blit_scaled_image_i(BLContextCore* self, const BLRectI* rect, const BLImageCore* img, const BLRectI* img_area) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->blit_scaled_image_i(self, rect, img, img_area);
}

BL_API_IMPL BLResult bl_context_blit_scaled_image_d(BLContextCore* self, const BLRect* rect, const BLImageCore* img, const BLRectI* img_area) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->blit_scaled_image_d(self, rect, img, img_area);
}

// Context - Runtime Registration
// ===============================

void bl_context_rt_init(BLRuntimeContext* rt) noexcept {
  using namespace bl;
  static constexpr BLObjectType object_type = BLContext::Info::kType;

  context_init_state(&null_state);
  init_null_context_virt(&null_context.virt);

  BLContextCore* null_object = static_cast<BLContextCore*>(&bl_object_defaults[object_type]);
  ObjectLayer::init_virt(null_object, object_type, uint8_t(0), &null_context.impl, &null_context.virt);
  null_object->_d.dyn.extra_ptr = &null_state;

  // Initialize built-in rendering context implementations.
  bl_raster_context_on_init(rt);
}
