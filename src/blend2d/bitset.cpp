// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "bitset_p.h"
#include "runtime_p.h"
#include "simd/simd_p.h"
#include "support/algorithm_p.h"
#include "support/bitops_p.h"
#include "support/intops_p.h"
#include "support/memops_p.h"
#include "support/scopedbuffer_p.h"

namespace bl {
namespace BitSetInternal {

// bl::BitSet - Constants
// ======================

static constexpr uint32_t kInitialImplSize = 128;

//! Number of temporary segments locally allocated in BitSet processing functions.
static constexpr uint32_t kTmpSegmentDataSize = 128;

// bl::BitSet - Bit/Word Utilities
// ===============================

static BL_INLINE_NODEBUG uint32_t bitIndexOf(uint32_t wordIndex) noexcept { return wordIndex * BitSetOps::kNumBits; }
static BL_INLINE_NODEBUG uint32_t wordIndexOf(uint32_t bitIndex) noexcept { return bitIndex / BitSetOps::kNumBits; }

static BL_INLINE_NODEBUG uint32_t alignBitDownToSegment(uint32_t bitIndex) noexcept { return bitIndex & ~uint32_t(kSegmentBitMask); }
static BL_INLINE_NODEBUG uint32_t alignWordDownToSegment(uint32_t wordIndex) noexcept { return wordIndex & ~uint32_t(kSegmentWordCount - 1u); }
static BL_INLINE_NODEBUG uint32_t alignWordUpToSegment(uint32_t wordIndex) noexcept { return (wordIndex + (kSegmentWordCount - 1)) & ~uint32_t(kSegmentWordCount - 1u); }

static BL_INLINE_NODEBUG bool isBitAlignedToSegment(uint32_t bitIndex) noexcept { return (bitIndex & kSegmentBitMask) == 0u; }
static BL_INLINE_NODEBUG bool isWordAlignedToSegment(uint32_t wordIndex) noexcept { return (wordIndex & (kSegmentWordCount - 1u)) == 0u; }

// bl::BitSet - PopCount
// =====================

static BL_NOINLINE uint32_t bitCount(const uint32_t* data, size_t n) noexcept {
  uint32_t count = 0;

  BL_NOUNROLL
  for (size_t i = 0; i < n; i++)
    if (data[i])
      count += IntOps::popCount(data[i]);

  return count;
}

// bl::BitSet - Segment Inserters
// ==============================

//! A helper struct that is used in places where a limited number of segments may be inserted.
template<size_t N>
struct StaticSegmentInserter {
  BLBitSetSegment _segments[N];
  uint32_t _count = 0;

  BL_INLINE_NODEBUG BLBitSetSegment* segments() noexcept { return _segments; }
  BL_INLINE_NODEBUG const BLBitSetSegment* segments() const noexcept { return _segments; }

  BL_INLINE_NODEBUG BLBitSetSegment& current() noexcept { return _segments[_count]; }
  BL_INLINE_NODEBUG const BLBitSetSegment& current() const noexcept { return _segments[_count]; }

  BL_INLINE BLBitSetSegment& prev() noexcept {
    BL_ASSERT(_count > 0);
    return _segments[_count - 1];
  }

  BL_INLINE const BLBitSetSegment& prev() const noexcept {
    BL_ASSERT(_count > 0);
    return _segments[_count - 1];
  }

  BL_INLINE_NODEBUG bool empty() const noexcept { return _count == 0;}
  BL_INLINE_NODEBUG uint32_t count() const noexcept { return _count; }

  BL_INLINE void advance() noexcept {
    BL_ASSERT(_count != N);
    _count++;
  }
};

//! A helper struct that is used in places where a dynamic number of segments is inserted.
struct DynamicSegmentInserter {
  BLBitSetSegment* _segments = nullptr;
  uint32_t _index = 0;
  uint32_t _capacity = 0;

  BL_INLINE_NODEBUG DynamicSegmentInserter() noexcept {}

  BL_INLINE_NODEBUG DynamicSegmentInserter(BLBitSetSegment* segments, uint32_t capacity) noexcept
    : _segments(segments),
      _index(0),
      _capacity(capacity) {}

  BL_INLINE_NODEBUG void reset(BLBitSetSegment* segments, uint32_t capacity) noexcept {
    _segments = segments;
    _index = 0;
    _capacity = capacity;
  }

  BL_INLINE_NODEBUG BLBitSetSegment* segments() noexcept { return _segments; }
  BL_INLINE_NODEBUG const BLBitSetSegment* segments() const noexcept { return _segments; }

  BL_INLINE BLBitSetSegment& current() noexcept {
    BL_ASSERT(_index < _capacity);
    return _segments[_index];
  }

  BL_INLINE const BLBitSetSegment& current() const noexcept {
    BL_ASSERT(_index < _capacity);
    return _segments[_index];
  }

  BL_INLINE BLBitSetSegment& prev() noexcept {
    BL_ASSERT(_index > 0);
    return _segments[_index - 1];
  }

  BL_INLINE const BLBitSetSegment& prev() const noexcept {
    BL_ASSERT(_index > 0);
    return _segments[_index - 1];
  }

  BL_INLINE_NODEBUG bool empty() const noexcept { return _index == 0; }
  BL_INLINE_NODEBUG uint32_t index() const noexcept { return _index; }
  BL_INLINE_NODEBUG uint32_t capacity() const noexcept { return _capacity; }

  BL_INLINE void advance() noexcept {
    BL_ASSERT(_index != _capacity);
    _index++;
  }
};

// bl::BitSet - Data Analysis
// ==========================

class QuickDataAnalysis {
public:
  uint32_t _accAnd;
  uint32_t _accOr;

  BL_INLINE_NODEBUG bool isZero() const noexcept { return _accOr == 0u; }
  BL_INLINE_NODEBUG bool isFull() const noexcept { return _accAnd == 0xFFFFFFFFu; }
};

static BL_INLINE QuickDataAnalysis quickDataAnalysis(const uint32_t* segmentWords) noexcept {
  uint32_t accAnd = segmentWords[0];
  uint32_t accOr = segmentWords[0];

  for (uint32_t i = 1; i < kSegmentWordCount; i++) {
    accOr |= segmentWords[i];
    accAnd &= segmentWords[i];
  }

  return QuickDataAnalysis{accAnd, accOr};
}

struct PreciseDataAnalysis {
  enum class Type : uint32_t {
    kDense = 0,
    kRange = 1,
    kEmpty = 2
  };

  Type type;
  uint32_t start;
  uint32_t end;

  BL_INLINE_NODEBUG bool empty() const noexcept { return type == Type::kEmpty; }
  BL_INLINE_NODEBUG bool isDense() const noexcept { return type == Type::kDense; }
  BL_INLINE_NODEBUG bool isRange() const noexcept { return type == Type::kRange; }
};

static PreciseDataAnalysis preciseDataAnalysis(uint32_t startWord, const uint32_t* data, uint32_t wordCount) noexcept {
  BL_ASSERT(wordCount > 0);

  // Finds the first non-zero word - in SSO dense data the termination should not be necessary as dense SSO data
  // should always contain at least one non-zero bit. However, we are defensive and return if all words are zero.
  uint32_t i = 0;
  uint32_t n = wordCount;

  BL_NOUNROLL
  while (data[i] == 0)
    if (++i == wordCount)
      return PreciseDataAnalysis{PreciseDataAnalysis::Type::kEmpty, 0, 0};

  // Finds the last non-zero word - this cannot fail as we have already found a non-zero word in `data`.
  BL_NOUNROLL
  while (data[--n] == 0)
    continue;

  uint32_t startZeros = BitSetOps::countZerosFromStart(data[i]);
  uint32_t endZeros = BitSetOps::countZerosFromEnd(data[n]);

  uint32_t rangeStart = bitIndexOf(startWord + i) + startZeros;
  uint32_t rangeEnd = bitIndexOf(startWord + n) + BitSetOps::kNumBits - endZeros;

  // Single word case.
  if (i == n) {
    uint32_t mask = BitSetOps::shiftToEnd(BitSetOps::nonZeroStartMask(BitSetOps::kNumBits - (startZeros + endZeros)), startZeros);
    return PreciseDataAnalysis{(PreciseDataAnalysis::Type)(data[i] == mask), rangeStart, rangeEnd};
  }

  PreciseDataAnalysis::Type type = PreciseDataAnalysis::Type::kRange;

  // Multiple word cases - checks both start & end words and verifies that all words in between have only ones.
  if (data[i] != BitSetOps::nonZeroEndMask(BitSetOps::kNumBits - startZeros) ||
      data[n] != BitSetOps::nonZeroStartMask(BitSetOps::kNumBits - endZeros)) {
    type = PreciseDataAnalysis::Type::kDense;
  }
  else {
    BL_NOUNROLL
    while (++i != n) {
      if (data[i] != BitSetOps::ones()) {
        type = PreciseDataAnalysis::Type::kDense;
        break;
      }
    }
  }

  return PreciseDataAnalysis{type, rangeStart, rangeEnd};
}

// bl::BitSet - SSO Range - Init
// =============================

static BL_INLINE BLResult initSSOEmpty(BLBitSetCore* self) noexcept {
  self->_d.initStatic(BLObjectInfo{BLBitSet::kSSOEmptySignature});
  return BL_SUCCESS;
}

static BL_INLINE BLResult initSSORange(BLBitSetCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  self->_d.initStatic(BLObjectInfo{BLBitSet::kSSOEmptySignature});
  return setSSORange(self, startBit, endBit);
}

// bl::BitSet - SSO Dense - Commons
// ================================

static BL_INLINE uint32_t getSSOWordCountFromData(const uint32_t* data, uint32_t n) noexcept {
  while (n && data[n - 1] == 0)
    n--;
  return n;
}

// bl::BitSet - SSO Dense - Init
// =============================

static BL_INLINE BLResult initSSODense(BLBitSetCore* self, uint32_t wordIndex) noexcept {
  BL_ASSERT(wordIndex <= kSSOLastWord);
  self->_d.initStatic(BLObjectInfo{BLBitSet::kSSODenseSignature});
  self->_d.u32_data[2] = wordIndex;
  return BL_SUCCESS;
}

static BL_INLINE BLResult initSSODenseWithData(BLBitSetCore* self, uint32_t wordIndex, const uint32_t* data, uint32_t n) noexcept {
  BL_ASSERT(n > 0 && n <= kSSOWordCount);
  initSSODense(self, wordIndex);
  MemOps::copyForwardInlineT(self->_d.u32_data, data, n);
  return BL_SUCCESS;
}

// bl::BitSet - SSO Dense - Chop
// =============================

static SSODenseInfo chopSSODenseData(const BLBitSetCore* self, uint32_t dst[kSSOWordCount], uint32_t startBit, uint32_t endBit) noexcept {
  SSODenseInfo info = getSSODenseInfo(self);

  uint32_t firstBit = blMax(startBit, info.startBit());
  uint32_t lastBit = blMin(endBit - 1, info.lastBit());

  if (firstBit > lastBit) {
    info._wordCount = 0;
    return info;
  }

  MemOps::fillSmallT(dst, uint32_t(0u), kSSOWordCount);
  BitSetOps::bitArrayFill(dst, firstBit - info.startBit(), lastBit - firstBit + 1);
  MemOps::combineSmall<BitOperator::And>(dst, self->_d.u32_data, kSSOWordCount);

  return info;
}

// bl::BitSet - Dynamic - Capacity
// ===============================

static BL_INLINE_CONSTEXPR uint32_t capacityFromImplSize(BLObjectImplSize implSize) noexcept {
  return uint32_t((implSize.value() - sizeof(BLBitSetImpl)) / sizeof(BLBitSetSegment));
}

static BL_INLINE_CONSTEXPR BLObjectImplSize implSizeFromCapacity(uint32_t capacity) noexcept {
  return BLObjectImplSize(sizeof(BLBitSetImpl) + capacity * sizeof(BLBitSetSegment));
}

static BL_INLINE_NODEBUG BLObjectImplSize alignImplSizeToMinimum(BLObjectImplSize implSize) noexcept {
  return BLObjectImplSize(blMax<size_t>(implSize.value(), kInitialImplSize));
}

static BL_INLINE_NODEBUG BLObjectImplSize expandImplSize(BLObjectImplSize implSize) noexcept {
  return alignImplSizeToMinimum(blObjectExpandImplSize(implSize));
}

// bl::BitSet - Dynamic - Init
// ===========================

static BL_INLINE BLResult initDynamic(BLBitSetCore* self, BLObjectImplSize implSize) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_BIT_SET);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLBitSetImpl>(self, info, implSize));

  BLBitSetImpl* impl = getImpl(self);
  impl->segmentCapacity = capacityFromImplSize(implSize);
  impl->segmentCount = 0;
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult initDynamicWithData(BLBitSetCore* self, BLObjectImplSize implSize, const BLBitSetSegment* segmentData, uint32_t segmentCount) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_BIT_SET);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLBitSetImpl>(self, info, implSize));

  BLBitSetImpl* impl = getImpl(self);
  impl->segmentCapacity = capacityFromImplSize(implSize);
  impl->segmentCount = segmentCount;
  memcpy(impl->segmentData(), segmentData, segmentCount * sizeof(BLBitSetSegment));
  return BL_SUCCESS;
}

// bl::BitSet - Dynamic - Cached Cardinality
// =========================================

//! Returns cached cardinality.
//!
//! If the returned value is zero it means that the cardinality is either not cached or zero. This means that zero
//! is always an unreliable value, which cannot be trusted. The implementation in general resets cardinality to zero
//! every time the BitSet is modified.
static BL_INLINE uint32_t getCachedCardinality(const BLBitSetCore* self) noexcept { return self->_d.u32_data[2]; }

//! Resets cached cardinality to zero, which signalizes that it's not valid.
static BL_INLINE BLResult resetCachedCardinality(BLBitSetCore* self) noexcept {
  self->_d.u32_data[2] = 0;
  return BL_SUCCESS;
}

//! Updates cached cardinality to `cardinality` after the cardinality has been calculated.
static BL_INLINE void updateCachedCardinality(const BLBitSetCore* self, uint32_t cardinality) noexcept {
  const_cast<BLBitSetCore*>(self)->_d.u32_data[2] = cardinality;
}

// bl::BitSet - Dynamic - Segment Utilities
// ========================================

struct SegmentWordIndex {
  uint32_t index;
};

// Helper for bl::lowerBound() and bl::upperBound().
static BL_INLINE bool operator<(const BLBitSetSegment& a, const SegmentWordIndex& b) noexcept {
  return a.endWord() <= b.index;
}

static BL_INLINE bool hasSegmentWordIndex(const BLBitSetSegment& segment, uint32_t wordIndex) noexcept {
  return Range{segment.startWord(), segment.endWord()}.hasIndex(wordIndex);
}

static BL_INLINE bool hasSegmentBitIndex(const BLBitSetSegment& segment, uint32_t bitIndex) noexcept {
  return Range{segment.startWord(), segment.endWord()}.hasIndex(wordIndexOf(bitIndex));
}

static BL_INLINE void initDenseSegment(BLBitSetSegment& segment, uint32_t startWord) noexcept {
  segment._startWord = startWord;
  segment.clearData();
}

static BL_INLINE void initDenseSegmentWithData(BLBitSetSegment& segment, uint32_t startWord, const uint32_t* wordData) noexcept {
  segment._startWord = startWord;
  MemOps::copyForwardInlineT(segment.data(), wordData, kSegmentWordCount);
}

static BL_INLINE void initDenseSegmentWithRange(BLBitSetSegment& segment, uint32_t startBit, uint32_t rangeSize) noexcept {
  uint32_t startWord = wordIndexOf(alignBitDownToSegment(startBit));
  segment._startWord = startWord;
  segment.clearData();

  BitSetOps::bitArrayFill(segment.data(), startBit & kSegmentBitMask, rangeSize);
}

static BL_INLINE void initDenseSegmentWithOnes(BLBitSetSegment& segment, uint32_t startWord) noexcept {
  segment._startWord = startWord;
  segment.fillData();
}

static BL_INLINE void initRangeSegment(BLBitSetSegment& segment, uint32_t startWord, uint32_t endWord) noexcept {
  uint32_t nWords = endWord - startWord;
  uint32_t filler = IntOps::bitMaskFromBool<uint32_t>(nWords < kSegmentWordCount * 2);

  segment._startWord = startWord | (~filler & BL_BIT_SET_RANGE_MASK);
  segment._data[0] = filler | endWord;
  MemOps::fillInlineT(segment._data + 1, filler, kSegmentWordCount - 1);
}

static BL_INLINE bool isSegmentDataZero(const uint32_t* wordData) noexcept {
#if BL_TARGET_ARCH_BITS >= 64
  uint64_t u = MemOps::readU64<BL_BYTE_ORDER_NATIVE, 4>(wordData);
  for (uint32_t i = 1; i < kSegmentWordCount / 2; i++)
    u |= MemOps::readU64<BL_BYTE_ORDER_NATIVE, 4>(reinterpret_cast<const uint64_t*>(wordData) + i);
  return u == 0u;
#else
  uint32_t u = wordData[0];
  for (uint32_t i = 1; i < kSegmentWordCount; i++)
    u |= wordData[i];
  return u == 0u;
#endif
}

