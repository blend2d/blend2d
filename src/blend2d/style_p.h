// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_STYLE_P_H
#define BLEND2D_STYLE_P_H

#include "./math_p.h"
#include "./style.h"
#include "./variant_p.h"

//! \cond iNTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLStyle - Internal Utilities]
// ============================================================================

static BL_INLINE bool blStyleIsValidRgba(const BLRgba& rgba) noexcept {
  return !blIsNaN(rgba.r, rgba.g, rgba.b, rgba.a);
}

static BL_INLINE uint32_t blStyleIsValidImplType(uint32_t implType) noexcept {
  return implType == BL_IMPL_TYPE_PATTERN ||
         implType == BL_IMPL_TYPE_GRADIENT;
}

static BL_INLINE uint32_t blStyleTypeFromImplType(uint32_t implType) noexcept {
  if (implType == BL_IMPL_TYPE_PATTERN)
    return BL_STYLE_TYPE_PATTERN;

  if (implType == BL_IMPL_TYPE_GRADIENT)
    return BL_STYLE_TYPE_GRADIENT;

  return BL_STYLE_TYPE_NONE;
}

static BL_INLINE BLResult blStyleInitNoneInline(BLStyleCore* self) noexcept {
  blDownCast(self)->_makeTagged(BL_STYLE_TYPE_NONE);
  return BL_SUCCESS;
}

static BL_INLINE BLResult blStyleInitObjectInline(BLStyleCore* self, BLVariantImpl* impl, uint32_t styleType) noexcept {
  blDownCast(self)->_makeTagged(styleType);
  self->variant.impl = impl;
  return BL_SUCCESS;
}

static BL_INLINE void blStyleDestroyInline(BLStyleCore* self) noexcept {
  if (blDownCast(self)->isObject())
    blVariantImplRelease(self->variant.impl);
}

//! \}
//! \endcond

#endif // BLEND2D_STYLE_P_H
