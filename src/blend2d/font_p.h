// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONT_P_H_INCLUDED
#define BLEND2D_FONT_P_H_INCLUDED

#include "api-internal_p.h"
#include "array_p.h"
#include "bitset_p.h"
#include "font.h"
#include "matrix_p.h"
#include "object_p.h"
#include "support/scopedbuffer_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

static constexpr uint32_t BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE = 2048;

struct BLFontPrivateImpl : public BLFontImpl {};

static BL_INLINE void blFontMatrixMultiply(BLMatrix2D* dst, const BLFontMatrix* a, const BLMatrix2D* b) noexcept {
  dst->reset(a->m00 * b->m00 + a->m01 * b->m10,
             a->m00 * b->m01 + a->m01 * b->m11,
             a->m10 * b->m00 + a->m11 * b->m10,
             a->m10 * b->m01 + a->m11 * b->m11,
             b->m20,
             b->m21);
}

static BL_INLINE void blFontMatrixMultiply(BLMatrix2D* dst, const BLMatrix2D* a, const BLFontMatrix* b) noexcept {
  dst->reset(a->m00 * b->m00 + a->m01 * b->m10,
             a->m00 * b->m01 + a->m01 * b->m11,
             a->m10 * b->m00 + a->m11 * b->m10,
             a->m10 * b->m01 + a->m11 * b->m11,
             a->m20 * b->m00 + a->m21 * b->m10,
             a->m20 * b->m01 + a->m21 * b->m11);
}


static BL_INLINE void blFontImplCtor(BLFontPrivateImpl* impl) noexcept {
  impl->face._d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d;
  blCallCtor(impl->featureSettings.dcast());
  blCallCtor(impl->variationSettings.dcast());
}

namespace bl {
namespace FontInternal {

//! \name BLFont - Internals - Common Functionality (Impl)
//! \{

static BL_INLINE bool isImplMutable(const BLFontPrivateImpl* impl) noexcept {
  return ObjectInternal::isImplMutable(impl);
}

BL_HIDDEN BLResult freeImpl(BLFontPrivateImpl* impl) noexcept;

template<RCMode kRCMode>
static BL_INLINE BLResult releaseImpl(BLFontPrivateImpl* impl) noexcept {
  return ObjectInternal::derefImplAndTest<kRCMode>(impl) ? freeImpl(impl) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLFont - Internals - Common Functionality (Instance)
//! \{

static BL_INLINE BLFontPrivateImpl* getImpl(const BLFontCore* self) noexcept {
  return static_cast<BLFontPrivateImpl*>(self->_d.impl);
}

static BL_INLINE bool isInstanceMutable(const BLFontCore* self) noexcept {
  return ObjectInternal::isImplMutable(self->_d.impl);
}

static BL_INLINE BLResult retainInstance(const BLFontCore* self, size_t n = 1) noexcept {
  return ObjectInternal::retainInstance(self, n);
}

static BL_INLINE BLResult releaseInstance(BLFontCore* self) noexcept {
  return releaseImpl<RCMode::kMaybe>(getImpl(self));
}

static BL_INLINE BLResult replaceInstance(BLFontCore* self, const BLFontCore* other) noexcept {
  BLFontPrivateImpl* impl = getImpl(self);
  self->_d = other->_d;
  return releaseImpl<RCMode::kMaybe>(impl);
}

//! \}

} // {FontInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_FONT_P_H_INCLUDED
