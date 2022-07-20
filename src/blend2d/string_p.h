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
#include "unicode_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLStringPrivate {

//! \name String - Private - Memory Management
//! \{

static BL_INLINE BLStringImpl* getImpl(const BLStringCore* self) noexcept {
  return static_cast<BLStringImpl*>(self->_d.impl);
}

static BL_INLINE BLResult freeImpl(BLStringImpl* impl, BLObjectInfo info) noexcept {
  return blObjectImplFreeInline(impl, info);
}

static BL_INLINE bool isMutable(const BLStringCore* self) noexcept {
  const size_t* refCountPtr = blObjectDummyRefCount;
  if (!self->_d.sso())
    refCountPtr = blObjectImplGetRefCountPtr(self->_d.impl);
  return *refCountPtr == 1;
}

static BL_INLINE BLResult releaseInstance(BLStringCore* self) noexcept {
  BLStringImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  if (blObjectImplDecRefAndTestIfRefCounted(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}

static BL_INLINE BLResult replaceInstance(BLStringCore* self, const BLStringCore* other) noexcept {
  BLStringImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;

  if (blObjectImplDecRefAndTestIfRefCounted(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
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

template<size_t kSize>
struct StaticStringData {
  size_t refCount;
  BLStringImpl impl;
  char data[kSize + 1];
};

#define BL_DEFINE_STATIC_STRING(name, content)                                   \
  static constexpr ::BLStringPrivate::StaticStringData<sizeof(content)> name = { \
    0,                                                                           \
    BLStringImpl(sizeof(content), sizeof(content)),                              \
    content                                                                      \
  };

template<size_t kSize>
static BL_INLINE void initStatic(BLStringCore* self, const StaticStringData<kSize>& data) noexcept {
  self->_d.initDynamic(BL_OBJECT_TYPE_STRING, BLObjectInfo{0}, (void*)&data.impl);
}

//! \}

} // {BLStringPrivate}

//! \}
//! \endcond

#endif // BLEND2D_STRING_P_H_INCLUDED
