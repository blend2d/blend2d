// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERCOMMAND_P_H
#define BLEND2D_RASTER_RASTERCOMMAND_P_H

#include "../geometry_p.h"
#include "../pipedefs_p.h"
#include "../zoneallocator_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/rastercontextstyle_p.h"
#include "../raster/rasterfetchdata_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct BLRasterFetchData;

// ============================================================================
// [Constants]
// ============================================================================

//! Raster command type.
enum BLRasterCommandType : uint32_t {
  BL_RASTER_COMMAND_TYPE_NONE = 0,

  BL_RASTER_COMMAND_TYPE_FILL_BOX_A = 1,
  BL_RASTER_COMMAND_TYPE_FILL_BOX_U = 2,

  BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_BASE = 3,
  BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_NON_ZERO = BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_BASE + BL_FILL_RULE_NON_ZERO,
  BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_EVEN_ODD = BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_BASE + BL_FILL_RULE_EVEN_ODD
};

// ============================================================================
// [BLRasterCommand]
// ============================================================================

//! Raster command data.
struct BLRasterCommand {
  //! \name Command Core Data
  //! \{

  //! Analytic rasterizer data used by the synchronous rendering context.
  struct AnalyticSync {
    //! Edge storage is used directly by the synchronous rendering context.
    BLEdgeStorage<int>* edgeStorage;
  };

  //! Analytic rasterizer data used by the asynchronous rendering context.
  struct AnalyticAsync {
    //! Points to the start of the first edge. Edges that start in next
    //! bands are linked next after edges of the previous band, which makes
    //! it possible to only store the start of the list.
    const BLEdgeVector<int>* edges;
    //! Topmost Y coordinate used to skip quickly the whole band if the worker
    //! is not there yet.
    int fixedY0;
    //! Index of state slot that is used by to keep track of the command progress,
    //! The index refers to a table where a command-specific state data is stored.
    uint32_t stateSlotIndex;
  };

  //! Either rectangular data or data for analytic rasterizer depending on
  //! the command type.
  union {
    //! Data used by FillBoxA and FillBoxU.
    BLBoxI _boxI;
    //! Data used by analytic rasterizer in case of synchronous rendering.
    AnalyticSync _analyticSync;
    //! Data used by analytic rasterizer in case of asynchronous rendering.
    AnalyticAsync _analyticAsync;
  };

  //! Global alpha value.
  uint32_t _alpha;
  //! Command type, see `BLRasterCommandType`.
  uint8_t _type;
  //! Command flags, see `BLRasterCommandFlags`.
  uint8_t _flags;
  //! Reserved.
  uint16_t _reserved;

  //! \}

  //! \name Command Source Data
  //! \{

  //! Source data - either solid data or pointer to `fetchData`.
  BLRasterContextStyleSource _source;

  //! \}

  //! \name Command Pipeline Data
  //! \{

  union {
    //! Pipeline fill function.
    //!
    //! Always filled by a synchronous rendering context. Asynchronous rendering
    //! context would only fill the function if it's available or use this address
    //! as `_fillPrev` in case that asynchronous pipeline compilation is enabled.
    BLPipeFillFunc _fillFunc;
    //! Link to the previous command that uses the same fill function (with the
    //! same signature). This is only used by asynchronous rendering context to
    //! make it possible to patch all commands that use the same fill function
    //! after the function is compiled by a worker thread.
    BLRasterCommand* _fillPrev;
  };

  //! \}

  //! \name Command Core Initialization
  //! \{

  BL_INLINE void initCommand(uint32_t alpha) noexcept {
    _alpha = alpha;
    _type = 0;
    _flags = 0;
    _reserved = 0;
  }

  BL_INLINE void initFillBoxA(const BLBoxI& boxA) noexcept {
    _boxI = boxA;
    _type = BL_RASTER_COMMAND_TYPE_FILL_BOX_A;
  }

  BL_INLINE void initFillBoxU(const BLBoxI& boxU) noexcept {
    _boxI = boxU;
    _type = BL_RASTER_COMMAND_TYPE_FILL_BOX_U;
  }

