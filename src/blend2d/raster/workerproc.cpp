// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../api-internal_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rendercommand_p.h"
#include "../raster/rendercommandprocasync_p.h"
#include "../raster/renderjob_p.h"
#include "../raster/renderjobproc_p.h"
#include "../raster/workdata_p.h"
#include "../raster/workerproc_p.h"
#include "../raster/workersynchronization_p.h"
#include "../support/bitops_p.h"
#include "../support/intops_p.h"

namespace bl {
namespace RasterEngine {
namespace WorkerProc {

// TODO: [Rendering Context] HARDCODED.
static const uint32_t fpScale = 256;

// bl::RasterEngine::WorkerProc - ProcessJobs
// ==========================================

static BL_NOINLINE void processJobs(WorkData* workData, RenderBatch* batch) noexcept {
  size_t jobCount = batch->jobCount();

  if (!jobCount) {
    workData->synchronization->noJobsToWaitFor();
    return;
  }

  const RenderJobQueue* queue = batch->jobList().first();
  BL_ASSERT(queue != nullptr);

  size_t queueIndex = 0;
  size_t queueEnd = queueIndex + queue->size();

  for (;;) {
    size_t jobIndex = batch->nextJobIndex();
    if (jobIndex >= jobCount)
      break;

    while (jobIndex >= queueEnd) {
      queue = queue->next();
      BL_ASSERT(queue != nullptr);

      queueIndex = queueEnd;
      queueEnd = queueIndex + queue->size();
    }

    RenderJob* job = queue->at(jobIndex - queueIndex);
    BL_ASSERT(job != nullptr);

    JobProc::processJob(workData, job);
  }

  workData->avoidCacheLineSharing();
  workData->synchronization->waitForJobsToFinish();
}

// bl::RasterEngine::WorkerProc - ProcessBand
// ==========================================

static void processBand(CommandProcAsync::ProcData& procData, bool isInitialBand) noexcept {
  // Should not happen.
  if (!procData.pendingCommandBitSetSize())
    return;

  typedef PrivateBitWordOps BitOps;
  RenderBatch* batch = procData.batch();

  BLBitWord* bitSetPtr = procData.pendingCommandBitSetData();
  BLBitWord* bitSetEndMinus1 = procData.pendingCommandBitSetEnd() - 1;
  BLBitWord bitSetMask = procData.pendingCommandBitSetMask();

  const RenderCommandQueue* commandQueue = batch->_commandList.first();
  const RenderCommand* commandData = commandQueue->data();
  const RenderCommand* commandDataEnd = commandQueue->end();

  for (;;) {
#ifdef __SANITIZE_ADDRESS__
    // We know it's uninitialized, that's why we use the mask.
    BLBitWord bitWord = !bitSetMask ? *bitSetPtr : bitSetMask;
#else
    BLBitWord bitWord = bitSetMask | *bitSetPtr;
#endif

    BitOps::BitIterator it(bitWord);
    while (it.hasNext()) {
      uint32_t bitIndex = it.next();
      const RenderCommand& command = commandData[bitIndex];

      bool finished = CommandProcAsync::processCommand(procData, command, isInitialBand);
      bitWord ^= BitOps::indexAsMask(bitIndex, finished);
    }

    *bitSetPtr = bitWord;

    if (++bitSetPtr >= bitSetEndMinus1) {
      bitSetMask = 0;
      if (bitSetPtr > bitSetEndMinus1)
        break;
    }

    commandData += IntOps::bitSizeOf<BLBitWord>();

    if (commandData == commandDataEnd) {
      commandQueue = commandQueue->next();
      BL_ASSERT(commandQueue != nullptr);

      commandData = commandQueue->data();
      commandDataEnd = commandQueue->end();
    }
  }

  procData.clearPendingCommandBitSetMask();
}

// bl::RasterEngine::WorkerProc - ProcessCommands
// ==============================================

static void processCommands(WorkData* workData, RenderBatch* batch) noexcept {
  ArenaAllocator::StatePtr zoneState = workData->workZone.saveState();
  CommandProcAsync::ProcData procData(workData, batch);

  BLResult result = procData.initProcData();
  if (result != BL_SUCCESS) {
    workData->accumulateError(result);
    return;
  }

  bool isInitialBand = true;
  uint32_t workerCount = batch->workerCount();
  uint32_t bandCount = batch->bandCount();

  // We can process several consecutive bands at once when there is enough of bands for all the threads.
  //
  // TODO: [Rendering Context] At the moment this feature is not used as it regressed bl_bench using 4+ threads.
  uint32_t consecutiveBandCount = 1;

  uint32_t bandId = workData->workerId() * consecutiveBandCount;
  uint32_t consecutiveIndex = 0;

  while (bandId + consecutiveIndex < bandCount) {
    procData.initBand(bandId + consecutiveIndex, workData->bandHeight(), fpScale);
    processBand(procData, isInitialBand);

    isInitialBand = false;

    if (++consecutiveIndex == consecutiveBandCount) {
      consecutiveIndex = 0;
      bandId += workerCount * consecutiveBandCount;
    }
  }

  workData->workZone.restoreState(zoneState);
}

// bl::RasterEngine::WorkerProc - Finished
// =======================================

static void finished(WorkData* workData, RenderBatch* batch) noexcept {
  workData->resetBatch();

  if (workData->isSync())
    return;

  uint32_t accumulatedErrorFlags = workData->accumulatedErrorFlags();
  if (!accumulatedErrorFlags)
    return;

  batch->accumulateErrorFlags(accumulatedErrorFlags);
  workData->cleanAccumulatedErrorFlags();
}

// bl::RasterEngine::WorkerProc - ProcessWorkData
// ==============================================

// Can be also called by the rendering context from user thread.
void processWorkData(WorkData* workData, RenderBatch* batch) noexcept {
  // NOTE: The zone must be cleared when the worker thread starts processing jobs and commands. The reason is that
  // once we finish job processing other threads can still use data produced by such job, so even when we are done
  // we cannot really clear the allocator, we must wait until all threads are done with the current batch, and that
  // is basically only guaranteed when we enter the proc again (or by the rendering context once it finishes).
  if (!workData->isSync())
    workData->startOver();

  // Fix the alignment of the arena allocator in case it's currently not aligned - this prevents possible sharing of
  // a cache line that was used for something that could be used by all worker threads with a possible allocation
  // that is only intended to be used by the worker - for a memory region that the worker can write to frequently
  // (like active edges during rasterization).
  workData->avoidCacheLineSharing();

  // Pass 1 - Process jobs.
  //
  // Once the thread acquires a job to process no other thread can have that job. Jobs can be processed in any order,
  // however, we just use atomics to increment the job counter and each thread acquires the next in the queue.
  processJobs(workData, batch);

  // Pass 2 - Process commands.
  //
  // Commands are processed after the last job finishes. Command are processed multiple times per each band. Threads
  // process all commands in a band and then move to the next available band. This ensures that even when there is
  // something more complicated in one band than in all other bands the distribution of threads should be fair as
  // other threads won't wait for a particular band to be rendered.
  processCommands(workData, batch);

  // Propagates accumulated error flags into the batch.
  finished(workData, batch);
}

// bl::RasterEngine::WorkerProc - WorkerThreadEntry
// ================================================

void workerThreadEntry(BLThread* thread, void* data) noexcept {
  blUnused(thread);

  WorkData* workData = static_cast<WorkData*>(data);
  WorkerSynchronization* synchronization = workData->synchronization;

  synchronization->threadStarted();
  processWorkData(workData, workData->acquireBatch());
  synchronization->threadDone();
}

} // {WorkerProc}
} // {RasterEngine}
} // {bl}
