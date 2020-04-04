// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERCONTEXT_P_H
#define BLEND2D_RASTER_RASTERCONTEXT_P_H

#include "../api-internal_p.h"
#include "../compop_p.h"
#include "../context_p.h"
#include "../font_p.h"
#include "../gradient_p.h"
#include "../matrix_p.h"
#include "../path_p.h"
#include "../piperuntime_p.h"
#include "../rgba.h"
#include "../region_p.h"
#include "../support_p.h"
#include "../zoneallocator_p.h"
#include "../raster/analyticrasterizer_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rastercommand_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/rasterfiller_p.h"
#include "../raster/rasterworkercontext_p.h"
#include "../raster/rasterworkermanager_p.h"

#if !defined(BL_BUILD_NO_JIT)
  #include "../pipegen/pipegenruntime_p.h"
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [BLRasterContextImpl]
// ============================================================================

//! Raster rendering context implementation.
class BLRasterContextImpl : public BLContextImpl {
public:
  //! Zone allocator used to allocate base data structures required by `BLRasterContextImpl`.
  BLZoneAllocator baseZone;
  //! Zone allocator used to allocate commands for deferred and asynchronous rendering.
  BLZoneAllocator cmdZone;
  //! Object pool used to allocate `BLRasterFetchData`.
  BLZonePool<BLRasterFetchData> fetchPool;
  //! Object pool used to allocate `BLRasterContextSavedState`.
  BLZonePool<BLRasterContextSavedState> statePool;

  //! Destination info.
  BLRasterContextDstInfo dstInfo;

  //! Worker used by synchronous rendering that also holds part of the current state.
  //!
  //! This worker is not used to do actual rendering in async mode, in such case it's
  //! only used to hold certain states that are only needed by the rendering context
  //! itself.
  BLRasterWorkerContext workerCtx;
  //! Thread manager (only used for deferred or asynchronous rendering).
  BLRasterWorkerManager workerMgr;

  //! Pipeline runtime (either global or isolated, depending on create options).
  BLPipeProvider pipeProvider;
  //! Pipeline lookup cache (always used before attempting to use `pipeProvider`.
  BLPipeLookupCache pipeLookupCache;

  //! Temporary text buffer used by high-level text rendering calls.
  BLGlyphBuffer glyphBuffer;

  //! Context origin ID used in `data0` member of `BLContextCookie`.
  uint64_t contextOriginId;
  //! Used to genearate unique IDs of this context.
  uint64_t stateIdCounter;

  //! The current state of the context accessible from outside.
  BLContextState currentState;
  //! Link to the previous saved state that will be restored by `BLContext::restore()`.
  BLRasterContextSavedState* savedState;

  //! Context flags.
  uint32_t contextFlags;

  //! Fixed point shift (able to multiply / divide by fpScale).
  int fpShiftI;
  //! Fixed point scale as int (either 256 or 65536).
  int fpScaleI;
  //! Fixed point mask calculated as `fpScaleI - 1`.
  int fpMaskI;

  //! Fixed point scale as double (either 256.0 or 65536.0).
  double fpScaleD;
  //! Minimum safe coordinate for integral transformation (scaled by 256.0 or 65536.0).
  double fpMinSafeCoordD;
  //! Maximum safe coordinate for integral transformation (scaled by 256.0 or 65536.0).
  double fpMaxSafeCoordD;
  //! Curve flattening tolerance scaled by `fpScaleD`.
  double toleranceFixedD;

  //! Fill and stroke styles.
  BLRasterContextStyleData style[BL_CONTEXT_OP_TYPE_COUNT];

  //! Composition operator simplification that matches the destination format and current `compOp`.
  const BLCompOpSimplifyInfo* compOpSimplifyTable;
  //! Solid format table used to select the best pixel format for solid fills.
  uint8_t solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_COUNT];

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

  //! Meta clip-box (int).
  BLBoxI metaClipBoxI;
  //! Final clip box (int).
  BLBoxI finalClipBoxI;
  //! Final clip-box (double).
  BLBox finalClipBoxD;

  //! Result of `(metaMatrix * userMatrix)`.
  BLMatrix2D finalMatrix;
  //! Meta matrix scaled by `fpScale`.
  BLMatrix2D metaMatrixFixed;
  //! Result of `(metaMatrix * userMatrix) * fpScale`.
  BLMatrix2D finalMatrixFixed;

  //! Integral offset to add to input coordinates in case integral transform is ok.
  BLPointI translationI;

  //! Static buffer used by `baseZone` for the first block.
  uint8_t staticBuffer[2048];

  inline BLRasterContextImpl(BLContextVirt* inVirt, uint16_t inMemPoolData) noexcept
    : baseZone(8192 - BLZoneAllocator::kBlockOverhead, 16, staticBuffer, sizeof(staticBuffer)),
      cmdZone(16384 - BLZoneAllocator::kBlockOverhead, 8),
      fetchPool(&baseZone),
      statePool(&baseZone),
      dstInfo {},
      workerCtx(this),
      workerMgr(),
      pipeProvider(),
      glyphBuffer(),
      contextOriginId(blContextIdGenerator.next()),
      stateIdCounter(0) {

    // Initializes BLContext2DImpl.
    virt = inVirt;
    refCount = 1;
    implType = uint8_t(BL_IMPL_TYPE_CONTEXT);
    implTraits = uint8_t(BL_IMPL_TRAIT_MUTABLE | BL_IMPL_TRAIT_VIRT);
    memPoolData = inMemPoolData;
    contextType = BL_CONTEXT_TYPE_RASTER;
    currentState.targetImage.impl = nullptr;
    state = &currentState;
  }

  inline ~BLRasterContextImpl() noexcept {}

  BL_INLINE const BLBox& finalClipBoxFixedD() const noexcept { return workerCtx.edgeBuilder._clipBoxD; }
  BL_INLINE void setFinalClipBoxFixedD(const BLBox& clipBox) { workerCtx.edgeBuilder.setClipBox(clipBox); }
};

// ============================================================================
// [BLRasterContextImpl - API]
// ============================================================================

BL_HIDDEN BLResult blRasterContextImplCreate(BLContextImpl** out, BLImageCore* image, const BLContextCreateInfo* options) noexcept;
BL_HIDDEN void blRasterContextRtInit(BLRuntimeContext* rt) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCONTEXT_P_H
