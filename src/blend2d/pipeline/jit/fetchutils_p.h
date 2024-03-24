// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHUTILS_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHUTILS_P_H_INCLUDED

#include "../../format.h"
#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/fetchutilspixelgather_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {
namespace FetchUtils {

//! Fetch 4 pixels indexed in XMM reg (32-bit unsigned offsets).
template<typename FetchFuncT>
static void fetch_4x_t(PipeCompiler* pc, const Vec& idx4x, const FetchFuncT& fetchFunc) noexcept {
  IndexExtractor extractor(pc);

  if (pc->is64Bit()) {
    Gp idx0 = pc->newGpPtr("@idx0");
    Gp idx1 = pc->newGpPtr("@idx1");

    extractor.begin(IndexExtractor::kTypeUInt32, idx4x);
    extractor.extract(idx0, 0);
    extractor.extract(idx1, 1);

    fetchFunc(idx0);
    extractor.extract(idx0, 2);

    fetchFunc(idx1);
    extractor.extract(idx1, 3);

    fetchFunc(idx0);
    fetchFunc(idx1);
  }
  else {
    // Use less registers in 32-bit mode otherwise we are risking to spill others.
    Gp idx = pc->newGpPtr("@idx");

    extractor.begin(IndexExtractor::kTypeUInt32, idx4x);
    extractor.extract(idx, 0);
    fetchFunc(idx);

    extractor.extract(idx, 1);
    fetchFunc(idx);

    extractor.extract(idx, 2);
    fetchFunc(idx);

    extractor.extract(idx, 3);
    fetchFunc(idx);
  }
}

static void fetch_4x(
  FetchContext* fcA, const Mem& srcA, const Vec& idx4x, uint32_t shift) noexcept {

  Mem m(srcA);
  m.setShift(shift);

  fetch_4x_t(fcA->_pc, idx4x, [&](const Gp& idx) {
    m.setIndex(idx);
    fcA->fetchPixel(m);
  });
}

static void fetch_4x_twice(
  FetchContext* fcA, const Mem& srcA,
  FetchContext* fcB, const Mem& srcB, const Vec& idx4x, uint32_t shift) noexcept {

  Mem mA(srcA);
  Mem mB(srcB);

  mA.setShift(shift);
  mB.setShift(shift);

  fetch_4x_t(fcA->_pc, idx4x, [&](const Gp& idx) {
    mA.setIndex(idx);
    mB.setIndex(idx);

    fcA->fetchPixel(mA);
    fcB->fetchPixel(mB);
  });
}

// Bilinear interpolation with calculated weights
// ==============================================
//
//   P' = [Px0y0 * (256 - Wx) * (256 - Wy) +
//         Px1y0 * (Wx      ) * (256 - Wy) +
//         Px0y1 * (256 - Wx) * (Wy      ) +
//         Px1y1 * (Wx      ) * (Wy      ) ]
//
//   P' = [Px0y0 * (256 - Wx) + Px1y0 * Wx] * (256 - Wy) +
//        [Px0y1 * (256 - Wx) + Px1y1 * Wx] * Wy
//
//   P' = [Px0y0 * (256 - Wy) + Px0y1 * Wy] * (256 - Wx) +
//        [Px1y0 * (256 - Wy) + Px1y1 * Wy] * Wx

//! Fetch 1xA8 pixel by doing a bilinear interpolation with its neighbors.
//!
//! Weights = {256-wy, wy, 256-wy, wy, 256-wx, wx, 256-wx, wx}
template<typename Pixels, typename Stride>
BL_NOINLINE void xFilterBilinearA8_1x(
  PipeCompiler* pc,
  Vec& out,
  const Pixels& pixels,
  const Stride& stride,
  FormatExt format,
  uint32_t indexShift,
  const Vec& indexes,
  const Vec& weights) noexcept {

  IndexExtractor extractor(pc);

  Gp pixSrcRow0 = pc->newGpPtr("pixSrcRow0");
  Gp pixSrcRow1 = pc->newGpPtr("pixSrcRow1");
  Gp pixSrcOff = pc->newGpPtr("pixSrcOff");
  Gp pixAcc = pc->newGp32("pixAcc");
  Vec wTmp = pc->newV128("wTmp");

  extractor.begin(IndexExtractor::kTypeUInt32, indexes);
  extractor.extract(pixSrcRow0, 2);
  extractor.extract(pixSrcRow1, 3);

  int32_t alphaOffset = 0;
  switch (format) {
    case FormatExt::kPRGB32:
      alphaOffset = 3;
      break;

    case FormatExt::kXRGB32:
      alphaOffset = 3;
      break;

    default:
      break;
  }

#if defined(BL_JIT_ARCH_X86)
  Mem row0m = mem_ptr(pixSrcRow0, pixSrcOff, indexShift, alphaOffset);
  Mem row1m = mem_ptr(pixSrcRow1, pixSrcOff, indexShift, alphaOffset);
#else
  Mem row0m;
  Mem row1m;

  if (alphaOffset != 0) {
    Gp pixSrcRow0a = pc->newSimilarReg(pixSrcRow0, "!row0_alpha");
    Gp pixSrcRow1a = pc->newSimilarReg(pixSrcRow1, "!row1_alpha");

    pc->add(pixSrcRow0a, pixSrcRow0, alphaOffset);
    pc->add(pixSrcRow1a, pixSrcRow1, alphaOffset);

    row0m = mem_ptr(pixSrcRow0a, pixSrcOff, indexShift);
    row1m = mem_ptr(pixSrcRow1a, pixSrcOff, indexShift);
  }
  else {
    row0m = mem_ptr(pixSrcRow0, pixSrcOff, indexShift);
    row1m = mem_ptr(pixSrcRow1, pixSrcOff, indexShift);
  }
#endif

  pc->mul(pixSrcRow0, pixSrcRow0, stride);
  pc->mul(pixSrcRow1, pixSrcRow1, stride);
  pc->add(pixSrcRow0, pixSrcRow0, pixels);
  pc->add(pixSrcRow1, pixSrcRow1, pixels);

  extractor.extract(pixSrcOff, 0);
  pc->load_u8(pixAcc, row0m);       // [0    , 0    , 0    , Px0y0]
  pc->shl(pixAcc, pixAcc, 8);       // [0    , 0    , Px0y0, 0    ]
  pc->load_merge_u8(pixAcc, row1m); // [0    , 0    , Px0y0, Px0y1]
  pc->shl(pixAcc, pixAcc, 8);       // [0    , Px0y0, Px0y1, 0    ]

  extractor.extract(pixSrcOff, 1);
  pc->load_merge_u8(pixAcc, row0m); // [0    , Px0y0, Px0y1, Px1y0]
  pc->shl(pixAcc, pixAcc, 8);       // [Px0y0, Px0y1, Px1y0, 0    ]
  pc->load_merge_u8(pixAcc, row1m); // [Px0y0, Px0y1, Px1y0, Px1y1]

  pc->s_mov_u32(out, pixAcc);
  pc->v_swizzle_u32x4(wTmp, weights, swizzle(3, 3, 2, 2));

  pc->v_cvt_u8_lo_to_u16(out, out);
  pc->v_mhadd_i16_to_i32(out, out, wTmp);
  pc->v_swizzle_lo_u16x4(wTmp, weights, swizzle(1, 1, 0, 0));
  pc->v_mulh_u16(out, out, wTmp);
  pc->v_swizzle_u32x4(wTmp, out, swizzle(3, 2, 0, 1));
  pc->v_add_i32(out, out, wTmp);
}

//! Fetch 1xPRGB pixel by doing a bilinear interpolation with its neighbors.
//!
//! Weights = {256-wy, 256-wy, wy, wy, 256-wx, 256-wx, wx, wx}
template<typename Pixels, typename Stride>
BL_NOINLINE void xFilterBilinearARGB32_1x(
  PipeCompiler* pc,
  Vec& out,
  const Pixels& pixels,
  const Stride& stride,
  const Vec& indexes,
  const Vec& weights) noexcept {

  IndexExtractor extractor(pc);

  Gp pixSrcRow0 = pc->newGpPtr("pixSrcRow0");
  Gp pixSrcRow1 = pc->newGpPtr("pixSrcRow1");
  Gp pixSrcOff = pc->newGpPtr("pixSrcOff");

  Vec pixTop = pc->newV128("pixTop");
  Vec pixBot = pc->newV128("pixBot");

  Vec pixTmp0 = out;
  Vec pixTmp1 = pc->newV128("pixTmp1");

  extractor.begin(IndexExtractor::kTypeUInt32, indexes);
  extractor.extract(pixSrcRow0, 2);
  extractor.extract(pixSrcRow1, 3);

  pc->mul(pixSrcRow0, pixSrcRow0, stride);
  pc->mul(pixSrcRow1, pixSrcRow1, stride);
  pc->add(pixSrcRow0, pixSrcRow0, pixels);
  pc->add(pixSrcRow1, pixSrcRow1, pixels);

  extractor.extract(pixSrcOff, 0);
  pc->v_loada32(pixTop, mem_ptr(pixSrcRow0, pixSrcOff, 2));
  pc->v_loada32(pixBot, mem_ptr(pixSrcRow1, pixSrcOff, 2));
  extractor.extract(pixSrcOff, 1);

#if defined(BL_JIT_ARCH_X86)
  if (!pc->hasSSE4_1()) {
    pc->v_loada32(pixTmp0, mem_ptr(pixSrcRow0, pixSrcOff, 2));
    pc->v_loada32(pixTmp1, mem_ptr(pixSrcRow1, pixSrcOff, 2));

    pc->v_interleave_lo_u32(pixTop, pixTop, pixTmp0);
    pc->v_interleave_lo_u32(pixBot, pixBot, pixTmp1);
  }
  else
#endif // BL_JIT_ARCH_X86
  {
    pc->v_insert_u32(pixTop, mem_ptr(pixSrcRow0, pixSrcOff, 2), 1);
    pc->v_insert_u32(pixBot, mem_ptr(pixSrcRow1, pixSrcOff, 2), 1);
  }

  pc->v_swizzle_u32x4(pixTmp0, weights, swizzle(3, 3, 3, 3));
  pc->v_cvt_u8_lo_to_u16(pixTop, pixTop);

  pc->v_swizzle_u32x4(pixTmp1, weights, swizzle(2, 2, 2, 2));
  pc->v_cvt_u8_lo_to_u16(pixBot, pixBot);

  pc->v_mul_u16(pixTop, pixTop, pixTmp0);
  pc->v_mul_u16(pixBot, pixBot, pixTmp1);

  pc->v_add_i16(pixBot, pixBot, pixTop);
  pc->v_srli_u16(pixBot, pixBot, 8);

  pc->v_swizzle_u32x4(pixTop, weights, swizzle(0, 0, 1, 1));
  pc->v_mul_u16(pixTop, pixTop, pixBot);

  pc->v_swizzle_u32x4(pixTmp0, pixTop, swizzle(1, 0, 3, 2));
  pc->v_add_i16(pixTmp0, pixTmp0, pixTop);
  pc->v_srli_u16(pixTmp0, pixTmp0, 8);
}

} // {FetchUtils}
} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHUTILS_P_H_INCLUDED
