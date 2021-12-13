// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_PIPEDEFS_P_H_INCLUDED
#define BLEND2D_PIPELINE_PIPEDEFS_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../format_p.h"
#include "../gradient_p.h"
#include "../matrix_p.h"
#include "../pattern_p.h"
#include "../runtime_p.h"
#include "../tables_p.h"
#include "../simd_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \namespace BLPipeline
//!
//! Blend2D pipeline.

//! \namespace BLPipeline::JIT
//!
//! Blend2D JIT pipeline compiler.

//! Global constants used by pipeline and affecting also rasterizers.
enum BLPipeGlobalConsts : uint32_t {
  //! How many pixels are represented by a single bit of a `BLBitWord`.
  //!
  //! This is a hardcoded value as it's required by both rasterizer and compositor. Before establishing `4` the values
  //! [4, 8, 16, 32] were tested. Candidates were `4` and `8` where `8` sometimes surpassed `4` in specific workloads,
  //! but `4` was stable across all tests.
  //!
  //! In general increasing `BL_PIPE_PIXELS_PER_ONE_BIT` would result in less memory consumed by bit vectors, but would
  //! increase the work compositors have to do to process cells produced by analytic rasterizer.
  BL_PIPE_PIXELS_PER_ONE_BIT = 4
};

namespace BLPipeline {

struct ContextData;
struct DispatchData;
struct FillData;
struct FetchData;

//! 8-bit alpha constants used by the pipeline and rasterizers.
struct A8Info {
  enum : uint32_t {
    kShift = 8,
    kScale = 1 << kShift,
    kMask = kScale - 1
  };
};

//! Pipeline fill-type.
//!
//! A unique id describing how a mask of each composited pixel is calculated.
enum class FillType : uint8_t {
  //! None or uninitialized.
  kNone = 0,
  //! Fill axis-aligned box.
  kBoxA = 1,
  //! Fill axis-unaligned box.
  kBoxU = 2,
  //! Fill mask command list.
  kMask = 3,
  //! Fill analytic non-zero/even-odd.
  kAnalytic = 4,

  //! Maximum value FillType can have.
  _kMaxValue = 4
};

//! Pipeline extend modes (non-combined).
//!
//! Pipeline sees extend modes a bit differently than the public API in most cases.
enum class ExtendMode : uint8_t {
  //! Pad, same as `BL_EXTEND_MODE_PAD`.
  kPad = 0,
  //! Repeat, same as `BL_EXTEND_MODE_REPEAT`.
  kRepeat = 1,
  //! Reflect, same as `BL_EXTEND_MODE_REFLECT`.
  kReflect = 2,
  //! Repeat-or-reflect (the same code-path for both cases).
  kRoR = 3,

  //! Maximum value the ExtendMode can have.
  _kMaxValue = 3
};

//! Mask command type.
//!
//! CMask must have the value 0.
enum class MaskCommandType : uint32_t {
  //! Constant mask, already multiplied with global alpha.
  kCMask = 0,
  //! Variable mask, already multiplied with global alpha.
  kA8 = 1,
  //! Variable mask, which was not multiplied with global alpha.
  kA8WithoutGA = 2,

  kFinishBitMask = 0x4,
  kEnd = kFinishBitMask | 0,
  kRepeat = kFinishBitMask | 1
};

//! Fill rule mask used during composition of mask produced by analytic-rasterizer.
//!
//! See blfillpart.cpp how this is used. What you see in these values is
//! mask shifted left by one bit as we expect such values in the pipeline.
enum class FillRuleMask : uint32_t {
  kNonZeroMask = uint32_t(0xFFFFFFFFu << 1),
  kEvenOddMask = uint32_t(0x000001FFu << 1)
};

//! Pipeline fetch-type.
//!
//! A unique id describing how pixels are fetched - supported fetchers include solid pixels, patterns (sometimes
//! referred as blits), and gradients.
//!
//! \note RoR is a shurtcut for repeat-or-reflect - a universal fetcher for both.
enum class FetchType : uint8_t {
  //! Solid fetch.
  kSolid = 0,

