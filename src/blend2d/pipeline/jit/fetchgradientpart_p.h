// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHGRADIENTPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHGRADIENTPART_P_H_INCLUDED

#include "../../pipeline/jit/fetchpart_p.h"
#include "../../support/wrap_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

//! Base class for all gradient fetch parts.
class FetchGradientPart : public FetchPart {
public:
  struct CommonRegs {
    x86::Gp table;
  };

  ExtendMode _extendMode {};

  FetchGradientPart(PipeCompiler* pc, FetchType fetchType, uint32_t format) noexcept;

  //! Returns the gradient extend mode.
  BL_INLINE ExtendMode extendMode() const noexcept { return _extendMode; }

  void fetchGradientPixel1(Pixel& dst, PixelFlags flags, const x86::Mem& src) noexcept;
};

//! Linear gradient fetch part.
class FetchLinearGradientPart : public FetchGradientPart {
public:
  struct LinearRegs : public CommonRegs {
    x86::Xmm pt;
    x86::Xmm dt;
    x86::Xmm dt2;
    x86::Xmm py;
    x86::Xmm dy;
    x86::Xmm rep;
    x86::Xmm msk;
    x86::Xmm vIdx;
  };

  BLWrap<LinearRegs> f;
  bool _isRoR;

  FetchLinearGradientPart(PipeCompiler* pc, FetchType fetchType, uint32_t format) noexcept;

  BL_INLINE bool isPad() const noexcept { return !_isRoR; }
  BL_INLINE bool isRoR() const noexcept { return  _isRoR; }

  void preparePart() noexcept override;

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(x86::Gp& x) noexcept override;
  void advanceX(x86::Gp& x, x86::Gp& diff) noexcept override;

  void prefetch1() noexcept override;
  void fetch1(Pixel& p, PixelFlags flags) noexcept override;

  void enterN() noexcept override;
  void leaveN() noexcept override;
  void prefetchN() noexcept override;
  void postfetchN() noexcept override;

  void fetch4(Pixel& p, PixelFlags flags) noexcept override;
  void fetch8(Pixel& p, PixelFlags flags) noexcept override;
};

//! Radial gradient fetch part.
class FetchRadialGradientPart : public FetchGradientPart {
public:
  // `d`   - determinant.
  // `dd`  - determinant delta.
  // `ddd` - determinant-delta delta.
  struct RadialRegs : public CommonRegs {
    x86::Xmm xx_xy;
    x86::Xmm yx_yy;

    x86::Xmm ax_ay;
    x86::Xmm fx_fy;
    x86::Xmm da_ba;

    x86::Xmm d_b;
    x86::Xmm dd_bd;
    x86::Xmm ddx_ddy;

    x86::Xmm px_py;
    x86::Xmm scale;
    x86::Xmm ddd;
    x86::Xmm value;

    x86::Gp maxi;
    x86::Xmm vmaxi; // Maximum table index, basically `precision - 1` (mask).
    x86::Xmm vmaxf; // Like `vmaxi`, but converted to `float`.

    // 4+ pixels.
    x86::Xmm d_b_prev;
    x86::Xmm dd_bd_prev;
  };

  BLWrap<RadialRegs> f;

  FetchRadialGradientPart(PipeCompiler* pc, FetchType fetchType, uint32_t format) noexcept;

  void preparePart() noexcept override;

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(x86::Gp& x) noexcept override;
  void advanceX(x86::Gp& x, x86::Gp& diff) noexcept override;

  void prefetch1() noexcept override;
  void fetch1(Pixel& p, PixelFlags flags) noexcept override;

  void prefetchN() noexcept override;
  void postfetchN() noexcept override;
  void fetch4(Pixel& p, PixelFlags flags) noexcept override;

  void precalc(x86::Xmm& px_py) noexcept;
};

//! Conical gradient fetch part.
class FetchConicalGradientPart : public FetchGradientPart {
public:
  struct ConicalRegs : public CommonRegs {
    x86::Xmm xx_xy;
    x86::Xmm yx_yy;

    x86::Xmm hx_hy;
    x86::Xmm px_py;

    x86::Gp consts;

    x86::Gp maxi;
    x86::Xmm vmaxi; // Maximum table index, basically `precision - 1` (mask).

    // 4+ pixels.
    x86::Xmm xx4_xy4;
    x86::Xmm xx_0123;
    x86::Xmm xy_0123;

    // Temporary.
    x86::Xmm x0, x1, x2, x3, x4, x5;
  };

  BLWrap<ConicalRegs> f;

  FetchConicalGradientPart(PipeCompiler* pc, FetchType fetchType, uint32_t format) noexcept;

  void preparePart() noexcept override;

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(x86::Gp& x) noexcept override;
  void advanceX(x86::Gp& x, x86::Gp& diff) noexcept override;

  void fetch1(Pixel& p, PixelFlags flags) noexcept override;
  void prefetchN() noexcept override;
  void fetch4(Pixel& p, PixelFlags flags) noexcept override;
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHGRADIENTPART_P_H_INCLUDED
