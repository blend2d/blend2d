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

namespace bl {
namespace Pipeline {
namespace JIT {

//! Base class for all pattern fetch parts.
class FetchPatternPart : public FetchPart {
public:
  //! How many bits to shift the `x` index to get the address to the pixel. If this value is 0xFF it means that
  //! shifting is not possible or that the pixel was already pre-shifted.
  uint8_t _idxShift = 0xFFu;

  //! Extend in X direction, used only by `FetchSimplePatternPart`.
  ExtendMode _extendX {};

  FetchPatternPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept;

  //! Tests whether the fetch-type is simple pattern {axis-aligned or axis-unaligned}.
  BL_INLINE_NODEBUG bool isSimple() const noexcept { return isFetchType(FetchType::kPatternSimpleFirst, FetchType::kPatternSimpleLast); }
  //! Tests whether the fetch-type is an affine pattern style.
  BL_INLINE_NODEBUG bool isAffine() const noexcept { return isFetchType(FetchType::kPatternAffineFirst, FetchType::kPatternAffineLast); }
};

//! Simple pattern fetch part.
//!
//! Simple pattern fetch doesn't do scaling or affine transformations, however, can perform fractional pixel
//! translation described as Fx and Fy values.
class FetchSimplePatternPart : public FetchPatternPart {
public:
  //! Aligned and fractional blits.
  struct SimpleRegs {
    //! Pointer to the previous scanline and/or pixel (fractional).
    Gp srcp0;
    //! Pointer to the current scanline and/or pixel (aligned).
    Gp srcp1;
    //! Pattern stride, used only by aligned blits.
    Gp stride;

    //! Vertical extend data.
    Mem vExtendData;

    //! X position.
    Gp x;
    //! Y position (counter, decreases to zero).
    Gp y;

    //! Pattern width (32-bit).
    Gp w;
    //! Pattern height (32-bit).
    Gp h;

    //! X repeat/reflect.
    Gp rx;
    //! Y repeat/reflect.
    Gp ry;

    //! X padded to [0-W) range.
    Gp xPadded;
    //! X origin, assigned to `x` at the beginning of each scanline.
    Gp xOrigin;
    //! X restart (used by scalar implementation, points to either -W or 0).
    Gp xRestart;

    //! Last loaded pixel (or combined pixel) of the first (srcp0) scanline.
    Vec pixL;

    // Weights used in RGBA mode.
    Vec wa, wb, wc, wd;

    Vec wa_wb;
    Vec wc_wd;

    // Weights used in alpha-only mode.
    Vec wd_wb;
    Vec wa_wc;
    Vec wb_wd;

    //! X position vector  `[  x, x+1, x+2, x+3]`.
    Vec xVec4;
    //! X setup vector     `[  0,   1,   2,   3]`.
    Vec xSet4;
    //! X increment vector `[  4,   4,   4,   4]`.
    Vec xInc4;
    //! X normalize vector.
    Vec xNrm4;
    //! X maximum vector   `[max, max, max, max]`.
    Vec xMax4;
  };

  Wrap<SimpleRegs> f;

  FetchSimplePatternPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept;

  //! Tests whether the fetch-type is axis-aligned blit (no extend modes, no overflows)
  BL_INLINE_NODEBUG bool isAlignedBlit() const noexcept { return isFetchType(FetchType::kPatternAlignedBlit); }
  //! Tests whether the fetch-type is axis-aligned pattern.
  BL_INLINE_NODEBUG bool isPatternAligned() const noexcept { return isFetchType(FetchType::kPatternAlignedFirst, FetchType::kPatternAlignedLast); }
  //! Tests whether the fetch-type is a "FracBi" pattern style.
  BL_INLINE_NODEBUG bool isPatternUnaligned() const noexcept { return isFetchType(FetchType::kPatternUnalignedFirst, FetchType::kPatternUnalignedLast); }
  //! Tests whether the fetch-type is a "FracBiX" pattern style.
  BL_INLINE_NODEBUG bool isPatternFx() const noexcept { return isFetchType(FetchType::kPatternFxFirst, FetchType::kPatternFxLast); }
  //! Tests whether the fetch-type is a "FracBiY" pattern style.
  BL_INLINE_NODEBUG bool isPatternFy() const noexcept { return isFetchType(FetchType::kPatternFyFirst, FetchType::kPatternFyLast); }
  //! Tests whether the fetch-type is a "FracBiXY" pattern style.
  BL_INLINE_NODEBUG bool isPatternFxFy() const noexcept { return isFetchType(FetchType::kPatternFxFyFirst, FetchType::kPatternFxFyLast); }

