// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERCOMMANDSERIALIZER_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERCOMMANDSERIALIZER_P_H_INCLUDED

#include "../raster/rendercommand_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/workermanager_p.h"
#include "../support/arenaallocator_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

enum class RenderCommandSerializerFlags : uint32_t {
  kNone = 0u,
  kBlit = 0x00000001u,
  kMask = 0x00000002u,
  kSolid = 0x00000004u,

  kMaxUInt = 0xFFFFFFFFu
};
BL_DEFINE_ENUM_FLAGS(RenderCommandSerializerFlags)

//! Abstracts storage used by the serializer.
//!
//! The reason we need a storage is simple - asynchronous rendering requres to serialize each rendering operation into
//! the respective command, which requres a dynamically allocated storage. However, synchronous rendering doesn't need
//! such storage and the command is actually allocated on stack (and we expect that the compiler actually optimizes
//! the access to such stack).
template<uint32_t RenderingMode>
struct RenderCommandSerializerStorage;

template<>
struct RenderCommandSerializerStorage<BL_RASTER_RENDERING_MODE_SYNC> {
  enum : uint32_t { kRenderingMode = BL_RASTER_RENDERING_MODE_SYNC };

  //! Command data passed to a synchronous command processor.
  RenderCommand _command;

  BL_INLINE BLResult initSerializer(BLRasterContextImpl* ctxI) noexcept {
    blUnused(ctxI);
    return BL_SUCCESS;
  }

  BL_INLINE constexpr bool isSync() const { return true; }
  BL_INLINE constexpr bool isAsync() const { return false; }

  BL_INLINE RenderCommand& command() noexcept { return _command; }
  BL_INLINE const RenderCommand& command() const noexcept { return _command; }
};

template<>
struct RenderCommandSerializerStorage<BL_RASTER_RENDERING_MODE_ASYNC> {
  enum : uint32_t { kRenderingMode = BL_RASTER_RENDERING_MODE_ASYNC };

  //! Command data pointing to a command in the command queue.
  RenderCommand* _command;

  BL_INLINE constexpr bool isSync() const { return false; }
  BL_INLINE constexpr bool isAsync() const { return true; }

  BL_INLINE BLResult initSerializer(BLRasterContextImpl* ctxI) noexcept {
    WorkerManager& mgr = ctxI->workerMgr();

    BL_PROPAGATE(mgr.ensureCommandQueue());
    _command = mgr.currentCommandData();
    ctxI->syncWorkData.saveState();

    return BL_SUCCESS;
  }

  BL_INLINE RenderCommand& command() noexcept { return *_command; }
  BL_INLINE const RenderCommand& command() const noexcept { return *_command; }

  BL_INLINE bool enqueued(BLRasterContextImpl* ctxI) const noexcept {
    return _command != ctxI->workerMgr().currentCommandData();
  }
};

//! Used to build and serialize a core rendering command.
//!
//! Initialization order:
//!   1. initSerializer()
//!   2. initPipeline()
//!   3. initCommand()
//!   4. Others in any order.
template<uint32_t RenderingMode>
struct RenderCommandSerializerCore : public RenderCommandSerializerStorage<RenderingMode> {
  using RenderCommandSerializerStorage<RenderingMode>::command;

  //! Pipeline signature.
  BLPipeline::Signature _pipeSignature;
  //! Style data to use when `_fetchData` is not available.
  const StyleData* _styleData;

  BL_INLINE BLPipeline::Signature& pipeSignature() noexcept { return _pipeSignature; }
  BL_INLINE const BLPipeline::Signature& pipeSignature() const noexcept { return _pipeSignature; }

  BL_INLINE const StyleData* styleData() const noexcept { return _styleData; }
  BL_INLINE void setStyleData(const StyleData* styleData) noexcept { _styleData = styleData; }

  BL_INLINE void initPipeline(const BLPipeline::Signature& signature) noexcept {
    pipeSignature().reset(signature);
    setStyleData(nullptr);
  }

  BL_INLINE void initCommand(uint32_t alpha) noexcept {
    command().initCommand(alpha);
  }

  BL_INLINE void initFillBoxA(const BLBoxI& boxA) noexcept {
    command().initFillBoxA(boxA);
    pipeSignature().addFillType(BLPipeline::FillType::kBoxA);
  }

  BL_INLINE void initFillBoxU(const BLBoxI& boxU) noexcept {
    command().initFillBoxU(boxU);
#ifdef BL_USE_MASKS
    pipeSignature().addFillType(BLPipeline::FillType::kMask);
#else
    pipeSignature().addFillType(BLPipeline::FillType::kBoxU);
#endif
  }

  BL_INLINE void initFillMaskRaw(const BLBoxI& boxA, const BLImageCore* maskImage, const BLPointI& maskOffset) noexcept {
    command().initFillMaskRaw(boxA, maskImage, maskOffset);
    pipeSignature().addFillType(BLPipeline::FillType::kMask);
  }

  BL_INLINE void initFillAnalyticSync(uint32_t fillRule, EdgeStorage<int>* edgeStorage) noexcept {
    command().initFillAnalyticSync(fillRule, edgeStorage);
    pipeSignature().addFillType(BLPipeline::FillType::kAnalytic);
  }

  BL_INLINE void initFillAnalyticAsync(uint32_t fillRule, EdgeVector<int>* edges) noexcept {
    command().initFillAnalyticAsync(fillRule, edges);
    pipeSignature().addFillType(BLPipeline::FillType::kAnalytic);
  }

  BL_INLINE void initFetchSolid(const BLPipeline::FetchData::Solid& solidData) noexcept {
    command().initFetchSolid(solidData);
  }

