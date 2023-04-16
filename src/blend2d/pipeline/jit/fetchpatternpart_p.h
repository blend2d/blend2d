// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHPATTERNPART_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHPATTERNPART_P_H_INCLUDED

#include "../../pipeline/jit/fetchpart_p.h"
#include "../../support/wrap_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

//! Base class for all pattern fetch parts.
class FetchPatternPart : public FetchPart {
public:
  //! Common registers (used by all fetch types).
  struct CommonRegs {
    //! Pattern width (32-bit).
    x86::Gp w;
    //! Pattern height (32-bit).
    x86::Gp h;
    //! Pattern pixels (pointer to the first scanline).
    x86::Gp srctop;
    //! Pattern stride.
    x86::Gp stride;
    //! Pattern stride (original value, used by PatternSimple only).
    x86::Gp strideOrig;
    //! Pointer to the previous scanline and/or pixel (fractional).
    x86::Gp srcp0;
    //! Pointer to the current scanline and/or pixel (aligned).
    x86::Gp srcp1;
  };

  //! How many bits to shift the `x` index to get the address to the pixel. If this value is 0xFF it means that
  //! shifting is not possible or that the pixel was already pre-shifted.
  uint8_t _idxShift = 0xFFu;

  //! Extend in X direction, used only by `FetchSimplePatternPart`.
  ExtendMode _extendX {};

  FetchPatternPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept;

  //! Tests whether the fetch-type is simple pattern {axis-aligned or axis-unaligned}.
  BL_INLINE bool isSimple() const noexcept { return isFetchType(FetchType::kPatternSimpleFirst, FetchType::kPatternSimpleLast); }
  //! Tests whether the fetch-type is an affine pattern style.
  BL_INLINE bool isAffine() const noexcept { return isFetchType(FetchType::kPatternAffineFirst, FetchType::kPatternAffineLast); }
};

//! Simple pattern fetch part.
//!
//! Simple pattern fetch doesn't do scaling or affine transformations, however, can perform fractional pixel
//! translation described as Fx and Fy values.
class FetchSimplePatternPart : public FetchPatternPart {
public:
  //! Aligned and fractional blits.
  struct SimpleRegs : public CommonRegs {
    //! X position.
    x86::Gp x;
    //! Y position (counter, decreases to zero).
    x86::Gp y;

    //! X repeat/reflect.
    x86::Gp rx;
    //! Y repeat/reflect.
    x86::Gp ry;

    //! X padded to [0-W) range.
    x86::Gp xPadded;
    //! X origin, assigned to `x` at the beginning of each scanline.
    x86::Gp xOrigin;
    //! X restart (used by scalar implementation, points to either -W or 0).
    x86::Gp xRestart;

    //! Last loaded pixel (or combined pixel) of the first (srcp0) scanline.
    x86::Xmm pixL;

    // Weights used in RGBA mode.
    x86::Xmm wb_wb;
    x86::Xmm wd_wd;
    x86::Xmm wa_wb;
    x86::Xmm wc_wd;

    // Weights used in alpha-only mode.
    x86::Xmm wd_wb;
    x86::Xmm wa_wc;
    x86::Xmm wb_wd;

    //! X position vector  `[  x, x+1, x+2, x+3]`.
    x86::Xmm xVec4;
    //! X setup vector     `[  0,   1,   2,   3]`.
    x86::Xmm xSet4;
    //! X increment vector `[  4,   4,   4,   4]`.
    x86::Xmm xInc4;
    //! X normalize vector.
    x86::Xmm xNrm4;
    //! X maximum vector   `[max, max, max, max]`.
    x86::Xmm xMax4;
  };

  BLWrap<SimpleRegs> f;

  FetchSimplePatternPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept;