static BL_INLINE bool isSegmentDataFilled(const uint32_t* wordData) noexcept {
#if BL_TARGET_ARCH_BITS >= 64
  uint64_t u = MemOps::readU64<BL_BYTE_ORDER_NATIVE, 4>(wordData);
  for (uint32_t i = 1; i < kSegmentWordCount / 2; i++)
    u &= MemOps::readU64<BL_BYTE_ORDER_NATIVE, 4>(reinterpret_cast<const uint64_t*>(wordData) + i);
  return ~u == 0u;
#else
  uint32_t u = wordData[0];
  for (uint32_t i = 1; i < kSegmentWordCount; i++)
    u &= wordData[i];
  return ~u == 0u;
#endif
}

// NOTE: These functions take an advantage of knowing that segments are fixed bit arrays. We are only interested
// in low part of `bitIndex` as we know that each segment's bit-start is aligned to `kSegmentBitCount`.

static BL_INLINE void addSegmentBit(BLBitSetSegment& segment, uint32_t bitIndex) noexcept {
  BL_ASSERT(hasSegmentBitIndex(segment, bitIndex));
  BitSetOps::bitArraySetBit(segment.data(), bitIndex & kSegmentBitMask);
}

static BL_INLINE void addSegmentRange(BLBitSetSegment& segment, uint32_t startBit, uint32_t count) noexcept {
  BL_ASSERT(count > 0);
  BL_ASSERT(hasSegmentBitIndex(segment, startBit));
  BL_ASSERT(hasSegmentBitIndex(segment, startBit + count - 1));
  BitSetOps::bitArrayFill(segment.data(), startBit & kSegmentBitMask, count);
}

static BL_INLINE void clearSegmentBit(BLBitSetSegment& segment, uint32_t bitIndex) noexcept {
  BL_ASSERT(hasSegmentBitIndex(segment, bitIndex));
  BitSetOps::bitArrayClearBit(segment.data(), bitIndex & kSegmentBitMask);
}

static BL_INLINE bool testSegmentBit(const BLBitSetSegment& segment, uint32_t bitIndex) noexcept {
  BL_ASSERT(hasSegmentBitIndex(segment, bitIndex));
  return BitSetOps::bitArrayTestBit(segment.data(), bitIndex & kSegmentBitMask);
}

// bl::BitSet - Dynamic - SegmentIterator
// ======================================

class SegmentIterator {
public:
  BLBitSetSegment* segmentPtr;
  BLBitSetSegment* segmentEnd;

  uint32_t curWord;
  uint32_t endWord;

  BL_INLINE SegmentIterator(const SegmentIterator& other) = default;

  BL_INLINE SegmentIterator(BLBitSetSegment* segmentData, uint32_t segmentCount) noexcept {
    reset(segmentData, segmentCount);
  }

  BL_INLINE void reset(BLBitSetSegment* segmentData, uint32_t segmentCount) noexcept {
    segmentPtr = segmentData;
    segmentEnd = segmentData + segmentCount;

    curWord = segmentPtr != segmentEnd ? segmentPtr->startWord() : kInvalidIndex;
    endWord = segmentPtr != segmentEnd ? segmentPtr->endWord() : kInvalidIndex;
  }

  BL_INLINE bool valid() const noexcept {
    return segmentPtr != segmentEnd;
  }

  BL_INLINE uint32_t* wordData() const noexcept {
    BL_ASSERT(valid());

    return segmentPtr->_data;
  }

  BL_INLINE uint32_t wordAt(size_t index) const noexcept {
    BL_ASSERT(valid());

    return segmentPtr->_data[index];
  }

  BL_INLINE uint32_t startWord() const noexcept {
    BL_ASSERT(valid());

    return segmentPtr->startWord();
  }

  BL_INLINE uint32_t end() const noexcept {
    BL_ASSERT(valid());

    return segmentPtr->endWord();
  }

  BL_INLINE bool allOnes() const noexcept {
    BL_ASSERT(valid());

    return segmentPtr->allOnes();
  }

  BL_INLINE void advanceTo(uint32_t indexWord) noexcept {
    BL_ASSERT(valid());

    curWord = indexWord;
    if (curWord == endWord)
      advanceSegment();
  }

  BL_INLINE void advanceSegment() noexcept {
    BL_ASSERT(valid());

    segmentPtr++;
    curWord = segmentPtr != segmentEnd ? segmentPtr->startWord() : kInvalidIndex;
    endWord = segmentPtr != segmentEnd ? segmentPtr->endWord() : kInvalidIndex;
  }
};

// bl::BitSet - Dynamic - Chop Segments
// ====================================

struct ChoppedSegments {
  // Indexes of start and end segments in the middle.
  uint32_t _middleIndex[2];
  // Could of leading [0] and trailing[1] segments.
  uint32_t _extraCount[2];

  // 4 segments should be enough, but... let's have 2 more in case we have overlooked something.
  BLBitSetSegment _extraData[6];

  BL_INLINE void reset() noexcept {
    _middleIndex[0] = 0u;
    _middleIndex[1] = 0u;
    _extraCount[0] = 0u;
    _extraCount[1] = 0u;
  }

  BL_INLINE bool empty() const noexcept { return finalCount() == 0; }
  BL_INLINE bool hasMiddleSegments() const noexcept { return _middleIndex[1] > _middleIndex[0]; }

  BL_INLINE uint32_t middleIndex() const noexcept { return _middleIndex[0]; }
  BL_INLINE uint32_t middleCount() const noexcept { return _middleIndex[1] - _middleIndex[0]; }

  BL_INLINE uint32_t leadingCount() const noexcept { return _extraCount[0]; }
  BL_INLINE uint32_t trailingCount() const noexcept { return _extraCount[1]; }

  BL_INLINE uint32_t finalCount() const noexcept { return middleCount() + leadingCount() + trailingCount(); }

  BL_INLINE const BLBitSetSegment* extraData() const noexcept { return _extraData; }
  BL_INLINE const BLBitSetSegment* leadingData() const noexcept { return _extraData; }
  BL_INLINE const BLBitSetSegment* trailingData() const noexcept { return _extraData + _extraCount[0]; }
};

static void chopSegments(const BLBitSetSegment* segmentData, uint32_t segmentCount, uint32_t startBit, uint32_t endBit, ChoppedSegments* out) noexcept {
  uint32_t bitIndex = startBit;
  uint32_t lastBit = endBit - 1;
  uint32_t alignedEndWord = wordIndexOf(alignBitDownToSegment(endBit));

  uint32_t middleIndex = 0;
  uint32_t extraIndex = 0;
  uint32_t prevExtraIndex = 0;

  // Initially we want to find segment for the initial bit and in the second iteration for the end bit.
  uint32_t findBitIndex = bitIndex;

  out->reset();

  for (uint32_t i = 0; i < 2; i++) {
    middleIndex += uint32_t(bl::lowerBound(segmentData + middleIndex, segmentCount - middleIndex, SegmentWordIndex{wordIndexOf(findBitIndex)}));
    if (middleIndex >= segmentCount) {
      out->_middleIndex[i] = middleIndex;
      break;
    }

    // Either an overlapping segment or a segment immediately after bitIndex.
    const BLBitSetSegment& segment = segmentData[middleIndex];

    // Normalize bitIndex to start at the segment boundary if it was lower - this skips uninteresting area of the BitSet.
    bitIndex = blMax(bitIndex, segment.startBit());

    // If the segment overlaps, process it.
    if (bitIndex < endBit && hasSegmentBitIndex(segment, bitIndex)) {
      // Skip this segment if this is a leading index. Trailing segment doesn't need this as it's always used as end.
      middleIndex += (1 - i);

      // The worst case is splitting up a range segment into 3 segments (leading, middle, and trailing).
      if (segment.allOnes()) {
        // Not a loop, just to be able to skip outside.
        do {
          // Leading segment.
          if (!isBitAlignedToSegment(bitIndex)) {
            BLBitSetSegment& leadingSegment = out->_extraData[extraIndex++];

            uint32_t rangeSize = blMin<uint32_t>(endBit - bitIndex, kSegmentBitCount - (bitIndex & kSegmentBitMask));
            initDenseSegmentWithRange(leadingSegment, bitIndex, rangeSize);

            bitIndex += rangeSize;
            if (bitIndex >= endBit)
              break;
          }

          // Middle segment - at this point it's guaranteed that `bitIndex` is aligned to a segment boundary.
          uint32_t middleWordCount = blMin(alignedEndWord, segment._rangeEndWord()) - wordIndexOf(bitIndex);
          if (middleWordCount >= kSegmentWordCount) {
            BLBitSetSegment& middleSegment = out->_extraData[extraIndex++];
            uint32_t wordIndex = wordIndexOf(bitIndex);

            if (middleWordCount >= kSegmentWordCount * 2u)
              initRangeSegment(middleSegment, wordIndex, wordIndex + middleWordCount);
            else
              initDenseSegmentWithOnes(middleSegment, wordIndex);

            bitIndex += middleWordCount * BitSetOps::kNumBits;
            if (bitIndex >= endBit)
              break;
          }

          // Trailing segment - bitIndex is aligned to a segment boundary - endIndex is not.
          if (bitIndex <= segment.lastBit()) {
            BLBitSetSegment& trailingSegment = out->_extraData[extraIndex++];

            uint32_t rangeSize = blMin<uint32_t>(lastBit, segment.lastBit()) - bitIndex + 1;
            initDenseSegmentWithRange(trailingSegment, bitIndex, rangeSize);
            bitIndex += rangeSize;
          }
        } while (false);
      }
      else {
        // Dense segment - easy case, just create a small dense segment with range, and combine it with this segment.
        uint32_t rangeSize = blMin<uint32_t>(endBit - bitIndex, kSegmentBitCount - (bitIndex & kSegmentBitMask));

        BLBitSetSegment& extraSegment = out->_extraData[extraIndex++];
        initDenseSegmentWithRange(extraSegment, bitIndex, rangeSize);

        BitSetOps::bitArrayCombineWords<BitOperator::And>(extraSegment.data(), segment.data(), kSegmentWordCount);
        bitIndex += rangeSize;
      }
    }

    out->_middleIndex[i] = middleIndex;
    out->_extraCount[i] = extraIndex - prevExtraIndex;

    findBitIndex = endBit;
    prevExtraIndex = extraIndex;

    if (bitIndex >= endBit)
      break;
  }

  // Normalize middle indexes to make it easier to count the number of middle segments.
  if (out->_middleIndex[1] < out->_middleIndex[0])
    out->_middleIndex[1] = out->_middleIndex[0];
}

// bl::BitSet - Dynamic - Test Operations
// ======================================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

template<typename Result>
struct BaseTestOp {
  typedef Result ResultType;

  static constexpr bool kSkipA0 = false;
  static constexpr bool kSkipA1 = false;
  static constexpr bool kSkipB0 = false;
  static constexpr bool kSkipB1 = false;
};

struct EqualsTestOp : public BaseTestOp<bool> {
  BL_INLINE bool makeResult() const noexcept { return true; }

  template<typename T>
  BL_INLINE bool makeResult(const T& a, const T& b) const noexcept { return false; }

  template<typename T>
  BL_INLINE bool shouldTerminate(const T& a, const T& b) const noexcept { return a != b; }
};

struct CompareTestOp : public BaseTestOp<int> {
  BL_INLINE int makeResult() const noexcept { return 0; }

  template<typename T>
  BL_INLINE int makeResult(const T& a, const T& b) const noexcept { return BitSetOps::compare(a, b); }

  template<typename T>
  BL_INLINE bool shouldTerminate(const T& a, const T& b) const noexcept { return a != b; }
};

struct SubsumesTestOp : public BaseTestOp<bool> {
  static constexpr bool kSkipA1 = true;
  static constexpr bool kSkipB0 = true;

  BL_INLINE bool makeResult() const noexcept { return true; }

  template<typename T>
  BL_INLINE bool makeResult(const T& a, const T& b) const noexcept { return false; }

  template<typename T>
  BL_INLINE bool shouldTerminate(const T& a, const T& b) const noexcept { return (a & b) != b; }
};

struct IntersectsTestOp : public BaseTestOp<bool> {
  static constexpr bool kSkipA0 = true;
  static constexpr bool kSkipB0 = true;

  BL_INLINE bool makeResult() const noexcept { return false; }

  template<typename T>
  BL_INLINE bool makeResult(const T& a, const T& b) const noexcept { return true; }

  template<typename T>
  BL_INLINE bool shouldTerminate(const T& a, const T& b) const noexcept { return (a & b) != 0; }
};

BL_DIAGNOSTIC_POP

template<typename Op>
static BL_INLINE typename Op::ResultType testOp(BLBitSetSegment* aSegmentData, uint32_t aSegmentCount, BLBitSetSegment* bSegmentData, uint32_t bSegmentCount, const Op& op) noexcept {
  constexpr uint32_t k0 = 0;
  constexpr uint32_t k1 = IntOps::allOnes<uint32_t>();

  SegmentIterator aIter(aSegmentData, aSegmentCount);
  SegmentIterator bIter(bSegmentData, bSegmentCount);

  for (;;) {
    if (aIter.curWord == bIter.curWord) {
      // End of bit-data.
      if (aIter.curWord == kInvalidIndex)
        return op.makeResult();

      uint32_t abEndWord = blMin(aIter.endWord, bIter.endWord);
      if (aIter.allOnes()) {
        if (bIter.allOnes()) {
          // 'A' is all ones and 'B' is all ones.
          if (!Op::kSkipA1 && !Op::kSkipB1) {
            if (op.shouldTerminate(k1, k1))
              return op.makeResult(k1, k1);
          }

          bIter.advanceTo(abEndWord);
        }
        else {
          // 'A' is all ones and 'B' has bit-data.
          if (!Op::kSkipA1) {
            for (uint32_t i = 0; i < kSegmentWordCount; i++)
              if (op.shouldTerminate(k1, bIter.wordAt(i)))
                return op.makeResult(k1, bIter.wordAt(i));
          }

          bIter.advanceSegment();
        }

        aIter.advanceTo(abEndWord);
      }
      else {
        if (bIter.allOnes()) {
          // 'A' has bit-data and 'B' is all ones.
          if (!Op::kSkipB1) {
            for (uint32_t i = 0; i < kSegmentWordCount; i++)
              if (op.shouldTerminate(aIter.wordAt(i), k1))
                return op.makeResult(aIter.wordAt(i), k1);
          }

          bIter.advanceTo(abEndWord);
        }
        else {
          // Both 'A' and 'B' have bit-data.
          for (uint32_t i = 0; i < kSegmentWordCount; i++)
            if (op.shouldTerminate(aIter.wordAt(i), bIter.wordAt(i)))
              return op.makeResult(aIter.wordAt(i), bIter.wordAt(i));

          bIter.advanceSegment();
        }

        aIter.advanceSegment();
      }
    }
    else if (aIter.curWord < bIter.curWord) {
      // 'A' is not at the end and 'B' is all zeros until `abEndWord`.
      BL_ASSERT(aIter.valid());
      uint32_t abEndWord = blMin(aIter.end(), bIter.curWord);

      if (!Op::kSkipB0) {
        // uint32_t aDataIndex = aIter.dataIndex();
        if (aIter.allOnes()) {
          // 'A' is all ones and 'B' is all zeros.
          if (op.shouldTerminate(k1, k0))
            return op.makeResult(k1, k0);
        }
        else {
          // 'A' has bit-data and 'B' is all zeros.
          for (uint32_t i = 0; i < kSegmentWordCount; i++)
            if (op.shouldTerminate(aIter.wordAt(i), k0))
              return op.makeResult(aIter.wordAt(i), k0);
        }
      }

      aIter.advanceTo(abEndWord);
    }
    else {
      // 'A' is all zeros until `abEndWord` and 'B' is not at the end.
      BL_ASSERT(bIter.valid());
      uint32_t abEndWord = blMin(bIter.end(), aIter.curWord);

      if (!Op::kSkipA0) {
        if (bIter.allOnes()) {
          if (op.shouldTerminate(k0, k1))
            return op.makeResult(k0, k1);
        }
        else {
          for (uint32_t i = 0; i < kSegmentWordCount; i++)
            if (op.shouldTerminate(k0, bIter.wordAt(i)))
              return op.makeResult(k0, bIter.wordAt(i));
        }
      }

      bIter.advanceTo(abEndWord);
    }
  }
};

// bl::BitSet - Dynamic - Segments From Range
// ==========================================

static BL_INLINE uint32_t segmentCountFromRange(uint32_t startBit, uint32_t endBit) noexcept {
  uint32_t lastBit = endBit - 1;

  uint32_t startSegmentId = startBit / kSegmentBitCount;
  uint32_t lastSegmentId = lastBit / kSegmentBitCount;

  uint32_t maxSegments = blMin<uint32_t>(lastSegmentId - startSegmentId + 1, 3);
  uint32_t collapsed = uint32_t(isBitAlignedToSegment(startBit)) +
                       uint32_t(isBitAlignedToSegment(endBit  )) ;

  if (collapsed >= maxSegments)
    collapsed = maxSegments - 1;

  return maxSegments - collapsed;
}

