// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RASTERCONTEXT_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERCONTEXT_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../compop_p.h"
#include "../context_p.h"
#include "../font_p.h"
#include "../gradient_p.h"
#include "../matrix_p.h"
#include "../path_p.h"
#include "../rgba.h"
#include "../pipeline/piperuntime_p.h"
#include "../raster/analyticrasterizer_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/rendercommand_p.h"
#include "../raster/renderfetchdata_p.h"
#include "../raster/renderjob_p.h"
#include "../raster/renderqueue_p.h"
#include "../raster/rendertargetinfo_p.h"
#include "../raster/statedata_p.h"
#include "../raster/styledata_p.h"
#include "../raster/workdata_p.h"
#include "../raster/workermanager_p.h"
#include "../support/arenaallocator_p.h"
#include "../threading/uniqueidgenerator_p.h"

#if !defined(BL_BUILD_NO_JIT)
  #include "../pipeline/jit/pipegenruntime_p.h"
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

//! Rendering mode.
enum BLRasterRenderingMode : uint32_t {
  BL_RASTER_RENDERING_MODE_SYNC = 0,
  BL_RASTER_RENDERING_MODE_ASYNC = 1
};

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
//! - `SHARED_` - shared states used by multithreaded rendering. Some functions
//!   in the rendering context don't care whether the rendering is synchronous
//!   or asynchronous and just clear the `SHARED_` flags in case that something
//!   shared was changed. Then before a command is enqueued such flags are checked
//!   and the shared state is created when necessary.
//!
//! - `STATE_` - describe which states must be saved to `SavedState`
//!   in order to modify them. Used by `save()`, `restore()` and by all other
//!   functions that manipulate the state. Initially all state flags are unset.
enum BLRasterContextFlags : uint32_t {
  //! Used as a result from conditional expressions.
  BL_RASTER_CONTEXT_NO_CONDITIONAL          = 0x00000001u,
  //! Reserved for custom flags used during command dispatching.
  BL_RASTER_CONTEXT_NO_RESERVED             = 0x0000000Fu,

  //! Global alpha is zero.
  BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA         = 0x00000010u,
  //! Start of fill/stroke 'no' alpha.
  BL_RASTER_CONTEXT_NO_BASE_ALPHA           = 0x00000020u,
  //! Fill alpha is zero.
  BL_RASTER_CONTEXT_NO_FILL_ALPHA           = 0x00000020u,
  //! Stroke alpha is zero.
  BL_RASTER_CONTEXT_NO_STROKE_ALPHA         = 0x00000040u,
  //! Start of fill/stroke 'no' flags.
  BL_RASTER_CONTEXT_NO_BASE_STYLE           = 0x00000080u,
  //! Fill style is invalid or none.
  BL_RASTER_CONTEXT_NO_FILL_STYLE           = 0x00000080u,
  //! Stroke style is invalid or none.
  BL_RASTER_CONTEXT_NO_STROKE_STYLE         = 0x00000100u,
  //! One or more stroke parameter is invalid.
  BL_RASTER_CONTEXT_NO_STROKE_OPTIONS       = 0x00000200u, // TODO: [Rendering Context] Never set!
  //! User clip-rect is empty.
  BL_RASTER_CONTEXT_NO_CLIP_RECT            = 0x00000400u,
  //! User clip-mask is empty.
  BL_RASTER_CONTEXT_NO_CLIP_MASK            = 0x00000800u,
  //! Meta matrix is invalid.
  BL_RASTER_CONTEXT_NO_META_MATRIX          = 0x00001000u,
  //! User matrix is invalid.
  BL_RASTER_CONTEXT_NO_USER_MATRIX          = 0x00002000u,
  //! Rendering is disabled because of fatal error.
  BL_RASTER_CONTEXT_NO_VALID_STATE          = 0x00004000u,
  //! All 'no' flags.
  BL_RASTER_CONTEXT_NO_ALL_FLAGS            = 0x0000FFFFu,

  //! Start of non-solid fill/stroke flag
  BL_RASTER_CONTEXT_BASE_FETCH_DATA         = 0x00010000u,
  //! Fill style is not solid nor none.
  BL_RASTER_CONTEXT_FILL_FETCH_DATA         = 0x00010000u,
  //! Stroke style is not solid nor none.
  BL_RASTER_CONTEXT_STROKE_FETCH_DATA       = 0x00020000u,

