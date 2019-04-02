// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_BLFETCHUTILS_P_H
#define BLEND2D_PIPEGEN_BLFETCHUTILS_P_H

#include "../pipegen/blpipecompiler_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::IndexExtractorU16]
// ============================================================================

//! Helper class that simplifies extracting 8 indexers from SIMD to GP registers.
class IndexExtractorU16 {
public:
  BL_NONCOPYABLE(IndexExtractorU16)

  enum Strategy : uint32_t {
    kStrategyExtractSSE2   = 0, //!< Use `PEXTRW` (SSE2) instruction.
    kStrategyStack         = 1  //!< Write indexes on stack then load to GP.
  };

  PipeCompiler* _pc;
  uint32_t _strategy;
  x86::Xmm _vec;
  x86::Mem _stack;

  inline explicit IndexExtractorU16(PipeCompiler* pc) noexcept
    : _pc(pc),
      _strategy(kStrategyStack),
      _vec(),
      _stack() {}

  inline IndexExtractorU16(PipeCompiler* pc, uint32_t strategy) noexcept
    : _pc(pc),
      _strategy(strategy),
      _vec(),
      _stack() {}

  void begin(const x86::Xmm& vec) noexcept;
  void extract(x86::Gp& dst, uint32_t index16) noexcept;
};

// ============================================================================
// [BLPipeGen::IndexExtractorU32]
// ============================================================================

//! Helper class that simplifies extracting 4 indexers from SIMD to GP registers.
class IndexExtractorU32 {
public:
  BL_NONCOPYABLE(IndexExtractorU32)

  enum Strategy : uint32_t {
    kStrategyExtractSSE4_1 = 0, //!< Use `PEXTRD` (SSE4.1) instruction.
    kStrategyStack         = 1  //!< Write indexes on stack then load to GP.
  };

  PipeCompiler* _pc;
  uint32_t _strategy;
  x86::Xmm _vec;
  x86::Mem _stack;

  inline explicit IndexExtractorU32(PipeCompiler* pc) noexcept
    : _pc(pc),
      _strategy(kStrategyStack),
      _vec(),
      _stack() {}

  inline IndexExtractorU32(PipeCompiler* pc, uint32_t strategy) noexcept
    : _pc(pc),
      _strategy(strategy),
      _vec(),
      _stack() {}

  void begin(const x86::Xmm& vec) noexcept;
  void extract(x86::Gp& dst, uint32_t index32) noexcept;
};

// ============================================================================
// [BLPipeGen::FetchContext4X]
// ============================================================================

class FetchContext4X {
public:
  BL_NONCOPYABLE(FetchContext4X)

  PipeCompiler* pc;
  PixelARGB* p;

  uint32_t fetchFlags;
  uint32_t fetchIndex;

  x86::Xmm pARGB32Tmp0;
  x86::Xmm pARGB32Tmp1;

  inline FetchContext4X(PipeCompiler* pc, PixelARGB* p, uint32_t flags) noexcept
    : pc(pc),
      p(p),
      fetchFlags(flags),
      fetchIndex(0) { _init(); }

  void _init() noexcept;
  void fetchARGB32(const x86::Mem& src) noexcept;
  void end() noexcept;
};

// ============================================================================
// [BLPipeGen::FetchContext8X]
// ============================================================================

class FetchContext8X {
public:
  BL_NONCOPYABLE(FetchContext8X)

  PipeCompiler* pc;
  PixelARGB* p;

  uint32_t fetchFlags;
  uint32_t fetchIndex;

  x86::Xmm pARGB32Tmp0;
  x86::Xmm pARGB32Tmp1;

  inline FetchContext8X(PipeCompiler* pc, PixelARGB* p, uint32_t flags) noexcept
    : pc(pc),
      p(p),
      fetchFlags(flags),
      fetchIndex(0) { _init(); }

  void _init() noexcept;
  void fetchARGB32(const x86::Mem& src) noexcept;
  void end() noexcept;
};

// ============================================================================
// [BLPipeGen::FetchUtils]
// ============================================================================

namespace FetchUtils {
  //! Fetch 4 pixels indexed in XMM reg (32-bit unsigned integers).
  template<typename FetchFuncT>
  static void fetchARGB32_4x_t(PipeCompiler* pc, const x86::Xmm& idx4x, const FetchFuncT& fetchFunc) noexcept {
    x86::Compiler* cc = pc->cc;

    /*
    if (pc->hasAVX()) {
      Operand& p0 = (fetchFlags & PixelARGB::kPC) ? p->pc[0] : p->uc[0];
      x86::Xmm p = p0.as<x86::Vec>().xmm();
      x86::Mem m(base);

      m.setIndex(idx4x);
      cc->vpcmpeqb(p, p, p);
      cc->vpgatherdd(p, m, p);

      isPacked = true;
      return;
    }
    */

    IndexExtractorU32 extractor(pc);
    x86::Gp idx0 = cc->newIntPtr("@idx0");
    x86::Gp idx1 = cc->newIntPtr("@idx1");

    extractor.begin(idx4x);
    extractor.extract(idx0, 0);
    extractor.extract(idx1, 1);

    fetchFunc(idx0);
    extractor.extract(idx0, 2);

    fetchFunc(idx1);
    extractor.extract(idx1, 3);

    fetchFunc(idx0);
    fetchFunc(idx1);
  }