static BL_NOINLINE uint32_t initSegmentsFromRange(BLBitSetSegment* dst, uint32_t startBit, uint32_t endBit) noexcept {
  uint32_t n = 0;
  uint32_t remain = endBit - startBit;

  if (!isBitAlignedToSegment(startBit) || (startBit & ~kSegmentBitMask) == ((endBit - 1) & ~kSegmentBitMask)) {
    uint32_t segmentBitIndex = startBit & kSegmentBitMask;
    uint32_t size = blMin<uint32_t>(remain, kSegmentBitCount - segmentBitIndex);

    initDenseSegmentWithRange(dst[n++], startBit, size);
    remain -= size;
    startBit += size;

    if (remain == 0)
      return n;
  }

  if (remain >= kSegmentBitCount) {
    uint32_t size = remain & ~kSegmentBitMask;
    initRangeSegment(dst[n], wordIndexOf(startBit), wordIndexOf(startBit + size));

    n++;
    remain &= kSegmentBitMask;
    startBit += size;
  }

  if (remain)
    initDenseSegmentWithRange(dst[n++], startBit, remain);

  return n;
}

static BL_NOINLINE uint32_t initSegmentsFromDenseData(BLBitSetSegment* dst, uint32_t startWord, const uint32_t* words, uint32_t count) noexcept {
  uint32_t firstSegmentId = startWord / kSegmentWordCount;
  uint32_t lastSegmentId = (startWord + count - 1) / kSegmentWordCount;
  uint32_t wordIndex = startWord;

  for (uint32_t segmentId = firstSegmentId; segmentId <= lastSegmentId; segmentId++) {
    uint32_t segmentStartWord = segmentId * kSegmentWordCount;
    uint32_t i = wordIndex % kSegmentWordCount;
    uint32_t n = blMin<uint32_t>(kSegmentWordCount - i, count);

    initDenseSegment(*dst, segmentStartWord);
    count -= n;
    wordIndex += n;

    n += i;
    do {
      dst->_data[i] = *words++;
    } while(++i != n);
  }

  return lastSegmentId - firstSegmentId + 1u;
}

static BL_INLINE uint32_t makeSegmentsFromSSOBitSet(BLBitSetSegment* dst, const BLBitSetCore* self) noexcept {
  BL_ASSERT(self->_d.sso());

  if (self->_d.isBitSetRange()) {
    Range range = getSSORange(self);
    return initSegmentsFromRange(dst, range.start, range.end);
  }
  else {
    SSODenseInfo info = getSSODenseInfo(self);
    return initSegmentsFromDenseData(dst, info.startWord(), self->_d.u32_data, info.wordCount());
  }
}

// bl::BitSet - Dynamic - WordData to Segments
// ===========================================

struct WordDataAnalysis {
  uint32_t segmentCount;
  uint32_t zeroSegmentCount;
};

// Returns the exact number of segments that is necessary to represent the given data. The returned number is the
// optimal case (with zero segments removed and consecutive full segments joined into a range segment).
static WordDataAnalysis analyzeWordDataForAssignment(uint32_t startWord, const uint32_t* wordData, uint32_t wordCount) noexcept {
  // Should only be called when there are actually words to assign.
  BL_ASSERT(wordCount > 0);

  // It's required to remove empty words before running the analysis.
  BL_ASSERT(wordData[0] != 0);
  BL_ASSERT(wordData[wordCount -1] != 0);

  uint32_t zeroCount = 0;
  uint32_t insertCount = 0;

  // If a leading word doesn't start on a segment boundary, then count it as an entire segment.
  uint32_t leadingAlignmentOffset = startWord - alignWordDownToSegment(startWord);
  if (leadingAlignmentOffset) {
    insertCount++;

    uint32_t leadingAlignmentWordsUsed = kSegmentWordCount - leadingAlignmentOffset;
    if (leadingAlignmentWordsUsed >= wordCount)
      return { insertCount, zeroCount };

    wordData += leadingAlignmentWordsUsed;
    wordCount -= leadingAlignmentWordsUsed;
  }

  // If a trailing segment doesn't end on a segment boundary, count it as an entire segment too.
  if (wordCount & (kSegmentWordCount - 1u)) {
    insertCount++;
    wordCount &= ~(kSegmentWordCount - 1u);
  }

  // Process words that form whole segments.
  if (wordCount) {
    const uint32_t* end = wordData + wordCount;

    do {
      QuickDataAnalysis qa = quickDataAnalysis(wordData);
      wordData += kSegmentWordCount;

      if (qa.isZero()) {
        zeroCount++;
        continue;
      }

      insertCount++;

      if (qa.isFull())
        while (wordData != end && isSegmentDataFilled(wordData))
          wordData += kSegmentWordCount;
    } while (wordData != end);
  }

  return WordDataAnalysis { insertCount, zeroCount };
}

// Returns the exact number of segments that is necessary to insert the given wordData into an existing
// BitSet. The real addition can produce less segments in certain scenarios, but never more segments...
//
// NOTE: The given segmentData must be adjusted to startWord - the caller must find which segment will
// be the first overlapping segment (or the next overlapping segment) by using `bl::lowerBound()`.
static WordDataAnalysis analyzeWordDataForCombining(uint32_t startWord, const uint32_t* wordData, uint32_t wordCount, const BLBitSetSegment* segmentData, uint32_t segmentCount) noexcept {
  // Should only be called when there are actually words to assign.
  BL_ASSERT(wordCount > 0);

  // It's required to remove empty words before running the analysis.
  BL_ASSERT(wordData[0] != 0);
  BL_ASSERT(wordData[wordCount -1] != 0);

  uint32_t wordIndex = startWord;
  uint32_t zeroCount = 0;
  uint32_t insertCount = 0;

  // Let's only use `segmentData` and `segmentEnd` to avoid indexing into `segmentData`.
  const BLBitSetSegment* segmentEnd = segmentData + segmentCount;

  // Process data that form a leading segment (only required if the data doesn't start on a segment boundary).
  uint32_t leadingAlignmentOffset = wordIndex - alignWordDownToSegment(wordIndex);
  if (leadingAlignmentOffset) {
    insertCount += uint32_t(!(segmentData != segmentEnd && hasSegmentWordIndex(*segmentData, wordIndex)));

    uint32_t leadingAlignmentWordsUsed = kSegmentWordCount - leadingAlignmentOffset;
    if (leadingAlignmentWordsUsed >= wordCount)
      return { insertCount, zeroCount };

    wordData += leadingAlignmentWordsUsed;
    wordIndex += leadingAlignmentWordsUsed;
    wordCount -= leadingAlignmentWordsUsed;

    if (segmentData != segmentEnd && segmentData->endWord() == wordIndex)
      segmentData++;
  }

  uint32_t trailingWordCount = wordCount & (kSegmentWordCount - 1);
  const uint32_t* wordEnd = wordData + (wordCount - trailingWordCount);

  // Process words that form whole segments.
  while (wordData != wordEnd) {
    if (segmentData != segmentEnd && hasSegmentWordIndex(*segmentData, wordIndex)) {
      wordData += kSegmentWordCount;
      wordIndex += kSegmentWordCount;

      if (segmentData->endWord() == wordIndex)
        segmentData++;
    }
    else {
      QuickDataAnalysis qa = quickDataAnalysis(wordData);

      wordData += kSegmentWordCount;
      wordIndex += kSegmentWordCount;

      if (qa.isZero()) {
        zeroCount++;
        continue;
      }

      insertCount++;

      if (qa.isFull()) {
        uint32_t wordCheck = 0xFFFFFFFFu;
        if (segmentData != segmentEnd)
          wordCheck = segmentData->startWord();

        while (wordIndex < wordCheck && wordData != wordEnd && isSegmentDataFilled(wordData)) {
          wordData += kSegmentWordCount;
          wordIndex += kSegmentWordCount;
        }
      }
    }
  }

  // Process data that form a trailing segment (only required if the data doesn't end on a segment boundary).
  if (trailingWordCount) {
    insertCount += uint32_t(!(segmentData != segmentEnd && hasSegmentWordIndex(*segmentData, wordIndex)));
  }

  return WordDataAnalysis { insertCount, zeroCount };
}

static bool getRangeFromAnalyzedWordData(uint32_t startWord, const uint32_t* wordData, uint32_t wordCount, Range* rangeOut) noexcept {
  // Should only be called when there are actually words to assign.
  BL_ASSERT(wordCount > 0);

  // It's required to remove empty words before running the analysis.
  BL_ASSERT(wordData[0] != 0);
  BL_ASSERT(wordData[wordCount -1] != 0);

  uint32_t firstWordBits = wordData[0];
  uint32_t lastWordBits = wordData[wordCount - 1];

  uint32_t startZeros = BitSetOps::countZerosFromStart(firstWordBits);
  uint32_t endZeros = BitSetOps::countZerosFromEnd(lastWordBits);

  rangeOut->start = bitIndexOf(startWord + 0) + startZeros;
  rangeOut->end = bitIndexOf(startWord + wordCount - 1) + BitSetOps::kNumBits - endZeros;

  // Single word case.
  if (wordCount == 1) {
    uint32_t mask = BitSetOps::shiftToEnd(BitSetOps::nonZeroStartMask(BitSetOps::kNumBits - (startZeros + endZeros)), startZeros);
    return wordData[0] == mask;
  }

  // Multiple word cases - first check whether the first and last words describe a consecutive mask.
  if (firstWordBits != BitSetOps::nonZeroEndMask(BitSetOps::kNumBits - startZeros) ||
      lastWordBits != BitSetOps::nonZeroStartMask(BitSetOps::kNumBits - endZeros)) {
    return false;
  }

  // Now verify that all other words that form first, middle, and last segment are all ones.
  //
  // NOTE: This function is only called after `analyzeWordDataForAssignment()`, which means that we know that there
  // are no zero segments and we know that the maximum number of segments all words form are 3. This means that we
  // don't have to process all words, only those that describe the first two segments and the last one (because there
  // are no other segments). If the range is really large, we can skip a lot of words.
  uint32_t firstWordsToCheck = blMin<uint32_t>(wordCount - 2, kSegmentWordCount * 2 - 1);
  uint32_t lastWordsToCheck = blMin<uint32_t>(wordCount - 2, kSegmentWordCount - 1);

  return MemOps::testSmallT(wordData + 1, firstWordsToCheck, BitSetOps::ones()) &&
         MemOps::testSmallT(wordData + wordCount - 1 - lastWordsToCheck, lastWordsToCheck, BitSetOps::ones());
}

// bl::BitSet - Dynamic - Splice Operation
// =======================================

// Replaces a segment at the given `index` by segments defined by `insertData` and `insertCount` (internal).
static BLResult spliceInternal(BLBitSetCore* self, BLBitSetSegment* segmentData, uint32_t segmentCount, uint32_t index, uint32_t deleteCount, const BLBitSetSegment* insertData, uint32_t insertCount, bool canModify) noexcept {
  uint32_t finalSegmentCount = segmentCount + insertCount - deleteCount;
  uint32_t additionalSegmentCount = insertCount - deleteCount;

  if (canModify) {
    BLBitSetImpl* selfI = getImpl(self);
    if (selfI->segmentCapacity >= finalSegmentCount) {
      selfI->segmentCount = finalSegmentCount;

      if (deleteCount != insertCount)
        memmove(segmentData + index + insertCount, segmentData + index + deleteCount, (segmentCount - index - deleteCount) * sizeof(BLBitSetSegment));

      MemOps::copyForwardInlineT(segmentData + index, insertData, insertCount);
      return resetCachedCardinality(self);
    }
  }

  BLBitSetCore tmp = *self;
  BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(segmentCount + additionalSegmentCount));
  BL_PROPAGATE(initDynamic(self, implSize));

  BLBitSetImpl* selfI = getImpl(self);
  selfI->segmentCount = segmentCount + additionalSegmentCount;

  MemOps::copyForwardInlineT(selfI->segmentData(), segmentData, index);
  MemOps::copyForwardInlineT(selfI->segmentData() + index, insertData, insertCount);
  MemOps::copyForwardInlineT(selfI->segmentData() + index + insertCount, segmentData + index + deleteCount, segmentCount - index - deleteCount);

  return releaseInstance(&tmp);
}

} // {BitSetInternal}
} // {bl}

// bl::BitSet - API - Init & Destroy
// =================================

BL_API_IMPL BLResult blBitSetInit(BLBitSetCore* self) noexcept {
  using namespace bl::BitSetInternal;

  return initSSOEmpty(self);
}

BL_API_IMPL BLResult blBitSetInitMove(BLBitSetCore* self, BLBitSetCore* other) noexcept {
  using namespace bl::BitSetInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isBitSet());

  self->_d = other->_d;
  return initSSOEmpty(other);
}

BL_API_IMPL BLResult blBitSetInitWeak(BLBitSetCore* self, const BLBitSetCore* other) noexcept {
  using namespace bl::BitSetInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isBitSet());

  self->_d = other->_d;
  return retainInstance(self);
}

BL_API_IMPL BLResult blBitSetInitRange(BLBitSetCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  using namespace bl::BitSetInternal;

  uint32_t mask = uint32_t(-int32_t(startBit < endBit));
  initSSORange(self, startBit & mask, endBit & mask);
  return mask ? BL_SUCCESS : blTraceError(BL_ERROR_INVALID_VALUE);
}

BL_API_IMPL BLResult blBitSetDestroy(BLBitSetCore* self) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  return releaseInstance(self);
}

// bl::BitSet - API - Reset
// ========================

BL_API_IMPL BLResult blBitSetReset(BLBitSetCore* self) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  releaseInstance(self);
  return initSSOEmpty(self);
}

// bl::BitSet - API - Assign BitSet
// ================================

