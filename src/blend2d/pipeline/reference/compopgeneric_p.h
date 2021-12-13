// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_REFERENCE_COMPOPGENERIC_P_H_INCLUDED
#define BLEND2D_PIPELINE_REFERENCE_COMPOPGENERIC_P_H_INCLUDED

#include "../../compop_p.h"
#include "../../pipeline/pipedefs_p.h"
#include "../../pipeline/reference/pixelgeneric_p.h"
#include "../../pipeline/reference/fetchgeneric_p.h"
#include "../../pixelops/scalar_p.h"
#include "../../support/memops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_reference
//! \{

namespace BLPipeline {
namespace Reference {

using Pixel::Repeat;

template<typename PixelT>
struct CompOp_SrcCopy_Op {
  typedef PixelT PixelType;

  enum : uint32_t {
    kCompOp = BL_COMP_OP_SRC_COPY,
    kOptimizeOpaque = 1
  };

  static BL_INLINE PixelType op_prgb32_prgb32(PixelType d, PixelType s) noexcept {
    blUnused(d);
    return s;
  }

  static BL_INLINE PixelType op_prgb32_prgb32(PixelType d, PixelType s, uint32_t m) noexcept {
    return (d.unpack() * Repeat{255 - m} + s.unpack() * Repeat{m}).div255().pack();
  }
};

template<typename PixelT>
struct CompOp_SrcOver_Op {
  typedef PixelT PixelType;

  enum : uint32_t {
    kCompOp = BL_COMP_OP_SRC_OVER,
    kOptimizeOpaque = 0
  };

  // Dca' = Sca + Dca.(1 - Sa)
  // Da'  = Sa  + Da .(1 - Sa)
  static BL_INLINE PixelType op_prgb32_prgb32(PixelType d, PixelType s) noexcept {
    return s + (d.unpack() * Repeat{BLPixelOps::Scalar::neg255(s.a())}).div255().pack();
  }

  // Dca' = Sca.m + Dca.(1 - Sa.m)
  // Da'  = Sa .m + Da .(1 - Sa.m)
  static BL_INLINE PixelType op_prgb32_prgb32(PixelType d, PixelType s, uint32_t m) noexcept {
    return op_prgb32_prgb32(d, (s.unpack() * Repeat{m}).div255().pack());
  }
};

template<typename PixelT>
struct CompOp_Plus_Op {
  typedef PixelT PixelType;

  enum : uint32_t {
    kCompOp = BL_COMP_OP_PLUS,
    kOptimizeOpaque = 0
  };

  static BL_INLINE PixelType op_prgb32_prgb32(PixelType d, PixelType s) noexcept {
    return (d.unpack().addus8(s.unpack())).pack();
  }

  static BL_INLINE PixelType op_prgb32_prgb32(PixelType d, PixelType s, uint32_t m) noexcept {
    return d.unpack().addus8((s.unpack() * Repeat{m}).div255()).pack();
  }
};

template<typename OpT, typename PixelT, uint32_t kDstBpp_>
struct CompOp_Base {
  typedef OpT Op;
  typedef PixelT PixelType;

  enum : uint32_t {
    kDstBPP = kDstBpp_,
    kOptimizeOpaque = Op::kOptimizeOpaque
  };
};

template<typename OpT, typename PixelT, typename FetchOp, uint32_t kDstBPP>
struct CompOp_Base_PRGB32 : public CompOp_Base<OpT, PixelT, kDstBPP> {
  typedef CompOp_Base<OpT, PixelT, kDstBPP> Base;

  using Base::kOptimizeOpaque;

  FetchOp fetchOp;

  BL_INLINE CompOp_Base_PRGB32(const void* fetchData) noexcept
    : fetchOp(fetchData) {}

  BL_INLINE void initRectY(uint32_t x, uint32_t y, uint32_t width) noexcept {
    fetchOp.initRectY(x, y, width);
  }

  BL_INLINE void beginRectX(uint32_t x) noexcept {
    fetchOp.beginRectX(x);
  }

  BL_INLINE void initSpanY(uint32_t y) noexcept {
    fetchOp.initSpanY(y);
  }

