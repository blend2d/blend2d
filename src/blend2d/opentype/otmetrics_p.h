// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_OPENTYPE_OTMETRICS_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTMETRICS_P_H_INCLUDED

#include "../opentype/otcore_p.h"

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

#endif // BLEND2D_OPENTYPE_OTMETRICS_P_H_INCLUDED