BL_API_IMPL BLResult blBitSetAssignMove(BLBitSetCore* self, BLBitSetCore* other) noexcept {
  using namespace bl::BitSetInternal;

  BL_ASSERT(self->_d.isBitSet());
  BL_ASSERT(other->_d.isBitSet());

  BLBitSetCore tmp = *other;
  initSSOEmpty(other);
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blBitSetAssignWeak(BLBitSetCore* self, const BLBitSetCore* other) noexcept {
  using namespace bl::BitSetInternal;

  BL_ASSERT(self->_d.isBitSet());
  BL_ASSERT(other->_d.isBitSet());

  retainInstance(other);
  return replaceInstance(self, other);
}

BL_API_IMPL BLResult blBitSetAssignDeep(BLBitSetCore* self, const BLBitSetCore* other) noexcept {
  using namespace bl::BitSetInternal;

  BL_ASSERT(self->_d.isBitSet());
  BL_ASSERT(other->_d.isBitSet());

  if (other->_d.sso())
    return replaceInstance(self, other);

  BLBitSetImpl* otherI = getImpl(other);
  uint32_t segmentCount = otherI->segmentCount;

  if (!segmentCount)
    return blBitSetClear(self);

  BLBitSetImpl* selfI = getImpl(self);
  if (!self->_d.sso() && isImplMutable(selfI)) {
    if (selfI->segmentCapacity >= segmentCount) {
      memcpy(selfI->segmentData(), otherI->segmentData(), segmentCount * sizeof(BLBitSetSegment));
      selfI->segmentCount = segmentCount;
      resetCachedCardinality(self);
      return BL_SUCCESS;
    }
  }

  BLBitSetCore tmp;
  BLObjectImplSize tmpImplSize = implSizeFromCapacity(segmentCount);

  BL_PROPAGATE(initDynamicWithData(&tmp, tmpImplSize, otherI->segmentData(), segmentCount));
  return replaceInstance(self, &tmp);
}

// bl::BitSet - API - Assign Range
// ===============================

BL_API_IMPL BLResult blBitSetAssignRange(BLBitSetCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (BL_UNLIKELY(startBit >= endBit)) {
    if (startBit > endBit)
      return blTraceError(BL_ERROR_INVALID_VALUE);
    else
      return blBitSetClear(self);
  }

  if (!self->_d.sso()) {
    BLBitSetImpl* selfI = getImpl(self);
    if (isImplMutable(selfI)) {
      uint32_t segmentCount = segmentCountFromRange(startBit, endBit);

      if (selfI->segmentCapacity >= segmentCount) {
        selfI->segmentCount = initSegmentsFromRange(selfI->segmentData(), startBit, endBit);
        return resetCachedCardinality(self);
      }
    }

    // If we cannot use dynamic BitSet let's just release it and use SSO Range.
    releaseInstance(self);
  }

  return initSSORange(self, startBit, endBit);
}

// bl::BitSet - API - Assign Words
// ===============================

static BL_INLINE BLResult normalizeWordDataParams(uint32_t& startWord, const uint32_t*& wordData, uint32_t& wordCount) noexcept {
  using namespace bl::BitSetInternal;

  if (BL_UNLIKELY(startWord > kLastWord))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (wordCount >= kLastWord + 1u - startWord) {
    if (wordCount > kLastWord + 1u - startWord)
      return blTraceError(BL_ERROR_INVALID_VALUE);

    // Make sure the last word doesn't have the last bit set. This bit is not indexable, so refuse it.
    if (wordCount > 0 && (wordData[wordCount - 1] & 1u) != 0)
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  // Skip zero words from the beginning and from the end.
  const uint32_t* end = wordData + wordCount;

  while (wordData != end && wordData[0] == 0u) {
    wordData++;
    startWord++;
  }

  while (wordData != end && end[-1] == 0u)
    end--;

  wordCount = (uint32_t)(size_t)(end - wordData);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blBitSetAssignWords(BLBitSetCore* self, uint32_t startWord, const uint32_t* wordData, uint32_t wordCount) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  BL_PROPAGATE(normalizeWordDataParams(startWord, wordData, wordCount));
  if (!wordCount)
    return blBitSetClear(self);

  BLBitSetCore tmp;
  uint32_t wordIndexEnd = startWord + wordCount;
  uint32_t startWordAlignedToSegment = alignWordDownToSegment(startWord);

  bool changedInPlace = false;
  uint32_t mutableSegmentCapacity = 0;
  BLBitSetSegment* dstSegment = nullptr;

  // Avoid analysis if the BitSet is dynamic, mutable, and has enough capacity to hold the whole data in dense segments.
  if (!self->_d.sso()) {
    BLBitSetImpl* selfI = getImpl(self);
    if (isImplMutable(selfI)) {
      mutableSegmentCapacity = selfI->segmentCapacity;

      uint32_t endWordAlignedUpToSegment = alignWordUpToSegment(startWord + wordCount);
      uint32_t worstCaseSegmentsRequirement = (endWordAlignedUpToSegment - startWordAlignedToSegment) / kSegmentWordCount;

      changedInPlace = (mutableSegmentCapacity >= worstCaseSegmentsRequirement);
      dstSegment = selfI->segmentData();
    }
  }

  if (!changedInPlace) {
    WordDataAnalysis analysis = analyzeWordDataForAssignment(startWord, wordData, wordCount);
    changedInPlace = mutableSegmentCapacity >= analysis.segmentCount;

    // A second chance or SSO attempt.
    if (!changedInPlace) {
      // If we cannot use the existing Impl, because it's not mutable, or doesn't have the required capacity, try
      // to use SSO instead of allocating a new Impl. SSO is possible if there is at most `kSSOWordCount` words or
      // if the data represents a range (all bits in `wordData` are consecutive).
      if (wordCount <= kSSOWordCount) {
        uint32_t ssoStartWord = blMin<uint32_t>(startWord, kSSOLastWord);
        uint32_t ssoWordOffset = startWord - ssoStartWord;

        initSSODense(&tmp, ssoStartWord);
        bl::MemOps::copyForwardInlineT(tmp._d.u32_data + ssoWordOffset, wordData, wordCount);
        return replaceInstance(self, &tmp);
      }

      // NOTE: 4 or more segments never describe a range - the maximum is 3 (leading, middle, and trailing segment).
      Range range;
      if (analysis.segmentCount <= 3 && analysis.zeroSegmentCount == 0 && getRangeFromAnalyzedWordData(startWord, wordData, wordCount, &range)) {
        initSSORange(&tmp, range.start, range.end);
        return replaceInstance(self, &tmp);
      }

      // Allocate a new Impl.
      BLObjectImplSize implSize = implSizeFromCapacity(blMax(analysis.segmentCount, capacityFromImplSize(BLObjectImplSize{kInitialImplSize})));
      BL_PROPAGATE(initDynamic(&tmp, implSize));
      dstSegment = getImpl(&tmp)->segmentData();
    }
  }

  {
    uint32_t wordIndex = alignWordDownToSegment(startWord);
    uint32_t endWordAlignedDownToSegment = alignWordDownToSegment(startWord + wordCount);

    // The leading segment requires special handling if it doesn't start on a segment boundary.
    if (wordIndex != startWord) {
      uint32_t segmentWordOffset = startWord - wordIndex;
      uint32_t segmentWordCount = blMin<uint32_t>(wordCount, kSegmentWordCount - segmentWordOffset);

      initDenseSegment(*dstSegment, wordIndex);
      bl::MemOps::copyForwardInlineT(dstSegment->data() + segmentWordOffset, wordData, segmentWordCount);

      dstSegment++;
      wordData += segmentWordCount;
      wordIndex += kSegmentWordCount;
    }

    // Process words that form whole segments.
    while (wordIndex < endWordAlignedDownToSegment) {
      QuickDataAnalysis qa = quickDataAnalysis(wordData);

      // Handle adding of Range segments.
      if (qa.isFull()) {
        const uint32_t* currentWordData = wordData + kSegmentWordCount;
        uint32_t segmentEndIndex = wordIndex + kSegmentWordCount;

        while (segmentEndIndex < endWordAlignedDownToSegment && isSegmentDataFilled(currentWordData)) {
          currentWordData += kSegmentWordCount;
          segmentEndIndex += kSegmentWordCount;
        }

        // Only add a Range segment if the range spans across at least 2 dense segments.
        if (segmentEndIndex - wordIndex > kSegmentWordCount) {
          initRangeSegment(*dstSegment, wordIndex, segmentEndIndex);

          dstSegment++;
          wordData = currentWordData;
          wordIndex = segmentEndIndex;
          continue;
        }
      }

      if (!qa.isZero()) {
        initDenseSegmentWithData(*dstSegment, wordIndex, wordData);
        dstSegment++;
      }

      wordData += kSegmentWordCount;
      wordIndex += kSegmentWordCount;
    }

    // Trailing segment requires special handling, if it doesn't end on a segment boundary.
    if (wordIndex != wordIndexEnd) {
      initDenseSegment(*dstSegment, wordIndex);
      bl::MemOps::copyForwardInlineT(dstSegment->data(), wordData, (size_t)(wordIndexEnd - wordIndex));

      dstSegment++;
    }
  }

  if (changedInPlace) {
    BLBitSetImpl* selfI = getImpl(self);
    selfI->segmentCount = (uint32_t)(size_t)(dstSegment - selfI->segmentData());
    return resetCachedCardinality(self);
  }
  else {
    BLBitSetImpl* tmpI = getImpl(&tmp);
    tmpI->segmentCount = (uint32_t)(size_t)(dstSegment - tmpI->segmentData());
    return replaceInstance(self, &tmp);
  }
}

// bl::BitSet - API - Accessors
// ============================

BL_API_IMPL bool blBitSetIsEmpty(const BLBitSetCore* self) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (self->_d.sso())
    return isSSOEmpty(self);

  uint32_t cardinality = getCachedCardinality(self);
  if (cardinality)
    return false;

  const BLBitSetImpl* selfI = getImpl(self);
  const BLBitSetSegment* segmentData = selfI->segmentData();
  uint32_t segmentCount = selfI->segmentCount;

  for (uint32_t i = 0; i < segmentCount; i++)
    if (segmentData[i].allOnes() || !isSegmentDataZero(segmentData[i].data()))
      return false;

  return true;
}

BL_API_IMPL BLResult blBitSetGetData(const BLBitSetCore* self, BLBitSetData* out) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (self->_d.sso()) {
    out->segmentData = out->ssoSegments;
    out->segmentCount = makeSegmentsFromSSOBitSet(out->ssoSegments, self);
  }
  else {
    const BLBitSetImpl* selfI = getImpl(self);
    out->segmentData = selfI->segmentData();
    out->segmentCount = selfI->segmentCount;
  }

  return BL_SUCCESS;
}

BL_API_IMPL uint32_t blBitSetGetSegmentCount(const BLBitSetCore* self) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (self->_d.sso()) {
    if (self->_d.isBitSetRange()) {
      Range range = getSSORange(self);
      if (range.empty())
        return 0;
      else
        return segmentCountFromRange(range.start, range.end);
    }
    else {
      SSODenseInfo info = getSSODenseInfo(self);
      uint32_t firstSegmentId = info.startWord() / kSegmentWordCount;
      uint32_t lastSegmentId = info.lastWord() / kSegmentWordCount;
      return 1u + uint32_t(firstSegmentId != lastSegmentId);
    }
  }

  return getImpl(self)->segmentCount;
}

BL_API_IMPL uint32_t blBitSetGetSegmentCapacity(const BLBitSetCore* self) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (self->_d.sso())
    return 0;

  return getImpl(self)->segmentCapacity;
}

// bl::BitSet - API - Bit Test Operations
// ======================================

BL_API_IMPL bool blBitSetHasBit(const BLBitSetCore* self, uint32_t bitIndex) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  uint32_t wordIndex = wordIndexOf(bitIndex);

  if (self->_d.sso()) {
    if (self->_d.isBitSetRange())
      return getSSORange(self).hasIndex(bitIndex);

    SSODenseInfo info = getSSODenseInfo(self);
    if (info.hasIndex(bitIndex))
      return bl::BitSetOps::hasBit(self->_d.u32_data[wordIndex - info.startWord()], bitIndex % bl::BitSetOps::kNumBits);
    else
      return false;
  }
  else {
    const BLBitSetImpl* selfI = getImpl(self);
    const BLBitSetSegment* segmentData = selfI->segmentData();

    uint32_t segmentCount = selfI->segmentCount;
    uint32_t segmentIndex = uint32_t(bl::lowerBound(segmentData, segmentCount, SegmentWordIndex{wordIndex}));

    if (segmentIndex >= segmentCount)
      return false;

    const BLBitSetSegment& segment = segmentData[segmentIndex];
    if (!hasSegmentWordIndex(segment, wordIndex))
      return false;

    return segment.allOnes() || testSegmentBit(segment, bitIndex);
  }
}

BL_API_IMPL bool blBitSetHasBitsInRange(const BLBitSetCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (startBit >= endBit)
    return false;

  uint32_t lastBit = endBit - 1;
  BLBitSetSegment ssoSegment;

  uint32_t curWord;
  uint32_t endWord;

  const BLBitSetSegment* segmentPtr;
  const BLBitSetSegment* segmentEnd;

  if (self->_d.sso()) {
    if (self->_d.isBitSetRange())
      return getSSORange(self).intersect(startBit, endBit).valid();

    SSODenseInfo info = getSSODenseInfo(self);
    startBit = blMax(startBit, info.startBit());
    lastBit = blMin(lastBit, info.lastBit());

    if (startBit > lastBit)
      return false;

    endBit = lastBit + 1;

    curWord = wordIndexOf(startBit);
    endWord = wordIndexOf(lastBit) + 1u;

    initDenseSegment(ssoSegment, curWord);
    bl::MemOps::copyForwardInlineT(ssoSegment._data, self->_d.u32_data + (curWord - info.startWord()), info.endWord() - curWord);

    segmentPtr = &ssoSegment;
    segmentEnd = segmentPtr + 1;
  }
  else {
    BLBitSetImpl* selfI = getImpl(self);

    curWord = wordIndexOf(startBit);
    endWord = wordIndexOf(lastBit) + 1u;

    segmentPtr = selfI->segmentData();
    segmentEnd = selfI->segmentData() + selfI->segmentCount;
    segmentPtr += bl::lowerBound(segmentPtr, selfI->segmentCount, SegmentWordIndex{curWord});

    // False if the range doesn't overlap any segment.
    if (segmentPtr == segmentEnd || endWord <= segmentPtr->startWord())
      return false;
  }

  // We handle start of the range separately as we have to construct a mask that would have the start index and
  // possibly also an end index (if the range is small) accounted. This means that the next loop can consider that
  // the range starts at a word boundary and has to handle only the end index, not both start and end indexes.
  if (hasSegmentWordIndex(*segmentPtr, curWord)) {
    if (segmentPtr->allOnes())
      return true;

    uint32_t index = startBit % bl::BitSetOps::kNumBits;
    uint32_t mask = bl::BitSetOps::nonZeroStartMask(blMin<uint32_t>(bl::BitSetOps::kNumBits - index, endBit - startBit), index);

    if (segmentPtr->wordAt(curWord - segmentPtr->_denseStartWord()) & mask)
      return true;

    if (++curWord >= endWord)
      return false;
  }

  // It's guaranteed that if we are here the range is aligned at word boundary and starts always with 0 bit for
  // each word processed here. The loop has to handle the end index though as the range doesn't have to cross
  // each processed word.
  do {
    curWord = blMax(segmentPtr->startWord(), curWord);
    if (curWord >= endWord)
      return false;

    uint32_t n = blMin(segmentPtr->endWord(), endWord) - curWord;
    if (n) {
      if (segmentPtr->allOnes())
        return true;

      do {
        uint32_t bits = segmentPtr->wordAt(curWord - segmentPtr->_denseStartWord());
        curWord++;

        if (bits) {
          uint32_t count = curWord != endWord ? 32 : ((endBit - 1) % bl::BitSetOps::kNumBits) + 1;
          uint32_t mask = bl::BitSetOps::nonZeroStartMask(count);
          return (bits & mask) != 0;
        }
      } while (--n);
    }
  } while (++segmentPtr < segmentEnd);

  return false;
}

// bl::BitSet - API - Subsumes Test
// ================================

BL_API_IMPL bool blBitSetSubsumes(const BLBitSetCore* a, const BLBitSetCore* b) noexcept {
  using namespace bl::BitSetInternal;

  BL_ASSERT(a->_d.isBitSet());
  BL_ASSERT(b->_d.isBitSet());

  BLBitSetSegment aSSOSegmentData[3];
  BLBitSetSegment bSSOSegmentData[3];

  BLBitSetSegment* aSegmentData;
  BLBitSetSegment* bSegmentData;

  uint32_t aSegmentCount;
  uint32_t bSegmentCount;

  if (a->_d.sso()) {
    aSegmentData = aSSOSegmentData;
    aSegmentCount = makeSegmentsFromSSOBitSet(aSegmentData, a);
  }
  else {
    aSegmentData = getImpl(a)->segmentData();
    aSegmentCount = getImpl(a)->segmentCount;
  }

  if (b->_d.sso()) {
    bSegmentData = bSSOSegmentData;
    bSegmentCount = makeSegmentsFromSSOBitSet(bSegmentData, b);
  }
  else {
    bSegmentData = getImpl(b)->segmentData();
    bSegmentCount = getImpl(b)->segmentCount;
  }

  return testOp(aSegmentData, aSegmentCount, bSegmentData, bSegmentCount, SubsumesTestOp{});
}

// bl::BitSet - API - Intersects Test
// ==================================

BL_API_IMPL bool blBitSetIntersects(const BLBitSetCore* a, const BLBitSetCore* b) noexcept {
  using namespace bl::BitSetInternal;

  BL_ASSERT(a->_d.isBitSet());
  BL_ASSERT(b->_d.isBitSet());

  BLBitSetSegment ssoSegmentData[3];
  BLBitSetSegment* aSegmentData;
  BLBitSetSegment* bSegmentData;

  uint32_t aSegmentCount;
  uint32_t bSegmentCount;

  // Make 'a' the SSO BitSet to make the logic simpler as the intersection is commutative.
  if (b->_d.sso())
    BLInternal::swap(a, b);

  // Handle intersection of SSO BitSets.
  if (a->_d.sso()) {
    if (a->_d.isBitSetRange()) {
      Range range = getSSORange(a);
      return blBitSetHasBitsInRange(b, range.start, range.end);
    }

    if (b->_d.sso()) {
      if (b->_d.isBitSetRange()) {
        Range range = getSSORange(b);
        return blBitSetHasBitsInRange(a, range.start, range.end);
      }

      // Both 'a' and 'b' are SSO Dense representations.
      uint32_t aWordIndex = getSSOWordIndex(a);
      uint32_t bWordIndex = getSSOWordIndex(b);

      const uint32_t* aWordData = a->_d.u32_data;
      const uint32_t* bWordData = b->_d.u32_data;

      // Make `aWordIndex <= bWordIndex`.
      if (aWordIndex > bWordIndex) {
        BLInternal::swap(aWordData, bWordData);
        BLInternal::swap(aWordIndex, bWordIndex);
      }

      uint32_t distance = bWordIndex - aWordIndex;
      if (distance >= kSSOWordCount)
        return false;

      aWordData += distance;
      uint32_t n = kSSOWordCount - distance;

      do {
        n--;
        if (aWordData[n] & bWordData[n])
          return true;
      } while (n);

      return false;
    }

    aSegmentData = ssoSegmentData;
    aSegmentCount = initSegmentsFromDenseData(aSegmentData, getSSOWordIndex(a), a->_d.u32_data, kSSOWordCount);
  }
  else {
    aSegmentData = getImpl(a)->segmentData();
    aSegmentCount = getImpl(a)->segmentCount;
  }

  bSegmentData = getImpl(b)->segmentData();
  bSegmentCount = getImpl(b)->segmentCount;

  return testOp(aSegmentData, aSegmentCount, bSegmentData, bSegmentCount, IntersectsTestOp{});
}

// bl::BitSet - API - Range Query
// ==============================

BL_API_IMPL bool blBitSetGetRange(const BLBitSetCore* self, uint32_t* startOut, uint32_t* endOut) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (self->_d.sso()) {
    if (self->_d.isBitSetRange()) {
      Range range = getSSORange(self);
      *startOut = range.start;
      *endOut = range.end;
      return true;
    }
    else {
      SSODenseInfo info = getSSODenseInfo(self);
      PreciseDataAnalysis pa = preciseDataAnalysis(info.startWord(), self->_d.u32_data, info.wordCount());

      *startOut = pa.start;
      *endOut = pa.end;
      return !pa.empty();
    }
  }
  else {
    const BLBitSetImpl* selfI = getImpl(self);

    const BLBitSetSegment* segmentPtr = selfI->segmentData();
    const BLBitSetSegment* segmentEnd = selfI->segmentDataEnd();

    uint32_t firstBit = 0;
    while (segmentPtr != segmentEnd) {
      if (segmentPtr->allOnes()) {
        firstBit = segmentPtr->startBit();
        break;
      }

      if (bl::BitSetOps::bitArrayFirstBit(segmentPtr->data(), kSegmentWordCount, &firstBit)) {
        firstBit += segmentPtr->startBit();
        break;
      }

      segmentPtr++;
    }

    if (segmentPtr == segmentEnd) {
      *startOut = 0;
      *endOut = 0;
      return false;
    }

    uint32_t lastBit = 0;
    while (segmentPtr != segmentEnd) {
      segmentEnd--;

      if (segmentEnd->allOnes()) {
        lastBit = segmentEnd->lastBit();
        break;
      }

      if (bl::BitSetOps::bitArrayLastBit(segmentEnd->data(), kSegmentWordCount, &lastBit)) {
        lastBit += segmentEnd->startBit();
        break;
      }
    }

    *startOut = firstBit;
    *endOut = lastBit + 1;
    return true;
  }
}