  //! Tests whether the fetch-type is axis-aligned blit (no extend modes, no overflows)
  BL_INLINE bool isAlignedBlit() const noexcept { return isFetchType(FetchType::kPatternAlignedBlit); }
  //! Tests whether the fetch-type is axis-aligned pattern.
  BL_INLINE bool isPatternAligned() const noexcept { return isFetchType(FetchType::kPatternAlignedFirst, FetchType::kPatternAlignedLast); }
  //! Tests whether the fetch-type is a "FracBi" pattern style.
  BL_INLINE bool isPatternUnaligned() const noexcept { return isFetchType(FetchType::kPatternUnalignedFirst, FetchType::kPatternUnalignedLast); }
  //! Tests whether the fetch-type is a "FracBiX" pattern style.
  BL_INLINE bool isPatternFx() const noexcept { return isFetchType(FetchType::kPatternFxFirst, FetchType::kPatternFxLast); }
  //! Tests whether the fetch-type is a "FracBiY" pattern style.
  BL_INLINE bool isPatternFy() const noexcept { return isFetchType(FetchType::kPatternFyFirst, FetchType::kPatternFyLast); }
  //! Tests whether the fetch-type is a "FracBiXY" pattern style.
  BL_INLINE bool isPatternFxFy() const noexcept { return isFetchType(FetchType::kPatternFxFyFirst, FetchType::kPatternFxFyLast); }

  //! Tests whether the fetch is pattern style that has fractional `x` or `x & y`.
  BL_INLINE bool hasFracX() const noexcept { return isPatternFx() || isPatternFxFy(); }
  //! Tests whether the fetch is pattern style that has fractional `y` or `x & y`.
  BL_INLINE bool hasFracY() const noexcept { return isPatternFy() || isPatternFxFy(); }

  //! Returns the extend-x mode.
  BL_INLINE ExtendMode extendX() const noexcept { return _extendX; }

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const x86::Gp& x) noexcept override;
  void advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept override;

  void advanceXByOne() noexcept;
  void repeatOrReflectX() noexcept;
  void prefetchAccX() noexcept;

  void enterN() noexcept override;
  void leaveN() noexcept override;
  void prefetchN() noexcept override;
  void postfetchN() noexcept override;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;
};

//! Affine pattern fetch part.
class FetchAffinePatternPart : public FetchPatternPart {
public:
  struct AffineRegs : public CommonRegs {
    //! Horizontal X/Y increments.
    x86::Xmm xx_xy;
    //! Vertical X/Y increments.
    x86::Xmm yx_yy;
    x86::Xmm tx_ty;
    x86::Xmm px_py;
    x86::Xmm ox_oy;
    //! Normalization after `px_py` gets out of bounds.
    x86::Xmm rx_ry;
    //! Like `px_py` but one pixel ahead [fetch4].
    x86::Xmm qx_qy;
    //! Advance twice (like `xx_xy`, but doubled) [fetch4].
    x86::Xmm xx2_xy2;

    //! Pad minimum coords.
    x86::Xmm minx_miny;
    //! Pad maximum coords.
    x86::Xmm maxx_maxy;
    //! Correction values (bilinear only).
    x86::Xmm corx_cory;
    //! Pattern width and height as doubles.
    x86::Xmm tw_th;

    //! Vector of pattern indexes.
    x86::Xmm vIdx;
    //! Vector containing multipliers for Y/X pairs.
    x86::Xmm vAddrMul;
  };

  enum ClampStep : uint32_t {
    kClampStepA_NN,
    kClampStepA_BI,

    kClampStepB_NN,
    kClampStepB_BI,

    kClampStepC_NN,
    kClampStepC_BI
  };

  BLWrap<AffineRegs> f;

  FetchAffinePatternPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept;

  BL_INLINE bool isAffineNn() const noexcept { return isFetchType(FetchType::kPatternAffineNNAny) || isFetchType(FetchType::kPatternAffineNNOpt); }
  BL_INLINE bool isAffineBi() const noexcept { return isFetchType(FetchType::kPatternAffineBIAny) || isFetchType(FetchType::kPatternAffineBIOpt); }
  BL_INLINE bool isOptimized() const noexcept { return isFetchType(FetchType::kPatternAffineNNOpt) || isFetchType(FetchType::kPatternAffineBIOpt); }

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const x86::Gp& x) noexcept override;
  void advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept override;

  void advancePxPy(x86::Xmm& px_py, const x86::Gp& i) noexcept;
  void normalizePxPy(x86::Xmm& px_py) noexcept;
  void clampVIdx32(x86::Xmm& dst, const x86::Xmm& src, ClampStep step) noexcept;

  void prefetch1() noexcept override;
  void enterN() noexcept override;
  void leaveN() noexcept override;
  void prefetchN() noexcept override;
  void postfetchN() noexcept override;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHPATTERNPART_P_H_INCLUDED
