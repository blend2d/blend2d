// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_BLRASTERDEFS_P_H
#define BLEND2D_RASTER_BLRASTERDEFS_P_H

#include "../blcompop_p.h"
#include "../blcontext_p.h"
#include "../blgradient_p.h"
#include "../blmatrix_p.h"
#include "../blpattern_p.h"
#include "../blpath_p.h"
#include "../blpipe_p.h"
#include "../blvariant_p.h"
#include "../pipegen/blpiperuntime_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct BLRasterContextImpl;
struct BLRasterContextSavedState;
struct BLRasterFetchData;

class BLRasterWorker;

// ============================================================================
// [Typedefs]
// ============================================================================

typedef void (BL_CDECL* BLRasterFetchDataDestroyFunc)(BLRasterContextImpl* ctxI, BLRasterFetchData* fetchData) BL_NOEXCEPT;

// ============================================================================
// [Constants]
// ============================================================================

//! Raster context flags:
//!
//! - `NO_` - used to describe that there will be nothing rendered regardless
//!   of the render command.
//!
//!   If one more no flag is set each rendering command will be terminated as
//!   early as possible as the engine knows that there will be nothing changed
//!   in the destination raster. Render parameters are still validated though.
//!
//! - `???_` - informative flags contain some precalculated values that are
//!   handy when determining code paths to execute.
//!
//! - `STATE_` - describe which states must be saved to `BLRasterContextSavedState`
//!   in order to modify them. Used by `save()`, `restore()` and by all other
//!   functions that manipulate the painter state. Initially all state flags
//!   are unset.
enum BLRasterContextFlags : uint32_t {
  BL_RASTER_CONTEXT_NO_CONDITIONAL       = 0x00000001u, //!< Used as a result from conditional expressions.
  BL_RASTER_CONTEXT_NO_RESERVED          = 0x0000000Fu, //!< Reserved for custom flags used during command dispatching.

  BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA      = 0x00000010u, //!< Global alpha is zero.
  BL_RASTER_CONTEXT_NO_BASE_ALPHA        = 0x00000020u, //!< Start of fill/stroke 'no' alpha.
  BL_RASTER_CONTEXT_NO_FILL_ALPHA        = 0x00000020u, //!< Fill alpha is zero.
  BL_RASTER_CONTEXT_NO_STROKE_ALPHA      = 0x00000040u, //!< Stroke alpha is zero.
  BL_RASTER_CONTEXT_NO_BASE_STYLE        = 0x00000080u, //!< Start of fill/stroke 'no' flags.
  BL_RASTER_CONTEXT_NO_FILL_STYLE        = 0x00000080u, //!< Fill style is invalid or none.
  BL_RASTER_CONTEXT_NO_STROKE_STYLE      = 0x00000100u, //!< Stroke style is invalid or none.
  BL_RASTER_CONTEXT_NO_STROKE_OPTIONS    = 0x00000200u, //!< One or more stroke parameter is invalid.
  BL_RASTER_CONTEXT_NO_CLIP_RECT         = 0x00000400u, //!< User region is empty.
  BL_RASTER_CONTEXT_NO_CLIP_MASK         = 0x00000800u, //!< User mask is empty.
  BL_RASTER_CONTEXT_NO_META_MATRIX       = 0x00001000u, //!< Meta matrix is invalid.
  BL_RASTER_CONTEXT_NO_USER_MATRIX       = 0x00002000u, //!< User matrix is invalid.
  BL_RASTER_CONTEXT_NO_VALID_STATE       = 0x00004000u, //!< Rendering is disabled because of fatal error.
  BL_RASTER_CONTEXT_NO_ALL_FLAGS         = 0x0000FFFFu, //!< All 'no' flags.

  BL_RASTER_CONTEXT_BASE_FETCH_DATA      = 0x00010000u, //!< Start of fill/stroke fetch-data.
  BL_RASTER_CONTEXT_FILL_FETCH_DATA      = 0x00010000u, //!< Fill style is gradient or pattern.
  BL_RASTER_CONTEXT_STROKE_FETCH_DATA    = 0x00020000u, //!< Stroke style is gradient or pattern.
  BL_RASTER_CONTEXT_STROKE_CHANGED       = 0x00040000u, //!< Stroke options changed so it's required to recheck them before use.
  BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION = 0x00080000u, //!< Final matrix is just a scale of `fpScaleD()` and integral translation.

