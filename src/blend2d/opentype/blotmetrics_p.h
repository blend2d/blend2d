// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_OPENTYPE_BLOTMETRICS_P_H
#define BLEND2D_OPENTYPE_BLOTMETRICS_P_H

#include "../opentype/blotcore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_opentype
//! \{

namespace BLOpenType {

// ============================================================================
// [BLOpenType::XHeaTable]
// ============================================================================

//! OpenType 'hhea' and 'vhea' tables.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/hhea
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/vhea
struct XHeaTable {
  enum : uint32_t { kMinSize = 36 };

  F16x16 version;
  Int16 ascender;
  Int16 descender;
  Int16 lineGap;
  UInt16 maxAdvance;
  Int16 minLeadingBearing;
  Int16 minTrailingBearing;
  Int16 maxExtent;
  Int16 caretSlopeRise;
  Int16 caretSlopeRun;
  Int16 caretOffset;
  Int16 reserved[4];
  UInt16 longMetricFormat;
  UInt16 longMetricCount;
};

// ============================================================================
// [BLOpenType::XMtxTable]
// ============================================================================

//! OpenType 'hmtx' and 'vmtx' tables.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/hmtx
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/vmtx
struct XMtxTable {
  // At least one LongMetric.
  enum : uint32_t { kMinSize = 4  };

  struct LongMetric {
    UInt16 advance;
    Int16 lsb;
  };

  /*
  LongMetric lmArray[];
  Int16 lsbArray[];
  */

  //! Paired advance width and left side bearing values, indexed by glyph ID.
  BL_INLINE const LongMetric* lmArray() const noexcept { return blOffsetPtr<const LongMetric>(this, 0); }
  //! Leading side bearings for glyph IDs greater than or equal to `metricCount`.
  BL_INLINE const Int16* lsbArray(size_t metricCount) const noexcept { return blOffsetPtr<const Int16>(this, metricCount * sizeof(LongMetric)); }
};

// ============================================================================
// [BLOpenType::MetricsData]
// ============================================================================

struct MetricsData {
  //! Metrics tables - 'hmtx' and 'vmtx' (if present).
  BLFontTableT<XMtxTable> xmtxTable[2];
  //! Count of LongMetric entries.
  uint16_t longMetricCount[2];
  //! Count of LSB entries.
  uint16_t lsbArraySize[2];
};

// ============================================================================
// [BLOpenType::MetricsImpl]
// ============================================================================

namespace MetricsImpl {
  BLResult init(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept;
}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_BLOTMETRICS_P_H
