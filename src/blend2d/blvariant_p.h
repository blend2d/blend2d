// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLVARIANT_P_H
#define BLEND2D_BLVARIANT_P_H

#include "./blapi-internal_p.h"
#include "./blvariant.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLVariant - Internal]
// ============================================================================

BL_HIDDEN BLResult blVariantImplDelete(BLVariantImpl* impl) noexcept;

static BL_INLINE BLResult blVariantImplRelease(BLVariantImpl* impl) noexcept {
  if (blImplDecRefAndTest(impl))
    return blVariantImplDelete(impl);
  return BL_SUCCESS;
}

//! \}
//! \endcond

#endif // BLEND2D_BLVARIANT_P_H