// bl::BitSet - API - Cardinality Query
// ====================================

namespace bl {
namespace BitSetInternal {

class SegmentCardinalityAggregator {
public:
  uint32_t _denseCardinalityInBits = 0;
  uint32_t _rangeCardinalityInWords = 0;

  BL_INLINE uint32_t value() const noexcept {
    return _denseCardinalityInBits + _rangeCardinalityInWords * BitSetOps::kNumBits;
  }

  BL_INLINE void aggregate(const BLBitSetSegment& segment) noexcept {
    if (segment.allOnes())
      _rangeCardinalityInWords += segment._rangeEndWord() - segment._rangeStartWord();
    else
      _denseCardinalityInBits += bitCount(segment.data(), kSegmentWordCount);
  }

  BL_INLINE void aggregate(const BLBitSetSegment* segmentData, uint32_t segmentCount) noexcept {
    for (uint32_t i = 0; i < segmentCount; i++)
      aggregate(segmentData[i]);
  }
};

} // {BitSetInternal}
} // {bl}

BL_API_IMPL uint32_t blBitSetGetCardinality(const BLBitSetCore* self) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (self->_d.sso()) {
    if (self->_d.isBitSetRange())
      return getSSORange(self).size();

    return bitCount(self->_d.u32_data, kSSOWordCount);
  }

  uint32_t cardinality = getCachedCardinality(self);
  if (cardinality)
    return cardinality;

  const BLBitSetImpl* selfI = getImpl(self);
  SegmentCardinalityAggregator aggregator;

  aggregator.aggregate(selfI->segmentData(), selfI->segmentCount);
  cardinality = aggregator.value();

  updateCachedCardinality(self, cardinality);
  return cardinality;
}

BL_API_IMPL uint32_t blBitSetGetCardinalityInRange(const BLBitSetCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (startBit >= endBit)
    return 0u;

  // SSO BitSet
  // ----------

  if (self->_d.sso()) {
    if (self->_d.isBitSetRange()) {
      Range range = getSSORange(self).intersect(startBit, endBit);
      return range.empty() ? 0u : range.size();
    }
    else {
      uint32_t tmp[kSSOWordCount];
      SSODenseInfo info = chopSSODenseData(self, tmp, startBit, endBit);

      if (!info.wordCount())
        return 0;

      return bitCount(tmp, info.wordCount());
    }
  }

  // Dynamic BitSet
  // --------------

  const BLBitSetImpl* selfI = getImpl(self);
  const BLBitSetSegment* segmentData = selfI->segmentData();
  uint32_t segmentCount = selfI->segmentCount;

  if (!segmentCount)
    return BL_SUCCESS;

  ChoppedSegments chopped;
  chopSegments(segmentData, segmentCount, startBit, endBit, &chopped);

  if (chopped.empty())
    return 0;

  // Use the default cardinality getter if the BitSet was not chopped at all, because it's cached.
  if (chopped.middleIndex() == 0 && chopped.middleCount() == segmentCount && (chopped.leadingCount() | chopped.trailingCount()) == 0)
    return blBitSetGetCardinality(self);

  SegmentCardinalityAggregator aggregator;
  aggregator.aggregate(selfI->segmentData() + chopped.middleIndex(), chopped.middleCount());
  aggregator.aggregate(chopped.extraData(), chopped.leadingCount() + chopped.trailingCount());
  return aggregator.value();
}

// bl::BitSet - API - Equality & Comparison
// ========================================

BL_API_IMPL bool blBitSetEquals(const BLBitSetCore* a, const BLBitSetCore* b) noexcept {
  using namespace bl::BitSetInternal;

  BL_ASSERT(a->_d.isBitSet());
  BL_ASSERT(b->_d.isBitSet());

  if (a->_d == b->_d)
    return true;

  BLBitSetSegment* aSegmentData;
  BLBitSetSegment* bSegmentData;
  BLBitSetSegment ssoSegmentData[3];

  uint32_t aSegmentCount;
  uint32_t bSegmentCount;

  if (a->_d.sso() == b->_d.sso()) {
    if (a->_d.sso()) {
      // Both 'a' and 'b' are SSO. We know that 'a' and 'b' are not binary equal, which means that if both objects
      // are in the same storage mode (like both are SSO Data or both are SSO Range) they are definitely not equal.
      if (a->_d.isBitSetRange() == b->_d.isBitSetRange())
        return false;

      // One BitSet is SSO Data and the other is SSO Range - let's make 'a' to be the SSO Data one.
      if (a->_d.isBitSetRange())
        BLInternal::swap(a, b);

      SSODenseInfo aInfo = getSSODenseInfo(a);
      PreciseDataAnalysis aPA = preciseDataAnalysis(aInfo.startWord(), a->_d.u32_data, aInfo.wordCount());

      Range bRange = getSSORange(b);
      return aPA.isRange() && aPA.start == bRange.start && aPA.end == bRange.end;
    }

    // Both 'a' and 'b' are dynamic BitSets.
    BLBitSetImpl* aI = getImpl(a);
    BLBitSetImpl* bI = getImpl(b);

    aSegmentData = aI->segmentData();
    aSegmentCount = aI->segmentCount;

    bSegmentData = bI->segmentData();
    bSegmentCount = bI->segmentCount;
  }
  else {
    // One BitSet is SSO, the other isn't - make 'a' the SSO one.
    if (!a->_d.sso())
      BLInternal::swap(a, b);

    aSegmentData = ssoSegmentData;
    aSegmentCount = makeSegmentsFromSSOBitSet(aSegmentData, a);

    BLBitSetImpl* bI = getImpl(b);
    bSegmentData = bI->segmentData();
    bSegmentCount = bI->segmentCount;
  }

  return testOp(aSegmentData, aSegmentCount, bSegmentData, bSegmentCount, EqualsTestOp{});
}

BL_API_IMPL int blBitSetCompare(const BLBitSetCore* a, const BLBitSetCore* b) noexcept {
  using namespace bl::BitSetInternal;

  BL_ASSERT(a->_d.isBitSet());
  BL_ASSERT(b->_d.isBitSet());

  BLBitSetSegment aSSOSegmentData[3];
  BLBitSetSegment bSSOSegmentData[3];

  BLBitSetSegment* aSegmentData;
  BLBitSetSegment* bSegmentData;

  uint32_t aSegmentCount;
  uint32_t bSegmentCount;

  if (a->_d.sso()) {
    aSegmentData = aSSOSegmentData;
    aSegmentCount = makeSegmentsFromSSOBitSet(aSegmentData, a);
  }
  else {
    aSegmentData = getImpl(a)->segmentData();
    aSegmentCount = getImpl(a)->segmentCount;
  }

  if (b->_d.sso()) {
    bSegmentData = bSSOSegmentData;
    bSegmentCount = makeSegmentsFromSSOBitSet(bSegmentData, b);
  }
  else {
    bSegmentData = getImpl(b)->segmentData();
    bSegmentCount = getImpl(b)->segmentCount;
  }

  return testOp(aSegmentData, aSegmentCount, bSegmentData, bSegmentCount, CompareTestOp{});
}

// bl::BitSet - API - Data Manipulation - Clear
// ============================================

BL_API_IMPL BLResult blBitSetClear(BLBitSetCore* self) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (!self->_d.sso()) {
    BLBitSetImpl* selfI = getImpl(self);
    if (isImplMutable(selfI)) {
      selfI->segmentCount = 0;
      return resetCachedCardinality(self);
    }
    releaseInstance(self);
  }

  return initSSOEmpty(self);
}

// bl::BitSet - API - Data Manipulation - Shrink & Optimize
// ========================================================

namespace bl {
namespace BitSetInternal {

// Calculates the number of segments required to make a BitSet optimized. Optimized BitSet uses
// ranges where applicable and doesn't have any zero segments (Dense segments with all bits zero).
static uint32_t getOptimizedSegmentCount(const BLBitSetSegment* segmentData, uint32_t segmentCount) noexcept {
  uint32_t optimizedSegmentCount = 0;
  const BLBitSetSegment* segmentEnd = segmentData + segmentCount;

  while (segmentData != segmentEnd) {
    segmentData++;
    optimizedSegmentCount++;

    if (!segmentData[-1].allOnes()) {
      QuickDataAnalysis qa = quickDataAnalysis(segmentData[-1].data());
      optimizedSegmentCount -= uint32_t(qa.isZero());

      if (qa.isZero() || !qa.isFull())
        continue;
    }

    // Range segment or Dense segment having all ones.
    uint32_t endWord = segmentData[-1].endWord();
    while (segmentData != segmentEnd && segmentData->startWord() == endWord && (segmentData->allOnes() || isSegmentDataFilled(segmentData->data()))) {
      endWord = segmentData->endWord();
      segmentData++;
    }
  }

  return optimizedSegmentCount;
}

// Copies `src` segments to `dst` and optimizes the output during the copy. The number of segments used
// should match the result of `getOptimizedSegmentCount()` if called with source segments and their size.
static BLBitSetSegment* copyOptimizedSegments(BLBitSetSegment* dst, const BLBitSetSegment* srcData, uint32_t srcCount) noexcept {
  const BLBitSetSegment* srcEnd = srcData + srcCount;

  while (srcData != srcEnd) {
    uint32_t startWord = srcData->startWord();
    srcData++;

    if (!srcData[-1].allOnes()) {
      QuickDataAnalysis qa = quickDataAnalysis(srcData[-1].data());
      if (qa.isZero())
        continue;

      if (!qa.isFull()) {
        initDenseSegmentWithData(*dst++, startWord, srcData[-1].data());
        continue;
      }
    }

    // Range segment or Dense segment having all ones.
    uint32_t endWord = srcData[-1].endWord();
    while (srcData != srcEnd && srcData->startWord() == endWord && (srcData->allOnes() || isSegmentDataFilled(srcData->data()))) {
      endWord = srcData->endWord();
      srcData++;
    }

    initRangeSegment(*dst++, startWord, endWord);
  }

  return dst;
}

static bool testSegmentsForRange(const BLBitSetSegment* segmentData, uint32_t segmentCount, Range* out) noexcept {
  Range range{};

  for (uint32_t i = 0; i < segmentCount; i++) {
    uint32_t startWord = segmentData->startWord();
    uint32_t endWord = segmentData->endWord();

    Range local;

    if (segmentData->allOnes()) {
      local.reset(startWord * BitSetOps::kNumBits, endWord * BitSetOps::kNumBits);
    }
    else {
      PreciseDataAnalysis pa = preciseDataAnalysis(startWord, segmentData->data(), kSegmentWordCount);
      if (!pa.isRange())
        return false;
      local.reset(pa.start, pa.end);
    }

    if (i == 0) {
      range = local;
      continue;
    }

    if (range.end != local.start)
      return false;
    else
      range.end = local.end;
  }

  *out = range;
  return range.valid();
}

static BLResult optimizeInternal(BLBitSetCore* self, bool shrink) noexcept {
  if (self->_d.sso()) {
    if (!self->_d.isBitSetRange()) {
      // Switch to SSO Range if the Dense data actually form a range - SSO Range is preferred over SSO Dense data.
      SSODenseInfo info = getSSODenseInfo(self);
      PreciseDataAnalysis pa = preciseDataAnalysis(info.startWord(), self->_d.u32_data, info.wordCount());

      if (pa.isRange())
        return initSSORange(self, pa.start, pa.end);

      if (pa.empty())
        return initSSOEmpty(self);
    }

    return BL_SUCCESS;
  }

  BLBitSetImpl* selfI = getImpl(self);
  BLBitSetSegment* segmentData = selfI->segmentData();
  uint32_t segmentCount = selfI->segmentCount;
  uint32_t optimizedSegmentCount = getOptimizedSegmentCount(segmentData, segmentCount);

  if (optimizedSegmentCount == 0)
    return blBitSetClear(self);

  // Switch to SSO Dense|Range in case shrink() was called and it's possible.
  if (shrink && optimizedSegmentCount <= 3) {
    BLBitSetSegment optimizedSegmentData[3];
    copyOptimizedSegments(optimizedSegmentData, segmentData, segmentCount);

    // Try SSO range representation.
    Range range;
    if (testSegmentsForRange(optimizedSegmentData, optimizedSegmentCount, &range)) {
      BLBitSetCore tmp;
      initSSORange(&tmp, range.start, range.end);
      return replaceInstance(self, &tmp);
    }

    // Try SSO dense representation.
    if (optimizedSegmentCount <= 2) {
      if (optimizedSegmentCount == 1 || optimizedSegmentData[0].endWord() == optimizedSegmentData[1].startWord()) {
        uint32_t optimizedWordData[kSegmentWordCount * 2];
        bl::MemOps::copyForwardInlineT(optimizedWordData, optimizedSegmentData[0].data(), kSegmentWordCount);
        if (optimizedSegmentCount > 1)
          bl::MemOps::copyForwardInlineT(optimizedWordData + kSegmentWordCount, optimizedSegmentData[1].data(), kSegmentWordCount);

        // Skip zero words from the beginning and from the end.
        const uint32_t* wordData = optimizedWordData;
        const uint32_t* wordEnd = wordData + optimizedSegmentCount * kSegmentWordCount;

        while (wordData != wordEnd && wordData[0] == 0u)
          wordData++;

        while (wordData != wordEnd && wordEnd[-1] == 0u)
          wordEnd--;

        uint32_t startWord = optimizedSegmentData[0].startWord() + uint32_t(wordData - optimizedWordData);
        uint32_t wordCount = (uint32_t)(size_t)(wordEnd - wordData);

        if (wordCount <= kSSOWordCount) {
          uint32_t ssoStartWord = blMin<uint32_t>(startWord, kSSOLastWord);
          uint32_t ssoWordOffset = startWord - ssoStartWord;

          BLBitSetCore tmp;
          initSSODense(&tmp, ssoStartWord);
          bl::MemOps::copyForwardInlineT(tmp._d.u32_data + ssoWordOffset, wordData, wordCount);
          return replaceInstance(self, &tmp);
        }
      }
    }
  }

  if (segmentCount == optimizedSegmentCount)
    return BL_SUCCESS;

  if (isImplMutable(selfI)) {
    copyOptimizedSegments(segmentData, segmentData, segmentCount);
    selfI->segmentCount = optimizedSegmentCount;

    // NOTE: No need to reset cardinality here as it hasn't changed.
    return BL_SUCCESS;
  }
  else {
    BLBitSetCore tmp;
    BLObjectImplSize implSize = implSizeFromCapacity(optimizedSegmentCount);

    BL_PROPAGATE(initDynamic(&tmp, implSize));
    BLBitSetImpl* tmpI = getImpl(&tmp);

    copyOptimizedSegments(tmpI->segmentData(), segmentData, segmentCount);
    tmpI->segmentCount = optimizedSegmentCount;

    return replaceInstance(self, &tmp);
  }
}

} // {BitSetInternal}
} // {bl}

BL_API_IMPL BLResult blBitSetShrink(BLBitSetCore* self) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  return optimizeInternal(self, true);
}

BL_API_IMPL BLResult blBitSetOptimize(BLBitSetCore* self) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  return optimizeInternal(self, false);
}

// bl::BitSet - API - Data Manipulation - Chop
// ===========================================

