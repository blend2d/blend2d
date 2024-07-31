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
#include "../simd/simd_p.h"
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

namespace {

#if BL_TARGET_ARCH_X86 && BL_SIMD_WIDTH_I

struct CommandMatcher {
#if BL_SIMD_WIDTH_I >= 256
  SIMD::Vec32xU8 _vqy;

  BL_INLINE explicit CommandMatcher(uint8_t qy) noexcept {
    _vqy = SIMD::make256_u8(qy);
  }

  BL_INLINE BLBitWord match(const uint8_t* bandQy0) noexcept {
    using namespace SIMD;

    Vec32xU8 q0 = cmp_ge_u8(_vqy, loadu<Vec32xU8>(bandQy0));

#if BL_TARGET_ARCH_BITS == 32
    return extract_mask_bits_i8(q0);
#else
    Vec32xU8 q1 = cmp_ge_u8(_vqy, loadu<Vec32xU8>(bandQy0 + 32));
    return extract_mask_bits_i8(q0, q1);
#endif
  }
#else
  SIMD::Vec16xU8 _vqy;

  BL_INLINE explicit CommandMatcher(uint8_t qy) noexcept {
    _vqy = SIMD::make128_u8(qy);
  }

  BL_INLINE BLBitWord match(const uint8_t* bandQy0) noexcept {
    using namespace SIMD;

    Vec16xU8 q0 = cmp_ge_u8(_vqy, loadu<Vec16xU8>(bandQy0 +  0));
    Vec16xU8 q1 = cmp_ge_u8(_vqy, loadu<Vec16xU8>(bandQy0 + 16));

#if BL_TARGET_ARCH_BITS == 32
    return extract_mask_bits_i8(q0, q1);
#else
    Vec16xU8 q2 = cmp_ge_u8(_vqy, loadu<Vec16xU8>(bandQy0 + 32));
    Vec16xU8 q3 = cmp_ge_u8(_vqy, loadu<Vec16xU8>(bandQy0 + 48));
    return extract_mask_bits_i8(q0, q1, q2, q3);
#endif
  }
#endif
};

#elif BL_TARGET_ARCH_ARM && BL_SIMD_WIDTH_I

// NOTE: We cannot use `extract_mask_bits_i8()` as it returns a LSB bit-mask, but we need a MSB one in this case.
struct CommandMatcher {
  SIMD::Vec16xU8 _vqy;
  SIMD::Vec16xU8 _vbm;

  BL_INLINE explicit CommandMatcher(uint8_t qy) noexcept {
    _vqy = SIMD::make128_u8(qy);
    _vbm = SIMD::make128_u8(0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u, 0x80u);
  }

  BL_INLINE BLBitWord match(const uint8_t* bandQy0) noexcept {
    using namespace SIMD;

    Vec16xU8 q0 = cmp_ge_u8(_vqy, loadu<Vec16xU8>(bandQy0 +  0));
    Vec16xU8 q1 = cmp_ge_u8(_vqy, loadu<Vec16xU8>(bandQy0 + 16));
    Vec16xU8 m0 = and_(vec_cast<Vec16xU8>(q0), _vbm);
    Vec16xU8 m1 = and_(vec_cast<Vec16xU8>(q1), _vbm);

#if BL_TARGET_ARCH_BITS == 32
    uint8x8_t acc0 = vpadd_u8(vget_low_u8(m0.v), vget_high_u8(m0.v));
    uint8x8_t acc1 = vpadd_u8(vget_low_u8(m1.v), vget_high_u8(m1.v));

    acc0 = vpadd_u8(acc0, acc1);
    acc0 = vpadd_u8(acc0, acc0);

    return IntOps::byteSwap32(vget_lane_u32(vreinterpret_u32_u8(acc0), 0));
#else
    Vec16xU8 q2 = cmp_ge_u8(_vqy, loadu<Vec16xU8>(bandQy0 + 32));
    Vec16xU8 q3 = cmp_ge_u8(_vqy, loadu<Vec16xU8>(bandQy0 + 48));
    Vec16xU8 m2 = and_(vec_cast<Vec16xU8>(q2), _vbm);
    Vec16xU8 m3 = and_(vec_cast<Vec16xU8>(q3), _vbm);

    uint8x16_t acc0 = vpaddq_u8(m0.v, m1.v);
    uint8x16_t acc1 = vpaddq_u8(m2.v, m3.v);

    acc0 = vpaddq_u8(acc0, acc1);
    acc0 = vpaddq_u8(acc0, acc0);

    return IntOps::byteSwap64(vgetq_lane_u64(vreinterpretq_u64_u8(acc0), 0));
#endif
  }
};

#endif

}

