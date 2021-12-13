// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERBATCH_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERBATCH_P_H_INCLUDED

#include "../image.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/renderqueue_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/arenalist_p.h"
#include "../threading/atomic_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

class WorkerSynchronization;

//! Holds jobs and commands to be dispatched and then consumed by worker threads.
class alignas(BL_CACHE_LINE_SIZE) RenderBatch {
public:
  struct alignas(BL_CACHE_LINE_SIZE) {
    //! Job index, incremented by each worker when trying to get the next job.
    //! Can go out of range in case there is no more jobs to process.
    volatile size_t _jobIndex;

    //! Accumulated errors, initially zero for each batch. Since all workers
    //! would only OR their errors (if happened) at the end we can share the
    //! cache line with `_jobIndex`.
    volatile uint32_t _accumulatedErrorFlags;
  };

  struct alignas(BL_CACHE_LINE_SIZE) {
    //! Band index, incremented by workers to get a band index to process.
    //! Can go out of range in case there is no more bands to process.
    volatile size_t _bandIndex;
  };

  //! Pointer to the synchronization data.
  WorkerSynchronization* _synchronization;

  //! Contains all jobs of this batch.
  BLArenaList<RenderJobQueue> _jobList;
  //! Contains all RenderFetchData objects of this batch.
  BLArenaList<RenderFetchQueue> _fetchList;
  //! Contains all commands of this batch.
  BLArenaList<RenderCommandQueue> _commandList;
  //! A storage where all user objects (currently only BLImage) are kept until they can be released.
  BLArenaList<RenderImageQueue> _imageList;

  BLArenaAllocator::Block* _pastBlock;

  uint32_t _workerCount;
  uint32_t _jobCount;
  uint32_t _commandCount;
  uint32_t _bandCount;
  uint32_t _stateSlotCount;

  BL_INLINE RenderBatch() noexcept
    : _jobIndex(0),
      _accumulatedErrorFlags(0),
      _bandIndex(0),
      _synchronization(nullptr),
      _jobList(),
      _fetchList(),
      _commandList(),
      _pastBlock(nullptr),
      _workerCount(0),
      _jobCount(0),
      _commandCount(0),
      _bandCount(0),
      _stateSlotCount(0) {}

  BL_INLINE ~RenderBatch() noexcept {}

  BL_INLINE size_t nextJobIndex() noexcept { return blAtomicFetchAdd(&_jobIndex); }
  BL_INLINE size_t nextBandIndex() noexcept { return blAtomicFetchAdd(&_bandIndex); }

  BL_INLINE const BLArenaList<RenderJobQueue>& jobList() const noexcept { return _jobList; }
  BL_INLINE const BLArenaList<RenderFetchQueue>& fetchList() const noexcept { return _fetchList; }
  BL_INLINE const BLArenaList<RenderCommandQueue>& commandList() const noexcept { return _commandList; }

  BL_INLINE uint32_t workerCount() const noexcept { return _workerCount; }

  BL_INLINE uint32_t jobCount() const noexcept { return _jobCount; }
  BL_INLINE uint32_t commandCount() const noexcept { return _commandCount; }

  BL_INLINE uint32_t bandCount() const noexcept { return _bandCount; }
  BL_INLINE uint32_t stateSlotCount() const noexcept { return _stateSlotCount; }

  BL_INLINE void accumulateErrorFlags(uint32_t errorFlags) noexcept {
    blAtomicFetchOr(&_accumulatedErrorFlags, errorFlags, std::memory_order_relaxed);
  }
};

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERBATCH_P_H_INCLUDED
