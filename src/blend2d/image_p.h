// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_IMAGE_P_H_INCLUDED
#define BLEND2D_IMAGE_P_H_INCLUDED

#include "api-internal_p.h"
#include "image.h"
#include "object_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name BLImage - Internals - Structs
//! \{

//! Private implementation that extends \ref BLImageImpl.
struct BLImagePrivateImpl : public BLImageImpl {
  //! Count of writers that write to this image.
  //!
  //! Writers don't increase the reference count of the image to keep it mutable. However, we must keep
  //! a counter that would tell the BLImage destructor that it's not the time if `writerCount > 0`.
  size_t writerCount;
};

//! \}

namespace bl {
namespace ImageInternal {

//! \name BLImage - Internals - Common Functionality (Impl)
//! \{

static BL_INLINE bool isImplMutable(const BLImageImpl* impl) noexcept {
  return ObjectInternal::isImplMutable(impl);
}

BL_HIDDEN BLResult freeImpl(BLImagePrivateImpl* impl) noexcept;

template<RCMode kRCMode>
static BL_INLINE BLResult releaseImpl(BLImageImpl* impl) noexcept {
  return ObjectInternal::derefImplAndTest<kRCMode>(impl) ? freeImpl(static_cast<BLImagePrivateImpl*>(impl)) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLImage - Internals - Common Functionality (Instance)
//! \{

static BL_INLINE_NODEBUG BLImagePrivateImpl* getImpl(const BLImageCore* self) noexcept {
  return static_cast<BLImagePrivateImpl*>(self->_d.impl);
}

static BL_INLINE BLResult retainInstance(const BLImageCore* self, size_t n = 1) noexcept {
  BL_ASSERT(self->_d.isImage());
  return ObjectInternal::retainInstance(self, n);
}

static BL_INLINE BLResult releaseInstance(BLImageCore* self) noexcept {
  BL_ASSERT(self->_d.isImage());
  return releaseImpl<RCMode::kMaybe>(getImpl(self));
}

static BL_INLINE BLResult replaceInstance(BLImageCore* self, const BLImageCore* other) noexcept {
  BLImagePrivateImpl* impl = getImpl(self);
  self->_d = other->_d;
  return releaseImpl<RCMode::kMaybe>(impl);
}

//! \}

} // {ImageInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_IMAGE_P_H_INCLUDED