BL_API_IMPL BLResult blBitSetChop(BLBitSetCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (BL_UNLIKELY(startBit >= endBit)) {
    if (startBit > endBit)
      return blTraceError(BL_ERROR_INVALID_VALUE);
    else
      return blBitSetClear(self);
  }

  // SSO BitSet
  // ----------

  if (self->_d.sso()) {
    if (self->_d.isBitSetRange()) {
      Range range = getSSORange(self).intersect(startBit, endBit);
      range.normalize();
      return initSSORange(self, range.start, range.end);
    }
    else {
      uint32_t tmp[kSSOWordCount + 2];
      SSODenseInfo info = chopSSODenseData(self, tmp, startBit, endBit);

      uint32_t i = 0;
      BL_NOUNROLL
      while (tmp[i] == 0)
        if (++i == info.wordCount())
          return initSSOEmpty(self);

      tmp[kSSOWordCount + 0] = 0u;
      tmp[kSSOWordCount + 1] = 0u;

      uint32_t startWord = blMin<uint32_t>(info.startWord() + i, kSSOLastWord);
      uint32_t wordOffset = startWord - info.startWord();
      return initSSODenseWithData(self, startWord, tmp + wordOffset, kSSOWordCount);
    }
  }

  // Dynamic BitSet
  // --------------

  BLBitSetImpl* selfI = getImpl(self);
  uint32_t segmentCount = selfI->segmentCount;
  BLBitSetSegment* segmentData = selfI->segmentData();

  if (!segmentCount)
    return BL_SUCCESS;

  ChoppedSegments chopped;
  chopSegments(segmentData, segmentCount, startBit, endBit, &chopped);

  if (chopped.empty())
    return blBitSetClear(self);

  uint32_t finalCount = chopped.finalCount();
  if (isImplMutable(selfI) && selfI->segmentCapacity >= finalCount) {
    if (chopped.leadingCount() != chopped.middleIndex())
      memmove(segmentData + chopped.leadingCount(), segmentData + chopped.middleIndex(), chopped.middleCount() * sizeof(BLBitSetSegment));

    bl::MemOps::copyForwardInlineT(segmentData, chopped.leadingData(), chopped.leadingCount());
    bl::MemOps::copyForwardInlineT(segmentData + chopped.leadingCount() + chopped.middleCount(), chopped.trailingData(), chopped.trailingCount());

    selfI->segmentCount = finalCount;
    resetCachedCardinality(self);

    return BL_SUCCESS;
  }
  else {
    BLBitSetCore tmp;
    BL_PROPAGATE(initDynamic(&tmp, implSizeFromCapacity(finalCount)));

    return replaceInstance(self, &tmp);
  }
}

// bl::BitSet - API - Data Manipulation - Add Bit
// ==============================================

BL_API_IMPL BLResult blBitSetAddBit(BLBitSetCore* self, uint32_t bitIndex) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (BL_UNLIKELY(bitIndex == kInvalidIndex))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLBitSetSegment ssoSegments[3];
  BLBitSetSegment* segmentData;

  bool canModify = false;
  uint32_t segmentCount;

  // SSO BitSet
  // ----------

  if (self->_d.sso()) {
    // SSO mode - first check whether the result of the operation can still be stored in SSO storage.
    if (self->_d.isBitSetRange()) {
      Range rSSO = getSSORange(self);

      // Extend the SSO range if the given `bitIndex` is next to its start/end.
      if (bitIndex == rSSO.end)
        return setSSORangeEnd(self, bitIndex + 1);

      if (bitIndex + 1 == rSSO.start)
        return setSSORangeStart(self, bitIndex);

      // Update an empty range [0, 0) if the BitSet is empty.
      if (rSSO.empty())
        return setSSORange(self, bitIndex, bitIndex + 1);

      // Do nothing if the given `bitIndex` lies within the SSO range.
      if (rSSO.hasIndex(bitIndex))
        return BL_SUCCESS;

      // Try to turn this SSO Range into a SSO Dense representation as the result is not a range anymore.
      uint32_t denseFirstWord = wordIndexOf(blMin(rSSO.start, bitIndex));
      uint32_t denseLastWord = wordIndexOf(blMax(rSSO.end - 1, bitIndex));

      // We don't want the SSO data to overflow the addressable words.
      denseFirstWord = blMin<uint32_t>(denseFirstWord, kSSOLastWord);

      if (denseLastWord - denseFirstWord < kSSOWordCount) {
        initSSODense(self, denseFirstWord);
        bl::BitSetOps::bitArrayFill(self->_d.u32_data, rSSO.start - bitIndexOf(denseFirstWord), rSSO.size());
        bl::BitSetOps::bitArraySetBit(self->_d.u32_data, bitIndex - bitIndexOf(denseFirstWord));
        return BL_SUCCESS;
      }
    }
    else {
      // First try whether the `bitIndex` bit lies within the dense SSO data.
      SSODenseInfo info = getSSODenseInfo(self);
      uint32_t wordIndex = wordIndexOf(bitIndex);

      if (wordIndex < info.endWord()) {
        // Just set the bit if it lies within the current window.
        uint32_t startWord = info.startWord();
        if (wordIndex >= startWord) {
          bl::BitSetOps::bitArraySetBit(self->_d.u32_data, bitIndex - info.startBit());
          return BL_SUCCESS;
        }

        // Alternatively, the `bitIndex` could be slightly before the `start`, and in such case we have to test whether
        // there are zero words at the end of the current data. In that case we would have to update the SSO index.
        uint32_t n = getSSOWordCountFromData(self->_d.u32_data, info.wordCount());

        if (wordIndex + kSSOWordCount >= startWord + n) {
          uint32_t tmp[kSSOWordCount];
          bl::MemOps::copyForwardInlineT(tmp, self->_d.u32_data, kSSOWordCount);

          initSSODense(self, wordIndex);
          bl::MemOps::copyForwardInlineT(self->_d.u32_data + (startWord - wordIndex), tmp, n);
          self->_d.u32_data[0] |= bl::BitSetOps::indexAsMask(bitIndex % bl::BitSetOps::kNumBits);

          return BL_SUCCESS;
        }
      }

      // Now we know for sure that the given `bitIndex` is outside of a possible dense SSO area. The only possible
      // case to consider to remain in SSO mode is to check whether the BitSet is actually a range that can be extended
      // by the given `bitIndex` - it can only be extended if the bitIndex is actually on the border of the range.
      PreciseDataAnalysis pa = preciseDataAnalysis(info.startWord(), self->_d.u32_data, info.wordCount());
      BL_ASSERT(!pa.empty());

      if (pa.isRange()) {
        if (bitIndex == pa.end)
          return initSSORange(self, pa.start, bitIndex + 1);

        if (bitIndex == pa.start - 1)
          return initSSORange(self, bitIndex, pa.end);
      }
    }

    // The result of the operation cannot be represented as SSO BitSet. The easiest way to turn this BitSet into a
    // Dynamic representation is to convert the existing SSO representation into segments, and then pretend that this
    // BitSet is not mutable - this would basically go the same path as an immutable BitSet, which is being changed.
    segmentData = ssoSegments;
    segmentCount = makeSegmentsFromSSOBitSet(segmentData, self);
  }
  else {
    BLBitSetImpl* selfI = getImpl(self);
    canModify = isImplMutable(selfI);
    segmentData = selfI->segmentData();
    segmentCount = selfI->segmentCount;
  }

  // Dynamic BitSet
  // --------------

  uint32_t segmentIndex;
  uint32_t wordIndex = wordIndexOf(bitIndex);

  // Optimize the search in case that addRange() is repeatedly called with an increasing bit index.
  if (segmentCount && segmentData[segmentCount - 1].startWord() <= wordIndex)
    segmentIndex = segmentCount - uint32_t(segmentData[segmentCount - 1].endWord() > wordIndex);
  else
    segmentIndex = uint32_t(bl::lowerBound(segmentData, segmentCount, SegmentWordIndex{wordIndex}));

  if (segmentIndex < segmentCount) {
    BLBitSetSegment& segment = segmentData[segmentIndex];
    if (hasSegmentBitIndex(segment, bitIndex)) {
      if (segment.allOnes())
        return BL_SUCCESS;

      if (canModify) {
        addSegmentBit(segment, bitIndex);
        return resetCachedCardinality(self);
      }

      // This prevents making a deep copy in case this is an immutable BitSet and the given `bitIndex` bit is already set.
      if (testSegmentBit(segment, bitIndex))
        return BL_SUCCESS;

      BLBitSetCore tmp = *self;
      BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(segmentCount));

      BL_PROPAGATE(initDynamicWithData(self, implSize, segmentData, segmentCount));
      BLBitSetSegment& dstSegment = getImpl(self)->segmentData()[segmentIndex];
      addSegmentBit(dstSegment, bitIndex);
      return releaseInstance(&tmp);
    }
  }

  // If we are here it means that the given `bitIndex` bit is outside of all segments. This means that we need to
  // insert a new segment to the BitSet. If there is a space in BitSet we can insert it on the fly, if not, or the
  // BitSet is not mutable, we create a new BitSet and insert to it the segments we need.
  uint32_t segmentStartWord = wordIndexOf(bitIndex & ~kSegmentBitMask);

  if (canModify && getImpl(self)->segmentCapacity > segmentCount) {
    // Existing instance can be modified.
    BLBitSetImpl* selfI = getImpl(self);

    selfI->segmentCount++;
    bl::MemOps::copyBackwardInlineT(segmentData + segmentIndex + 1, segmentData + segmentIndex, segmentCount - segmentIndex);

    BLBitSetSegment& dstSegment = segmentData[segmentIndex];
    initDenseSegment(dstSegment, segmentStartWord);
    addSegmentBit(dstSegment, bitIndex);

    return resetCachedCardinality(self);
  }
  else {
    // A new BitSet instance has to be created.
    BLBitSetCore tmp = *self;
    BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(segmentCount + 1));

    BL_PROPAGATE(initDynamic(self, implSize));
    BLBitSetImpl* selfI = getImpl(self);

    bl::MemOps::copyForwardInlineT(selfI->segmentData(), segmentData, segmentIndex);
    bl::MemOps::copyForwardInlineT(selfI->segmentData() + segmentIndex + 1, segmentData + segmentIndex, segmentCount - segmentIndex);
    selfI->segmentCount = segmentCount + 1;

    BLBitSetSegment& dstSegment = selfI->segmentData()[segmentIndex];
    initDenseSegment(dstSegment, segmentStartWord);
    addSegmentBit(dstSegment, bitIndex);

    return releaseInstance(&tmp);
  }
}

// bl::BitSet - API - Data Manipulation - Add Range
// ================================================

BL_API_IMPL BLResult blBitSetAddRange(BLBitSetCore* self, uint32_t rangeStartBit, uint32_t rangeEndBit) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (BL_UNLIKELY(rangeStartBit >= rangeEndBit)) {
    if (rangeStartBit > rangeEndBit)
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return BL_SUCCESS;
  }

  BLBitSetSegment ssoSegments[3];
  BLBitSetSegment* segmentData;

  bool canModify = false;
  uint32_t segmentCount;

  uint32_t rangeStartWord = wordIndexOf(rangeStartBit);
  uint32_t rangeLastWord = wordIndexOf(rangeEndBit - 1);

  // SSO BitSet
  // ----------

  if (self->_d.sso()) {
    // SSO mode - first check whether the result of the operation can still be stored in SSO storage.
    if (self->_d.isBitSetRange()) {
      Range rSSO = getSSORange(self);

      // Update the SSO range if the given range extends SSO range.
      if ((rangeStartBit <= rSSO.end) & (rangeEndBit >= rSSO.start))
        return setSSORange(self, blMin(rangeStartBit, rSSO.start), blMax(rangeEndBit, rSSO.end));

      if (rSSO.empty())
        return setSSORange(self, rangeStartBit, rangeEndBit);

      // Try to turn this SSO Range into a SSO Dense representation as the result is not a range anymore.
      uint32_t denseFirstWord = blMin(rangeStartWord, wordIndexOf(rSSO.start));
      uint32_t denseLastWord = blMax(rangeLastWord, wordIndexOf(rSSO.end - 1));

      // We don't want the SSO data to overflow the addressable words.
      denseFirstWord = blMin<uint32_t>(denseFirstWord, kSSOLastWord);

      if (denseLastWord - denseFirstWord < kSSOWordCount) {
        initSSODense(self, denseFirstWord);
        bl::BitSetOps::bitArrayFill(self->_d.u32_data, rSSO.start - bitIndexOf(denseFirstWord), rSSO.size());
        bl::BitSetOps::bitArrayFill(self->_d.u32_data, rangeStartBit - bitIndexOf(denseFirstWord), rangeEndBit - rangeStartBit);
        return BL_SUCCESS;
      }
    }
    else {
      // First try whether the range lies within the dense SSO data.
      SSODenseInfo info = getSSODenseInfo(self);

      if (rangeLastWord < info.endWord()) {
        // Just fill the range if it lies within the current window.
        uint32_t iStartWord = info.startWord();
        if (rangeStartWord >= iStartWord) {
          bl::BitSetOps::bitArrayFill(self->_d.u32_data, rangeStartBit - info.startBit(), rangeEndBit - rangeStartBit);
          return BL_SUCCESS;
        }

        // Alternatively, the range could be slightly before the start of the dense data, and in such case we have
        // to test whether there are zero words at the end of the current data and update SSO dense data start when
        // necessary.
        uint32_t n = getSSOWordCountFromData(self->_d.u32_data, info.wordCount());

        if ((rangeLastWord - rangeStartWord) < kSSOWordCount && rangeLastWord < iStartWord + n) {
          uint32_t tmp[kSSOWordCount];
          bl::MemOps::copyForwardInlineT(tmp, self->_d.u32_data, kSSOWordCount);

          initSSODense(self, rangeStartWord);
          bl::MemOps::copyForwardInlineT(self->_d.u32_data + (iStartWord - rangeStartWord), tmp, n);
          bl::BitSetOps::bitArrayFill(self->_d.u32_data, rangeStartBit - bitIndexOf(rangeStartWord), rangeEndBit - rangeStartBit);

          return BL_SUCCESS;
        }
      }

      // We have to guarantee that a result of any operaton in SSO mode must also stay in SSO mode if representable.
      // To simplify all the remaining checks we copy the current content to a temporary buffer and fill the
      // intersecting part of it, otherwise we wouldn't do it properly and we will miss cases that we shouldn't.
      uint32_t tmp[kSSOWordCount];
      bl::MemOps::copyForwardInlineT(tmp, self->_d.u32_data, kSSOWordCount);

      Range intersection = Range{rangeStartWord, rangeLastWord + 1}.intersect(info.startWord(), info.endWord());
      if (!intersection.empty()) {
        uint32_t iFirst = blMax(info.startBit(), rangeStartBit);
        uint32_t iLast = blMin(info.lastBit(), rangeEndBit - 1);
        bl::BitSetOps::bitArrayFill(tmp, iFirst - info.startBit(), iLast - iFirst + 1);
      }

      PreciseDataAnalysis pa = preciseDataAnalysis(info.startWord(), tmp, info.wordCount());
      BL_ASSERT(!pa.empty());

      if (pa.isRange() && ((rangeStartBit <= pa.end) & (rangeEndBit >= pa.start)))
        return initSSORange(self, blMin(rangeStartBit, pa.start), blMax(rangeEndBit, pa.end));
    }

    // The result of the operation cannot be represented as SSO BitSet.
    segmentData = ssoSegments;
    segmentCount = makeSegmentsFromSSOBitSet(segmentData, self);
  }
  else {
    BLBitSetImpl* selfI = getImpl(self);

    canModify = isImplMutable(selfI);
    segmentData = selfI->segmentData();
    segmentCount = selfI->segmentCount;
  }

  // Dynamic BitSet
  // --------------

  // Optimize the search in case that addRange() is repeatedly called with increasing start/end indexes.
  uint32_t segmentIndex;
  if (segmentCount && segmentData[segmentCount - 1].startWord() <= rangeStartWord)
    segmentIndex = segmentCount - uint32_t(segmentData[segmentCount - 1].endWord() > rangeStartWord);
  else
    segmentIndex = uint32_t(bl::lowerBound(segmentData, segmentCount, SegmentWordIndex{rangeStartWord}));

  // If the range spans across a single segment or segments that have all bits set, we can avoid a more generic case.
  while (segmentIndex < segmentCount) {
    BLBitSetSegment& segment = segmentData[segmentIndex];
    if (!hasSegmentWordIndex(segment, rangeStartWord))
      break;

    if (segment.allOnes()) {
      // Skip intersecting segments, which are all ones.
      rangeStartWord = segment._rangeEndWord();
      rangeStartBit = bitIndexOf(rangeStartWord);

      // Quicky return if this Range segment completely subsumes the range to be added.
      if (rangeStartBit >= rangeEndBit)
        return BL_SUCCESS;

      segmentIndex++;
    }
    else {
      // Only change data within a single segment. The reason is that we cannot start changing segments without
      // knowing whether we would need to grow the BitSet, which could fail if memory allocation fails. Blend2D
      // API is transactional, which means that on failure the content of the BitSet must be kept unmodified.
      if (canModify && rangeLastWord < segment._denseEndWord()) {
        addSegmentRange(segment, rangeStartBit, rangeEndBit - rangeStartBit);
        return resetCachedCardinality(self);
      }

      break;
    }
  }

  // Build an array of segments that will replace matching segments in the BitSet.
  StaticSegmentInserter<8> inserter;
  uint32_t insertIndex = segmentIndex;

  do {
    // Create a Range segment if the range starts/ends a segment boundary or spans across multiple segments.
    uint32_t rangeSize = rangeEndBit - rangeStartBit;
    if (isBitAlignedToSegment(rangeStartBit) && rangeSize >= kSegmentBitCount) {
      uint32_t segmentEndWord = wordIndexOf(alignBitDownToSegment(rangeEndBit));

      // Check whether it would be possible to merge this Range segment with a previous Range segment.
      if (inserter.empty() && segmentIndex > 0) {
        const BLBitSetSegment& prev = segmentData[segmentIndex - 1];
        if (prev.allOnes() && prev._rangeEndWord() == rangeStartWord) {
          // Merging is possible - this effectively decreases the index for insertion as we replace a previous segment.
          insertIndex--;

          // Don't duplicate the code required to insert a new range here as there is few cases to handle.
          rangeStartWord = prev.startWord();
          goto InitRange;
        }
      }

      // We know that we cannot merge this range with the previous one. In general it's required to have at least
      // two segments in order to create a Range segment, otherwise a regular Dense segment must be used.
      if (rangeSize >= kSegmentBitCount * 2) {
InitRange:
        initRangeSegment(inserter.current(), rangeStartWord, segmentEndWord);
        inserter.advance();

        rangeStartWord = segmentEndWord;
        rangeStartBit = bitIndexOf(rangeStartWord);

        // Discard all segments that the new Range segment overlaps.
        while (segmentIndex < segmentCount && segmentData[segmentIndex].startWord() < rangeStartWord)
          segmentIndex++;

        // If the last discarded segment overruns this one, then we have to merge it
        if (segmentIndex != 0) {
          const BLBitSetSegment& prev = segmentData[segmentIndex - 1];
          if (prev.allOnes() && prev._rangeEndWord() > rangeStartWord) {
            inserter.prev()._setRangeEndWord(prev._rangeEndWord());
            break;
          }
        }

        continue;
      }
    }

    // Create a Dense segment if the Range check failed.
    rangeSize = blMin<uint32_t>(rangeSize, kSegmentBitCount - (rangeStartBit & kSegmentBitMask));
    initDenseSegmentWithRange(inserter.current(), rangeStartBit, rangeSize);
    inserter.advance();

    if (segmentIndex < segmentCount && hasSegmentWordIndex(segmentData[segmentIndex], rangeStartWord)) {
      if (segmentData[segmentIndex].allOnes()) {
        // This cannot happen with a leading segment as the case must have been already detected in the previous loop.
        // We know that a Range segment spans always at least 2 segments, so we can safely terminate the loop even
        // when this is a middle segment followed by a trailing one.
        BL_ASSERT(isBitAlignedToSegment(rangeStartBit));
        break;
      }
      else {
        bl::BitSetOps::bitArrayCombineWords<bl::BitOperator::Or>(inserter.prev().data(), segmentData[segmentIndex].data(), kSegmentWordCount);
        segmentIndex++;
      }
    }

    rangeStartBit += rangeSize;
    rangeStartWord = wordIndexOf(rangeStartBit);
  } while (rangeStartBit < rangeEndBit);

  if (segmentIndex < segmentCount) {
    BLBitSetSegment& next = segmentData[segmentIndex];
    if (next.allOnes() && next.startWord() <= inserter.prev().startWord()) {
      initRangeSegment(inserter.current(), inserter.prev().startWord(), next.endWord());
      inserter.advance();
      segmentIndex++;
    }
  }

  return spliceInternal(self, segmentData, segmentCount, insertIndex, segmentIndex - insertIndex, inserter.segments(), inserter.count(), canModify);
}

