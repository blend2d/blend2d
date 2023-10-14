// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_STRING_P_H_INCLUDED
#define BLEND2D_STRING_P_H_INCLUDED

#include "api-internal_p.h"
#include "array_p.h"
#include "object_p.h"
#include "string.h"
#include "unicode/unicode_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {
namespace StringInternal {

//! \name BLString - Internals - Common Functionality (Container)
//! \{

static BL_INLINE constexpr BLObjectImplSize implSizeFromCapacity(size_t capacity) noexcept {
  return BLObjectImplSize(sizeof(BLStringImpl) + 1 + capacity);
}

static BL_INLINE constexpr size_t capacityFromImplSize(BLObjectImplSize implSize) noexcept {
  return implSize.value() - sizeof(BLStringImpl) - 1;
}

//! \}

//! \name BLString - Internals - Common Functionality (Impl)
//! \{

static BL_INLINE bool isImplMutable(BLStringImpl* impl) noexcept {
  return ObjectInternal::isImplMutable(impl);
}


static BL_INLINE BLResult freeImpl(BLStringImpl* impl) noexcept {
  return ObjectInternal::freeImpl(impl);
}

template<RCMode kRCMode>
static BL_INLINE BLResult releaseImpl(BLStringImpl* impl) noexcept {
  return ObjectInternal::derefImplAndTest<kRCMode>(impl) ? freeImpl(impl) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLString - Internals - Common Functionality (Instance)
//! \{

static BL_INLINE BLStringImpl* getImpl(const BLStringCore* self) noexcept {
  return static_cast<BLStringImpl*>(self->_d.impl);
}

static BL_INLINE bool isInstanceMutable(const BLStringCore* self) noexcept {
  return ObjectInternal::isInstanceMutable(self);
}

static BL_INLINE BLResult retainInstance(const BLStringCore* self, size_t n = 1) noexcept {
  return ObjectInternal::retainInstance(self, n);
}

static BL_INLINE BLResult releaseInstance(BLStringCore* self) noexcept {
  return self->_d.isRefCountedObject() ? releaseImpl<RCMode::kForce>(getImpl(self)) : BLResult(BL_SUCCESS);
}

static BL_INLINE BLResult replaceInstance(BLStringCore* self, const BLStringCore* other) noexcept {
  // NOTE: UBSAN doesn't like casting the impl in case the String is in SSO mode, so wait with the cast.
  void* impl = static_cast<void*>(self->_d.impl);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;
  return info.isRefCountedObject() ? releaseImpl<RCMode::kForce>(static_cast<BLStringImpl*>(impl)) : BLResult(BL_SUCCESS);
}

//! \}

//! \name String - Private - Accessors
//! \{

struct UnpackedData {
  char* data;
  size_t size;
  size_t capacity;
};

static BL_INLINE size_t getSSOSize(const BLStringCore* self) noexcept {
  return (self->_d.info.bits ^ BLString::kSSOEmptySignature) >> BL_OBJECT_INFO_A_SHIFT;
}

static BL_INLINE UnpackedData unpackData(const BLStringCore* self) noexcept {
  if (self->_d.sso()) {
    return UnpackedData{const_cast<char*>(self->_d.char_data), getSSOSize(self), BLString::kSSOCapacity};
  }
  else {
    BLStringImpl* impl = getImpl(self);
    return UnpackedData{impl->data(), impl->size, impl->capacity};
  }
}

static BL_INLINE char* getData(const BLStringCore* self) noexcept {
  return self->_d.sso() ? const_cast<char*>(self->_d.char_data) : getImpl(self)->data();
}

static BL_INLINE size_t getSize(const BLStringCore* self) noexcept {
  return self->_d.sso() ? getSSOSize(self) : getImpl(self)->size;
}

static BL_INLINE size_t getCapacity(const BLStringCore* self) noexcept {
  return self->_d.sso() ? size_t(BLString::kSSOCapacity) : getImpl(self)->capacity;
}

//! \}

//! \name String - Private - Static String
//! \{

struct BL_MAY_ALIAS StaticStringImpl {
  size_t size;
  size_t capacity;
};

template<size_t kSize>
struct StaticStringData {
  BLObjectEternalHeader header;
  StaticStringImpl impl;
  char data[kSize + 1];
};

#define BL_DEFINE_STATIC_STRING(name, content)                                                 \
  static const constexpr ::bl::StringInternal::StaticStringData<sizeof(content) - 1u> name = { \
    {},                                                                                        \
    {sizeof(content) - 1u, sizeof(content) - 1u},                                              \
    content                                                                                    \
  };

template<size_t kSize>
static BL_INLINE void initStatic(BLStringCore* self, const StaticStringData<kSize>& data) noexcept {
  self->_d.initDynamic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_STRING), (BLObjectImpl*)&data.impl);
}

//! \}

} // {StringInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_STRING_P_H_INCLUDED
