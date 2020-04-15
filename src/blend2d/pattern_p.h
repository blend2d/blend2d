// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_PATTERN_P_H
#define BLEND2D_PATTERN_P_H

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

#endif // BLEND2D_PATTERN_P_H
