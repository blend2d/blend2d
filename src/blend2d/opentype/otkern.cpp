// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../font_p.h"
#include "../trace_p.h"
#include "../opentype/otface_p.h"
#include "../opentype/otkern_p.h"
#include "../support/algorithm_p.h"
#include "../support/memops_p.h"
#include "../support/ptrops_p.h"

namespace BLOpenType {
namespace KernImpl {

// OpenType::KernImpl - Tracing
// ============================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_KERN)
  #define Trace BLDebugTrace
#else
  #define Trace BLDummyTrace
#endif

// OpenType::KernImpl - Lookup Tables
// ==================================

static const uint8_t minKernSubTableSize[4] = {
  uint8_t(sizeof(KernTable::Format0)),
  uint8_t(sizeof(KernTable::Format1)),
  uint8_t(sizeof(KernTable::Format2) + 6 + 2), // Includes class table and a single kerning value.
  uint8_t(sizeof(KernTable::Format3))
};

// OpenType::KernImpl - Match
// ==========================

struct KernMatch {
  uint32_t combined;
  BL_INLINE KernMatch(uint32_t combined) noexcept : combined(combined) {}
};
static BL_INLINE bool operator==(const KernTable::Pair& a, const KernMatch& b) noexcept { return a.combined() == b.combined; }
static BL_INLINE bool operator<=(const KernTable::Pair& a, const KernMatch& b) noexcept { return a.combined() <= b.combined; }

// OpenType::KernImpl - Utilities
// ==============================

// Used to define a range of unsorted kerning pairs.
struct UnsortedRange {
  BL_INLINE void reset(uint32_t start_, uint32_t end_) noexcept {
    this->start = start_;
    this->end = end_;
  }