  //!< Pattern {aligned} (blit) [Base].
  kPatternAlignedBlit,
  //!< Pattern {aligned} (pad-x) [Base].
  kPatternAlignedPad,
  //!< Pattern {aligned} (repeat-large-x) [Optimized].
  kPatternAlignedRepeat,
  //!< Pattern {aligned} (ror-x) [Base].
  kPatternAlignedRoR,

  //!< Pattern {frac-x} (pad-x) [Optimized].
  kPatternFxPad,
  //!< Pattern {frac-x} (ror-x) [Optimized].
  kPatternFxRoR,
  //!< Pattern {frac-y} (pad-x) [Optimized].
  kPatternFyPad,
  //!< Pattern {frac-x} (ror-x) [Optimized].
  kPatternFyRoR,
  //!< Pattern {frac-xy} (pad-x) [Base].
  kPatternFxFyPad,
  //!< Pattern {frac-xy} (ror-x) [Base].
  kPatternFxFyRoR,

  //!< Pattern {affine-nearest}  (any) [Base].
  kPatternAffineNNAny,
  //!< Pattern {affine-nearest}  (any) [Optimized].
  kPatternAffineNNOpt,
  //!< Pattern {affine-bilinear} (any) [Base].
  kPatternAffineBIAny,
  //!< Pattern {affine-bilinear} (any) [Optimized].
  kPatternAffineBIOpt,

  //!< Linear gradient (pad) [Base].
  kGradientLinearPad,
  //!< Linear gradient (ror) [Base].
  kGradientLinearRoR,

  //!< Radial gradient (pad) [Base].
  kGradientRadialPad,
  //!< Radial gradient (repeat) [Base].
  kGradientRadialRepeat,
  //!< Radial gradient (reflect) [Base].
  kGradientRadialReflect,

  //!< Conical gradient (any) [Base].
  kGradientConical,

  //!< Maximum value of a valid FetchType.
  _kMaxValue = kGradientConical,

  //!< Pixel pointer (special value, not a valid fetch type).
  kPixelPtr,

  //!< Invalid fetch type (special value, signalizes error).
  kFailure = 0xFFu,

  kPatternAnyFirst = kPatternAlignedBlit,
  kPatternAnyLast = kPatternAffineBIOpt,

  kPatternAlignedFirst = kPatternAlignedBlit,
  kPatternAlignedLast = kPatternAlignedRoR,

  kPatternUnalignedFirst = kPatternFxPad,
  kPatternUnalignedLast = kPatternFxFyRoR,

  kPatternFxFirst = kPatternFxPad,
  kPatternFxLast = kPatternFxRoR,

  kPatternFyFirst = kPatternFyPad,
  kPatternFyLast = kPatternFyRoR,

  kPatternFxFyFirst = kPatternFxFyPad,
  kPatternFxFyLast = kPatternFxFyRoR,

  kPatternSimpleFirst = kPatternAlignedBlit,
  kPatternSimpleLast = kPatternFxFyRoR,

  kPatternAffineFirst = kPatternAffineNNAny,
  kPatternAffineLast = kPatternAffineBIOpt,

  kGradientAnyFirst = kGradientLinearPad,
  kGradientAnyLast = kGradientConical,

  kGradientLinearFirst = kGradientLinearPad,
  kGradientLinearLast = kGradientLinearRoR,

  kGradientRadialFirst = kGradientRadialPad,
  kGradientRadialLast = kGradientRadialReflect,

  kGradientConicalFirst = kGradientConical,
  kGradientConicalLast = kGradientConical
};

typedef void (BL_CDECL* FillFunc)(ContextData* ctxData, const void* fillData, const void* fetchData) BL_NOEXCEPT;
typedef void (BL_CDECL* FetchFunc)(ContextData* ctxData, const void* fillData, const void* fetchData) BL_NOEXCEPT;

struct DispatchData {
  FillFunc fillFunc;
  FetchFunc fetchFunc;

