// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHGRADIENTPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHGRADIENTPART_P_H_INCLUDED

#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchutilspixelgather_p.h"
#include "../../support/wrap_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {

class GradientDitheringContext {
public:
  //! \name Members
  //! \{

  PipeCompiler* pc {};
  bool _isRectFill {};
  Gp _dmPosition;
  Gp _dmOriginX;
  Vec _dmValues;

  //! \}

  BL_INLINE explicit GradientDitheringContext(PipeCompiler* pc) noexcept
    : pc(pc) {}

  //! Returns whether this dithering context is used in a rectangular fill.
  BL_INLINE_NODEBUG bool isRectFill() const noexcept { return _isRectFill; }

  void initY(const PipeFunction& fn, const Gp& x, const Gp& y) noexcept;
  void advanceY() noexcept;

  void startAtX(const Gp& x) noexcept;
  void advanceX(const Gp& x, const Gp& diff, bool diffWithinBounds) noexcept;
  void advanceXAfterFetch(uint32_t n) noexcept;

  void ditherUnpackedPixels(Pixel& p, AdvanceMode advanceMode) noexcept;
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

  BL_INLINE_NODEBUG void setDitheringEnabled(bool value) noexcept {
    _ditheringEnabled = value;
    if (value)
      _partFlags |= PipePartFlags::kAdvanceXNeedsX;
  }

  BL_INLINE_NODEBUG int tablePtrShift() const noexcept { return _ditheringEnabled ? 3 : 2; }

  void fetchSinglePixel(Pixel& dst, PixelFlags flags, const Gp& idx) noexcept;

  void fetchMultiplePixels(Pixel& dst, PixelCount n, PixelFlags flags, const Vec& idx, FetchUtils::IndexLayout indexLayout, GatherMode mode, InterleaveCallback cb, void* cbData) noexcept;

  inline void fetchMultiplePixels(Pixel& dst, PixelCount n, PixelFlags flags, const Vec& idx, FetchUtils::IndexLayout indexLayout, GatherMode mode) noexcept {
    fetchMultiplePixels(dst, n, flags, idx, indexLayout, mode, dummyInterleaveCallback, nullptr);
  }

  template<class InterleaveFunc>
  inline void fetchMultiplePixels(Pixel& dst, PixelCount n, PixelFlags flags, const Vec& idx, FetchUtils::IndexLayout indexLayout, GatherMode mode, InterleaveFunc&& interleaveFunc) noexcept {
    fetchMultiplePixels(dst, n, flags, idx, indexLayout, mode, [](uint32_t step, void* data) noexcept {
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

  BL_INLINE_NODEBUG VecWidth vecWidth() const noexcept { return blMin(pc->vecWidth(), VecWidth::k256); }

  void preparePart() noexcept override;

  void _initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const Gp& x) noexcept override;
  void advanceX(const Gp& x, const Gp& diff) noexcept override;
  void advanceX(const Gp& x, const Gp& diff, bool diffWithinBounds) noexcept;
  void calcAdvanceX(const Vec& dst, const Gp& diff) const noexcept;

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
    Vec ty_tx;
    Vec yy_yx;

    Vec dd0_b0;
    Vec ddy_by;

    Vec vy;
    Vec inv2a_4a;
    Vec sqinv2a_sqfr;

    Vec d;
    Vec b;
    Vec dd;
    Vec vx;
    Vec vx_start;
    Vec value;

    Vec bd;
    Vec ddd;

    Vec vmaxi;
    Vec vrori;
  };

  Wrap<RadialRegs> f;

  FetchRadialGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept;

  BL_INLINE_NODEBUG VecWidth vecWidth() const noexcept { return blMin(pc->vecWidth(), VecWidth::k256); }

  void preparePart() noexcept override;

  void _initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const Gp& x) noexcept override;
  void advanceX(const Gp& x, const Gp& diff) noexcept override;
  void advanceX(const Gp& x, const Gp& diff, bool diffWithinBounds) noexcept;

  void prefetchN() noexcept override;
  void postfetchN() noexcept override;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;

  void initVx(const Vec& vx, const Gp& x) noexcept;
  FetchUtils::IndexLayout applyExtend(const Vec& idx0, const Vec& idx1, const Vec& tmp) noexcept;
};

//! Conic gradient fetch part.
class FetchConicGradientPart : public FetchGradientPart {
public:
  static constexpr uint8_t kQ0 = 0;
  static constexpr uint8_t kQ1 = 1;
  static constexpr uint8_t kQ2 = 2;
  static constexpr uint8_t kQ3 = 3;

  static constexpr uint8_t kNDiv1 = 0;
  static constexpr uint8_t kNDiv2 = 1;
  static constexpr uint8_t kNDiv4 = 2;
  static constexpr uint8_t kAngleOffset = 3;

  struct ConicRegs {
    Vec ty_tx;
    Vec yy_yx;

    Vec tx;
    Vec xx;
    Vec vx;
    Vec vx_start;

    Vec ay;
    Vec by;

    Vec q_coeff;
    Vec n_coeff;

    Vec maxi;
    Vec rori;
  };

  Wrap<ConicRegs> f;

  FetchConicGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept;

  BL_INLINE_NODEBUG VecWidth vecWidth(uint32_t nPixels) const noexcept {
    return blMin(pc->vecWidth(), VecWidth(nPixels >> 3));
  }

  void preparePart() noexcept override;

  void _initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const Gp& x) noexcept override;
  void advanceX(const Gp& x, const Gp& diff) noexcept override;
  void advanceX(const Gp& x, const Gp& diff, bool diffWithinBounds) noexcept;

  void prefetchN() noexcept override;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;

  void initVx(const Vec& vx, const Gp& x) noexcept;
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHGRADIENTPART_P_H_INCLUDED