  BL_INLINE void initFillAnalyticSync(uint32_t fillRule, BLEdgeStorage<int>* edgeStorage) noexcept {
    BL_ASSERT(fillRule < BL_FILL_RULE_COUNT);

    _analyticSync.edgeStorage = edgeStorage;
    _type = uint8_t(BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_BASE + fillRule);
  }

  //! Initialize the command (asynchronous)
  //!
  //! \note `edges` may be null in case that this command requires a job to build the
  //! edges. In this case both `edges` and `fixedY0` members will be changed when such
  //! job completes.
  BL_INLINE void initFillAnalyticAsync(uint32_t fillRule, BLEdgeVector<int>* edges) noexcept {
    BL_ASSERT(fillRule < BL_FILL_RULE_COUNT);

    _analyticAsync.edges = edges;
    _analyticAsync.fixedY0 = 0;
    _type = uint8_t(BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_BASE + fillRule);
  }

  BL_INLINE void setEdgesAsync(BLEdgeStorage<int>* edgeStorage) noexcept {
    _analyticAsync.edges = edgeStorage->flattenEdgeLinks();
    _analyticAsync.fixedY0 = edgeStorage->boundingBox().y0;
  }

  //! \}

  //! \name Command Source Initialization
  //! \{

  BL_INLINE void initFetchSolid(const BLPipeFetchData::Solid& solidData) noexcept {
    _source.solid = solidData;
  }

  BL_INLINE void initFetchData(BLRasterFetchData* fetchData) noexcept {
    _source.fetchData = fetchData;
    _flags |= BL_RASTER_COMMAND_FLAG_FETCH_DATA;
  }

  //! \}

  //! \name Command Pipeline Initialization
  //! \{

  BL_INLINE void initFillFunc(BLPipeFillFunc fillFunc) noexcept {
    _fillFunc = fillFunc;
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE uint32_t type() const noexcept { return _type; }
  BL_INLINE uint32_t flags() const noexcept { return _flags; }
  BL_INLINE uint32_t alpha() const noexcept { return _alpha; }
  BL_INLINE BLPipeFillFunc fillFunc() const noexcept { return _fillFunc; }

  BL_INLINE bool isBoxA() const noexcept { return type() == BL_RASTER_COMMAND_TYPE_FILL_BOX_A; }
  BL_INLINE bool isBoxU() const noexcept { return type() == BL_RASTER_COMMAND_TYPE_FILL_BOX_U; }
  BL_INLINE const BLBoxI& boxI() const noexcept { return _boxI; }

  BL_INLINE bool isAnalytic() const noexcept {
    return type() >= BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_BASE &&
           type() <  BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_BASE + BL_FILL_RULE_COUNT;
  }

  BL_INLINE bool hasFlag(uint32_t flag) const noexcept { return (_flags & flag) != 0; }
  BL_INLINE bool hasFetchData() const noexcept { return hasFlag(BL_RASTER_COMMAND_FLAG_FETCH_DATA); }

  BL_INLINE uint32_t analyticFillRule() const noexcept {
    BL_ASSERT(isAnalytic());
    return type() - BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_BASE;
  }

  BL_INLINE const BLEdgeStorage<int>* analyticEdgesSync() const noexcept {
    BL_ASSERT(isAnalytic());
    return _analyticSync.edgeStorage;
  }

  BL_INLINE const BLEdgeVector<int>* analyticEdgesAsync() const noexcept {
    return _analyticAsync.edges;
  }

  //! Returns a pointer to `BLPipeFillData` that is only valid when the command type
  //! is `BL_RASTER_COMMAND_TYPE_FILL_BOX_A`. It casts `_rect` member to the requested
  //! data type as it's compatible. This trick cannot be used for any other command
  //! types.
  BL_INLINE const void* getPipeFillDataOfBoxA() const noexcept {
    BL_ASSERT(type() == BL_RASTER_COMMAND_TYPE_FILL_BOX_A);
    return &_boxI;
  }

  //! Returns `_solidData` or `_fetchData` casted properly to `BLPipeFetchData` type.
  BL_INLINE const void* getPipeFetchData() const noexcept {
    const void* data = &_source.solid;
    if (hasFetchData())
      data = &_source.fetchData->_data;
    return data;
  }

  //! \}
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCOMMAND_P_H
