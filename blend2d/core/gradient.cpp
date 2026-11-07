// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/algorithm_p.h>
#include <blend2d/axl/commons.h>
#include <blend2d/axl/lookuptable_p.h>
#include <blend2d/axl/math_p.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/format_p.h>
#include <blend2d/core/gradient_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/rgba_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/pixelops/funcs_p.h>
#include <blend2d/threading/atomic_p.h>

namespace bl::GradientDetail {

// Gradient - Internals - Null
// ===========================

static consteval GradientPrivateImpl make_eternal_gradient_impl() noexcept {
  GradientPrivateImpl impl{};
  impl.transform = BLMatrix2D::make_identity();
  return impl;
}

static constexpr ObjectLayer::EternalImpl<GradientPrivateImpl> null_gradient {
  .padding{},
  .header{},
  .impl = make_eternal_gradient_impl()
};

// Gradient - Internals - Tables
// =============================

struct BLGradientValueCountTableGen {
  static consteval uint8_t value(size_t i) noexcept {
    return i == BL_GRADIENT_TYPE_LINEAR ? uint8_t(sizeof(BLLinearGradientValues) / sizeof(double)) :
           i == BL_GRADIENT_TYPE_RADIAL ? uint8_t(sizeof(BLRadialGradientValues) / sizeof(double)) :
           i == BL_GRADIENT_TYPE_CONIC  ? uint8_t(sizeof(BLConicGradientValues ) / sizeof(double)) : uint8_t(0);
  }
};

static constexpr const auto gradient_value_count_table =
  make_lookup_table<uint8_t, BL_GRADIENT_TYPE_MAX_VALUE + 1, BLGradientValueCountTableGen>();

// Gradient - Internals & Utilities
// ================================

static constexpr size_t gradient_impl_size_initial = axl::align_up(size_t(impl_size_from_capacity(4)), ObjectLayer::kMinImplAlignment);

// Gradient - Internals - Analysis
// ===============================

//! Gradient analysis describes whether color stops are conforming to the storage, non-conforming, or totally invalid.
enum class GradientAnalysis : uint32_t {
  //! There are no gradient stops or gradient stops are conforming (passed exactly as expected).
  Conforming = 0, // Must be 0
  //! Gradient stops are valid, but non-conforming (must be processed).
  NonConforming = 1, // Must be 1

