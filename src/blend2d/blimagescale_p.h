// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLIMAGESCALE_P_H
#define BLEND2D_BLIMAGESCALE_P_H

#include "./blgeometry.h"
#include "./blimage.h"

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

#endif // BLEND2D_BLIMAGESCALE_P_H
