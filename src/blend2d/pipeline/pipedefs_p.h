// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_PIPEDEFS_P_H_INCLUDED
#define BLEND2D_PIPELINE_PIPEDEFS_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../compop_p.h"
#include "../format_p.h"
#include "../gradient_p.h"
#include "../matrix_p.h"
#include "../pattern_p.h"
#include "../runtime_p.h"
#include "../simd/simd_p.h"
#include "../support/memops_p.h"
#include "../tables/tables_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \namespace bl::Pipeline
//!
//! Blend2D pipeline.

//! \namespace bl::Pipeline::JIT
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

namespace bl {
namespace Pipeline {

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
  //! Fill mask command list.
  kMask = 2,
  //! Fill analytic non-zero/even-odd.
  kAnalytic = 3,

  //! Maximum value FillType can have.
  _kMaxValue = 3
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
  //! End or repeat (whether it repeats actually depends on repeat count, which is 1 for end).
  kEndOrRepeat = 0,
  //! Constant mask.
  kCMask = 1,
  //! Variable mask, already multiplied with global alpha.
  kVMaskA8WithGA = 2,
  //! Variable mask, which was not multiplied with global alpha.
  kVMaskA8WithoutGA = 3,

  _kMaxValue = 3
};

//! Fill rule mask used during composition of mask produced by analytic-rasterizer.
enum class FillRuleMask : uint32_t {
  kNonZeroMask = 0xFFFFFFFFu,
  kEvenOddMask = 0x000001FFu
};

//! Pipeline fetch-type.
//!
//! A unique id describing how pixels are fetched - supported fetchers include solid pixels, patterns (sometimes
//! referred as blits), and gradients.
//!
//! \note RoR is a shortcut for repeat-or-reflect - a universal fetcher for both.
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
  kGradientLinearNNPad,
  //!< Linear gradient (repeat or reflect) [Base].
  kGradientLinearNNRoR,

  //!< Linear gradient (pad) [Dither].
  kGradientLinearDitherPad,
  //!< Linear gradient (repeat or reflect) [Dither].
  kGradientLinearDitherRoR,

  //!< Radial gradient (pad) [Base].
  kGradientRadialNNPad,
  //!< Radial gradient (repeat or reflect) [Base].
  kGradientRadialNNRoR,

  //!< Radial gradient (pad) [Dither].
  kGradientRadialDitherPad,
  //!< Radial gradient (repeat or reflect) [Dither].
  kGradientRadialDitherRoR,

  //!< Conic gradient (any) [Base].
  kGradientConicNN,
  //!< Conic gradient (any) [Dither].
  kGradientConicDither,

  //!< Maximum value of a valid FetchType.
  _kMaxValue = kGradientConicDither,

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

  kGradientAnyFirst = kGradientLinearNNPad,
  kGradientAnyLast = kGradientConicDither,

  kGradientLinearFirst = kGradientLinearNNPad,
  kGradientLinearLast = kGradientLinearDitherRoR,

  kGradientRadialFirst = kGradientRadialNNPad,
  kGradientRadialLast = kGradientRadialDitherRoR,

  kGradientConicFirst = kGradientConicNN,
  kGradientConicLast = kGradientConicDither
};