  //! Gradient stops are invalid (offset possibly out of range).
  Invalid = BL_ERROR_INVALID_VALUE,
  //! Gradient is too large and cannot be possibly allocated and placed into Impl.
  TooLarge = BL_ERROR_OUT_OF_MEMORY
};

static BL_INLINE_NODEBUG bool analysis_failed(GradientAnalysis analysis_result) noexcept {
  return analysis_result > GradientAnalysis::NonConforming;
}

static BL_INLINE bool gradient_is_valid_offset(double offset) noexcept {
  return axl::bool_and(offset >= 0.0, offset <= 1.0);
}

static BL_INLINE GradientAnalysis gradient_analyze_stops(BLSpan<const BLGradientStop> stops) noexcept {
  uint32_t result = static_cast<uint32_t>(GradientAnalysis::Conforming);
  size_t i = 0;

  // Place the check here.
  if (stops.size() - 1u >= kMaxSize) {
    result = static_cast<uint32_t>(
      stops.size() == 0 ? GradientAnalysis::Conforming : GradientAnalysis::TooLarge);
  }
  else {
    double prev = -1.0;
    uint32_t was_same = false;

    do {
      double offset = stops[i].offset;
      if (!gradient_is_valid_offset(offset)) {
        return GradientAnalysis::Invalid;
      }

      uint32_t is_same = uint32_t(offset == prev);
      result |= uint32_t(offset < prev);
      result |= is_same & was_same;

      prev = offset;
      was_same = is_same;
    } while (++i < stops.size());
    BL_ASSUME(result <= 1u);
  }

  return static_cast<GradientAnalysis>(result);
}

// Gradient - Internals - Stop Matcher
// ===================================

struct GradientStopMatcher {
  double offset;
  BL_INLINE GradientStopMatcher(double offset) noexcept : offset(offset) {}
};
static BL_INLINE bool operator==(const BLGradientStop& a, const GradientStopMatcher& b) noexcept { return a.offset == b.offset; }
static BL_INLINE bool operator<=(const BLGradientStop& a, const GradientStopMatcher& b) noexcept { return a.offset <= b.offset; }

// Gradient - Internals - AltStop
// ==============================

// Alternative representation of `BLGradientStop` that is used to sort unknown stop array that is either unsorted or
// may contain more than 2 stops that have the same offset. The `index` member is actually an index to the original
// stop array.
struct GradientStopAlt {
  double offset;
  union {
    intptr_t index;
    uint64_t rgba;
  };
};

static_assert(sizeof(GradientStopAlt) == sizeof(BLGradientStop));

// Gradient - Internals - Utilities
// ================================

static BL_INLINE void gradient_init_values(double* dst, const double* src, size_t n) noexcept {
  size_t i;

  BL_NOUNROLL
  for (i = 0; i < n; i++) {
    dst[i] = src[i];
  }

  BL_NOUNROLL
  while (i <= BL_GRADIENT_VALUE_MAX_VALUE) {
    dst[i++] = 0.0;
  }
}

static BL_INLINE void gradient_move_stops(BLGradientStop* dst, const BLGradientStop* src, size_t n) noexcept {
  memmove(dst, src, n * sizeof(BLGradientStop));
}

static BL_INLINE size_t gradient_copy_stops(BLGradientStop* dst, const BLGradientStop* src, size_t n) noexcept {
  axl::copy_forward(dst, src, n);
  return n;
}

static BL_NOINLINE size_t gradient_copy_unsafe_stops(BLGradientStop* dst, const BLGradientStop* src, size_t n, GradientAnalysis analysis) noexcept {
  BL_ASSERT(analysis == GradientAnalysis::Conforming ||
            analysis == GradientAnalysis::NonConforming);

  if (analysis == GradientAnalysis::Conforming) {
    return gradient_copy_stops(dst, src, n);
  }

  // If `n == 0` the analysis would always be `GradientAnalysis::Conforming`.
  BL_ASSERT(n != 0u);

  // First copy source stops into the destination and index them.
  GradientStopAlt* stops = reinterpret_cast<GradientStopAlt*>(dst);
  for (size_t i = 0; i < n; i++) {
    stops[i].offset = src[i].offset;
    stops[i].index = intptr_t(i);
  }

  // Now sort the stops and use both `offset` and `index` as a comparator. After the sort is done we will have
  // preserved the order of all stops that have the same `offset`.
  bl::quick_sort(stops, n, [](const GradientStopAlt& a, const GradientStopAlt& b) noexcept -> intptr_t {
    intptr_t result = 0;
    if (a.offset < b.offset) result = -1;
    if (a.offset > b.offset) result = 1;
    return result ? result : a.index - b.index;
  });

  // Now assign rgba value to the stop and remove all duplicates. If there are 3 or more consecutive stops we
  // remove all except the first/second to make sharp transitions possible.
  size_t j = 0;
  double prev1 = -1.0; // Dummy, cannot be within [0..1] range.
  double prev2 = -1.0;

  for (size_t i = 0; i < n; i++) {
    double offset = stops[i].offset;
    BLRgba64 rgba = src[size_t(stops[i].index)].rgba;

    j -= size_t((prev1 == prev2) & (prev2 == offset));
    stops[j].offset = offset;
    stops[j].rgba = rgba.value;

    j++;
    prev1 = prev2;
    prev2 = offset;
  }

  // Returns the final number of stops kept. Could be the same as `n` or less.
  return j;
}

static BL_INLINE GradientLUT* copy_maybe_null_lut(GradientLUT* lut) noexcept {
  return lut ? lut->retain() : nullptr;
}

// Cache invalidation means to remove the cached lut tables from `impl`. Since modification always means to either
// create a copy of it or to modify a unique instance (not shared) it also means that we don't have to worry about
// atomic operations here.
static BL_INLINE BLResult gradient_invalidate_cache(GradientPrivateImpl* impl) noexcept {
  GradientLUT* lut32 = impl->lut32();
  GradientLUT* lut64 = impl->lut64();

  if (uintptr_t(lut32) | uintptr_t(lut64)) {
    if (lut32) {
      lut32->release();
    }

    if (lut64) {
      lut64->release();
    }

    impl->lut[0] = nullptr;
    impl->lut[1] = nullptr;
  }

  impl->info32.packed = 0;
  return BL_SUCCESS;
}

GradientInfo ensure_info(const BLGradientCore* self) noexcept {
  constexpr uint32_t FLAG_ALPHA_NOT_ONE  = 0x1; // Has alpha that is not 1.0.
  constexpr uint32_t FLAG_ALPHA_NOT_ZERO = 0x2; // Has alpha that is not 0.0.
  constexpr uint32_t FLAG_TRANSITION     = 0x4; // Has transition.

  GradientPrivateImpl* impl = ObjectLayer::get_impl(self);
  GradientInfo info = impl->info32;

  if (info.packed == 0) {
    BLSpan<const BLGradientStop> stops = get_stops(self);

    if (stops.size() != 0) {
      uint32_t flags = 0;
      uint64_t prev = stops[0].rgba.value & 0xFF00FF00FF00FF00u;
      uint32_t lut_size = 0;

      if (prev < 0xFF00000000000000u) {
        flags |= FLAG_ALPHA_NOT_ONE;
      }

      if (prev > 0x00FFFFFFFFFFFFFFu) {
        flags |= FLAG_ALPHA_NOT_ZERO;
      }

      for (size_t i = 1; i < stops.size(); i++) {
        uint64_t value = stops[i].rgba.value & 0xFF00FF00FF00FF00u;
        if (value == prev) {
          continue;
        }

        flags |= FLAG_TRANSITION;
        if (value < 0xFF00000000000000u) {
          flags |= FLAG_ALPHA_NOT_ONE;
        }
        if (value > 0x00FFFFFFFFFFFFFFu) {
          flags |= FLAG_ALPHA_NOT_ZERO;
        }
        prev = value;
      }

      // If all alpha values are zero then we consider this to be without transition, because the whole transition
      // would result in transparent black.
      if (!(flags & FLAG_ALPHA_NOT_ZERO)) {
        flags &= ~FLAG_TRANSITION;
      }

      if (!(flags & FLAG_TRANSITION)) {
        // Minimal LUT size for no transition. The engine should always convert such style into solid fill, so such
        // LUT should never be used by the renderer.
        lut_size = 256;
      }
      else {
        // TODO: This is kinda adhoc, it would be much better if we base the calculation on both stops and their
        // offsets and estimate how big the ideal table should be.
        switch (stops.size()) {
          case 1: {
            lut_size = 256;
            break;
          }

          case 2: {
            // 2 stops at endpoints only require 256 entries, more stops will use 512.
            double delta = stops[1].offset - stops[0].offset;
            lut_size = (delta >= 0.998) ? 256 : 512;
            break;
          }

          case 3: {
            lut_size = (stops[0].offset <= 0.002 && stops[1].offset == 0.5 && stops[2].offset >= 0.998) ? 512 : 1024;
            break;
          }

          default: {
            lut_size = 1024;
            break;
          }
        }
      }

      info.solid = uint8_t(flags & FLAG_TRANSITION ? 0 : 1);
      info.format = uint8_t(flags & FLAG_ALPHA_NOT_ONE ? FormatExt::kPRGB32 : FormatExt::kFRGB32);
      info._lut_size = uint16_t(lut_size);

      // Update the info. It doesn't have to be atomic.
      impl->info32.packed = info.packed;
    }
  }

  return info;
}

GradientLUT* ensure_lut32(const BLGradientCore* self, GradientPrivateImpl* impl, uint32_t lut_size) noexcept {
  GradientLUT* lut = impl->lut32();
  if (lut) {
    BL_ASSERT(lut->size == lut_size);
    return lut;
  }

  lut = GradientLUT::alloc(lut_size, 4);
  if (BL_UNLIKELY(!lut)) {
    return nullptr;
  }

  BLSpan<const BLGradientStop> stops = get_stops(self);
  PixelOps::funcs.interpolate_prgb32(lut->data<uint32_t>(), lut_size, stops.data(), stops.size());

  // We must drop this LUT if another thread created it meanwhile.
  GradientLUT* expected = nullptr;
  if (!Atomic::compare_exchange(impl->lut[GradientLUT::index_32], expected, lut)) {
    BL_ASSERT(expected != nullptr);
    GradientLUT::destroy(lut);
    lut = expected;
  }

  return lut;
}

GradientLUT* ensure_lut64(const BLGradientCore* self, GradientPrivateImpl* impl, uint32_t lut_size) noexcept {
  GradientLUT* lut = impl->lut64();
  if (lut) {
    BL_ASSERT(lut->size == lut_size);
    return lut;
  }

  lut = GradientLUT::alloc(lut_size, 8);
  if (BL_UNLIKELY(!lut)) {
    return nullptr;
  }

  BLSpan<const BLGradientStop> stops = get_stops(self);
  PixelOps::funcs.interpolate_prgb64(lut->data<uint64_t>(), lut_size, stops.data(), stops.size());

  // We must drop this LUT if another thread created it meanwhile.
  GradientLUT* expected = nullptr;
  if (!Atomic::compare_exchange(impl->lut[GradientLUT::index_64], expected, lut)) {
    BL_ASSERT(expected != nullptr);
    GradientLUT::destroy(lut);
    lut = expected;
  }

  return lut;
}

// Gradient - Internals - Alloc & Free Impl
// ========================================

static BLResult gradient_alloc_impl(BLGradientCore* self, ObjectImplSize impl_size, const double* values, size_t value_count, const BLMatrix2D& transform) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_impl_t<GradientPrivateImpl>(self, BL_OBJECT_TYPE_GRADIENT, impl_size));

  GradientPrivateImpl* impl = ObjectLayer::get_impl(self);
  ObjectLayer::set_dyn_data(self, axl::offset_ptr<BLGradientStop>(impl, ObjectLayer::data_offset_of<BLGradientCore>));
  ObjectLayer::set_dyn_capacity(self, capacity_from_impl_size(impl_size));

  impl->transform = transform;
  gradient_init_values(impl->values, values, value_count);
  impl->lut[0] = nullptr;
  impl->lut[1] = nullptr;
  impl->info32.packed = 0;
  return BL_SUCCESS;
}

