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

#ifndef BLEND2D_CONTEXT_P_H_INCLUDED
#define BLEND2D_CONTEXT_P_H_INCLUDED

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
  self->targetImage = nullptr;
  self->targetSize.reset();
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

//! \}
//! \endcond

#endif // BLEND2D_CONTEXT_P_H_INCLUDED