// bl::BitSet - API - Data Manipulation - Add Words
// ================================================

namespace bl {
namespace BitSetInternal {

// Inserts temporary segments into segmentData.
//
// SegmentData must have at least `segmentCount + insertedCount` capacity - because the merged segments are inserted
// to `segmentData`. This function does merge from the end to ensure that we won't overwrite segments during merging.
static BL_INLINE void mergeInsertedSegments(BLBitSetSegment* segmentData, uint32_t segmentCount, const BLBitSetSegment* insertedData, uint32_t insertedCount) noexcept {
  BLBitSetSegment* p = segmentData + segmentCount + insertedCount;

  const BLBitSetSegment* segmentEnd = segmentData + segmentCount;
  const BLBitSetSegment* insertedEnd = insertedData + insertedCount;

  while (segmentData != segmentEnd && insertedData != insertedEnd) {
    const BLBitSetSegment* src;
    if (segmentEnd[-1].startWord() > insertedEnd[-1].startWord())
      src = --segmentEnd;
    else
      src = --insertedEnd;
    *--p = *src;
  }

  while (insertedData != insertedEnd) {
    *--p = *--insertedEnd;
  }

  // Make sure we ended at the correct index after merge.
  BL_ASSERT(p == segmentEnd);
}

} // {BitSetInternal}
} // {bl}

BL_API_IMPL BLResult blBitSetAddWords(BLBitSetCore* self, uint32_t startWord, const uint32_t* wordData, uint32_t wordCount) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  BL_PROPAGATE(normalizeWordDataParams(startWord, wordData, wordCount));
  if (!wordCount)
    return BL_SUCCESS;

  BLBitSetSegment ssoSegmentData[3];
  BLBitSetSegment* segmentData = nullptr;
  uint32_t segmentCount = 0;
  uint32_t segmentCapacity = 0;

  bl::ScopedBufferTmp<sizeof(BLBitSetSegment) * kTmpSegmentDataSize> tmpSegmentBuffer;
  DynamicSegmentInserter inserter;

  // SSO BitSet
  // ----------

  if (self->_d.sso()) {
    // Try some optimized SSO cases first if the BitSet is in SSO mode.
    if (isSSOEmpty(self))
      return blBitSetAssignWords(self, startWord, wordData, wordCount);

    if (!self->_d.isBitSetRange()) {
      uint32_t ssoWordIndex = getSSOWordIndex(self);
      uint32_t ssoWordCount = getSSOWordCountFromData(self->_d.u32_data, kSSOWordCount);

      if (startWord < ssoWordIndex) {
        uint32_t distance = ssoWordIndex - startWord;
        if (distance + ssoWordCount <= kSSOWordCount) {
          BLBitSetCore tmp;
          initSSODense(&tmp, startWord);

          bl::MemOps::copyForwardInlineT(tmp._d.u32_data, wordData, wordCount);
          bl::MemOps::combineSmall<bl::BitOperator::Or>(tmp._d.u32_data, self->_d.u32_data + distance, ssoWordCount);

          self->_d = tmp._d;
          return BL_SUCCESS;
        }
      }
      else {
        uint32_t distance = startWord - ssoWordIndex;
        if (distance + wordCount <= kSSOWordCount) {
          bl::MemOps::combineSmall<bl::BitOperator::Or>(self->_d.u32_data + distance, wordData, wordCount);
          return BL_SUCCESS;
        }
      }
    }

    segmentData = ssoSegmentData;
    segmentCount = makeSegmentsFromSSOBitSet(segmentData, self);
  }
  else {
    BLBitSetImpl* selfI = getImpl(self);

    segmentData = selfI->segmentData();
    segmentCount = selfI->segmentCount;

    if (!segmentCount)
      return blBitSetAssignWords(self, startWord, wordData, wordCount);

    if (isImplMutable(selfI))
      segmentCapacity = selfI->segmentCapacity;
  }

  // Dynamic BitSet (or SSO BitSet As Segments)
  // ------------------------------------------

  uint32_t startWordAlignedToSegment = alignWordDownToSegment(startWord);
  uint32_t endWordAlignedToSegment = alignWordUpToSegment(startWord + wordCount);

  // Find the first segment we have to modify.
  BL_ASSERT(segmentCount > 0);
  uint32_t segmentIndex = segmentCount;

  if (segmentData[segmentCount - 1].endWord() > startWordAlignedToSegment)
    segmentIndex = uint32_t(bl::lowerBound(segmentData, segmentCount, SegmentWordIndex{startWordAlignedToSegment}));

  uint32_t wordIndexEnd = startWord + wordCount;
  uint32_t insertSegmentCount = (endWordAlignedToSegment - startWordAlignedToSegment) / kSegmentWordCount;

  // We need a temporary storage for segments to be inserted in case that any of the existing segment overlaps with
  // wordData. In that case `tmpSegmentBuffer` will be used to store such segments, and these segments will be merged
  // with BitSet at the end of the function.
  bool requiresTemporaryStorage = (segmentIndex != segmentCount && insertSegmentCount > 0);

  if (requiresTemporaryStorage) {
    BLBitSetSegment* p = static_cast<BLBitSetSegment*>(tmpSegmentBuffer.alloc(insertSegmentCount * sizeof(BLBitSetSegment)));
    if (BL_UNLIKELY(!p))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    inserter.reset(p, insertSegmentCount);
  }

  if (segmentCount + insertSegmentCount > segmentCapacity) {
    // If there is not enough capacity or the BitSet is not mutable, do a more precise analysis.
    WordDataAnalysis analysis = analyzeWordDataForCombining(startWord, wordData, wordCount, segmentData + segmentIndex, segmentCount - segmentIndex);
    insertSegmentCount = analysis.segmentCount;

    if (segmentCount + insertSegmentCount > segmentCapacity) {
      // Allocate a new Impl.
      BLBitSetCore tmp;
      BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(segmentCount + insertSegmentCount));
      BL_PROPAGATE(initDynamic(&tmp, implSize));

      BLBitSetImpl* newI = getImpl(&tmp);
      memcpy(newI->segmentData(), segmentData, segmentCount * sizeof(BLBitSetSegment));
      segmentData = newI->segmentData();
      segmentCapacity = getImpl(&tmp)->segmentCapacity;

      replaceInstance(self, &tmp);
    }
  }

  if (!requiresTemporaryStorage)
    inserter.reset(segmentData + segmentCount, segmentCapacity - segmentCount);

  // Leading segment requires special handling if it doesn't start at a segment boundary.
  uint32_t wordIndex = startWordAlignedToSegment;
  if (wordIndex != startWord) {
    uint32_t segmentWordOffset = startWord - wordIndex;
    uint32_t segmentWordCount = blMin<uint32_t>(wordCount, kSegmentWordCount - segmentWordOffset);

    if (segmentIndex != segmentCount && hasSegmentWordIndex(segmentData[segmentIndex], wordIndex)) {
      if (!segmentData[segmentIndex].allOnes())
        bl::MemOps::combineSmall<bl::BitOperator::Or>(segmentData[segmentIndex].data() + segmentWordOffset, wordData, segmentWordCount);

      if (segmentData[segmentIndex].endWord() == wordIndex + kSegmentWordCount)
        segmentIndex++;
    }
    else {
      initDenseSegment(inserter.current(), wordIndex);
      bl::MemOps::copyForwardInlineT(inserter.current().data() + segmentWordOffset, wordData, segmentWordCount);
      inserter.advance();
    }

    wordData += segmentWordCount;
    wordCount -= segmentWordCount;
    wordIndex += kSegmentWordCount;
  }

  // Main loop - wordIndex is aligned to a segment boundary, so process a single segment at a time.
  uint32_t wordIndexAlignedEnd = wordIndex + bl::IntOps::alignDown<uint32_t>(wordCount, kSegmentWordCount);
  while (wordIndex != wordIndexAlignedEnd) {
    // Combine with an existing segment, if there is an intersection.
    if (segmentIndex != segmentCount) {
      BLBitSetSegment& current = segmentData[segmentIndex];
      if (hasSegmentWordIndex(current, wordIndex)) {
        if (current.allOnes()) {
          // Terminate if the current Range segment completely subsumes the remaining words.
          if (current._rangeEndWord() >= wordIndexEnd)
            break;

          uint32_t skipCount = current._rangeEndWord() - wordIndex;
          wordData += skipCount;
          wordIndex += skipCount;
        }
        else {
          bl::MemOps::combineSmall<bl::BitOperator::Or>(current.data(), wordData, kSegmentWordCount);
          wordData += kSegmentWordCount;
          wordIndex += kSegmentWordCount;
        }

        segmentIndex++;
        continue;
      }
    }

    // The data doesn't overlap with an existing segment.
    QuickDataAnalysis qa = quickDataAnalysis(wordData);
    uint32_t initialWordIndex = wordIndex;

    // Advance here so we don't have to do it.
    wordData += kSegmentWordCount;
    wordIndex += kSegmentWordCount;

    // Handle a zero segment - this is a good case as BitSet builders can use more words than a
    // single segment occupies. So if the whole segment is zero, don't create it to save space.
    if (qa.isZero())
      continue;

    // Handle a full segment - either merge with the previous range segment or try to find more
    // full segments and create a new one if merging is not possible.
    if (qa.isFull()) {
      uint32_t rangeEndWord = wordIndexAlignedEnd;

      // Merge with the previous segment, if possible.
      if (segmentIndex > 0) {
        BLBitSetSegment& prev = segmentData[segmentIndex - 1];
        if (prev.allOnes() && prev._rangeEndWord() == initialWordIndex) {
          prev._setRangeEndWord(wordIndex);
          continue;
        }
      }

      // Merge with the next segment, if possible.
      BLBitSetSegment* next = nullptr;
      if (segmentIndex < segmentCount) {
        next = &segmentData[segmentIndex];
        rangeEndWord = blMin<uint32_t>(rangeEndWord, next->endWord());

        if (next->startWord() == wordIndex) {
          if (next->allOnes()) {
            next->_setRangeStartWord(initialWordIndex);
            continue;
          }
        }
      }

      // Analyze how many full segments are next to each other.
      while (wordIndex != rangeEndWord) {
        if (!isSegmentDataFilled(wordData))
          break;

        wordData += kSegmentWordCount;
        wordIndex += kSegmentWordCount;
      }

      // Create a Range segment if two or more full segments are next to each other.
      if (initialWordIndex - wordIndex > kSegmentWordCount) {
        if (next) {
          if (next->allOnes() && wordIndex >= next->startWord()) {
            next->_setRangeStartWord(initialWordIndex);
            continue;
          }

          if (wordIndex > next->startWord()) {
            initRangeSegment(*next, initialWordIndex, wordIndex);
            segmentIndex++;
            continue;
          }
        }

        // Insert a new Range segment.
        initRangeSegment(inserter.current(), initialWordIndex, wordIndex);
        inserter.advance();
        continue;
      }
    }

    // Insert a new Dense segment.
    initDenseSegmentWithData(inserter.current(), wordIndex - kSegmentWordCount, wordData - kSegmentWordCount);
    inserter.advance();
  }

  // Tail segment requires special handling if it doesn't end on a segment boundary.
  //
  // NOTE: We don't have to analyze the data as we already know it's not a full segment and that it's not empty.
  if (wordIndex < wordIndexEnd) {
    if (segmentIndex != segmentCount && hasSegmentWordIndex(segmentData[segmentIndex], wordIndex)) {
      // Combine with an existing segment, if data and segment overlaps.
      BLBitSetSegment& current = segmentData[segmentIndex];
      if (!current.allOnes())
        bl::MemOps::combineSmall<bl::BitOperator::Or>(current.data(), wordData, wordIndexEnd - wordIndexAlignedEnd);
      segmentIndex++;
    }
    else {
      // Insert a new Dense segment if data doesn't overlap with an existing segment.
      initDenseSegment(inserter.current(), wordIndex);
      bl::MemOps::copyForwardInlineT(inserter.current().data(), wordData, wordIndexEnd - wordIndexAlignedEnd);
      inserter.advance();
    }
  }

  // Merge temporarily created segments to BitSet, if any.
  if (!inserter.empty() && requiresTemporaryStorage)
    mergeInsertedSegments(segmentData, segmentCount, inserter.segments(), inserter.index());

  getImpl(self)->segmentCount = segmentCount + inserter.index();
  resetCachedCardinality(self);
  return BL_SUCCESS;
}

// bl::BitSet - API - Data Manipulation - Clear Bit
// ================================================