static BLResult gradient_alloc_copy_impl(
  BLGradientCore* self,
  ObjectImplSize impl_size,
  const BLGradientCore* other,
  BLSpan<const BLGradientStop> stops
) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_impl_t<GradientPrivateImpl>(self, BL_OBJECT_TYPE_GRADIENT, impl_size));

  size_t capacity = capacity_from_impl_size(impl_size);
  BL_ASSERT(capacity >= stops.size());

  ObjectLayer::set_payload_u16(self, ObjectLayer::get_payload_u16(other));

  GradientPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  GradientStop* self_stops = axl::offset_ptr<BLGradientStop>(self_impl, ObjectLayer::data_offset_of<BLGradientCore>);
  ObjectLayer::set_dyn_data(self, self_stops);
  ObjectLayer::set_dyn_size(self, stops.size());
  ObjectLayer::set_dyn_capacity(self, capacity);

  GradientPrivateImpl* other_impl = ObjectLayer::get_impl(other);
  self_impl->transform = other_impl->transform;
  memcpy(self_impl->values, other_impl->values, sizeof(self_impl->values));

  self_impl->lut[0] = nullptr;
  self_impl->lut[1] = nullptr;
  self_impl->info32.packed = 0;

  gradient_copy_stops(self_stops, stops.data(), stops.size());
  return BL_SUCCESS;
}

