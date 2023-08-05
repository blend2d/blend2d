// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "bitarray_p.h"
#include "object_p.h"
#include "runtime_p.h"

namespace BLBitArrayPrivate {

// BLBitArray - Private - Commons
// ==============================

static constexpr size_t kSSOWordCapacity = uint32_t(BLBitArray::kSSOWordCount);
static constexpr size_t kSSOBitCapacity = kSSOWordCapacity * uint32_t(BLBitArrayOps::kNumBits);

static BL_INLINE_NODEBUG constexpr size_t bitIndexOf(size_t wordIndex) noexcept { return wordIndex * BLBitArrayOps::kNumBits; }
static BL_INLINE_NODEBUG constexpr size_t wordIndexOf(size_t bitIndex) noexcept { return bitIndex / BLBitArrayOps::kNumBits; }

static BL_INLINE_NODEBUG constexpr size_t wordCountFromBitCount(size_t bitCount) noexcept {
  return BL_TARGET_ARCH_BITS >= 64 ? uint32_t((uint64_t(bitCount) + BLBitArrayOps::kBitMask) / BLBitArrayOps::kNumBits)
                                   : (bitCount / BLBitArrayOps::kNumBits) + uint32_t((bitCount & BLBitArrayOps::kBitMask) != 0u);
}

static BL_INLINE_NODEBUG constexpr size_t bitCountFromWordCount(size_t wordCount) noexcept {
  return blMin<size_t>(wordCount * BLBitArrayOps::kNumBits, 0xFFFFFFFFu);
}

static BL_INLINE_NODEBUG constexpr BLObjectImplSize implSizeFromWordCapacity(size_t wordCapacity) noexcept {
  return BLObjectImplSize(sizeof(BLBitArrayImpl) + wordCapacity * sizeof(uint32_t));
}

static BL_INLINE_NODEBUG constexpr size_t wordCapacityFromImplSize(BLObjectImplSize implSize) noexcept {
  return (implSize.value() - sizeof(BLBitArrayImpl)) / sizeof(uint32_t);
}

static BL_INLINE_NODEBUG BLObjectImplSize expandImplSize(BLObjectImplSize implSize) noexcept {
  return blObjectExpandImplSize(implSize);
}

// BLBitArray - Private - SSO Representation
// =========================================

static BL_INLINE BLResult initSSO(BLBitArrayCore* self, size_t size = 0) noexcept {
  self->_d.initStatic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_BIT_ARRAY) | BLObjectInfo::fromAbcp(0, 0, 0, uint32_t(size)));
  return BL_SUCCESS;
}

static BL_INLINE void setSSOSize(BLBitArrayCore* self, size_t newSize) noexcept {
  BL_ASSERT(self->_d.sso());
  self->_d.info.setPField(uint32_t(newSize));
}

// BLBitArray - Private - Memory Management
// ========================================

static BL_INLINE BLResult initDynamic(BLBitArrayCore* self, BLObjectImplSize implSize, size_t size = 0u) noexcept {
  BL_ASSERT(size <= UINT32_MAX);

  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_BIT_ARRAY);
  BL_PROPAGATE(BLObjectPrivate::allocImplT<BLBitArrayImpl>(self, info, implSize));

  BLBitArrayImpl* impl = getImpl(self);
  impl->capacity = uint32_t(bitCountFromWordCount(wordCapacityFromImplSize(implSize)));
  impl->size = uint32_t(size);
  return BL_SUCCESS;
}

// BLBitArray - Private - Modify Op
// ================================

// A helper function that makes the BitArray mutable, but only if `from` is within its bounds.
static BL_NOINLINE BLResult makeMutableForModifyOp(BLBitArrayCore* self, size_t from, BitData* out) noexcept {
  if (self->_d.sso()) {
    size_t size = getSSOSize(self);
    if (from >= size)
      return blTraceError(BL_ERROR_INVALID_VALUE);

    *out = BitData{self->_d.u32_data, getSSOSize(self)};
    return BL_SUCCESS;
  }
  else {
    BLBitArrayImpl* selfI = getImpl(self);
    size_t size = selfI->size;

    if (from >= size)
      return blTraceError(BL_ERROR_INVALID_VALUE);

    if (isImplMutable(selfI)) {
      *out = BitData{selfI->data(), size};
      return BL_SUCCESS;
    }

    BLBitArrayCore newO;
    if (size <= kSSOBitCapacity) {
      initSSO(&newO, size);

      *out = BitData{self->_d.u32_data, size};
      return replaceInstance(self, &newO);
    }

    BL_PROPAGATE(initDynamic(&newO, implSizeFromWordCapacity(wordCountFromBitCount(size)), size));
    *out = BitData{getImpl(&newO)->data(), size};
    return replaceInstance(self, &newO);
  }
}

