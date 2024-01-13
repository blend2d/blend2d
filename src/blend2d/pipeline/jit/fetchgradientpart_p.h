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

namespace bl {
namespace Pipeline {
namespace JIT {

class GradientDitheringContext {
public:
  PipeCompiler* pc {};
  bool _isRectFill {};
  Gp _dmPosition;
  Gp _dmOriginX;
  Vec _dmValues;

  BL_INLINE explicit GradientDitheringContext(PipeCompiler* pc) noexcept
    : pc(pc) {}

  BL_INLINE_NODEBUG bool isRectFill() const noexcept { return _isRectFill; }

  void initY(const Gp& x, const Gp& y) noexcept;
  void advanceY() noexcept;

  void startAtX(const Gp& x) noexcept;
  void advanceX(const Gp& x, const Gp& diff) noexcept;
  void advanceXAfterFetch(uint32_t n) noexcept;

  void ditherUnpackedPixels(Pixel& p) noexcept;
};

//! Base class for all gradient fetch parts.
class FetchGradientPart : public FetchPart {
public:
  ExtendMode _extendMode {};
  bool _ditheringEnabled {};

  Gp _tablePtr;
  GradientDitheringContext _ditheringContext;

  FetchGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept;

  //! Returns the gradient extend mode.
  BL_INLINE_NODEBUG ExtendMode extendMode() const noexcept { return _extendMode; }

  //! Returns true if the gradient extend mode is Pad.
  BL_INLINE_NODEBUG bool isPad() const noexcept { return _extendMode == ExtendMode::kPad; }
  //! Returns true if the gradient extend mode is RoR.
  BL_INLINE_NODEBUG bool isRoR() const noexcept { return _extendMode == ExtendMode::kRoR; }

  BL_INLINE_NODEBUG bool ditheringEnabled() const noexcept { return _ditheringEnabled; }
  BL_INLINE_NODEBUG void setDitheringEnabled(bool value) noexcept { _ditheringEnabled = value; }

  BL_INLINE_NODEBUG int tablePtrShift() const noexcept { return _ditheringEnabled ? 3 : 2; }

  void fetchSinglePixel(Pixel& dst, PixelFlags flags, const Gp& idx) noexcept;

  void fetchMultiplePixels(Pixel& dst, PixelCount n, PixelFlags flags, const Vec& idx, IndexLayout indexLayout, InterleaveCallback cb, void* cbData) noexcept;

  template<class InterleaveFunc>
  void fetchMultiplePixels(Pixel& dst, PixelCount n, PixelFlags flags, const Vec& idx, IndexLayout indexLayout, InterleaveFunc&& interleaveFunc) noexcept {
    fetchMultiplePixels(dst, n, flags, idx, indexLayout, [](uint32_t step, void* data) noexcept {
      (*static_cast<const InterleaveFunc*>(data))(step);
    }, (void*)&interleaveFunc);
  }
};

//! Linear gradient fetch part.
class FetchLinearGradientPart : public FetchGradientPart {
public:
  struct LinearRegs {
    Gp dtGp;
    Vec pt;
    Vec dt;
    Vec dtN;
    Vec py;
    Vec dy;
    Vec maxi;
    Vec rori;
    Vec vIdx;
  };

  Wrap<LinearRegs> f;

  FetchLinearGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept;

  void preparePart() noexcept override;

  void _initPart(Gp& x, Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const Gp& x) noexcept override;
  void advanceX(const Gp& x, const Gp& diff) noexcept override;
  void calcAdvanceX(const Vec& dst, const Gp& diff) const noexcept;

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
    Vec xx_xy;
    Vec yx_yy;

    Vec ax_ay;
    Vec fx_fy;
    Vec da_ba;

    Vec d_b;
    Vec dd_bd;
    Vec ddx_ddy;

    Vec px_py;
    Vec scale;
    Vec ddd;
    Vec value;

    Vec vmaxi;
    Vec vrori;
    Vec vmaxf; // Like `vmaxi`, but converted to `float`.

    // 4+ pixels.
    Vec d_b_prev;
    Vec dd_bd_prev;
  };

  Wrap<RadialRegs> f;

  FetchRadialGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept;

  void preparePart() noexcept override;

  void _initPart(Gp& x, Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const Gp& x) noexcept override;
  void advanceX(const Gp& x, const Gp& diff) noexcept override;

  void prefetch1() noexcept override;
  void prefetchN() noexcept override;
  void postfetchN() noexcept override;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;

  void precalc(const Vec& px_py) noexcept;
};

//! Conic gradient fetch part.
class FetchConicGradientPart : public FetchGradientPart {
public:
  struct ConicRegs {
    Gp consts;

    Vec px;
    Vec xx;
    Vec hx_hy;
    Vec yx_yy;
    Vec ay;
    Vec by;
    Vec angleOffset;
    Vec maxi; // Maximum table index, basically `precision - 1` (mask).

    // Temporary values precalculated for the next fetch loop.
    Vec t0, t1, t2;

#if defined(BL_JIT_ARCH_X86)
    KReg t1Pred;
#endif // BL_JIT_ARCH_X86

    // 4+ pixels.
    Vec xx_inc;
    Vec xx_off;
  };

  Wrap<ConicRegs> f;

  FetchConicGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept;

  void preparePart() noexcept override;

  void _initPart(Gp& x, Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const Gp& x) noexcept override;
  void advanceX(const Gp& x, const Gp& diff) noexcept override;

  void recalcX() noexcept;

  void prefetchN() noexcept override;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHGRADIENTPART_P_H_INCLUDED