BLResult free_object(BLGradientCore* self, GradientPrivateImpl* impl, ObjectLayer::MetaByte meta_byte) noexcept {
  axl::maybe_unused(self);
  gradient_invalidate_cache(impl);
  return ObjectLayer::dealloc_impl(impl, meta_byte);
}

// Gradient - Internals - Deep Copy & Mutation
// ===========================================

static BL_NOINLINE BLResult deep_copy(BLGradientCore* self, const BLGradientCore* other, bool copy_cache) noexcept {
  const GradientPrivateImpl* other_impl = ObjectLayer::get_impl(other);

  BLGradientCore tmp;
  ObjectImplSize impl_size = impl_size_from_capacity(ObjectLayer::dyn_capacity(self));
  BL_PROPAGATE(gradient_alloc_copy_impl(&tmp, impl_size, other, get_stops(other)));

  GradientPrivateImpl* tmp_impl = ObjectLayer::get_impl(&tmp);
  if (copy_cache) {
    tmp_impl->lut[0] = copy_maybe_null_lut(other_impl->lut[0]);
    tmp_impl->lut[1] = copy_maybe_null_lut(other_impl->lut[1]);
    tmp_impl->info32.packed = other_impl->info32.packed;
  }

  return ObjectLayer::replace_object(self, &tmp);
}

static BL_INLINE BLResult make_mutable(BLGradientCore* self, bool copy_cache) noexcept {
  // NOTE: `copy_cache` should be a constant so its handling should have zero cost.
  if (!ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
    return deep_copy(self, self, copy_cache);
  }

  if (!copy_cache) {
    return gradient_invalidate_cache(ObjectLayer::get_impl(self));
  }

  return BL_SUCCESS;
}

} // {bl::GradientDetail}

// Gradient - API - Init & Destroy
// ===============================

BL_API_IMPL BLResult bl_gradient_init(BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  return ObjectLayer::init_object(self);
}

BL_API_IMPL BLResult bl_gradient_init_move(BLGradientCore* self, BLGradientCore* other) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_gradient_init_weak(BLGradientCore* self, const BLGradientCore* other) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_gradient_init_as(BLGradientCore* self, BLGradientType type, const void* values, BLExtendMode extend_mode, const BLGradientStop* stops, size_t n, const BLMatrix2D* transform) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);

  ObjectLayer::init_object(self);
  return bl_gradient_create(self, type, values, extend_mode, stops, n, transform);
}

BL_API_IMPL BLResult bl_gradient_destroy(BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object<ObjectLayer::ObjectMode::kAny>(self);
}

// Gradient - API - Reset
// ======================