  //! Shared fill-state has valid data.
  BL_RASTER_CONTEXT_SHARED_FILL_STATE       = 0x00100000u,
  //! Shared stroke-state has valid base-stroke data.
  BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE= 0x00200000u,
  //! Shared stroke-state has valid extended-stroke data.
  BL_RASTER_CONTEXT_SHARED_STROKE_EXT_STATE = 0x00400000u,

  //! Final matrix is just a scale of `fpScaleD()` and integral translation.
  BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION    = 0x00800000u,

  //! Configuration (tolerance).
  BL_RASTER_CONTEXT_STATE_CONFIG            = 0x01000000u,
  //! Clip state.
  BL_RASTER_CONTEXT_STATE_CLIP              = 0x02000000u,
  //! Start of fill/stroke style flags.
  BL_RASTER_CONTEXT_STATE_BASE_STYLE        = 0x04000000u,
  //! Fill style state.
  BL_RASTER_CONTEXT_STATE_FILL_STYLE        = 0x04000000u,
  //! Stroke style state.
  BL_RASTER_CONTEXT_STATE_STROKE_STYLE      = 0x08000000u,
  //! Stroke params state.
  BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS    = 0x10000000u,
  //! Meta matrix state.
  BL_RASTER_CONTEXT_STATE_META_MATRIX       = 0x20000000u,
  //! User matrix state.
  BL_RASTER_CONTEXT_STATE_USER_MATRIX       = 0x40000000u,
  //! All states' flags.
  BL_RASTER_CONTEXT_STATE_ALL_FLAGS         = 0xFF000000u,

  //! All possible flags that prevent something to be cleared.
  BL_RASTER_CONTEXT_NO_CLEAR_FLAGS          = BL_RASTER_CONTEXT_NO_RESERVED
                                            | BL_RASTER_CONTEXT_NO_CLIP_RECT
                                            | BL_RASTER_CONTEXT_NO_CLIP_MASK
                                            | BL_RASTER_CONTEXT_NO_META_MATRIX
                                            | BL_RASTER_CONTEXT_NO_USER_MATRIX
                                            | BL_RASTER_CONTEXT_NO_VALID_STATE,

  //! Like `BL_RASTER_CONTEXT_NO_FILL_FLAGS`, but without having Matrix checks
  //! as FillAll works regardless of transformation.
  BL_RASTER_CONTEXT_NO_CLEAR_FLAGS_FORCE    = BL_RASTER_CONTEXT_NO_RESERVED
                                            | BL_RASTER_CONTEXT_NO_CLIP_RECT
                                            | BL_RASTER_CONTEXT_NO_CLIP_MASK
                                            | BL_RASTER_CONTEXT_NO_VALID_STATE,

  //! All possible flags that prevent something to be filled.
  BL_RASTER_CONTEXT_NO_FILL_FLAGS           = BL_RASTER_CONTEXT_NO_RESERVED
                                            | BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA
                                            | BL_RASTER_CONTEXT_NO_FILL_ALPHA
                                            | BL_RASTER_CONTEXT_NO_FILL_STYLE
                                            | BL_RASTER_CONTEXT_NO_CLIP_RECT
                                            | BL_RASTER_CONTEXT_NO_CLIP_MASK
                                            | BL_RASTER_CONTEXT_NO_META_MATRIX
                                            | BL_RASTER_CONTEXT_NO_USER_MATRIX
                                            | BL_RASTER_CONTEXT_NO_VALID_STATE,

  //! Like `BL_RASTER_CONTEXT_NO_FILL_FLAGS`, but without having Matrix checks
  //! as FillAll works regardless of transformation.
  BL_RASTER_CONTEXT_NO_FILL_FLAGS_FORCE     = BL_RASTER_CONTEXT_NO_RESERVED
                                            | BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA
                                            | BL_RASTER_CONTEXT_NO_FILL_ALPHA
                                            | BL_RASTER_CONTEXT_NO_FILL_STYLE
                                            | BL_RASTER_CONTEXT_NO_CLIP_RECT
                                            | BL_RASTER_CONTEXT_NO_CLIP_MASK
                                            | BL_RASTER_CONTEXT_NO_VALID_STATE,

