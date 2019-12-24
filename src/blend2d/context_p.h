// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_CONTEXT_P_H
#define BLEND2D_CONTEXT_P_H

#include "./api-internal_p.h"
#include "./array_p.h"
#include "./context.h"
#include "./path_p.h"
#include "./support_p.h"
#include "./threading/atomic_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Constants]
// ============================================================================

static constexpr const double BL_CONTEXT_MINIMUM_TOLERANCE = 0.01;
static constexpr const double BL_CONTEXT_MAXIMUM_TOLERANCE = 0.50;

// ============================================================================
// [Utilities]
// ============================================================================

static BL_INLINE void blContextStateInit(BLContextState* self) noexcept {
  self->hints.reset();
  self->compOp = uint8_t(BL_COMP_OP_SRC_OVER);
  self->fillRule = uint8_t(BL_FILL_RULE_NON_ZERO);
  self->styleType[BL_CONTEXT_OP_TYPE_FILL] = uint8_t(BL_STYLE_TYPE_NONE);
  self->styleType[BL_CONTEXT_OP_TYPE_STROKE] = uint8_t(BL_STYLE_TYPE_NONE);
  memset(self->reserved, 0, sizeof(self->reserved));

  self->approximationOptions = blMakeDefaultApproximationOptions();
  self->globalAlpha = 1.0;
  self->styleAlpha[BL_CONTEXT_OP_TYPE_FILL] = 1.0;
  self->styleAlpha[BL_CONTEXT_OP_TYPE_STROKE] = 1.0;

  blStrokeOptionsInit(&self->strokeOptions);
  self->metaMatrix.reset();
  self->userMatrix.reset();
  self->savedStateCount = 0;
}

static BL_INLINE void blContextStateDestroy(BLContextState* self) noexcept {
  blArrayReset(&self->strokeOptions.dashArray);
}

BL_HIDDEN extern BLWrap<BLContextState> blNullContextState;
BL_HIDDEN extern BLAtomicUInt64Generator blContextIdGenerator;

//! \}
//! \endcond

#endif // BLEND2D_CONTEXT_P_H