// Returns the original size of the BitArray when passed to this function (basically it returns the index where to append the bits).
static BL_NOINLINE BLResult makeMutableForAppendOp(BLBitArrayCore* self, size_t appendBitCount, size_t* bitIndex, BitData* out) noexcept {
  BL_ASSERT(appendBitCount > 0u);

  BitData d;
  if (self->_d.sso()) {
    d = BitData{self->_d.u32_data, getSSOSize(self)};
    *bitIndex = d.size;

    size_t remainingCapacity = size_t(kSSOBitCapacity) - d.size;
    if (appendBitCount <= remainingCapacity) {
      size_t newSize = d.size + appendBitCount;
      setSSOSize(self, newSize);

      *out = BitData{d.data, newSize};
      return BL_SUCCESS;
    }

    if (BL_UNLIKELY(appendBitCount > size_t(UINT32_MAX) - d.size))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }
  else {
    BLBitArrayImpl* selfI = getImpl(self);

    d = BitData{selfI->data(), selfI->size};
    *bitIndex = d.size;

    size_t remainingCapacity = size_t(selfI->capacity) - d.size;
    size_t mutableMsk = BLIntOps::bitMaskFromBool<size_t>(isImplMutable(selfI));

    if (appendBitCount <= (remainingCapacity & mutableMsk)) {
      size_t newSize = d.size + appendBitCount;
      size_t fromWord = wordIndexOf(d.size + BLBitArrayOps::kBitMask);
      size_t lastWord = wordIndexOf(newSize - 1u);

      BLMemOps::fillInlineT(d.data + fromWord, uint32_t(0), lastWord - fromWord + 1);
      selfI->size = uint32_t(newSize);

      *out = BitData{d.data, newSize};
      return BL_SUCCESS;
    }
  }

  BLOverflowFlag of = 0;
  size_t newSize = BLIntOps::addOverflow(d.size, appendBitCount, &of);

  if (BL_UNLIKELY(of))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  size_t oldWordCount = wordCountFromBitCount(d.size);
  size_t newWordCount = wordCountFromBitCount(newSize);
  BLObjectImplSize implSize = implSizeFromWordCapacity(newWordCount);

  BLBitArrayCore newO;
  BL_PROPAGATE(initDynamic(&newO, expandImplSize(implSize), newSize));

  BLBitArrayImpl* newI = getImpl(&newO);
  BLMemOps::copyForwardInlineT(newI->data(), d.data, oldWordCount);
  BLMemOps::fillInlineT(newI->data() + oldWordCount, uint32_t(0), newWordCount - oldWordCount);

  *out = BitData{newI->data(), newSize};
  return replaceInstance(self, &newO);
}

// BLBitArray - Private - Combine Op
// =================================