  //! All possible flags that prevent something to be stroked.
  BL_RASTER_CONTEXT_NO_STROKE_FLAGS         = BL_RASTER_CONTEXT_NO_RESERVED
                                            | BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA
                                            | BL_RASTER_CONTEXT_NO_STROKE_ALPHA
                                            | BL_RASTER_CONTEXT_NO_STROKE_STYLE
                                            | BL_RASTER_CONTEXT_NO_STROKE_OPTIONS
                                            | BL_RASTER_CONTEXT_NO_CLIP_RECT
                                            | BL_RASTER_CONTEXT_NO_CLIP_MASK
                                            | BL_RASTER_CONTEXT_NO_META_MATRIX
                                            | BL_RASTER_CONTEXT_NO_USER_MATRIX
                                            | BL_RASTER_CONTEXT_NO_VALID_STATE,

  //! All possible flags that prevent something to be blitted.
  BL_RASTER_CONTEXT_NO_BLIT_FLAGS           = BL_RASTER_CONTEXT_NO_RESERVED
                                            | BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA
                                            | BL_RASTER_CONTEXT_NO_CLIP_RECT
                                            | BL_RASTER_CONTEXT_NO_CLIP_MASK
                                            | BL_RASTER_CONTEXT_NO_META_MATRIX
                                            | BL_RASTER_CONTEXT_NO_USER_MATRIX
                                            | BL_RASTER_CONTEXT_NO_VALID_STATE,

  BL_RASTER_CONTEXT_SHARED_ALL_FLAGS        = BL_RASTER_CONTEXT_SHARED_FILL_STATE
                                            | BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE
                                            | BL_RASTER_CONTEXT_SHARED_STROKE_EXT_STATE
};

//! Status returned by by command preparation functions that is used to possibly
//! change the source style into a solid color or to inform the caller that the
//! operation wouldn't do anything.
enum BLRasterContextPrepareStatus : uint32_t {
  //! The operation doesn't do anything, and thus must be discarded.
  BL_RASTER_CONTEXT_PREPARE_STATUS_NOP = 0,
  //! The operation always uses solid source, fetchData cannot be used.
  BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID = 1,
  //! The operation can use fetchData or solid source depending on other options.
  BL_RASTER_CONTEXT_PREPARE_STATUS_FETCH = 2
};

//! Preferred fill-rule (fastest) to use when the fill-rule doesn't matter.
//!
//! Since the filler doesn't care of fill-rule (it always uses the same code-path
//! for non-zero and even-odd fills) it doesn't really matter. However, if there
//! is more rasterizers added in the future this can be adjusted to always select
//! the fastest one.
static constexpr const uint32_t BL_RASTER_CONTEXT_PREFERRED_FILL_RULE = BL_FILL_RULE_EVEN_ODD;

//! Preferred extend mode (fastest) to use when blitting images. The extend mode
//! can be either PAD or REFLECT as these have the same effect on blits that are
//! bound to the size of the image. We prefer REFLECT, because it's useful also
//! outside regular blits.
static constexpr const uint32_t BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND = BL_EXTEND_MODE_REFLECT;

//! Minimum size of a path (in vertices) to make it an asynchronous job. The
//! reason for this threshold is that very small paths actually do not benefit
//! from being dispatched into a worker thread (the cost of serializing the job
//! is higher than the cost of processing that path in a user thread).
static constexpr const uint32_t BL_RASTER_CONTEXT_MINIMUM_ASYNC_PATH_SIZE = 10;

//! Maximum size of a text to be copied as is when dispatching asynchronous jobs.
//! When the limit is reached the job serialized would create a BLGlyphBuffer
//! instead of making raw copy of the text, as the glyph-buffer has to copy it
//! anyway.
static constexpr const uint32_t BL_RASTER_CONTEXT_MAXIMUM_EMBEDDED_TEXT_SIZE = 256;

//! Raster rendering context implementation (software-accelerated).
class BLRasterContextImpl : public BLContextImpl {
public:
  BL_NONCOPYABLE(BLRasterContextImpl)

  //! Zone allocator used to allocate base data structures required by `BLRasterContextImpl`.
  BLArenaAllocator baseZone;
  //! Object pool used to allocate `RenderFetchData`.
  BLArenaPool<BLRasterEngine::RenderFetchData> fetchDataPool;
  //! Object pool used to allocate `SavedState`.
  BLArenaPool<BLRasterEngine::SavedState> savedStatePool;