  BL_RASTER_CONTEXT_STATE_CONFIG         = 0x01000000u, //!< Configuration (tolerance).
  BL_RASTER_CONTEXT_STATE_CLIP           = 0x02000000u, //!< Clip state.
  BL_RASTER_CONTEXT_STATE_BASE_STYLE     = 0x04000000u, //!< Start of fill/stroke style flags.
  BL_RASTER_CONTEXT_STATE_FILL_STYLE     = 0x04000000u, //!< Fill style state.
  BL_RASTER_CONTEXT_STATE_STROKE_STYLE   = 0x08000000u, //!< Stroke style state.
  BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS = 0x10000000u, //!< Stroke params state.
  BL_RASTER_CONTEXT_STATE_META_MATRIX    = 0x20000000u, //!< Meta matrix state.
  BL_RASTER_CONTEXT_STATE_USER_MATRIX    = 0x40000000u, //!< User matrix state.
  BL_RASTER_CONTEXT_STATE_ALL_FLAGS      = 0xFF000000u, //!< All states' flags.

  //! All possible flags that prevent something to be cleared.
  BL_RASTER_CONTEXT_NO_CLEAR_FLAGS       = BL_RASTER_CONTEXT_NO_RESERVED      |
                                           BL_RASTER_CONTEXT_NO_CLIP_RECT     |
                                           BL_RASTER_CONTEXT_NO_CLIP_MASK     |
                                           BL_RASTER_CONTEXT_NO_META_MATRIX   |
                                           BL_RASTER_CONTEXT_NO_USER_MATRIX   |
                                           BL_RASTER_CONTEXT_NO_VALID_STATE   ,

  //! Like `BL_RASTER_CONTEXT_NO_FILL_FLAGS`, but without having Matrix checks
  //! as FillAll works regardless of transformation.
  BL_RASTER_CONTEXT_NO_CLEAR_FLAGS_FORCE = BL_RASTER_CONTEXT_NO_RESERVED      |
                                           BL_RASTER_CONTEXT_NO_CLIP_RECT     |
                                           BL_RASTER_CONTEXT_NO_CLIP_MASK     |
                                           BL_RASTER_CONTEXT_NO_VALID_STATE   ,

  //! All possible flags that prevent something to be filled.
  BL_RASTER_CONTEXT_NO_FILL_FLAGS        = BL_RASTER_CONTEXT_NO_RESERVED      |
                                           BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA  |
                                           BL_RASTER_CONTEXT_NO_FILL_ALPHA    |
                                           BL_RASTER_CONTEXT_NO_FILL_STYLE    |
                                           BL_RASTER_CONTEXT_NO_CLIP_RECT     |
                                           BL_RASTER_CONTEXT_NO_CLIP_MASK     |
                                           BL_RASTER_CONTEXT_NO_META_MATRIX   |
                                           BL_RASTER_CONTEXT_NO_USER_MATRIX   |
                                           BL_RASTER_CONTEXT_NO_VALID_STATE   ,

  //! Like `BL_RASTER_CONTEXT_NO_FILL_FLAGS`, but without having Matrix checks
  //! as FillAll works regardless of transformation.
  BL_RASTER_CONTEXT_NO_FILL_FLAGS_FORCE  = BL_RASTER_CONTEXT_NO_RESERVED      |
                                           BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA  |
                                           BL_RASTER_CONTEXT_NO_FILL_ALPHA    |
                                           BL_RASTER_CONTEXT_NO_FILL_STYLE    |
                                           BL_RASTER_CONTEXT_NO_CLIP_RECT     |
                                           BL_RASTER_CONTEXT_NO_CLIP_MASK     |
                                           BL_RASTER_CONTEXT_NO_VALID_STATE   ,

