// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERCOMMAND_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERCOMMAND_P_H_INCLUDED

#include "../geometry_p.h"
#include "../pipeline/pipedefs_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/renderfetchdata_p.h"
#include "../raster/styledata_p.h"
#include "../support/arenaallocator_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

struct RenderFetchData;

//! Render command.
//!
//! Render command provides information required to render the lowest-level operation.
struct RenderCommand {
  //! \name Constants
  //! \{

  //! Render command type.
  enum Type : uint8_t {
    kTypeNone = 0,

    kTypeFillBoxA = 1,
    kTypeFillBoxU = 2,
    kTypeFillAnalytic = 3,

    kTypeFillMaskRaw = 4,
    kTypeFillMaskBoxA = 5,
    kTypeFillMaskBoxU = 6,
    kTypeFillMaskAnalytic = 7
  };

  //! Maximum size of the payload embedded in the \ref RenderCommand itself.
  enum PayloadDataSize : uint32_t { kPayloadDataSize = 32 };

  //! \}

  //! \name Payload
  //! \{

  //! FillBoxA, FillBoxU, FillMaskBoxA, FillMaskBoxU payload.
  struct FillBox {
    RenderFetchData* maskFetchData;
    uint8_t reserved[kPayloadDataSize - sizeof(BLBoxI) - sizeof(RenderFetchData*)];
    BLBoxI boxI;
  };

  //! FillMaskRaw payload - special case for aligned fills with aligned mask.
  //!
  //! This payload was designed to save space in command buffer as it avoids RenderFetchData.
  struct FillMaskRaw {
    BLImageImpl* maskImageI;
    BLPointI maskOffsetI;
    BLBoxI boxI;
  };

  //! FillAnalytic and FillMaskAnalytic payload, used by the synchronous rendering context implementation.
  struct FillAnalyticAny {
    RenderFetchData* maskFetchData;
    void* edgeData;
    uint32_t fillRule;
  };

  //! FillAnalytic and FillMaskAnalytic payload, used by the synchronous rendering context implementation.
  struct FillAnalyticSync {
    //! Fetch data used by mask `kTypeFillMaskAnalytic` command types.
    RenderFetchData* maskFetchData;
    //! Edge storage is used directly by the synchronous rendering context.
    EdgeStorage<int>* edgeStorage;
    //! Fill rule.
    uint32_t fillRule;
  };

  //! FillAnalytic and FillMaskAnalytic paylod, used by the asynchronous rendering context implementation.
  struct FillAnalyticAsync {
    //! Fetch data used by mask `kTypeFillMaskAnalytic` command types.
    RenderFetchData* maskFetchData;
    //! Points to the start of the first edge. Edges that start in next bands are linked next after edges of the previous
    //! band, which makes it possible to only store the start of the list.
    const EdgeVector<int>* edges;
    //! Fill rule.
    uint32_t fillRule;
    //! Topmost Y coordinate used to skip quickly the whole band if the worker is not there yet.
    int fixedY0;
    //! Index of state slot that is used by to keep track of the command progress. The index refers to a table where
    //! a command-specific state data is stored.
    uint32_t stateSlotIndex;
  };

  //! Command payload - each command type has a specific payload.
  union Payload {
    //! Payload used by FillBoxA, FillBoxU, FillMaskA, FillMaskU.
    FillBox box;
    //! Payload used by FillBoxAMaskA.
    FillMaskRaw maskRaw;
    //! Payload used by FillAnalytic in both synchronous and asynchronous case.
    FillAnalyticAny analyticAny;
    //! Payload used by FillAnalytic in case of synchronous rendering.
    FillAnalyticSync analyticSync;
    //! Payload used by FillAnalytic in case of asynchronous rendering.
    FillAnalyticAsync analyticAsync;

    //! Mask fetch-data, which is provided by the most commands.
    RenderFetchData* maskFetchData;

    //! Payload buffer (holds RAW data).
    uint8_t buffer[kPayloadDataSize];
  };

