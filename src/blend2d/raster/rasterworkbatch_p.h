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

#ifndef BLEND2D_RASTER_RASTERWORKBATCH_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERWORKBATCH_P_H_INCLUDED

#include "../image.h"
#include "../zoneallocator_p.h"
#include "../zonelist_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/rasterworkqueue_p.h"
#include "../threading/atomic_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

class BLRasterContextImpl;
class BLRasterWorkSynchronization;

// ============================================================================
// [BLRasterWorkBatch]
// ============================================================================

//! Holds jobs and commands to be dispatched and then consumed by worker threads.
class alignas(BL_CACHE_LINE_SIZE) BLRasterWorkBatch {
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
  BLRasterWorkSynchronization* _synchronization;

  BLZoneList<BLRasterJobQueue> _jobQueueList;
  BLZoneList<BLRasterFetchQueue> _fetchQueueList;
  BLZoneList<BLRasterCommandQueue> _commandQueueList;
  BLZoneAllocator::Block* _pastBlock;

  uint32_t _jobCount;
  uint32_t _commandCount;
  uint32_t _bandCount;
  uint32_t _stateSlotCount;

  BL_INLINE BLRasterWorkBatch() noexcept
    : _jobIndex(0),
      _accumulatedErrorFlags(0),
      _bandIndex(0),
      _synchronization(nullptr),
      _jobQueueList(),
      _fetchQueueList(),
      _commandQueueList(),
      _pastBlock(nullptr),
      _jobCount(0),
      _commandCount(0),
      _bandCount(0),
      _stateSlotCount(0) {}

  BL_INLINE ~BLRasterWorkBatch() noexcept {}

  BL_INLINE size_t nextJobIndex() noexcept { return blAtomicFetchAdd(&_jobIndex); }
  BL_INLINE size_t nextBandIndex() noexcept { return blAtomicFetchAdd(&_bandIndex); }

  BL_INLINE const BLZoneList<BLRasterJobQueue>& jobQueueList() const noexcept { return _jobQueueList; }
  BL_INLINE const BLZoneList<BLRasterFetchQueue>& fetchQueueList() const noexcept { return _fetchQueueList; }
  BL_INLINE const BLZoneList<BLRasterCommandQueue>& commandQueueList() const noexcept { return _commandQueueList; }

  BL_INLINE uint32_t jobCount() const noexcept { return _jobCount; }
  BL_INLINE uint32_t commandCount() const noexcept { return _commandCount; }

  BL_INLINE uint32_t bandCount() const noexcept { return _bandCount; }
  BL_INLINE uint32_t stateSlotCount() const noexcept { return _stateSlotCount; }

  BL_INLINE void accumulateErrorFlags(uint32_t errorFlags) noexcept {
    blAtomicFetchOr(&_accumulatedErrorFlags, errorFlags, std::memory_order_relaxed);
  }
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERWORKBATCH_P_H_INCLUDED