template<typename BitOp>
static BL_INLINE BLResult combineWordData(BitData d, size_t bitIndex, const uint32_t* wordData, size_t wordCount) noexcept {
  size_t bitEnd = bitIndex + blMin(bitIndexOf(wordCount), d.size - bitIndex);
  size_t bitCount = bitEnd - bitIndex;

  size_t wordIndex = wordIndexOf(bitIndex);
  uint32_t* dst = d.data + wordIndex;
  uint32_t bitShift = uint32_t(bitIndex & BLBitArrayOps::kBitMask);

  // Special case - if `wordData` is aligned to a word boundary, we don't have to shift the input BitWords.
  if (bitShift == 0u) {
    wordCount = blMin(wordCountFromBitCount(bitCount), wordCount);
    uint32_t endBitCount = uint32_t(bitEnd & BLBitArrayOps::kBitMask);

    size_t end = wordCount - size_t(endBitCount != 0);
    BLBitArrayOps::bitArrayCombineWords<BitOp>(dst, wordData, end);

    if (endBitCount)
      dst[end] = BitOp::opMasked(dst[end], wordData[end], BLBitArrayOps::nonZeroStartMask(endBitCount));

    return BL_SUCCESS;
  }

  uint32_t w = wordData[0];
  uint32_t bitShiftInv = BLBitArrayOps::kNumBits - bitShift;

  // Special case - if the number of processed bits is less than number of the remaining bits in the current BitWord.
  if (bitCount <= bitShiftInv) {
    uint32_t mask = BLBitArrayOps::nonZeroStartMask(bitCount, bitShift);
    dst[0] = BitOp::opMasked(dst[0], BLBitArrayOps::shiftToEnd(w, bitShift), mask);
    return BL_SUCCESS;
  }

  // Process the first BitWord, which is not fully combined (must combine under a write-mask).
  dst[0] = BitOp::opMasked(dst[0], BLBitArrayOps::shiftToEnd(w, bitShift), BLBitArrayOps::nonZeroEndMask(bitShiftInv));
  bitCount -= bitShiftInv;

  // Process guaranteed BitWord quantities.
  size_t i = 1;
  size_t n = wordIndexOf(bitCount);

  while (i <= n) {
    uint32_t prevWordBits = BLBitArrayOps::shiftToStart(w, bitShiftInv);
    w = wordData[i];
    dst[i] = BitOp::op(dst[i], prevWordBits | BLBitArrayOps::shiftToEnd(w, bitShift));

    i++;
  }

  bitCount &= BLBitArrayOps::kBitMask;
  if (bitCount == 0)
    return BL_SUCCESS;

  uint32_t lastWordBits = BLBitArrayOps::shiftToStart(w, bitShiftInv);
  if (bitShiftInv < bitCount)
    lastWordBits |= BLBitArrayOps::shiftToEnd(wordData[i], bitShift);

  dst[i] = BitOp::opMasked(dst[i], lastWordBits, BLBitArrayOps::nonZeroStartMask(bitCount));
  return BL_SUCCESS;
}

} // {BLBitArrayPrivate}

// BLBitArray - API - Init & Destroy
// =================================

BL_API_IMPL BLResult blBitArrayInit(BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  return initSSO(self);
}

BL_API_IMPL BLResult blBitArrayInitMove(BLBitArrayCore* self, BLBitArrayCore* other) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(other->_d.isBitArray());

  BLBitArrayCore tmp = *other;
  initSSO(other);
  *self = tmp;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blBitArrayInitWeak(BLBitArrayCore* self, const BLBitArrayCore* other) noexcept {
  using namespace BLBitArrayPrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isBitArray());

  self->_d = other->_d;
  return retainInstance(self);
}

BL_API_IMPL BLResult blBitArrayDestroy(BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  return releaseInstance(self);
}

// BLBitArray - API - Reset
// ========================

BL_API_IMPL BLResult blBitArrayReset(BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  releaseInstance(self);
  return initSSO(self);
}

// BLBitArray - API - Assign
// =========================

