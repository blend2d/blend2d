// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERFILLER_P_H
#define BLEND2D_RASTER_RASTERFILLER_P_H

#include "../blcontext_p.h"
#include "../blgeometry_p.h"
#include "../blpipe_p.h"
#include "../blzoneallocator_p.h"
#include "../raster/bledgebuilder_p.h"
#include "../raster/blrasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [BLRasterFiller]
// ============================================================================

class BLRasterFiller {
public:
  typedef BLResult (BL_CDECL* WorkFunc)(BLRasterFiller* ctx, BLRasterWorker* worker, const BLRasterFetchData* fetchData) BL_NOEXCEPT;

  WorkFunc workFunc;
  BLPipeFillFunc fillFunc;
  BLPipeFillData fillData;
  BLPipeSignature fillSignature;
  BLEdgeStorage<int>* edgeStorage;

  BL_INLINE BLRasterFiller() noexcept
    : workFunc(nullptr),
      fillFunc(nullptr),
      fillSignature(0),
      edgeStorage(nullptr) {}

  BL_INLINE bool isValid() const noexcept { return this->fillSignature.value != 0; }
  BL_INLINE void reset() noexcept { this->fillSignature.reset(); }

  BL_INLINE void initBoxAA8bpc(uint32_t alpha, int x0, int y0, int x1, int y1) noexcept {
    this->workFunc = fillRectImpl;
    fillSignature.addFillType(
      fillData.initBoxAA8bpc(alpha, x0, y0, x1, y1));
  }

  BL_INLINE void initBoxAU8bpc24x8(uint32_t alpha, int x0, int y0, int x1, int y1) noexcept {
    this->workFunc = fillRectImpl;
    fillSignature.addFillType(
      fillData.initBoxAU8bpc24x8(alpha, x0, y0, x1, y1));
  }

  BL_INLINE void initAnalytic(uint32_t alpha, BLEdgeStorage<int>* edgeStorage, uint32_t fillRule) noexcept {
    this->workFunc = fillAnalyticImpl;
    this->fillData.analytic.alpha.u = alpha;
    this->fillData.analytic.fillRuleMask =
      fillRule == BL_FILL_RULE_NON_ZERO
        ? BL_PIPE_FILL_RULE_MASK_NON_ZERO
        : BL_PIPE_FILL_RULE_MASK_EVEN_ODD;
    this->fillSignature.addFillType(BL_PIPE_FILL_TYPE_ANALYTIC);
    this->edgeStorage = edgeStorage;
  }

  BL_INLINE void setFillFunc(BLPipeFillFunc fillFunc) noexcept {
    this->fillFunc = fillFunc;
  }

  BL_INLINE BLResult doWork(BLRasterWorker* worker, const BLRasterFetchData* fetchData) noexcept {
    return workFunc(this, worker, fetchData);
  }

  static BLResult BL_CDECL fillRectImpl(BLRasterFiller* ctx, BLRasterWorker* worker, const BLRasterFetchData* fetchData) noexcept;
  static BLResult BL_CDECL fillAnalyticImpl(BLRasterFiller* ctx, BLRasterWorker* worker, const BLRasterFetchData* fetchData) noexcept;
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERFILLER_P_H
