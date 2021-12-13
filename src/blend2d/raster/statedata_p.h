// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_STATEDATA_P_H_INCLUDED
#define BLEND2D_RASTER_STATEDATA_P_H_INCLUDED

#include "../geometry.h"
#include "../matrix_p.h"
#include "../path_p.h"
#include "../raster/styledata_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

//! Raster rendering context state - based on public `BLContextState`.
class RasterContextState : public BLContextState {
public:
  //! Type of meta matrix.
  uint8_t metaMatrixType;
  //! Type of final matrix.
  uint8_t finalMatrixType;
  //! Type of meta matrix that scales to fixed point.
  uint8_t metaMatrixFixedType;
  //! Type of final matrix that scales to fixed point.
  uint8_t finalMatrixFixedType;
  //! Global alpha as integer (0..255 or 0..65535).
  uint32_t globalAlphaI;

  //! Curve flattening tolerance scaled by `fpScaleD`.
  double toleranceFixedD;

  //! Fill and stroke styles.
  StyleData style[BL_CONTEXT_OP_TYPE_MAX_VALUE + 1];

  //! Result of `(metaMatrix * userMatrix)`.
  BLMatrix2D finalMatrix;
  //! Meta matrix scaled by `fpScale`.
  BLMatrix2D metaMatrixFixed;
  //! Result of `(metaMatrix * userMatrix) * fpScale`.
  BLMatrix2D finalMatrixFixed;
  //! Integral offset to add to input coordinates in case integral transform is ok.
  BLPointI translationI;

  //! Meta clip-box (int).
  BLBoxI metaClipBoxI;
  //! Final clip box (int).
  BLBoxI finalClipBoxI;
  //! Final clip-box (double).
  BLBox finalClipBoxD;
};

//! Structure that holds a previously saved state (see `save()` and `restore()`).
//!
//! \note The struct is designed to have no gaps required by alignment so the order of members doesn't have to make
//! much sense.
struct alignas(16) SavedState {
  //! Link to the previous state.
  SavedState* prevState;
  //! Stroke options.
  BLStrokeOptionsCore strokeOptions;

  //! State ID (only valid if a cookie was used).
  uint64_t stateId;
  //! Copy of previous `BLRasterContextImpl::_contextFlags`.
  uint32_t prevContextFlags;
  //! Global alpha as integer (0..255 or 0..65535).
  uint32_t globalAlphaI;

  //! Context hints.
  BLContextHints hints;
  //! Composition operator.
  uint8_t compOp;
  //! Fill rule.
  uint8_t fillRule;
  //! Clip mode.
  uint8_t clipMode;
  //! Type of meta matrix.
  uint8_t metaMatrixType;
  //! Type of final matrix.
  uint8_t finalMatrixType;
  //! Type of meta matrix that scales to fixed point.
  uint8_t metaMatrixFixedType;
  //! Type of final matrix that scales to fixed point.
  uint8_t finalMatrixFixedType;
  //! Padding at the moment.
  uint8_t reserved[1];
  //! Approximation options.
  BLApproximationOptions approximationOptions;

  //! Global alpha value [0, 1].
  double globalAlpha;
  //! Fill and stroke alpha values [0, 1].
  double styleAlpha[2];

  //! Final clipBox (double).
  BLBox finalClipBoxD;

  //! Fill and stroke styles.
  StyleData style[BL_CONTEXT_OP_TYPE_MAX_VALUE + 1];

  //! Meta matrix or final matrix (depending on flags).
  BLMatrix2D altMatrix;
  //! User matrix.
  BLMatrix2D userMatrix;
  //! Integral translation, if possible.
  BLPointI translationI;
};

//! A shared fill state is used by asynchronous rendering context and can be shared between multiple rendering jobs.
struct SharedFillState {
  BLBox finalClipBoxFixedD;
  BLMatrix2D finalMatrixFixed;
  double toleranceFixedD;
};

//! A shared stroke state is used by asynchronous rendering context and can be shared between multiple rendering jobs.
struct SharedBaseStrokeState {
  BLStrokeOptions strokeOptions;
  BLApproximationOptions approximationOptions;

  BL_INLINE explicit SharedBaseStrokeState(const BLStrokeOptions& strokeOptions, const BLApproximationOptions& approximationOptions) noexcept
    : strokeOptions(strokeOptions),
      approximationOptions(approximationOptions) {}
};

//! A shared stroke state that is used by strokes with specific transformOrder.
struct SharedExtendedStrokeState : public SharedBaseStrokeState {
  BLMatrix2D userMatrix;
  BLMatrix2D metaMatrixFixed;

  BL_INLINE explicit SharedExtendedStrokeState(const BLStrokeOptions& strokeOptions, const BLApproximationOptions& approximationOptions) noexcept
    : SharedBaseStrokeState(strokeOptions, approximationOptions) {}
};

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_STATEDATA_P_H_INCLUDED
