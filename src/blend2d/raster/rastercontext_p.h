// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RASTERCONTEXT_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERCONTEXT_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../compopinfo_p.h"
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

//! Preferred fill-rule (fastest) to use when the fill-rule doesn't matter.
//!
//! Since the filler doesn't care of fill-rule (it always uses the same code-path for non-zero and even-odd fills) it
//! doesn't really matter. However, if there is more rasterizers added in the future this can be adjusted to always
//! select the fastest one.
static constexpr const BLFillRule BL_RASTER_CONTEXT_PREFERRED_FILL_RULE = BL_FILL_RULE_EVEN_ODD;

//! Preferred extend mode (fastest) to use when blitting images. The extend mode can be either PAD or REFLECT as these
//! have the same effect on blits that are bound to the size of the image. We prefer REFLECT, because it's useful also
//! outside regular blits.
static constexpr const BLExtendMode BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND = BL_EXTEND_MODE_REFLECT;

//! Minimum size of a path (in vertices) to make it an asynchronous job. The reason for this threshold is that very
//! small paths actually do not benefit from being dispatched into a worker thread (the cost of serializing the job
//! is higher than the cost of processing that path in a user thread).
static constexpr const uint32_t BL_RASTER_CONTEXT_MINIMUM_ASYNC_PATH_SIZE = 10;

//! Maximum size of a text to be copied as is when dispatching asynchronous jobs. When the limit is reached the job
//! serialized would create a BLGlyphBuffer instead of making raw copy of the text, as the glyph-buffer has to copy
//! it anyway.
static constexpr const uint32_t BL_RASTER_CONTEXT_MAXIMUM_EMBEDDED_TEXT_SIZE = 256;

static constexpr const uint32_t BL_RASTER_CONTEXT_DEFAULT_SAVED_STATE_LIMIT = 4096;

static constexpr const uint32_t BL_RASTER_CONTEXT_DEFAULT_COMMAND_QUEUE_LIMIT = 10240;

//! Raster rendering context implementation (software accelerated).
class BLRasterContextImpl : public BLContextImpl {
public:
  BL_NONCOPYABLE(BLRasterContextImpl)

  //! \name Members
  //! \{

  //! Context flags.
  bl::RasterEngine::ContextFlags contextFlags;
  //! Rendering mode.
  uint8_t renderingMode;
  //! Whether workerMgr has been initialized.
  uint8_t workerMgrInitialized;
  //! Precision information.
  bl::RasterEngine::RenderTargetInfo renderTargetInfo;

  //! Work data used by synchronous rendering that also holds part of the current state. In async mode the work data
  //! can still be used by user thread in case it's allowed, otherwise it would only hold some states that are used
  //! by the rendering context directly.
  bl::RasterEngine::WorkData syncWorkData;

  //! Pipeline lookup cache (always used before attempting to use `pipeProvider`).
  bl::Pipeline::PipeLookupCache pipeLookupCache;

