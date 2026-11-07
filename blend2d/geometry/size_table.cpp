// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#include <blend2d/geometry/size_table_p.h>

namespace bl::Geometry {

// Geometry - Size Table
// =====================

struct GeometryTypeSizeTableGen {
  static consteval uint8_t value(size_t i) noexcept {
    return i == BL_GEOMETRY_TYPE_BOX_I        ? uint8_t(sizeof(BLBoxI))      :
           i == BL_GEOMETRY_TYPE_BOX_D        ? uint8_t(sizeof(BLBox))       :
           i == BL_GEOMETRY_TYPE_RECT_I       ? uint8_t(sizeof(BLRectI))     :
           i == BL_GEOMETRY_TYPE_RECT_D       ? uint8_t(sizeof(BLRect))      :
           i == BL_GEOMETRY_TYPE_CIRCLE_D     ? uint8_t(sizeof(BLCircle))    :
           i == BL_GEOMETRY_TYPE_ELLIPSE_D    ? uint8_t(sizeof(BLEllipse))   :
           i == BL_GEOMETRY_TYPE_ROUND_RECT_D ? uint8_t(sizeof(BLRoundRect)) :
           i == BL_GEOMETRY_TYPE_ARC_D        ? uint8_t(sizeof(BLArc))       :
           i == BL_GEOMETRY_TYPE_CHORD_D      ? uint8_t(sizeof(BLArc))       :
           i == BL_GEOMETRY_TYPE_PIE_D        ? uint8_t(sizeof(BLArc))       :
           i == BL_GEOMETRY_TYPE_LINE_D       ? uint8_t(sizeof(BLLine))      :
           i == BL_GEOMETRY_TYPE_TRIANGLE_D   ? uint8_t(sizeof(BLTriangle))  : 0;
  }
};

static constexpr auto geometry_type_size_table_ = make_lookup_table<uint8_t, BL_GEOMETRY_TYPE_SIMPLE_LAST + 1, GeometryTypeSizeTableGen>();
const LookupTable<uint8_t, BL_GEOMETRY_TYPE_SIMPLE_LAST + 1> geometry_type_size_table = geometry_type_size_table_;

} // {bl::Geometry}
