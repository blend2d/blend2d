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

//! \name Image - Private Structs
//! \{

//! Private implementation that extends \ref BLImageImpl.
struct BLImagePrivateImpl : public BLImageImpl {
  //! Count of writers that write to this image.
  //!
  //! Writers don't increase the reference count of the image to keep it mutable. However, we must keep a counter
  //! that would tell the BLImage destructor that it's not the time if `writerCount > 0`.
  volatile size_t writerCount;
};

//! \}

//! \name Image - Private API
//! \{

namespace BLImagePrivate {

BL_HIDDEN BLResult freeImpl(BLImagePrivateImpl* impl, BLObjectInfo info) noexcept;

static BL_INLINE BLImagePrivateImpl* getImpl(const BLImageCore* self) noexcept {
  return static_cast<BLImagePrivateImpl*>(self->_d.impl);
}

static BL_INLINE bool isMutable(const BLImageCore* self) noexcept {
  const size_t* refCountPtr = blObjectImplGetRefCountPtr(self->_d.impl);
  return *refCountPtr == 1;
}

static BL_INLINE BLResult releaseInstance(BLImageCore* self) noexcept {
  BLImagePrivateImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}

static BL_INLINE BLResult replaceInstance(BLImageCore* self, const BLImageCore* other) noexcept {
  BLImagePrivateImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}

} // {BLImagePrivate}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_IMAGE_P_H_INCLUDED