BL_API_IMPL BLResult blBitArrayAssignMove(BLBitArrayCore* self, BLBitArrayCore* other) noexcept {
  using namespace BLBitArrayPrivate;

  BL_ASSERT(self->_d.isBitArray());
  BL_ASSERT(other->_d.isBitArray());

  BLBitArrayCore tmp = *other;
  initSSO(other);
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blBitArrayAssignWeak(BLBitArrayCore* self, const BLBitArrayCore* other) noexcept {
  using namespace BLBitArrayPrivate;

  BL_ASSERT(self->_d.isBitArray());
  BL_ASSERT(other->_d.isBitArray());

  retainInstance(other);
  return replaceInstance(self, other);
}

BL_API_IMPL BLResult blBitArrayAssignWords(BLBitArrayCore* self, const uint32_t* wordData, uint32_t wordCount) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  if (self->_d.sso()) {
    if (wordCount <= kSSOWordCapacity) {
      setSSOSize(self, wordCount * BLBitArrayOps::kNumBits);
      BLMemOps::copyForwardInlineT(self->_d.u32_data, wordData, wordCount);
      BLMemOps::fillInlineT(self->_d.u32_data + wordCount, uint32_t(0), kSSOWordCapacity - wordCount);
      return BL_SUCCESS;
    }
  }
  else {
    BLBitArrayImpl* selfI = getImpl(self);

    size_t capacityInWords = wordCountFromBitCount(selfI->capacity);
    size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isImplMutable(selfI));

    if ((wordCount | immutableMsk) > capacityInWords) {
      BLBitArrayCore newO;
      initSSO(&newO, size_t(wordCount) * BLBitArrayOps::kNumBits);
      BLMemOps::copyForwardInlineT(newO._d.u32_data, wordData, wordCount);

      return replaceInstance(self, &newO);
    }
  }

  BLBitArrayCore newO;
  BL_PROPAGATE(initDynamic(&newO, implSizeFromWordCapacity(wordCount)));

  BLBitArrayImpl* newI = getImpl(&newO);
  BLMemOps::copyForwardInlineT(newI->data(), wordData, wordCount);

  return replaceInstance(self, &newO);
}

// BLBitArray - API - Accessors
// ============================

BL_API_IMPL bool blBitArrayIsEmpty(const BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  return getSize(self) == 0;
}

BL_API_IMPL uint32_t blBitArrayGetSize(const BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  return uint32_t(getSize(self));
}

BL_API_IMPL uint32_t BL_CDECL blBitArrayGetWordCount(const BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  return uint32_t(wordCountFromBitCount(getSize(self)));
}

BL_API_IMPL uint32_t blBitArrayGetCapacity(const BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  return uint32_t(getCapacity(self));
}

BL_API_IMPL const uint32_t* blBitArrayGetData(const BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d = unpack(self);
  return d.data;
}

BL_API_IMPL uint32_t blBitArrayGetCardinality(const BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d = unpack(self);
  if (!d.size)
    return 0u;

  BLIntOps::PopCounter<uint32_t> counter;
  counter.addArray(d.data, wordCountFromBitCount(d.size));
  return counter.get();
}

BL_API_IMPL uint32_t blBitArrayGetCardinalityInRange(const BLBitArrayCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d = unpack(self);
  size_t start = startBit;
  size_t end = blMin<size_t>(endBit, d.size);

  if (start >= end)
    return 0u;

  size_t startWord = wordIndexOf(start);
  size_t lastWord = wordIndexOf(end - 1u);
  BLIntOps::PopCounter<uint32_t> counter;

  if (startWord == lastWord) {
    // Special case - the range is within a single BitWord.
    uint32_t mask = BLBitArrayOps::nonZeroStartMask(end - start, startBit);
    counter.addItem(d.data[startWord] & mask);
  }
  else {
    uint32_t startMask = BLBitArrayOps::nonZeroEndMask(BLBitArrayOps::kNumBits - uint32_t(start & BLBitArrayOps::kBitMask));
    uint32_t endMask = BLBitArrayOps::nonZeroStartMask((uint32_t(end - 1u) & BLBitArrayOps::kBitMask) + 1u);

    counter.addItem(d.data[startWord] & startMask);
    counter.addArray(d.data + startWord + 1, lastWord - startWord - 1);
    counter.addItem(d.data[lastWord] & endMask);
  }

  return counter.get();
}

BL_API_IMPL bool blBitArrayHasBit(const BLBitArrayCore* self, uint32_t bitIndex) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d = unpack(self);
  if (bitIndex >= d.size)
    return false;

  return BLBitArrayOps::bitArrayTestBit(d.data, bitIndex);
}

BL_API_IMPL bool blBitArrayHasBitsInRange(const BLBitArrayCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d = unpack(self);
  size_t start = startBit;
  size_t end = blMin(d.size, size_t(endBit));

  if (start >= end)
    return false;

  size_t startWord = wordIndexOf(start);
  size_t endWord = wordIndexOf(end);

  if (startWord == endWord) {
    // Special case - the range is within a single BitWord.
    uint32_t mask = BLBitArrayOps::nonZeroStartMask(end - start, start);
    return (d.data[startWord] & mask) != 0u;
  }

  uint32_t startMask = BLBitArrayOps::nonZeroEndMask(BLBitArrayOps::kNumBits - (start & BLBitArrayOps::kBitMask));
  if (d.data[startWord] & startMask)
    return true;

  for (size_t i = startWord + 1; i < endWord; i++)
    if (d.data[i])
      return true;

  uint32_t endMask = BLBitArrayOps::nonZeroStartMask(end & BLBitArrayOps::kBitMask);
  return (d.data[endWord] & endMask) != 0u;
}