  //! Initializes the dispatch data.
  //!
  //! If both `fillFunc` and `fetchFunc` are non-null the pipeline would be two-stage, if `fetchFunc` is null the
  //! pipeline would be one-stage. Typically JIT compiled pipelines are one-stage only (the fetch phase is inlined
  //! into the pipeline, but it's not a hard requirement).
  BL_INLINE void init(FillFunc fillFunc, FetchFunc fetchFunc = nullptr) noexcept {
    this->fillFunc = fillFunc;
    this->fetchFunc = fetchFunc;
  }

  //! Tests whether the dispatch data contains a one-stage pipeline.
  //!
  //! One-stage pipelines have no fetch function, which means that the fill function is a real pipeline.
  BL_INLINE bool isOneStage() const noexcept {
    return fetchFunc == nullptr;
  }
};

union PipeValue32 {
  uint32_t u;
  int32_t i;
  float f;
};

union PipeValue64 {
  uint64_t u64;
  int64_t i64;
  double d;

  int32_t i32[2];
  uint32_t u32[2];

  int16_t i16[4];
  uint16_t u16[4];

#if BL_BYTE_ORDER == 1234 // LITTLE ENDIAN
  struct { int32_t  i32Lo, i32Hi; };
  struct { uint32_t u32Lo, u32Hi; };
#else
  struct { int32_t  i32Hi, i32Lo; };
  struct { uint32_t u32Hi, u32Lo; };
#endif

  BL_INLINE void expandLoToHi() noexcept { u32Hi = u32Lo; }
};

//! Mask command.
struct MaskCommand {
  enum : uint32_t {
    kTypeBits = 3,
    kTypeMask = 0x7
  };

  //! Start of the span, inclusive.
  uint32_t _x0AndType;
  //! End of the span, exclusive.
  uint32_t _x1;

  union {
    uintptr_t value;
    const void* ptr;
  } _data;

  //! Mask command type and increment.
  uintptr_t _maskAdvance;

  //! \name Accessors
  //! \{

  BL_INLINE MaskCommandType type() const noexcept { return MaskCommandType(_x0AndType & kTypeMask); }
  BL_INLINE uint32_t x0() const noexcept { return _x0AndType >> kTypeBits; }
  BL_INLINE uint32_t x1() const noexcept { return _x1; }

  BL_INLINE bool isConstMask() const noexcept { return type() == MaskCommandType::kCMask; }

  BL_INLINE uint32_t maskValue() const noexcept { return uint32_t(_data.value); }
  BL_INLINE const void* maskData() const noexcept { return _data.ptr; }

  BL_INLINE intptr_t maskAdvance() const noexcept { return _maskAdvance; }

  BL_INLINE void initTypeAndSpan(MaskCommandType type, uint32_t x0, uint32_t x1) noexcept {
    BL_ASSERT(((x0 << kTypeBits) >> kTypeBits) == x0);
    _x0AndType = uint32_t(type) | (x0 << kTypeBits);
    _x1 = x1;
  }

  BL_INLINE void initConstMask(MaskCommandType type, uint32_t x0, uint32_t x1, uint32_t maskValue) noexcept {
    initTypeAndSpan(type, x0, x1);
    _data.value = maskValue;
  }

  BL_INLINE void initVariableMask(MaskCommandType type, uint32_t x0, uint32_t x1, const void* maskData, intptr_t maskAdvance = 0) noexcept {
    initTypeAndSpan(type, x0, x1);
    _data.ptr = maskData;
    _maskAdvance = maskAdvance;
  }

  BL_INLINE void initEnd() noexcept { initTypeAndSpan(MaskCommandType::kEnd, 0, 0); }
  BL_INLINE void initRepeat() noexcept { initTypeAndSpan(MaskCommandType::kRepeat, 0, 0); }