typedef void (BL_CDECL* FillFunc)(ContextData* ctxData, const void* fillData, const void* fetchData) BL_NOEXCEPT;
typedef void (BL_CDECL* FetchFunc)(ContextData* ctxData, const void* fillData, const void* fetchData) BL_NOEXCEPT;

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
    kMaskDstFormat   = 0x0000000Fu, // (4 bits)
    kMaskSrcFormat   = 0x000000F0u, // (4 bits)
    kMaskCompOp      = 0x00003F00u, // (6 bits)
    kMaskFillType    = 0x0000C000u, // (2 bits)
    kMaskFetchType   = 0x001F0000u, // (5 bits)
    kMaskPendingFlag = 0x80000000u  // (1 bit)
  };

  //! \}

  //! \name Members
  //! \{

  //! Signature as a 32-bit value.
  uint32_t value;

  //! \}

  //! \name Static Constructors
  //! \{

  //! Returns a signature only containing a DstFormat.
  static BL_INLINE_NODEBUG constexpr Signature fromDstFormat(FormatExt format) noexcept { return Signature{uint32_t(format) << IntOps::bitShiftOf(kMaskDstFormat)}; }
  //! Returns a signature only containing a SrcFormat.
  static BL_INLINE_NODEBUG constexpr Signature fromSrcFormat(FormatExt format) noexcept { return Signature{uint32_t(format) << IntOps::bitShiftOf(kMaskSrcFormat)}; }
  //! Returns a signature only containing a CompOp.
  static BL_INLINE_NODEBUG constexpr Signature fromCompOp(CompOpExt compOp) noexcept { return Signature{uint32_t(compOp) << IntOps::bitShiftOf(kMaskCompOp)}; }
  //! Returns a signature only containing a FillType.
  static BL_INLINE_NODEBUG constexpr Signature fromFillType(FillType fillType) noexcept { return Signature{uint32_t(fillType) << IntOps::bitShiftOf(kMaskFillType)}; }
  //! Returns a signature only containing a FetchType.
  static BL_INLINE_NODEBUG constexpr Signature fromFetchType(FetchType fetchType) noexcept { return Signature{uint32_t(fetchType) << IntOps::bitShiftOf(kMaskFetchType)}; }
  //! Returns a signature only containing a PendingFlag.
  static BL_INLINE_NODEBUG constexpr Signature fromPendingFlag(uint32_t flag) noexcept { return  Signature{uint32_t(flag) << IntOps::bitShiftOf(kMaskPendingFlag)}; }

  //! \}

  BL_INLINE_NODEBUG bool operator==(const Signature& other) const noexcept { return value == other.value; }
  BL_INLINE_NODEBUG bool operator!=(const Signature& other) const noexcept { return value != other.value; }

  BL_INLINE_NODEBUG Signature operator|(const Signature& other) const noexcept { return Signature{value | other.value}; }
  BL_INLINE_NODEBUG Signature operator^(const Signature& other) const noexcept { return Signature{value ^ other.value}; }

  BL_INLINE_NODEBUG Signature& operator|=(const Signature& other) noexcept { value |= other.value; return *this; }
  BL_INLINE_NODEBUG Signature& operator^=(const Signature& other) noexcept { value ^= other.value; return *this; }

  BL_INLINE_NODEBUG uint32_t _get(uint32_t mask) const noexcept {
    return (this->value & mask) >> IntOps::bitShiftOf(mask);
  }

  BL_INLINE void _set(uint32_t mask, uint32_t v) noexcept {
    BL_ASSERT(v <= (mask >> IntOps::bitShiftOf(mask)));
    this->value = (this->value & ~mask) | (v << IntOps::bitShiftOf(mask));
  }

  BL_INLINE void _add(uint32_t mask, uint32_t v) noexcept {
    BL_ASSERT(v <= (mask >> IntOps::bitShiftOf(mask)));
    this->value |= (v << IntOps::bitShiftOf(mask));
  }

  //! Reset all values to zero.
  BL_INLINE_NODEBUG void reset() noexcept { this->value = 0; }
  //! Reset all values to `v`.
  BL_INLINE_NODEBUG void reset(uint32_t v) noexcept { this->value = v; }
  //! Reset all values to the `other` signature.
  BL_INLINE_NODEBUG void reset(const Signature& other) noexcept { this->value = other.value; }

  //! Set the signature from a packed 32-bit integer.
  BL_INLINE_NODEBUG void setValue(uint32_t v) noexcept { this->value = v; }
  //! Set the signature from another `Signature`.
  BL_INLINE_NODEBUG void setValue(const Signature& other) noexcept { this->value = other.value; }

  //! Extracts destination pixel format from the signature.
  BL_INLINE_NODEBUG FormatExt dstFormat() const noexcept { return FormatExt(_get(kMaskDstFormat)); }
  //! Extracts source pixel format from the signature.
  BL_INLINE_NODEBUG FormatExt srcFormat() const noexcept { return FormatExt(_get(kMaskSrcFormat)); }
  //! Extracts composition operator from the signature.
  BL_INLINE_NODEBUG CompOpExt compOp() const noexcept { return CompOpExt(_get(kMaskCompOp)); }
  //! Extracts sweep type from the signature.
  BL_INLINE_NODEBUG FillType fillType() const noexcept { return FillType(_get(kMaskFillType)); }
  //! Extracts fetch type from the signature.
  BL_INLINE_NODEBUG FetchType fetchType() const noexcept { return FetchType(_get(kMaskFetchType)); }
  //! Extracts pending flag from the signature.
  BL_INLINE_NODEBUG bool hasPendingFlag() const noexcept { return (value & kMaskPendingFlag) != 0u; }

  BL_INLINE_NODEBUG bool isSolid() const noexcept { return (value & kMaskFetchType) == 0u; }

  BL_INLINE_NODEBUG bool isGradient() const noexcept {
    return fetchType() >= FetchType::kGradientAnyFirst && fetchType() <= FetchType::kGradientAnyLast;
  }

  //! Add destination pixel format.
  BL_INLINE_NODEBUG void setDstFormat(FormatExt v) noexcept { _set(kMaskDstFormat, uint32_t(v)); }
  //! Add source pixel format.
  BL_INLINE_NODEBUG void setSrcFormat(FormatExt v) noexcept { _set(kMaskSrcFormat, uint32_t(v)); }
  //! Add clip mode.
  BL_INLINE_NODEBUG void setCompOp(CompOpExt v) noexcept { _set(kMaskCompOp, uint32_t(v)); }
  //! Add sweep type.
  BL_INLINE_NODEBUG void setFillType(FillType v) noexcept { _set(kMaskFillType, uint32_t(v)); }
  //! Add fetch type.
  BL_INLINE_NODEBUG void setFetchType(FetchType v) noexcept { _set(kMaskFetchType, uint32_t(v)); }

  // The following methods are used to build the signature. They use '|' operator
  // which doesn't clear the previous value, each function is expected to be called
  // only once when building a new signature.

  //! Combine with other signature.
  BL_INLINE_NODEBUG void add(uint32_t v) noexcept { this->value |= v; }
  //! Combine with other signature.
  BL_INLINE_NODEBUG void add(const Signature& other) noexcept { this->value |= other.value; }

  //! Add destination pixel format.
  BL_INLINE_NODEBUG void addDstFormat(FormatExt v) noexcept { _add(kMaskDstFormat, uint32_t(v)); }
  //! Add source pixel format.
  BL_INLINE_NODEBUG void addSrcFormat(FormatExt v) noexcept { _add(kMaskSrcFormat, uint32_t(v)); }
  //! Add clip mode.
  BL_INLINE_NODEBUG void addCompOp(CompOpExt v) noexcept { _add(kMaskCompOp, uint32_t(v)); }
  //! Add sweep type.
  BL_INLINE_NODEBUG void addFillType(FillType v) noexcept { _add(kMaskFillType, uint32_t(v)); }
  //! Add fetch type.
  BL_INLINE_NODEBUG void addFetchType(FetchType v) noexcept { _add(kMaskFetchType, uint32_t(v)); }

  BL_INLINE_NODEBUG void addPendingBit(uint32_t v) noexcept { _add(kMaskPendingFlag, v); }
  BL_INLINE_NODEBUG void clearPendingBit() noexcept { value &= ~kMaskPendingFlag; }
};

