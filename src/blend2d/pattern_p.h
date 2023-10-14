// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATTERN_P_H_INCLUDED
#define BLEND2D_PATTERN_P_H_INCLUDED

#include "api-internal_p.h"
#include "object_p.h"
#include "pattern.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name BLPattern - Private Structs
//! \{

//! Private implementation that extends \ref BLPatternImpl.
struct BLPatternPrivateImpl : public BLPatternImpl {
  // Nothing at the moment.
};

//! \}

//! \name BLPattern - Private API
//! \{

namespace bl {
namespace PatternInternal {

//! \name BLPattern - Internals - Common Functionality (Impl)
//! \{

static BL_INLINE bool isImplMutable(BLPatternImpl* impl) noexcept {
  return ObjectInternal::isImplMutable(impl);
}

BL_HIDDEN BLResult freeImpl(BLPatternPrivateImpl* impl) noexcept;

template<RCMode kRCMode>
static BL_INLINE BLResult releaseImpl(BLPatternPrivateImpl* impl) noexcept {
  return ObjectInternal::derefImplAndTest<kRCMode>(impl) ? freeImpl(impl) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLPattern - Internals - Common Functionality (Instance)
//! \{

static BL_INLINE BLPatternPrivateImpl* getImpl(const BLPatternCore* self) noexcept {
  return static_cast<BLPatternPrivateImpl*>(self->_d.impl);
}

static BL_INLINE bool isInstanceMutable(const BLPatternCore* self) noexcept {
  return isImplMutable(getImpl(self));
}

static BL_INLINE BLResult retainInstance(const BLPatternCore* self, size_t n = 1) noexcept {
  return ObjectInternal::retainInstance(self, n);
}

static BL_INLINE BLResult releaseInstance(BLPatternCore* self) noexcept {
  return releaseImpl<RCMode::kMaybe>(getImpl(self));
}

static BL_INLINE BLResult replaceInstance(BLPatternCore* self, const BLPatternCore* other) noexcept {
  BLPatternPrivateImpl* impl = getImpl(self);
  self->_d = other->_d;
  return releaseImpl<RCMode::kMaybe>(impl);
}

//! \}

//! \name BLPattern - Internals - Accessors
//! \{

static BL_INLINE BLExtendMode getExtendMode(const BLPatternCore* self) noexcept { return (BLExtendMode)self->_d.info.bField(); }
static BL_INLINE BLTransformType getTransformType(const BLPatternCore* self) noexcept { return (BLTransformType)self->_d.info.cField(); }

static BL_INLINE void setExtendMode(BLPatternCore* self, BLExtendMode extendMode) noexcept { self->_d.info.setBField(uint32_t(extendMode)); }
static BL_INLINE void setTransformType(BLPatternCore* self, BLTransformType transformType) noexcept { self->_d.info.setCField(uint32_t(transformType)); }

static BL_INLINE bool isAreaValid(const BLRectI& area, const BLSizeI& size) noexcept {
  typedef unsigned U;
  return bool((U(area.x) < U(size.w)) &
              (U(area.y) < U(size.h)) &
              (U(area.w) <= U(size.w) - U(area.x)) &
              (U(area.h) <= U(size.h) - U(area.y)));
}

static BL_INLINE bool isAreaValidAndNonZero(const BLRectI& area, const BLSizeI& size) noexcept {
  typedef unsigned U;
  return bool((U(area.x) < U(size.w)) &
              (U(area.y) < U(size.h)) &
              (U(area.w) - U(1) < U(size.w) - U(area.x)) &
              (U(area.h) - U(1) < U(size.h) - U(area.y)));
}

//! \}

} // {PatternInternal}
} // {bl}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_PATTERN_P_H_INCLUDED
