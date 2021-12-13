// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GRADIENT_P_H_INCLUDED
#define BLEND2D_GRADIENT_P_H_INCLUDED

#include "api-internal_p.h"
#include "gradient.h"
#include "object_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name Gradient - Private Structs
//! \{

//! Lookup table that contains interpolated pixels of the gradient in either PRGB32 or PRGB64 format.
struct BLGradientLUT {
  //! \name Members
  //! \{

  //! Reference count.
  volatile size_t refCount;
  //! Table size - must be a power of 2!
  size_t size;

  //! \}

  //! \name Accessors
  //! \{

  template<typename T = void>
  BL_INLINE T* data() noexcept { return reinterpret_cast<T*>(this + 1); }

  template<typename T = void>
  const BL_INLINE T* data() const noexcept { return reinterpret_cast<const T*>(this + 1); }

  //! \}

  //! \name Alloc & Destroy
  //! \{

  static BL_INLINE BLGradientLUT* alloc(size_t size, uint32_t pixelSize) noexcept {
    BLGradientLUT* self = static_cast<BLGradientLUT*>(malloc(sizeof(BLGradientLUT) + size * pixelSize));

    if (BL_UNLIKELY(!self))
      return nullptr;

    self->refCount = 1;
    self->size = size;

    return self;
  }

  static BL_INLINE void destroy(BLGradientLUT* self) noexcept {
    free(self);
  }

  //! \}

  //! \name Reference Counting
  //! \{

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

  //! \}
};

//! Additional information maintained by `BLGradient` that is cached and is useful when deciding how to
//! render the gradient and how big the LUT should be.
struct BLGradientInfo {
  //! \name Members
  //! \{

  union {
    uint32_t packed;

    struct {
      //! True if the gradient is a solid color.
      uint8_t solid;
      //! Gradient format (either 32-bit or 64-bit).
      uint8_t format;
      //! Optimal BLGradientLUT size.
      uint16_t lutSize;
    };
  };

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE bool empty() const noexcept { return this->packed == 0; }
  BL_INLINE void reset() noexcept { this->packed = 0; }

  //! \}
};

//! \}

//! \name Gradient - Private Impl
//! \{

//! Private implementation that extends \ref BLGradientImpl.
struct BLGradientPrivateImpl : public BLGradientImpl {
  //! Gradient LUT cache (32-bit).
  BLGradientLUT* volatile lut32;
  //! Information regarding gradient stops.
  volatile BLGradientInfo info32;
};

//! \}

//! \name Gradient - Private API
//! \{

namespace BLGradientPrivate {

static BL_INLINE BLGradientPrivateImpl* getImpl(const BLGradientCore* self) noexcept {
  return static_cast<BLGradientPrivateImpl*>(self->_d.impl);
}

BL_HIDDEN BLResult freeImpl(BLGradientPrivateImpl* impl, BLObjectInfo info) noexcept;

static BL_INLINE BLResult releaseInstance(BLGradientCore* self) noexcept {
  BLGradientPrivateImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}

static BL_INLINE BLResult replaceInstance(BLGradientCore* self, const BLGradientCore* other) noexcept {
  BLGradientPrivateImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}


BL_HIDDEN BLGradientInfo ensureInfo32(BLGradientPrivateImpl* impl_) noexcept;
BL_HIDDEN BLGradientLUT* ensureLut32(BLGradientPrivateImpl* impl_) noexcept;

} // {BLGradientPrivate}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_GRADIENT_P_H_INCLUDED
