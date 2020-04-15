// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#include "../api-internal_p.h"
#include "../bitops_p.h"
#include "../raster/rastercommand_p.h"
#include "../raster/rastercommandprocasync_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rasterjob_p.h"
#include "../raster/rasterjobproc_p.h"
#include "../raster/rasterworkdata_p.h"
#include "../raster/rasterworkproc_p.h"
#include "../raster/rasterworksynchronization_p.h"

// TODO: HARDCODED.
static const uint32_t fpScale = 256;

// ============================================================================
// [BLRasterWorkProc - ProcessJobQueue]
// ============================================================================

static void blRasterWorkProcessJobs(BLRasterWorkData* workData) noexcept {
  BLRasterWorkBatch* batch = workData->batch;
  size_t jobCount = batch->jobCount();

  if (!jobCount)
    return;

  const BLRasterJobQueue* queue = batch->jobQueueList().first();
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

    BLRasterJobData* jobData = queue->at(jobIndex - queueIndex);
    BL_ASSERT(jobData != nullptr);

    blRasterJobProcAsync(workData, jobData);
  }

  batch->_synchronization->waitForJobsToFinish();
}

// ============================================================================
// [BLRasterWorkProc - ProcessCommandQueue]
// ============================================================================

static void blRasterWorkProcessBand(BLRasterCommandProcAsyncData& procData, bool isInitialBand) noexcept {
  // Should not happen.
  if (!procData.pendingCommandBitSetSize())
    return;

  typedef BLPrivateBitOps<BLBitWord> BitOps;
  BLRasterWorkBatch* batch = procData.batch();

  BLBitWord* bitSetPtr = procData.pendingCommandBitSetData();
  BLBitWord* bitSetEndMinus1 = procData.pendingCommandBitSetEnd() - 1;
  BLBitWord bitSetMask = procData.pendingCommandBitSetMask();

  const BLRasterCommandQueue* commandQueue = batch->_commandQueueList.first();
  const BLRasterCommand* commandQueueData = commandQueue->data();

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
      const BLRasterCommand& command = commandQueueData[bitIndex];
      if (blRasterCommandProcAsync(procData, command, isInitialBand)) {
        bitWord &= ~BitOps::indexAsMask(bitIndex);
      }
    }
    *bitSetPtr = bitWord;

    if (++bitSetPtr >= bitSetEndMinus1) {
      bitSetMask = 0;
      if (bitSetPtr > bitSetEndMinus1)
        break;
    }

    commandQueueData += blBitSizeOf<BLBitWord>();
    if (commandQueueData == commandQueue->end()) {
      commandQueue = commandQueue->next();
      BL_ASSERT(commandQueue != nullptr);
      commandQueueData = commandQueue->data();
    }
  }

  procData.clearPendingCommandBitSetMask();
}

static void blRasterWorkProcessCommands(BLRasterWorkData* workData) noexcept {
  BLRasterWorkBatch* batch = workData->batch;
  BLZoneAllocator::StatePtr zoneState = workData->workZone.saveState();

  BLRasterCommandProcAsyncData procData(workData);
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
    blRasterWorkProcessBand(procData, isInitialBand);

    isInitialBand = false;
  }

  workData->workZone.restoreState(zoneState);
}

// ============================================================================
// [BLRasterWorkProc - Done]
// ============================================================================

static void blRasterWorkProcDone(BLRasterWorkData* workData) noexcept {
  if (workData->isSync())
    return;

  uint32_t accumulatedErrorFlags = workData->accumulatedErrorFlags();
  if (!accumulatedErrorFlags)
    return;

  workData->batch->accumulateErrorFlags(accumulatedErrorFlags);
  workData->cleanAccumulatedErrorFlags();
}

// ============================================================================
// [BLRasterWorkProc - Main]
// ============================================================================

void blRasterWorkProc(BLRasterWorkData* workData) noexcept {
  // NOTE: The zone must be cleared when the worker threads starts processing
  // jobs and commands. The reason is that once we finish job processing other
  // threads can still use data produced by such job, so even when we are done
  // we cannot really clear the allocator, we must wait until all threads are
  // done with the current batch, and that is basically only guaranteed when
  // we enter the proc again (or by the rendering context once it finishes).
  if (!workData->isSync())
    workData->startOver();

  // Pass 1 - Process jobs.
  //
  // Once the thread acquires a job to process no other thread can have that
  // job. Jobs can be processed in any order, however, we just use atomics to
  // increment the job counter and each thread acquires the next in the queue.
  blRasterWorkProcessJobs(workData);

  // Pass 2 - Process commands.
  //
  // Commands are processed after the last job finishes. Command are processed
  // multiple times per each band. Threads process all commands in a band and
  // then move to the next available band. This ensures that even when there
  // is something more complicated in one band than in all other bands the
  // distribution of threads should be fair as other threads won't wait for
  // a particular band to be rendered.
  blRasterWorkProcessCommands(workData);

  // Propagates accumulated error flags into the batch.
  blRasterWorkProcDone(workData);
}

// ============================================================================
// [BLRasterWorkProc - Thread Entry / Done]
// ============================================================================

void blRasterWorkThreadEntry(BLThread* thread, void* data) noexcept {
  BL_UNUSED(thread);

  BLRasterWorkData* workData = static_cast<BLRasterWorkData*>(data);
  blRasterWorkProc(workData);
}

void blRasterWorkThreadDone(BLThread* thread, void* data) noexcept {
  BL_UNUSED(thread);

  BLRasterWorkData* workData = static_cast<BLRasterWorkData*>(data);
  workData->batch->_synchronization->threadDone();
}
