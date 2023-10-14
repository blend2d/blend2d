// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_BITARRAY_P_H
#define BLEND2D_BITARRAY_P_H

#include "api-internal_p.h"
#include "bitarray.h"
#include "object_p.h"
#include "support/algorithm_p.h"
#include "support/bitops_p.h"
#include "support/intops_p.h"
#include "support/memops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! \name BLBitArray - Types
//! \{

typedef ParametrizedBitOps<BitOrder::kMSB, uint32_t> BitArrayOps;

//! \}

namespace BitArrayInternal {

//! \name BLBitArray - Internals - Common Functionality (Impl)
//! \{

static BL_INLINE bool isImplMutable(BLBitArrayImpl* impl) noexcept {
  return ObjectInternal::isImplMutable(impl);
}

static BL_INLINE_NODEBUG BLResult freeImpl(BLBitArrayImpl* impl) noexcept {
  return ObjectInternal::freeImpl(impl);
}

template<RCMode kRCMode>
static BL_INLINE BLResult releaseImpl(BLBitArrayImpl* impl) noexcept {
  return ObjectInternal::derefImplAndTest<kRCMode>(impl) ? freeImpl(impl) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLBitArray - Internals - Common Functionality (Instance)
//! \{

static BL_INLINE_NODEBUG BLBitArrayImpl* getImpl(const BLBitArrayCore* self) noexcept {
  return static_cast<BLBitArrayImpl*>(self->_d.impl);
}

static BL_INLINE BLResult retainInstance(const BLBitArrayCore* self, size_t n = 1) noexcept {
  return ObjectInternal::retainInstance(self, n);
}

static BL_INLINE BLResult releaseInstance(BLBitArrayCore* self) noexcept {
  return self->_d.isRefCountedObject() ? releaseImpl<RCMode::kForce>(getImpl(self)) : BLResult(BL_SUCCESS);
}

static BL_INLINE BLResult replaceInstance(BLBitArrayCore* self, const BLBitArrayCore* other) noexcept {
  // NOTE: UBSAN doesn't like casting the impl in case the BitArray is in SSO mode, so wait with the cast.
  void* impl = static_cast<void*>(self->_d.impl);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;
  return info.isRefCountedObject() ? releaseImpl<RCMode::kForce>(static_cast<BLBitArrayImpl*>(impl)) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLBitArray - Internals - Accessors
//! \{

struct BitData {
  uint32_t* data;
  size_t size;
};

static BL_INLINE_NODEBUG size_t getSSOSize(const BLBitArrayCore* self) noexcept { return self->_d.pField(); }
static BL_INLINE_NODEBUG uint32_t* getSSOData(BLBitArrayCore* self) noexcept { return self->_d.u32_data; }
static BL_INLINE_NODEBUG const uint32_t* getSSOData(const BLBitArrayCore* self) noexcept { return self->_d.u32_data; }

static BL_INLINE BitData unpack(const BLBitArrayCore* self) noexcept {
  if (self->_d.sso()) {
    return BitData{const_cast<uint32_t*>(self->_d.u32_data), self->_d.pField()};
  }
  else {
    BLBitArrayImpl* impl = getImpl(self);
    return BitData{impl->data(), impl->size};
  }
}

static BL_INLINE_NODEBUG uint32_t* getData(BLBitArrayCore* self) noexcept {
  return self->_d.sso() ? self->_d.u32_data : getImpl(self)->data();
}

static BL_INLINE_NODEBUG const uint32_t* getData(const BLBitArrayCore* self) noexcept {
  return self->_d.sso() ? self->_d.u32_data : getImpl(self)->data();
}

static BL_INLINE_NODEBUG size_t getSize(const BLBitArrayCore* self) noexcept {
  return self->_d.sso() ? self->_d.pField() : getImpl(self)->size;
}

static BL_INLINE_NODEBUG size_t getCapacity(const BLBitArrayCore* self) noexcept {
  return self->_d.sso() ? size_t(BLBitArray::kSSOWordCount * 32u) : getImpl(self)->capacity;
}

static BL_INLINE void setSize(BLBitArrayCore* self, size_t newSize) noexcept {
  BL_ASSERT(newSize <= getCapacity(self));
  if (self->_d.sso())
    self->_d.info.setPField(uint32_t(newSize));
  else
    getImpl(self)->size = uint32_t(newSize);
}

//! \}

} // {BitArrayInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_BITARRAY_P_H