  //! \}
};

struct ContextData {
  BLImageData dst;

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

struct FillData {
  struct Common {
    //! Rectangle to fill.
    BLBoxI box;
    //! Alpha value (range depends on target pixel format).
    PipeValue32 alpha;
  };

  //! Rectangle (axis-aligned).
  struct BoxA {
    //! Rectangle to fill.
    BLBoxI box;
    //! Alpha value (range depends on target pixel format).
    PipeValue32 alpha;
  };

  //! Rectangle (axis-unaligned).
  struct BoxU {
    //! Rectangle to fill.
    BLBoxI box;
    //! Alpha value (range depends on target pixel format).
    PipeValue32 alpha;

    //! Masks of top, middle and bottom part of the rect.
    //!
    //! \note The last value `masks[3]` must be zero as it's a sentinel for the pipeline.
    uint32_t masks[4];
    //! Height of the middle (1) and last (2) masks.
    uint32_t heights[2];
    //! Start width (from 1 to 3).
    uint32_t startWidth;
    //! Inner width (from 0 to width).
    uint32_t innerWidth;
  };

  struct Mask {
    //! Fill boundary.
    BLBoxI box;
    //! Alpha value (range depends on target pixel format).
    PipeValue32 alpha;
    //! Reserved for future use (padding).
    uint32_t reserved;

    //! The first mask command to process.
    MaskCommand* maskCommandData;
  };

  struct Analytic {
    //! Fill boundary.
    BLBoxI box;
    //! Alpha value (range depends on format).
    PipeValue32 alpha;
    //! All ones if NonZero or 0x01FF if EvenOdd.
    uint32_t fillRuleMask;

    //! Shadow bit-buffer (marks a group of cells which are non-zero).
    BLBitWord* bitTopPtr;
    //! Bit-buffer stride (in bytes).
    size_t bitStride;

    //! Cell buffer.
    uint32_t* cellTopPtr;
    //! Cell stride (in bytes).
    size_t cellStride;
  };

  union {
    Common common;
    BoxA boxAA;
    BoxU boxAU;
    Mask mask;
    Analytic analytic;
  };

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  //! \name Init
  //! \{

  BL_INLINE bool initBoxA8bpc(uint32_t alpha, int x0, int y0, int x1, int y1) noexcept {
    // The rendering engine should never pass out-of-range alpha.
    BL_ASSERT(alpha <= 255);

    // The rendering engine should never pass invalid box to the pipeline.
    BL_ASSERT(x0 < x1);
    BL_ASSERT(y0 < y1);

    boxAA.alpha.u = alpha;
    boxAA.box.reset(int(x0), int(y0), int(x1), int(y1));

    return true;
  }

  template<typename T>
  BL_INLINE bool initBoxU8bpcT(uint32_t alpha, T x0, T y0, T x1, T y1) noexcept {
    return initBoxU8bpc24x8(alpha, blTruncToInt(x0 * T(256)),
                                   blTruncToInt(y0 * T(256)),
                                   blTruncToInt(x1 * T(256)),
                                   blTruncToInt(y1 * T(256)));
  }