  //! Tests whether the fetch is pattern style that has fractional `x` or `x & y`.
  BL_INLINE_NODEBUG bool hasFracX() const noexcept { return isPatternFx() || isPatternFxFy(); }
  //! Tests whether the fetch is pattern style that has fractional `y` or `x & y`.
  BL_INLINE_NODEBUG bool hasFracY() const noexcept { return isPatternFy() || isPatternFxFy(); }

  //! Returns the extend-x mode.
  BL_INLINE_NODEBUG ExtendMode extendX() const noexcept { return _extendX; }

  void _initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void swapStrideStopData(VecArray& v) noexcept;

  void advanceY() noexcept override;
  void startAtX(const Gp& x) noexcept override;
  void advanceX(const Gp& x, const Gp& diff) noexcept override;

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
  struct AffineRegs {
    //! Pattern pixels (pointer to the first scanline).
    Gp srctop;
    //! Pattern stride.
    Gp stride;

    //! Horizontal X/Y increments.
    Vec xx_xy;
    //! Vertical X/Y increments.
    Vec yx_yy;
    Vec tx_ty;
    Vec px_py;
    Vec ox_oy;
    //! Normalization after `px_py` gets out of bounds.
    Vec rx_ry;
    //! Like `px_py` but one pixel ahead [fetch4].
    Vec qx_qy;
    //! Advance twice (like `xx_xy`, but doubled) [fetch4].
    Vec xx2_xy2;

    //! Pad minimum coords.
    Vec minx_miny;
    //! Pad maximum coords.
    Vec maxx_maxy;
    //! Correction values (bilinear only).
    Vec corx_cory;
    //! Pattern width and height as doubles.
    Vec tw_th;

    //! Vector of pattern indexes.
    Vec vIdx;
    //! Vector containing multipliers for Y/X pairs.
    Vec vAddrMul;
  };

  enum ClampStep : uint32_t {
    kClampStepA_NN,
    kClampStepA_BI,

    kClampStepB_NN,
    kClampStepB_BI,

    kClampStepC_NN,
    kClampStepC_BI
  };

  Wrap<AffineRegs> f;

  FetchAffinePatternPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept;

  BL_INLINE_NODEBUG bool isAffineNN() const noexcept { return isFetchType(FetchType::kPatternAffineNNAny) || isFetchType(FetchType::kPatternAffineNNOpt); }
  BL_INLINE_NODEBUG bool isAffineBI() const noexcept { return isFetchType(FetchType::kPatternAffineBIAny) || isFetchType(FetchType::kPatternAffineBIOpt); }
  BL_INLINE_NODEBUG bool isOptimized() const noexcept { return isFetchType(FetchType::kPatternAffineNNOpt) || isFetchType(FetchType::kPatternAffineBIOpt); }

  void _initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(const Gp& x) noexcept override;
  void advanceX(const Gp& x, const Gp& diff) noexcept override;

  void advancePxPy(Vec& px_py, const Gp& i) noexcept;
  void normalizePxPy(Vec& px_py) noexcept;
  void clampVIdx32(Vec& dst, const Vec& src, ClampStep step) noexcept;

  void enterN() noexcept override;
  void leaveN() noexcept override;
  void prefetchN() noexcept override;
  void postfetchN() noexcept override;

  void fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept override;
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHPATTERNPART_P_H_INCLUDED
