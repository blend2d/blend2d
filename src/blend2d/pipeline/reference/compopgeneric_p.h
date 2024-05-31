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

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_reference
//! \{

namespace bl {
namespace Pipeline {
namespace Reference {
namespace {

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
    return (d.unpack() * Repeat{m ^ 0xFFu} + s.unpack() * Repeat{m}).div255().pack();
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
    return s + (d.unpack() * Repeat{PixelOps::Scalar::neg255(s.a())}).div255().pack();
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

template<typename OpT, typename PixelT, typename FetchOp, uint32_t kDstBPP_>
struct CompOp_Base {
  typedef OpT Op;
  typedef PixelT PixelType;

  enum : uint32_t {
    kDstBPP = kDstBPP_,
    kCompOp = Op::kCompOp,
    kOptimizeOpaque = Op::kOptimizeOpaque
  };

  FetchOp fetchOp;

  static constexpr FormatExt kFormat = PixelTypeToFormat<PixelT>::kFormat;

  BL_INLINE void rectInitFetch(ContextData* ctxData, const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    fetchOp.rectInitFetch(ctxData, fetchData, xPos, yPos, rectWidth);
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    fetchOp.rectStartX(xPos);
  }

  BL_INLINE void spanInitY(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    fetchOp.spanInitY(ctxData, fetchData, yPos);
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    fetchOp.spanStartX(xPos);
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    fetchOp.spanAdvanceX(xPos, xDiff);
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    fetchOp.spanEndX(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    fetchOp.advanceY();
  }

  BL_INLINE uint8_t* compositePixelOpaque(uint8_t* dstPtr) noexcept {
    if (uint32_t(OpT::kCompOp) == BL_COMP_OP_SRC_COPY) {
      PixelIO<PixelT, kFormat>::store(dstPtr, fetchOp.fetch());
      return dstPtr + kDstBPP;
    }
    else {
      PixelIO<PixelT, kFormat>::store(dstPtr, OpT::op_prgb32_prgb32(PixelIO<PixelT, kFormat>::fetch(dstPtr), fetchOp.fetch()));
      return dstPtr + kDstBPP;
    }
  }

  BL_INLINE uint8_t* compositePixelMasked(uint8_t* dstPtr, uint32_t m) noexcept {
    PixelIO<PixelT, kFormat>::store(dstPtr, OpT::op_prgb32_prgb32(PixelIO<PixelT, kFormat>::fetch(dstPtr), fetchOp.fetch(), m));
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

  BL_INLINE uint8_t* compositeVSpanWithGA(uint8_t* BL_RESTRICT dstPtr, const uint8_t* BL_RESTRICT maskPtr, size_t w) noexcept {
    size_t i = w;
    do {
      uint32_t msk = maskPtr[0];
      dstPtr = compositePixelMasked(dstPtr, msk);
      maskPtr++;
    } while (--i);
    return dstPtr;
  }

  BL_INLINE uint8_t* compositeVSpanWithoutGA(uint8_t* BL_RESTRICT dstPtr, const uint8_t* BL_RESTRICT maskPtr, uint32_t globalAlpha, size_t w) noexcept {
    size_t i = w;
    do {
      uint32_t msk = PixelOps::Scalar::udiv255(uint32_t(maskPtr[0]) * globalAlpha);
      maskPtr++;
      dstPtr = compositePixelMasked(dstPtr, msk);
    } while (--i);
    return dstPtr;
  }
};

} // {anonymous}
} // {Reference}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_REFERENCE_COMPOPGENERIC_P_H_INCLUDED