BL_API_IMPL BLResult bl_gradient_reset(BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// Gradient - API - Assign
// =======================

BL_API_IMPL BLResult bl_gradient_assign_move(BLGradientCore* self, BLGradientCore* other) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_gradient_assign_weak(BLGradientCore* self, const BLGradientCore* other) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

BL_API_IMPL BLResult bl_gradient_create(BLGradientCore* self, BLGradientType type, const void* values, BLExtendMode extend_mode, const BLGradientStop* stops, size_t n, const BLMatrix2D* transform) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(axl::bool_or(uint32_t(type) > BL_GRADIENT_TYPE_MAX_VALUE), (uint32_t(extend_mode) > BL_EXTEND_MODE_SIMPLE_MAX_VALUE))) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  uint32_t payload = (uint32_t(type       ) << BLGradient::GradientTypeField::kShift) |
                     (uint32_t(extend_mode) << BLGradient::ExtendModeField::kShift  ) ;

  if (!values) {
    values = null_gradient.impl.values;
  }

  if (!transform) {
    transform = &null_gradient.impl.transform;
  }
  else {
    payload |= (uint32_t(transform->type()) << BLGradient::TransformTypeField::kShift);
  }

  GradientAnalysis analysis = gradient_analyze_stops({stops, n});
  if (BL_UNLIKELY(analysis_failed(analysis))) {
    return bl_make_error(static_cast<BLResult>(analysis));
  }

  GradientPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  size_t value_count = gradient_value_count_table[type];

  if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n)) {
    ObjectLayer::set_payload_u16(self, payload);
    gradient_init_values(self_impl->values, static_cast<const double*>(values), value_count);
    self_impl->transform = *transform;
    ObjectLayer::set_dyn_size(self, gradient_copy_unsafe_stops(ObjectLayer::get_data<BLGradientStop>(self), stops, n, analysis));
    return gradient_invalidate_cache(self_impl);
  }
  else {
    BLGradientCore tmp;
    ObjectImplSize impl_size = bl_max(impl_size_from_capacity(n), ObjectImplSize(gradient_impl_size_initial));

    BL_PROPAGATE(gradient_alloc_impl(&tmp, impl_size, static_cast<const double*>(values), value_count, *transform));
    ObjectLayer::set_payload_u16(&tmp, payload);
    ObjectLayer::set_dyn_size(&tmp, gradient_copy_unsafe_stops(ObjectLayer::get_data<BLGradientStop>(&tmp), stops, n, analysis));
    return ObjectLayer::replace_object(self, &tmp);
  }
}

// Gradient - API - Storage
// ========================

BL_API_IMPL BLResult bl_gradient_shrink(BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ObjectImplSize fitting_impl_size = impl_size_from_capacity(ObjectLayer::get_size(self));
  ObjectImplSize current_impl_size = impl_size_from_capacity(ObjectLayer::get_capacity(self));

  if (fitting_impl_size == current_impl_size) {
    return BL_SUCCESS;
  }

  BLGradientCore tmp;
  BL_PROPAGATE(gradient_alloc_copy_impl(&tmp, fitting_impl_size, self, get_stops(self)));

  GradientPrivateImpl* tmp_impl = ObjectLayer::get_impl(&tmp);
  GradientPrivateImpl* old_impl = ObjectLayer::get_impl(self);

  tmp_impl->lut[0] = copy_maybe_null_lut(old_impl->lut[0]);
  tmp_impl->lut[1] = copy_maybe_null_lut(old_impl->lut[1]);

  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_gradient_reserve(BLGradientCore* self, size_t n) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n)) {
    return BL_SUCCESS;
  }

  if (BL_UNLIKELY(n >= kMaxSize)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLGradientCore tmp;
  ObjectImplSize impl_size = bl_max(impl_size_from_capacity(n), ObjectImplSize(gradient_impl_size_initial));
  BL_PROPAGATE(gradient_alloc_copy_impl(&tmp, impl_size, self, get_stops(self)));

  GradientPrivateImpl* tmp_impl = ObjectLayer::get_impl(&tmp);
  GradientPrivateImpl* old_impl = ObjectLayer::get_impl(self);

  tmp_impl->lut[0] = copy_maybe_null_lut(old_impl->lut[0]);
  tmp_impl->lut[1] = copy_maybe_null_lut(old_impl->lut[1]);

  return ObjectLayer::replace_object(self, &tmp);
}

// Gradient - API - Accessors
// ==========================

BL_API_IMPL BLGradientType bl_gradient_get_type(const BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_payload_field<BLGradient::GradientTypeField>(self);
}

BL_API_IMPL BLResult bl_gradient_set_type(BLGradientCore* self, BLGradientType type) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(uint32_t(type) > BL_GRADIENT_TYPE_MAX_VALUE)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  return ObjectLayer::set_payload_field<BLGradient::GradientTypeField>(self, type);
}

BL_API_IMPL BLExtendMode bl_gradient_get_extend_mode(const BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_payload_field<BLGradient::ExtendModeField>(self);
}

BL_API_IMPL BLResult bl_gradient_set_extend_mode(BLGradientCore* self, BLExtendMode extend_mode) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(extend_mode > BL_EXTEND_MODE_SIMPLE_MAX_VALUE)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  return ObjectLayer::set_payload_field<BLGradient::ExtendModeField>(self, extend_mode);
}

BL_API_IMPL double bl_gradient_get_value(const BLGradientCore* self, size_t index) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(index > BL_GRADIENT_VALUE_MAX_VALUE)) {
    return bl::Math::nan<double>();
  }

  return ObjectLayer::get_impl(self)->values[index];
}

