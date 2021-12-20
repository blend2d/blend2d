// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_BITSET_H
#define BLEND2D_BITSET_H

#include "object.h"

//! \addtogroup blend2d_api_globals
//! \{

//! \name BLBitSet - Constants
//! \{

BL_DEFINE_ENUM(BLBitSetConstants) {
  //! Invalid bit-index.
  //!
  //! This is the only index that cannot be stored in `BLBitSet`.
  BL_BIT_SET_INVALID_INDEX = 0xFFFFFFFFu,

  //! Range mask used by `BLBitsetSegment::start` value - if set the segment is a range of all ones.
  BL_BIT_SET_RANGE_MASK = 0x80000000u,

  //! Number of words in a BLBitSetSegment.
  BL_BIT_SET_SEGMENT_WORD_COUNT = 4u
};

//! \}

//! \name BLBitSet - C API
//! \{

//! \cond INTERNAL
//! BitSet segment.
//!
//! Segment provides either a dense set of bits starting at `start` or a range of bits all set to one. The start of
//! the segment is always aligned to segment size, which can be calculated as `32 * BL_BIT_SET_SEGMENT_WORD_COUNT`.
//! Even ranges are aligned to this value, thus up to 3 segments are used to describe a range that doesn't start/end
//! at the segment boundary.
//!
//! When the segment describes dense bits its size is always fixed and represents `32 * BL_BIT_SET_SEGMENT_WORD_COUNT`
//! bits, which is currently 128 bits. However, when the segment describes all ones, the first value in data `data[0]`
//! describes the last bit of the range, which means that an arbitrary range can be encoded within a single segment.
struct BLBitSetSegment {
  uint32_t _startWord;
  uint32_t _data[BL_BIT_SET_SEGMENT_WORD_COUNT];

#ifdef __cplusplus
  BL_INLINE bool allOnes() const noexcept { return (_startWord & BL_BIT_SET_RANGE_MASK) != 0; }
  BL_INLINE void clearData() noexcept { memset(_data, 0, sizeof(_data)); }
  BL_INLINE void fillData() noexcept { memset(_data, 0xFF, sizeof(_data)); }

  BL_INLINE uint32_t* data() noexcept { return _data; }
  BL_INLINE const uint32_t* data() const noexcept { return _data; }
  BL_INLINE uint32_t wordAt(size_t index) const noexcept { return _data[index]; }

  BL_INLINE uint32_t _rangeStartWord() const noexcept { return _startWord & ~BL_BIT_SET_RANGE_MASK; }
  BL_INLINE uint32_t _rangeEndWord() const noexcept { return _data[0]; }

  BL_INLINE uint32_t _denseStartWord() const noexcept { return _startWord; }
  BL_INLINE uint32_t _denseEndWord() const noexcept { return _startWord + BL_BIT_SET_SEGMENT_WORD_COUNT; }

  BL_INLINE void _setRangeStartWord(uint32_t index) noexcept { _startWord = index; }
  BL_INLINE void _setRangeEndWord(uint32_t index) noexcept { _data[0] = index; }

  BL_INLINE uint32_t startWord() const noexcept { return _startWord & ~BL_BIT_SET_RANGE_MASK; }
  BL_INLINE uint32_t startSegmentId() const noexcept { return startWord() / BL_BIT_SET_SEGMENT_WORD_COUNT; }
  BL_INLINE uint32_t startBit() const noexcept { return _startWord * 32u; }

  BL_INLINE uint32_t endWord() const noexcept {
    uint32_t rangeEnd = _rangeEndWord();
    uint32_t denseEnd = _denseEndWord();
    return allOnes() ? rangeEnd : denseEnd;
  }

  BL_INLINE uint32_t endSegmentId() const noexcept { return endWord() / BL_BIT_SET_SEGMENT_WORD_COUNT; }
  BL_INLINE uint32_t lastBit() const noexcept { return endWord() * 32u - 1u; }
#endif
};

//! BitSet container [Impl].
struct BLBitSetImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Count of used segments in `segmentData`.
  uint32_t segmentCount;
  //! Count of allocated segments in `segmentData`.
  uint32_t segmentCapacity;

