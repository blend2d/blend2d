// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_BLFETCHGRADIENTPART_P_H
#define BLEND2D_PIPEGEN_BLFETCHGRADIENTPART_P_H

#include "../pipegen/blfetchpart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::FetchGradientPart]
// ============================================================================

//! Base class for all gradient fetch parts.
class FetchGradientPart : public FetchPart {
public:
  BL_NONCOPYABLE(FetchGradientPart)

  struct CommonRegs {
    x86::Gp table;
  };

  uint8_t _extend;

  FetchGradientPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;

  //! Get gradient extend.
  inline uint32_t extend() const noexcept { return _extend; }
};

// ============================================================================
// [BLPipeGen::FetchLinearGradientPart]
// ============================================================================

//! Linear gradient fetch part.
class FetchLinearGradientPart : public FetchGradientPart {
public:
  BL_NONCOPYABLE(FetchLinearGradientPart)

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

  FetchLinearGradientPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;

  inline bool isPad() const noexcept { return !_isRoR; }
  inline bool isRoR() const noexcept { return  _isRoR; }

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(x86::Gp& x) noexcept override;
  void advanceX(x86::Gp& x, x86::Gp& diff) noexcept override;

  void prefetch1() noexcept override;
  void fetch1(PixelARGB& p, uint32_t flags) noexcept override;

  void enterN() noexcept override;
  void leaveN() noexcept override;
  void prefetchN() noexcept override;
  void postfetchN() noexcept override;

  void fetch4(PixelARGB& p, uint32_t flags) noexcept override;
  void fetch8(PixelARGB& p, uint32_t flags) noexcept override;
};

// ============================================================================
// [BLPipeGen::FetchRadialGradientPart]
// ============================================================================

//! Radial gradient fetch part.
class FetchRadialGradientPart : public FetchGradientPart {
public:
  BL_NONCOPYABLE(FetchRadialGradientPart)

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

    // These are only used by `radialFetch4()` and restored by `radialLeaveN()`.
    x86::Xmm d_b_prev;
    x86::Xmm dd_bd_prev;
  };

  BLWrap<RadialRegs> f;

  FetchRadialGradientPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(x86::Gp& x) noexcept override;
  void advanceX(x86::Gp& x, x86::Gp& diff) noexcept override;

  void prefetch1() noexcept override;
  void fetch1(PixelARGB& p, uint32_t flags) noexcept override;

  void prefetchN() noexcept override;
  void postfetchN() noexcept override;
  void fetch4(PixelARGB& p, uint32_t flags) noexcept override;

  void precalc(x86::Xmm& px_py) noexcept;
};

// ============================================================================
// [BLPipeGen::FetchConicalGradientPart]
// ============================================================================

//! Conical gradient fetch part.
class FetchConicalGradientPart : public FetchGradientPart {
public:
  BL_NONCOPYABLE(FetchConicalGradientPart)

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

  FetchConicalGradientPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(x86::Gp& x) noexcept override;
  void advanceX(x86::Gp& x, x86::Gp& diff) noexcept override;

  void fetch1(PixelARGB& p, uint32_t flags) noexcept override;
  void prefetchN() noexcept override;
  void fetch4(PixelARGB& p, uint32_t flags) noexcept override;
};

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_BLFETCHGRADIENTPART_P_H
