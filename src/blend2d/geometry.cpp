// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "geometry_p.h"

namespace BLGeometry {

// BLGeometry - Tables
// ===================

struct BLGeometryTypeSizeTableGen {
  static BL_INLINE constexpr uint8_t value(size_t i) noexcept {
    return i == BL_GEOMETRY_TYPE_BOXI       ? uint8_t(sizeof(BLBoxI))      :
           i == BL_GEOMETRY_TYPE_BOXD       ? uint8_t(sizeof(BLBox))       :
           i == BL_GEOMETRY_TYPE_RECTI      ? uint8_t(sizeof(BLRectI))     :
           i == BL_GEOMETRY_TYPE_RECTD      ? uint8_t(sizeof(BLRect))      :
           i == BL_GEOMETRY_TYPE_CIRCLE     ? uint8_t(sizeof(BLCircle))    :
           i == BL_GEOMETRY_TYPE_ELLIPSE    ? uint8_t(sizeof(BLEllipse))   :
           i == BL_GEOMETRY_TYPE_ROUND_RECT ? uint8_t(sizeof(BLRoundRect)) :
           i == BL_GEOMETRY_TYPE_ARC        ? uint8_t(sizeof(BLArc))       :
           i == BL_GEOMETRY_TYPE_CHORD      ? uint8_t(sizeof(BLArc))       :
           i == BL_GEOMETRY_TYPE_PIE        ? uint8_t(sizeof(BLArc))       :
           i == BL_GEOMETRY_TYPE_LINE       ? uint8_t(sizeof(BLLine))      :
           i == BL_GEOMETRY_TYPE_TRIANGLE   ? uint8_t(sizeof(BLTriangle))  : 0;
  }
};

static constexpr auto blGeometryTypeSizeTable_ = blMakeLookupTable<uint8_t, BL_GEOMETRY_TYPE_SIMPLE_LAST + 1, BLGeometryTypeSizeTableGen>();
const BLLookupTable<uint8_t, BL_GEOMETRY_TYPE_SIMPLE_LAST + 1> blGeometryTypeSizeTable = blGeometryTypeSizeTable_;

} // {BLGeometry}