struct DispatchData {
  FillFunc fillFunc;
  FetchFunc fetchFunc;

  //! Initializes the dispatch data.
  //!
  //! If both `fillFuncInit` and `fetchFuncInit` are non-null the pipeline would be two-stage, if `fetchFunc` is
  //! null the pipeline would be one-stage. Typically JIT compiled pipelines are one-stage only (the fetch phase
  //! is inlined into the pipeline, but it's not a hard requirement).
  BL_INLINE void init(FillFunc fillFuncInit, FetchFunc fetchFuncInit = nullptr) noexcept {
    fillFunc = fillFuncInit;
    fetchFunc = fetchFuncInit;
  }

  //! Tests whether the dispatch data contains a one-stage pipeline.
  //!
  //! One-stage pipelines have no fetch function as it has been merged with fill function.
  BL_INLINE bool isOneStage() const noexcept { return fetchFunc == nullptr; }
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

  //! \name Members
  //! \{

  //! Start of the span, inclusive.
  uint32_t _x0;
  //! End of the span combined with command type, exclusive.
  //!
  //! \note Most people would add type into _x0 member, however, it's not good for most micro-architectures
  //! as today's CPUs are speculative and not knowing X0 would cause a lot of frontend cycle stalls due to
  //! not knowing the index on load.
  uint32_t _x1AndType;