#ifdef __cplusplus
  BL_INLINE BLBitSetSegment* segmentData() const noexcept { return (BLBitSetSegment*)(this + 1); }
  BL_INLINE BLBitSetSegment* segmentDataEnd() const noexcept { return segmentData() + segmentCount; }
#endif
};
//! \endcond

//! BitSet container [C API].
struct BLBitSetCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
};

//! BitSet data view [C API].
struct BLBitSetData {
  const BLBitSetSegment* segmentData;
  uint32_t segmentCount;
  BLBitSetSegment ssoSegments[3];

#ifdef __cplusplus
  BL_INLINE bool empty() const noexcept {
    return segmentCount == 0;
  }

  BL_INLINE void reset() noexcept {
    segmentData = nullptr;
    segmentCount = 0;
  }
#endif
};

//! BitSet builder [C API].
struct BLBitSetBuilderCore {
  //! Shift to get `_areaIndex` from bit index, equals to `log2(kBitCount)`.
  uint32_t _areaShift;
  //! Area index - index from 0...N where each index represents `kBitCount` bits.
  uint32_t _areaIndex;

  /*
  //! Area word data.
  uint32_t areaWords[1 << (areaShift - 5)];
  */

#ifdef __cplusplus
  enum : uint32_t {
    kInvalidAreaIndex = 0xFFFFFFFFu
  };