BL_API_IMPL BLResult bl_gradient_set_value(BLGradientCore* self, size_t index, double value) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(index > BL_GRADIENT_VALUE_MAX_VALUE)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  BL_PROPAGATE(make_mutable(self, true));

  ObjectLayer::get_impl(self)->values[index] = value;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_gradient_set_values(BLGradientCore* self, size_t index, const double* values, size_t value_count) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(axl::bool_or(index > BL_GRADIENT_VALUE_MAX_VALUE,
                               value_count > BL_GRADIENT_VALUE_MAX_VALUE + 1 - index))) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  if (BL_UNLIKELY(!value_count)) {
    return BL_SUCCESS;
  }

  BL_PROPAGATE(make_mutable(self, true));
  double* dst = ObjectLayer::get_impl(self)->values + index;

  BL_NOUNROLL
  for (size_t i = 0; i < value_count; i++) {
    dst[i] = values[i];
  }

  return BL_SUCCESS;
}

// Gradient - API - Stops
// ======================

BL_API_IMPL size_t bl_gradient_get_size(const BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_size(self);
}

BL_API_IMPL size_t bl_gradient_get_capacity(const BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_capacity(self);
}

BL_API_IMPL const BLGradientStop* bl_gradient_get_stops(const BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_data<BLGradientStop>(self);
}

BL_API_IMPL BLResult bl_gradient_reset_stops(BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!ObjectLayer::get_size(self)) {
    return BL_SUCCESS;
  }

  GradientPrivateImpl* impl = ObjectLayer::get_impl(self);
  if (ObjectLayer::is_impl_mutable(impl)) {
    ObjectLayer::set_dyn_size(self, 0u);
    return gradient_invalidate_cache(impl);
  }
  else {
    BLGradientCore tmp;
    ObjectImplSize impl_size = ObjectImplSize{gradient_impl_size_initial};
    BL_PROPAGATE(gradient_alloc_copy_impl(&tmp, impl_size, self, {}));

    return ObjectLayer::replace_object(self, &tmp);
  }
}

BL_API_IMPL BLResult bl_gradient_assign_stops(BLGradientCore* self, const BLGradientStop* stops, size_t n) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(n == 0u)) {
    return bl_gradient_reset_stops(self);
  }

  GradientAnalysis analysis = gradient_analyze_stops({stops, n});
  if (BL_UNLIKELY(analysis_failed(analysis))) {
    return bl_make_error(static_cast<BLResult>(analysis));
  }

  if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n)) {
    ObjectLayer::set_dyn_size(self, gradient_copy_unsafe_stops(ObjectLayer::get_data<BLGradientStop>(self), stops, n, analysis));
    return gradient_invalidate_cache(ObjectLayer::get_impl(self));
  }
  else {
    BLGradientCore tmp;
    ObjectImplSize impl_size = bl_max(impl_size_from_capacity(n), ObjectImplSize(gradient_impl_size_initial));

    BL_PROPAGATE(gradient_alloc_copy_impl(&tmp, impl_size, self, {}));
    ObjectLayer::set_dyn_size(&tmp, gradient_copy_unsafe_stops(ObjectLayer::get_data<BLGradientStop>(&tmp), stops, n, analysis));

    return ObjectLayer::replace_object(self, &tmp);
  }
}

BL_API_IMPL BLResult bl_gradient_add_stop_rgba32(BLGradientCore* self, double offset, uint32_t rgba32) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_gradient_add_stop_rgba64(self, offset, bl::RgbaInternal::rgba64_from_rgba32(rgba32));
}

BL_API_IMPL BLResult bl_gradient_add_stop_rgba64(BLGradientCore* self, double offset, uint64_t rgba64) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(!gradient_is_valid_offset(offset))) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  size_t insert_index = 0;
  BLSpan<BLGradientStop> stops = get_stops_mut(self);

  if (!stops.is_empty() && offset >= stops[0].offset) {
    insert_index = binary_search_closest_last(stops.data(), stops.size(), GradientStopMatcher(offset));

    // If there are two stops that have the same offset then we would replace the second one. This is supported
    // and it would make a sharp transition.
    if (insert_index > 0 && stops[insert_index - 1].offset == offset) {
      return bl_gradient_replace_stop_rgba64(self, insert_index, offset, rgba64);
    }

    // Insert a new stop after `i`.
    insert_index++;
  }

  // If we are here it means that we are going to insert a stop at `insert_index`. All other cases were handled at
  // this point so focus on a generic insert.
  if (ObjectLayer::check_dynamic_capacity_and_mutability(self, 1u, stops.size())) {
    BLGradientStop* stops_ptr = stops.data();
    gradient_move_stops(stops_ptr + insert_index + 1u, stops_ptr + insert_index, stops.size() - insert_index);
    stops_ptr[insert_index] = BLGradientStop(offset, BLRgba64(rgba64));

    ObjectLayer::set_dyn_size(self, stops.size() + 1u);
    return gradient_invalidate_cache(ObjectLayer::get_impl(self));
  }
  else {
    if (BL_UNLIKELY(stops.size() == kMaxSize)) {
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
    }

    BLGradientCore tmp;
    ObjectImplSize impl_size = ObjectLayer::expand_impl_size_32(impl_size_from_capacity(stops.size() + 1u));
    BL_PROPAGATE(gradient_alloc_copy_impl(&tmp, impl_size, self, {stops.data(), insert_index}));

    BLGradientStop* stops_ptr = ObjectLayer::get_data<BLGradientStop>(&tmp);
    stops_ptr[insert_index] = BLGradientStop(offset, BLRgba64(rgba64));
    gradient_copy_stops(stops_ptr + insert_index + 1u, stops.data() + insert_index, stops.size() - insert_index);

    ObjectLayer::set_dyn_size(&tmp, stops.size() + 1u);
    return ObjectLayer::replace_object(self, &tmp);
  }
}

