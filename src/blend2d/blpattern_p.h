// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLPATTERN_P_H
#define BLEND2D_BLPATTERN_P_H

#include "./blapi-internal_p.h"
#include "./blpattern.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLPattern [Internal[]
// ============================================================================

//! Internal implementation that extends `BLPatternImpl`.
struct BLInternalPatternImpl : public BLPatternImpl {
  // Nothing at the moment.
};

template<>
struct BLInternalCastImpl<BLPatternImpl> { typedef BLInternalPatternImpl Type; };

BL_HIDDEN BLResult blPatternImplDelete(BLPatternImpl* impl_) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_BLPATTERN_P_H
