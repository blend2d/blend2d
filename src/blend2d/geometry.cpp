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