// BLBitArray - API - Testing
// ==========================

BL_API_IMPL bool blBitArraySubsumes(const BLBitArrayCore* a, const BLBitArrayCore* b) noexcept {
  using namespace BLBitArrayPrivate;

  BL_ASSERT(a->_d.isBitArray());
  BL_ASSERT(b->_d.isBitArray());

  BitData ad = unpack(a);
  BitData bd = unpack(b);

  size_t sharedWordCount = wordCountFromBitCount(blMin(ad.size, bd.size));
  for (size_t i = 0; i < sharedWordCount; i++)
    if ((ad.data[i] & bd.data[i]) != bd.data[i])
      return false;

  size_t bWordCount = wordCountFromBitCount(bd.size);
  for (size_t i = sharedWordCount; i < bWordCount; i++)
    if (bd.data[i])
      return false;

  return true;
}

BL_API_IMPL bool blBitArrayIntersects(const BLBitArrayCore* a, const BLBitArrayCore* b) noexcept {
  using namespace BLBitArrayPrivate;

  BL_ASSERT(a->_d.isBitArray());
  BL_ASSERT(b->_d.isBitArray());

  BitData ad = unpack(a);
  BitData bd = unpack(b);

  size_t sharedWordCount = wordCountFromBitCount(blMin(ad.size, bd.size));
  for (size_t i = 0; i < sharedWordCount; i++)
    if ((ad.data[i] & bd.data[i]) != 0u)
      return true;

  return false;
}

BL_API_IMPL bool blBitArrayGetRange(const BLBitArrayCore* self, uint32_t* startOut, uint32_t* endOut) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d = unpack(self);
  size_t wordCount = wordCountFromBitCount(d.size);

  for (size_t i = 0; i < wordCount; i++) {
    uint32_t bits = d.data[i];
    if (bits) {
      size_t start = bitIndexOf(i) + BLBitArrayOps::countZerosFromStart(bits);
      for (size_t j = wordCount; j != 0; j--) {
        bits = d.data[j - 1];
        if (bits) {
          size_t end = bitIndexOf(j) - BLBitArrayOps::countZerosFromEnd(bits);
          *startOut = uint32_t(start);
          *endOut = uint32_t(end);
          return true;
        }
      }
    }
  }

  // There are no bits set in this BitArray.
  *startOut = 0u;
  *endOut = 0u;
  return false;
}

// BLBitArray - API - Equality & Comparison
// ========================================

BL_API_IMPL bool blBitArrayEquals(const BLBitArrayCore* a, const BLBitArrayCore* b) noexcept {
  using namespace BLBitArrayPrivate;

  BL_ASSERT(a->_d.isBitArray());
  BL_ASSERT(b->_d.isBitArray());

  BitData ad = unpack(a);
  BitData bd = unpack(b);

  if (ad.size != bd.size)
    return false;

  size_t wordCount = wordCountFromBitCount(ad.size);
  for (size_t i = 0; i < wordCount; i++)
    if (ad.data[i] != bd.data[i])
      return false;

  return true;
}

BL_API_IMPL int blBitArrayCompare(const BLBitArrayCore* a, const BLBitArrayCore* b) noexcept {
  using namespace BLBitArrayPrivate;

  BL_ASSERT(a->_d.isBitArray());
  BL_ASSERT(b->_d.isBitArray());

  BitData ad = unpack(a);
  BitData bd = unpack(b);

  size_t minSize = blMin(ad.size, bd.size);
  size_t wordCount = wordCountFromBitCount(minSize);

  // We don't need any masking here - bits in a BitWord that are outside of a BitArray range must be zero. If one
  // of the BitArray has a greater size and any bit not used by the other filled, it's would compare as greater.
  for (size_t i = 0; i < wordCount; i++)
    if (ad.data[i] != bd.data[i])
      return BLBitArrayOps::compare(ad.data[i], bd.data[i]);

  return ad.size < bd.size ? -1 : int(ad.size > bd.size);
}

