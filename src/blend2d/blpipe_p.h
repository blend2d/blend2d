// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLPIPE_P_H
#define BLEND2D_BLPIPE_P_H

#include "./blapi-internal_p.h"
#include "./blformat_p.h"
#include "./blgradient_p.h"
#include "./blmatrix_p.h"
#include "./blpattern_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"
#include "./bltables_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct BLPipeContextData;
struct BLPipeFillData;
struct BLPipeFetchData;
struct BLPipeSignature;

// ============================================================================
// [Constants]
// ============================================================================

//! Global constants used by pipeline and affecting also rasterizers.
enum BLPipeGlobalConsts : uint32_t {
  //! How many pixels are represented by a single bit of a `BLBitWord`.
  //!
  //! This is a hardcoded value as it's required by both rasterizer and compositor.
  //! Before establishing `4` the values [4, 8, 16, 32] were tested. Candidates
  //! were `4` and `8` where `8` sometimes surpassed `4` in specific workloads,
  //! but `4` was stable across all tests.
  //!
  //! In general increasing `BL_PIPE_PIXELS_PER_ONE_BIT` would result in less
  //! memory consumed by bit vectors, but would increase the work compositors
  //! have to do to process cells produced by analytic rasterizer.
  BL_PIPE_PIXELS_PER_ONE_BIT = 4
};

//! 8-bit alpha constants used by pipelines and affecting also rasterizers.
enum BLPipeA8Consts : uint32_t {
  BL_PIPE_A8_SHIFT = 8,                     // 8.
  BL_PIPE_A8_SCALE = 1 << BL_PIPE_A8_SHIFT, // 256.
  BL_PIPE_A8_MASK  = BL_PIPE_A8_SCALE - 1   // 255.
};

//! Pipeline extend modes (non-combined).
//!
//! Pipeline sees extend modes a bit differently in most cases.
enum BLPipeExtendMode : uint32_t {
  BL_PIPE_EXTEND_MODE_PAD         = 0,         //!< Pad, same as `BL_EXTEND_MODE_PAD`.
  BL_PIPE_EXTEND_MODE_REPEAT      = 1,         //!< Repeat, same as `BL_EXTEND_MODE_REPEAT`.
  BL_PIPE_EXTEND_MODE_REFLECT     = 2,         //!< Reflect, same as `BL_EXTEND_MODE_REFLECT`.
  BL_PIPE_EXTEND_MODE_ROR         = 3,         //!< Repeat-or-reflect (the same code-path for both cases).

  BL_PIPE_EXTEND_MODE_COUNT       = 4          //! Count of pipeline-specific extend modes.
};

//! Pipeline fill-type.
//!
//! A unique id describing how a mask of each filled pixel is calculated.
enum BLPipeFillType : uint32_t {
  BL_PIPE_FILL_TYPE_NONE          = 0,         //!< None or uninitialized.
  BL_PIPE_FILL_TYPE_BOX_AA        = 1,         //!< Fill axis-aligned box.
  BL_PIPE_FILL_TYPE_BOX_AU        = 2,         //!< Fill axis-unaligned box.
  BL_PIPE_FILL_TYPE_ANALYTIC      = 3,         //!< Fill analytic non-zero/even-odd.

  BL_PIPE_FILL_TYPE_COUNT         = 4          //!< Count of fill types.
};

//! Fill rule mask used during composition of mask produced by analytic-rasterizer.
//!
//! See blfillpart.cpp how this is used. What you see in these values is
//! mask shifted left by one bit as we expect such values in the pipeline.
enum BLPipeFillRuleMask : uint32_t {
  BL_PIPE_FILL_RULE_MASK_NON_ZERO = uint32_t(0xFFFFFFFFu << 1),
  BL_PIPE_FILL_RULE_MASK_EVEN_ODD = uint32_t(0x000001FFu << 1)
};

//! Pipeline fetch-type.
//!
//! A unique id describing how pixels are fetched - supported fetchers include
//! solid pixels, patterns (sometimes referred as blits), and gradients.
//!
//! NOTE: RoR is a shurtcut for repeat-or-reflect - an universal fetcher for both.
enum BLPipeFetchType : uint32_t {
  BL_PIPE_FETCH_TYPE_SOLID  = 0,               //!< Solid fetch.

  BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT,          //!< Pattern {aligned} (blit) [Base].
  BL_PIPE_FETCH_TYPE_PATTERN_AA_PAD,           //!< Pattern {aligned} (pad-x) [Base].
  BL_PIPE_FETCH_TYPE_PATTERN_AA_REPEAT,        //!< Pattern {aligned} (repeat-large-x) [Optimized].
  BL_PIPE_FETCH_TYPE_PATTERN_AA_ROR,           //!< Pattern {aligned} (ror-x) [Base].
  BL_PIPE_FETCH_TYPE_PATTERN_FX_PAD,           //!< Pattern {frac-x} (pad-x) [Optimized].
  BL_PIPE_FETCH_TYPE_PATTERN_FX_ROR,           //!< Pattern {frac-x} (ror-x) [Optimized].
  BL_PIPE_FETCH_TYPE_PATTERN_FY_PAD,           //!< Pattern {frac-y} (pad-x) [Optimized].
  BL_PIPE_FETCH_TYPE_PATTERN_FY_ROR,           //!< Pattern {frac-x} (ror-x) [Optimized].
  BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_PAD,        //!< Pattern {frac-xy} (pad-x) [Base].
  BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_ROR,        //!< Pattern {frac-xy} (ror-x) [Base].
  BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_ANY,    //!< Pattern {affine-nearest}  (any) [Base].
  BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_OPT,    //!< Pattern {affine-nearest}  (any) [Optimized].
  BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_ANY,    //!< Pattern {affine-bilinear} (any) [Base].
  BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_OPT,    //!< Pattern {affine-bilinear} (any) [Optimized].

  BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_PAD,      //!< Linear gradient (pad) [Base].
  BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_ROR,      //!< Linear gradient (ror) [Base].
  BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_PAD,      //!< Radial gradient (pad) [Base].
  BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_REPEAT,   //!< Radial gradient (repeat) [Base].
  BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_REFLECT,  //!< Radial gradient (reflect) [Base].
  BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL,         //!< Conical gradient (any) [Base].

  BL_PIPE_FETCH_TYPE_COUNT,                    //!< Number of fetch types.

  BL_PIPE_FETCH_TYPE_PIXEL_PTR = 0xFF,         //!< Pixel pointer, not a valid fetch type.

  BL_PIPE_FETCH_TYPE_PATTERN_ANY_FIRST         = BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT,
  BL_PIPE_FETCH_TYPE_PATTERN_ANY_LAST          = BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_OPT,

  BL_PIPE_FETCH_TYPE_PATTERN_AA_FIRST          = BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT,
  BL_PIPE_FETCH_TYPE_PATTERN_AA_LAST           = BL_PIPE_FETCH_TYPE_PATTERN_AA_ROR,

  BL_PIPE_FETCH_TYPE_PATTERN_AU_FIRST          = BL_PIPE_FETCH_TYPE_PATTERN_FX_PAD,
  BL_PIPE_FETCH_TYPE_PATTERN_AU_LAST           = BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_ROR,

  BL_PIPE_FETCH_TYPE_PATTERN_FX_FIRST          = BL_PIPE_FETCH_TYPE_PATTERN_FX_PAD,
  BL_PIPE_FETCH_TYPE_PATTERN_FX_LAST           = BL_PIPE_FETCH_TYPE_PATTERN_FX_ROR,

  BL_PIPE_FETCH_TYPE_PATTERN_FY_FIRST          = BL_PIPE_FETCH_TYPE_PATTERN_FY_PAD,
  BL_PIPE_FETCH_TYPE_PATTERN_FY_LAST           = BL_PIPE_FETCH_TYPE_PATTERN_FY_ROR,

  BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_FIRST       = BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_PAD,
  BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_LAST        = BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_ROR,

  BL_PIPE_FETCH_TYPE_PATTERN_SIMPLE_FIRST      = BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT,
  BL_PIPE_FETCH_TYPE_PATTERN_SIMPLE_LAST       = BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_ROR,

  BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_FIRST      = BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_ANY,
  BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_LAST       = BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_OPT,

  BL_PIPE_FETCH_TYPE_GRADIENT_ANY_FIRST        = BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_PAD,
  BL_PIPE_FETCH_TYPE_GRADIENT_ANY_LAST         = BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL,

  BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_FIRST     = BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_PAD,
  BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_LAST      = BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_ROR,

  BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_FIRST     = BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_PAD,
  BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_LAST      = BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_REFLECT,

  BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL_FIRST    = BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL,
  BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL_LAST     = BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL
};

//! Masks used by `BLPipeSignature`.
//!
//! Each mask represents one value in a signature. Each value describes a part
//! in a signature like format, composition operator, etc. All parts packed
//! together form a 32-bit integer that can be used to uniquely describe the
//! whole pipeline and can act as a key or hash-code in pipeline function caches.
enum BLPipeSignatureMasks : uint32_t {
  BL_PIPE_SIGNATURE_DST_FORMAT    = 0x0000000Fu <<  0, // [00..03] {16 values}
  BL_PIPE_SIGNATURE_SRC_FORMAT    = 0x0000000Fu <<  4, // [04..07] {16 values}
  BL_PIPE_SIGNATURE_COMP_OP       = 0x0000003Fu <<  8, // [08..13] {64 values}
  BL_PIPE_SIGNATURE_FILL_TYPE     = 0x00000003u << 14, // [14..15] {4 values}
  BL_PIPE_SIGNATURE_FETCH_TYPE    = 0x0000001Fu << 16, // [16..20] {32 values}
  BL_PIPE_SIGNATURE_FETCH_PAYLOAD = 0x000007FFu << 21  // [21..31] {2048 values}
};

// ============================================================================
// [Typedefs]
// ============================================================================

typedef BLResult (BL_CDECL* BLPipeFillFunc)(void* ctxData, void* fillData, const void* fetchData) BL_NOEXCEPT;

// ============================================================================
// [BLPipeValue32]
// ============================================================================

union BLPipeValue32 {
  uint32_t u;
  int32_t i;
  float f;
};

// ============================================================================
// [BLPipeValue64]
// ============================================================================

union BLPipeValue64 {
  uint64_t u64;
  int64_t i64;
  double d;

  int32_t i32[2];
  uint32_t u32[2];

  int16_t i16[4];
  uint16_t u16[4];

#if BL_BUILD_BYTE_ORDER == 1234
  struct { uint32_t i32Lo, i32Hi; };
  struct { uint32_t u32Lo, u32Hi; };
#else
  struct { uint32_t i32Hi, i32Lo; };
  struct { uint32_t u32Hi, u32Lo; };
#endif

  BL_INLINE void expandLoToHi() noexcept { u32Hi = u32Lo; }
};

// ============================================================================
// [BLPipeContextData]
// ============================================================================

struct BLPipeContextData {
  BLImageData dst;

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// [BLPipeFillData]
// ============================================================================

struct BLPipeFillData {
  struct Common {
    //! Rectangle to fill.
    BLBoxI box;
    //! Alpha value (range depends on format).
    BLPipeValue32 alpha;
  };

  //! Rectangle (axis-aligned).
  struct BoxAA {
    //! Rectangle to fill.
    BLBoxI box;
    //! Alpha value (range depends on format).
    BLPipeValue32 alpha;
  };

  //! Rectangle (axis-unaligned).
  struct BoxAU {
    //! Rectangle to fill.
    BLBoxI box;
    //! Alpha value (range depends on format).
    BLPipeValue32 alpha;

    //! Masks of top, middle and bottom part of the rect.
    uint32_t masks[3];
    //! Start width (from 1 to 3).
    uint32_t startWidth;
    //! Inner width (from 0 to width).
    uint32_t innerWidth;
  };

  struct Analytic {
    //! Fill boundary (x0 is ignored, x1 acts as maxWidth, y0/y1 are used normally).
    BLBoxI box;
    //! Alpha value (range depends on format).
    BLPipeValue32 alpha;
    //! All ones if NonZero or 0x01FF if EvenOdd.
    uint32_t fillRuleMask;

    //! Rasterizer bits (marks a group of cells which are non-zero).
    BLBitWord* bitTopPtr;
    //! Bit stride (in bytes).
    size_t bitStride;

    //! Rasterizer cells.
    uint32_t* cellTopPtr;
    //! Cell stride [in bytes].
    size_t cellStride;
  };

  union {
    Common common;
    BoxAA boxAA;
    BoxAU boxAU;
    Analytic analytic;
  };