  bool initBoxU8bpc24x8(uint32_t alpha, int x0, int y0, int x1, int y1) noexcept {
    // The rendering engine should never pass out-of-range alpha.
    BL_ASSERT(alpha <= 255);

    // The rendering engine should never pass invalid box to the pipeline.
    BL_ASSERT(x0 < x1);
    BL_ASSERT(y0 < y1);

    uint32_t ax0 = uint32_t(x0) >> 8;
    uint32_t ay0 = uint32_t(y0) >> 8;
    uint32_t ax1 = uint32_t(x1) >> 8;
    uint32_t ay1 = uint32_t(y1) >> 8;

    boxAU.alpha.u = alpha;
    boxAU.box.reset(int(ax0), int(ay0), int(ax1), int(ay1));

    uint32_t fx0 = uint32_t(x0) & 0xFFu;
    uint32_t fy0 = uint32_t(y0) & 0xFFu;
    uint32_t fx1 = uint32_t(x1) & 0xFFu;
    uint32_t fy1 = uint32_t(y1) & 0xFFu;

    boxAU.box.x1 += fx1 != 0;
    boxAU.box.y1 += fy1 != 0;

    if (!fx1) fx1 = 256;
    if (!fy1) fy1 = 256;

    if (((x0 ^ x1) >> 8) == 0) { fx0 = fx1 - fx0; fx1 = 0; } else { fx0 = 256 - fx0; }
    if (((y0 ^ y1) >> 8) == 0) { fy0 = fy1 - fy0; fy1 = 0; } else { fy0 = 256 - fy0; }

    uint32_t fy0_a = fy0 * alpha;
    uint32_t fy1_a = fy1 * alpha;

    uint32_t m0 = (fx1 * fy0_a) >> 16;
    uint32_t m1 = (fx1 * alpha) >>  8;
    uint32_t m2 = (fx1 * fy1_a) >> 16;

    uint32_t iw = uint32_t(boxAU.box.x1 - boxAU.box.x0);
    if (iw > 2) {
      m0 = (m0 << 8) | (fy0_a >> 8);
      m1 = (m1 << 8) | alpha;
      m2 = (m2 << 8) | (fy1_a >> 8);
    }

    if (iw > 1) {
      m0 = (m0 << 8) | ((fx0 * fy0_a) >> 16);
      m1 = (m1 << 8) | ((fx0 * alpha) >>  8);
      m2 = (m2 << 8) | ((fx0 * fy1_a) >> 16);
    }

    if (!m1)
      return false;

    // Border case - if alpha is too low it can cause `m0` or `m2` to be zero,
    // which would then confuse the pipeline as it would think to stop instead
    // of jumping to 'CMask' loop. So we patch `m0`
    if (!m0) {
      m0 = m1;
      boxAU.box.y0++;
      if (boxAU.box.y0 == boxAU.box.y1)
        return false;
    }

    uint32_t ih = uint32_t(boxAU.box.y1 - boxAU.box.y0);

    boxAU.masks[0] = m0;
    boxAU.masks[1] = m1;
    boxAU.masks[2] = m2;
    boxAU.masks[3] = 0;
    boxAU.heights[0] = ih - 2;
    boxAU.heights[1] = 1;

    // There is no middle layer (m1) if the height is 2 pixels or less.
    if (ih <= 2) {
      boxAU.masks[1] = boxAU.masks[2];
      boxAU.masks[2] = 0;
      boxAU.heights[0] = ih - 1;
      boxAU.heights[1] = 0;
    }

    if (ih <= 1) {
      boxAU.masks[1] = 0;
      boxAU.heights[0] = 0;
    }

    if (iw > 3) {
      boxAU.startWidth = 1;
      boxAU.innerWidth = iw - 2;
    }
    else {
      boxAU.startWidth = iw;
      boxAU.innerWidth = 0;
    }

    return true;
  }

  BL_INLINE void initMaskA(uint32_t alpha, int x0, int y0, int x1, int y1, MaskCommand* maskCommandData) noexcept {
    mask.alpha.u = alpha;
    mask.box.reset(x0, y0, x1, y1);
    mask.maskCommandData = maskCommandData;
  }

  BL_INLINE bool initAnalytic(uint32_t alpha, uint32_t fillRule, BLBitWord* bitTopPtr, size_t bitStride, uint32_t* cellTopPtr, size_t cellStride) noexcept {
    analytic.alpha.u = alpha;
    analytic.fillRuleMask = uint32_t(fillRule == BL_FILL_RULE_NON_ZERO ? FillRuleMask::kNonZeroMask : FillRuleMask::kEvenOddMask);
    analytic.bitTopPtr = bitTopPtr;
    analytic.bitStride = bitStride;
    analytic.cellTopPtr = cellTopPtr;
    analytic.cellStride = cellStride;

    return true;
  }