  //! Destination image.
  BLImageCore dstImage;
  //! Destination image data.
  BLImageData dstData;
  //! Precision information.
  BLRasterEngine::RenderTargetInfo renderTargetInfo;
  //! Minimum safe coordinate for integral transformation (scaled by 256.0 or 65536.0).
  double fpMinSafeCoordD;
  //! Maximum safe coordinate for integral transformation (scaled by 256.0 or 65536.0).
  double fpMaxSafeCoordD;

  //! Work data used by synchronous rendering that also holds part of the
  //! current state. In async mode the work data can still be used by user
  //! thread in case it's allowed, otherwise it would only hold some states
  //! that are used by the rendering context directly.
  BLRasterEngine::WorkData syncWorkData;

  //! Pipeline runtime (either global or isolated, depending on create-options).
  BLPipeline::PipeProvider pipeProvider;
  //! Pipeline lookup cache (always used before attempting to use `pipeProvider`).
  BLPipeline::PipeLookupCache pipeLookupCache;

  //! Context origin ID used in `data0` member of `BLContextCookie`.
  uint64_t contextOriginId;
  //! Used to genearate unique IDs of this context.
  uint64_t stateIdCounter;

  //! Link to the previous saved state that will be restored by `BLContext::restore()`.
  BLRasterEngine::SavedState* savedState;
  //! An actual shared fill-state (asynchronous rendering).
  BLRasterEngine::SharedFillState* sharedFillState;
  //! An actual shared stroke-state (asynchronous rendering).
  BLRasterEngine::SharedBaseStrokeState* sharedStrokeState;

  //! The current state of the rendering context, the `BLContextState` part is public.
  BLRasterEngine::RasterContextState internalState;

  //! Rendering mode.
  uint8_t renderingMode;
  //! Whether workerMgr has been initialized.
  uint8_t workerMgrInitialized;
  //! Context flags.
  uint32_t contextFlags;