  inline void reset() noexcept { memset(this, 0, sizeof(*this)); }

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  inline uint32_t initBoxAA8bpc(uint32_t alpha, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) noexcept {
    // The rendering engine should never pass out-of-range alpha.
    BL_ASSERT(alpha <= 256);

    // The rendering engine should never pass invalid box to the pipeline.
    BL_ASSERT(x0 < x1);
    BL_ASSERT(y0 < y1);

    boxAA.alpha.u = alpha;
    boxAA.box.reset(int(x0), int(y0), int(x1), int(y1));
    return BL_PIPE_FILL_TYPE_BOX_AA;
  }

  template<typename T>
  inline uint32_t initBoxAU8bpcT(uint32_t alpha, T x0, T y0, T x1, T y1) noexcept {
    return initBoxAU8bpc24x8(alpha, uint32_t(blTruncToInt(x0 * T(256))),
                                    uint32_t(blTruncToInt(y0 * T(256))),
                                    uint32_t(blTruncToInt(x1 * T(256))),
                                    uint32_t(blTruncToInt(y1 * T(256))));
  }

  uint32_t initBoxAU8bpc24x8(uint32_t alpha, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) noexcept {
    // The rendering engine should never pass out-of-range alpha.
    BL_ASSERT(alpha <= 256);

    uint32_t ax0 = x0 >> 8;
    uint32_t ay0 = y0 >> 8;
    uint32_t ax1 = x1 >> 8;
    uint32_t ay1 = y1 >> 8;

    boxAU.alpha.u = alpha;
    boxAU.box.reset(int(ax0), int(ay0), int(ax1), int(ay1));

    // Special case - coordinates are very close (nothing to render).
    if (x0 >= x1 || y0 >= y1)
      return BL_PIPE_FILL_TYPE_NONE;

    // Special case - aligned box.
    if (((x0 | x1 | y0 | y1) & 0xFFu) == 0u)
      return BL_PIPE_FILL_TYPE_BOX_AA;

    uint32_t fx0 = x0 & 0xFFu;
    uint32_t fy0 = y0 & 0xFFu;
    uint32_t fx1 = x1 & 0xFFu;
    uint32_t fy1 = y1 & 0xFFu;

    boxAU.box.x1 += fx1 != 0;
    boxAU.box.y1 += fy1 != 0;

    if (fx1 == 0) fx1 = 256;
    if (fy1 == 0) fy1 = 256;

    fx0 = 256 - fx0;
    fy0 = 256 - fy0;

    if ((x0 & ~0xFF) == (x1 & ~0xFF)) { fx0 = fx1 - fx0; fx1 = 0; }
    if ((y0 & ~0xFF) == (y1 & ~0xFF)) { fy0 = fy1 - fy0; fy1 = 0; }

    uint32_t iw = uint32_t(boxAU.box.x1 - boxAU.box.x0);
    uint32_t m0 = ((fx1 * fy0) >> 8);
    uint32_t m1 = ( fx1            );
    uint32_t m2 = ((fx1 * fy1) >> 8);

    if (iw > 2) {
      m0 = (m0 << 9) + fy0;
      m1 = (m1 << 9) + 256;
      m2 = (m2 << 9) + fy1;
    }

    if (iw > 1) {
      m0 = (m0 << 9) + ((fx0 * fy0) >> 8);
      m1 = (m1 << 9) + fx0;
      m2 = (m2 << 9) + ((fx0 * fy1) >> 8);
    }

    if (alpha != 256) {
      m0 = mulPackedMaskByAlpha(m0, alpha);
      m1 = mulPackedMaskByAlpha(m1, alpha);
      m2 = mulPackedMaskByAlpha(m2, alpha);
    }

    boxAU.masks[0] = m0;
    boxAU.masks[1] = m1;
    boxAU.masks[2] = m2;

    if (iw > 3) {
      boxAU.startWidth = 1;
      boxAU.innerWidth = iw - 2;
    }
    else {
      boxAU.startWidth = iw;
      boxAU.innerWidth = 0;
    }

    return BL_PIPE_FILL_TYPE_BOX_AU;
  }

  inline uint32_t initAnalytic(uint32_t alpha, BLBitWord* bitTopPtr, size_t bitStride, uint32_t* cellTopPtr, size_t cellStride) noexcept {
    analytic.alpha.u = alpha;
    analytic.bitTopPtr = bitTopPtr;
    analytic.bitStride = bitStride;
    analytic.cellTopPtr = cellTopPtr;
    analytic.cellStride = cellStride;

    return BL_PIPE_FILL_TYPE_ANALYTIC;
  }