// BLBitArray - API - Manipulation - Clear
// =======================================

BL_API_IMPL BLResult blBitArrayClear(BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  if (self->_d.sso())
    return initSSO(self);

  BLBitArrayImpl* selfI = getImpl(self);
  if (isImplMutable(selfI)) {
    selfI->size = 0;
    return BL_SUCCESS;
  }
  else {
    releaseInstance(self);
    initSSO(self);
    return BL_SUCCESS;
  }
}

// BLBitArray - API - Manipulation - Resize
// ========================================

BL_API_IMPL BLResult blBitArrayResize(BLBitArrayCore* self, uint32_t nBits) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d;

  if (self->_d.sso()) {
    d = BitData{getSSOData(self), getSSOSize(self)};
    if (nBits <= kSSOBitCapacity) {
      if (nBits < d.size) {
        // SSO mode requires ALL bits outside of the range to be set to zero.
        size_t i = wordIndexOf(nBits);

        if (nBits & BLBitArrayOps::kBitMask)
          d.data[i++] &= BLBitArrayOps::nonZeroStartMask(nBits & BLBitArrayOps::kBitMask);

        while (i < kSSOWordCapacity)
          d.data[i++] = 0;
      }

      setSSOSize(self, nBits);
      return BL_SUCCESS;
    }
  }
  else {
    BLBitArrayImpl* selfI = getImpl(self);
    size_t immutableMask = BLIntOps::bitMaskFromBool<size_t>(!isImplMutable(selfI));

    d = BitData{selfI->data(), selfI->size};
    if ((nBits | immutableMask) <= selfI->capacity) {
      if (nBits < d.size) {
        size_t i = wordIndexOf(nBits);
        if (nBits & BLBitArrayOps::kBitMask)
          d.data[i] &= BLBitArrayOps::nonZeroStartMask(nBits & BLBitArrayOps::kBitMask);
      }
      else {
        size_t from = wordIndexOf(d.size + BLBitArrayOps::kBitMask);
        size_t end = wordCountFromBitCount(nBits);
        BLMemOps::fillInlineT(d.data + from, uint32_t(0), end - from);
      }

      selfI->size = uint32_t(nBits);
      return BL_SUCCESS;
    }
  }

  BLBitArrayCore newO;
  uint32_t* dst;

  if (nBits <= kSSOBitCapacity) {
    initSSO(&newO, nBits);
    dst = newO._d.u32_data;
  }
  else {
    BLObjectImplSize implSize = implSizeFromWordCapacity(wordCountFromBitCount(nBits));
    BL_PROPAGATE(initDynamic(&newO, implSize, nBits));
    dst = getImpl(&newO)->data();
  }

  size_t bitCount = blMin<size_t>(nBits, d.size);
  size_t wordCount = wordCountFromBitCount(bitCount);

  BLMemOps::copyForwardInlineT(dst, d.data, wordCount);
  uint32_t lastWordBitCount = uint32_t(bitCount & BLBitArrayOps::kBitMask);

  if (lastWordBitCount)
    dst[wordCount - 1] &= BLBitArrayOps::nonZeroStartMask(lastWordBitCount);

  return replaceInstance(self, &newO);
}

// BLBitArray - API - Manipulation - Reserve
// =========================================

BL_API_IMPL BLResult blBitArrayReserve(BLBitArrayCore* self, uint32_t nBits) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d;
  if (self->_d.sso()) {
    if (nBits <= kSSOBitCapacity)
      return BL_SUCCESS;

    d = BitData{getSSOData(self), getSSOSize(self)};
  }
  else {
    BLBitArrayImpl* selfI = getImpl(self);
    size_t immutableMask = BLIntOps::bitMaskFromBool<size_t>(!isImplMutable(selfI));

    if ((nBits | immutableMask) <= selfI->capacity)
      return BL_SUCCESS;

    d = BitData{selfI->data(), selfI->size};
  }

  BLObjectImplSize implSize = implSizeFromWordCapacity(wordCountFromBitCount(nBits));
  BLBitArrayCore newO;
  BL_PROPAGATE(initDynamic(&newO, implSize, d.size));

  BLBitArrayImpl* newI = getImpl(&newO);
  BLMemOps::copyForwardInlineT(newI->data(), d.data, wordCountFromBitCount(d.size));
  return replaceInstance(self, &newO);
}

