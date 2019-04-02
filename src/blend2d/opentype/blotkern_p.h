// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_OPENTYPE_BLOTKERN_P_H
#define BLEND2D_OPENTYPE_BLOTKERN_P_H

#include "../blarray_p.h"
#include "../blsupport_p.h"
#include "../opentype/blotcore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_opentype
//! \{

namespace BLOpenType {

// ============================================================================
// [BLOpenType::Table]
// ============================================================================

//! OpenType 'kern' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/kern
//!   - https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6kern.html
struct KernTable {
  enum : uint32_t { kMinSize = 4 };

  struct WinTableHeader {
    UInt16 version;
    UInt16 tableCount;
  };

  struct MacTableHeader {
    F16x16 version;
    UInt32 tableCount;
  };

  struct WinGroupHeader {
    enum Coverage : uint8_t {
      kCoverageHorizontal   = 0x01u,
      kCoverageMinimum      = 0x02u,
      kCoverageCrossStream  = 0x04u,
      kCoverageOverride     = 0x08u,
      kCoverageReservedBits = 0xF0u
    };

    UInt16 version;
    UInt16 length;
    UInt8 format;
    UInt8 coverage;
  };

  struct MacGroupHeader {
    enum Coverage : uint8_t {
      kCoverageVertical     = 0x80u,
      kCoverageCrossStream  = 0x40u,
      kCoverageVariation    = 0x20u,
      kCoverageReservedBits = 0x1Fu
    };

    UInt32 length;
    UInt8 coverage;
    UInt8 format;
    UInt16 tupleIndex;
  };

  struct Pair {
    union {
      UInt32 combined;
      struct {
        UInt16 left;
        UInt16 right;
      };
    };
    Int16 value;
  };

  struct Format0 {
    UInt16 pairCount;
    UInt16 searchRange;
    UInt16 entrySelector;
    UInt16 rangeShift;
    /*
    Pair pairArray[pairCount];
    */

    BL_INLINE const Pair* pairArray() const noexcept { return blOffsetPtr<const Pair>(this, 8); }
  };

  struct Format1 {
    struct StateHeader {
      UInt16 stateSize;
      UInt16 classTable;
      UInt16 stateArray;
      UInt16 entryTable;
    };

    enum ValueBits : uint16_t {
      kValueOffsetMask      = 0x3FFFu,
      kValueNoAdvance       = 0x4000u,
      kValuePush            = 0x8000u
    };

    StateHeader stateHeader;
    UInt16 valueTable;
  };

  struct Format2 {
    struct Table {
      UInt16 firstGlyph;
      UInt16 glyphCount;
      /*
      UInt16 offsetArray[glyphCount];
      */

      BL_INLINE const UInt16* offsetArray() const noexcept { return blOffsetPtr<const UInt16>(this, 4); }
    };

    UInt16 rowWidth;
    UInt16 leftClassTable;
    UInt16 rightClassTable;
    UInt16 kerningArray;
  };

  WinTableHeader header;
};

// ============================================================================
// [BLOpenType::KernPairSet]
// ============================================================================

//! Array of kerning pairs.
struct KernPairSet {
#if BL_TARGET_ARCH_BITS < 64
  uint32_t pairCount : 31;
  uint32_t synthesized : 1;
#else
  uint32_t pairCount;
  uint32_t synthesized;
#endif

  union {
    uintptr_t dataOffset;
    KernTable::Pair* dataPtr;
  };

  BL_INLINE const KernTable::Pair* pairs(const void* base) const noexcept {
    return synthesized ? dataPtr : blOffsetPtr<const KernTable::Pair>(base, dataOffset);
  }

  static BL_INLINE KernPairSet makeLinked(uintptr_t dataOffset, uint32_t pairCount) noexcept {
    return KernPairSet { pairCount, false, { dataOffset } };
  }

  static BL_INLINE KernPairSet makeSynthesized(KernTable::Pair* pairs, uint32_t pairCount) noexcept {
    return KernPairSet { pairCount, true, { (uintptr_t)pairs } };
  }
};

// ============================================================================
// [BLOpenType::KernCollection]
// ============================================================================

class KernCollection {
public:
  BL_NONCOPYABLE(KernCollection)

  enum HeaderType : uint32_t {
    kHeaderNone    = 0,
    kHeaderMac     = 1,
    kHeaderWindows = 2
  };

  // Using the same bits as `KernTable::WinGroupHeader::Coverage`.
  enum Coverage : uint32_t {
    kCoverageHorizontal   = 0x01u,
    kCoverageMinimum      = 0x02u,
    kCoverageCrossStream  = 0x04u,
    kCoverageOverride     = 0x08u
  };

  uint8_t format;
  uint8_t flags;
  uint8_t coverage;
  uint8_t reserved;
  BLArray<KernPairSet> sets;

  BL_INLINE KernCollection() noexcept
    : format(0),
      flags(0),
      coverage(0),
      reserved(0) {}

  BL_INLINE ~KernCollection() noexcept {
    releaseData();
  }

  BL_INLINE bool empty() const noexcept {
    return sets.empty();
  }

  BL_INLINE void reset() noexcept {
    releaseData();
    format = 0;
    flags = 0;
    coverage = 0;
    sets.reset();
  }

  void releaseData() noexcept {
    size_t count = sets.size();
    for (size_t i = 0; i < count; i++) {
      const KernPairSet& set = sets[i];
      if (set.synthesized)
        free(set.dataPtr);
    }
  }
};

// ============================================================================
// [BLOpenType::KernData]
// ============================================================================

//! Kerning data stored in `OTFace` and used to perform kerning.
class KernData {
public:
  BL_NONCOPYABLE(KernData)

  BLFontTable table;
  KernCollection collection[2];

  BL_INLINE KernData() noexcept {}
};

// ============================================================================
// [BLOpenType::KernImpl]
// ============================================================================

namespace KernImpl {
  BLResult init(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept;
}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_BLOTKERN_P_H