  union {
    uintptr_t data;
    const void* ptr;
  } _value;

  //! Added to `_value.data` each time this command is processed by the filler.
  uintptr_t _maskAdvance;

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG MaskCommandType type() const noexcept { return MaskCommandType(_x1AndType & kTypeMask); }
  BL_INLINE_NODEBUG uint32_t x0() const noexcept { return _x0; }
  BL_INLINE_NODEBUG uint32_t x1() const noexcept { return _x1AndType >> kTypeBits; }

  BL_INLINE_NODEBUG uint32_t repeatCount() const noexcept { return _x0; }
  BL_INLINE_NODEBUG void updateRepeatCount(uint32_t value) noexcept { _x0 = value; }

  BL_INLINE_NODEBUG bool isConstMask() const noexcept { return type() == MaskCommandType::kCMask; }

  BL_INLINE_NODEBUG uint32_t maskValue() const noexcept { return uint32_t(_value.data); }
  BL_INLINE_NODEBUG const void* maskData() const noexcept { return _value.ptr; }

  BL_INLINE_NODEBUG intptr_t maskAdvance() const noexcept { return _maskAdvance; }

  BL_INLINE void initTypeAndSpan(MaskCommandType type, uint32_t x0, uint32_t x1) noexcept {
    BL_ASSERT(((x1 << kTypeBits) >> kTypeBits) == x1);
    _x0 = x0;
    _x1AndType = uint32_t(type) | (x1 << kTypeBits);
  }

  BL_INLINE void initCMask(MaskCommandType cmdType, uint32_t x0, uint32_t x1, uint32_t maskValue) noexcept {
    initTypeAndSpan(cmdType, x0, x1);
    _value.data = maskValue;
    _maskAdvance = 0;
  }

  BL_INLINE void initVMask(MaskCommandType type, uint32_t x0, uint32_t x1, const void* maskData, intptr_t maskAdvance = 0) noexcept {
    initTypeAndSpan(type, x0, x1);
    _value.ptr = maskData;
    _maskAdvance = uintptr_t(maskAdvance);
  }

  BL_INLINE void initCMaskA8(uint32_t x0, uint32_t x1, uint32_t maskValue) noexcept {
    initCMask(MaskCommandType::kCMask, x0, x1, maskValue);
  }

  BL_INLINE void initVMaskA8WithGA(uint32_t x0, uint32_t x1, const void* maskData, intptr_t maskAdvance = 0) noexcept {
    initVMask(MaskCommandType::kVMaskA8WithGA, x0, x1, maskData, maskAdvance);
  }

  BL_INLINE void initVMaskA8WithoutGA(uint32_t x0, uint32_t x1, const void* maskData, intptr_t maskAdvance = 0) noexcept {
    initVMask(MaskCommandType::kVMaskA8WithoutGA, x0, x1, maskData, maskAdvance);
  }

  BL_INLINE void initEnd() noexcept {
    initTypeAndSpan(MaskCommandType::kEndOrRepeat, 1, 0);
  }

  BL_INLINE void initRepeat(uint32_t nRepeat = 0xFFFFFFFFu) noexcept {
    initTypeAndSpan(MaskCommandType::kEndOrRepeat, nRepeat, 0);
  }

  //! \}
};

//! Contains data that is required to decompose a BoxU fill into mask commands.
struct BoxUToMaskData {
  // At most 4 commands per scanline, at most 3 distinct scanlines.
  MaskCommand maskCmd[4u * 3u];
  // At most 32 bytes per scanline, at most 3 distinct scanlines.
  uint8_t maskData[32 * 3u];
};

struct ContextData {
  BLImageData dst;
  BLPointI pixelOrigin;

  BL_INLINE void reset() noexcept { *this = ContextData{}; }
};