  BL_INLINE uint32_t* areaWords() noexcept { return reinterpret_cast<uint32_t*>(this + 1); }
  BL_INLINE const uint32_t* areaWords() const noexcept { return reinterpret_cast<const uint32_t*>(this + 1); }
#endif
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blBitSetInit(BLBitSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetInitMove(BLBitSetCore* self, BLBitSetCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetInitWeak(BLBitSetCore* self, const BLBitSetCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetInitRange(BLBitSetCore* self, uint32_t startBit, uint32_t endBit) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetDestroy(BLBitSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetReset(BLBitSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetAssignMove(BLBitSetCore* self, BLBitSetCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetAssignWeak(BLBitSetCore* self, const BLBitSetCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetAssignDeep(BLBitSetCore* self, const BLBitSetCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetAssignRange(BLBitSetCore* self, uint32_t startBit, uint32_t endBit) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetAssignWords(BLBitSetCore* self, uint32_t startWord, const uint32_t* wordData, uint32_t wordCount) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitSetIsEmpty(const BLBitSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetGetData(const BLBitSetCore* self, BLBitSetData* out) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blBitSetGetSegmentCount(const BLBitSetCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API uint32_t BL_CDECL blBitSetGetSegmentCapacity(const BLBitSetCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API uint32_t BL_CDECL blBitSetGetCardinality(const BLBitSetCore* self) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blBitSetGetCardinalityInRange(const BLBitSetCore* self, uint32_t startBit, uint32_t endBit) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitSetHasBit(const BLBitSetCore* self, uint32_t bitIndex) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitSetHasBitsInRange(const BLBitSetCore* self, uint32_t startBit, uint32_t endBit) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitSetSubsumes(const BLBitSetCore* a, const BLBitSetCore* b) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitSetIntersects(const BLBitSetCore* a, const BLBitSetCore* b) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitSetGetRange(const BLBitSetCore* self, uint32_t* startOut, uint32_t* endOut) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitSetEquals(const BLBitSetCore* a, const BLBitSetCore* b) BL_NOEXCEPT_C;
BL_API int BL_CDECL blBitSetCompare(const BLBitSetCore* a, const BLBitSetCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetClear(BLBitSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetShrink(BLBitSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetOptimize(BLBitSetCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetChop(BLBitSetCore* self, uint32_t startBit, uint32_t endBit) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetAddBit(BLBitSetCore* self, uint32_t bitIndex) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetAddRange(BLBitSetCore* self, uint32_t rangeStartBit, uint32_t rangeEndBit) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetAddWords(BLBitSetCore* self, uint32_t startWord, const uint32_t* wordData, uint32_t wordCount) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetClearBit(BLBitSetCore* self, uint32_t bitIndex) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetClearRange(BLBitSetCore* self, uint32_t rangeStartBit, uint32_t rangeEndBit) BL_NOEXCEPT_C;

// TODO: Future API (BitSet).
/*
BL_API BLResult BL_CDECL blBitSetCombine(BLBitSetCore* dst, const BLBitSetCore* a, const BLBitSetCore* b, BLBooleanOp booleanOp) BL_NOEXCEPT_C;
*/

BL_API BLResult BL_CDECL blBitSetBuilderCommit(BLBitSetCore* self, BLBitSetBuilderCore* builder, uint32_t newAreaIndex) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitSetBuilderAddRange(BLBitSetCore* self, BLBitSetBuilderCore* builder, uint32_t startBit, uint32_t endBit) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}

//! \name BLBitSet - C++ API
//! \{
#ifdef __cplusplus

//! BitSet container [C++ API].
//!
//! BitSet container implements sparse BitSet that consists of segments, where each segment represents either dense
//! range of bits or a range of bits that are all set to one. In addition, the BitSet provides also a SSO mode, in
//! which it's possible to store 96 dense bits (3 consecutive BitWords) in the whole addresable range or a range of
//! ones. SSO mode optimizes use cases, in which very small BitSets are used (typically in OpenType pipeline).
//!
//! The BitSet itself has been optimized for Blend2D use cases, which are the following:
//!
//!   1. Representing character coverage of fonts and unicode text. This use-case requires sparseness and ranges as
//!      some fonts, especially those designed for Chinese/Japan languages, provide thousands of glyphs that have
//!      pretty high code points - using a simple BitVector would be pretty wasteful in this case.
//!
//!   2. Storing OpenType processing instructions, where each bit represents one operation in the OpenType pipeline.
//!      SSO BitSet will be most likely enough to describe this as most fonts have less than 96 GPOS/GSUB instructions.
//!
class BLBitSet : public BLBitSetCore {
public:
  //! \cond INTERNAL
  //! \name Constants
  //! \{

  enum : uint32_t {
    //! Number of words that can be used by SSO dense representation.
    kSSOWordCount = 3,
    //! Mask in BLObjectInfo that represents a word index of a dense SSO BitSet (27 bits).
    kSSOIndexMask = 0xFFFFFFFFu / 32u,
    //! Word index that describes a SSO range instead of SSE dense data, only used by BitSet in SSO mode.
    kSSORangeIndex = 0xFFFFFFFFu >> 5
  };

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLBitSet() noexcept {
    // Inlined call to `blBitSetInit()`.
    _d.initStatic(BL_OBJECT_TYPE_BIT_SET, BLObjectInfo{kSSORangeIndex});
  }

  BL_INLINE BLBitSet(BLBitSet&& other) noexcept {
    // Inlined call to `blBitSetInitMove()`.
    _d = other._d;
    other._d.initStatic(BL_OBJECT_TYPE_BIT_SET, BLObjectInfo{kSSORangeIndex});
  }

  BL_INLINE BLBitSet(const BLBitSet& other) noexcept {
    blBitSetInitWeak(this, &other);
  }

  BL_INLINE BLBitSet(uint32_t startBit, uint32_t endBit) noexcept {
    // Inlined call to `blBitSetInitRange()`.
    _d.initStatic(BL_OBJECT_TYPE_BIT_SET, BLObjectInfo{kSSORangeIndex});

    // Stores an empty range [0, 0) if start/end bits are invalid.
    uint32_t mask = uint32_t(-int32_t(startBit < endBit));
    _d.u32_data[0] = startBit & mask;
    _d.u32_data[1] = endBit & mask;
  }

  //! Destroys the BitSet.
  BL_INLINE ~BLBitSet() noexcept { blBitSetDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the BitSet has any content.
  //!
  //! \note This is essentially the opposite of `empty()`.
  BL_INLINE explicit operator bool() const noexcept { return !empty(); }

  //! Move assignment.
  //!
  //! \note The `other` BitSet is reset by move assignment, so its state after the move operation is the same as
  //! a default constructed BitSet.
  BL_INLINE BLBitSet& operator=(BLBitSet&& other) noexcept { blBitSetAssignMove(this, &other); return *this; }
  //! Copy assignment, performs weak copy of the data held by the `other` BitSet.
  BL_INLINE BLBitSet& operator=(const BLBitSet& other) noexcept { blBitSetAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLBitSet& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLBitSet& other) const noexcept { return !equals(other); }
  BL_INLINE bool operator<(const BLBitSet& other) const noexcept { return compare(other) < 0; }
  BL_INLINE bool operator<=(const BLBitSet& other) const noexcept { return compare(other) <= 0; }
  BL_INLINE bool operator>(const BLBitSet& other) const noexcept { return compare(other) > 0; }
  BL_INLINE bool operator>=(const BLBitSet& other) const noexcept { return compare(other) >= 0; }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Clears the content of the BitSet and releases its data.
  //!
  //! After reset the BitSet content matches a default constructed instance.
  BL_INLINE BLResult reset() noexcept { return blBitSetReset(this); }

  //! Swaps the content of this string with the `other` string.
  BL_INLINE void swap(BLBitSetCore& other) noexcept { _d.swap(other._d); }

  //! \name Accessors
  //! \{

  //! Tests whether the BitSet is empty (has no content).
  //!
  //! Returns `true` if the BitSet's size is zero.
  BL_INLINE bool empty() const noexcept { return blBitSetIsEmpty(this); }

  //! Returns the number of segments this BitSet occupies.
  //!
  //! \note If the BitSet is in SSO mode then the returned value is the number of segments the BitSet would occupy
  //! when the BitSet was converted to dynamic.
  BL_INLINE uint32_t segmentCount() const noexcept { return blBitSetGetSegmentCount(this); }

  //! Returns the number of segments this BitSet has allocated.
  //!
  //! \note If the BitSet is in SSO mode the returned value is always zero.
  BL_INLINE uint32_t segmentCapacity() const noexcept { return _d.sso() ? 0 : static_cast<BLBitSetImpl*>(_d.impl)->segmentCapacity; }

  //! Returns the range of the BitSet as `[startOut, endOut)`.
  //!
  //! Returns true if the query was successful, false if the BitSet is empty.
  BL_INLINE bool getRange(uint32_t* startOut, uint32_t* endOut) const noexcept { return blBitSetGetRange(this, startOut, endOut); }

  //! Returns the number of bits set in the BitSet.
  BL_INLINE uint32_t cardinality() const noexcept { return blBitSetGetCardinality(this); }

  //! Returns the number of bits set in the given `[startBit, endBit)` range.
  BL_INLINE uint32_t cardinalityInRange(uint32_t startBit, uint32_t endBit) const noexcept { return blBitSetGetCardinalityInRange(this, startBit, endBit); }

  //! Stores a normalized BitSet data represented as segments into `out`.
  //!
  //! If the BitSet is in SSO mode, it will be converter to temporary segments provided by `BLBitSetData::ssoSegments`,
  //! if the BitSet is in dynamic mode (already contains segments) then only a pointer to the data will be stored into
  //! `out`.
  //!
  //! \remarks The data written into `out` can reference the data in the BitSet, thus the BitSet cannot be manipulated
  //! during the use of the data. This function is ideal for inspecting the content of the BitSet in a unique way and
  //! for implementing iterators that don't have to be aware of how SSO data is represented and used.
  BL_INLINE BLResult getData(BLBitSetData* out) const noexcept { return blBitSetGetData(this, out); }

  //! \}

  //! \name Test Operations
  //! \{

  //! Returns a bit-value at the given `bitIndex`.
  BL_INLINE bool hasBit(uint32_t bitIndex) const noexcept { return blBitSetHasBit(this, bitIndex); }
  //! Returns whether the bit-set has at least on bit in the given range `[start:end)`.
  BL_INLINE bool hasBitsInRange(uint32_t startBit, uint32_t endBit) const noexcept { return blBitSetHasBitsInRange(this, startBit, endBit); }

  //! Returns whether this BitSet subsumes `other`.
  BL_INLINE bool subsumes(const BLBitSetCore& other) const noexcept { return blBitSetSubsumes(this, &other); }
  //! Returns whether this BitSet intersects with `other`.
  BL_INLINE bool intersects(const BLBitSetCore& other) const noexcept { return blBitSetIntersects(this, &other); }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Returns whether this BitSet and `other` are bitwise equal.
  BL_INLINE bool equals(const BLBitSetCore& other) const noexcept { return blBitSetEquals(this, &other); }
  //! Compares this BitSet with `other` and returns either `-1`, `0`, or `1`.
  BL_INLINE int compare(const BLBitSetCore& other) const noexcept { return blBitSetCompare(this, &other); }

  //! \}

  //! \name Content Manipulation
  //! \{

  //! Move assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE BLResult assign(BLBitSetCore&& other) noexcept { return blBitSetAssignMove(this, &other); }

  //! Copy assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE BLResult assign(const BLBitSetCore& other) noexcept { return blBitSetAssignWeak(this, &other); }

  //! Copy assignment, but creates a deep copy of the `other` BitSet instead of weak copy.
  BL_INLINE BLResult assignDeep(const BLBitSetCore& other) noexcept { return blBitSetAssignDeep(this, &other); }

  //! Replaces the content of the BitSet by the given range.
  BL_INLINE BLResult assignRange(uint32_t startBit, uint32_t endBit) noexcept { return blBitSetAssignRange(this, startBit, endBit); }

  //! Replaces the content of the BitSet by bits specified by `wordData` of size `wordCount` [the size is in uint32_t units].
  BL_INLINE BLResult assignWords(uint32_t startWord, const uint32_t* wordData, uint32_t wordCount) noexcept { return blBitSetAssignWords(this, startWord, wordData, wordCount); }

  //! Clears the content of the BitSet without releasing its dynamically allocated data, if possible.
  BL_INLINE BLResult clear() noexcept { return blBitSetClear(this); }

  //! Shrinks the capacity of the BitSet to match the actual content.
  BL_INLINE BLResult shrink() noexcept { return blBitSetShrink(this); }

  //! Optimizes the BitSet by clearing unused pages and by merging continuous pages, without reallocating the BitSet.
  //! This functions should always return `BL_SUCCESS`.
  BL_INLINE BLResult optimize() noexcept { return blBitSetOptimize(this); }

  //! Bounds the BitSet to the given interval `[start:end)`.
  BL_INLINE BLResult chop(uint32_t startBit, uint32_t endBit) noexcept { return blBitSetChop(this, startBit, endBit); }
  //! Truncates the BitSet so it's maximum bit set is less than `n`.
  BL_INLINE BLResult truncate(uint32_t n) noexcept { return blBitSetChop(this, 0, n); }

  //! Adds a bit to the BitSet at the given `index`.
  BL_INLINE BLResult addBit(uint32_t bitIndex) noexcept { return blBitSetAddBit(this, bitIndex); }
  //! Adds a range of bits `[rangeStartBit:rangeEndBit)` to the BitSet.
  BL_INLINE BLResult addRange(uint32_t rangeStartBit, uint32_t rangeEndBit) noexcept { return blBitSetAddRange(this, rangeStartBit, rangeEndBit); }
  //! Adds a dense data to the BitSet starting a bit index `start`.
  BL_INLINE BLResult addWords(uint32_t startWord, const uint32_t* wordData, uint32_t wordCount) noexcept { return blBitSetAddWords(this, startWord, wordData, wordCount); }

  //! Clears a bit in the BitSet at the given `index`.
  BL_INLINE BLResult clearBit(uint32_t bitIndex) noexcept { return blBitSetClearBit(this, bitIndex); }
  //! Clears a range of bits `[rangeStartBit:rangeEndBit)` in the BitSet.
  BL_INLINE BLResult clearRange(uint32_t rangeStartBit, uint32_t rangeEndBit) noexcept { return blBitSetClearRange(this, rangeStartBit, rangeEndBit); }

  /*
  // TODO: Future API (BitSet).

  BL_INLINE BLResult and_(const BLBitSetCore& other) noexcept { return blBitSetCombine(this, this, &other, BL_BOOLEAN_OP_AND); }
  BL_INLINE BLResult or_(const BLBitSetCore& other) noexcept { return blBitSetCombine(this, this, &other, BL_BOOLEAN_OP_OR); }
  BL_INLINE BLResult xor_(const BLBitSetCore& other) noexcept { return blBitSetCombine(this, this, &other, BL_BOOLEAN_OP_XOR); }
  BL_INLINE BLResult andNot(const BLBitSetCore& other) noexcept { return blBitSetCombine(this, this, &other, BL_BOOLEAN_OP_AND_NOT); }
  BL_INLINE BLResult notAnd(const BLBitSetCore& other) noexcept { return blBitSetCombine(this, this, &other, BL_BOOLEAN_OP_NOT_AND); }
  BL_INLINE BLResult combine(const BLBitSetCore& other, BLBooleanOp booleanOp) noexcept { return blBitSetCombine(this, this, &other, booleanOp); }

  static BL_INLINE BLResult and_(BLBitSetCore& dst, const BLBitSetCore& a, const BLBitSetCore& b) noexcept { return blBitSetCombine(&dst, &a, &b, BL_BOOLEAN_OP_AND); }
  static BL_INLINE BLResult or_(BLBitSetCore& dst, const BLBitSetCore& a, const BLBitSetCore& b) noexcept { return blBitSetCombine(&dst, &a, &b, BL_BOOLEAN_OP_OR); }
  static BL_INLINE BLResult xor_(BLBitSetCore& dst, const BLBitSetCore& a, const BLBitSetCore& b) noexcept { return blBitSetCombine(&dst, &a, &b, BL_BOOLEAN_OP_XOR); }
  static BL_INLINE BLResult andNot(BLBitSetCore& dst, const BLBitSetCore& a, const BLBitSetCore& b) noexcept { return blBitSetCombine(&dst, &a, &b, BL_BOOLEAN_OP_AND_NOT); }
  static BL_INLINE BLResult notAnd(BLBitSetCore& dst, const BLBitSetCore& a, const BLBitSetCore& b) noexcept { return blBitSetCombine(&dst, &a, &b, BL_BOOLEAN_OP_NOT_AND); }
  static BL_INLINE BLResult combine(BLBitSetCore& dst, const BLBitSetCore& a, const BLBitSetCore& b, BLBooleanOp booleanOp) noexcept { return blBitSetCombine(&dst, &a, &b, booleanOp); }
  */

  //! \}
};

//! BitSet builder [C++ API].
//!
//! BitSet builder is a low-level utility class that can be used to efficiently build a BitSet in C++. It maintains
//! a configurable buffer (called area) where intermediate bits are set, which are then committed to BitSet when
//! an added bit/range is outside of the area or when user is done with BitSet building. The commit uses \ref
//! blBitSetBuilderCommit() function, which was specifically designed for `BLBitSetBuilderT<BitCount>` in addition
//! to the `BLBitSetBuilder` alias.
//!
//! \note The destructor doesn't do anything. If there are still bits to be committed, they will be lost.
template<uint32_t BitCount>
class BLBitSetBuilderT : public BLBitSetBuilderCore {
public:
  static_assert(BitCount >= 128, "BitCount must be at least 128");
  static_assert((BitCount & (BitCount - 1)) == 0, "BitCount must be a power of 2");

  //! \name Constants
  //! \{

  enum : uint32_t {
    kAreaBitCount = BitCount,
    kAreaWordCount = kAreaBitCount / uint32_t(sizeof(uint32_t) * 8u),
    kAreaShift = BLInternal::ConstCTZ<kAreaBitCount>::kValue
  };

  //! \}

  //! \name Members
  //! \{

  //! Area words data.
  uint32_t _areaWords[kAreaWordCount];

  //! BitSet we are building.
  //!
  //! \note This member is not part of C API. C API requires both BLBitSetCore and BLBitSetBuilderCore to be passed.
  BLBitSetCore* _bitSet;

  //! \}

  //! \name Construction & Destruction
  //! \{

  //! Constructs a new BitSet builder having no BitSet assigned.
  BL_INLINE BLBitSetBuilderT() noexcept {
    _bitSet = nullptr;
    _areaShift = kAreaShift;
    _areaIndex = kInvalidAreaIndex;
  }

  //! Constructs a new BitSet builder having the given `bitSet` assigned.
  //!
  //! \note The builder only stores a pointer to the `bitSet` - the user must guarantee to not destroy the BitSet
  //! before the builder is destroyed or reset.
  BL_INLINE explicit BLBitSetBuilderT(BLBitSetCore* bitSet) noexcept {
    _bitSet = bitSet;
    _areaShift = kAreaShift;
    _areaIndex = kInvalidAreaIndex;
  }

  BL_INLINE BLBitSetBuilderT(const BLBitSetBuilderT&) = delete;
  BL_INLINE BLBitSetBuilderT& operator=(const BLBitSetBuilderT&) = delete;

  //! \}

  //! \name Accessors
  //! \{

  //! Returns whether the BitSet builder is valid, which means that is has an associated `BLBitSet` instance.
  BL_INLINE bool isValid() const noexcept { return _bitSet != nullptr; }
  //! Returns an associated `BLBitSet` instance that this builder commits to.
  BL_INLINE BLBitSet* bitSet() const noexcept { return static_cast<BLBitSet*>(_bitSet); }

  //! \}

  //! \name Builder Interface
  //! \{

  BL_INLINE BLResult reset() noexcept {
    _bitSet = nullptr;
    _areaShift = kAreaShift;
    _areaIndex = kInvalidAreaIndex;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult reset(BLBitSetCore* bitSet) noexcept {
    _bitSet = bitSet;
    _areaShift = kAreaShift;
    _areaIndex = kInvalidAreaIndex;
    return BL_SUCCESS;
  }

  //! Adds a bit to the area maintained by BitSet builder.
  //!
  //! If the area of `bitIndex` is different compared to the current active area, the current area will be
  //! committed to the BitSet. This is actually the only operation that can return \ref BL_ERROR_OUT_OF_MEMORY.
  BL_INLINE BLResult addBit(uint32_t bitIndex) noexcept {
    uint32_t areaIndex = bitIndex / kAreaBitCount;
    if (_areaIndex != areaIndex)
      BL_PROPAGATE(blBitSetBuilderCommit(_bitSet, this, areaIndex));

    bitIndex &= kAreaBitCount - 1;
    _areaWords[bitIndex / 32u] |= uint32_t(0x80000000u) >> (bitIndex % 32u);
    return BL_SUCCESS;
  }

  //! Adds a `[startBit, endBit)` range of bits to the BitSet.
  //!
  //! If the range is relatively small and fits into a single builder area, it will be added to that area.
  //! On the other hand, if the range is large, the area will be kept and the builder would call \ref
  //! BLBitSet::addRange() instead. If the are of the range is different compared to the current active area,
  //! the data in the current acive area will be committed.
  BL_INLINE BLResult addRange(uint32_t startBit, uint32_t endBit) noexcept {
    return blBitSetBuilderAddRange(_bitSet, this, startBit, endBit);
  }

  //! Commits changes in the current active area to the BitSet.
  //!
  //! \note This must be called in order to finalize building the BitSet. If this function is not called the
  //! BitSet could have missing bits that are in the current active area.
  BL_INLINE BLResult commit() noexcept {
    return blBitSetBuilderCommit(_bitSet, this, kInvalidAreaIndex);
  }

  //! Similar to \ref commit(), but the additional parameter `newAreaIndex` will be used to set the current
  //! active area.
  BL_INLINE BLResult commit(uint32_t newAreaIndex) noexcept {
    return blBitSetBuilderCommit(_bitSet, this, newAreaIndex);
  }

  //! \}
};

//! BitSet builder [C++ API] that is configured to have a temporary storage of 512 bits.
using BLBitSetBuilder = BLBitSetBuilderT<512>;

//! BitSet word iterator [C++ API].
//!
//! Low-level iterator that sees a BitSet as an array of bit words. It only iterates non-zero words and
//! returns zero at the end of iteration.
//!
//! A simple way of printing all non-zero words of a BitWord:
//!
//! ```
//! BLBitSet set;
//! set.addRange(100, 200);
//!
//! BLBitSetWordIterator it(set);
//! while (uint32_t bits = it.nextWord()) {
//!   printf("{WordIndex: %u, WordData: %08X }\n", it.wordIndex(), bits);
//! }
//! ```
class BLBitSetWordIterator {
public:
  //! \name Members
  //! \{

  const BLBitSetSegment* _segmentPtr;
  const BLBitSetSegment* _segmentEnd;
  BLBitSetData _data;
  uint32_t _wordIndex;

  //! \}

  //! \name Construction & Destruction
  //! \{

  //! Creates a default constructed iterator, not initialized to iterate any BitSet.
  BL_INLINE BLBitSetWordIterator() noexcept { reset(); }
  //! Creates an iterator, that will iterate the given `bitSet`.
  //!
  //! \note The `bitSet` cannot change or be destroyed during iteration.
  BL_INLINE BLBitSetWordIterator(const BLBitSetCore& bitSet) noexcept { reset(bitSet); }
  //! Creates a copy of `other` iterator.
  BL_INLINE BLBitSetWordIterator(const BLBitSetWordIterator& other) noexcept = default;

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLBitSetWordIterator& operator=(const BLBitSetWordIterator& other) noexcept = default;

  //! \}

  //! \name Reset
  //! \{

  //! Resets the iterator (puts it into a default constructed state).
  BL_INLINE void reset() noexcept {
    _segmentPtr = nullptr;
    _segmentEnd = nullptr;
    _data.reset();
    _wordIndex = 0;
  }

  //! Reinitializes the iterator to iterate the given `bitSet`, from the beginning.
  BL_INLINE void reset(const BLBitSetCore& bitSet) noexcept {
    blBitSetGetData(&bitSet, &_data);
    _segmentPtr = _data.segmentData;
    _segmentEnd = _data.segmentData + _data.segmentCount;
    _wordIndex = _segmentPtr != _segmentEnd ? uint32_t(_segmentPtr->startWord() - 1u) : uint32_t(0xFFFFFFFFu);
  }

  //! \}

  //! \name Iterator Interface
  //! \{

  //! Returns the next (or the first, if called the first time) word non-zero word of the BitSet or zero if the
  //! iteration ended.
  //!
  //! Use `wordIndex()` to get the index (in word units) of the word returned.
  BL_INLINE uint32_t nextWord() noexcept {
    if (_segmentPtr == _segmentEnd)
      return 0;

    _wordIndex++;
    for (;;) {
      if (_segmentPtr->allOnes()) {
        if (_wordIndex < _segmentPtr->_rangeEndWord())
          return 0xFFFFFFFFu;
      }
      else {
        uint32_t endWord = _segmentPtr->_denseEndWord();
        while (_wordIndex < endWord) {
          uint32_t bits = _segmentPtr->_data[_wordIndex & (BL_BIT_SET_SEGMENT_WORD_COUNT - 1u)];
          if (bits != 0u)
            return bits;
          _wordIndex++;
        }
      }

      if (++_segmentPtr == _segmentEnd)
        return 0;
      _wordIndex = _segmentPtr->startWord();
    }
  }

  //! Returns the current bit index of a word returned by `nextWord()`.
  BL_INLINE uint32_t bitIndex() const noexcept { return _wordIndex * 32u; }

  //! Returns the current word index of a word returned by `nextWord()`.
  BL_INLINE uint32_t wordIndex() const noexcept { return _wordIndex; }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_BITSET_H
