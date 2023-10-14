// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_BITSET_P_H
#define BLEND2D_BITSET_P_H

#include "api-internal_p.h"
#include "bitset.h"
#include "object_p.h"
#include "support/algorithm_p.h"
#include "support/bitops_p.h"
#include "support/intops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! \name BLBitSet - Types
//! \{

typedef ParametrizedBitOps<BitOrder::kMSB, uint32_t> BitSetOps;

//! \}

namespace BitSetInternal {

//! \name BLBitSet - Constants
//! \{

static constexpr uint32_t kInvalidIndex = BL_BIT_SET_INVALID_INDEX;
static constexpr uint32_t kSegmentWordCount = BL_BIT_SET_SEGMENT_WORD_COUNT;

static constexpr uint32_t kSSOWordCount = BLBitSet::kSSOWordCount;
static constexpr uint32_t kLastWord = 0xFFFFFFFFu / BitSetOps::kNumBits;

//! Index of the last large word index of SSE Dense BitSet.
//!
//! We don't want to overflow the maximum addressable word, so large word index cannot exceed this value.
static constexpr uint32_t kSSOLastWord = kLastWord - kSSOWordCount + 1;

static constexpr uint32_t kSegmentBitCount = BL_BIT_SET_SEGMENT_WORD_COUNT * IntOps::bitSizeOf<uint32_t>();
static constexpr uint32_t kSegmentBitMask = kSegmentBitCount - 1;

/*
// TODO: Unused

static constexpr uint32_t kMaxSegmentCount = (kLastWord + 1) / kSegmentWordCount;
static constexpr uint32_t kMaxImplSize = uint32_t(sizeof(BLBitSetImpl) + kMaxSegmentCount * sizeof(BLBitSetSegment));
*/

//! \}

//! \name BLBitSet - Internals - Common Functionality (Impl)
//! \{

static BL_INLINE bool isImplMutable(BLBitSetImpl* impl) noexcept {
  return ObjectInternal::isImplMutable(impl);
}

static BL_INLINE BLResult freeImpl(BLBitSetImpl* impl) noexcept {
  return ObjectInternal::freeImpl(impl);
}

template<RCMode kRCMode>
static BL_INLINE BLResult releaseImpl(BLBitSetImpl* impl) noexcept {
  return ObjectInternal::derefImplAndTest<kRCMode>(impl) ? freeImpl(impl) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLBitSet - Internals - Common Functionality (Instance)
//! \{

static BL_INLINE_NODEBUG BLBitSetImpl* getImpl(const BLBitSetCore* self) noexcept {
  return static_cast<BLBitSetImpl*>(self->_d.impl);
}

static BL_INLINE BLResult retainInstance(const BLBitSetCore* self, size_t n = 1) noexcept {
  return ObjectInternal::retainInstance(self, n);
}

static BL_INLINE BLResult releaseInstance(BLBitSetCore* self) noexcept {
  return self->_d.isRefCountedObject() ? releaseImpl<RCMode::kForce>(getImpl(self)) : BLResult(BL_SUCCESS);
}

static BL_INLINE BLResult replaceInstance(BLBitSetCore* self, const BLBitSetCore* other) noexcept {
  // NOTE: UBSAN doesn't like casting the impl in case the BitSet is in SSO mode, so wait with the cast.
  void* impl = static_cast<void*>(self->_d.impl);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;
  return info.isRefCountedObject() ? releaseImpl<RCMode::kForce>(static_cast<BLBitSetImpl*>(impl)) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLBitSet - Internals - SSO Commons
//! \{

//! SSO BitSet is empty when the first 2 words are zero.
//!
//! This check handles both dense SSO data and SSO ranges.
static BL_INLINE_NODEBUG bool isSSOEmpty(const BLBitSetCore* self) noexcept { return self->_d.u64_data[0] == 0; }

static BL_INLINE_NODEBUG uint32_t getSSORangeStart(const BLBitSetCore* self) noexcept { return self->_d.u32_data[0]; }
static BL_INLINE_NODEBUG uint32_t getSSORangeEnd(const BLBitSetCore* self) noexcept { return self->_d.u32_data[1]; }
static BL_INLINE_NODEBUG uint32_t getSSOWordIndex(const BLBitSetCore* self) noexcept { return self->_d.u32_data[2]; }

//! \}

//! \name BLBitSet - Internals - SSO Range
//! \{

struct Range {
  uint32_t start;
  uint32_t end;

  BL_INLINE_NODEBUG void reset(uint32_t rStart, uint32_t rEnd) noexcept {
    start = rStart;
    end = rEnd;
  }

  BL_INLINE_NODEBUG bool valid() const noexcept { return start < end; }
  BL_INLINE_NODEBUG bool empty() const noexcept { return start >= end; }

  BL_INLINE_NODEBUG bool hasIndex(uint32_t index) const noexcept { return index - start < end - start; }
  BL_INLINE_NODEBUG uint32_t size() const noexcept { return end - start; }

  BL_INLINE_NODEBUG Range intersect(uint32_t rStart, uint32_t rEnd) const noexcept { return Range{blMax(start, rStart), blMin(end, rEnd)}; }
  BL_INLINE_NODEBUG Range intersect(const Range& other) const noexcept { return intersect(other.start, other.end); }

  BL_INLINE void normalize() noexcept {
    uint32_t mask = IntOps::bitMaskFromBool<uint32_t>(valid());
    start &= mask;
    end &= mask;
  }
};

static BL_INLINE Range getSSORange(const BLBitSetCore* self) noexcept {
  return Range{self->_d.u32_data[0], self->_d.u32_data[1]};
}

static BL_INLINE BLResult setSSORangeStart(BLBitSetCore* self, uint32_t value) noexcept {
  self->_d.u32_data[0] = value;
  return BL_SUCCESS;
}

static BL_INLINE BLResult setSSORangeEnd(BLBitSetCore* self, uint32_t value) noexcept {
  self->_d.u32_data[1] = value;
  return BL_SUCCESS;
}

static BL_INLINE BLResult setSSORange(BLBitSetCore* self, uint32_t startBit, uint32_t endBit) noexcept {
  self->_d.u32_data[0] = startBit;
  self->_d.u32_data[1] = endBit;
  return BL_SUCCESS;
}

//! \}

//! \name BLBitSet - Internals - SSO Dense
//! \{

struct SSODenseInfo {
  uint32_t _wordIndex;
  uint32_t _wordCount;

  BL_INLINE_NODEBUG uint32_t startWord() const noexcept { return _wordIndex; }
  BL_INLINE_NODEBUG uint32_t lastWord() const noexcept { return _wordIndex + _wordCount - 1; }
  BL_INLINE_NODEBUG uint32_t endWord() const noexcept { return _wordIndex + _wordCount; }
  BL_INLINE_NODEBUG uint32_t wordCount() const noexcept { return _wordCount; }

  BL_INLINE_NODEBUG uint32_t startBit() const noexcept { return _wordIndex * BitSetOps::kNumBits; }
  BL_INLINE_NODEBUG uint32_t lastBit() const noexcept { return (_wordIndex + _wordCount) * BitSetOps::kNumBits - 1; }

  BL_INLINE_NODEBUG bool hasIndex(uint32_t index) const noexcept { return (index / BitSetOps::kNumBits) - _wordIndex < _wordCount; }
};

static BL_INLINE SSODenseInfo getSSODenseInfo(const BLBitSetCore* self) noexcept {
  return SSODenseInfo{getSSOWordIndex(self), kSSOWordCount};
}

//! \}

} // {BitSetInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_BITSET_P_H
