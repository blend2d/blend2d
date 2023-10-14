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

//! \name BLGradient - Private Structs
//! \{

//! Lookup table that contains interpolated pixels of the gradient in either PRGB32 or PRGB64 format.
struct BLGradientLUT {
  //! \name Members
  //! \{

  //! Reference count.
  size_t refCount;
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

  BL_INLINE BLGradientLUT* retain() noexcept {
    blAtomicFetchAddRelaxed(&refCount);
    return this;
  }

  BL_INLINE bool decRefAndTest() noexcept {
    return blAtomicFetchSubStrong(&refCount) == 1;
  }

  BL_INLINE void release() noexcept {
    if (decRefAndTest())
      destroy(this);
  }

  //! \}
};

//! Additional information maintained by `BLGradientImpl` that is cached and is useful when deciding how to
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
      uint16_t _lutSize;
    };
  };

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG bool empty() const noexcept { return packed == 0; }
  BL_INLINE_NODEBUG void reset() noexcept { packed = 0; }

  BL_INLINE uint32_t lutSize(bool highQuality) const noexcept {
    if (highQuality)
      return blMin<uint32_t>(_lutSize * 2, 1024);
    else
      return _lutSize;
  }

  //! \}
};

//! \}

//! \name BLGradient - Private Impl
//! \{

//! Private implementation that extends \ref BLGradientImpl.
struct BLGradientPrivateImpl : public BLGradientImpl {
  union {
    //! Gradient LUT cache as an array.
    BLGradientLUT* lut[2];
    struct {
      //! Gradient LUT cache (32-bit).
      BLGradientLUT* lut32;
      //! Gradient LUT cache (64-bit).
      BLGradientLUT* lut64;
    };
  };
  //! Information regarding gradient stops.
  BLGradientInfo info32;
};

//! \}

namespace bl {
namespace GradientInternal {

//! \name BLGradient - Internals - Common Functionality (Container)
//! \{

static BL_INLINE_NODEBUG constexpr BLObjectImplSize implSizeFromCapacity(size_t n) noexcept {
  return BLObjectImplSize(sizeof(BLGradientPrivateImpl) + n * sizeof(BLGradientStop));
}

static BL_INLINE_NODEBUG constexpr size_t capacityFromImplSize(BLObjectImplSize implSize) noexcept {
  return (implSize.value() - sizeof(BLGradientPrivateImpl)) / sizeof(BLGradientStop);
}

//! \}

//! \name BLGradient - Internals - Common Functionality (Impl)
//! \{

static BL_INLINE bool isImplMutable(BLGradientImpl* impl) noexcept {
  return ObjectInternal::isImplMutable(impl);
}

BL_HIDDEN BLResult freeImpl(BLGradientPrivateImpl* impl) noexcept;

template<RCMode kRCMode>
static BL_INLINE BLResult releaseImpl(BLGradientPrivateImpl* impl) noexcept {
  return ObjectInternal::derefImplAndTest<kRCMode>(impl) ? freeImpl(impl) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLGradient - Internals - Common Functionality (Instance)
//! \{

static BL_INLINE_NODEBUG BLGradientPrivateImpl* getImpl(const BLGradientCore* self) noexcept {
  return static_cast<BLGradientPrivateImpl*>(self->_d.impl);
}

static BL_INLINE BLResult retainInstance(const BLGradientCore* self, size_t n = 1) noexcept {
  return ObjectInternal::retainInstance(self, n);
}

static BL_INLINE BLResult releaseInstance(BLGradientCore* self) noexcept {
  return releaseImpl<RCMode::kMaybe>(getImpl(self));
}

static BL_INLINE BLResult replaceInstance(BLGradientCore* self, const BLGradientCore* other) noexcept {
  BLGradientPrivateImpl* impl = getImpl(self);
  self->_d = other->_d;
  return releaseImpl<RCMode::kMaybe>(impl);
}

//! \}

//! \name BLGradient - Internals - Accessors
//! \{

static BL_INLINE_NODEBUG uint32_t packAbcp(BLGradientType type, BLExtendMode extendMode, BLTransformType transformType) noexcept {
  return BLObjectInfo::packAbcp(uint32_t(type), uint32_t(extendMode), uint32_t(transformType));
}

static BL_INLINE_NODEBUG BLGradientType getGradientType(const BLGradientCore* self) noexcept { return BLGradientType(self->_d.info.aField()); }
static BL_INLINE_NODEBUG BLExtendMode getExtendMode(const BLGradientCore* self) noexcept { return BLExtendMode(self->_d.info.bField()); }
static BL_INLINE_NODEBUG BLTransformType getTransformType(const BLGradientCore* self) noexcept { return BLTransformType(self->_d.info.cField()); }

static BL_INLINE_NODEBUG void setGradientType(BLGradientCore* self, BLGradientType type) noexcept { self->_d.info.setAField(uint32_t(type)); }
static BL_INLINE_NODEBUG void setExtendMode(BLGradientCore* self, BLExtendMode extendMode) noexcept { self->_d.info.setBField(uint32_t(extendMode)); }
static BL_INLINE_NODEBUG void setTransformType(BLGradientCore* self, BLTransformType transformType) noexcept { self->_d.info.setCField(uint32_t(transformType)); }

//! \}

//! \name BLGradient - Internals - LUT Cache
//! \{

BL_HIDDEN BLGradientInfo ensureInfo(BLGradientPrivateImpl* impl_) noexcept;
BL_HIDDEN BLGradientLUT* ensureLut32(BLGradientPrivateImpl* impl_, uint32_t lutSize) noexcept;
BL_HIDDEN BLGradientLUT* ensureLut64(BLGradientPrivateImpl* impl_, uint32_t lutSize) noexcept;

//! \}

} // {GradientInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_GRADIENT_P_H_INCLUDED
