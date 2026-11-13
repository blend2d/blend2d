// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_CONTEXT_P_H_INCLUDED
#define BLEND2D_CORE_CONTEXT_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/context.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/threading/atomic_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

template<>
struct ObjectDispatchInfo<BLContextCore> {
  using ImplType = BLContextImpl;

  static constexpr bool kHasInitSSO = false;

  static BL_INLINE BLResult free_object(BLContextCore* self, ImplType* impl) noexcept {
    axl::maybe_unused(impl);
    return self->_virt()->base.destroy(self);
  }
};

static constexpr const float context_min_tolerance = 0.01f;
static constexpr const float context_max_tolerance = 0.50f;

static BL_INLINE void context_init_state(BLContextState* self) noexcept {
  self->target_image = nullptr;
  self->target_size.reset();
  self->hints.reset();
  self->comp_op = uint8_t(BL_COMP_OP_SRC_OVER);
  self->fill_rule = uint8_t(BL_FILL_RULE_NON_ZERO);
  self->style_type[BL_CONTEXT_STYLE_SLOT_FILL] = uint8_t(BL_OBJECT_TYPE_NULL);
  self->style_type[BL_CONTEXT_STYLE_SLOT_STROKE] = uint8_t(BL_OBJECT_TYPE_NULL);
  self->saved_state_count = 0u;

  self->global_alpha = 1.0;
  self->style_alpha[BL_CONTEXT_STYLE_SLOT_FILL] = 1.0;
  self->style_alpha[BL_CONTEXT_STYLE_SLOT_STROKE] = 1.0;

  bl_stroke_options_init(&self->stroke_options);
  self->approximation_options = PathDetail::make_default_approximation_options();

  self->meta_transform.reset();
  self->user_transform.reset();
}

static BL_INLINE void context_destroy_state(BLContextState* self) noexcept {
  bl_stroke_options_destroy(&self->stroke_options);
}

static BL_INLINE_NODEBUG const BLContextState* context_get_state(const BLContextCore* self) noexcept {
  return static_cast<const BLContextState*>(self->_d.dyn.extra_ptr);
}

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_CONTEXT_P_H_INCLUDED
