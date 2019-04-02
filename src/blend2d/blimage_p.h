// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLIMAGE_P_H
#define BLEND2D_BLIMAGE_P_H

#include "./blapi-internal_p.h"
#include "./blimage.h"
#include "./blsupport_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLImage - Internal]
// ============================================================================

enum : uint32_t { BL_INTERNAL_IMAGE_DATA_ALIGNMENT = 8 };

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

static BL_INLINE size_t blImageStrideForWidth(uint32_t width, uint32_t depth) noexcept {
  return depth <= 8 ? (size_t(width) * depth + 7u) / 8u
                    : blAlignUp<size_t>(size_t(width) * (depth / 8u), (depth <= 32) ? 4 : 8);
}

BL_HIDDEN BLResult blImageImplDelete(BLImageImpl* impl) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_BLIMAGE_P_H