  static void fetchARGB32_4x(
    FetchContext4X* fcA, const x86::Mem& srcA, const x86::Xmm& idx4x, int shift) noexcept {

    x86::Mem m(srcA);
    m.setShift(shift);

    fetchARGB32_4x_t(fcA->pc, idx4x, [&](const x86::Gp& idx) {
      m.setIndex(idx);
      fcA->fetchARGB32(m);
    });
  }

  static void fetchARGB32_4x_twice(
    FetchContext4X* fcA, const x86::Mem& srcA,
    FetchContext4X* fcB, const x86::Mem& srcB, const x86::Xmm& idx4x, int shift) noexcept {

    x86::Mem mA(srcA);
    x86::Mem mB(srcB);

    mA.setShift(shift);
    mB.setShift(shift);

    fetchARGB32_4x_t(fcA->pc, idx4x, [&](const x86::Gp& idx) {
      mA.setIndex(idx);
      mB.setIndex(idx);

      fcA->fetchARGB32(mA);
      fcB->fetchARGB32(mB);
    });
  }

  //! Fetch 1 pixel by doing a bilinear interpolation with its neighbors.
  //!
  //! weights = {256-wy, wy, 256-wx, wx}
  //!
  //! P' = [x0y0 * (256 - wx) * (256 - wy) +
  //!       x1y0 * (wx      ) * (256 - wy) +
  //!       x0y1 * (256 - wx) * (wy      ) +
  //!       x1y1 * (wx      ) * (wy      ) ]
  //!
  //! P' = [x0y0 * (256 - wx) + x1y0 * (wx)] * (256 - wy) +
  //!      [x0y1 * (256 - wx) + x1y1 * (wx)] * wy
  //!
  //! P' = [x0y0 * (256 - wy) + x0y1 * (wy)] * (256 - wx) +
  //!      [x1y0 * (256 - wy) + x1y1 * (wy)] * wx
  template<typename Pixels, typename Stride>
  BL_NOINLINE void xFilterBilinearARGB32_1x(
    PipeCompiler* pc,
    x86::Vec& out,
    const Pixels& pixels,
    const Stride& stride,
    const x86::Vec& indexes,
    const x86::Vec& weights) noexcept {

    IndexExtractorU32 extractor(pc, IndexExtractorU32::kStrategyStack);
    x86::Compiler* cc = pc->cc;

    x86::Gp pixSrcRow0 = cc->newIntPtr("pixSrcRow0");
    x86::Gp pixSrcRow1 = cc->newIntPtr("pixSrcRow1");
    x86::Gp pixSrcOff = cc->newInt32("pixSrcOff");

    x86::Vec pixTop = cc->newXmm("pixTop");
    x86::Vec pixBot = cc->newXmm("pixBot");

    x86::Vec pixTmp0 = out;
    x86::Vec pixTmp1 = cc->newXmm("pixTmp1");

    extractor.begin(indexes.as<x86::Xmm>());

    extractor.extract(pixSrcRow0, 2);
    extractor.extract(pixSrcRow1, 3);
    extractor.extract(pixSrcOff, 0);

    cc->imul(pixSrcRow0, stride);
    cc->imul(pixSrcRow1, stride);

    cc->add(pixSrcRow0, pixels);
    cc->add(pixSrcRow1, pixels);

    pc->vloadi32(pixTop, x86::ptr(pixSrcRow0, pixSrcOff, 2));
    pc->vloadi32(pixBot, x86::ptr(pixSrcRow1, pixSrcOff, 2));
    extractor.extract(pixSrcOff, 1);

    if (pc->hasSSE4_1()) {
      pc->vinsertu32_(pixTop, pixTop, x86::ptr(pixSrcRow0, pixSrcOff, 2), 1);
      pc->vinsertu32_(pixBot, pixBot, x86::ptr(pixSrcRow1, pixSrcOff, 2), 1);
    }
    else {
      pc->vloadi32(pixTmp0, x86::ptr(pixSrcRow0, pixSrcOff, 2));
      pc->vloadi32(pixTmp1, x86::ptr(pixSrcRow1, pixSrcOff, 2));

      pc->vunpackli32(pixTop, pixTop, pixTmp0);
      pc->vunpackli32(pixBot, pixBot, pixTmp1);
    }

    pc->vswizi32(pixTmp0, weights, x86::Predicate::shuf(3, 3, 3, 3));
    pc->vmovu8u16(pixTop, pixTop);

    pc->vswizi32(pixTmp1, weights, x86::Predicate::shuf(2, 2, 2, 2));
    pc->vmovu8u16(pixBot, pixBot);

    pc->vmulu16(pixTop, pixTop, pixTmp0);
    pc->vmulu16(pixBot, pixBot, pixTmp1);
    pc->vaddi16(pixBot, pixBot, pixTop);

    pc->vswizi32(pixTop, weights, x86::Predicate::shuf(0, 0, 1, 1));
    pc->vmulhu16(pixTop, pixTop, pixBot);

    pc->vswizi32(pixTmp0, pixTop, x86::Predicate::shuf(1, 0, 3, 2));
    pc->vaddi16(pixTmp0, pixTmp0, pixTop);
  }
}

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_BLFETCHUTILS_P_H