  BL_INLINE void beginSpanX(uint32_t x) noexcept {
    fetchOp.beginSpanX(x);
  }

  BL_INLINE void advanceSpanX(uint32_t x, uint32_t diff) noexcept {
    fetchOp.advanceSpanX(x, diff);
  }

  BL_INLINE void endSpanX(uint32_t x) noexcept {
    fetchOp.endSpanX(x);
  }

  BL_INLINE void advanceY() noexcept {
    fetchOp.advanceY();
  }

  BL_INLINE uint8_t* compositePixelOpaque(uint8_t* dstPtr) noexcept {
    if (uint32_t(OpT::kCompOp) == BL_COMP_OP_SRC_COPY) {
      BLMemOps::writeU32a(dstPtr, fetchOp.fetch().value());
      return dstPtr + kDstBPP;
    }
    else {
      BLMemOps::writeU32a(dstPtr, OpT::op_prgb32_prgb32(PixelT::fromValue(BLMemOps::readU32a(dstPtr)), fetchOp.fetch()).value());
      return dstPtr + kDstBPP;
    }
  }

  BL_INLINE uint8_t* compositePixelMasked(uint8_t* dstPtr, uint32_t m) noexcept {
    BLMemOps::writeU32a(dstPtr, OpT::op_prgb32_prgb32(PixelT::fromValue(BLMemOps::readU32a(dstPtr)), fetchOp.fetch(), m).value());
    return dstPtr + kDstBPP;
  }

  BL_INLINE uint8_t* compositeCSpanOpaque(uint8_t* dstPtr, size_t w) noexcept {
    size_t i = w;
    do {
      dstPtr = compositePixelOpaque(dstPtr);
    } while (--i);
    return dstPtr;
  }

  BL_INLINE uint8_t* compositeCSpanMasked(uint8_t* dstPtr, size_t w, uint32_t m) noexcept {
    size_t i = w;
    do {
      dstPtr = compositePixelMasked(dstPtr, m);
    } while (--i);
    return dstPtr;
  }

  BL_INLINE uint8_t* compositeCSpan(uint8_t* dstPtr, size_t w, uint32_t m) noexcept {
    if (kOptimizeOpaque && m == 255)
      return compositeCSpanOpaque(dstPtr, w);
    else
      return compositeCSpanMasked(dstPtr, w, m);
  }
};

typedef CompOp_Base_PRGB32<CompOp_SrcCopy_Op<Pixel::P32_A8R8G8B8>, Pixel::P32_A8R8G8B8, FetchSolid<Pixel::P32_A8R8G8B8>, 4> CompOp_SrcCopy_PRGB32_Solid;
typedef CompOp_Base_PRGB32<CompOp_SrcOver_Op<Pixel::P32_A8R8G8B8>, Pixel::P32_A8R8G8B8, FetchSolid<Pixel::P32_A8R8G8B8>, 4> CompOp_SrcOver_PRGB32_Solid;
typedef CompOp_Base_PRGB32<CompOp_Plus_Op<Pixel::P32_A8R8G8B8>, Pixel::P32_A8R8G8B8, FetchSolid<Pixel::P32_A8R8G8B8>, 4> CompOp_Plus_PRGB32_Solid;

typedef CompOp_Base_PRGB32<CompOp_SrcCopy_Op<Pixel::P32_A8R8G8B8>, Pixel::P32_A8R8G8B8, FetchLinearGradient<Pixel::P32_A8R8G8B8, false>, 4> CompOp_SrcCopy_PRGB32_Linear;
typedef CompOp_Base_PRGB32<CompOp_SrcOver_Op<Pixel::P32_A8R8G8B8>, Pixel::P32_A8R8G8B8, FetchLinearGradient<Pixel::P32_A8R8G8B8, false>, 4> CompOp_SrcOver_PRGB32_Linear;
typedef CompOp_Base_PRGB32<CompOp_Plus_Op<Pixel::P32_A8R8G8B8>, Pixel::P32_A8R8G8B8, FetchLinearGradient<Pixel::P32_A8R8G8B8, false>, 4> CompOp_Plus_PRGB32_Linear;

} // {Reference}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_REFERENCE_COMPOPGENERIC_P_H_INCLUDED