BL_API_IMPL BLResult bl_gradient_remove_stop(BLGradientCore* self, size_t index) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_gradient_remove_stops_by_index(self, index, index + 1u);
}


BL_API_IMPL BLResult bl_gradient_remove_stop_by_offset(BLGradientCore* self, double offset, BLGradientStopRemovePolicy policy) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(!gradient_is_valid_offset(offset))) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  BLSpan<const BLGradientStop> stops = get_stops(self);
  for (size_t a = 0; a < stops.size(); a++) {
    if (stops[a].offset > offset) {
      break;
    }

    if (stops[a].offset == offset) {
      size_t b = a + 1;
      if (policy > BL_GRADIENT_STOP_REMOVE_POLICY_FIRST) {
        if (b < stops.size() && stops[b].offset == offset) {
          b = a + 2;
          a += size_t(policy == BL_GRADIENT_STOP_REMOVE_POLICY_SECOND);
        }
        else {
          if (policy == BL_GRADIENT_STOP_REMOVE_POLICY_SECOND) {
            return bl_make_error(BL_ERROR_NO_ENTRY);
          }
        }
      }
      return bl_gradient_remove_stops_by_index(self, a, b);
    }
  }

  return bl_make_error(BL_ERROR_NO_ENTRY);
}

BL_API_IMPL BLResult bl_gradient_remove_stops_by_index(BLGradientCore* self, size_t r_start, size_t r_end) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<BLGradientStop> stops = get_stops_mut(self);
  size_t index = r_start;
  size_t end = bl_min(r_end, stops.size());

  if (BL_UNLIKELY(index >= stops.size() || end <= index)) {
    return bl_make_error(BL_ERROR_NO_ENTRY);
  }

  size_t removed_count = end - index;
  size_t shifted_count = stops.size() - end;
  size_t result_count = stops.size() - removed_count;

  GradientPrivateImpl* impl = ObjectLayer::get_impl(self);
  if (ObjectLayer::is_impl_mutable(impl)) {
    gradient_move_stops(stops.data() + index, stops.data() + end, shifted_count);
    ObjectLayer::set_dyn_size(self, result_count);
    return gradient_invalidate_cache(impl);
  }
  else {
    BLGradientCore tmp;
    ObjectImplSize impl_size = impl_size_from_capacity(result_count);
    BL_PROPAGATE(gradient_alloc_copy_impl(&tmp, impl_size, self, {stops.data(), index}));

    gradient_copy_stops(ObjectLayer::get_data<BLGradientStop>(&tmp) + index, stops.data() + end, shifted_count);
    ObjectLayer::set_dyn_size(&tmp, result_count);

    return ObjectLayer::replace_object(self, &tmp);
  }
}

BL_API_IMPL BLResult bl_gradient_remove_stops_by_offset(BLGradientCore* self, double offset_min, double offset_max) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(offset_max < offset_min)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  BLSpan<const BLGradientStop> stops = get_stops(self);

  size_t a = 0; while (a < stops.size() && stops[a].offset <  offset_min) a++;
  size_t b = a; while (b < stops.size() && stops[b].offset <= offset_max) b++;

  return bl_gradient_remove_stops_by_index(self, a, b);
}

BL_API_IMPL BLResult bl_gradient_replace_stop_rgba32(BLGradientCore* self, size_t index, double offset, uint32_t rgba32) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_gradient_replace_stop_rgba64(self, index, offset, bl::RgbaInternal::rgba64_from_rgba32(rgba32));
}

BL_API_IMPL BLResult bl_gradient_replace_stop_rgba64(BLGradientCore* self, size_t index, double offset, uint64_t rgba64) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(index >= ObjectLayer::dyn_size(self))) {
    return bl_make_error(BL_ERROR_NO_ENTRY);
  }

  BL_PROPAGATE(make_mutable(self, false));

  BLGradientStop* stops = ObjectLayer::get_data<BLGradientStop>(self);
  if (stops[index].offset == offset) {
    // Replacing a stop of a matching offset.
    stops[index].rgba.value = rgba64;
    return BL_SUCCESS;
  }
  else {
    // If the offset doesn't match the stop has to be removed and a new one inserted.
    BL_PROPAGATE(bl_gradient_remove_stop(self, index));
    return bl_gradient_add_stop_rgba64(self, offset, rgba64);
  }
}

