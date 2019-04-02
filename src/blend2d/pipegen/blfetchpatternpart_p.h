// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_BLFETCHPATTERNPART_P_H
#define BLEND2D_PIPEGEN_BLFETCHPATTERNPART_P_H

#include "../pipegen/blfetchpart_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::FetchPatternPart]
// ============================================================================

//! Base class for all pattern fetch parts.
class FetchPatternPart : public FetchPart {
public:
  BL_NONCOPYABLE(FetchPatternPart)

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

  FetchPatternPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;

  //! Get whether the fetch-type is simple pattern {axis-aligned or axis-unaligned}.
  inline bool isSimple() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_SIMPLE_FIRST, BL_PIPE_FETCH_TYPE_PATTERN_SIMPLE_LAST); }
  //! Get whether the fetch-type is an affine pattern style.
  inline bool isAffine() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_FIRST, BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_LAST); }
};

// ============================================================================
// [BLPipeGen::FetchSimplePatternPart]
// ============================================================================

//! Simple pattern fetch part.
//!
//! Simple pattern fetch doesn't do scaling or affine transformations, however,
//! can perform fractional pixel translation described as Fx and Fy values.
class FetchSimplePatternPart : public FetchPatternPart {
public:
  BL_NONCOPYABLE(FetchSimplePatternPart)

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

    //! Last loaded pixel (or combined pixel) of the first  (srcp0) scanline.
    x86::Xmm pixL;

    x86::Xmm wb_wb;
    x86::Xmm wd_wd;
    x86::Xmm wa_wb;
    x86::Xmm wc_wd;

    // Only used by fetchN.

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

  uint8_t _extendX;
  BLWrap<SimpleRegs> f;

  FetchSimplePatternPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;

  //! Get whether the fetch-type is axis-aligned blit (no extend modes, no overflows)
  inline bool isBlitA() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT); }
  //! Get whether the fetch-type is axis-aligned pattern.
  inline bool isPatternA() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AA_FIRST, BL_PIPE_FETCH_TYPE_PATTERN_AA_LAST); }
  //! Get whether the fetch-type is a "FracBi" pattern style.
  inline bool isPatternF() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AU_FIRST, BL_PIPE_FETCH_TYPE_PATTERN_AU_LAST); }
  //! Get whether the fetch-type is a "FracBiX" pattern style.
  inline bool isPatternFx() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_FX_FIRST, BL_PIPE_FETCH_TYPE_PATTERN_FX_LAST); }
  //! Get whether the fetch-type is a "FracBiY" pattern style.
  inline bool isPatternFy() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_FY_FIRST, BL_PIPE_FETCH_TYPE_PATTERN_FY_LAST); }
  //! Get whether the fetch-type is a "FracBiXY" pattern style.
  inline bool isPatternFxFy() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_FIRST, BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_LAST); }

  //! Get whether the fetch is pattern style that has fractional `x` or `x & y`.
  inline bool hasFracX() const noexcept { return isPatternFx() || isPatternFxFy(); }
  //! Get whether the fetch is pattern style that has fractional `y` or `x & y`.
  inline bool hasFracY() const noexcept { return isPatternFy() || isPatternFxFy(); }

  //! Get extend-x mode.
  inline uint32_t extendX() const noexcept { return _extendX; }

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(x86::Gp& x) noexcept override;
  void advanceX(x86::Gp& x, x86::Gp& diff) noexcept override;

  void advanceXByOne() noexcept;
  void repeatOrReflectX() noexcept;
  void prefetchAccX() noexcept;

  // NOTE: We don't do prefetch here. Since the prefetch we need is the same
  // for `prefetch1()` and `prefetchN()` we always prefetch by `prefetchAccX()`
  // during `startAtX()` and `advanceX()`.
  void fetch1(PixelARGB& p, uint32_t flags) noexcept override;

  void enterN() noexcept override;
  void leaveN() noexcept override;
  void prefetchN() noexcept override;
  void postfetchN() noexcept override;
  void fetch4(PixelARGB& p, uint32_t flags) noexcept override;
  void fetch8(PixelARGB& p, uint32_t flags) noexcept override;
};

// ============================================================================
// [BLPipeGen::FetchAffinePatternPart]
// ============================================================================

//! Affine pattern fetch part.
class FetchAffinePatternPart : public FetchPatternPart {
public:
  BL_NONCOPYABLE(FetchAffinePatternPart)

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
    //! Correction .
    x86::Xmm corx_cory;
    //! Pattern width and height as doubles.
    x86::Xmm tw_th;

    //! Vector of pattern indexes.
    x86::Xmm vIdx;
    //! Vector containing multipliers for Y/X pairs.
    x86::Xmm vAddrMul;
  };

  BLWrap<AffineRegs> f;

  FetchAffinePatternPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;

  inline bool isAffineNn() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_ANY) | isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_OPT); }
  inline bool isAffineBi() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_ANY) | isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_OPT); }
  inline bool isOptimized() const noexcept { return isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_OPT) | isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_OPT); }

  void _initPart(x86::Gp& x, x86::Gp& y) noexcept override;
  void _finiPart() noexcept override;

  void advanceY() noexcept override;
  void startAtX(x86::Gp& x) noexcept override;
  void advanceX(x86::Gp& x, x86::Gp& diff) noexcept override;

  void advancePxPy(x86::Xmm& px_py, const x86::Gp& i) noexcept;
  void normalizePxPy(x86::Xmm& px_py) noexcept;

  void prefetch1() noexcept override;
  void fetch1(PixelARGB& p, uint32_t flags) noexcept override;

  void enterN() noexcept override;
  void leaveN() noexcept override;
  void prefetchN() noexcept override;
  void postfetchN() noexcept override;
  void fetch4(PixelARGB& p, uint32_t flags) noexcept override;

  enum ClampStep : uint32_t {
    kClampStepA_NN,
    kClampStepA_BI,

    kClampStepB_NN,
    kClampStepB_BI,

    kClampStepC_NN,
    kClampStepC_BI
  };

  void clampVIdx32(x86::Xmm& dst, const x86::Xmm& src, uint32_t step) noexcept;
};

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_BLFETCHPATTERNPART_P_H
