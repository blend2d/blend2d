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

namespace bl {
namespace RasterEngine {

struct RenderFetchData;

//! Source data that belongs to a \ref RenderCommand, but stored separately.
union RenderCommandSource {
  //! Solid data.
  Pipeline::FetchData::Solid solid;
  //! Fetch data.
  RenderFetchData* fetchData;

  //! Reset all data to zero.
  BL_INLINE_NODEBUG void reset() noexcept { solid.prgb64 = 0; }
  //! Copy all data from `other` to this command source.
  BL_INLINE_NODEBUG void reset(const RenderCommandSource& other) noexcept { *this = other; }
};

//! Render command type.
enum class RenderCommandType : uint8_t {
  kNone = 0,
  kFillBoxA = 1,
  kFillBoxU = 2,
  kFillAnalytic = 3,
  kFillBoxMaskA = 4
};

//! Raster command flags.
enum class RenderCommandFlags : uint8_t {
  //! No flags specified.
  kNoFlags = 0x00u,

  //! The command holds `_source.fetchData` (the operation is non-solid, fetch-data is valid and used).
  kHasStyleFetchData = 0x10u,

  //! The command retains `_source.fetchData`, which must be released during batch finalization.
  kRetainsStyleFetchData = 0x20u,

  //! The command retains `_payload.maskRaw.maskImageI`, which must be released during batch finalization.
  //!
  //! \note This flag cannot be set together with \ref kRetainsMaskImageData, one or the other.
  kRetainsMaskFetchData = 0x40u,

  //! The command retains `_payload.maskFetchData`, which must be released during batch finalization
  //!
  //! \note This flag cannot be set together with \ref kRetainsMaskFetchData, one or the other.
  kRetainsMaskImageData = 0x80u
};

BL_DEFINE_ENUM_FLAGS(RenderCommandFlags)

//! 64-bit pointer to unify the layout of the render command.
//!
//! The reason is that a command has a fixed size calculated to be good for 8-byte pointers (64-bit machines).
template<typename T>
struct Ptr64 {
  T* ptr;
#if BL_TARGET_ARCH_BITS < 64
  uint32_t padding;
#endif
};

//! Render command.
//!
//! Render command provides information required to render the lowest-level operation.
struct RenderCommand {
  //! \name Constants
  //! \{

  //! Maximum size of the payload embedded in the \ref RenderCommand itself.
  enum PayloadDataSize : uint32_t { kPayloadDataSize = 32 };

  //! \}

  //! \name Payload
  //! \{

  //! FillBoxA, FillBoxU, FillMaskBoxA, FillMaskBoxU payload.
  struct FillBox {
    Ptr64<RenderFetchData> maskFetchData;
    uint8_t reserved[8];
    BLBoxI boxI;
  };

  //! FillBoxWithMaskA payload - special case for aligned fills with aligned mask.
  //!
  //! This payload was designed to save space in command buffer as it avoids RenderFetchData.
  struct FillBoxMaskA {
    Ptr64<BLImageImpl> maskImageI;
    BLPointI maskOffsetI;
    BLBoxI boxI;
  };

  //! FillAnalytic and FillMaskAnalytic payload, used by the asynchronous rendering context implementation.
  struct FillAnalytic {
    //! Fetch data used by mask `kTypeFillMaskAnalytic` command types.
    Ptr64<RenderFetchData> maskFetchData;
    //! Points to the start of the first edge. Edges that start in next bands are linked next after edges of the previous
    //! band, which makes it possible to only store the start of the list.
    Ptr64<const EdgeVector<int>> edges;
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
    FillBoxMaskA boxMaskA;
    //! Payload used by FillAnalytic in case of asynchronous rendering.
    FillAnalytic analytic;

    //! Mask fetch-data, which is provided by the most commands.
    Ptr64<RenderFetchData> maskFetchData;

    //! Payload buffer (holds RAW data).
    uint8_t buffer[kPayloadDataSize];
  };

  BL_STATIC_ASSERT(sizeof(Payload) == kPayloadDataSize);
  BL_STATIC_ASSERT(sizeof(FillBox) == kPayloadDataSize);
  BL_STATIC_ASSERT(sizeof(FillBoxMaskA) <= kPayloadDataSize);
  BL_STATIC_ASSERT(sizeof(FillAnalytic) <= kPayloadDataSize);

  //! \}

  //! \name Members
  //! \{

  //! Command payload.
  Payload _payload;

  //! Global alpha value.
  uint32_t _alpha;
  //! Command type.
  RenderCommandType _type;
  //! Command flags.
  RenderCommandFlags _flags;
  //! Reserved.
  uint16_t _reserved;

  RenderCommandSource _source;

  union {
    //! Dispatch data.
    Pipeline::DispatchData _dispatchData;
    //! Signature, used during command construction, replaced by `_dispatchData` when constructed.
    Pipeline::Signature _signature;
  };

  //! \}

  //! \name Command Core Initialization
  //! \{

  BL_INLINE void initCommand(uint32_t alpha) noexcept {
    _alpha = alpha;
    _type = RenderCommandType::kNone;
    _flags = RenderCommandFlags::kNoFlags;
    _reserved = 0;
  }

  BL_INLINE void initFillBoxA(const BLBoxI& boxA) noexcept {
    _payload.box.boxI = boxA;
    _type = RenderCommandType::kFillBoxA;
  }

