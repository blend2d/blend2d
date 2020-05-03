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

#ifndef BLEND2D_IMAGESCALE_P_H_INCLUDED
#define BLEND2D_IMAGESCALE_P_H_INCLUDED

#include "./geometry.h"
#include "./image.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLImageScaleContext]
// ============================================================================

//! Low-level image scaling context.
class BLImageScaleContext {
public:
  enum Dir : uint32_t {
    kDirHorz = 0,
    kDirVert = 1
  };

  struct Record {
    uint32_t pos;
    uint32_t count;
  };

  struct Data {
    int dstSize[2];
    int srcSize[2];
    int kernelSize[2];
    int isUnbound[2];

    double scale[2];
    double factor[2];
    double radius[2];

    int32_t* weightList[2];
    Record* recordList[2];
  };

  Data* data;

  BL_INLINE BLImageScaleContext() noexcept
    : data(nullptr) {}

  BL_INLINE ~BLImageScaleContext() noexcept { reset(); }

  BL_INLINE bool isInitialized() const noexcept { return data != nullptr; }

  BL_INLINE int dstWidth() const noexcept { return data->dstSize[kDirHorz]; }
  BL_INLINE int dstHeight() const noexcept { return data->dstSize[kDirVert]; }

  BL_INLINE int srcWidth() const noexcept { return data->srcSize[kDirHorz]; }
  BL_INLINE int srcHeight() const noexcept { return data->srcSize[kDirVert]; }

  BL_HIDDEN BLResult reset() noexcept;
  BL_HIDDEN BLResult create(const BLSizeI& to, const BLSizeI& from, uint32_t filter, const BLImageScaleOptions* options) noexcept;

  BL_HIDDEN BLResult processHorzData(uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride, uint32_t format) const noexcept;
  BL_HIDDEN BLResult processVertData(uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride, uint32_t format) const noexcept;
};

//! \}
//! \endcond

#endif // BLEND2D_IMAGESCALE_P_H_INCLUDED
