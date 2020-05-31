// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_RASTER_RASTERCOMMANDSERIALIZER_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERCOMMANDSERIALIZER_P_H_INCLUDED

#include "../zoneallocator_p.h"
#include "../raster/rastercommand_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rasterworkermanager_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [BLRasterCommandSerializerStorage]
// ============================================================================

template<uint32_t RenderingMode>
struct BLRasterCommandSerializerStorage;

template<>
struct BLRasterCommandSerializerStorage<BL_RASTER_RENDERING_MODE_SYNC> {
  //! Command data passed to a synchronous command processor.
  BLRasterCommand _command;

  BL_INLINE BLRasterCommand& command() noexcept { return _command; }
  BL_INLINE const BLRasterCommand& command() const noexcept { return _command; }

  BL_INLINE BLResult initStorage(BLRasterContextImpl* ctxI) noexcept {
    // Synchronous rendering doesn't use command storage.
    blUnused(ctxI);
    return BL_SUCCESS;
  }
};

template<>
struct BLRasterCommandSerializerStorage<BL_RASTER_RENDERING_MODE_ASYNC> {
  //! Command data pointing to a command in the command queue.
  BLRasterCommand* _command;

  BL_INLINE BLRasterCommand& command() noexcept { return *_command; }
  BL_INLINE const BLRasterCommand& command() const noexcept { return *_command; }

  BL_INLINE BLResult initStorage(BLRasterContextImpl* ctxI) noexcept {
    BLRasterWorkerManager& mgr = ctxI->workerMgr();

    BL_PROPAGATE(mgr.ensureCommandQueue());
    _command = mgr.currentCommandData();
    ctxI->syncWorkData.saveState();

    return BL_SUCCESS;
  }

  BL_INLINE bool enqueued(BLRasterContextImpl* ctxI) const noexcept {
    return _command != ctxI->workerMgr().currentCommandData();
  }
};

// ============================================================================
// [BLRasterCoreCommandSerializer]
// ============================================================================

//! Used to build and serialize a core rendering command.
//!
//! Initialization order:
//!   1. initStorage()
//!   2. initPipeline()
//!   3. initCommand()
//!   4. Others in any order.
template<uint32_t RenderingMode>
struct BLRasterCoreCommandSerializer : public BLRasterCommandSerializerStorage<RenderingMode> {
  using BLRasterCommandSerializerStorage<RenderingMode>::command;

  //! Pipeline signature.
  BLPipeSignature _pipeSignature;
  //! Style data to use when `_fetchData` is not available.
  const BLRasterContextStyleData* _styleData;

  BL_INLINE BLPipeSignature& pipeSignature() noexcept { return _pipeSignature; }
  BL_INLINE const BLPipeSignature& pipeSignature() const noexcept { return _pipeSignature; }

  BL_INLINE const BLRasterContextStyleData* styleData() const noexcept { return _styleData; }
  BL_INLINE void setStyleData(const BLRasterContextStyleData* styleData) noexcept { _styleData = styleData; }

  BL_INLINE void initPipeline(const BLPipeSignature& signature) noexcept {
    pipeSignature().reset(signature);
    setStyleData(nullptr);
  }

  BL_INLINE void initCommand(uint32_t alpha) noexcept {
    command().initCommand(alpha);
  }

  BL_INLINE void initFillBoxA(const BLBoxI& boxA) noexcept {
    command().initFillBoxA(boxA);
    pipeSignature().addFillType(BL_PIPE_FILL_TYPE_BOX_A);
  }

  BL_INLINE void initFillBoxU(const BLBoxI& boxU) noexcept {
    command().initFillBoxU(boxU);
    pipeSignature().addFillType(BL_PIPE_FILL_TYPE_BOX_U);
  }

  BL_INLINE void initFillAnalyticSync(uint32_t fillRule, BLEdgeStorage<int>* edgeStorage) noexcept {
    command().initFillAnalyticSync(fillRule, edgeStorage);
    pipeSignature().addFillType(BL_PIPE_FILL_TYPE_ANALYTIC);
  }

