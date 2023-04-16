// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHUTILS_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHUTILS_P_H_INCLUDED

#include "../../format.h"
#include "../../pipeline/jit/pipecompiler_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

// Interleave callback is used to interleae a sequence of code into pixel fetching sequence. There are two scenarios in
// general:
//
//   - Fetching is performed by scalar loads and shuffles to form the destination pixel. In this case individual fetches
//     can be interleaved with another code to hide the latency of reading from memory and shuffling.
//   - Fetching is performed by hardware (vpgatherxx). In this case the interleave code is inserted after gather to hide
//     its latency (i.e. to not immediately depend on gathered content).
typedef void (*InterleaveCallback)(uint32_t step, void* data) BL_NOEXCEPT;

static void dummyInterleaveCallback(uint32_t step, void* data) noexcept { blUnused(step, data); }

//! Index extractor makes it easy to extract indexes from SIMD registers. We have learned the hard way that the best
//! way of extracting indexes is to use stack instead of dedicated instructions like PEXTRW/PEXTRD. The problem of such
//! instructions is that they have high latency on many older microarchitectures. Newer architectures decreased the
//! latency, but even 2-3 cycles is worse than fetching the index from stack.
class IndexExtractor {
public:
  BL_NONCOPYABLE(IndexExtractor)

  enum Type : uint32_t {
    kTypeNone,
    kTypeInt16,
    kTypeUInt16,
    kTypeInt32,
    kTypeUInt32,
    kTypeCount
  };

  PipeCompiler* _pc;
  x86::Mem _mem;
  uint32_t _type;
  uint16_t _indexSize;
  uint16_t _memSize;

  //! Creates a zero-initialized `IndexExtractor`.
  //!
  //! You must call `begin()` to make it usable.
  explicit IndexExtractor(PipeCompiler* pc) noexcept;

  //! Begins index extraction from a SIMD register `vec`.
  void begin(uint32_t type, const x86::Vec& vec) noexcept;
  //! Begins index extraction from memory.
  void begin(uint32_t type, const x86::Mem& mem, uint32_t memSize) noexcept;
  //! Extracts the given `index` into the destination register `dst`.
  void extract(const x86::Gp& dst, uint32_t index) noexcept;
};

//! Context that is used to fetch more than 1 pixel - used by SIMD fetchers that fetch 2, 4, 8, 16, or 32 pixels per
//! a single loop iteration.
class FetchContext {
public:
  BL_NONCOPYABLE(FetchContext)

  PipeCompiler* _pc;
  Pixel* _pixel;

  PixelFlags _fetchFlags;
  uint32_t _fetchIndex;
  BLInternalFormat _fetchFormat;
  uint8_t _fetchDone;
  uint8_t _a8FetchMode;
  uint8_t _a8FetchShift;

  x86::Gp aAcc;
  x86::Xmm aTmp;
  x86::Xmm pTmp0;
  x86::Xmm pTmp1;

  inline FetchContext(PipeCompiler* pc, Pixel* pixel, PixelCount n, BLInternalFormat format, PixelFlags fetchFlags) noexcept
    : _pc(pc),
      _pixel(pixel),
      _fetchFlags(fetchFlags),
      _fetchIndex(0),
      _fetchFormat(format),
      _fetchDone(0),
      _a8FetchMode(0),
      _a8FetchShift(0) { _init(n); }

  void _init(PixelCount n) noexcept;
  void fetchPixel(const x86::Mem& src) noexcept;
  void _fetchAll(const x86::Mem& src, uint32_t srcShift, IndexExtractor& extractor, const uint8_t* indexes, InterleaveCallback cb, void* cbData) noexcept;
  void packedFetchDone() noexcept;

  // Fetches all pixels and allows to interleave the fetch sequence with a lambda function `interleaveFunc`.
  template<class InterleaveFunc>
  void fetchAll(const x86::Mem& src, uint32_t srcShift, IndexExtractor& extractor, const uint8_t* indexes, InterleaveFunc&& interleaveFunc) noexcept {
    _fetchAll(src, srcShift, extractor, indexes, [](uint32_t step, void* data) noexcept {
      (*static_cast<const InterleaveFunc*>(data))(step);
    }, (void*)&interleaveFunc);
  }

  void end() noexcept;
};

enum class IndexLayout {
  kRegular,
  kHigh16Bits
};

