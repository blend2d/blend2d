// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../blarrayops_p.h"
#include "../blfont_p.h"
#include "../blsupport_p.h"
#include "../bltrace_p.h"
#include "../opentype/blotface_p.h"
#include "../opentype/blotkern_p.h"

namespace BLOpenType {
namespace KernImpl {

// ============================================================================
// [BLOpenType::KernImpl - Tracing]
// ============================================================================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_KERN)
  #define Trace BLDebugTrace
#else
  #define Trace BLDummyTrace
#endif

// ============================================================================
// [BLOpenType::KernImpl - Match]
// ============================================================================

struct KernMatch {
  uint32_t combined;
  BL_INLINE KernMatch(uint32_t combined) noexcept : combined(combined) {}
};
static BL_INLINE bool operator==(const KernTable::Pair& a, const KernMatch& b) noexcept { return a.combined() == b.combined; }
static BL_INLINE bool operator<=(const KernTable::Pair& a, const KernMatch& b) noexcept { return a.combined() <= b.combined; }

// ============================================================================
// [BLOpenType::KernImpl - Utilities]
// ============================================================================

// Used to define a range of unsorted kerning pairs.
struct UnsortedRange {
  BL_INLINE void reset(uint32_t start, uint32_t end) noexcept {
    this->start = start;
    this->end = end;
  }

  uint32_t start;
  uint32_t end;
};

// Checks whether the pairs in `pairArray` are sorted and can be b-searched.
// The last `start` arguments specifies the start index from which the check
// should start as this is required by some utilities here.
static size_t checkKernPairs(const KernTable::Pair* pairArray, size_t pairCount, size_t start) noexcept {
  if (start >= pairCount)
    return pairCount;

  size_t i;
  uint32_t prev = pairArray[start].combined();

  for (i = start; i < pairCount; i++) {
    uint32_t pair = pairArray[i].combined();
    // We must use `prev > pair`, because some fonts have kerning pairs
    // duplicated for no reason (the same values repeated). This doesn't
    // violate the binary search requirements so we are okay with it.
    if (BL_UNLIKELY(prev > pair))
      break;
    prev = pair;
  }

  return i;
}

// Finds ranges of sorted pairs that can be used and creates ranges of unsorted
// pairs that will be merged into a single (synthesized) range of pairs. This
// function is only called if the kerning data in 'kern' is not sorted, and thus
// has to be fixed.
static BLResult fixUnsortedKernPairs(KernCollection& collection, const KernTable::Format0* fmtData, uint32_t dataOffset, uint32_t pairCount, size_t currentIndex, Trace trace) noexcept {
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

  BL_PROPAGATE(collection.sets.reserve(collection.sets.size() + kMaxGroups + 1));

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
      collection.sets.append(KernPairSet::makeLinked(subOffset, uint32_t(rangeLength)));
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

    blQuickSort(synthesizedPairs, unsortedPairSum, [](const Pair& a, const Pair& b) noexcept -> int {
      uint32_t aCombined = a.combined();
      uint32_t bCombined = b.combined();
      return aCombined < bCombined ? -1 : aCombined > bCombined ? 1 : 0;
    });

    // Cannot fail as we reserved enough.
    collection.sets.append(KernPairSet::makeSynthesized(synthesizedPairs, uint32_t(unsortedPairSum)));
  }

