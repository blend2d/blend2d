// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTVARIATIONSETTINGS_P_H_INCLUDED
#define BLEND2D_FONTVARIATIONSETTINGS_P_H_INCLUDED

#include "api-internal_p.h"
#include "fontvariationsettings.h"
#include "object_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {
namespace FontVariationSettingsInternal {

//! \name BLFontVariationSettings - Internals - Common Functionality (Container)
//! \{

static BL_INLINE_NODEBUG constexpr BLObjectImplSize implSizeFromCapacity(size_t capacity) noexcept {
  return BLObjectImplSize(sizeof(BLFontVariationSettingsImpl) + capacity * sizeof(BLFontVariationItem));
}

static BL_INLINE_NODEBUG constexpr size_t capacityFromImplSize(BLObjectImplSize implSize) noexcept {
  return (implSize.value() - sizeof(BLFontVariationSettingsImpl)) / sizeof(BLFontVariationItem);
}

//! \}

//! \name BLFontVariationSettings - Internals - Common Functionality (Impl)
//! \{

static BL_INLINE bool isImplMutable(BLFontVariationSettingsImpl* impl) noexcept {
  return ObjectInternal::isImplMutable(impl);
}

static BL_INLINE BLResult freeImpl(BLFontVariationSettingsImpl* impl) noexcept {
  return ObjectInternal::freeImpl(impl);
}

template<RCMode kRCMode>
static BL_INLINE void retainImpl(BLFontVariationSettingsImpl* impl, size_t n = 1) noexcept {
  ObjectInternal::retainImpl<kRCMode>(impl, n);
}

template<RCMode kRCMode>
static BL_INLINE BLResult releaseImpl(BLFontVariationSettingsImpl* impl) noexcept {
  return ObjectInternal::derefImplAndTest<kRCMode>(impl) ? freeImpl(impl) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLFontVariationSettings - Internals - Common Functionality (Instance)
//! \{

static BL_INLINE_NODEBUG BLFontVariationSettingsImpl* getImpl(const BLFontVariationSettingsCore* self) noexcept {
  return static_cast<BLFontVariationSettingsImpl*>(self->_d.impl);
}

static BL_INLINE BLResult retainInstance(const BLFontVariationSettingsCore* self, size_t n = 1) noexcept {
  return ObjectInternal::retainInstance(self, n);
}

static BL_INLINE BLResult releaseInstance(BLFontVariationSettingsCore* self) noexcept {
  return self->_d.info.isRefCountedObject() ? releaseImpl<RCMode::kForce>(getImpl(self)) : BLResult(BL_SUCCESS);
}

static BL_INLINE BLResult replaceInstance(BLFontVariationSettingsCore* self, const BLFontVariationSettingsCore* other) noexcept {
  BLFontVariationSettingsImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;
  return info.isRefCountedObject() ? releaseImpl<RCMode::kForce>(impl) : BLResult(BL_SUCCESS);
}

//! \}

} // {FontVariationSettingsInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_FONTVARIATIONSETTINGS_P_H_INCLUDED