  //! \}
};

//! Pipeline fetch data.
struct alignas(16) FetchData {
  //! Solid fetch data.
  struct Solid {
    union {
      struct {
        //! 32-bit ARGB, premultiplied.
        uint32_t prgb32;
        //! Reserved in case 32-bit data is used.
        uint32_t reserved32;
      };
      //! 64-bit ARGB, premultiplied.
      uint64_t prgb64;
    };

    BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
  };

  //! Pattern fetch data.
  struct alignas(16) Pattern {
    //! Source image data.
    struct SourceData {
      const uint8_t* pixelData;
      intptr_t stride;
      BLSizeI size;
    };

    //! Simple pattern data (only identity or translation matrix).
    struct alignas(16) Simple {
      //! Translate by x/y (inverted).
      int32_t tx, ty;
      //! Repeat/Reflect w/h.
      int32_t rx, ry;
      //! Safe X increments by 1..16 (fetchN).
      BLModuloTable ix;
      //! 9-bit or 17-bit weight at [0, 0] (A).
      uint32_t wa;
      //! 9-bit or 17-bit weight at [1, 0] (B).
      uint32_t wb;
      //! 9-bit or 17-bit weight at [0, 1] (C).
      uint32_t wc;
      //! 9-bit or 17-bit weight at [1, 1] (D).
      uint32_t wd;
    };

    //! Affine pattern data.
    struct alignas(16) Affine {
      //! Single X/Y step in X direction.
      PipeValue64 xx, xy;
      //! Single X/Y step in Y direction.
      PipeValue64 yx, yy;
      //! Pattern offset at [0, 0].
      PipeValue64 tx, ty;
      //! Pattern overflow check.
      PipeValue64 ox, oy;
      //! Pattern overflow correction (repeat/reflect).
      PipeValue64 rx, ry;
      //! Two X/Y steps in X direction, used by `fetch4()`.
      PipeValue64 xx2, xy2;
      //! Pattern padding minimum (0 for PAD, INT32_MIN for other modes).
      int32_t minX, minY;
      //! Pattern padding maximum (width-1 and height-1).
      int32_t maxX, maxY;
      //! Correction X/Y values in case that maxX/maxY was exceeded (PAD, BILINEAR)
      int32_t corX, corY;
      //! Repeated tile width/height (doubled if reflected).
      double tw, th;

      //! 32-bit value to be used by [V]PMADDWD instruction to calculate address from Y/X pairs.
      int16_t addrMul[2];
    };

    //! Source image data.
    SourceData src;

    union {
      //! Simple pattern data.
      Simple simple;
      //! Affine pattern data.
      Affine affine;
    };

    BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
  };

  //! Gradient fetch data.
  struct alignas(16) Gradient {
    //! Precomputed lookup table, used by all gradient fetchers.
    struct LUT {
      //! Pixel data, array of either 32-bit or 64-bit pixels.
      const void* data;
      //! Number of pixels stored in `data`, must be a power of 2.
      uint32_t size;
    };

    //! Linear gradient data.
    struct alignas(16) Linear {
      //! Gradient offset of the pixel at [0, 0].
      PipeValue64 pt[2];
      //! One Y step.
      PipeValue64 dy;
      //! One X step.
      PipeValue64 dt;
      //! Two X steps.
      PipeValue64 dt2;
      //! Reflect/Repeat mask (repeated/reflected size - 1).
      PipeValue64 rep;
      //! Size mask (gradient size - 1).
      PipeValue32 msk;
    };

    //! Radial gradient data.
    struct alignas(16) Radial {
      //! Gradient X/Y increments (horizontal).
      double xx, xy;
      //! Gradient X/Y increments (vertical).
      double yx, yy;
      //! Gradient X/Y offsets of the pixel at [0, 0].
      double ox, oy;

      double ax, ay;
      double fx, fy;

