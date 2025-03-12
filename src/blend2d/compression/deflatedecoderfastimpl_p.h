// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../runtime_p.h"
#include "../compression/deflatedecoder_p.h"
#include "../compression/deflatedecoderfast_p.h"
#include "../compression/deflatedecoderutils_p.h"
#include "../support/memops_p.h"
#include "../support/intops_p.h"
#include "../support/ptrops_p.h"

namespace bl {
namespace Compression {
namespace Deflate {
namespace Fast {
namespace {

DecoderFastResult decodeImpl(
  Decoder* ctx,
  uint8_t* dstStart,
  uint8_t* dstPtr,
  uint8_t* dstEnd,
  const uint8_t* srcPtr,
  const uint8_t* srcEnd
) noexcept {
  constexpr size_t kCopyRegSize = sizeof(CopyContext::Register);

  DecoderBits bits;
  bits.loadState(ctx);

  const DecodeTables& tables = ctx->tables;

  DecoderTableMask litlenTableMask(ctx->_litlenFastTableBits);
  DecoderTableMask offsetTableMask(ctx->_offsetTableInfo.tableBits);

  for (;;) {
    // Destination and source pointer conditions:
    //  - at least one full refill and 8 additional bytes must be available to enter the fast loop.
    //  - at least one full match must be possible for decoding one entry (thus `kMaxMatchLen + kDstBytesPerIter`).
    intptr_t srcRemainingIters = intptr_t(PtrOps::bytesUntil(srcPtr, srcEnd) >> kSrcBytesPerIterShift);
    intptr_t dstRemainingIters = intptr_t(PtrOps::bytesUntil(dstPtr, dstEnd) >> kDstBytesPerIterShift);

    // We can write up to kDstBytesPerIter bytes + one full match each iteration - if more bytes are written,
    // safeIters is recalculated.
    intptr_t safeIters = blMin(srcRemainingIters - intptr_t(kSrcMinScratchShifted),
                               dstRemainingIters - intptr_t(kDstMinScratchShifted));

    // NOTE: If safeIters is low it will keep jumping to FastLoop_Restart too often, sometimes even after each
    // iteration, so we really want a reasonable number of iterations to execute before recalculating.
    if (safeIters <= intptr_t(kMinimumFastIterationCount)) {
      break;
    }

    BL_DECODER_UPDATE_STATISTICS(ctx->statistics.fast.numRestarts++);
    srcPtr += bits.refillBitWord(MemOps::loadu_le<BLBitWord>(srcPtr));

    // Decode one entry ahead here.
    DecodeEntry entry = tables.litlen_decode_table[bits.extract(litlenTableMask)];

    while (safeIters > 0) {
      // Make sure that the safe loop assumption is not breached - if any of the following assertion fails
      // it means that there is a bug, which must be fixed. The possible bug will be in the offset handler.
      BL_ASSERT(PtrOps::bytesUntil(dstPtr, dstEnd) >= kDstBytesPerIter + kMaxMatchLen);
      BL_ASSERT(PtrOps::bytesUntil(srcPtr, srcEnd) >= kSrcBytesPerIter);

      BLBitWord refillData = MemOps::loadu_le<BLBitWord>(srcPtr);

      srcPtr += bits.refillBitWord(refillData);
      uint32_t payload = DecoderUtils::rawPayload(entry);

      if (DecoderUtils::isLiteral(entry)) {
        bits.consumed(entry);
        size_t entryIndex = bits.extract(litlenTableMask);

        uint32_t litBits = entry.value & 0xFFu;
        uint32_t litCount = litBits >> 6;

        entry = tables.litlen_decode_table[entryIndex];

        if (BL_LIKELY(DecoderUtils::isLiteral(entry))) {
          bits.consumed(entry);
          entryIndex = bits.extract(litlenTableMask);

          payload += (entry.value >> 8) << (litCount * 8u);
          litCount += (entry.value & 0xFFu) >> 6;
          entry = tables.litlen_decode_table[entryIndex];

          MemOps::storeu_le(dstPtr, payload);
          dstPtr += litCount;

          safeIters--;
          continue;
        }
        else {
          MemOps::storeu_le(dstPtr, uint16_t(payload));
          dstPtr += litCount;
          payload = DecoderUtils::rawPayload(entry);
        }
      }

      uint32_t length = payload + DecoderUtils::extractExtra(bits.bitWord, entry);

      if (BL_UNLIKELY(!DecoderUtils::isOffOrLen(entry))) {
        BLBitWord prevBits = bits.bitWord;
        entry = tables.litlen_decode_table[length & 0x7FFFu];

        payload = DecoderUtils::rawPayload(entry);
        refillData = MemOps::loadu_le<BLBitWord>(srcPtr);

        bits.consumed(entry);
        srcPtr += bits.refillBitWord(refillData);

        if (DecoderUtils::isLiteral(entry)) {
          entry = tables.litlen_decode_table[bits.extract(litlenTableMask)];
          *dstPtr++ = uint8_t(payload & 0xFFu);

          safeIters--;
          continue;
        }

        if (BL_UNLIKELY(DecoderUtils::isEndOfBlock(entry))) {
          if (BL_LIKELY(!DecoderUtils::isEndOfBlockInvalid(entry)))
            goto BlockDone;
          else
            goto ErrorInvalidData;
        }

        length = payload + DecoderUtils::extractExtra(prevBits, entry);
      }
      else {
        bits.consumed(entry);
      }

      // There must be space for the whole copy - if not it's a bug in the fast loop!
      BL_ASSERT(PtrOps::bytesUntil(dstPtr, dstEnd) >= length + kDstBytesPerIter);

      entry = tables.offset_decode_table[bits.extract(offsetTableMask)];
      uint32_t offset = DecoderUtils::rawPayload(entry) + DecoderUtils::extractExtra(bits.bitWord, entry);

      if (BL_UNLIKELY(!DecoderUtils::isOffOrLen(entry))) {
        entry = tables.offset_decode_table[offset];
        offset = DecoderUtils::rawPayload(entry) + DecoderUtils::extractExtra(bits.bitWord, entry);

        if (BL_UNLIKELY(DecoderUtils::isEndOfBlock(entry))) {
          goto ErrorInvalidData;
        }
      }

      size_t dstSize = PtrOps::byteOffset(dstStart, dstPtr);
      bits.consumed(entry);

      if (BL_UNLIKELY(offset > dstSize)) {
        goto ErrorInvalidData;
      }

      const uint8_t* matchPtr = dstPtr - offset;
      CopyContext::Register r0 = CopyContext::load(matchPtr);

      uint8_t* copyPtr = dstPtr;
      dstPtr += length;

      safeIters -= intptr_t((length + (kDstBytesPerIter - 1)) / kDstBytesPerIter);

      BL_DECODER_UPDATE_STATISTICS(ctx->statistics.fast.matchUpTo8 += unsigned(length <= 8));
      BL_DECODER_UPDATE_STATISTICS(ctx->statistics.fast.matchUpTo16 += unsigned(length <= 16));
      BL_DECODER_UPDATE_STATISTICS(ctx->statistics.fast.matchUpTo32 += unsigned(length <= 32));
      BL_DECODER_UPDATE_STATISTICS(ctx->statistics.fast.matchUpTo64 += unsigned(length <= 64));

      BL_DECODER_UPDATE_STATISTICS(ctx->statistics.fast.matchMoreThan8 += unsigned(length > 8));
      BL_DECODER_UPDATE_STATISTICS(ctx->statistics.fast.matchMoreThan16 += unsigned(length > 16));
      BL_DECODER_UPDATE_STATISTICS(ctx->statistics.fast.matchMoreThan32 += unsigned(length > 32));
      BL_DECODER_UPDATE_STATISTICS(ctx->statistics.fast.matchMoreThan64 += unsigned(length > 64));

      if (BL_LIKELY(offset >= kCopyRegSize)) {
        BL_DECODER_UPDATE_STATISTICS(ctx->statistics.fast.matchNear++);
        CopyContext::store(copyPtr, r0);

        r0 = CopyContext::load_raw(matchPtr + kCopyRegSize);
        entry = tables.litlen_decode_table[bits.extract(litlenTableMask)];
        matchPtr += kCopyRegSize * 2u;

        CopyContext::store_raw(copyPtr + kCopyRegSize, r0);
        copyPtr += kCopyRegSize * 2u;

        BL_NOUNROLL
        while (copyPtr < dstPtr) {
          r0 = CopyContext::load_raw(matchPtr);
          CopyContext::store_raw(copyPtr, r0);

          r0 = CopyContext::load_raw(matchPtr + kCopyRegSize);
          matchPtr += kCopyRegSize * 2u;

          CopyContext::store_raw(copyPtr + kCopyRegSize, r0);
          copyPtr += kCopyRegSize * 2u;
        }
      }
      else {
        CopyContext matchCtx;

        matchCtx.initRepeat(offset);
        r0 = matchCtx.repeat(r0);

        matchCtx.initRotate(offset);
        entry = tables.litlen_decode_table[bits.extract(litlenTableMask)];

        CopyContext::store(copyPtr, r0);
        copyPtr += kCopyRegSize;

        BL_NOUNROLL
        while (copyPtr < dstPtr) {
          r0 = matchCtx.rotate(r0);
          CopyContext::store(copyPtr, r0);
          copyPtr += kCopyRegSize;
        }
      }
    }
  }

  bits.fixLengthAfterFastLoop();
  bits.storeState(ctx);
  return DecoderFastResult{DecoderFastStatus::kOk, dstPtr, srcPtr};

BlockDone:
  bits.fixLengthAfterFastLoop();
  bits.storeState(ctx);
  return DecoderFastResult{DecoderFastStatus::kBlockDone, dstPtr, srcPtr};

ErrorInvalidData:
  bits.fixLengthAfterFastLoop();
  bits.storeState(ctx);
  return DecoderFastResult{DecoderFastStatus::kInvalidData, dstPtr, srcPtr};
}

} // {anonymous}
} // {Fast}
} // {Deflate}
} // {Compression}
} // {bl}