  BL_INLINE void initFetchDataFromStyle(const StyleData* styleData) noexcept {
    command()._flags |= styleData->cmdFlags;
    command()._source = styleData->source;
    setStyleData(styleData);
  }

  BL_INLINE void clearFetchFlags() noexcept {
    command()._flags &= ~BL_RASTER_COMMAND_FLAG_FETCH_DATA;
  }

  BL_INLINE BLPipeline::DispatchData* pipeDispatchData() noexcept { return command().dispatchData(); }
  BL_INLINE const BLPipeline::DispatchData* pipeDispatchData() const noexcept { return command().dispatchData(); }
};

typedef RenderCommandSerializerCore<BL_RASTER_RENDERING_MODE_SYNC> RenderCommandSerializerCoreSync;
typedef RenderCommandSerializerCore<BL_RASTER_RENDERING_MODE_ASYNC> RenderCommandSerializerCoreAsync;

template<uint32_t RenderingMode>
struct RenderCommandSerializerMask;

template<>
struct RenderCommandSerializerMask<BL_RASTER_RENDERING_MODE_SYNC> : public RenderCommandSerializerCoreSync {
  RenderFetchData _maskFetchData;

  BL_INLINE BLResult initFetchDataForMask(BLRasterContextImpl* ctxI) noexcept {
    blUnused(ctxI);

    command().initMaskFetchData(&_maskFetchData);
    return BL_SUCCESS;
  }

  BL_INLINE void rollbackFetchData(BLRasterContextImpl* ctxI) noexcept {
    // Nothing in synchronous rendering case.
    blUnused(ctxI);
  }

  BL_INLINE RenderFetchData* maskFetchData() { return &_maskFetchData; }
  BL_INLINE const RenderFetchData* maskFetchData() const { return &_maskFetchData; }
};

template<>
struct RenderCommandSerializerMask<BL_RASTER_RENDERING_MODE_ASYNC> : public RenderCommandSerializerCoreAsync {
  BL_INLINE BLResult initFetchDataForMask(BLRasterContextImpl* ctxI) noexcept {
    // Allocates on worker manager pool as it's easier to roll it back in case something fails.
    RenderFetchData* maskFetchData = ctxI->workerMgr()._allocator.allocT<RenderFetchData>();

    if (BL_UNLIKELY(!maskFetchData))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    command().initMaskFetchData(maskFetchData);
    return BL_SUCCESS;
  }

  BL_INLINE void rollbackFetchData(BLRasterContextImpl* ctxI) noexcept {
    ctxI->workerMgr()._allocator.restoreState(
      reinterpret_cast<BLArenaAllocator::StatePtr>(_command->_source.fetchData));
  }

  BL_INLINE RenderFetchData* maskFetchData() { return command()._payload.maskFetchData; }
  BL_INLINE const RenderFetchData* maskFetchData() const { return command()._payload.maskFetchData; }
};

typedef RenderCommandSerializerMask<BL_RASTER_RENDERING_MODE_SYNC> RenderCommandSerializerMaskSync;
typedef RenderCommandSerializerMask<BL_RASTER_RENDERING_MODE_ASYNC> RenderCommandSerializerMaskAsync;

//! Used to build and serialize a blit rendering command.
//!
//! \note Blit rendering commands are basically the same as other fill commands. However, to make them more optimized
//! we use an inline `RenderFetchData` in synchronous case as the data won't be used outside of the render call, and
//! we allocate such data on a different pool than common FetchData in asynchronous case.
template<uint32_t RenderingMode>
struct RenderCommandSerializerBlit;

template<>
struct RenderCommandSerializerBlit<BL_RASTER_RENDERING_MODE_SYNC> : public RenderCommandSerializerCoreSync {
  RenderFetchData _fetchData;

  BL_INLINE BLResult initFetchDataForBlit(BLRasterContextImpl* ctxI) noexcept {
    blUnused(ctxI);

    command().initFetchData(&_fetchData);
    return BL_SUCCESS;
  }

  BL_INLINE void rollbackFetchData(BLRasterContextImpl* ctxI) noexcept {
    // Nothing in synchronous rendering case.
    blUnused(ctxI);
  }

  BL_INLINE RenderFetchData* fetchData() { return &_fetchData; }
  BL_INLINE const RenderFetchData* fetchData() const { return &_fetchData; }
};

template<>
struct RenderCommandSerializerBlit<BL_RASTER_RENDERING_MODE_ASYNC> : public RenderCommandSerializerCoreAsync {
  BL_INLINE BLResult initFetchDataForBlit(BLRasterContextImpl* ctxI) noexcept {
    // Allocates on worker manager pool as it's easier to roll it back in case something fails.
    RenderFetchData* fetchData = ctxI->workerMgr()._allocator.allocT<RenderFetchData>();

    if (BL_UNLIKELY(!fetchData))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    command().initFetchData(fetchData);
    return BL_SUCCESS;
  }

  BL_INLINE void rollbackFetchData(BLRasterContextImpl* ctxI) noexcept {
    if (_command->_source.fetchData) {
      ctxI->workerMgr()._allocator.restoreState(
        reinterpret_cast<BLArenaAllocator::StatePtr>(_command->_source.fetchData));
    }
  }

  BL_INLINE RenderFetchData* fetchData() { return _command->_source.fetchData; }
  BL_INLINE const RenderFetchData* fetchData() const { return _command->_source.fetchData; }
};

typedef RenderCommandSerializerBlit<BL_RASTER_RENDERING_MODE_SYNC> RenderCommandSerializerBlitSync;
typedef RenderCommandSerializerBlit<BL_RASTER_RENDERING_MODE_ASYNC> RenderCommandSerializerBlitAsync;

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERCOMMANDSERIALIZER_P_H_INCLUDED