  //! Composition operator simplification that matches the destination format and current `compOp`.
  const BLCompOpSimplifyInfo* compOpSimplifyInfo;
  //! Table that contains solid fetch data that is used by simplified solid fills, see `BLCompOpSolidId`.
  const BLPipeline::FetchData::Solid* solidFetchDataTable;
  //! Solid format table used to select the best pixel format for solid fills.
  uint8_t solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_COUNT];

  //! Worker manager (only used by asynchronous rendering context).
  BLWrap<BLRasterEngine::WorkerManager> workerMgr;

  //! Static buffer used by `baseZone` for the first block.
  uint64_t staticBuffer[2048 / sizeof(uint64_t)];

  //! \name Construction / Destruction
  //! \{

  BL_INLINE BLRasterContextImpl(BLContextVirt* virtIn) noexcept
    : baseZone(8192 - BLArenaAllocator::kBlockOverhead, 16, staticBuffer, sizeof(staticBuffer)),
      fetchDataPool(),
      savedStatePool(),
      dstImage {},
      dstData {},
      renderTargetInfo {},
      syncWorkData(this),
      pipeProvider(),
      contextOriginId(BLUniqueIdGenerator::generateId(BLUniqueIdGenerator::Domain::kContext)),
      stateIdCounter(0),
      renderingMode(uint8_t(BL_RASTER_RENDERING_MODE_SYNC)),
      workerMgrInitialized(false),
      contextFlags(0) {

    // Initializes BLRasterContext2DImpl.
    virt = virtIn;
    contextType = BL_CONTEXT_TYPE_RASTER;
    state = &internalState;
  }

  BL_INLINE ~BLRasterContextImpl() noexcept {
    destroyWorkerMgr();
  }

  //! \}

  //! \name Memory Management
  //! \{

  BL_INLINE BLArenaAllocator& fetchDataZone() noexcept { return baseZone; }
  BL_INLINE BLArenaAllocator& savedStateZone() noexcept { return baseZone; }

  BL_INLINE BLRasterEngine::RenderFetchData* allocFetchData() noexcept { return fetchDataPool.alloc(fetchDataZone()); }
  BL_INLINE void freeFetchData(BLRasterEngine::RenderFetchData* fetchData) noexcept { fetchDataPool.free(fetchData); }

  BL_INLINE BLRasterEngine::SavedState* allocSavedState() noexcept { return savedStatePool.alloc(savedStateZone()); }
  BL_INLINE void freeSavedState(BLRasterEngine::SavedState* state) noexcept { savedStatePool.free(state); }

  BL_INLINE void ensureWorkerMgr() noexcept {
    if (!workerMgrInitialized) {
      workerMgr.init();
      workerMgrInitialized = true;
    }
  }

  BL_INLINE void destroyWorkerMgr() noexcept {
    if (workerMgrInitialized) {
      workerMgr.destroy();
      workerMgrInitialized = false;
    }
  }

  //! \}

  //! \name Context Accessors
  //! \{

  BL_INLINE bool isSync() const noexcept {
    return renderingMode == BL_RASTER_RENDERING_MODE_SYNC;
  }

  BL_INLINE uint32_t format() const noexcept { return dstData.format; }
  BL_INLINE double fpScaleD() const noexcept { return renderTargetInfo.fpScaleD; }
  BL_INLINE double fullAlphaD() const noexcept { return renderTargetInfo.fullAlphaD; }

  BL_INLINE uint32_t bandCount() const noexcept { return syncWorkData.bandCount(); }
  BL_INLINE uint32_t bandHeight() const noexcept { return syncWorkData.bandHeight(); }

  //! \}

  //! \name State Accessors
  //! \{

  BL_INLINE uint8_t compOp() const noexcept { return internalState.compOp; }
  BL_INLINE uint8_t fillRule() const noexcept { return internalState.fillRule; }
  BL_INLINE const BLContextHints& hints() const noexcept { return internalState.hints; }
  BL_INLINE const BLApproximationOptions& approximationOptions() const noexcept { return internalState.approximationOptions; }

  BL_INLINE const BLStrokeOptions& strokeOptions() const noexcept { return internalState.strokeOptions.dcast(); }

  BL_INLINE uint32_t globalAlphaI() const noexcept { return internalState.globalAlphaI; }
  BL_INLINE double globalAlphaD() const noexcept { return internalState.globalAlpha; }

  BL_INLINE const BLRasterEngine::StyleData* getStyle(size_t index) const noexcept { return &internalState.style[index]; }

  BL_INLINE uint8_t metaMatrixType() const noexcept { return internalState.metaMatrixType; }
  BL_INLINE uint8_t metaMatrixFixedType() const noexcept { return internalState.metaMatrixFixedType; }

  BL_INLINE uint8_t finalMatrixType() const noexcept { return internalState.finalMatrixType; }
  BL_INLINE uint8_t finalMatrixFixedType() const noexcept { return internalState.finalMatrixFixedType; }

  BL_INLINE const BLMatrix2D& metaMatrix() const noexcept { return internalState.metaMatrix; }
  BL_INLINE const BLMatrix2D& metaMatrixFixed() const noexcept { return internalState.metaMatrixFixed; }

  BL_INLINE const BLMatrix2D& userMatrix() const noexcept { return internalState.userMatrix; }

  BL_INLINE const BLMatrix2D& finalMatrix() const noexcept { return internalState.finalMatrix; }
  BL_INLINE const BLMatrix2D& finalMatrixFixed() const noexcept { return internalState.finalMatrixFixed; }

  BL_INLINE const BLPointI& translationI() const noexcept { return internalState.translationI; }
  BL_INLINE void setTranslationI(const BLPointI& pt) noexcept { internalState.translationI = pt; }

  BL_INLINE const BLBoxI& metaClipBoxI() const noexcept { return internalState.metaClipBoxI; }
  BL_INLINE const BLBoxI& finalClipBoxI() const noexcept { return internalState.finalClipBoxI; }
  BL_INLINE const BLBox& finalClipBoxD() const noexcept { return internalState.finalClipBoxD; }

  BL_INLINE const BLBoxI& finalClipBoxFixedI() const noexcept { return syncWorkData.edgeBuilder._clipBoxI; }
  BL_INLINE const BLBox& finalClipBoxFixedD() const noexcept { return syncWorkData.edgeBuilder._clipBoxD; }
  BL_INLINE void setFinalClipBoxFixedD(const BLBox& clipBox) { syncWorkData.edgeBuilder.setClipBox(clipBox); }

  //! \}

  //! \name Error Accumulation
  //! \{

  BL_INLINE BLResult accumulateError(BLResult error) noexcept { return syncWorkData.accumulateError(error); }

  //! \}
};

BL_HIDDEN BLResult blRasterContextInitImpl(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* options) noexcept;
BL_HIDDEN void blRasterContextOnInit(BLRuntimeContext* rt) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCONTEXT_P_H_INCLUDED