BL_API_IMPL BLResult blBitSetClearBit(BLBitSetCore* self, uint32_t bitIndex) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (BL_UNLIKELY(bitIndex == kInvalidIndex))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLBitSetSegment ssoSegments[3];
  BLBitSetSegment* segmentData;

  bool canModify = false;
  uint32_t segmentCount;

  // SSO BitSet
  // ----------

  if (self->_d.sso()) {
    // SSO mode - first check whether the result of the operation can still be represented as SSO.
    if (self->_d.isBitSetRange()) {
      Range rSSO = getSSORange(self);

      // Nothing to do if the `bitIndex` is outside of SSO range.
      if (!rSSO.hasIndex(bitIndex))
        return BL_SUCCESS;

      // Shrink the SSO range if the given `bitIndex` is at start/end.
      if (bitIndex == rSSO.start) {
        // We would never allow an empty range like [12:12) - if this happens turn the bit set to an empty one.
        if (bitIndex + 1 == rSSO.end)
          return initSSOEmpty(self);
        else
          return setSSORangeStart(self, bitIndex + 1);
      }

      if (bitIndex == rSSO.end - 1)
        return setSSORangeEnd(self, bitIndex);

      // We know that the bitIndex is somewhere inside the SSO range, but not at the start/end. If the range can
      // be represented as a dense SSO BitSet then it's guaranteed that the result would also fit in SSO storage.
      uint32_t firstWord = wordIndexOf(rSSO.start);
      uint32_t lastWord = wordIndexOf(rSSO.end - 1u);

      if (lastWord - firstWord < kSSOWordCount) {
        initSSODense(self, firstWord);
        bl::BitSetOps::bitArrayFill(self->_d.u32_data, rSSO.start % bl::BitSetOps::kNumBits, rSSO.size());
        bl::BitSetOps::bitArrayClearBit(self->_d.u32_data, bitIndex - bitIndexOf(firstWord));
        return BL_SUCCESS;
      }
    }
    else {
      // This will always succeed. However, one thing that we have to guarantee is that if the first word is cleared
      // to zero we offset the start of the BitSet to the first non-zero word - and if the cleared bit was the last
      // one in the entire BitSet we turn it to an empty BitSet, which has always the same signature in SSO mode.
      SSODenseInfo info = getSSODenseInfo(self);

      if (!info.hasIndex(bitIndex))
        return BL_SUCCESS;

      // No data shift necessary if the first word is non-zero after the operation.
      bl::BitSetOps::bitArrayClearBit(self->_d.u32_data, bitIndex - info.startBit());
      if (self->_d.u32_data[0] != 0)
        return BL_SUCCESS;

      // If the first word was cleared out, it would most likely have to be shifted and start index updated.
      uint32_t i = 1;
      uint32_t buffer[kSSOWordCount];
      bl::MemOps::copyForwardInlineT(buffer, self->_d.u32_data, kSSOWordCount);

      BL_NOUNROLL
      while (buffer[i] == 0)
        if (++i == info.wordCount())
          return initSSOEmpty(self);

      uint32_t startWord = blMin<uint32_t>(info.startWord() + i, kSSOLastWord);
      uint32_t shift = startWord - info.startWord();
      return initSSODenseWithData(self, startWord, buffer + shift, info.wordCount() - shift);
    }

    // The result of the operation cannot be represented as SSO BitSet.
    segmentData = ssoSegments;
    segmentCount = makeSegmentsFromSSOBitSet(segmentData, self);
  }
  else {
    BLBitSetImpl* selfI = getImpl(self);

    canModify = isImplMutable(selfI);
    segmentData = selfI->segmentData();
    segmentCount = selfI->segmentCount;
  }

  // Dynamic BitSet
  // --------------

  // Nothing to do if the bit of the given `bitIndex` is not within any segment.
  uint32_t segmentIndex = uint32_t(bl::lowerBound(segmentData, segmentCount, SegmentWordIndex{wordIndexOf(bitIndex)}));
  if (segmentIndex >= segmentCount)
    return BL_SUCCESS;

  BLBitSetSegment& segment = segmentData[segmentIndex];
  if (!hasSegmentBitIndex(segment, bitIndex))
    return BL_SUCCESS;

  if (segment.allOnes()) {
    // The hardest case. If this segment is all ones, it's longer run of ones, which means that we will have to split
    // the segment into 2 or 3 segments, which would replace the original one.
    StaticSegmentInserter<3> inserter;

    uint32_t initialSegmentStartWord = segment._rangeStartWord();
    uint32_t middleSegmentStartWord = wordIndexOf(bitIndex & ~kSegmentBitMask);
    uint32_t finalSegmentStartWord = middleSegmentStartWord + kSegmentWordCount;

    // Calculate initial segment, if exists.
    if (initialSegmentStartWord < middleSegmentStartWord) {
      if (middleSegmentStartWord - initialSegmentStartWord <= kSegmentWordCount)
        initDenseSegmentWithOnes(inserter.current(), initialSegmentStartWord);
      else
        initRangeSegment(inserter.current(), initialSegmentStartWord, middleSegmentStartWord);
      inserter.advance();
    }

    // Calculate middle segment (always exists).
    initDenseSegmentWithOnes(inserter.current(), middleSegmentStartWord);
    clearSegmentBit(inserter.current(), bitIndex);
    inserter.advance();

    // Calculate final segment, if exists.
    if (finalSegmentStartWord < segment._rangeEndWord()) {
      if (segment._rangeEndWord() - finalSegmentStartWord <= kSegmentWordCount)
        initDenseSegmentWithOnes(inserter.current(), finalSegmentStartWord);
      else
        initRangeSegment(inserter.current(), finalSegmentStartWord, segment._rangeEndWord());
      inserter.advance();
    }

    return spliceInternal(self, segmentData, segmentCount, segmentIndex, 1, inserter.segments(), inserter.count(), canModify);
  }
  else {
    if (canModify) {
      clearSegmentBit(segment, bitIndex);
      return resetCachedCardinality(self);
    }

    // If the BitSet is immutable we have to create a new one. First copy all segments, then modify the required one.
    BLBitSetCore tmp = *self;
    BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(segmentCount));

    BL_PROPAGATE(initDynamicWithData(self, implSize, segmentData, segmentCount));
    BLBitSetSegment& dstSegment = getImpl(self)->segmentData()[segmentIndex];
    clearSegmentBit(dstSegment, bitIndex);
    return releaseInstance(&tmp);
  }
}

// bl::BitSet - API - Data Manipulation - Clear Range
// ==================================================

BL_API_IMPL BLResult blBitSetClearRange(BLBitSetCore* self, uint32_t rangeStartBit, uint32_t rangeEndBit) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (BL_UNLIKELY(rangeStartBit >= rangeEndBit)) {
    if (rangeStartBit > rangeEndBit)
      return blTraceError(BL_ERROR_INVALID_VALUE);
    return BL_SUCCESS;
  }

  BLBitSetSegment ssoSegments[3];
  BLBitSetSegment* segmentData;

  bool canModify = false;
  uint32_t segmentCount;

  // SSO BitSet
  // ----------

  if (self->_d.sso()) {
    // SSO mode - first check whether the result of the operation can still be represented as SSO.
    if (self->_d.isBitSetRange()) {
      Range rSSO = getSSORange(self);

      // NOP if the given range doesn't cross SSO range.
      Range intersection = rSSO.intersect(rangeStartBit, rangeEndBit);
      if (intersection.empty())
        return BL_SUCCESS;

      if (intersection.start == rSSO.start) {
        // If the given range intersects SSO range fully it would make the BitSet empty.
        if (intersection.end == rSSO.end)
          return initSSOEmpty(self);
        else
          return setSSORangeStart(self, intersection.end);
      }

      if (intersection.end == rSSO.end)
        return setSSORangeEnd(self, intersection.start);

      // We know that the range is somewhere inside the SSO range, but not at the start/end. If the range can be
      // represented as a dense SSO BitSet then it's guaranteed that the result would also fit in SSO storage.
      uint32_t denseFirstWord = wordIndexOf(rSSO.start);
      uint32_t denseLastWord = wordIndexOf(rSSO.end - 1u);

      if (denseFirstWord - denseLastWord < kSSOWordCount) {
        initSSODense(self, denseFirstWord);
        bl::BitSetOps::bitArrayFill(self->_d.u32_data, rSSO.start % bl::BitSetOps::kNumBits, rSSO.size());
        bl::BitSetOps::bitArrayClear(self->_d.u32_data, intersection.start - bitIndexOf(denseFirstWord), intersection.size());
        return BL_SUCCESS;
      }
    }
    else {
      // This will always succeed. However, one thing that we have to guarantee is that if the first word is cleared
      // to zero we offset the start of the BitSet to the first non-zero word - and if the cleared bit was the last
      // one in the entire BitSet we turn it to an empty BitSet, which has always the same signature in SSO mode.
      SSODenseInfo info = getSSODenseInfo(self);

      uint32_t rStart = blMax(rangeStartBit, info.startBit());
      uint32_t rLast = blMin(rangeEndBit - 1, info.lastBit());

      // Nothing to do if the given range is outside of the SSO range.
      if (rStart > rLast)
        return BL_SUCCESS;

      // No data shift necessary if the first word is non-zero after the operation.
      bl::BitSetOps::bitArrayClear(self->_d.u32_data, rStart - info.startBit(), rLast - rStart + 1);
      if (self->_d.u32_data[0] != 0)
        return BL_SUCCESS;

      // If the first word was cleared out, it would most likely have to be shifted and start index updated.
      uint32_t i = 1;
      uint32_t buffer[kSSOWordCount];
      bl::MemOps::copyForwardInlineT(buffer, self->_d.u32_data, kSSOWordCount);

      BL_NOUNROLL
      while (buffer[i] == 0)
        if (++i == info.wordCount())
          return initSSOEmpty(self);

      uint32_t startWord = blMin<uint32_t>(info.startWord() + i, kSSOLastWord);
      uint32_t shift = startWord - info.startWord();
      return initSSODenseWithData(self, startWord, buffer + shift, info.wordCount() - shift);
    }

    // The result of the operation cannot be represented as SSO BitSet.
    segmentData = ssoSegments;
    segmentCount = makeSegmentsFromSSOBitSet(segmentData, self);
  }
  else {
    BLBitSetImpl* selfI = getImpl(self);

    canModify = isImplMutable(selfI);
    segmentData = selfI->segmentData();
    segmentCount = selfI->segmentCount;
  }

  // Dynamic BitSet
  // --------------

  uint32_t rangeStartWord = wordIndexOf(rangeStartBit);
  uint32_t rangeLastWord = wordIndexOf(rangeEndBit - 1);
  uint32_t segmentIndex = uint32_t(bl::lowerBound(segmentData, segmentCount, SegmentWordIndex{rangeStartWord}));

  // If no existing segment matches the range to clear, then there is nothing to clear.
  if (segmentIndex >= segmentCount)
    return BL_SUCCESS;

  // Build an array of segments that will replace matching segments in the BitSet.
  StaticSegmentInserter<8> inserter;
  uint32_t insertIndex = segmentIndex;

  do {
    const BLBitSetSegment& segment = segmentData[segmentIndex];
    uint32_t segmentStartWord = segment.startWord();
    uint32_t segmentEndWord = segment.endWord();

    // Discard non-intersecting areas.
    if (rangeStartWord < segmentStartWord) {
      rangeStartWord = segmentStartWord;
      rangeStartBit = bitIndexOf(rangeStartWord);

      if (rangeStartWord > rangeLastWord)
        break;
    }

    // If the range to clear completely overlaps this segment, remove it.
    if (rangeLastWord >= segmentEndWord && rangeStartBit == segment.startBit())
      continue;

    // The range to clear doesn't completely overlap this segment, so clear the bits required.
    if (segment.allOnes()) {
      // More complicated case - we have to split the range segment into 1 or 4 segments depending on
      // where the input range intersects with the segment. See the illustration below that describes
      // a possible result of this operation:
      //
      // +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
      // |           Existing range segment spanning across multiple segment boundaries            | <- Input Segment
      // +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
      //
      //                                +--------------------------+
      // + <- Segment boundaries        |   Input range to clear   |                                 <- Clear Range
      //                                +--------------------------+
      //
      // +--------+--------+--------+--------+                 +--------+--------+--------+--------+
      // |    New leading range     |DenseSeg| Cleared entirely|DenseSeg|    New trailing range    | <- Output Segments
      // +--------+--------+--------+--------+                 +--------+--------+--------+--------+
      //
      // NOTE: Every time we insert a new segment, `segmentStartWord` gets updated to reflect the remaining slice.

      // Handle a possible leading segment, which won't be cleared.
      uint32_t rangeStartSegmentWord = alignWordDownToSegment(rangeStartWord);
      if (segmentStartWord < rangeStartSegmentWord) {
        if (rangeStartSegmentWord - segmentStartWord >= kSegmentWordCount * 2) {
          // If the leading range spans across two or more segments, insert a Range segment.
          initRangeSegment(inserter.current(), segmentStartWord, rangeStartSegmentWord);
          inserter.advance();
        }
        else {
          // If the learing range covers only a single segment, insert a Dense segment.
          initDenseSegmentWithOnes(inserter.current(), segmentStartWord);
          inserter.advance();
        }

        // NOTE: There must be an intersection. This is just a leading segment we have to keep, but it's guaranteed
        // that at least one additional segment will be inserted. This is the main reason there is no `continue` here.
        segmentStartWord = rangeStartSegmentWord;
        BL_ASSERT(segmentStartWord < segmentEndWord);
      }

      // Handle the intersection with the beginning of the range to clear (if any), if it's not at the segment boundary.
      if (!isBitAlignedToSegment(rangeStartBit)) {
        uint32_t denseRangeIndex = rangeStartBit & kSegmentBitMask;
        uint32_t denseRangeCount = blMin(kSegmentBitCount - denseRangeIndex, rangeEndBit - rangeStartBit);

        initDenseSegmentWithOnes(inserter.current(), segmentStartWord);
        bl::BitSetOps::bitArrayClear(inserter.current().data(), denseRangeIndex, denseRangeCount);
        inserter.advance();

        rangeStartWord = segmentStartWord;
        rangeStartBit = bitIndexOf(rangeStartWord);

        // Nothing else to do with this segment if the rest is cleared entirely.
        if (segmentStartWord >= segmentEndWord || rangeLastWord >= segmentEndWord)
          continue;
      }

      // Handle the intersection with the end of the range to clear (if any), if it's not at the segment boundary.
      segmentStartWord = wordIndexOf(alignBitDownToSegment(rangeEndBit));
      if (segmentStartWord >= segmentEndWord)
        continue;

      if (!isBitAlignedToSegment(rangeEndBit) && rangeStartWord <= rangeLastWord) {
        uint32_t denseRangeIndex = 0;
        uint32_t denseRangeCount = rangeEndBit & kSegmentBitMask;

        initDenseSegmentWithOnes(inserter.current(), segmentStartWord);
        bl::BitSetOps::bitArrayClear(inserter.current().data(), denseRangeIndex, denseRangeCount);
        inserter.advance();

        segmentStartWord += kSegmentWordCount;
        rangeStartWord = segmentStartWord;
        rangeStartBit = bitIndexOf(rangeStartWord);

        // Nothing else to do with this segment if the rest is cleared entirely.
        if (segmentStartWord >= segmentEndWord || rangeLastWord >= segmentEndWord)
          continue;
      }

      // Handle a possible trailing segment, which won't be cleared.
      uint32_t trailingWordCount = segmentEndWord - segmentStartWord;
      BL_ASSERT(trailingWordCount >= 1);

      if (trailingWordCount >= kSegmentWordCount * 2) {
        // If the trailing range spans across two or more segments, insert a Range segment.
        initRangeSegment(inserter.current(), segmentStartWord, segmentEndWord);
        inserter.advance();
      }
      else {
        // If the trailing range covers only a single segment, insert a Dense segment.
        initDenseSegmentWithOnes(inserter.current(), segmentStartWord);
        inserter.advance();
      }
    }
    else {
      uint32_t segmentStartBit = rangeStartBit & kSegmentBitMask;
      uint32_t segmentRange = rangeEndBit - rangeStartBit;

      if (rangeLastWord < segment.endWord()) {
        // If this is the only segment to touch, and the BitSet is mutable, do it in place and return
        if (canModify && insertIndex == segmentIndex && inserter.empty()) {
          bl::BitSetOps::bitArrayClear(segmentData[segmentIndex].data(), segmentStartBit, segmentRange);
          return resetCachedCardinality(self);
        }
      }
      else {
        segmentRange = kSegmentBitCount - segmentStartBit;
      }

      inserter.current() = segment;
      bl::BitSetOps::bitArrayClear(inserter.current().data(), segmentStartBit, segmentRange);
      inserter.advance();
    }
  } while (++segmentIndex < segmentCount);

  return spliceInternal(self, segmentData, segmentCount, insertIndex, segmentIndex - insertIndex, inserter.segments(), inserter.count(), canModify);
}

/*
// TODO: Future API (BitSet).

// bl::BitSet - API - Data Manipulation - Combine
// ==============================================

BL_API_IMPL BLResult blBitSetCombine(BLBitSetCore* dst, const BLBitSetCore* a, const BLBitSetCore* b, BLBooleanOp booleanOp) noexcept {
  BL_ASSERT(dst->_d.isBitSet());
  BL_ASSERT(a->_d.isBitSet());
  BL_ASSERT(b->_d.isBitSet());

  return BL_ERROR_NOT_IMPLEMENTED;
}
*/

// bl::BitSet - API - Builder Interface
// ====================================

BL_API_IMPL BLResult blBitSetBuilderCommit(BLBitSetCore* self, BLBitSetBuilderCore* builder, uint32_t newAreaIndex) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  uint32_t areaShift = builder->_areaShift;
  uint32_t wordCount = (1 << areaShift) / bl::BitSetOps::kNumBits;

  if (builder->_areaIndex != BLBitSetBuilderCore::kInvalidAreaIndex) {
    uint32_t startWord = wordIndexOf(builder->_areaIndex << areaShift);
    BL_PROPAGATE(blBitSetAddWords(self, startWord, builder->areaWords(), wordCount));
  }

  builder->_areaIndex = newAreaIndex;
  bl::MemOps::fillInlineT(builder->areaWords(), uint32_t(0), wordCount);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blBitSetBuilderAddRange(BLBitSetCore* self, BLBitSetBuilderCore* builder, uint32_t startBit, uint32_t endBit) noexcept {
  using namespace bl::BitSetInternal;
  BL_ASSERT(self->_d.isBitSet());

  if (startBit >= endBit)
    return BL_SUCCESS;

  uint32_t areaShift = builder->_areaShift;
  uint32_t lastBit = endBit - 1;
  uint32_t areaIndex = startBit >> areaShift;

  // Don't try to add long ranges here.
  if (areaIndex != (lastBit >> areaShift))
    return blBitSetAddRange(self, startBit, endBit);

  if (areaIndex != builder->_areaIndex)
    BL_PROPAGATE(blBitSetBuilderCommit(self, builder, areaIndex));

  uint32_t areaBitIndex = startBit - (areaIndex << areaShift);
  bl::BitSetOps::bitArrayFill(builder->areaWords(), areaBitIndex, endBit - startBit);

  return BL_SUCCESS;
}

// bl::BitSet - Runtime Registration
// =================================

void blBitSetRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);
  blObjectDefaults[BL_OBJECT_TYPE_BIT_SET]._d.initStatic(BLObjectInfo{BLBitSet::kSSOEmptySignature});
}