  BL_INLINE void initFillBoxU(const BLBoxI& boxU) noexcept {
    _payload.box.boxI = boxU;
    _type = RenderCommandType::kFillBoxU;
  }

  //! Initializes FillAnalytic command.
  //!
  //! \note `edges` may be null in case that this command requires a job to build the edges. In this case both `edges`
  //! and `fixedY0` members will be changed when such job completes.
  BL_INLINE void initFillAnalytic(EdgeVector<int>* edges, int fixedY0, BLFillRule fillRule) noexcept {
    BL_ASSERT(fillRule <= BL_FILL_RULE_MAX_VALUE);

    _payload.analytic.edges.ptr = edges;
    _payload.analytic.fixedY0 = fixedY0;
    _payload.analytic.fillRule = uint32_t(fillRule);
    _type = RenderCommandType::kFillAnalytic;
  }

  BL_INLINE void initFillBoxMaskA(const BLBoxI& boxA, const BLImageCore* maskImage, const BLPointI& maskOffsetI) noexcept {
    _payload.boxMaskA.maskImageI.ptr = ImageInternal::getImpl(maskImage);
    _payload.boxMaskA.maskOffsetI = maskOffsetI;
    _payload.boxMaskA.boxI = boxA;
    _type = RenderCommandType::kFillBoxMaskA;
  }

  //! Sets edges of FillAnalytic or FillMaskAnalytic command.
  BL_INLINE void setAnalyticEdges(EdgeStorage<int>* edgeStorage) noexcept {
    _payload.analytic.edges.ptr = edgeStorage->flattenEdgeLinks();
    _payload.analytic.fixedY0 = edgeStorage->boundingBox().y0;
  }

  BL_INLINE void markFetchData() noexcept { addFlags(RenderCommandFlags::kHasStyleFetchData); }

  //! \}

  //! \name Command Source and Mask Initialization
  //! \{

  BL_INLINE void initMaskFetchData(RenderFetchData* maskFetchData) noexcept {
    _payload.maskFetchData.ptr = maskFetchData;
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG RenderCommandType type() const noexcept { return RenderCommandType(_type); }

  BL_INLINE_NODEBUG bool isFillBoxA() const noexcept { return _type == RenderCommandType::kFillBoxA; }
  BL_INLINE_NODEBUG bool isFillBoxU() const noexcept { return _type == RenderCommandType::kFillBoxU; }
  BL_INLINE_NODEBUG bool isFillAnalytic() const noexcept { return _type == RenderCommandType::kFillAnalytic; }
  BL_INLINE_NODEBUG bool isFillBoxMaskA() const noexcept { return _type == RenderCommandType::kFillBoxMaskA; }

  BL_INLINE_NODEBUG RenderCommandFlags flags() const noexcept { return RenderCommandFlags(_flags); }
  BL_INLINE_NODEBUG bool hasFlag(RenderCommandFlags flag) const noexcept { return uint32_t(_flags & flag) != 0; }
  BL_INLINE_NODEBUG void addFlags(RenderCommandFlags flags) noexcept { _flags |= flags; }

  BL_INLINE_NODEBUG bool hasStyleFetchData() const noexcept { return hasFlag(RenderCommandFlags::kHasStyleFetchData); }
  BL_INLINE_NODEBUG bool retainsStyleFetchData() const noexcept { return hasFlag(RenderCommandFlags::kRetainsStyleFetchData); }
  BL_INLINE_NODEBUG bool retainsMask() const noexcept { return hasFlag(RenderCommandFlags::kRetainsMaskImageData | RenderCommandFlags::kRetainsMaskFetchData); }
  BL_INLINE_NODEBUG bool retainsMaskImageData() const noexcept { return hasFlag(RenderCommandFlags::kRetainsMaskImageData); }
  BL_INLINE_NODEBUG bool retainsMaskFetchData() const noexcept { return hasFlag(RenderCommandFlags::kRetainsMaskFetchData); }

  BL_INLINE_NODEBUG uint32_t alpha() const noexcept { return _alpha; }
  BL_INLINE_NODEBUG const BLBoxI& boxI() const noexcept { return _payload.box.boxI; }

  BL_INLINE uint32_t analyticFillRule() const noexcept {
    BL_ASSERT(isFillAnalytic());
    return _payload.analytic.fillRule;
  }

  BL_INLINE const EdgeVector<int>* analyticEdges() const noexcept {
    BL_ASSERT(isFillAnalytic());
    return _payload.analytic.edges.ptr;
  }

  //! Returns a pointer to `Pipeline::FillData` that is only valid when the command type is `kTypeFillBoxA`. It casts
  //! `_rect` member to the requested data type as it's  compatible. This trick cannot be used for any other command types.
  BL_INLINE const void* getPipeFillDataOfBoxA() const noexcept {
    BL_ASSERT(isFillBoxA());
    return &_payload.box.boxI;
  }

  //! Returns `_solidData` or `_fetchData` casted properly to `Pipeline::FetchData` type.
  BL_INLINE const void* getPipeFetchData() const noexcept {
    const void* data = &_source.solid;
    if (hasStyleFetchData())
      data = &_source.fetchData->pipelineData;
    return data;
  }

  BL_INLINE_NODEBUG Pipeline::DispatchData* pipeDispatchData() noexcept { return &_dispatchData; }
  BL_INLINE_NODEBUG const Pipeline::DispatchData* pipeDispatchData() const noexcept { return &_dispatchData; }

  //! \}
};

} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERCOMMAND_P_H_INCLUDED