  //! All possible flags that prevent something to be stroked.
  BL_RASTER_CONTEXT_NO_STROKE_FLAGS      = BL_RASTER_CONTEXT_NO_RESERVED      |
                                           BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA  |
                                           BL_RASTER_CONTEXT_NO_STROKE_ALPHA  |
                                           BL_RASTER_CONTEXT_NO_STROKE_STYLE  |
                                           BL_RASTER_CONTEXT_NO_STROKE_OPTIONS|
                                           BL_RASTER_CONTEXT_NO_CLIP_RECT     |
                                           BL_RASTER_CONTEXT_NO_CLIP_MASK     |
                                           BL_RASTER_CONTEXT_NO_META_MATRIX   |
                                           BL_RASTER_CONTEXT_NO_USER_MATRIX   |
                                           BL_RASTER_CONTEXT_NO_VALID_STATE   ,

  //! All possible flags that prevent something to be blitted.
  BL_RASTER_CONTEXT_NO_BLIT_FLAGS        = BL_RASTER_CONTEXT_NO_RESERVED      |
                                           BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA  |
                                           BL_RASTER_CONTEXT_NO_CLIP_RECT     |
                                           BL_RASTER_CONTEXT_NO_CLIP_MASK     |
                                           BL_RASTER_CONTEXT_NO_META_MATRIX   |
                                           BL_RASTER_CONTEXT_NO_USER_MATRIX   |
                                           BL_RASTER_CONTEXT_NO_VALID_STATE
};

//! Indexes to a `BLRasterContextImpl::solidFormatTable`, which describes pixel
//! formats used by solid fills. There are in total 3 choices that are selected
//! based on properties of the solid color.
enum BLRasterContextSolidFormatId : uint32_t {
  BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB = 0,
  BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB = 1,
  BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO = 2,

  BL_RASTER_CONTEXT_SOLID_FORMAT_COUNT = 3
};

enum BLRasterContextFillStatus : uint32_t {
  BL_RASTER_CONTEXT_FILL_STATUS_NOP   = 0,
  BL_RASTER_CONTEXT_FILL_STATUS_SOLID = 1,
  BL_RASTER_CONTEXT_FILL_STATUS_FETCH = 2
};

//! Preferred fill-rule (fastest) to use when the fill-rule doesn't matter.
//!
//! Since the filler doesn't care of fill-rule (it always uses the same code-path
//! for non-zero and even-odd fills) it doesn't really matter. However, if there
//! is more rasterizers added in the future this can be adjusted to tell the
//! context, which fill-rule would be faster.
static constexpr const uint32_t BL_RASTER_CONTEXT_PREFERRED_FILL_RULE = BL_FILL_RULE_NON_ZERO;

//! Preferred extend mode (fastest) to use when blitting images. The extend mode
//! can be either PAD or REFLECT as these have the same effect on blits that are
//! bound to the size of the image. We prefer PAD, because in can be a little
//! bit faster in some cases in which a different code-path is used compared to
//! a generic pipeline that handle handle all extend modes by design.
static constexpr const uint32_t BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND = BL_EXTEND_MODE_REFLECT;

// ============================================================================
// [BLRasterContextDstInfo]
// ============================================================================

//! Raster rendering context destination info.
//!
//! The information is immutable after the image has been attached.
struct BLRasterContextDstInfo {
  uint8_t format;
  //! Whether the destination uses 16 bits per component.
  uint8_t is16Bit;
  //! Reserved.
  uint8_t reserved[2];
  //! Full alpha (256 or 65536).
  uint32_t fullAlphaI;
  //! Full alpha (256 or 65536) stored as `double`.
  double fullAlphaD;

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// [BLRasterFetchData]
// ============================================================================

struct alignas(16) BLRasterFetchData {
  BLPipeFetchData data;

  //! Reference count (not atomic, not needed here).
  size_t refCount;
  //! Destroys (unreferences) the dynamically allocated data used by the style.
  BLRasterFetchDataDestroyFunc destroy;

  //! Contains link to a dynamically allocated data required by the style.
  union {
    void* voidPtr;
    BLImageImpl* imageI;
    BLGradientLUT* gradientLut;
  };

