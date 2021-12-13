// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_ARRAY_P_H_INCLUDED
#define BLEND2D_ARRAY_P_H_INCLUDED

#include "api-internal_p.h"
#include "array.h"
#include "object_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLArrayPrivate {

//! \name Array - Internals - Memory Management
//! \{

BL_HIDDEN BLResult freeImpl(BLArrayImpl* impl, BLObjectInfo info) noexcept;

static BL_INLINE BLArrayImpl* getImpl(const BLArrayCore* self) noexcept {
  return static_cast<BLArrayImpl*>(self->_d.impl);
}

static BL_INLINE bool isMutable(const BLArrayCore* self) noexcept {
  const size_t* refCountPtr = blObjectDummyRefCount;
  if (!self->_d.sso())
    refCountPtr = blObjectImplGetRefCountPtr(self->_d.impl);
  return *refCountPtr == 1;
}

static BL_INLINE BLResult releaseInstance(BLArrayCore* self) noexcept {
  BLArrayImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}

static BL_INLINE BLResult replaceInstance(BLArrayCore* self, const BLArrayCore* other) noexcept {
  BLArrayImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}

//! \}

//! \name Array - Internals - Accessors
//! \{

struct UnpackedData {
  uint8_t* data;
  size_t size;
  size_t capacity;
};

static BL_INLINE UnpackedData unpack(const BLArrayCore* self) noexcept {
  if (self->_d.sso()) {
    return UnpackedData { const_cast<uint8_t*>(self->_d.u8_data), self->_d.aField(), self->_d.bField() };
  }
  else {
    BLArrayImpl* impl = getImpl(self);
    return UnpackedData { impl->dataAs<uint8_t>(), impl->size, impl->capacity };
  }
}

template<typename T = void>
static BL_INLINE T* getData(const BLArrayCore* self) noexcept {
  if (self->_d.sso())
    return (T*)(self->_d.char_data);
  else
    return getImpl(self)->dataAs<T>();
}

static BL_INLINE size_t getSize(const BLArrayCore* self) noexcept {
  if (self->_d.sso())
    return size_t(self->_d.aField());
  else
    return getImpl(self)->size;
}

static BL_INLINE size_t getCapacity(const BLArrayCore* self) noexcept {
  if (self->_d.sso())
    return size_t(self->_d.bField());
  else
    return getImpl(self)->capacity;
}

static BL_INLINE void setSize(BLArrayCore* self, size_t newSize) noexcept {
  BL_ASSERT(newSize <= getCapacity(self));
  if (self->_d.sso())
    self->_d.info.setAField(uint32_t(newSize));
  else
    getImpl(self)->size = newSize;
}

//! \}

} // {BLArrayPrivate}

//! \}
//! \endcond

#endif // BLEND2D_ARRAY_P_H_INCLUDED