static BL_INLINE void writeBoxUMaskToMaskBuffer(uint8_t* dst, uint32_t m) noexcept {
#if BL_TARGET_ARCH_BITS >= 64
  uint64_t repeated = uint64_t(m) * 0x0101010101010101u;
  uint64_t mask = BL_BYTE_ORDER == 1234 ? 0xFFFFFFFF00000000u : 0x00000000FFFFFFFFu;
  MemOps::writeU64a(dst + 24, repeated);
  MemOps::writeU64a(dst + 16, repeated);
  MemOps::writeU64a(dst +  8, repeated);
  MemOps::writeU64a(dst +  0, repeated & mask);
#else
  uint32_t repeated = m * 0x01010101u;
  MemOps::writeU32a(dst +  0, 0u);
  MemOps::writeU32a(dst +  4, repeated);
  MemOps::writeU32a(dst +  8, repeated);
  MemOps::writeU32a(dst + 12, repeated);
  MemOps::writeU32a(dst + 16, repeated);
  MemOps::writeU32a(dst + 20, repeated);
  MemOps::writeU32a(dst + 24, repeated);
  MemOps::writeU32a(dst + 28, repeated);
#endif
}

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
  BL_INLINE bool initBoxU8bpcT(uint32_t alpha, T x0, T y0, T x1, T y1, BoxUToMaskData& maskData) noexcept {
    return initBoxU8bpc24x8(alpha,
      Math::truncToInt(x0 * T(256)),
      Math::truncToInt(y0 * T(256)),
      Math::truncToInt(x1 * T(256)),
      Math::truncToInt(y1 * T(256)),
      maskData);
  }

  bool initBoxU8bpc24x8(uint32_t alpha, int x0, int y0, int x1, int y1, BoxUToMaskData& maskData) noexcept {
    // The rendering engine should never pass out-of-range alpha.
    BL_ASSERT(alpha <= 255);

    // The rendering engine should never pass invalid box to the pipeline.
    BL_ASSERT(x0 < x1);
    BL_ASSERT(y0 < y1);

    constexpr uint32_t kInnerAlignment = 8;
    constexpr uint32_t kMaskScanlineWidth = 32;
    constexpr uint32_t kMaxMaskOnlyWidth = 20;

    uint32_t ax0 = uint32_t(x0) >> 8;
    uint32_t ay0 = uint32_t(y0) >> 8;
    uint32_t ax1 = uint32_t(x1 + 0xFF) >> 8;
    uint32_t ay1 = uint32_t(y1 + 0xFF) >> 8;

    uint32_t fx0 = uint32_t(x0) & 0xFFu;
    uint32_t fy0 = uint32_t(y0) & 0xFFu;
    uint32_t fx1 = (uint32_t(x1 - 1) & 0xFFu) + 1u;
    uint32_t fy1 = (uint32_t(y1 - 1) & 0xFFu) + 1u;

    uint32_t w = ax1 - ax0;
    uint32_t h = ay1 - ay0;

    fy0 = (h == 1 ? fy1 : 256u) - fy0;

    uint32_t fy0_a = fy0 * alpha;
    uint32_t fy1_a = fy1 * alpha;

    MaskCommand* maskCmd = maskData.maskCmd;
    uint8_t* maskPtr = maskData.maskData;

    mask.alpha.u = 0xFF;
    mask.box.reset(int(ax0), int(ay0), int(ax0 + w), int(ay0 + h));
    mask.maskCommandData = maskCmd;

    // Special cases first - smaller the rectangle => greater the overhead per pixel if we do unnecessary work.
    if (w == 1) {
      // If the rectangle has 1 pixel width, we have to sum fx0 and fx1 to calculate the mask value. This is
      // not needed for a regular case in which the width is greater than 1 - in that case there are always
      // two bordering pixels, which masks are calculated separately.
      fx0 = fx1 - fx0;

      uint32_t m0 = (fx0 * fy0_a) >> 16u;
      maskCmd[0].initCMaskA8(ax0, ax1, m0);
      maskCmd[1].initEnd();

      if (h == 1)
        return m0 != 0;

      maskCmd += m0 ? 2u : 0u;
      mask.box.y0 += int(m0 == 0);

      uint32_t m1 = (fx0 * alpha) >> 8u;
      maskCmd[0].initCMaskA8(ax0, ax1, m1);
      maskCmd[1].initRepeat(h - 2);
      maskCmd += h > 2 ? 2u : 0u;

      uint32_t m2 = (fx0 * fy1_a) >> 16u;
      maskCmd[0].initCMaskA8(ax0, ax1, m2);
      maskCmd[1].initEnd();

      mask.box.y1 -= int(m2 == 0);
      return mask.box.y0 < mask.box.y1 && m1 != 0;
    }

    // Common case - if width > 1 then we don't have to worry about fx0 and fx1 - both represent a different pixel.
    uint32_t m0x1 = fy0_a >> 8u;
    uint32_t m1x1 = alpha;
    uint32_t m2x1 = fy1_a >> 8u;

    fx0 = 256 - fx0;

    if ((fx0 & fx1) == 256) {
      // If the rectangle doesn't have a fractional X0/X1 then each scanline would only need a single CMask
      // command instead of either VMask or [VMask, CMask, VMask] sequence.
      maskCmd[0].initCMaskA8(ax0, ax1, m0x1);
      maskCmd[1].initEnd();
      maskCmd += m0x1 ? 2u : 0u;
      mask.box.y0 += int(m0x1 == 0);

      maskCmd[0].initCMaskA8(ax0, ax1, m1x1);
      maskCmd[1].initRepeat(h - 2);
      maskCmd += h > 2 ? 2u : 0u;

      maskCmd[0].initCMaskA8(ax0, ax1, m2x1);
      maskCmd[1].initEnd();
      mask.box.y1 -= int(m2x1 == 0);

      return mask.box.y0 < mask.box.y1;
    }

    uint32_t m0x0 = (fx0 * fy0_a) >> 16u;
    uint32_t m0x2 = (fx1 * fy0_a) >> 16u;
    writeBoxUMaskToMaskBuffer(maskPtr + kMaskScanlineWidth * 0u, m0x1);
    maskPtr[kMaskScanlineWidth * 0u + 4] = uint8_t(m0x0);

    uint32_t m1x0 = (fx0 * alpha) >> 8u;
    uint32_t m1x2 = (fx1 * alpha) >> 8u;
    writeBoxUMaskToMaskBuffer(maskPtr + kMaskScanlineWidth * 1u, m1x1);
    maskPtr[kMaskScanlineWidth * 1u + 4] = uint8_t(m1x0);

    uint32_t m2x0 = (fx0 * fy1_a) >> 16u;
    uint32_t m2x2 = (fx1 * fy1_a) >> 16u;
    writeBoxUMaskToMaskBuffer(maskPtr + kMaskScanlineWidth * 2u, m2x1);
    maskPtr[kMaskScanlineWidth * 2u + 4] = uint8_t(m2x0);

    maskPtr += 4;
    uint32_t wAlign = IntOps::alignUpDiff(w, 4);

    if (wAlign > ax0)
      wAlign = 0;

    ax0 -= wAlign;
    w += wAlign;
    maskPtr -= wAlign;

    if (w <= kMaxMaskOnlyWidth) {
      maskPtr[kMaskScanlineWidth * 0u + w - 1u] = uint8_t(m0x2);
      maskPtr[kMaskScanlineWidth * 1u + w - 1u] = uint8_t(m1x2);
      maskPtr[kMaskScanlineWidth * 2u + w - 1u] = uint8_t(m2x2);

      maskCmd[0].initVMaskA8WithGA(ax0, ax1, maskPtr + kMaskScanlineWidth * 0u, 0);
      maskCmd[1].initEnd();
      maskCmd += m0x1 ? 2u : 0u;

      mask.box.y0 += int(m0x1 == 0);

      maskCmd[0].initVMaskA8WithGA(ax0, ax1, maskPtr + kMaskScanlineWidth * 1u, 0);
      maskCmd[1].initRepeat(h - 2);
      maskCmd += h > 2 ? 2u : 0u;

      maskCmd[0].initVMaskA8WithGA(ax0, ax1, maskPtr + kMaskScanlineWidth * 2u, 0);
      maskCmd[1].initEnd();
      mask.box.y1 -= int(m2x1 == 0);

      return mask.box.y0 < mask.box.y1;
    }
    else {
      uint32_t innerWidth = IntOps::alignDown(w - 5, kInnerAlignment);
      uint32_t innerEnd = ax0 + 4u + innerWidth;
      uint32_t tailWidth = ax1 - innerEnd;

      const uint8_t* maskTail = maskPtr + 16 - tailWidth;

      maskPtr[kMaskScanlineWidth * 0u + 15u] = uint8_t(m0x2);
      maskPtr[kMaskScanlineWidth * 1u + 15u] = uint8_t(m1x2);
      maskPtr[kMaskScanlineWidth * 2u + 15u] = uint8_t(m2x2);

      maskCmd[0].initVMaskA8WithGA(ax0, ax0 + 4u, maskPtr + kMaskScanlineWidth * 0u, 0);
      maskCmd[1].initCMaskA8(ax0 + 4u, innerEnd, m0x1);
      maskCmd[2].initVMaskA8WithGA(innerEnd, ax1, maskTail + kMaskScanlineWidth * 0u, 0);
      maskCmd[3].initEnd();
      maskCmd += m0x1 ? 4u : 0u;
      mask.box.y0 += int(m0x1 == 0);

      maskCmd[0].initVMaskA8WithGA(ax0, ax0 + 4u, maskPtr + kMaskScanlineWidth * 1u, 0);
      maskCmd[1].initCMaskA8(ax0 + 4u, innerEnd, m1x1);
      maskCmd[2].initVMaskA8WithGA(innerEnd, ax1, maskTail + kMaskScanlineWidth * 1u, 0);
      maskCmd[3].initRepeat(h - 2);
      maskCmd += h > 2 ? 4u : 0u;

      maskCmd[0].initVMaskA8WithGA(ax0, ax0 + 4u, maskPtr + kMaskScanlineWidth * 2u, 0);
      maskCmd[1].initCMaskA8(ax0 + 4u, innerEnd, m2x1);
      maskCmd[2].initVMaskA8WithGA(innerEnd, ax1, maskTail + kMaskScanlineWidth * 2u, 0);
      maskCmd[3].initEnd();
      mask.box.y1 -= int(m2x1 == 0);

      return mask.box.y0 < mask.box.y1;
    }
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
      //! 64-bit ARGB, premultiplied.
      uint64_t prgb64;

      struct {
#if BL_BYTE_ORDER == 1234
        //! 32-bit ARGB, premultiplied.
        uint32_t prgb32;
        //! Reserved in case 32-bit data is used.
        uint32_t reserved32;
#else
        //! Reserved in case 32-bit data is used.
        uint32_t reserved32;
        //! 32-bit ARGB, premultiplied.
        uint32_t prgb32;
#endif
      };
    };
  };

  //! Pattern fetch data.
  struct alignas(16) Pattern {
    //! Source image data.
    struct SourceData {
      const uint8_t* pixelData;
      intptr_t stride;
      BLSizeI size;
    };

    struct AlignedBlit {
      //! Translate by x/y (inverted).
      int32_t tx, ty;
    };

    //! Extend data used by pipelines to handle vertical PAD, REPEAT, and REFLECT extend modes dynamically.
    struct VertExtendData {
      //! Stride and alternative stride:
      //!
      //!   - PAD    : [src.stride, 0]
      //!   - REPEAT : [src.stride, src.stride]
      //!   - REFLECT: [src.stride,-src.stride]
      intptr_t stride[2];

      //! Y-stop and alternative y-stop:
      //!
      //!   - PAD    : [src.size.h, 0]
      //!   - REPEAT : [src.size.h, src.size.h]
      //!   - REFLECT: [src.size.h, src.size.h]
      uintptr_t yStop[2];

      //! Offset that is applied to Y variable when the scanline reaches a local y-stop.
      //!
      //! This value must be 0 in PAD case and `src.size.h` in REPEAT or REFLECT case.
      uintptr_t yRewindOffset;

      //! Offset that is applied to pixel data when the scanline reaches a local y-stop.
      //!
      //! This value must be 0 in PAD or REFLECT case, and `src.size.h - 1 * stride` in REPEAT case.
      intptr_t pixelPtrRewindOffset;
    };

    //! Simple pattern data (only identity or translation matrix).
    struct alignas(16) Simple {
      //! Translate by x/y (inverted).
      int32_t tx, ty;
      //! Repeat/Reflect w/h.
      int32_t rx, ry;
      //! Safe X increments by 1..16 (fetchN).
      ModuloTable ix;
      //! 9-bit or 17-bit weight at [0, 0] (A).
      uint32_t wa;
      //! 9-bit or 17-bit weight at [1, 0] (B).
      uint32_t wb;
      //! 9-bit or 17-bit weight at [0, 1] (C).
      uint32_t wc;
      //! 9-bit or 17-bit weight at [1, 1] (D).
      uint32_t wd;

      //! Vertical extend data.
      VertExtendData vExtendData;
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

      union {
        //! 16-bit multipliers to be used by [V]PMADDWD instruction to calculate address from Y/X pairs.
        int16_t addrMul16[2];
        //! 32-bit multipliers for X and Y coordinates.
        int32_t addrMul32[2];
      };
    };

    //! Source image data.
    SourceData src;

    union {
      //! Simple pattern data.
      Simple simple;
      //! Affine pattern data.
      Affine affine;
    };
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

      //! Maximum index value taking into account pad, repeat, and reflection - `(repeated_or_reflected_size - 1)`.
      uint32_t maxi;
      //! Repeat/Reflect mask to apply to index (either `reflected_size - 1` or `zero`).
      uint32_t rori;
    };

    //! Radial gradient data.
    struct alignas(16) Radial {
      //! Gradient X/Y offsets at [0, 0].
      double tx, ty;
      //! Gradient X/Y increments (vertical).
      double yx, yy;

      double amul4, inv2a;
      double sq_fr, sq_inv2a;

      double b0, dd0;
      double by, ddy;

      float f32_ddd;
      float f32_bd;

      //! Maximum index value taking into account pad, repeat, and reflection - `(repeated_or_reflected_size - 1)`.
      uint32_t maxi;
      //! Repeat/Reflect mask to apply to index (either `reflected_size - 1` or `zero`).
      uint32_t rori;
    };

    //! Conic gradient data.
    struct alignas(16) Conic {
      //! Gradient X/Y offsets of the pixel at [0, 0].
      double tx, ty;
      //! Gradient X/Y increments (vertical).
      double yx, yy;

      //! Atan approximation coefficients.
      float q_coeff[4];
      //! Table size divided by 1, 2, and 4.
      float n_div_1_2_4[3];
      //! Angle offset.
      float offset;

      //! Gradient X increment (horizontal)
      //!
      //! \note There is no Y increment in X direction as the transformation matrix has been rotated in a way to
      //! make it zero, which simplifies computation requirements per pixel.
      float xx;

      //! Maximum index value - `lut.size - 1`.
      uint32_t maxi;
      //! Repeat mask to apply to index.
      uint32_t rori;
    };

    //! Precomputed lookup table.
    LUT lut;
    //! Union of all possible gradient data types.
    union {
      //! Linear gradient specific data.
      Linear linear;
      //! Radial gradient specific data.
      Radial radial;
      //! Conic gradient specific data.
      Conic conic;
    };
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
};

