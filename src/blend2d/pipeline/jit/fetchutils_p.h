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

namespace bl {
namespace Pipeline {
namespace JIT {

enum class IndexLayout : uint32_t {
  //! Consecutive unsigned 16-bit indexes.
  kUInt16,
  //! Consecutive unsigned 32-bit indexes.
  kUInt32,
  //! Unsigned 16-bit indexes in hi 32-bit words (or, odd 16-bit indexes).
  kUInt32Lo16,
  //! Unsigned 16-bit indexes in hi 32-bit words (or, odd 16-bit indexes).
  kUInt32Hi16
};

// Interleave callback is used to interleave a sequence of code into pixel fetching sequence. There are two scenarios in
// general:
//
//   - Fetching is performed by scalar loads and shuffles to form the destination pixel. In this case individual fetches
//     can be interleaved with another code to hide the latency of reading from memory and shuffling.
//   - Fetching is performed by hardware (vpgatherxx). In this case the interleave code is inserted after gather to hide
//     its latency (i.e. to not immediately depend on gathered content).
typedef void (*InterleaveCallback)(uint32_t step, void* data) BL_NOEXCEPT;

static void dummyInterleaveCallback(uint32_t step, void* data) noexcept { blUnused(step, data); }

#if defined(BL_JIT_ARCH_X86)

//! Index extractor makes it easy to extract indexes from SIMD registers. We have learned the hard way that the best
//! way of extracting indexes is to use stack instead of dedicated instructions like PEXTRW/PEXTRD. The problem of such
//! instructions is that they have high latency on many older micro-architectures. Newer architectures decreased the
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
  Mem _mem;
  uint32_t _type;
  uint16_t _indexSize;
  uint16_t _memSize;

  //! Creates a zero-initialized `IndexExtractor`.
  //!
  //! You must call `begin()` to make it usable.
  explicit IndexExtractor(PipeCompiler* pc) noexcept;

  //! Begins index extraction from a SIMD register `vec`.
  void begin(uint32_t type, const Vec& vec) noexcept;
  //! Begins index extraction from memory.
  void begin(uint32_t type, const Mem& mem, uint32_t memSize) noexcept;
  //! Extracts the given `index` into the destination register `dst`.
  void extract(const Gp& dst, uint32_t index) noexcept;
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
  FormatExt _fetchFormat;
  uint8_t _fetchDone;
  uint8_t _a8FetchMode;
  uint8_t _a8FetchShift;

  Gp aAcc;
  Vec aTmp;
  Vec pTmp0;
  Vec pTmp1;

  inline FetchContext(PipeCompiler* pc, Pixel* pixel, PixelCount n, FormatExt format, PixelFlags fetchFlags) noexcept
    : _pc(pc),
      _pixel(pixel),
      _fetchFlags(fetchFlags),
      _fetchIndex(0),
      _fetchFormat(format),
      _fetchDone(0),
      _a8FetchMode(0),
      _a8FetchShift(0) { _init(n); }

  void _init(PixelCount n) noexcept;
  void fetchPixel(const Mem& src) noexcept;
  void _fetchAll(const Mem& src, uint32_t srcShift, IndexExtractor& extractor, const uint8_t* indexes, InterleaveCallback cb, void* cbData) noexcept;
  void packedFetchDone() noexcept;

  // Fetches all pixels and allows to interleave the fetch sequence with a lambda function `interleaveFunc`.
  template<class InterleaveFunc>
  void fetchAll(const Mem& src, uint32_t srcShift, IndexExtractor& extractor, const uint8_t* indexes, InterleaveFunc&& interleaveFunc) noexcept {
    _fetchAll(src, srcShift, extractor, indexes, [](uint32_t step, void* data) noexcept {
      (*static_cast<const InterleaveFunc*>(data))(step);
    }, (void*)&interleaveFunc);
  }

  void end() noexcept;
};

namespace FetchUtils {

void x_gather_pixels(PipeCompiler* pc, Pixel& p, PixelCount n, FormatExt format, PixelFlags flags, const Mem& src, const Vec& idx, uint32_t shift, IndexLayout indexLayout, InterleaveCallback cb, void* cbData) noexcept;

template<class InterleaveFunc>
static void x_gather_pixels(PipeCompiler* pc, Pixel& p, PixelCount n, FormatExt format, PixelFlags flags, const Mem& src, const Vec& idx, uint32_t shift, IndexLayout indexLayout, InterleaveFunc&& interleaveFunc) noexcept {
  x_gather_pixels(pc, p, n, format, flags, src, idx, shift, indexLayout, [](uint32_t step, void* data) noexcept {
    (*static_cast<const InterleaveFunc*>(data))(step);
  }, (void*)&interleaveFunc);
}

void x_convert_gathered_pixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, const VecArray& gPix) noexcept;

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

