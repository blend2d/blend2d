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

#ifndef BLEND2D_IMAGE_P_H_INCLUDED
#define BLEND2D_IMAGE_P_H_INCLUDED

#include "./api-internal_p.h"
#include "./image.h"
#include "./support_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLImage - Internal]
// ============================================================================

enum : uint32_t {
  BL_INTERNAL_IMAGE_SMALL_DATA_ALIGNMENT = 8,
  BL_INTERNAL_IMAGE_LARGE_DATA_ALIGNMENT = 64,
  BL_INTERNAL_IMAGE_LARGE_DATA_THRESHOLD = 1024
};

//! Internal implementation that extends `BLImagetImpl`.
struct BLInternalImageImpl : public BLImageImpl {
  //! Count of writers that write to this image.
  //!
  //! Writers don't increase the reference count of the image to keep it
  //! mutable. However, we must keep a counter that would tell the BLImage
  //! destructor that it's not the time if `writerCount > 0`.
  volatile size_t writerCount;
};

template<>
struct BLInternalCastImpl<BLImageImpl> { typedef BLInternalImageImpl Type; };

static BL_INLINE size_t blImageStrideForWidth(uint32_t width, uint32_t depth, BLOverflowFlag* of) noexcept {
  if (depth <= 8)
    return (size_t(width) * depth + 7u) / 8u;

  size_t bytesPerPixel = size_t(depth / 8u);
  return blMulOverflow(size_t(width), bytesPerPixel, of);
}

BL_HIDDEN BLResult blImageImplDelete(BLImageImpl* impl) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_IMAGE_P_H_INCLUDED
