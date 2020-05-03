// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_STYLE_P_H_INCLUDED
#define BLEND2D_STYLE_P_H_INCLUDED

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

#endif // BLEND2D_STYLE_P_H_INCLUDED