  BL_STATIC_ASSERT(sizeof(Payload) == kPayloadDataSize);
  BL_STATIC_ASSERT(sizeof(FillBox) == kPayloadDataSize);
  BL_STATIC_ASSERT(sizeof(FillMaskRaw) <= kPayloadDataSize);
  BL_STATIC_ASSERT(sizeof(FillAnalyticSync) <= kPayloadDataSize);
  BL_STATIC_ASSERT(sizeof(FillAnalyticAsync) <= kPayloadDataSize);

  //! \}

  //! \name Members
  //! \{

  //! Command payload.
  Payload _payload;

  //! Global alpha value.
  uint32_t _alpha;
  //! Command type, see \ref Type.
  uint8_t _type;
  //! Command flags, see `RenderCommandFlags`.
  uint8_t _flags;
  //! Reserved.
  uint16_t _reserved;

  //! Source data - either solid data or pointer to `fetchData`.
  StyleSource _source;

  //! Dispatch data.
  BLPipeline::DispatchData _dispatchData;

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
    _payload.box.boxI = boxA;
    _type = kTypeFillBoxA;
  }

  BL_INLINE void initFillBoxU(const BLBoxI& boxU) noexcept {
    _payload.box.boxI = boxU;
    _type = kTypeFillBoxU;
  }

  //! Initializes FillAnalytic command (synchronous).
  BL_INLINE void initFillAnalyticSync(uint32_t fillRule, EdgeStorage<int>* edgeStorage) noexcept {
    BL_ASSERT(fillRule <= BL_FILL_RULE_MAX_VALUE);

    _payload.analyticSync.edgeStorage = edgeStorage;
    _payload.analyticSync.fillRule = fillRule;
    _type = kTypeFillAnalytic;
  }

  //! Initializes FillAnalytic command (asynchronous).
  //!
  //! \note `edges` may be null in case that this command requires a job to build the edges. In this case both `edges`
  //! and `fixedY0` members will be changed when such job completes.
  BL_INLINE void initFillAnalyticAsync(uint32_t fillRule, EdgeVector<int>* edges) noexcept {
    BL_ASSERT(fillRule <= BL_FILL_RULE_MAX_VALUE);

    _payload.analyticAsync.edges = edges;
    _payload.analyticAsync.fixedY0 = 0;
    _payload.analyticAsync.fillRule = fillRule;
    _type = kTypeFillAnalytic;
  }

  BL_INLINE void initFillMaskRaw(const BLBoxI& boxA, const BLImageCore* maskImage, const BLPointI& maskOffsetI) noexcept {
    _payload.maskRaw.maskImageI = BLImagePrivate::getImpl(maskImage);
    _payload.maskRaw.maskOffsetI = maskOffsetI;
    _payload.maskRaw.boxI = boxA;
    _type = kTypeFillMaskRaw;
  }

  BL_INLINE void initFillMaskBoxA(const BLBoxI& boxA, RenderFetchData* maskFetchData) noexcept {
    _payload.box.maskFetchData = maskFetchData;
    _payload.box.boxI = boxA;
    _type = kTypeFillMaskBoxA;
  }

  BL_INLINE void initFillMaskBoxU(const BLBoxI& boxU, RenderFetchData* maskFetchData) noexcept {
    _payload.box.maskFetchData = maskFetchData;
    _payload.box.boxI = boxU;
    _type = kTypeFillMaskBoxU;
  }

  BL_INLINE void initFillMaskAnalyticSync(uint32_t fillRule, EdgeStorage<int>* edgeStorage, RenderFetchData* maskFetchData) noexcept {
    BL_ASSERT(fillRule <= BL_FILL_RULE_MAX_VALUE);

    _payload.analyticSync.maskFetchData = maskFetchData;
    _payload.analyticSync.edgeStorage = edgeStorage;
    _payload.analyticSync.fillRule = fillRule;
    _type = kTypeFillAnalytic;
  }

  BL_INLINE void initFillMaskAnalyticAsync(uint32_t fillRule, EdgeVector<int>* edges, RenderFetchData* maskFetchData) noexcept {
    BL_ASSERT(fillRule <= BL_FILL_RULE_MAX_VALUE);

    _payload.analyticAsync.maskFetchData = maskFetchData;
    _payload.analyticAsync.edges = edges;
    _payload.analyticAsync.fixedY0 = 0;
    _payload.analyticAsync.fillRule = fillRule;
    _type = kTypeFillAnalytic;
  }

  //! Sets edges of FillAnalytic or FillMaskAnalytic command.
  BL_INLINE void setEdgesAsync(EdgeStorage<int>* edgeStorage) noexcept {
    _payload.analyticAsync.edges = edgeStorage->flattenEdgeLinks();
    _payload.analyticAsync.fixedY0 = edgeStorage->boundingBox().y0;
  }

  //! \}

  //! \name Command Source and Mask Initialization
  //! \{

  BL_INLINE void initFetchSolid(const BLPipeline::FetchData::Solid& solidData) noexcept {
    _source.solid = solidData;
  }

  BL_INLINE void initFetchData(RenderFetchData* fetchData) noexcept {
    _source.fetchData = fetchData;
    _flags |= BL_RASTER_COMMAND_FLAG_FETCH_DATA;
  }

  BL_INLINE void initMaskFetchData(RenderFetchData* maskFetchData) noexcept {
    _payload.maskFetchData = maskFetchData;
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE uint32_t type() const noexcept { return _type; }

  BL_INLINE bool isFillBoxA() const noexcept { return _type == kTypeFillBoxA; }
  BL_INLINE bool isFillBoxU() const noexcept { return _type == kTypeFillBoxU; }
  BL_INLINE bool isFillAnalytic() const noexcept { return _type == kTypeFillAnalytic; }

  BL_INLINE bool isFillMaskRaw() const noexcept { return _type == kTypeFillMaskRaw; }
  BL_INLINE bool isFillMaskBoxA() const noexcept { return _type == kTypeFillMaskBoxA; }
  BL_INLINE bool isFillMaskBoxU() const noexcept { return _type == kTypeFillMaskBoxU; }
  BL_INLINE bool isFillMaskAnalytic() const noexcept { return _type == kTypeFillMaskAnalytic; }

  BL_INLINE uint32_t flags() const noexcept { return _flags; }
  BL_INLINE bool hasFlag(uint32_t flag) const noexcept { return (_flags & flag) != 0; }
  BL_INLINE bool hasFetchData() const noexcept { return hasFlag(BL_RASTER_COMMAND_FLAG_FETCH_DATA); }

  BL_INLINE uint32_t alpha() const noexcept { return _alpha; }
  BL_INLINE const BLBoxI& boxI() const noexcept { return _payload.box.boxI; }

  BL_INLINE uint32_t analyticFillRule() const noexcept {
    BL_ASSERT(isFillAnalytic() || isFillMaskAnalytic());
    return _payload.analyticAny.fillRule;
  }

  BL_INLINE const EdgeStorage<int>* analyticEdgesSync() const noexcept {
    BL_ASSERT(isFillAnalytic() || isFillMaskAnalytic());
    return _payload.analyticSync.edgeStorage;
  }

  BL_INLINE const EdgeVector<int>* analyticEdgesAsync() const noexcept {
    BL_ASSERT(isFillAnalytic() || isFillMaskAnalytic());
    return _payload.analyticAsync.edges;
  }

  //! Returns a pointer to `BLPipeline::FillData` that is only valid when the command type is `kTypeFillBoxA`. It casts
  //! `_rect` member to the requested data type as it's  compatible. This trick cannot be used for any other command types.
  BL_INLINE const void* getPipeFillDataOfBoxA() const noexcept {
    BL_ASSERT(_type == kTypeFillBoxA);
    return &_payload.box.boxI;
  }

  //! Returns `_solidData` or `_fetchData` casted properly to `BLPipeline::FetchData` type.
  BL_INLINE const void* getPipeFetchData() const noexcept {
    const void* data = &_source.solid;
    if (hasFetchData())
      data = &_source.fetchData->_data;
    return data;
  }

  BL_INLINE BLPipeline::DispatchData* pipeDispatchData() noexcept { return &_dispatchData; }
  BL_INLINE const BLPipeline::DispatchData* pipeDispatchData() const noexcept { return &_dispatchData; }

  //! \}
};

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERCOMMAND_P_H_INCLUDED