// BLBitArray - API - Manipulation - Shrink
// ========================================

BL_API_IMPL BLResult blBitArrayShrink(BLBitArrayCore* self) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  if (self->_d.sso())
    return BL_SUCCESS;

  BLBitArrayImpl* impl = getImpl(self);
  size_t size = impl->size;
  size_t capacity = impl->capacity;

  if (size <= kSSOBitCapacity) {
    BLBitArrayCore newO;
    initSSO(&newO, size);
    BLMemOps::copyForwardInlineT(newO._d.u32_data, impl->data(), wordCountFromBitCount(size));
    return replaceInstance(self, &newO);
  }

  BLObjectImplSize currentImplSize = implSizeFromWordCapacity(wordCountFromBitCount(capacity));
  BLObjectImplSize optimalImplSize = implSizeFromWordCapacity(wordCountFromBitCount(size));

  if (optimalImplSize + BL_OBJECT_IMPL_ALIGNMENT <= currentImplSize) {
    BLBitArrayCore newO;
    BL_PROPAGATE(initDynamic(&newO, optimalImplSize, size));

    BLBitArrayImpl* newI = getImpl(&newO);
    BLMemOps::copyForwardInlineT(newI->data(), impl->data(), wordCountFromBitCount(size));
    return replaceInstance(self, &newO);
  }

  return BL_SUCCESS;
}

// BLBitArray - API - Manipulation - Set / Fill
// ============================================

BL_API_IMPL BLResult blBitArraySetBit(BLBitArrayCore* self, uint32_t bitIndex) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d;
  BL_PROPAGATE(makeMutableForModifyOp(self, bitIndex, &d));

  BLBitArrayOps::bitArraySetBit(d.data, bitIndex);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blBitArrayFillRange(BLBitArrayCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  if (BL_UNLIKELY(startBit >= endBit))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BitData d;
  BL_PROPAGATE(makeMutableForModifyOp(self, startBit, &d));

  size_t end = blMin(size_t(endBit), d.size);
  BLBitArrayOps::bitArrayFill(d.data, startBit, end - startBit);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blBitArrayFillWords(BLBitArrayCore* self, uint32_t bitIndex, const uint32_t* wordData, uint32_t wordCount) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d;
  BL_PROPAGATE(makeMutableForModifyOp(self, bitIndex, &d));

  combineWordData<BitOperator::Or>(d, bitIndex, wordData, wordCount);
  return BL_SUCCESS;
}

// BLBitArray - API - Manipulation - Clear
// =======================================

BL_API_IMPL BLResult blBitArrayClearBit(BLBitArrayCore* self, uint32_t bitIndex) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d;
  BL_PROPAGATE(makeMutableForModifyOp(self, bitIndex, &d));

  BLBitArrayOps::bitArrayClearBit(d.data, bitIndex);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blBitArrayClearRange(BLBitArrayCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  if (BL_UNLIKELY(startBit >= endBit))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BitData d;
  BL_PROPAGATE(makeMutableForModifyOp(self, startBit, &d));

  size_t end = blMin(size_t(endBit), d.size);
  BLBitArrayOps::bitArrayClear(d.data, startBit, end - startBit);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blBitArrayClearWord(BLBitArrayCore* self, uint32_t bitIndex, uint32_t wordValue) noexcept {
  return blBitArrayClearWords(self, bitIndex, &wordValue, 1);
}

BL_API_IMPL BLResult blBitArrayClearWords(BLBitArrayCore* self, uint32_t bitIndex, const uint32_t* wordData, uint32_t wordCount) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d;
  BL_PROPAGATE(makeMutableForModifyOp(self, bitIndex, &d));

  combineWordData<BitOperator::AndNot>(d, bitIndex, wordData, wordCount);
  return BL_SUCCESS;
}