namespace FetchUtils {

static void x_convert_gathered_pixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, const VecArray& gPix) noexcept {
  if (p.isA8()) {
    x86::Compiler* cc = pc->cc;

    pc->v_srl_i32(gPix, gPix, 24);

    if (blTestFlag(flags, PixelFlags::kPA)) {
      SimdWidth paSimdWidth = pc->simdWidthOf(DataWidth::k8, n);
      uint32_t paRegCount = pc->regCountOf(DataWidth::k8, n);

      pc->newVecArray(p.pa, paRegCount, paSimdWidth, p.name(), "pa");
      BL_ASSERT(p.pa.size() == 1);

      if (pc->hasAVX512()) {
        cc->vpmovdb(p.pa[0], gPix[0]);
      }
      else {
        pc->x_packs_i16_u8(p.pa[0].cloneAs(gPix[0]), gPix[0], gPix[0]);
        pc->x_packs_i16_u8(p.pa[0], p.pa[0], p.pa[0]);
      }
    }
    else {
      SimdWidth uaSimdWidth = pc->simdWidthOf(DataWidth::k16, n);
      uint32_t uaRegCount = pc->regCountOf(DataWidth::k16, n);

      pc->newVecArray(p.ua, uaRegCount, uaSimdWidth, p.name(), "ua");
      BL_ASSERT(p.ua.size() == 1);

      if (pc->hasAVX512())
        cc->vpmovdw(p.ua[0], gPix[0]);
      else
        pc->x_packs_i16_u8(p.ua[0].cloneAs(gPix[0]), gPix[0], gPix[0]);
    }
  }
  else {
    p.pc = gPix;
    pc->rename(p.pc, p.name(), "pc");
  }
}