  AsmCompiler* cc = pc->cc;
  IndexExtractor extractor(pc);

  Gp pixSrcRow0 = pc->newGpPtr("pixSrcRow0");
  Gp pixSrcRow1 = pc->newGpPtr("pixSrcRow1");
  Gp pixSrcOff = pc->newGpPtr("pixSrcOff");
  Gp pixAcc = pc->newGp32("pixAcc");
  Vec wTmp = cc->newXmm("wTmp");

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

  Mem row0m = x86::byte_ptr(pixSrcRow0, pixSrcOff, indexShift, alphaOffset);
  Mem row1m = x86::byte_ptr(pixSrcRow1, pixSrcOff, indexShift, alphaOffset);

  pc->mul(pixSrcRow0, pixSrcRow0, stride);
  pc->mul(pixSrcRow1, pixSrcRow1, stride);
  pc->add(pixSrcRow0, pixSrcRow0, pixels);
  pc->add(pixSrcRow1, pixSrcRow1, pixels);

  extractor.extract(pixSrcOff, 0);
  pc->load_u8(pixAcc, row0m);      // [0    , 0    , 0    , Px0y0]
  pc->shl(pixAcc, pixAcc, 8);      // [0    , 0    , Px0y0, 0    ]
  cc->mov(pixAcc.r8(), row1m);     // [0    , 0    , Px0y0, Px0y1]
  pc->shl(pixAcc, pixAcc, 8);      // [0    , Px0y0, Px0y1, 0    ]

  extractor.extract(pixSrcOff, 1);
  cc->mov(pixAcc.r8(), row0m);     // [0    , Px0y0, Px0y1, Px1y0]
  pc->shl(pixAcc, pixAcc, 8);      // [Px0y0, Px0y1, Px1y0, 0    ]
  cc->mov(pixAcc.r8(), row1m);     // [Px0y0, Px0y1, Px1y0, Px1y1]

  pc->s_mov_i32(out, pixAcc);
  pc->v_swizzle_u32(wTmp, weights, x86::shuffleImm(3, 3, 2, 2));

  pc->v_mov_u8_u16(out, out);
  pc->v_madd_i16_i32(out, out, wTmp);
  pc->v_swizzle_lo_u16(wTmp, weights, x86::shuffleImm(1, 1, 0, 0));
  pc->v_mulh_u16(out, out, wTmp);
  pc->v_swizzle_u32(wTmp, out, x86::shuffleImm(3, 2, 0, 1));
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

  Vec pixTop = pc->newXmm("pixTop");
  Vec pixBot = pc->newXmm("pixBot");

  Vec pixTmp0 = out;
  Vec pixTmp1 = pc->newXmm("pixTmp1");

  extractor.begin(IndexExtractor::kTypeUInt32, indexes);
  extractor.extract(pixSrcRow0, 2);
  extractor.extract(pixSrcRow1, 3);

  pc->mul(pixSrcRow0, pixSrcRow0, stride);
  pc->mul(pixSrcRow1, pixSrcRow1, stride);
  pc->add(pixSrcRow0, pixSrcRow0, pixels);
  pc->add(pixSrcRow1, pixSrcRow1, pixels);

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

    pc->v_interleave_lo_u32(pixTop, pixTop, pixTmp0);
    pc->v_interleave_lo_u32(pixBot, pixBot, pixTmp1);
  }

  pc->v_swizzle_u32(pixTmp0, weights, x86::shuffleImm(3, 3, 3, 3));
  pc->v_mov_u8_u16(pixTop, pixTop);

  pc->v_swizzle_u32(pixTmp1, weights, x86::shuffleImm(2, 2, 2, 2));
  pc->v_mov_u8_u16(pixBot, pixBot);

  pc->v_mul_u16(pixTop, pixTop, pixTmp0);
  pc->v_mul_u16(pixBot, pixBot, pixTmp1);

  pc->v_add_i16(pixBot, pixBot, pixTop);
  pc->v_srl_i16(pixBot, pixBot, 8);

  pc->v_swizzle_u32(pixTop, weights, x86::shuffleImm(0, 0, 1, 1));
  pc->v_mul_u16(pixTop, pixTop, pixBot);

  pc->v_swizzle_u32(pixTmp0, pixTop, x86::shuffleImm(1, 0, 3, 2));
  pc->v_add_i16(pixTmp0, pixTmp0, pixTop);
  pc->v_srl_i16(pixTmp0, pixTmp0, 8);
}

} // {FetchUtils}

#endif

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHUTILS_P_H_INCLUDED
