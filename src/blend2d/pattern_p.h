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

#ifndef BLEND2D_PATTERN_P_H_INCLUDED
#define BLEND2D_PATTERN_P_H_INCLUDED

#include "./api-internal_p.h"
#include "./pattern.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLPattern - Internal]
// ============================================================================

//! Internal implementation that extends `BLPatternImpl`.
struct BLInternalPatternImpl : public BLPatternImpl {
  // Nothing at the moment.
};

template<>
struct BLInternalCastImpl<BLPatternImpl> { typedef BLInternalPatternImpl Type; };

BL_HIDDEN BLResult blPatternImplDelete(BLPatternImpl* impl_) noexcept;

// ============================================================================
// [BLPattern - Utilities]
// ============================================================================

static BL_INLINE bool blPatternIsAreaValid(const BLRectI& area, const BLSizeI& size) noexcept {
  typedef unsigned U;
  return bool((U(area.x) < U(size.w)) &
              (U(area.y) < U(size.h)) &
              (U(area.w) <= U(size.w) - U(area.x)) &
              (U(area.h) <= U(size.h) - U(area.y)));
}

static BL_INLINE bool blPatternIsAreaValidAndNonZero(const BLRectI& area, const BLSizeI& size) noexcept {
  typedef unsigned U;
  return bool((U(area.x) < U(size.w)) &
              (U(area.y) < U(size.h)) &
              (U(area.w) - U(1) < U(size.w) - U(area.x)) &
              (U(area.h) - U(1) < U(size.h) - U(area.y)));
}

//! \}
//! \endcond

#endif // BLEND2D_PATTERN_P_H_INCLUDED