BL_API_IMPL size_t bl_gradient_index_of_stop(const BLGradientCore* self, double offset) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLSpan<const BLGradientStop> stops = get_stops(self);
  size_t i = binary_search(stops.data(), stops.size(), GradientStopMatcher(offset));

  if (i >= stops.size()) {
    return SIZE_MAX;
  }

  // There can be up to 2 stops sharing the same offset to create sharp transitions, so return the first.
  if (i) {
    i -= size_t(stops[i - 1].offset == offset);
  }

  return i;
}

// Gradient - API - Transform
// ==========================

BL_API_IMPL BLResult bl_gradient_get_transform(const BLGradientCore* self, BLMatrix2D* transform_out) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  *transform_out = ObjectLayer::get_impl(self)->transform;
  return BL_SUCCESS;
}

BL_API_IMPL BLTransformType bl_gradient_get_transform_type(const BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_payload_field<BLGradient::TransformTypeField>(self);
}

BL_API_IMPL BLResult bl_gradient_set_transform(BLGradientCore* self, const BLMatrix2D* transform) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  GradientPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  if (!ObjectLayer::is_object_mutable(self)) {
    if (ObjectLayer::get_impl(self)->transform == *transform) {
      return BL_SUCCESS;
    }

    BL_PROPAGATE(deep_copy(self, self, true));
    self_impl = ObjectLayer::get_impl(self);
  }

  self_impl->transform = *transform;
  return ObjectLayer::set_payload_field<BLGradient::TransformTypeField>(self, transform->type());
}

BL_API_IMPL BLResult bl_gradient_reset_transform(BLGradientCore* self) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  GradientPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  if (!ObjectLayer::is_object_mutable(self)) {
    if (ObjectLayer::get_payload_field<BLGradient::TransformTypeField>(self) == BL_TRANSFORM_TYPE_IDENTITY) {
      return BL_SUCCESS;
    }

    BL_PROPAGATE(deep_copy(self, self, true));
    self_impl = ObjectLayer::get_impl(self);
  }

  self_impl->transform.reset();
  return ObjectLayer::set_payload_field<BLGradient::TransformTypeField>(self, BL_TRANSFORM_TYPE_IDENTITY);
}


BL_API_IMPL BLResult bl_gradient_apply_transform_op(BLGradientCore* self, BLTransformOp op_type, const void* op_data) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (op_type <= BL_TRANSFORM_OP_ASSIGN) {
    if (op_type == BL_TRANSFORM_OP_RESET) {
      return bl_gradient_reset_transform(self);
    }
    else {
      return bl_gradient_set_transform(self, static_cast<const BLMatrix2D*>(op_data));
    }
  }

  GradientPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  if (!ObjectLayer::is_object_mutable(self)) {
    BLMatrix2D transform(self_impl->transform);

    BL_PROPAGATE(bl_matrix2d_apply_op(&transform, op_type, op_data));
    BL_PROPAGATE(deep_copy(self, self, true));

    self_impl = ObjectLayer::get_impl(self);
    self_impl->transform = transform;
  }
  else {
    BL_PROPAGATE(bl_matrix2d_apply_op(&self_impl->transform, op_type, op_data));
  }

  return ObjectLayer::set_payload_field<BLGradient::TransformTypeField>(self, self_impl->transform.type());
}

// Gradient - API - Equals
// =======================

BL_API_IMPL bool bl_gradient_equals(const BLGradientCore* a, const BLGradientCore* b) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  const GradientPrivateImpl* a_impl = ObjectLayer::get_impl(a);
  const GradientPrivateImpl* b_impl = ObjectLayer::get_impl(b);

  size_t stop_count = ObjectLayer::get_size(a);

  if (!axl::bool_and(a->_d.pl0_byte() == b->_d.pl0_byte(),
                     a->_d.pl1_byte() == b->_d.pl1_byte(),
                     stop_count == ObjectLayer::get_size(b))) {
    return false;
  }

  if (a_impl == b_impl) {
    return true;
  }

  if (!axl::bool_and(a_impl->transform == b_impl->transform,
                     memcmp(a_impl->values, b_impl->values, sizeof(a_impl->values)) == 0)) {
    return false;
  }

  const BLGradientStop* a_stops = ObjectLayer::get_data<BLGradientStop>(a);
  const BLGradientStop* b_stops = ObjectLayer::get_data<BLGradientStop>(b);
  return memcmp(a_stops, b_stops, stop_count * sizeof(BLGradientStop)) == 0;
}

// Gradient - Runtime Registration
// ===============================

void bl_gradient_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::GradientDetail);
  static constexpr BLObjectType object_type = BLGradient::Info::kType;

  GradientPrivateImpl* impl = const_cast<GradientPrivateImpl*>(&null_gradient.impl);
  ObjectLayer::init_dyn(&bl_object_defaults[object_type], object_type, BLObjectDetail::kMetaFlagD, impl, nullptr);
}
