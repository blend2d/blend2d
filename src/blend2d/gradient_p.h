// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_GRADIENT_P_H_INCLUDED
#define BLEND2D_GRADIENT_P_H_INCLUDED

#include "./api-internal_p.h"
#include "./gradient.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLGradient - LUT]
// ============================================================================

//! Lookup table that contains interpolated pixels of the gradient in either
//! PRGB32 or PRGB64 format (no other format is ever used).
struct BLGradientLUT {
  //! Reference count.
  volatile size_t refCount;
  //! Table size - must be power of 2!
  size_t size;

  template<typename T = void>
  BL_INLINE T* data() noexcept { return reinterpret_cast<T*>(this + 1); }

  template<typename T = void>
  const BL_INLINE T* data() const noexcept { return reinterpret_cast<const T*>(this + 1); }

  BL_INLINE BLGradientLUT* incRef() noexcept {
    blAtomicFetchAdd(&refCount);
    return this;
  }

  BL_INLINE bool decRefAndTest() noexcept {
    return blAtomicFetchSub(&refCount) == 1;
  }

  BL_INLINE void release() noexcept {
    if (decRefAndTest())
      destroy(this);
  }

  static BL_INLINE BLGradientLUT* alloc(size_t size, uint32_t pixelSize) noexcept {
    BLGradientLUT* self = static_cast<BLGradientLUT*>(
      malloc(sizeof(BLGradientLUT) + size * pixelSize));

    if (BL_UNLIKELY(!self))
      return nullptr;

    self->refCount = 1;
    self->size = size;

    return self;
  }

  static BL_INLINE void destroy(BLGradientLUT* self) noexcept {
    free(self);
  }
};

// ============================================================================
// [BLGradient - Info]
// ============================================================================

//! Additional information maintained by `BLGradient` that is cached and is
//! useful when deciding how to render the gradient and how big the LUT should
//! be.
struct BLGradientInfo {
  union {
    uint32_t packed;

    struct {
      //! True if the gradient is a solid color.
      uint8_t solid;
      //! Gradient format (either 32-bit or 64-bit).
      uint8_t format;
      //! Optimal LUT size.
      uint16_t lutSize;
    };
  };

  BL_INLINE bool empty() const noexcept { return this->packed == 0; }
  BL_INLINE void reset() noexcept { this->packed = 0; }
};

// ============================================================================
// [BLGradient - Internal]
// ============================================================================

//! Internal implementation that extends `BLGradientImpl` and adds LUT cache to it.
struct BLInternalGradientImpl : public BLGradientImpl {
  //! Gradient lookup table (32-bit).
  BLGradientLUT* volatile lut32;
  volatile BLGradientInfo info32;
};

template<>
struct BLInternalCastImpl<BLGradientImpl> { typedef BLInternalGradientImpl Type; };

BL_HIDDEN BLResult blGradientImplDelete(BLGradientImpl* impl) noexcept;

BL_HIDDEN BLGradientInfo blGradientImplEnsureInfo32(BLGradientImpl* impl_) noexcept;
BL_HIDDEN BLGradientLUT* blGradientImplEnsureLut32(BLGradientImpl* impl_) noexcept;

// ============================================================================
// [BLGradient - Ops]
// ============================================================================

struct BLGradientOps {
  void (BL_CDECL* interpolate32)(uint32_t* dst, uint32_t dstSize, const BLGradientStop* stops, size_t stopCount) BL_NOEXCEPT;
};
BL_HIDDEN extern BLGradientOps blGradientOps;

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN void BL_CDECL blGradientInterpolate32_SSE2(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept;
#endif

#ifdef BL_BUILD_OPT_AVX2
BL_HIDDEN void BL_CDECL blGradientInterpolate32_AVX2(uint32_t* dPtr, uint32_t dWidth, const BLGradientStop* sPtr, size_t sSize) noexcept;
#endif

//! \}
//! \endcond

#endif // BLEND2D_GRADIENT_P_H_INCLUDED