  // --------------------------------------------------------------------------
  // [Helpers]
  // --------------------------------------------------------------------------

  static inline uint32_t mulPackedMaskByAlpha(uint32_t m, uint32_t alpha) noexcept {
    return (((((m >> 18)        ) * alpha) >> 8) << 18) |
           (((((m >>  9) & 0x1FF) * alpha) >> 8) <<  9) |
           (((((m      ) & 0x1FF) * alpha) >> 8)      ) ;
  }
};

// ============================================================================
// [BLPipeFetchData]
// ============================================================================

//! Blend2D pipeline fetch data.
struct alignas(16) BLPipeFetchData {
  //! Solid fetch data.
  struct Solid {
    union {
      //! 32-bit ARGB, premultiplied.
      uint32_t prgb32;
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
      BLPipeValue64 xx, xy;
      //! Single X/Y step in Y direction.
      BLPipeValue64 yx, yy;
      //! Pattern offset at [0, 0].
      BLPipeValue64 tx, ty;
      //! Pattern overflow check.
      BLPipeValue64 ox, oy;
      //! Pattern overflow correction (repeat/reflect).
      BLPipeValue64 rx, ry;
      //! Two X/Y steps in X direction, used by `fetch4()`.
      BLPipeValue64 xx2, xy2;
      //! Pattern padding minimum (0 for PAD, INT32_MIN for other modes).
      int32_t minX, minY;
      //! Pattern padding maximum (width-1 and height-1).
      int32_t maxX, maxY;
      //! Pattern correction X/Y in case that maxX/maxY was exceeded (PAD, BILINEAR)
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
      BLPipeValue64 pt[2];
      //! One Y step.
      BLPipeValue64 dy;
      //! One X step.
      BLPipeValue64 dt;
      //! Two X steps.
      BLPipeValue64 dt2;
      //! Reflect/Repeat mask (repeated/reflected size - 1).
      BLPipeValue64 rep;
      //! Size mask (gradient size - 1).
      BLPipeValue32 msk;
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

  BL_INLINE uint32_t initPatternBlit() noexcept {
    pattern.simple.tx = 0;
    pattern.simple.ty = 0;
    pattern.simple.rx = 0;
    pattern.simple.ry = 0;
    return BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT;
  }

  BL_HIDDEN uint32_t initPatternAxAy(
    uint32_t extendMode,
    int x, int y) noexcept;

  BL_HIDDEN uint32_t initPatternFxFy(
    uint32_t extendMode,
    uint32_t filter,
    int64_t tx64, int64_t ty64) noexcept;

  BL_HIDDEN uint32_t initPatternAffine(
    uint32_t extendMode,
    uint32_t filter,
    const BLMatrix2D& m,
    const BLMatrix2D& mInv) noexcept;

  BL_HIDDEN uint32_t initGradient(
    uint32_t gradientType,
    const void* values,
    uint32_t extendMode,
    const BLGradientLUT* lut,
    const BLMatrix2D& m,
    const BLMatrix2D& mInv) noexcept;
};

// ============================================================================
// [BLPipeSignature]
// ============================================================================

//! Pipeline signature packed to a single `uint32_t` value.
//!
//! Can be used to build signatures as well as it offers the required functionality.
struct BLPipeSignature {
  //! Signature as a 32-bit value.
  uint32_t value;

  BL_INLINE BLPipeSignature() noexcept = default;
  BL_INLINE BLPipeSignature(const BLPipeSignature&) noexcept = default;
  BL_INLINE explicit BLPipeSignature(uint32_t value) : value(value) {}

  BL_INLINE uint32_t _get(uint32_t mask) const noexcept {
    return (this->value & mask) >> blBitShiftOf(mask);
  }

  BL_INLINE void _set(uint32_t mask, uint32_t v) noexcept {
    BL_ASSERT(v <= (mask >> blBitShiftOf(mask)));
    this->value = (this->value & ~mask) | (v << blBitShiftOf(mask));
  }

  BL_INLINE void _add(uint32_t mask, uint32_t v) noexcept {
    BL_ASSERT(v <= (mask >> blBitShiftOf(mask)));
    this->value |= (v << blBitShiftOf(mask));
  }