  //! Composition operator simplification that matches the destination format and current `compOp`.
  const bl::CompOpSimplifyInfo* compOpSimplifyInfo;
  //! Solid format table used to select the best pixel format for solid fills.
  uint8_t solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_COUNT];
  //! Table that can be used to override a fill/stroke color by one from SolidId (after a simplification).
  bl::RasterEngine::RenderFetchDataSolid* solidOverrideFillTable;
  //! Solid fill override table indexed by \ref bl::CompOpSolidId.
  bl::RasterEngine::RenderFetchDataHeader* solidFetchDataOverrideTable[uint32_t(bl::CompOpSolidId::kAlwaysNop) + 1u];

  //! The current state of the rendering context, the `BLContextState` part is public.
  bl::RasterEngine::RasterContextState internalState;
  //! Link to the previous saved state that will be restored by `BLContext::restore()`.
  bl::RasterEngine::SavedState* savedState;
  //! An actual shared fill-state (asynchronous rendering).
  bl::RasterEngine::SharedFillState* sharedFillState;
  //! An actual shared stroke-state (asynchronous rendering).
  bl::RasterEngine::SharedBaseStrokeState* sharedStrokeState;

  //! Zone allocator used to allocate base data structures required by `BLRasterContextImpl`.
  bl::ArenaAllocator baseZone;
  //! Object pool used to allocate `RenderFetchData`.
  bl::ArenaPool<bl::RasterEngine::RenderFetchData> fetchDataPool;
  //! Object pool used to allocate `SavedState`.
  bl::ArenaPool<bl::RasterEngine::SavedState> savedStatePool;

  //! Pipeline runtime (either global or isolated, depending on create-options).
  bl::Pipeline::PipeProvider pipeProvider;
  //! Worker manager (only used by asynchronous rendering context).
  bl::Wrap<bl::RasterEngine::WorkerManager> workerMgr;

  //! Context origin ID used in `data0` member of `BLContextCookie`.
  uint64_t contextOriginId;
  //! Used to generate unique IDs of this context.
  uint64_t stateIdCounter;

  //! The number of states that can be saved by `BLContext::save()` call.
  uint32_t savedStateLimit;

  //! Destination image.
  BLImageCore dstImage;
  //! Destination image data.
  BLImageData dstData;

  //! Minimum safe coordinate for integral transformation (scaled by 256.0 or 65536.0).
  double fpMinSafeCoordD;
  //! Maximum safe coordinate for integral transformation (scaled by 256.0 or 65536.0).
  double fpMaxSafeCoordD;

  //! Pointers to essential transformations that can be applied to styles.
  const BLMatrix2D* transformPtrs[BL_CONTEXT_STYLE_TRANSFORM_MODE_MAX_VALUE + 1u];

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLRasterContextImpl(BLContextVirt* virtIn, void* staticData, size_t staticSize) noexcept
    : contextFlags(bl::RasterEngine::ContextFlags::kNoFlagsSet),
      renderingMode(uint8_t(bl::RasterEngine::RenderingMode::kSync)),
      workerMgrInitialized(false),
      renderTargetInfo {},
      syncWorkData(this, nullptr),
      pipeLookupCache{},
      compOpSimplifyInfo{},
      solidFormatTable{},
      solidOverrideFillTable{},
      solidFetchDataOverrideTable{},
      savedState{},
      sharedFillState{},
      sharedStrokeState{},
      baseZone(8192 - bl::ArenaAllocator::kBlockOverhead, 16, staticData, staticSize),
      fetchDataPool(),
      savedStatePool(),
      pipeProvider(),
      contextOriginId(BLUniqueIdGenerator::generateId(BLUniqueIdGenerator::Domain::kContext)),
      stateIdCounter(0),
      savedStateLimit(0),
      dstImage{},
      dstData{},
      fpMinSafeCoordD(0.0),
      fpMaxSafeCoordD(0.0) {

    // Initializes BLRasterContext2DImpl.
    virt = virtIn;
    contextType = BL_CONTEXT_TYPE_RASTER;
    state = &internalState;
    transformPtrs[BL_CONTEXT_STYLE_TRANSFORM_MODE_USER] = &internalState.finalTransform;
    transformPtrs[BL_CONTEXT_STYLE_TRANSFORM_MODE_META] = &internalState.metaTransform;
    transformPtrs[BL_CONTEXT_STYLE_TRANSFORM_MODE_NONE] = &bl::TransformInternal::identityTransform;
  }

  BL_INLINE ~BLRasterContextImpl() noexcept {
    destroyWorkerMgr();
  }

  //! \}

  //! \name Memory Management
  //! \{

  BL_INLINE_NODEBUG bl::ArenaAllocator& fetchDataZone() noexcept { return baseZone; }
  BL_INLINE_NODEBUG bl::ArenaAllocator& savedStateZone() noexcept { return baseZone; }

  BL_INLINE bl::RasterEngine::RenderFetchData* allocFetchData() noexcept { return fetchDataPool.alloc(fetchDataZone()); }
  BL_INLINE void freeFetchData(bl::RasterEngine::RenderFetchData* fetchData) noexcept { fetchDataPool.free(fetchData); }

  BL_INLINE bl::RasterEngine::SavedState* allocSavedState() noexcept { return savedStatePool.alloc(savedStateZone()); }
  BL_INLINE void freeSavedState(bl::RasterEngine::SavedState* state) noexcept { savedStatePool.free(state); }

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

  BL_INLINE_NODEBUG bool isSync() const noexcept { return renderingMode == uint8_t(bl::RasterEngine::RenderingMode::kSync); }

  BL_INLINE_NODEBUG bl::FormatExt format() const noexcept { return bl::FormatExt(dstData.format); }
  BL_INLINE_NODEBUG double fpScaleD() const noexcept { return renderTargetInfo.fpScaleD; }
  BL_INLINE_NODEBUG double fullAlphaD() const noexcept { return renderTargetInfo.fullAlphaD; }

  BL_INLINE_NODEBUG uint32_t bandCount() const noexcept { return syncWorkData.bandCount(); }
  BL_INLINE_NODEBUG uint32_t bandHeight() const noexcept { return syncWorkData.bandHeight(); }
  BL_INLINE_NODEBUG uint32_t commandQuantizationShiftAA() const noexcept { return syncWorkData.commandQuantizationShiftAA(); }
  BL_INLINE_NODEBUG uint32_t commandQuantizationShiftFp() const noexcept { return syncWorkData.commandQuantizationShiftFp(); }

  //! \}

  //! \name State Accessors
  //! \{

  BL_INLINE_NODEBUG uint8_t clipMode() const noexcept { return syncWorkData.clipMode; }

  BL_INLINE_NODEBUG uint8_t compOp() const noexcept { return internalState.compOp; }
  BL_INLINE_NODEBUG BLFillRule fillRule() const noexcept { return BLFillRule(internalState.fillRule); }
  BL_INLINE_NODEBUG const BLContextHints& hints() const noexcept { return internalState.hints; }

  BL_INLINE_NODEBUG const BLStrokeOptions& strokeOptions() const noexcept { return internalState.strokeOptions.dcast(); }
  BL_INLINE_NODEBUG const BLApproximationOptions& approximationOptions() const noexcept { return internalState.approximationOptions; }

  BL_INLINE_NODEBUG uint32_t globalAlphaI() const noexcept { return internalState.globalAlphaI; }
  BL_INLINE_NODEBUG double globalAlphaD() const noexcept { return internalState.globalAlpha; }

  BL_INLINE_NODEBUG const bl::RasterEngine::StyleData* getStyle(size_t index) const noexcept { return &internalState.style[index]; }

  BL_INLINE_NODEBUG const BLMatrix2D& metaTransform() const noexcept { return internalState.metaTransform; }
  BL_INLINE_NODEBUG BLTransformType metaTransformType() const noexcept { return BLTransformType(internalState.metaTransformType); }

  BL_INLINE_NODEBUG const BLMatrix2D& metaTransformFixed() const noexcept { return internalState.metaTransformFixed; }
  BL_INLINE_NODEBUG BLTransformType metaTransformFixedType() const noexcept { return BLTransformType(internalState.metaTransformFixedType); }

  BL_INLINE_NODEBUG const BLMatrix2D& userTransform() const noexcept { return internalState.userTransform; }

  BL_INLINE_NODEBUG const BLMatrix2D& finalTransform() const noexcept { return internalState.finalTransform; }
  BL_INLINE_NODEBUG BLTransformType finalTransformType() const noexcept { return BLTransformType(internalState.finalTransformType); }

  BL_INLINE_NODEBUG const BLMatrix2D& finalTransformFixed() const noexcept { return internalState.finalTransformFixed; }
  BL_INLINE_NODEBUG BLTransformType finalTransformFixedType() const noexcept { return BLTransformType(internalState.finalTransformFixedType); }

  BL_INLINE_NODEBUG const BLPointI& translationI() const noexcept { return internalState.translationI; }
  BL_INLINE_NODEBUG void setTranslationI(const BLPointI& pt) noexcept { internalState.translationI = pt; }

  BL_INLINE_NODEBUG const BLBoxI& metaClipBoxI() const noexcept { return internalState.metaClipBoxI; }
  BL_INLINE_NODEBUG const BLBoxI& finalClipBoxI() const noexcept { return internalState.finalClipBoxI; }
  BL_INLINE_NODEBUG const BLBox& finalClipBoxD() const noexcept { return internalState.finalClipBoxD; }

  BL_INLINE_NODEBUG const BLBoxI& finalClipBoxFixedI() const noexcept { return syncWorkData.edgeBuilder._clipBoxI; }
  BL_INLINE_NODEBUG const BLBox& finalClipBoxFixedD() const noexcept { return syncWorkData.edgeBuilder._clipBoxD; }
  BL_INLINE_NODEBUG void setFinalClipBoxFixedD(const BLBox& clipBox) { syncWorkData.edgeBuilder.setClipBox(clipBox); }

  //! \}

  //! \name Error Accumulation
  //! \{

  BL_INLINE_NODEBUG BLResult accumulateError(BLResult error) noexcept { return syncWorkData.accumulateError(error); }

  //! \}
};

BL_HIDDEN BLResult blRasterContextInitImpl(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* options) noexcept;
BL_HIDDEN void blRasterContextOnInit(BLRuntimeContext* rt) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCONTEXT_P_H_INCLUDED
