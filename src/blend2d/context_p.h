// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CONTEXT_P_H_INCLUDED
#define BLEND2D_CONTEXT_P_H_INCLUDED

#include "api-internal_p.h"
#include "array_p.h"
#include "context.h"
#include "object_p.h"
#include "path_p.h"
#include "threading/atomic_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLContextPrivate {

//! \name Context - Private - Constants
//! \{

static constexpr const double kMinimumTolerance = 0.01;
static constexpr const double kMaximumTolerance = 0.50;

//! \}

//! \name Context - Private - State Construction & Destruction
//! \{

static BL_INLINE void initState(BLContextState* self) noexcept {
  self->targetImage = nullptr;
  self->targetSize.reset();
  self->hints.reset();
  self->compOp = uint8_t(BL_COMP_OP_SRC_OVER);
  self->fillRule = uint8_t(BL_FILL_RULE_NON_ZERO);
  self->styleType[BL_CONTEXT_OP_TYPE_FILL] = uint8_t(BL_OBJECT_TYPE_NULL);
  self->styleType[BL_CONTEXT_OP_TYPE_STROKE] = uint8_t(BL_OBJECT_TYPE_NULL);
  memset(self->reserved, 0, sizeof(self->reserved));

  self->approximationOptions = BLPathPrivate::makeDefaultApproximationOptions();
  self->globalAlpha = 1.0;
  self->styleAlpha[BL_CONTEXT_OP_TYPE_FILL] = 1.0;
  self->styleAlpha[BL_CONTEXT_OP_TYPE_STROKE] = 1.0;

  blStrokeOptionsInit(&self->strokeOptions);
  self->metaMatrix.reset();
  self->userMatrix.reset();
  self->savedStateCount = 0;
}

static BL_INLINE void destroyState(BLContextState* self) noexcept {
  blArrayReset(&self->strokeOptions.dashArray);
}

//! \}

} // {BLContextPrivate}

//! \}
//! \endcond

#endif // BLEND2D_CONTEXT_P_H_INCLUDED