static void x_gather_pixels(PipeCompiler* pc, Pixel& p, PixelCount n, BLInternalFormat format, PixelFlags flags, const x86::Mem& src, const x86::Vec& idx, uint32_t shift, IndexLayout indexLayout, InterleaveCallback cb, void* cbData) noexcept {
  x86::Compiler* cc = pc->cc;
  x86::Mem mem(src);

  if (blFormatInfo[size_t(format)].depth == 32 && pc->hasOptFlag(PipeOptFlags::kFastGather)) {
    mem.setIndex(idx);
    mem.setShift(shift);

    VecArray pixels;
    if (n <= 4u) {
      pc->newXmmArray(pixels, 1, p.name(), "pc");
    }
    else {
      pc->newYmmArray(pixels, 1, p.name(), "pc");
    }

    if (indexLayout == IndexLayout::kHigh16Bits)
      pc->v_srl_i32(idx, idx, 16);

    pc->v_zero_i(pixels[0]);
    if (pc->hasAVX512()) {
      x86::KReg pred = cc->newKw("pred");
      cc->kxnorw(pred, pred, pred);
      cc->k(pred).vpgatherdd(pixels[0], mem);
    }
    else {
      x86::Vec pred = cc->newSimilarReg(pixels[0], "pred");
      pc->v_ones_i(pred);
      cc->vpgatherdd(pixels[0], mem, pred);
    }

    for (uint32_t i = 0; i < p.count().value(); i++)
      cb(i, cbData);

    x_convert_gathered_pixels(pc, p, n, flags, pixels);
  }
  else {
    uint32_t indexType = 0;
    const uint8_t* iExtSequence = nullptr;

    if (indexLayout == IndexLayout::kRegular) {
      static const uint8_t indexes[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
      indexType = IndexExtractor::kTypeUInt32;
      iExtSequence = indexes;
    }
    else {
      static const uint8_t indexes[] = { 1, 3, 5, 7, 9, 11, 13, 15 };
      indexType = IndexExtractor::kTypeUInt16;
      iExtSequence = indexes;
    }

    IndexExtractor iExt(pc);
    iExt.begin(indexType, idx);

    FetchContext fCtx(pc, &p, n, format, flags);
    fCtx._fetchAll(src, shift, iExt, iExtSequence, cb, cbData);
  }
}

template<class InterleaveFunc>
static void x_gather_pixels(PipeCompiler* pc, Pixel& p, PixelCount n, BLInternalFormat format, PixelFlags flags, const x86::Mem& src, const x86::Vec& idx, uint32_t shift, IndexLayout indexLayout, InterleaveFunc&& interleaveFunc) noexcept {
  x_gather_pixels(pc, p, n, format, flags, src, idx, shift, indexLayout, [](uint32_t step, void* data) noexcept {
    (*static_cast<const InterleaveFunc*>(data))(step);
  }, (void*)&interleaveFunc);
}

//! Fetch 4 pixels indexed in XMM reg (32-bit unsigned offsets).
template<typename FetchFuncT>
static void fetch_4x_t(PipeCompiler* pc, const x86::Xmm& idx4x, const FetchFuncT& fetchFunc) noexcept {
  x86::Compiler* cc = pc->cc;
  IndexExtractor extractor(pc);

  if (cc->is64Bit()) {
    x86::Gp idx0 = cc->newIntPtr("@idx0");
    x86::Gp idx1 = cc->newIntPtr("@idx1");

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
    x86::Gp idx = cc->newIntPtr("@idx");

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
  FetchContext* fcA, const x86::Mem& srcA, const x86::Xmm& idx4x, uint32_t shift) noexcept {

  x86::Mem m(srcA);
  m.setShift(shift);

  fetch_4x_t(fcA->_pc, idx4x, [&](const x86::Gp& idx) {
    m.setIndex(idx);
    fcA->fetchPixel(m);
  });
}

static void fetch_4x_twice(
  FetchContext* fcA, const x86::Mem& srcA,
  FetchContext* fcB, const x86::Mem& srcB, const x86::Xmm& idx4x, uint32_t shift) noexcept {

  x86::Mem mA(srcA);
  x86::Mem mB(srcB);

  mA.setShift(shift);
  mB.setShift(shift);

  fetch_4x_t(fcA->_pc, idx4x, [&](const x86::Gp& idx) {
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
  x86::Vec& out,
  const Pixels& pixels,
  const Stride& stride,
  BLInternalFormat format,
  uint32_t indexShift,
  const x86::Vec& indexes,
  const x86::Vec& weights) noexcept {

  x86::Compiler* cc = pc->cc;
  IndexExtractor extractor(pc);

  x86::Gp pixSrcRow0 = cc->newIntPtr("pixSrcRow0");
  x86::Gp pixSrcRow1 = cc->newIntPtr("pixSrcRow1");
  x86::Gp pixSrcOff = cc->newIntPtr("pixSrcOff");
  x86::Gp pixAcc = cc->newUInt32("pixAcc");
  x86::Vec wTmp = cc->newXmm("wTmp");

  extractor.begin(IndexExtractor::kTypeUInt32, indexes);
  extractor.extract(pixSrcRow0, 2);
  extractor.extract(pixSrcRow1, 3);

  int32_t alphaOffset = 0;
  switch (format) {
    case BLInternalFormat::kPRGB32:
      alphaOffset = 3;
      break;

    case BLInternalFormat::kXRGB32:
      alphaOffset = 3;
      break;

    default:
      break;
  }

  x86::Mem row0m = x86::byte_ptr(pixSrcRow0, pixSrcOff, indexShift, alphaOffset);
  x86::Mem row1m = x86::byte_ptr(pixSrcRow1, pixSrcOff, indexShift, alphaOffset);

  cc->imul(pixSrcRow0, stride);
  cc->imul(pixSrcRow1, stride);
  cc->add(pixSrcRow0, pixels);
  cc->add(pixSrcRow1, pixels);

  extractor.extract(pixSrcOff, 0);
  cc->movzx(pixAcc, row0m);        // [0    , 0    , 0    , Px0y0]
  cc->shl(pixAcc, 8);              // [0    , 0    , Px0y0, 0    ]
  cc->mov(pixAcc.r8(), row1m);     // [0    , 0    , Px0y0, Px0y1]
  cc->shl(pixAcc, 8);              // [0    , Px0y0, Px0y1, 0    ]

  extractor.extract(pixSrcOff, 1);
  cc->mov(pixAcc.r8(), row0m);     // [0    , Px0y0, Px0y1, Px1y0]
  cc->shl(pixAcc, 8);              // [Px0y0, Px0y1, Px1y0, 0    ]
  cc->mov(pixAcc.r8(), row1m);     // [Px0y0, Px0y1, Px1y0, Px1y1]

  pc->s_mov_i32(out, pixAcc);
  pc->v_swizzle_i32(wTmp, weights, x86::shuffleImm(3, 3, 2, 2));

  pc->v_mov_u8_u16(out, out);
  pc->v_madd_i16_i32(out, out, wTmp);
  pc->v_swizzle_lo_i16(wTmp, weights, x86::shuffleImm(1, 1, 0, 0));
  pc->v_mulh_u16(out, out, wTmp);
  pc->v_swizzle_i32(wTmp, out, x86::shuffleImm(3, 2, 0, 1));
  pc->v_add_i32(out, out, wTmp);
}

//! Fetch 1xPRGB pixel by doing a bilinear interpolation with its neighbors.
//!
//! Weights = {256-wy, 256-wy, wy, wy, 256-wx, 256-wx, wx, wx}
template<typename Pixels, typename Stride>
BL_NOINLINE void xFilterBilinearARGB32_1x(
  PipeCompiler* pc,
  x86::Vec& out,
  const Pixels& pixels,
  const Stride& stride,
  const x86::Vec& indexes,
  const x86::Vec& weights) noexcept {

  IndexExtractor extractor(pc);
  x86::Compiler* cc = pc->cc;

  x86::Gp pixSrcRow0 = cc->newIntPtr("pixSrcRow0");
  x86::Gp pixSrcRow1 = cc->newIntPtr("pixSrcRow1");
  x86::Gp pixSrcOff = cc->newIntPtr("pixSrcOff");

  x86::Vec pixTop = cc->newXmm("pixTop");
  x86::Vec pixBot = cc->newXmm("pixBot");

  x86::Vec pixTmp0 = out;
  x86::Vec pixTmp1 = cc->newXmm("pixTmp1");

  extractor.begin(IndexExtractor::kTypeUInt32, indexes);
  extractor.extract(pixSrcRow0, 2);
  extractor.extract(pixSrcRow1, 3);

  cc->imul(pixSrcRow0, stride);
  cc->imul(pixSrcRow1, stride);
  cc->add(pixSrcRow0, pixels);
  cc->add(pixSrcRow1, pixels);

  extractor.extract(pixSrcOff, 0);
  pc->v_load_i32(pixTop, x86::ptr(pixSrcRow0, pixSrcOff, 2));
  pc->v_load_i32(pixBot, x86::ptr(pixSrcRow1, pixSrcOff, 2));
  extractor.extract(pixSrcOff, 1);

  if (pc->hasSSE4_1()) {
    pc->v_insert_u32_(pixTop, pixTop, x86::ptr(pixSrcRow0, pixSrcOff, 2), 1);
    pc->v_insert_u32_(pixBot, pixBot, x86::ptr(pixSrcRow1, pixSrcOff, 2), 1);
  }
  else {
    pc->v_load_i32(pixTmp0, x86::ptr(pixSrcRow0, pixSrcOff, 2));
    pc->v_load_i32(pixTmp1, x86::ptr(pixSrcRow1, pixSrcOff, 2));

    pc->v_interleave_lo_i32(pixTop, pixTop, pixTmp0);
    pc->v_interleave_lo_i32(pixBot, pixBot, pixTmp1);
  }

  pc->v_swizzle_i32(pixTmp0, weights, x86::shuffleImm(3, 3, 3, 3));
  pc->v_mov_u8_u16(pixTop, pixTop);

  pc->v_swizzle_i32(pixTmp1, weights, x86::shuffleImm(2, 2, 2, 2));
  pc->v_mov_u8_u16(pixBot, pixBot);

  pc->v_mul_u16(pixTop, pixTop, pixTmp0);
  pc->v_mul_u16(pixBot, pixBot, pixTmp1);

  pc->v_add_i16(pixBot, pixBot, pixTop);
  pc->v_srl_i16(pixBot, pixBot, 8);

  pc->v_swizzle_i32(pixTop, weights, x86::shuffleImm(0, 0, 1, 1));
  pc->v_mul_u16(pixTop, pixTop, pixBot);

  pc->v_swizzle_i32(pixTmp0, pixTop, x86::shuffleImm(1, 0, 3, 2));
  pc->v_add_i16(pixTmp0, pixTmp0, pixTop);
  pc->v_srl_i16(pixTmp0, pixTmp0, 8);
}

} // {FetchUtils}

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHUTILS_P_H_INCLUDED