static void processBand(CommandProcAsync::ProcData& procData, uint32_t currentBandId, uint32_t prevBandId, uint32_t nextBandId) noexcept {
  // Should not happen.
  if (!procData.pendingCommandBitSetSize())
    return;

  typedef PrivateBitWordOps BitOps;

  RenderBatch* batch = procData.batch();
  WorkData* workData = procData.workData();

  // Initialize the `procData` with the current band.
  procData.initBand(currentBandId, workData->bandHeight(), fpScale);

  BLBitWord* bitSetPtr = procData.pendingCommandBitSetData();
  BLBitWord* bitSetEndMinus1 = procData.pendingCommandBitSetEnd() - 1;
  BLBitWord pendingGlobalMask = procData.pendingCommandBitSetMask();

  const RenderCommandQueue* commandQueue = batch->_commandList.first();
  const RenderCommand* commandData = commandQueue->data();
  const RenderCommand* commandDataEnd = commandQueue->end();
  const uint8_t* commandQuantizedY0 = commandQueue->_quantizedY0;

  int32_t prevBandFy1 = int32_t((prevBandId + 1u) * workData->bandHeightFixed()) - 1u;
  int32_t nextBandFy0 = int32_t((nextBandId     ) * workData->bandHeightFixed());

  if (currentBandId == prevBandId)
    prevBandFy1 = -1;

  uint32_t bandQy0 = uint8_t(procData.bandY0() >> workData->commandQuantizationShiftAA());
#if (BL_TARGET_ARCH_X86 || BL_TARGET_ARCH_ARM) && BL_SIMD_WIDTH_I
  CommandMatcher matcher(static_cast<uint8_t>(bandQy0));
#endif

  for (;;) {
#ifdef __SANITIZE_ADDRESS__
    // We know it's uninitialized, that's why we use the mask, which is either all ones or all zeros.
    BLBitWord pendingMask = !pendingGlobalMask ? *bitSetPtr : pendingGlobalMask;
#else
    BLBitWord pendingMask = pendingGlobalMask | *bitSetPtr;
#endif

    if (pendingMask) {
#if (BL_TARGET_ARCH_X86 || BL_TARGET_ARCH_ARM) && BL_SIMD_WIDTH_I
      BLBitWord processMask = pendingMask & matcher.match(commandQuantizedY0);
      BitOps::BitIterator it(processMask);

      while (it.hasNext()) {
        uint32_t bitIndex = it.next();
        const RenderCommand& command = commandData[bitIndex];

        CommandProcAsync::CommandStatus status = CommandProcAsync::processCommand(procData, command, prevBandFy1, nextBandFy0);
        pendingMask ^= BitOps::indexAsMask(bitIndex, status);
      }
#else
      BitOps::BitIterator it(pendingMask);

      while (it.hasNext()) {
        uint32_t bitIndex = it.next();
        if (bandQy0 >= commandQuantizedY0[bitIndex]) {
          const RenderCommand& command = commandData[bitIndex];
          CommandProcAsync::CommandStatus status = CommandProcAsync::processCommand(procData, command, prevBandFy1, nextBandFy0);
          pendingMask ^= BitOps::indexAsMask(bitIndex, status);
        }
      }
#endif
      *bitSetPtr = pendingMask;
    }

    if (++bitSetPtr >= bitSetEndMinus1) {
      pendingGlobalMask = 0;
      if (bitSetPtr > bitSetEndMinus1)
        break;
    }

    commandData += IntOps::bitSizeOf<BLBitWord>();
    commandQuantizedY0 += IntOps::bitSizeOf<BLBitWord>();

    if (commandData == commandDataEnd) {
      commandQueue = commandQueue->next();
      BL_ASSERT(commandQueue != nullptr);

      commandData = commandQueue->data();
      commandDataEnd = commandQueue->end();
      commandQuantizedY0 = commandQueue->_quantizedY0;
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

  uint32_t workerCount = batch->workerCount();
  uint32_t bandCount = batch->bandCount();

  // We can process several consecutive bands at once when there is enough of bands for all the threads.
  //
  // TODO: [Rendering Context] At the moment this feature is not used as it regressed bl_bench using 4+ threads.
  uint32_t consecutiveBandCount = 1;

  uint32_t bandId = workData->workerId() * consecutiveBandCount;
  uint32_t consecutiveIndex = 0;

  uint32_t currentBandId = bandId + consecutiveIndex;
  uint32_t prevBandId = currentBandId;

  while (currentBandId < bandCount) {
    // Calculate the next band so we can pass it to `processBand()`.
    if (++consecutiveIndex == consecutiveBandCount) {
      consecutiveIndex = 0;
      bandId += workerCount * consecutiveBandCount;
    }

    uint32_t nextBandId = bandId + consecutiveIndex;
    processBand(procData, currentBandId, prevBandId, nextBandId);

    prevBandId = currentBandId;
    currentBandId = nextBandId;
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