namespace FetchUtils {

static BL_INLINE void initImageSource(FetchData::Pattern& fetchData, const uint8_t* pixelData, intptr_t stride, int w, int h) noexcept {
  fetchData.src.pixelData = pixelData;
  fetchData.src.stride = stride;
  fetchData.src.size.reset(w, h);
}

static BL_INLINE Signature initPatternBlit(FetchData::Pattern& fetchData, int x, int y) noexcept {
  fetchData.simple.tx = x;
  fetchData.simple.ty = y;
  fetchData.simple.rx = 0;
  fetchData.simple.ry = 0;
  return Signature::fromFetchType(FetchType::kPatternAlignedBlit);
}

Signature initPatternAxAy(
  FetchData::Pattern& fetchData,
  BLExtendMode extendMode,
  int x, int y) noexcept;

Signature initPatternFxFy(
  FetchData::Pattern& fetchData,
  BLExtendMode extendMode,
  BLPatternQuality quality,
  uint32_t bytesPerPixel,
  int64_t tx64, int64_t ty64) noexcept;

Signature initPatternAffine(
  FetchData::Pattern& fetchData,
  BLExtendMode extendMode,
  BLPatternQuality quality,
  uint32_t bytesPerPixel,
  const BLMatrix2D& transform) noexcept;

Signature initGradient(
  FetchData::Gradient& fetchData,
  BLGradientType gradientType,
  BLExtendMode extendMode,
  BLGradientQuality quality,
  const void* values,
  const void* lutData, uint32_t lutSize,
  const BLMatrix2D& transform) noexcept;

} // {FetchUtils}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_PIPEDEFS_P_H_INCLUDED