  //! Fetch type.
  uint8_t fetchType;
  //! Fetch (source) format.
  uint8_t fetchFormat;
};

// ============================================================================
// [BLStyleData]
// ============================================================================

//! Style data holds a copy of used provided style with additional members that
//! allow to create a `BLRasterFetchData` from it. When a style is assigned to
//! the rendering context it has to calculate the style transformation matrix
//! and few other things that could degrade the style into a solid style.
struct BLRasterContextStyleData {
  union {
    uint32_t packed;
    struct {
      //! Style type.
      uint8_t styleType;
      //! Style pixel format.
      uint8_t styleFormat;
      //! Gradient/Pattern filter.
      uint8_t quality;
      //! Adjusted matrix type.
      uint8_t adjustedMatrixType;
    };
  };

  //! Alpha value (0..256 or 0..65536).
  uint32_t alphaI;
  //! Solid data.
  BLPipeFetchData::Solid solidData;
  //! Fetch data.
  BLRasterFetchData* fetchData;

  union {
    //! Solid color as non-premultiplied RGBA64.
    BLRgba64 rgba64;
    //!< Style as variant.
    BLWrap<BLVariant> variant;
    //!< Style as pattern.
    BLWrap<BLPattern> pattern;
    //!< Style as gradient.
    BLWrap<BLGradient> gradient;
  };

  //! Adjusted matrix.
  BLMatrix2D adjustedMatrix;
};

// ============================================================================
// [BLFillCmd]
// ============================================================================

struct BLRasterFillCmd {
  //! Signature parts related to destination format, compOp and source style.
  BLPipeSignature baseSignature;
  //! Final alpha (integral).
  uint32_t alphaI;

  union {
    uint32_t packed;
    struct {
      uint8_t fillRule;
      uint8_t styleFormat;
      uint8_t reserved[2];
    };
  };

  //! Solid data.
  BLPipeFetchData::Solid solidData;
  //! Fetch data.
  BLRasterFetchData* fetchData;
  //! Style data to use when `_fetchData` is not available.
  BLRasterContextStyleData* styleData;

  BL_INLINE void reset(const BLPipeSignature& initialSignature, uint32_t alphaI, uint32_t fillRule) noexcept {
    this->baseSignature = initialSignature;
    this->alphaI = alphaI;
    this->packed = 0;
    this->fillRule = uint8_t(fillRule);
    this->styleData = nullptr;
  }

  BL_INLINE void setFetchDataFromLocal(BLRasterFetchData* fetchData) noexcept {
    this->fetchData = fetchData;
  }

  BL_INLINE void setFetchDataFromStyle(BLRasterContextStyleData* styleData) noexcept {
    this->solidData = styleData->solidData;
    this->fetchData = styleData->fetchData;
    this->styleData = styleData;
  }
};

// ============================================================================
// [BLRasterContextSavedState]
// ============================================================================

//! Structure that holds a saved state.
//!
//! NOTE: The struct is designed to have no gaps required by alignment so the
//! order of members doesn't have to make much sense.
struct alignas(16) BLRasterContextSavedState {
  //! Link to the previous state.
  BLRasterContextSavedState* prevState;
  //! Stroke options.
  BLStrokeOptionsCore strokeOptions;

  //! State ID (only valid if a cookie was used).
  uint64_t stateId;
  //! Copy of previous `BLRasterContextImpl::_contextFlags`.
  uint32_t prevContextFlags;
  //! Global alpha as integer (0..256 or 0..65536).
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
  //! Fill alpha value [0, 1].
  double fillAlpha;
  //! Stroke alpha value [0, 1].
  double strokeAlpha;

  //! Final clipBox (double).
  BLBox finalClipBoxD;

  //! Fill and stroke styles.
  BLRasterContextStyleData style[BL_CONTEXT_OP_TYPE_COUNT];

  //! Meta matrix or final matrix (depending on flags).
  BLMatrix2D altMatrix;
  //! User matrix.
  BLMatrix2D userMatrix;
  //! Integral translation, if possible.
  BLPointI translationI;
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_BLRASTERDEFS_P_H
