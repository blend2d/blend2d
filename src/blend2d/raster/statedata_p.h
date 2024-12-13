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

namespace bl {
namespace RasterEngine {

//! Raster rendering context state - based on public `BLContextState`.
class alignas(16) RasterContextState : public BLContextState {
public:
  union {
    uint32_t transformTypesPacked;
    struct {
      //! Type of final transformation matrix that scales to fixed point.
      uint8_t finalTransformFixedType;
      //! Type of meta transformation matrix that scales to fixed point.
      uint8_t metaTransformFixedType;
      //! Type of final transformation matrix.
      uint8_t finalTransformType;
      //! Type of meta transformation matrix.
      uint8_t metaTransformType;
      //! Type of the identity transformation matrix (used by Style API).
      uint8_t identityTransformType;
    };

    struct {
      uint8_t fixedTransformTypes[2];
      //! Transform types indexed by \ref BLContextStyleTransformMode (used by Style API).
      uint8_t transformTypes[uint32_t(BL_CONTEXT_STYLE_TRANSFORM_MODE_MAX_VALUE) + 1u];
    };
  };

  //! Global alpha as integer (0..255 or 0..65535).
  uint32_t globalAlphaI;
  //! Current fill or stroke alpha converted to integer indexed by style slot, see \ref BLContextStyleSlot.
  uint32_t styleAlphaI[2];

  //! Curve flattening tolerance scaled by `fpScaleD`.
  double toleranceFixedD;

  //! Fill and stroke styles, and one additional style that is never used in practice, but is used during error checking.
  StyleData style[2];

  //! Integral offset to add to input coordinates in case integral transform is ok.
  BLPointI translationI;

  //! Meta matrix scaled by `fpScale`.
  alignas(16) BLMatrix2D metaTransformFixed;
  //! Result of `(metaTransform * userTransform) * fpScale`.
  alignas(16) BLMatrix2D finalTransformFixed;

  //! Meta clip-box (int).
  alignas(16) BLBoxI metaClipBoxI;
  //! Final clip box (int).
  alignas(16) BLBoxI finalClipBoxI;
  //! Final clip-box (double).
  alignas(16) BLBox finalClipBoxD;
};

//! Structure that holds a previously saved state, see \ref BLContext::save() and \ref BLContext::restore().
//!
//! \note The struct is designed to have no gaps required by alignment so the order of members doesn't have to make
//! much sense.
struct alignas(16) SavedState {
  //! Link to the previous state.
  SavedState* prevState;
  //! State ID (only valid if a cookie was used).
  uint64_t stateId;

  //! Context hints.
  BLContextHints hints;
  //! Composition operator.
  uint8_t compOp;
  //! Fill rule.
  uint8_t fillRule;
  //! Current type of a style object of fill and stroke operations indexed by \ref BLContextStyleSlot.
  uint8_t styleType[2];

  //! Clip mode.
  uint8_t clipMode;
  //! Padding at the moment.
  uint8_t reserved[7];

  //! Copy of previous `BLRasterContextImpl::_contextFlags`.
  ContextFlags prevContextFlags;

  union {
    uint32_t transformTypesPacked;
    struct {
      //! Type of final matrix that scales to fixed point.
      uint8_t finalTransformFixedType;
      //! Type of meta matrix that scales to fixed point.
      uint8_t metaTransformFixedType;
      //! Type of final matrix.
      uint8_t finalTransformType;
      //! Type of meta matrix.
      uint8_t metaTransformType;
    };
  };
  //! Global alpha as integer (0..255 or 0..65535).
  uint32_t globalAlphaI;
  //! Alpha value (0..255 or 0..65535).
  uint32_t styleAlphaI[2];

  //! Global alpha value [0, 1].
  double globalAlpha;
  //! Fill and stroke alpha values [0, 1].
  double styleAlpha[2];
  //! Fill and stroke styles.
  StyleData style[2];

  //! Approximation options.
  BLApproximationOptions approximationOptions;
  //! Stroke options.
  BLStrokeOptionsCore strokeOptions;

  //! Final clipBox (double).
  BLBox finalClipBoxD;

  //! Integral translation, if possible.
  BLPointI translationI;
  //! Meta or final transformation matrix (depending on flags).
  BLMatrix2D altTransform;
  //! User transformation matrix.
  BLMatrix2D userTransform;
};

struct Matrix2x2 {
  double m[4];
};

//! A shared fill state is used by asynchronous rendering context and can be shared between multiple rendering jobs.
struct SharedFillState {
  BLBox finalClipBoxFixedD;
  Matrix2x2 finalTransformFixed;
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
  Matrix2x2 userTransform;
  Matrix2x2 metaTransformFixed;

  BL_INLINE explicit SharedExtendedStrokeState(const BLStrokeOptions& strokeOptions, const BLApproximationOptions& approximationOptions) noexcept
    : SharedBaseStrokeState(strokeOptions, approximationOptions) {}
};

} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_STATEDATA_P_H_INCLUDED