// BLBitArray - API - Manipulation - Replace
// =========================================

BL_API_IMPL BLResult blBitArrayReplaceOp(BLBitArrayCore* self, uint32_t nBits, uint32_t** dataOut) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BLBitArrayCore newO;
  size_t wordCount = wordCountFromBitCount(nBits);
  uint32_t* dst = nullptr;

  // Not a real loop, just to be able to jump to the end without the use of the hated 'goto'.
  do {
    if (self->_d.sso()) {
      if (nBits <= kSSOBitCapacity) {
        initSSO(self, nBits);

        *dataOut = getSSOData(self);
        return BL_SUCCESS;
      }
    }
    else {
      BLBitArrayImpl* selfI = getImpl(self);
      size_t immutableMask = BLIntOps::bitMaskFromBool<size_t>(!isImplMutable(selfI));

      if ((nBits | immutableMask) <= selfI->capacity) {
        dst = selfI->data();
        selfI->size = uint32_t(nBits);

        // Using the passed instance's Impl, it's mutable and it has enough capacity.
        break;
      }
      else if (nBits <= kSSOBitCapacity) {
        releaseInstance(self);
        initSSO(self, nBits);

        *dataOut = getSSOData(self);
        return BL_SUCCESS;
      }
    }

    BLObjectImplSize implSize = implSizeFromWordCapacity(wordCountFromBitCount(nBits));
    BL_PROPAGATE(initDynamic(&newO, implSize, nBits));
    releaseInstance(self);

    dst = getImpl(&newO)->data();
    *self = newO;
  } while (0);

  // We don't know whether the C++ compiler would decide to unroll this one, that's it's only once in the body.
  BLMemOps::fillInlineT(dst, uint32_t(0), wordCount);

  *dataOut = dst;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blBitArrayReplaceBit(BLBitArrayCore* self, uint32_t bitIndex, bool bitValue) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  if (bitValue)
    return blBitArraySetBit(self, bitIndex);
  else
    return blBitArrayClearBit(self, bitIndex);
}

BL_API_IMPL BLResult blBitArrayReplaceWord(BLBitArrayCore* self, uint32_t bitIndex, uint32_t wordValue) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  return blBitArrayReplaceWords(self, bitIndex, &wordValue, 1);
}

BL_API_IMPL BLResult blBitArrayReplaceWords(BLBitArrayCore* self, uint32_t bitIndex, const uint32_t* wordData, uint32_t wordCount) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d;
  BL_PROPAGATE(makeMutableForModifyOp(self, bitIndex, &d));

  combineWordData<BitOperator::Assign>(d, bitIndex, wordData, wordCount);
  return BL_SUCCESS;
}

// BLBitArray - API - Manipulation - Append
// ========================================

BL_API_IMPL BLResult blBitArrayAppendBit(BLBitArrayCore* self, bool bitValue) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  BitData d;
  size_t bitIndex;
  BL_PROPAGATE(makeMutableForAppendOp(self, 1u, &bitIndex, &d));

  BLBitArrayOps::bitArrayOrBit(d.data, bitIndex, bitValue);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blBitArrayAppendWord(BLBitArrayCore* self, uint32_t wordValue) noexcept {
  return blBitArrayAppendWords(self, &wordValue, 1);
}

BL_API_IMPL BLResult blBitArrayAppendWords(BLBitArrayCore* self, const uint32_t* wordData, uint32_t wordCount) noexcept {
  using namespace BLBitArrayPrivate;
  BL_ASSERT(self->_d.isBitArray());

  if (!wordCount)
    return BL_SUCCESS;

  BitData d;
  size_t bitIndex;
  BL_PROPAGATE(makeMutableForAppendOp(self, size_t(wordCount) * BLBitArrayOps::kNumBits, &bitIndex, &d));

  combineWordData<BitOperator::Or>(d, bitIndex, wordData, wordCount);
  return BL_SUCCESS;
}

// BLBitArray - Runtime Registration
// =================================

void blBitArrayRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);
  BLBitArrayPrivate::initSSO(static_cast<BLBitArrayCore*>(&blObjectDefaults[BL_OBJECT_TYPE_BIT_ARRAY]));
}