      double dd, bd;
      double ddx, ddy;
      double ddd, scale;

      int maxi;
    };

    //! Conical gradient data.
    struct alignas(16) Conical {
      //! Gradient X/Y increments (horizontal).
      double xx, xy;
      //! Gradient X/Y increments (vertical).
      double yx, yy;
      //! Gradient X/Y offsets of the pixel at [0, 0].
      double ox, oy;
      //! Atan2 approximation constants.
      const BLCommonTable::Conical* consts;

      int maxi;
    };

    //! Precomputed lookup table.
    LUT lut;
    //! Union of all possible gradient data types.
    union {
      //! Linear gradient specific data.
      Linear linear;
      //! Radial gradient specific data.
      Radial radial;
      //! Conical gradient specific data.
      Conical conical;
    };

    BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
  };

  //! Union of all possible fetch data types.
  union {
    //! Solid fetch data.
    Solid solid;
    //! Pattern fetch data.
    Pattern pattern;
    //! Gradient fetch data.
    Gradient gradient;
  };

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  BL_INLINE void initPatternSource(const uint8_t* pixelData, intptr_t stride, int w, int h) noexcept {
    pattern.src.pixelData = pixelData;
    pattern.src.stride = stride;
    pattern.src.size.reset(w, h);
  }

  BL_INLINE FetchType initPatternBlit(int x, int y) noexcept {
    pattern.simple.tx = x;
    pattern.simple.ty = y;
    pattern.simple.rx = 0;
    pattern.simple.ry = 0;
    return FetchType::kPatternAlignedBlit;
  }

  BL_HIDDEN FetchType initPatternAxAy(
    uint32_t extendMode,
    int x, int y) noexcept;

  BL_HIDDEN FetchType initPatternFxFy(
    uint32_t extendMode,
    uint32_t filter,
    uint32_t bytesPerPixel,
    int64_t tx64, int64_t ty64) noexcept;

  BL_HIDDEN FetchType initPatternAffine(
    uint32_t extendMode,
    uint32_t filter,
    uint32_t bytesPerPixel,
    const BLMatrix2D& m) noexcept;

  BL_HIDDEN FetchType initGradient(
    uint32_t gradientType,
    const void* values,
    uint32_t extendMode,
    const BLGradientLUT* lut,
    const BLMatrix2D& m) noexcept;
};

//! Pipeline signature packed to a single `uint32_t` value.
//!
//! Can be used to build signatures as well as it offers the required functionality.
struct Signature {
  //! \name Constants
  //! \{

  //! Masks used by the Signature.
  //!
  //! Each mask represents one value in a signature. Each value describes a part in a signature like format,
  //! composition operator, etc. All parts packed together form a 32-bit integer that can be used to uniquely
  //! describe the whole pipeline and can act as a key or hash-code in pipeline function caches.
  enum Masks : uint32_t {
    kMaskDstFormat = 0x0000000Fu <<  0, // [00..03] {16 values}
    kMaskSrcFormat = 0x0000000Fu <<  4, // [04..07] {16 values}
    kMaskCompOp    = 0x0000003Fu <<  8, // [08..13] {64 values}
    kMaskFillType  = 0x00000007u << 14, // [14..15] { 8 values}
    kMaskFetchType = 0x0000001Fu << 17  // [17..21] {32 values}
  };

  //! \}

  //! \name Members
  //! \{

  //! Signature as a 32-bit value.
  uint32_t value;

  //! \}

  BL_INLINE Signature() noexcept = default;
  BL_INLINE constexpr Signature(const Signature&) noexcept = default;
  BL_INLINE constexpr explicit Signature(uint32_t value) : value(value) {}

  BL_INLINE bool operator==(const Signature& other) const noexcept { return value == other.value; }
  BL_INLINE bool operator!=(const Signature& other) const noexcept { return value != other.value; }

  BL_INLINE uint32_t _get(uint32_t mask) const noexcept {
    return (this->value & mask) >> BLIntOps::bitShiftOf(mask);
  }

