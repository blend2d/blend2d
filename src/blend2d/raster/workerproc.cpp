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

namespace BLRasterEngine {
namespace WorkerProc {

// TODO: HARDCODED.
static const uint32_t fpScale = 256;

// RasterEngine::WorkerProc - ProcessJobs
// ======================================

static void processJobs(WorkData* workData) noexcept {
  RenderBatch* batch = workData->batch;
  size_t jobCount = batch->jobCount();

  if (!jobCount)
    return;

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

    blRasterJobProcAsync(workData, job);
  }

  batch->_synchronization->waitForJobsToFinish();
}

// RasterEngine::WorkerProc - ProcessBand
// ======================================

static void processBand(CommandProcAsync::ProcData& procData, bool isInitialBand) noexcept {
  // Should not happen.
  if (!procData.pendingCommandBitSetSize())
    return;

  typedef BLPrivateBitWordOps BitOps;
  RenderBatch* batch = procData.batch();

  BLBitWord* bitSetPtr = procData.pendingCommandBitSetData();
  BLBitWord* bitSetEndMinus1 = procData.pendingCommandBitSetEnd() - 1;
  BLBitWord bitSetMask = procData.pendingCommandBitSetMask();

  const RenderCommandQueue* commandQueue = batch->_commandList.first();
  const RenderCommand* commandQueueData = commandQueue->data();

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
      const RenderCommand& command = commandQueueData[bitIndex];
      if (CommandProcAsync::processCommand(procData, command, isInitialBand)) {
        bitWord &= ~BitOps::indexAsMask(bitIndex);
      }
    }

    *bitSetPtr = bitWord;

    if (++bitSetPtr >= bitSetEndMinus1) {
      bitSetMask = 0;
      if (bitSetPtr > bitSetEndMinus1)
        break;
    }

    commandQueueData += BLIntOps::bitSizeOf<BLBitWord>();
    if (commandQueueData == commandQueue->end()) {
      commandQueue = commandQueue->next();
      BL_ASSERT(commandQueue != nullptr);
      commandQueueData = commandQueue->data();
    }
  }

  procData.clearPendingCommandBitSetMask();
}

// RasterEngine::WorkerProc - ProcessCommands
// ==========================================

#if 1
static void processCommands(WorkData* workData) noexcept {
  RenderBatch* batch = workData->batch;

  BLArenaAllocator::StatePtr zoneState = workData->workZone.saveState();
  CommandProcAsync::ProcData procData(workData);

  BLResult result = procData.initProcData();
  if (result != BL_SUCCESS) {
    workData->accumulateError(result);
    return;
  }

  bool isInitialBand = true;
  size_t bandCount = batch->bandCount();

  for (;;) {
    size_t bandId = batch->nextBandIndex();
    if (bandId >= bandCount)
      break;

    procData.initBand(uint32_t(bandId), workData->bandHeight(), fpScale);
    processBand(procData, isInitialBand);

    isInitialBand = false;
  }

  workData->workZone.restoreState(zoneState);
}
#endif

#if 0
static void processCommands(WorkData* workData) noexcept {
  RenderBatch* batch = workData->batch;

  BLArenaAllocator::StatePtr zoneState = workData->workZone.saveState();
  CommandProcAsync::ProcData procData(workData);

  BLResult result = procData.initProcData();
  if (result != BL_SUCCESS) {
    workData->accumulateError(result);
    return;
  }

  bool isInitialBand = true;
  uint32_t workerCount = batch->workerCount();
  uint32_t bandCount = batch->bandCount();
  uint32_t bandId = workData->workerId();

  while (bandId < bandCount) {
    procData.initBand(uint32_t(bandId), workData->bandHeight(), fpScale);
    processBand(procData, isInitialBand);

    isInitialBand = false;
    bandId += workerCount;
  }

  workData->workZone.restoreState(zoneState);
}
#endif

// RasterEngine::WorkerProc - Finished
// ===================================

static void finished(WorkData* workData) noexcept {
  RenderBatch* batch = workData->batch;
  workData->batch = nullptr;

  if (workData->isSync())
    return;

  uint32_t accumulatedErrorFlags = workData->accumulatedErrorFlags();
  if (!accumulatedErrorFlags)
    return;

  batch->accumulateErrorFlags(accumulatedErrorFlags);
  workData->cleanAccumulatedErrorFlags();
}

// RasterEngine::WorkerProc - ProcessWorkData
// ==========================================

// Can be also called by the rendering context from user thread.
void processWorkData(WorkData* workData) noexcept {
  // NOTE: The zone must be cleared when the worker thread starts processing jobs and commands. The reason is that
  // once we finish job processing other threads can still use data produced by such job, so even when we are done
  // we cannot really clear the allocator, we must wait until all threads are done with the current batch, and that
  // is basically only guaranteed when we enter the proc again (or by the rendering context once it finishes).
  if (!workData->isSync())
    workData->startOver();

  // Pass 1 - Process jobs.
  //
  // Once the thread acquires a job to process no other thread can have that job. Jobs can be processed in any order,
  // however, we just use atomics to increment the job counter and each thread acquires the next in the queue.
  processJobs(workData);

  // Pass 2 - Process commands.
  //
  // Commands are processed after the last job finishes. Command are processed multiple times per each band. Threads
  // process all commands in a band and then move to the next available band. This ensures that even when there is
  // something more complicated in one band than in all other bands the distribution of threads should be fair as
  // other threads won't wait for a particular band to be rendered.
  processCommands(workData);

  // Propagates accumulated error flags into the batch.
  finished(workData);
}

// RasterEngine::WorkerProc - WorkerThreadEntry
// ============================================

void workerThreadEntry(BLThread* thread, void* data) noexcept {
  blUnused(thread);

  WorkData* workData = static_cast<WorkData*>(data);
  RenderBatch* batch = workData->batch;

  processWorkData(workData);

  // NOTE: At this point `batch` is no longer referenced by `workData`, we just saved the pointer so we can call
  // threadDone() after all commands were processed.
  batch->_synchronization->threadDone();
}

} // {WorkerProc}
} // {BLRasterEngine}