  //! Reset all values to zero.
  BL_INLINE void reset() noexcept { this->value = 0; }
  //! Reset all values to other signature.
  BL_INLINE void reset(uint32_t v) noexcept { this->value = v; }

  //! Set the signature from a packed 32-bit integer.
  BL_INLINE void setValue(uint32_t v) noexcept { this->value = v; }
  //! Set the signature from another `BLPipeSignature`.
  BL_INLINE void setValue(const BLPipeSignature& other) noexcept { this->value = other.value; }

  //! Extracts destination pixel format from the signature.
  BL_INLINE uint32_t dstFormat() const noexcept { return _get(BL_PIPE_SIGNATURE_DST_FORMAT); }
  //! Extracts source pixel format from the signature.
  BL_INLINE uint32_t srcFormat() const noexcept { return _get(BL_PIPE_SIGNATURE_SRC_FORMAT); }
  //! Extracts compositing operator from the signature.
  BL_INLINE uint32_t compOp() const noexcept { return _get(BL_PIPE_SIGNATURE_COMP_OP); }
  //! Extracts sweep type from the signature.
  BL_INLINE uint32_t fillType() const noexcept { return _get(BL_PIPE_SIGNATURE_FILL_TYPE); }
  //! Extracts fetch type from the signature.
  BL_INLINE uint32_t fetchType() const noexcept { return _get(BL_PIPE_SIGNATURE_FETCH_TYPE); }
  //! Extracts fetch data from the signature.
  BL_INLINE uint32_t fetchPayload() const noexcept { return _get(BL_PIPE_SIGNATURE_FETCH_PAYLOAD); }

  //! Add destination pixel format.
  BL_INLINE void setDstFormat(uint32_t v) noexcept { _set(BL_PIPE_SIGNATURE_DST_FORMAT, v); }
  //! Add source pixel format.
  BL_INLINE void setSrcFormat(uint32_t v) noexcept { _set(BL_PIPE_SIGNATURE_SRC_FORMAT, v); }
  //! Add clip mode.
  BL_INLINE void setCompOp(uint32_t v) noexcept { _set(BL_PIPE_SIGNATURE_COMP_OP, v); }
  //! Add sweep type.
  BL_INLINE void setFillType(uint32_t v) noexcept { _set(BL_PIPE_SIGNATURE_FILL_TYPE, v); }
  //! Add fetch type.
  BL_INLINE void setFetchType(uint32_t v) noexcept { _set(BL_PIPE_SIGNATURE_FETCH_TYPE, v); }
  //! Add fetch data.
  BL_INLINE void setFetchPayload(uint32_t v) noexcept { _set(BL_PIPE_SIGNATURE_FETCH_PAYLOAD, v); }

  // The following methods are used to build the signature. They use '|' operator
  // which doesn't clear the previous value, each function is expected to be called
  // only once when building a new signature.

  //! Combine with other signature.
  BL_INLINE void add(uint32_t v) noexcept { this->value |= v; }
  //! Combine with other signature.
  BL_INLINE void add(const BLPipeSignature& other) noexcept { this->value |= other.value; }

  //! Add destination pixel format.
  BL_INLINE void addDstFormat(uint32_t v) noexcept { _add(BL_PIPE_SIGNATURE_DST_FORMAT, v); }
  //! Add source pixel format.
  BL_INLINE void addSrcFormat(uint32_t v) noexcept { _add(BL_PIPE_SIGNATURE_SRC_FORMAT, v); }
  //! Add clip mode.
  BL_INLINE void addCompOp(uint32_t v) noexcept { _add(BL_PIPE_SIGNATURE_COMP_OP, v); }
  //! Add sweep type.
  BL_INLINE void addFillType(uint32_t v) noexcept { _add(BL_PIPE_SIGNATURE_FILL_TYPE, v); }
  //! Add fetch type.
  BL_INLINE void addFetchType(uint32_t v) noexcept { _add(BL_PIPE_SIGNATURE_FETCH_TYPE, v); }
  //! Add fetch data.
  BL_INLINE void addFetchPayload(uint32_t v) noexcept { _add(BL_PIPE_SIGNATURE_FETCH_PAYLOAD, v); }
};

//! \}
//! \endcond

#endif // BLEND2D_BLPIPE_P_H