  BL_INLINE void _set(uint32_t mask, uint32_t v) noexcept {
    BL_ASSERT(v <= (mask >> BLIntOps::bitShiftOf(mask)));
    this->value = (this->value & ~mask) | (v << BLIntOps::bitShiftOf(mask));
  }

  BL_INLINE void _add(uint32_t mask, uint32_t v) noexcept {
    BL_ASSERT(v <= (mask >> BLIntOps::bitShiftOf(mask)));
    this->value |= (v << BLIntOps::bitShiftOf(mask));
  }

  //! Reset all values to zero.
  BL_INLINE void reset() noexcept { this->value = 0; }
  //! Reset all values to `v`.
  BL_INLINE void reset(uint32_t v) noexcept { this->value = v; }
  //! Reset all values to the `other` signature.
  BL_INLINE void reset(const Signature& other) noexcept { this->value = other.value; }

  //! Set the signature from a packed 32-bit integer.
  BL_INLINE void setValue(uint32_t v) noexcept { this->value = v; }
  //! Set the signature from another `Signature`.
  BL_INLINE void setValue(const Signature& other) noexcept { this->value = other.value; }

  //! Extracts destination pixel format from the signature.
  BL_INLINE uint32_t dstFormat() const noexcept { return _get(kMaskDstFormat); }
  //! Extracts source pixel format from the signature.
  BL_INLINE uint32_t srcFormat() const noexcept { return _get(kMaskSrcFormat); }
  //! Extracts composition operator from the signature.
  BL_INLINE uint32_t compOp() const noexcept { return _get(kMaskCompOp); }
  //! Extracts sweep type from the signature.
  BL_INLINE FillType fillType() const noexcept { return FillType(_get(kMaskFillType)); }
  //! Extracts fetch type from the signature.
  BL_INLINE FetchType fetchType() const noexcept { return FetchType(_get(kMaskFetchType)); }

  BL_INLINE bool isSolid() const noexcept { return fetchType() == FetchType::kSolid; }

  //! Add destination pixel format.
  BL_INLINE void setDstFormat(uint32_t v) noexcept { _set(kMaskDstFormat, v); }
  //! Add source pixel format.
  BL_INLINE void setSrcFormat(uint32_t v) noexcept { _set(kMaskSrcFormat, v); }
  //! Add clip mode.
  BL_INLINE void setCompOp(uint32_t v) noexcept { _set(kMaskCompOp, v); }
  //! Add sweep type.
  BL_INLINE void setFillType(uint32_t v) noexcept { _set(kMaskFillType, v); }
  //! Add fetch type.
  BL_INLINE void setFetchType(uint32_t v) noexcept { _set(kMaskFetchType, v); }

  // The following methods are used to build the signature. They use '|' operator
  // which doesn't clear the previous value, each function is expected to be called
  // only once when building a new signature.

  //! Combine with other signature.
  BL_INLINE void add(uint32_t v) noexcept { this->value |= v; }
  //! Combine with other signature.
  BL_INLINE void add(const Signature& other) noexcept { this->value |= other.value; }

  //! Add destination pixel format.
  BL_INLINE void addDstFormat(uint32_t v) noexcept { _add(kMaskDstFormat, v); }
  //! Add source pixel format.
  BL_INLINE void addSrcFormat(uint32_t v) noexcept { _add(kMaskSrcFormat, v); }
  //! Add clip mode.
  BL_INLINE void addCompOp(uint32_t v) noexcept { _add(kMaskCompOp, v); }
  //! Add sweep type.
  BL_INLINE void addFillType(FillType v) noexcept { _add(kMaskFillType, uint32_t(v)); }
  //! Add fetch type.
  BL_INLINE void addFetchType(FetchType v) noexcept { _add(kMaskFetchType, uint32_t(v)); }
};

} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_PIPEDEFS_P_H_INCLUDED