  BL_INLINE void initFillAnalyticAsync(uint32_t fillRule, BLEdgeVector<int>* edges) noexcept {
    command().initFillAnalyticAsync(fillRule, edges);
    pipeSignature().addFillType(BL_PIPE_FILL_TYPE_ANALYTIC);
  }

  BL_INLINE void initFetchSolid(const BLPipeFetchData::Solid& solidData) noexcept {
    command().initFetchSolid(solidData);
  }

  BL_INLINE void initFetchDataFromStyle(const BLRasterContextStyleData* styleData) noexcept {
    command()._flags |= styleData->cmdFlags;
    command()._source = styleData->source;
    setStyleData(styleData);
  }

  BL_INLINE void initFillFunc(BLPipeFillFunc fillFunc) noexcept {
    command().initFillFunc(fillFunc);
  }
};

typedef BLRasterCoreCommandSerializer<BL_RASTER_RENDERING_MODE_SYNC> BLRasterCoreCommandSerializerSync;
typedef BLRasterCoreCommandSerializer<BL_RASTER_RENDERING_MODE_ASYNC> BLRasterCoreCommandSerializerAsync;

// ============================================================================
// [BLRasterBlitCommandSerializer]
// ============================================================================

//! Used to build and serialize a blit rendering command.
//!
//! \note Blit rendering commands are basically the same as other fill commands.
//! However, to make them more optimized we use an inline `BLRasterFetchData` in
//! synchronous case as the data won't be used after the call is done, and we
//! allocate such data on a different pool in asynchronous case.
template<uint32_t RenderingMode>
struct BLRasterBlitCommandSerializer;

template<>
struct BLRasterBlitCommandSerializer<BL_RASTER_RENDERING_MODE_SYNC> : public BLRasterCoreCommandSerializerSync {
  BLRasterFetchData _fetchData;

  BL_INLINE BLResult initFetchDataForBlit(BLRasterContextImpl* ctxI) noexcept {
    blUnused(ctxI);

    command().initFetchData(&_fetchData);
    return BL_SUCCESS;
  }

  BL_INLINE void rollbackFetchData(BLRasterContextImpl* ctxI) noexcept {
    // Nothing in synchronous rendering case.
    blUnused(ctxI);
  }

  BL_INLINE BLRasterFetchData* fetchData() { return &_fetchData; }
  BL_INLINE const BLRasterFetchData* fetchData() const { return &_fetchData; }
};

template<>
struct BLRasterBlitCommandSerializer<BL_RASTER_RENDERING_MODE_ASYNC> : public BLRasterCoreCommandSerializerAsync {
  BL_INLINE BLResult initFetchDataForBlit(BLRasterContextImpl* ctxI) noexcept {
    // We allocate on worker manager pool as it's easier to roll it back in
    // case that this command is not added to the queue for various reasons.
    BLRasterFetchData* fetchData = ctxI->workerMgr()._allocator.allocT<BLRasterFetchData>();

    if (BL_UNLIKELY(!fetchData))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    command().initFetchData(fetchData);
    return BL_SUCCESS;
  }

  BL_INLINE void rollbackFetchData(BLRasterContextImpl* ctxI) noexcept {
    ctxI->workerMgr()._allocator.restoreState(
      reinterpret_cast<BLZoneAllocator::StatePtr>(_command->_source.fetchData));
  }

  BL_INLINE BLRasterFetchData* fetchData() { return _command->_source.fetchData; }
  BL_INLINE const BLRasterFetchData* fetchData() const { return _command->_source.fetchData; }
};

typedef BLRasterBlitCommandSerializer<BL_RASTER_RENDERING_MODE_SYNC> BLRasterBlitCommandSerializerSync;
typedef BLRasterBlitCommandSerializer<BL_RASTER_RENDERING_MODE_ASYNC> BLRasterBlitCommandSerializerAsync;

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCOMMANDSERIALIZER_P_H_INCLUDED
