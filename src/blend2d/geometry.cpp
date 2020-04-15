// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "./api-build_p.h"
#include "./geometry_p.h"

// ============================================================================
// [blSimpleGeometrySize]
// ============================================================================

struct BLGeometrySizeTableGen {
  static constexpr uint8_t value(size_t i) noexcept {
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

static constexpr const BLLookupTable<uint8_t, BL_GEOMETRY_TYPE_SIMPLE_LAST + 1> blSimpleGeometrySize_
  = blLookupTable<uint8_t, BL_GEOMETRY_TYPE_SIMPLE_LAST + 1, BLGeometrySizeTableGen>();

const BLLookupTable<uint8_t, BL_GEOMETRY_TYPE_SIMPLE_LAST + 1> blSimpleGeometrySize = blSimpleGeometrySize_;
