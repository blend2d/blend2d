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

//! \name Pattern - Private Structs
//! \{

//! Private implementation that extends \ref BLPatternImpl.
struct BLPatternPrivateImpl : public BLPatternImpl {
  // Nothing at the moment.
};

//! \}

//! \name Pattern - Private API
//! \{

namespace BLPatternPrivate {

static BL_INLINE BLPatternPrivateImpl* getImpl(const BLPatternCore* self) noexcept {
  return static_cast<BLPatternPrivateImpl*>(self->_d.impl);
}

BL_HIDDEN BLResult freeImpl(BLPatternPrivateImpl* impl, BLObjectInfo info) noexcept;

static BL_INLINE BLExtendMode getExtendMode(const BLPatternCore* self) noexcept { return (BLExtendMode)self->_d.info.bField(); }
static BL_INLINE BLMatrix2DType getMatrixType(const BLPatternCore* self) noexcept { return (BLMatrix2DType)self->_d.info.cField(); }

static BL_INLINE void setExtendMode(BLObjectInfo& info, BLExtendMode extendMode) noexcept { info.setBField(uint32_t(extendMode)); }
static BL_INLINE void setMatrixType(BLObjectInfo& info, BLMatrix2DType matrixType) noexcept { info.setCField(uint32_t(matrixType)); }

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

} // {BLPatternPrivate}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_PATTERN_P_H_INCLUDED