  uint32_t start;
  uint32_t end;
};

// Checks whether the pairs in `pairArray` are sorted and can be b-searched. The last `start` arguments
// specifies the start index from which the check should start as this is required by some utilities here.
static size_t checkKernPairs(const KernTable::Pair* pairArray, size_t pairCount, size_t start) noexcept {
  if (start >= pairCount)
    return pairCount;

  size_t i;
  uint32_t prev = pairArray[start].combined();

  for (i = start; i < pairCount; i++) {
    uint32_t pair = pairArray[i].combined();
    // We must use `prev > pair`, because some fonts have kerning pairs duplicated for no reason (the same
    // values repeated). This doesn't violate the binary search requirements so we are okay with it.
    if (BL_UNLIKELY(prev > pair))
      break;
    prev = pair;
  }

  return i;
}

// Finds ranges of sorted pairs that can be used and creates ranges of unsorted pairs that will be merged into a
// single (synthesized) range of pairs. This function is only called if the kerning data in 'kern' is not sorted,
// and thus has to be fixed.
static BLResult fixUnsortedKernPairs(KernCollection& collection, const KernTable::Format0* fmtData, uint32_t dataOffset, uint32_t pairCount, size_t currentIndex, uint32_t groupFlags, Trace trace) noexcept {
  typedef KernTable::Pair Pair;

  enum : uint32_t {
    kMaxGroups    = 8, // Maximum number of sub-ranges of sorted pairs.
    kMinPairCount = 32 // Minimum number of pairs in a sub-range.
  };

  size_t rangeStart = 0;
  size_t unsortedStart = 0;
  size_t threshold = blMax<size_t>((pairCount - rangeStart) / kMaxGroups, kMinPairCount);

  // Small ranges that are unsorted will be copied into a single one and then sorted.
  // Number of ranges must be `kMaxGroups + 1` to consider also a last trailing range.
  UnsortedRange unsortedRanges[kMaxGroups + 1];
  size_t unsortedCount = 0;
  size_t unsortedPairSum = 0;

  BL_PROPAGATE(collection.groups.reserve(collection.groups.size() + kMaxGroups + 1));
  for (;;) {
    size_t rangeLength = (currentIndex - rangeStart);

    if (rangeLength >= threshold) {
      if (rangeStart != unsortedStart) {
        BL_ASSERT(unsortedCount < BL_ARRAY_SIZE(unsortedRanges));

        unsortedRanges[unsortedCount].reset(uint32_t(unsortedStart), uint32_t(rangeStart));
        unsortedPairSum += rangeStart - unsortedStart;
        unsortedCount++;
      }

      unsortedStart = currentIndex;
      uint32_t subOffset = uint32_t(dataOffset + rangeStart * sizeof(Pair));

      // Cannot fail as we reserved enough.
      trace.warn("Adding Sorted Range [%zu:%zu]\n", rangeStart, currentIndex);
      collection.groups.append(KernGroup::makeReferenced(0, groupFlags, subOffset, uint32_t(rangeLength)));
    }

    rangeStart = currentIndex;
    if (currentIndex == pairCount)
      break;

    currentIndex = checkKernPairs(fmtData->pairArray(), pairCount, currentIndex);
  }

  // Trailing unsorted range.
  if (unsortedStart != pairCount) {
    BL_ASSERT(unsortedCount < BL_ARRAY_SIZE(unsortedRanges));

    unsortedRanges[unsortedCount].reset(uint32_t(unsortedStart), uint32_t(rangeStart));
    unsortedPairSum += pairCount - unsortedStart;
    unsortedCount++;
  }

  if (unsortedPairSum) {
    Pair* synthesizedPairs = static_cast<Pair*>(malloc(unsortedPairSum * sizeof(Pair)));
    if (BL_UNLIKELY(!synthesizedPairs))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t synthesizedIndex = 0;
    for (size_t i = 0; i < unsortedCount; i++) {
      UnsortedRange& r = unsortedRanges[i];
      size_t rangeLength = (r.end - r.start);

      trace.warn("Adding Synthesized Range [%zu:%zu]\n", size_t(r.start), size_t(r.end));
      memcpy(synthesizedPairs + synthesizedIndex, fmtData->pairArray() + r.start, rangeLength * sizeof(Pair));

      synthesizedIndex += rangeLength;
    }
    BL_ASSERT(synthesizedIndex == unsortedPairSum);

    BLAlgorithm::quickSort(synthesizedPairs, unsortedPairSum, [](const Pair& a, const Pair& b) noexcept -> int {
      uint32_t aCombined = a.combined();
      uint32_t bCombined = b.combined();
      return aCombined < bCombined ? -1 : aCombined > bCombined ? 1 : 0;
    });

    // Cannot fail as we reserved enough.
    collection.groups.append(KernGroup::makeSynthesized(0, groupFlags, synthesizedPairs, uint32_t(unsortedPairSum)));
  }

  return BL_SUCCESS;
}

static BL_INLINE size_t findKernPair(const KernTable::Pair* pairs, size_t count, uint32_t pair) noexcept {
  return BLAlgorithm::binarySearch(pairs, count, KernMatch(pair));
}

// OpenType::KernImpl - Apply
// ==========================

static constexpr int32_t kKernMaskOverride = 0x0;
static constexpr int32_t kKernMaskMinimum = 0x1;
static constexpr int32_t kKernMaskCombine = -1;

// Calculates the mask required by `combineKernValue()` from coverage `flags`.
static BL_INLINE int32_t maskFromKernGroupFlags(uint32_t flags) noexcept {
  if (flags & KernGroup::kFlagOverride)
    return kKernMaskOverride;
  else if (flags & KernGroup::kFlagMinimum)
    return kKernMaskMinimum;
  else
    return kKernMaskCombine;
}

// There are several options of combining the kerning value with the previous one. The most common is simply adding
// these two together, but there are also minimum and override (aka replace) functions that we handle here.
static BL_INLINE int32_t combineKernValue(int32_t origVal, int32_t newVal, int32_t mask) noexcept {
  if (mask == kKernMaskMinimum)
    return blMin<int32_t>(origVal, newVal); // Handles 'minimum' function.
  else
    return (origVal & mask) + newVal;       // Handles both 'add' and 'override' functions.
}

// Kern SubTable Format 0 - Ordered list of kerning pairs.
static BL_INLINE int32_t applyKernFormat0(const OTFaceImpl* faceI, const void* dataPtr, size_t dataSize, uint32_t* glyphData, BLGlyphPlacement* placementData, size_t count, int32_t mask) noexcept {
  blUnused(faceI);

  // Format0's `dataPtr` is not a pointer to the start of the table, instead it points to kerning pairs that are
  // either references to the original font data or synthesized in case that the data was wrong or not sorted.
  const KernTable::Pair* pairData = static_cast<const KernTable::Pair*>(dataPtr);
  size_t pairCount = dataSize;

  int32_t allCombined = 0;
  uint32_t pair = glyphData[0] << 16;

  for (size_t i = 1; i < count; i++, pair <<= 16) {
    pair |= glyphData[i];

    size_t index = findKernPair(pairData, pairCount, pair);
    if (index == SIZE_MAX)
      continue;

    int32_t value = pairData[index].value();
    int32_t combined = combineKernValue(placementData[i].placement.x, value, mask);

    placementData[i].placement.x = combined;
    allCombined |= combined;
  }

  return allCombined;
}

// Kern SubTable Format 2 - Simple NxM array of kerning values.
static BL_INLINE int32_t applyKernFormat2(const OTFaceImpl* faceI, const void* dataPtr, size_t dataSize, uint32_t* glyphData, BLGlyphPlacement* placementData, size_t count, int32_t mask) noexcept {
  typedef KernTable::Format2 Format2;
  typedef Format2::ClassTable ClassTable;

  const Format2* subTable = BLPtrOps::offset<const Format2>(dataPtr, faceI->kern.headerSize);
  uint32_t leftClassTableOffset = subTable->leftClassTable();
  uint32_t rightClassTableOffset = subTable->rightClassTable();

  if (BL_UNLIKELY(blMax(leftClassTableOffset, rightClassTableOffset) > dataSize - sizeof(ClassTable)))
    return 0;

  const ClassTable* leftClassTable = BLPtrOps::offset<const ClassTable>(dataPtr, leftClassTableOffset);
  const ClassTable* rightClassTable = BLPtrOps::offset<const ClassTable>(dataPtr, rightClassTableOffset);

  uint32_t leftGlyphCount = leftClassTable->glyphCount();
  uint32_t rightGlyphCount = rightClassTable->glyphCount();

  uint32_t leftTableEnd = leftClassTableOffset + 4u + leftGlyphCount * 2u;
  uint32_t rightTableEnd = rightClassTableOffset + 4u + rightGlyphCount * 2u;

  if (BL_UNLIKELY(blMax(leftTableEnd, rightTableEnd) > dataSize))
    return 0;

  uint32_t leftFirstGlyph = leftClassTable->firstGlyph();
  uint32_t rightFirstGlyph = rightClassTable->firstGlyph();

  int32_t allCombined = 0;
  uint32_t leftGlyph = glyphData[0];
  uint32_t rightGlyph = 0;

  for (size_t i = 1; i < count; i++, leftGlyph = rightGlyph) {
    rightGlyph = glyphData[i];

    uint32_t leftIndex  = leftGlyph - leftFirstGlyph;
    uint32_t rightIndex = rightGlyph - rightFirstGlyph;

    if ((leftIndex >= leftGlyphCount) | (rightIndex >= rightGlyphCount))
      continue;

    uint32_t leftClass = leftClassTable->offsetArray()[leftIndex].value();
    uint32_t rightClass = rightClassTable->offsetArray()[rightIndex].value();

    // Cannot overflow as both components are unsigned 16-bit integers.
    uint32_t valueOffset = leftClass + rightClass;
    if (leftClass * rightClass == 0 || valueOffset > dataSize - 2u)
      continue;

    int32_t value = BLPtrOps::offset<const FWord>(dataPtr, valueOffset)->value();
    int32_t combined = combineKernValue(placementData[i].placement.x, value, mask);

    placementData[i].placement.x = combined;
    allCombined |= combined;
  }

  return allCombined;
}

// Kern SubTable Format 3 - Simple NxM array of kerning indexes.
static BL_INLINE int32_t applyKernFormat3(const OTFaceImpl* faceI, const void* dataPtr, size_t dataSize, uint32_t* glyphData, BLGlyphPlacement* placementData, size_t count, int32_t mask) noexcept {
  typedef KernTable::Format3 Format3;

  const Format3* subTable = BLPtrOps::offset<const Format3>(dataPtr, faceI->kern.headerSize);
  uint32_t glyphCount = subTable->glyphCount();
  uint32_t kernValueCount = subTable->kernValueCount();
  uint32_t leftClassCount = subTable->leftClassCount();
  uint32_t rightClassCount = subTable->rightClassCount();

  uint32_t requiredSize = faceI->kern.headerSize + uint32_t(sizeof(Format3)) + kernValueCount * 2u + glyphCount * 2u + leftClassCount * rightClassCount;
  if (BL_UNLIKELY(requiredSize < dataSize))
    return 0;

  const FWord* valueTable = BLPtrOps::offset<const FWord>(subTable, sizeof(Format3));
  const UInt8* classTable = BLPtrOps::offset<const UInt8>(valueTable, kernValueCount * 2u);
  const UInt8* indexTable = classTable + glyphCount * 2u;

  int32_t allCombined = 0;
  uint32_t leftGlyph = glyphData[0];
  uint32_t rightGlyph = 0;

  for (size_t i = 1; i < count; i++, leftGlyph = rightGlyph) {
    rightGlyph = glyphData[i];
    if (blMax(leftGlyph, rightGlyph) >= glyphCount)
      continue;

    uint32_t leftClass = classTable[leftGlyph].value();
    uint32_t rightClass = classTable[glyphCount + rightGlyph].value();

    if ((leftClass >= leftClassCount) | (rightClass >= rightClassCount))
      continue;

    uint32_t valueIndex = indexTable[leftClass * rightClassCount + rightClass].value();
    if (valueIndex >= kernValueCount)
      continue;

    int32_t value = valueTable[valueIndex].value();
    int32_t combined = combineKernValue(placementData[i].placement.x, value, mask);

    placementData[i].placement.x = combined;
    allCombined |= combined;
  }

  return allCombined;
}

// Applies the data calculated by applyKernFormatN.
static BL_INLINE void finishKern(const OTFaceImpl* faceI, uint32_t* glyphData, BLGlyphPlacement* placementData, size_t count) noexcept {
  blUnused(faceI, glyphData);
  for (size_t i = 1; i < count; i++) {
    placementData[i - 1].advance += placementData[i].placement;
    placementData[i].placement.reset();
  }
}

static BLResult BL_CDECL applyKern(const BLFontFaceImpl* faceI_, uint32_t* glyphData, BLGlyphPlacement* placementData, size_t count) noexcept {
  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);
  if (count < 2)
    return BL_SUCCESS;

