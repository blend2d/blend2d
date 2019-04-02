// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLGRADIENT_P_H
#define BLEND2D_BLGRADIENT_P_H

#include "./blapi-internal_p.h"
#include "./blgradient.h"

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
    blAtomicFetchIncRef(&refCount);
    return this;
  }

  BL_INLINE bool decRefAndTest() noexcept {
    return blAtomicFetchDecRef(&refCount) == 1;
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

#endif // BLEND2D_BLGRADIENT_P_H
