// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_GRADIENT_P_H_INCLUDED
#define BLEND2D_CORE_GRADIENT_P_H_INCLUDED

#include <blend2d/axl/mem_alloc.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/gradient.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! Lookup table that contains interpolated pixels of the gradient in either PRGB32 or PRGB64 format.
struct GradientLUT {

  static constexpr size_t index_32 = 0;
  static constexpr size_t index_64 = 1;

  //! \name Members
  //! \{

  //! Reference count.
  size_t ref_count;
  //! Table size - must be a power of 2!
  size_t size;

  //! \}

  //! \name Accessors
  //! \{

  template<typename T = void>
  BL_INLINE_NODEBUG T* data() noexcept { return reinterpret_cast<T*>(this + 1); }

  template<typename T = void>
  const BL_INLINE_NODEBUG T* data() const noexcept { return reinterpret_cast<const T*>(this + 1); }

  //! \}

  //! \name Alloc & Destroy
  //! \{

  static BL_INLINE GradientLUT* alloc(size_t size, uint32_t pixel_size) noexcept {
    GradientLUT* self = axl::mem_alloc<GradientLUT>(sizeof(GradientLUT) + size * pixel_size);
    if (BL_UNLIKELY(!self)) {
      return nullptr;
    }

    self->ref_count = 1;
    self->size = size;

    return self;
  }

  static BL_INLINE void destroy(GradientLUT* self) noexcept {
    axl::mem_free(self);
  }

  //! \}

  //! \name Reference Counting
  //! \{

  BL_INLINE GradientLUT* retain() noexcept {
    bl::Atomic::fetch_add_rc(ref_count);
    return this;
  }

  BL_INLINE bool dec_ref_and_test() noexcept {
    return bl::Atomic::fetch_sub_rc(ref_count) == 1;
  }

  BL_INLINE void release() noexcept {
    if (dec_ref_and_test()) {
      destroy(this);
    }
  }

  //! \}
};

//! Additional information maintained by `BLGradientImpl` that is cached and is useful when deciding how to
//! render the gradient and how big the LUT should be.
struct GradientInfo {
  //! \name Members
  //! \{

  union {
    uint32_t packed;

    struct {
      //! True if the gradient is a solid color.
      uint8_t solid;
      //! Gradient format (either 32-bit or 64-bit).
      uint8_t format;
      //! Optimal GradientLUT size.
      uint16_t _lut_size;
    };
  };

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG bool is_empty() const noexcept { return packed == 0; }
  BL_INLINE_NODEBUG void reset() noexcept { packed = 0; }

  BL_INLINE uint32_t lut_size(bool high_quality) const noexcept {
    return high_quality ? bl_min<uint32_t>(_lut_size * 2, 1024) : _lut_size;
  }

  //! \}
};

//! Private implementation that extends \ref BLGradientImpl.
struct GradientPrivateImpl : public BLGradientImpl {
  //! Gradient LUT cache as an array.
  GradientLUT* lut[2];
  //! Information regarding gradient stops.
  GradientInfo info32;

  [[nodiscard]] BL_INLINE_CONSTEXPR GradientLUT* lut32() noexcept { return lut[GradientLUT::index_32]; }
  [[nodiscard]] BL_INLINE_CONSTEXPR GradientLUT* lut64() noexcept { return lut[GradientLUT::index_64]; }

  BL_INLINE_CONSTEXPR void set_lut32(GradientLUT* lut32) noexcept { lut[GradientLUT::index_32] = lut32; }
  BL_INLINE_CONSTEXPR void set_lut64(GradientLUT* lut64) noexcept { lut[GradientLUT::index_64] = lut64; }
};

} // {bl}

namespace bl::GradientDetail {

BLResult free_object(BLGradientCore* self, GradientPrivateImpl* impl) noexcept;

GradientInfo ensure_info(const BLGradientCore* self) noexcept;
GradientLUT* ensure_lut32(const BLGradientCore* self, GradientPrivateImpl* impl, uint32_t lut_size) noexcept;
GradientLUT* ensure_lut64(const BLGradientCore* self, GradientPrivateImpl* impl, uint32_t lut_size) noexcept;

static BL_INLINE_CONSTEXPR ObjectImplSize impl_size_from_capacity(size_t n) noexcept {
  return ObjectImplSize(sizeof(GradientPrivateImpl) + n * sizeof(BLGradientStop));
}

static BL_INLINE_CONSTEXPR size_t capacity_from_impl_size(ObjectImplSize impl_size) noexcept {
  return (size_t(impl_size) - sizeof(GradientPrivateImpl)) / sizeof(BLGradientStop);
}

static BL_INLINE_NODEBUG BLSpan<const BLGradientStop> get_stops(const BLGradientCore* self) noexcept {
  return {ObjectLayer::get_data<BLGradientStop>(self), ObjectLayer::get_size(self)};
}

static BL_INLINE_NODEBUG BLSpan<BLGradientStop> get_stops_mut(BLGradientCore* self) noexcept {
  return {ObjectLayer::get_data<BLGradientStop>(self), ObjectLayer::get_size(self)};
}

static constexpr size_t kMaxSize = capacity_from_impl_size(ObjectImplSize{ObjectLayer::kMaxImplSize32});

} // {bl::GradientDetail}

namespace bl {

template<>
struct ObjectDispatchInfo<BLGradientCore> {
  using ImplType = GradientPrivateImpl;

  static constexpr bool kHasInitSSO = false;
  static constexpr size_t kImplDataOffset = sizeof(GradientPrivateImpl);

  static BL_INLINE BLResult free_object(BLGradientCore* self, ImplType* impl) noexcept {
    return GradientDetail::free_object(self, impl);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_GRADIENT_P_H_INCLUDED