  const void* basePtr = faceI->kern.table.data;
  const KernCollection& collection = faceI->kern.collection[0];

  const KernGroup* kernGroups = collection.groups.data();
  size_t groupCount = collection.groups.size();

  int32_t allCombined = 0;

  for (size_t groupIndex = 0; groupIndex < groupCount; groupIndex++) {
    const KernGroup& kernGroup = kernGroups[groupIndex];

    const void* dataPtr = kernGroup.calcDataPtr(basePtr);
    size_t dataSize = kernGroup.dataSize;

    uint32_t format = kernGroup.format;
    int32_t mask = maskFromKernGroupFlags(kernGroup.flags);

    switch (format) {
      case 0: allCombined |= applyKernFormat0(faceI, dataPtr, dataSize, glyphData, placementData, count, mask); break;
      case 2: allCombined |= applyKernFormat2(faceI, dataPtr, dataSize, glyphData, placementData, count, mask); break;
      case 3: allCombined |= applyKernFormat3(faceI, dataPtr, dataSize, glyphData, placementData, count, mask); break;
    }
  }

  // Only finish kerning if we actually did something, if no kerning pair was found or all kerning pairs were
  // zero then there is nothing to do.
  if (allCombined)
    finishKern(faceI, glyphData, placementData, count);

  return BL_SUCCESS;
}

// OpenType::KernImpl - Init
// =========================

BLResult init(OTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  typedef KernTable::WinGroupHeader WinGroupHeader;
  typedef KernTable::MacGroupHeader MacGroupHeader;

  BLFontTableT<KernTable> kern;
  if (!fontData->queryTable(faceI->faceInfo.faceIndex, &kern, BL_MAKE_TAG('k', 'e', 'r', 'n')))
    return BL_SUCCESS;

  Trace trace;
  trace.info("BLOpenType::Init 'kern' [Size=%zu]\n", kern.size);
  trace.indent();

  if (BL_UNLIKELY(!blFontTableFitsT<KernTable>(kern))) {
    trace.warn("Table is too small\n");
    faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
    return BL_SUCCESS;
  }

  const uint8_t* dataPtr = kern.data;
  const uint8_t* dataEnd = dataPtr + kern.size;

  // Kern Header
  // -----------

  // Detect the header format. Windows header uses 16-bit field describing the version of the table and only defines
  // version 0. Apple uses a different header format which uses a 32-bit version number (`F16x16`). Luckily we can
  // distinguish between these two easily.
  uint32_t majorVersion = BLMemOps::readU16uBE(dataPtr);

  uint32_t headerType = 0xFFu;
  uint32_t headerSize = 0;
  uint32_t groupCount = 0;

  if (majorVersion == 0) {
    headerType = KernData::kHeaderWindows;
    headerSize = uint32_t(sizeof(WinGroupHeader));
    groupCount = BLMemOps::readU16uBE(dataPtr + 2u);

    trace.info("Version: 0 (WINDOWS)\n");
    trace.info("GroupCount: %u\n", groupCount);

    // Not forbidden by the spec, just ignore the table if true.
    if (!groupCount) {
      trace.warn("No kerning pairs defined\n");
      return BL_SUCCESS;
    }

    dataPtr += 4;
  }
  else if (majorVersion == 1) {
    uint32_t minorVersion = BLMemOps::readU16uBE(dataPtr + 2u);
    trace.info("Version: 1 (MAC)\n");

    if (minorVersion != 0) {
      trace.warn("Invalid minor version (%u)\n", minorVersion);
      faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
      return BL_SUCCESS;
    }

    // Minimum mac header is 8 bytes. We have to check this explicitly as the minimum size of "any" header is 4 bytes,
    // so make sure we won't read beyond.
    if (kern.size < 8u) {
      trace.warn("InvalidSize: %zu\n", size_t(kern.size));
      faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
      return BL_SUCCESS;
    }

    headerType = KernData::kHeaderMac;
    headerSize = uint32_t(sizeof(MacGroupHeader));

    groupCount = BLMemOps::readU32uBE(dataPtr + 4u);
    trace.info("GroupCount: %u\n", groupCount);

    // Not forbidden by the spec, just ignore the table if true.
    if (!groupCount) {
      trace.warn("No kerning pairs defined\n");
      return BL_SUCCESS;
    }

    dataPtr += 8;
  }
  else {
    trace.info("Version: %u (UNKNOWN)\n", majorVersion);

    // No other major version is defined by OpenType. Since KERN table has been superseded by "GPOS" table there will
    // never be any other version.
    trace.fail("Invalid version");
    faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
    return BL_SUCCESS;
  }

  faceI->kern.headerType = uint8_t(headerType);
  faceI->kern.headerSize = uint8_t(headerSize);

  // Kern Groups
  // -----------

  uint32_t groupIndex = 0;
  do {
    size_t remainingSize = (size_t)(dataEnd - dataPtr);
    if (BL_UNLIKELY(remainingSize < headerSize)) {
      trace.warn("No more data for group #%u\n", groupIndex);
      break;
    }

    uint32_t length = 0;
    uint32_t format = 0;
    uint32_t coverage = 0;

    trace.info("Group #%u\n", groupIndex);
    trace.indent();

    if (headerType == KernData::kHeaderWindows) {
      const WinGroupHeader* group = reinterpret_cast<const WinGroupHeader*>(dataPtr);

      format = group->format();
      length = group->length();

      // Some fonts having only one group have an incorrect length set to the same value as the as the whole 'kern'
      // table. Detect it and fix it.
      if (length == kern.size && groupCount == 1) {
        length = uint32_t(remainingSize);
        trace.warn("Group length is same as the table length, fixed to %u\n", length);
      }

      // The last sub-table can have truncated length to 16 bits even when it needs more to represent all kerning
      // pairs. This is not covered by the specification, but it's a common practice.
      if (length != remainingSize && groupIndex == groupCount - 1) {
        trace.warn("Fixing reported length from %u to %zu\n", length, remainingSize);
        length = uint32_t(remainingSize);
      }

      // Not interested in undefined flags.
      coverage = group->coverage() & ~WinGroupHeader::kCoverageReservedBits;
    }
    else {
      const MacGroupHeader* group = reinterpret_cast<const MacGroupHeader*>(dataPtr);

      format = group->format();
      length = group->length();

      // Translate coverate flags from MAC format to Windows format that we prefer.
      uint32_t macCoverage = group->coverage();
      if ((macCoverage & MacGroupHeader::kCoverageVertical   ) == 0) coverage |= WinGroupHeader::kCoverageHorizontal;
      if ((macCoverage & MacGroupHeader::kCoverageCrossStream) != 0) coverage |= WinGroupHeader::kCoverageCrossStream;
    }

    if (length < headerSize) {
      trace.fail("Group length too small [Length=%u RemainingSize=%zu]\n", length, remainingSize);
      faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
      return BL_SUCCESS;
    }

    if (length > remainingSize) {
      trace.fail("Group length exceeds the remaining space [Length=%u RemainingSize=%zu]\n", length, remainingSize);
      faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
      return BL_SUCCESS;
    }

    // Move to the beginning of the content of the group.
    dataPtr += headerSize;

    // It's easier to calculate everything without the header (as its size is variable), so make `length` raw data size
    // that we will store in KernData.
    length -= headerSize;
    remainingSize -= headerSize;

    // Even on 64-bit machine this cannot overflow as a table length in SFNT header is stored as UInt32.
    uint32_t offset = (uint32_t)(size_t)(dataPtr - kern.data);
    uint32_t orientation = (coverage & WinGroupHeader::kCoverageHorizontal) ? BL_ORIENTATION_HORIZONTAL : BL_ORIENTATION_VERTICAL;
    uint32_t groupFlags = coverage & (KernGroup::kFlagMinimum | KernGroup::kFlagCrossStream | KernGroup::kFlagOverride);

    trace.info("Format: %u%s\n", format, format > 3 ? " (UNKNOWN)" : "");
    trace.info("Coverage: %u\n", coverage);
    trace.info("Orientation: %s\n", orientation == BL_ORIENTATION_HORIZONTAL ? "Horizontal" : "Vertical");

    if (format < BL_ARRAY_SIZE(minKernSubTableSize) && length >= minKernSubTableSize[format]) {
      KernCollection& collection = faceI->kern.collection[orientation];
      switch (format) {
        // Kern SubTable Format 0 - Ordered list of kerning pairs.
        case 0: {
          const KernTable::Format0* fmtData = reinterpret_cast<const KernTable::Format0*>(dataPtr);
          uint32_t pairCount = fmtData->pairCount();
          trace.info("PairCount=%zu\n", pairCount);

          if (pairCount == 0)
            break;

          uint32_t pairDataOffset = offset + 8;
          uint32_t pairDataSize = pairCount * uint32_t(sizeof(KernTable::Pair)) + uint32_t(sizeof(KernTable::Format0));

          if (BL_UNLIKELY(pairDataSize > length)) {
            uint32_t fixedPairCount = (length - uint32_t(sizeof(KernTable::Format0))) / 6;
            trace.warn("Fixing the number of pairs from [%u] to [%u] to match the remaining size [%u]\n", pairCount, fixedPairCount, length);

            faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_FIXED_KERN_DATA;
            pairCount = fixedPairCount;
          }

          // Check whether the pairs are sorted.
          const KernTable::Pair* pairData = fmtData->pairArray();
          size_t unsortedIndex = checkKernPairs(pairData, pairCount, 0);

          if (unsortedIndex != pairCount) {
            trace.warn("Pair #%zu violates ordering constraint (kerning pairs are not sorted)\n", unsortedIndex);

            BLResult result = fixUnsortedKernPairs(collection, fmtData, pairDataOffset, pairCount, unsortedIndex, groupFlags, trace);
            if (result != BL_SUCCESS) {
              trace.fail("Cannot allocate data for synthesized kerning pairs\n");
              return result;
            }

            faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_FIXED_KERN_DATA;
            break;
          }
          else {
            BLResult result = collection.groups.append(KernGroup::makeReferenced(0, groupFlags, pairDataOffset, uint32_t(pairCount)));
            if (result != BL_SUCCESS) {
              trace.fail("Cannot allocate data for referenced kerning pairs\n");
              return result;
            }
          }

          break;
        }

        // Kern SubTable Format 2 - Simple NxM array of kerning values.
        case 2: {
          const void* subTable = static_cast<const uint8_t*>(dataPtr) - headerSize;
          size_t subTableSize = length + headerSize;

          const KernTable::Format2* fmtData = reinterpret_cast<const KernTable::Format2*>(dataPtr);
          uint32_t leftClassTableOffset = fmtData->leftClassTable();
          uint32_t rightClassTableOffset = fmtData->rightClassTable();
          uint32_t kerningArrayOffset = fmtData->kerningArray();

          if (leftClassTableOffset > subTableSize - 6u) {
            trace.warn("Invalid offset [%u] of left ClassTable\n", unsigned(leftClassTableOffset));
            break;
          }

          if (rightClassTableOffset > subTableSize - 6u) {
            trace.warn("Invalid offset [%u] of right ClassTable\n", unsigned(rightClassTableOffset));
            break;
          }

          if (kerningArrayOffset > subTableSize - 2u) {
            trace.warn("Invalid offset [%u] of KerningArray\n", unsigned(kerningArrayOffset));
            break;
          }

          const KernTable::Format2::ClassTable* leftClassTable = BLPtrOps::offset<const KernTable::Format2::ClassTable>(subTable, leftClassTableOffset);
          const KernTable::Format2::ClassTable* rightClassTable = BLPtrOps::offset<const KernTable::Format2::ClassTable>(subTable, rightClassTableOffset);

          uint32_t leftGlyphCount = leftClassTable->glyphCount();
          uint32_t rightGlyphCount = rightClassTable->glyphCount();

          uint32_t leftTableSize = leftClassTableOffset + 4u + leftGlyphCount * 2u;
          uint32_t rightTableSize = rightClassTableOffset + 4u + rightGlyphCount * 2u;

          if (leftTableSize > subTableSize) {
            trace.warn("Left ClassTable's GlyphCount [%u] overflows table size by [%zu] bytes\n", unsigned(leftGlyphCount), size_t(leftTableSize - subTableSize));
            break;
          }

          if (rightTableSize > subTableSize) {
            trace.warn("Right ClassTable's GlyphCount [%u] overflows table size by [%zu] bytes\n", unsigned(rightGlyphCount), size_t(rightTableSize - subTableSize));
            break;
          }

          BLResult result = collection.groups.append(KernGroup::makeReferenced(format, groupFlags, offset - headerSize, uint32_t(subTableSize)));
          if (result != BL_SUCCESS) {
            trace.fail("Cannot allocate data for a referenced kerning group of format #%u\n", unsigned(format));
            return result;
          }

          break;
        }

        // Kern SubTable Format 3 - Simple NxM array of kerning indexes.
        case 3: {
          size_t subTableSize = length + headerSize;

          const KernTable::Format3* fmtData = reinterpret_cast<const KernTable::Format3*>(dataPtr);
          uint32_t glyphCount = fmtData->glyphCount();
          uint32_t kernValueCount = fmtData->kernValueCount();
          uint32_t leftClassCount = fmtData->leftClassCount();
          uint32_t rightClassCount = fmtData->rightClassCount();

          uint32_t requiredSize = faceI->kern.headerSize + uint32_t(sizeof(KernTable::Format3)) + kernValueCount * 2u + glyphCount * 2u + leftClassCount * rightClassCount;
          if (BL_UNLIKELY(requiredSize > subTableSize)) {
            trace.warn("Kerning table data overflows the table size by [%zu] bytes\n", size_t(requiredSize - subTableSize));
            break;
          }

          BLResult result = collection.groups.append(KernGroup::makeReferenced(format, groupFlags, offset - headerSize, uint32_t(subTableSize)));
          if (result != BL_SUCCESS) {
            trace.fail("Cannot allocate data for a referenced kerning group of format #%u\n", unsigned(format));
            return result;
          }

          break;
        }

        default:
          faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
          break;
      }
    }
    else {
      trace.warn("Skipping subtable\n");
    }

    trace.deindent();
    dataPtr += length;
  } while (++groupIndex < groupCount);

  if (!faceI->kern.collection[BL_ORIENTATION_HORIZONTAL].empty()) {
    faceI->kern.table = kern;
    faceI->kern.collection[BL_ORIENTATION_HORIZONTAL].groups.shrink();
    faceI->faceInfo.faceFlags |= BL_FONT_FACE_FLAG_HORIZONTAL_KERNING;
    faceI->featureTags.dcast<BLArray<BLTag>>().append(BL_MAKE_TAG('k', 'e', 'r', 'n'));
    faceI->funcs.applyKern = applyKern;
  }

  return BL_SUCCESS;
}

} // {KernImpl}
} // {BLOpenType}
