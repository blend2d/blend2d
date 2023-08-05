// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHGRADIENTPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHGRADIENTPART_P_H_INCLUDED

#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchutils_p.h"
#include "../../support/wrap_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

class GradientDitheringContext {
public:
  PipeCompiler* pc {};
  bool _isRectFill {};
  x86::Gp _dmPosition;
  x86::Gp _dmOriginX;
  x86::Vec _dmValues;

  BL_INLINE explicit GradientDitheringContext(PipeCompiler* pc) noexcept
    : pc(pc) {}

  BL_INLINE_NODEBUG bool isRectFill() const noexcept { return _isRectFill; }

  void initY(const x86::Gp& x, const x86::Gp& y) noexcept;
  void advanceY() noexcept;

  void startAtX(const x86::Gp& x) noexcept;
  void advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept;
  void advanceXAfterFetch(uint32_t n) noexcept;

  void ditherUnpackedPixels(Pixel& p) noexcept;
};

//! Base class for all gradient fetch parts.
class FetchGradientPart : public FetchPart {
public:
  ExtendMode _extendMode {};
  bool _ditheringEnabled {};

  x86::Gp _tablePtr;
  GradientDitheringContext _ditheringContext;

  FetchGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept;

  //! Returns the gradient extend mode.
  BL_INLINE_NODEBUG ExtendMode extendMode() const noexcept { return _extendMode; }

  //! Returns true if the gradient extend mode is Pad.
  BL_INLINE_NODEBUG bool isPad() const noexcept { return _extendMode == ExtendMode::kPad; }
  //! Returns true if the gradient extend mode is RoR.
  BL_INLINE_NODEBUG bool isRoR() const noexcept { return _extendMode == ExtendMode::kRoR; }

  BL_INLINE_NODEBUG bool ditheringEnabled() const noexcept { return _ditheringEnabled; }
  BL_INLINE_NODEBUG void setDitheringEnabled(bool value) noexcept { _ditheringEnabled = value; }

  BL_INLINE_NODEBUG int tablePtrShift() const noexcept { return _ditheringEnabled ? 3 : 2; }

  void fetchSinglePixel(Pixel& dst, PixelFlags flags, const x86::Gp& idx) noexcept;

  void fetchMultiplePixels(Pixel& dst, PixelCount n, PixelFlags flags, const x86::Vec& idx, IndexLayout indexLayout, InterleaveCallback cb, void* cbData) noexcept;

  template<class InterleaveFunc>
  void fetchMultiplePixels(Pixel& dst, PixelCount n, PixelFlags flags, const x86::Vec& idx, IndexLayout indexLayout, InterleaveFunc&& interleaveFunc) noexcept {
    fetchMultiplePixels(dst, n, flags, idx, indexLayout, [](uint32_t step, void* data) noexcept {
      (*static_cast<const InterleaveFunc*>(data))(step);
    }, (void*)&interleaveFunc);
  }
};

//! Linear gradient fetch part.
class FetchLinearGradientPart : public FetchGradientPart {
public:
  struct LinearRegs {
    x86::Gp dtGp;
    x86::Vec pt;
    x86::Vec dt;
    x86::Vec dtN;
    x86::Vec py;
    x86::Vec dy;
    x86::Vec maxi;
    x86::Vec rori;
    x86::Vec vIdx;
  };

  BLWrap<LinearRegs> f;

  FetchLinearGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept;

  void preparePart() noexcept override;

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const x86::Gp& x) noexcept override;
  void advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept override;
  void calcAdvanceX(const x86::Vec& dst, const x86::Gp& diff) const noexcept;

  void prefetch1() noexcept override;
  void enterN() noexcept override;
  void leaveN() noexcept override;
  void prefetchN() noexcept override;
  void postfetchN() noexcept override;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;
};

//! Radial gradient fetch part.
class FetchRadialGradientPart : public FetchGradientPart {
public:
  // `d`   - determinant.
  // `dd`  - determinant delta.
  // `ddd` - determinant-delta delta.
  struct RadialRegs {
    x86::Vec xx_xy;
    x86::Vec yx_yy;

    x86::Vec ax_ay;
    x86::Vec fx_fy;
    x86::Vec da_ba;

    x86::Vec d_b;
    x86::Vec dd_bd;
    x86::Vec ddx_ddy;

    x86::Vec px_py;
    x86::Vec scale;
    x86::Vec ddd;
    x86::Vec value;

    x86::Vec vmaxi;
    x86::Vec vrori;
    x86::Vec vmaxf; // Like `vmaxi`, but converted to `float`.

    // 4+ pixels.
    x86::Vec d_b_prev;
    x86::Vec dd_bd_prev;
  };

  BLWrap<RadialRegs> f;

  FetchRadialGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept;

  void preparePart() noexcept override;

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const x86::Gp& x) noexcept override;
  void advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept override;

  void prefetch1() noexcept override;
  void prefetchN() noexcept override;
  void postfetchN() noexcept override;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;

  void precalc(const x86::Vec& px_py) noexcept;
};

//! Conic gradient fetch part.
class FetchConicGradientPart : public FetchGradientPart {
public:
  struct ConicRegs {
    x86::Gp consts;

    x86::Xmm px;
    x86::Xmm xx;
    x86::Xmm hx_hy;
    x86::Xmm yx_yy;
    x86::Vec ay;
    x86::Vec by;
    x86::Vec angleOffset;
    x86::Vec maxi; // Maximum table index, basically `precision - 1` (mask).

    // Temporary values precalculated for the next fetch loop.
    x86::Vec t0, t1, t2;
    x86::KReg t1Pred;

    // 4+ pixels.
    x86::Vec xx_inc;
    x86::Vec xx_off;
  };

  BLWrap<ConicRegs> f;

  FetchConicGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept;

  void preparePart() noexcept override;

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const x86::Gp& x) noexcept override;
  void advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept override;

  void recalcX() noexcept;

  void prefetchN() noexcept override;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHGRADIENTPART_P_H_INCLUDED