  return BL_SUCCESS;
}

static BL_INLINE size_t findKernPair(const KernTable::Pair* pairs, size_t count, uint32_t pair) noexcept {
  return blBinarySearch(pairs, count, KernMatch(pair));
  /*
  for (size_t i = count; i != 0; i >>= 1) {
    const KernTable::Pair* candidate = pairs + (i >> 1);
    uint32_t combined = candidate->combined();

    if (pair < combined)
      continue;

    i--;
    pairs = candidate + 1;
    if (pair > combined)
      continue;

    return candidate;
  }

  return nullptr;
  */
}

// ============================================================================
// [BLOpenType::KernImpl - Format 0]
// ============================================================================

static BLResult BL_CDECL applyKernPairAdjustmentFormat0(const BLFontFaceImpl* faceI_, BLGlyphItem* itemData, BLGlyphPlacement* placementData, size_t count) noexcept {
  const BLOTFaceImpl* faceI = static_cast<const BLOTFaceImpl*>(faceI_);

  if (count < 2)
    return BL_SUCCESS;

  typedef KernTable::Pair Pair;
  const void* basePtr = faceI->kern.table.data;

  const KernCollection& collection = faceI->kern.collection[0];
  const KernPairSet* pairSetArray = collection.sets.data();
  size_t groupCount = collection.sets.size();

  uint32_t pair = uint32_t(itemData[0].glyphId) << 16;
  for (size_t i = 1; i < count; i++, pair <<= 16) {
    pair |= uint32_t(itemData[i].glyphId);

    for (size_t groupIndex = 0; groupIndex < groupCount; groupIndex++) {
      const KernPairSet& set = pairSetArray[groupIndex];
      const Pair* pairs = set.pairs(basePtr);

      size_t index = findKernPair(pairs, set.pairCount, pair);
      if (index == SIZE_MAX)
        continue;

      placementData[i - 1].advance.x += pairs[index].value();
      break;
    }
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLOpenType::KernImpl - Init]
// ============================================================================

BLResult init(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  typedef KernTable::WinGroupHeader WinGroupHeader;
  typedef KernTable::MacGroupHeader MacGroupHeader;

  BLFontTableT<KernTable> kern;
  if (!fontData->queryTable(&kern, BL_MAKE_TAG('k', 'e', 'r', 'n')))
    return BL_SUCCESS;

  Trace trace;
  trace.info("OpenType::Init 'kern' [Size=%zu]\n", kern.size);
  trace.indent();

  if (BL_UNLIKELY(!blFontTableFitsT<KernTable>(kern))) {
    trace.warn("Table is too small\n");
    faceI->diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
    return BL_SUCCESS;
  }

  const uint8_t* dataPtr = kern.data;
  const uint8_t* dataEnd = dataPtr + kern.size;

  // --------------------------------------------------------------------------
  // [Header]
  // --------------------------------------------------------------------------

  // Detect the header format. Windows header uses 16-bit field describing the
  // version of the table and only defines version 0. Apple uses a different
  // header format which uses a 32-bit version number (`F16x16`). Luckily we
  // can distinguish between these two easily.
  uint32_t majorVersion = blMemReadU16uBE(dataPtr);

  uint32_t headerType = KernCollection::kHeaderNone;
  uint32_t headerSize = 0;
  uint32_t groupCount = 0;

  if (majorVersion == 0) {
    headerType = KernCollection::kHeaderWindows;
    headerSize = uint32_t(sizeof(WinGroupHeader));
    groupCount = blMemReadU16uBE(dataPtr + 2u);

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
    uint32_t minorVersion = blMemReadU16uBE(dataPtr + 2u);
    trace.info("Version: 1 (MAC)\n");

    if (minorVersion != 0) {
      trace.warn("Invalid minor version (%u)\n", minorVersion);
      faceI->diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
      return BL_SUCCESS;
    }

    // Minimum mac header is 8 bytes. We have to check this explicitly as the
    // minimum size of "any" header is 4 bytes, so make sure we won't read beyond.
    if (kern.size < 8u) {
      trace.warn("InvalidSize: %zu\n", size_t(kern.size));
      faceI->diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
      return BL_SUCCESS;
    }

    headerType = KernCollection::kHeaderMac;
    headerSize = uint32_t(sizeof(MacGroupHeader));

    groupCount = blMemReadU32uBE(dataPtr + 4u);
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

    // No other major version is defined by OpenType. Since KERN table has
    // been superseded by "GPOS" table there will never be any other version.
    trace.fail("Invalid version");
    faceI->diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
    return BL_SUCCESS;
  }

  // --------------------------------------------------------------------------
  // [Groups]
  // --------------------------------------------------------------------------

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

    if (headerType == KernCollection::kHeaderWindows) {
      const WinGroupHeader* group = reinterpret_cast<const WinGroupHeader*>(dataPtr);

      format = group->format();
      length = group->length();

      // Some fonts having only one group have an incorrect length set to the
      // same value as the as the whole 'kern' table. Detect it and fix it.
      if (length == kern.size && groupCount == 1) {
        length = uint32_t(remainingSize);
        trace.warn("Group length is same as the table length, fixed to %u\n", length);
      }

      // The last sub-table can have truncated length to 16 bits even when it
      // needs more to represent all kerning pairs. This is not covered by the
      // specification, but it's a common practice.
      if (length != remainingSize && groupIndex == groupCount - 1) {
        trace.warn("Fixing reported length from %u to %zu\n", length, remainingSize);
        length = uint32_t(remainingSize);
      }

      // We don't have to translate coverage flags to KernData::Coverage as they are the same.
      coverage = group->coverage() & ~WinGroupHeader::kCoverageReservedBits;
    }
    else {
      const MacGroupHeader* group = reinterpret_cast<const MacGroupHeader*>(dataPtr);

      format = group->format();
      length = group->length();

      uint32_t macCoverage = group->coverage();
      if ((macCoverage & MacGroupHeader::kCoverageVertical   ) == 0) coverage |= KernCollection::kCoverageHorizontal;
      if ((macCoverage & MacGroupHeader::kCoverageCrossStream) != 0) coverage |= KernCollection::kCoverageCrossStream;
    }

    if (length < headerSize) {
      trace.fail("Group length too small [Length=%u RemainingSize=%zu]\n", length, remainingSize);
      faceI->diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
      return BL_SUCCESS;
    }

    if (length > remainingSize) {
      trace.fail("Group length exceeds the remaining space [Length=%u RemainingSize=%zu]\n", length, remainingSize);
      faceI->diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
      return BL_SUCCESS;
    }

    // Move to the beginning of the content of the group.
    dataPtr += headerSize;

    // It's easier to calculate everything without the header (as its size is
    // variable), so make `length` raw data size that we will store in KernData.
    length -= headerSize;
    remainingSize -= headerSize;

    // Even on 64-bit machine this cannot overflow as a table length in SFNT header is stored as UInt32.
    uint32_t offset = (uint32_t)(size_t)(dataPtr - kern.data);
    uint32_t orientation =
      (coverage & KernCollection::kCoverageHorizontal)
        ? BL_TEXT_ORIENTATION_HORIZONTAL
        : BL_TEXT_ORIENTATION_VERTICAL;

    trace.info("Format: %u%s\n", format, format > 3 ? " (UNKNOWN)" : "");
    trace.info("Coverage: %u\n", coverage);
    trace.info("Orientation: %s\n", orientation == BL_TEXT_ORIENTATION_HORIZONTAL ? "Horizontal" : "Vertical");

    KernCollection& collection = faceI->kern.collection[orientation];
    if (collection.empty() || (collection.format == format && collection.coverage == coverage)) {
      switch (format) {
        case 0: {
          if (length < sizeof(KernTable::Format0))
            break;

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

            faceI->diagFlags |= BL_FONT_FACE_DIAG_FIXED_KERN_DATA;
            pairCount = fixedPairCount;
          }

          // Check whether the pairs are sorted.
          const KernTable::Pair* pairData = fmtData->pairArray();
          size_t unsortedIndex = checkKernPairs(pairData, pairCount, 0);

          if (unsortedIndex != pairCount) {
            trace.warn("Pair #%zu violates ordering constraint (kerning pairs are not sorted)\n", unsortedIndex);

            BLResult result = fixUnsortedKernPairs(collection, fmtData, pairDataOffset, pairCount, unsortedIndex, trace);
            if (result != BL_SUCCESS) {
              trace.fail("Cannot allocate data for synthesized pairs\n");
              return result;
            }

            faceI->diagFlags |= BL_FONT_FACE_DIAG_FIXED_KERN_DATA;
          }
          else {
            BLResult result = collection.sets.append(KernPairSet::makeLinked(pairDataOffset, uint32_t(pairCount)));
            if (result != BL_SUCCESS) {
              trace.fail("Cannot allocate data for linked pairs\n");
              return result;
            }
          }

          break;
        }

        default:
          faceI->diagFlags |= BL_FONT_FACE_DIAG_WRONG_KERN_DATA;
          break;
      }

      if (!collection.empty()) {
        collection.format = uint8_t(format);
        collection.coverage = uint8_t(coverage);
      }
    }
    else {
      trace.warn("Skipping subtable\n");
    }

    trace.deindent();
    dataPtr += length;
  } while (++groupIndex < groupCount);

  if (!faceI->kern.collection[BL_TEXT_ORIENTATION_HORIZONTAL].empty()) {
    switch (faceI->kern.collection[BL_TEXT_ORIENTATION_HORIZONTAL].format) {
      case 0:
        faceI->kern.table = kern;
        faceI->faceFlags |= BL_FONT_FACE_FLAG_HORIZONTAL_KERNING;
        faceI->featureTags.append(BL_MAKE_TAG('k', 'e', 'r', 'n'));

        faceI->funcs.applyKern = applyKernPairAdjustmentFormat0;
        break;

      default:
        break;
    }
  }

  return BL_SUCCESS;
}

} // {KernImpl}
} // {BLOpenType}
