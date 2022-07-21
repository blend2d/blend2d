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

//! \name Font - Uncategoried Internals
//! \{

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

//! \}

//! \name BLFont - Internal Memory Management
//! \{

struct BLInternalFontImpl : public BLFontImpl {};

static BL_INLINE BLInternalFontImpl* blFontGetImpl(const BLFontCore* self) noexcept {
  return static_cast<BLInternalFontImpl*>(self->_d.impl);
}

BL_HIDDEN BLResult blFontImplFree(BLInternalFontImpl* impl, BLObjectInfo info) noexcept;

static BL_INLINE bool blFontPrivateIsMutable(const BLFontCore* self) noexcept {
  const size_t* refCountPtr = blObjectImplGetRefCountPtr(self->_d.impl);
  return *refCountPtr == 1;
}

static BL_INLINE void blFontImplCtor(BLInternalFontImpl* impl) noexcept {
  impl->face._d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d;
  blCallCtor(impl->featureSettings);
  blCallCtor(impl->variationSettings);
}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_FONT_P_H_INCLUDED
