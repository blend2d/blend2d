// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPECOMPILER_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPECOMPILER_P_H_INCLUDED

#include "../../runtime_p.h"
#include "../../pipeline/jit/jitbase_p.h"

// TODO: [JIT] The intention is to have it as independent as possible, so remove this in the future!
#include "../../pipeline/jit/pipeprimitives_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {

//! The behavior of a floating point scalar operation.
enum class ScalarOpBehavior : uint8_t {
  //! The rest of the elements are zeroed, only the first element would contain the result (AArch64).
  kZeroing,
  //! The rest of the elements are unchanged, elements above 128-bits are zeroed.
  kPreservingVec128
};

//! The behavior of a floating point min/max instructions when comparing against NaN.
enum class FMinMaxOpBehavior : uint8_t {
  //! Min and max selects a finite value if one of the compared values is NaN.
  kFiniteValue,
  //! Min and max is implemented like `if a <|> b ? a : b`.
  kTernaryLogic
};

//! The behavior of floating point `madd` instructions.
enum class FMulAddOpBehavior : uint8_t {
  //! FMA is not available, thus `madd` is translated into two instructions (MUL + ADD).
  kNoFMA,
  //! FMA is available, the ISA allows to store the result to any of the inputs (X86).
  kFMAStoreToAny,
  //! FMA is available, the ISA always uses accumulator register as a destination register (AArch64).
  kFMAStoreToAccumulator
};

enum class OpcodeCond : uint32_t {
  kAssignAnd,
  kAssignOr,
  kAssignXor,
  kAssignAdd,
  kAssignSub,
  kAssignShr,
  kTest,
  kBitTest,
  kCompare,

  kMaxValue = kCompare
};

enum class OpcodeM : uint32_t {
  kStoreZeroReg,
  kStoreZeroU8,
  kStoreZeroU16,
  kStoreZeroU32,
  kStoreZeroU64
};

enum class OpcodeRM : uint32_t {
  kLoadReg,
  kLoadI8,
  kLoadU8,
  kLoadI16,
  kLoadU16,
  kLoadI32,
  kLoadU32,
  kLoadI64,
  kLoadU64,
  kLoadMergeU8,
  kLoadShiftU8,
  kLoadMergeU16,
  kLoadShiftU16
};

enum class OpcodeMR : uint32_t {
  kStoreReg,
  kStoreU8,
  kStoreU16,
  kStoreU32,
  kStoreU64,
  kAddReg,
  kAddU8,
  kAddU16,
  kAddU32,
  kAddU64
};

//! Arithmetic operation having 2 operands (dst, src).
enum class OpcodeRR : uint32_t {
  kAbs,
  kNeg,
  kNot,
  kBSwap,
  kCLZ,
  kCTZ,
  kReflect,
  kMaxValue = kReflect
};

//! Arithmetic operation having 3 operands (dst, src1, src2).
enum class OpcodeRRR : uint32_t {
  kAnd,
  kOr,
  kXor,
  kBic,
  kAdd,
  kSub,
  kMul,
  kUDiv,
  kUMod,
  kSMin,
  kSMax,
  kUMin,
  kUMax,
  kSll,
  kSrl,
  kSra,
  kRol,
  kRor,
  kSBound,

  kMaxValue = kSBound
};

enum class OpcodeVR : uint32_t {
  kMov,
  kMovU32,
  kMovU64,
  kInsertU8,
  kInsertU16,
  kInsertU32,
  kInsertU64,
  kExtractU8,
  kExtractU16,
  kExtractU32,
  kExtractU64,
  kCvtIntToF32,
  kCvtIntToF64,
  kCvtTruncF32ToInt,
  kCvtRoundF32ToInt,
  kCvtTruncF64ToInt,
  kCvtRoundF64ToInt,

  kMaxValue = kCvtRoundF64ToInt
};

enum class OpcodeVM : uint32_t {
  kLoad8,
  kLoad16_U16,
  kLoad32_U32,
  kLoad32_F32,

  kLoad64_U32,
  kLoad64_U64,
  kLoad64_F32,
  kLoad64_F64,

  kLoad128_U32,
  kLoad128_U64,
  kLoad128_F32,
  kLoad128_F64,

  kLoad256_U32,
  kLoad256_U64,
  kLoad256_F32,
  kLoad256_F64,

  kLoad512_U32,
  kLoad512_U64,
  kLoad512_F32,
  kLoad512_F64,

  kLoadN_U32,
  kLoadN_U64,
  kLoadN_F32,
  kLoadN_F64,

  kLoadCvt16_U8ToU64,
  kLoadCvt32_U8ToU64,
  kLoadCvt64_U8ToU64,

  kLoadCvt32_I8ToI16,
  kLoadCvt32_U8ToU16,
  kLoadCvt32_I8ToI32,
  kLoadCvt32_U8ToU32,
  kLoadCvt32_I16ToI32,
  kLoadCvt32_U16ToU32,
  kLoadCvt32_I32ToI64,
  kLoadCvt32_U32ToU64,

  kLoadCvt64_I8ToI16,
  kLoadCvt64_U8ToU16,
  kLoadCvt64_I8ToI32,
  kLoadCvt64_U8ToU32,
  kLoadCvt64_I16ToI32,
  kLoadCvt64_U16ToU32,
  kLoadCvt64_I32ToI64,
  kLoadCvt64_U32ToU64,

  kLoadCvt128_I8ToI16,
  kLoadCvt128_U8ToU16,
  kLoadCvt128_I8ToI32,
  kLoadCvt128_U8ToU32,
  kLoadCvt128_I16ToI32,
  kLoadCvt128_U16ToU32,
  kLoadCvt128_I32ToI64,
  kLoadCvt128_U32ToU64,

  kLoadCvt256_I8ToI16,
  kLoadCvt256_U8ToU16,
  kLoadCvt256_I16ToI32,
  kLoadCvt256_U16ToU32,
  kLoadCvt256_I32ToI64,
  kLoadCvt256_U32ToU64,

  kLoadCvtN_U8ToU64,

  kLoadCvtN_I8ToI16,
  kLoadCvtN_U8ToU16,
  kLoadCvtN_I8ToI32,
  kLoadCvtN_U8ToU32,
  kLoadCvtN_I16ToI32,
  kLoadCvtN_U16ToU32,
  kLoadCvtN_I32ToI64,
  kLoadCvtN_U32ToU64,

  kLoadInsertU8,
  kLoadInsertU16,
  kLoadInsertU32,
  kLoadInsertU64,
  kLoadInsertF32,
  kLoadInsertF32x2,
  kLoadInsertF64,

  kMaxValue = kLoadInsertF64
};

enum class OpcodeMV : uint32_t {
  kStore8,
  kStore16_U16,
  kStore32_U32,
  kStore32_F32,

  kStore64_U32,
  kStore64_U64,
  kStore64_F32,
  kStore64_F64,

  kStore128_U32,
  kStore128_U64,
  kStore128_F32,
  kStore128_F64,

  kStore256_U32,
  kStore256_U64,
  kStore256_F32,
  kStore256_F64,

  kStore512_U32,
  kStore512_U64,
  kStore512_F32,
  kStore512_F64,

  kStoreN_U32,
  kStoreN_U64,
  kStoreN_F32,
  kStoreN_F64,

  kStoreExtractU16,
  kStoreExtractU32,
  kStoreExtractU64,

  /*
  kStoreCvtz64_U16ToU8,
  kStoreCvtz64_U32ToU16,
  kStoreCvtz64_U64ToU32,
  kStoreCvts64_I16ToI8,
  kStoreCvts64_I16ToU8,
  kStoreCvts64_U16ToU8,
  kStoreCvts64_I32ToI16,
  kStoreCvts64_U32ToU16,
  kStoreCvts64_I64ToI32,
  kStoreCvts64_U64ToU32,

  kStoreCvtz128_U16ToU8,
  kStoreCvtz128_U32ToU16,
  kStoreCvtz128_U64ToU32,
  kStoreCvts128_I16ToI8,
  kStoreCvts128_I16ToU8,
  kStoreCvts128_U16ToU8,
  kStoreCvts128_I32ToI16,
  kStoreCvts128_U32ToU16,
  kStoreCvts128_I64ToI32,
  kStoreCvts128_U64ToU32,

  kStoreCvtz256_U16ToU8,
  kStoreCvtz256_U32ToU16,
  kStoreCvtz256_U64ToU32,
  kStoreCvts256_I16ToI8,
  kStoreCvts256_I16ToU8,
  kStoreCvts256_U16ToU8,
  kStoreCvts256_I32ToI16,
  kStoreCvts256_U32ToU16,
  kStoreCvts256_I64ToI32,
  kStoreCvts256_U64ToU32,

  kStoreCvtzN_U16ToU8,
  kStoreCvtzN_U32ToU16,
  kStoreCvtzN_U64ToU32,
  kStoreCvtsN_I16ToI8,
  kStoreCvtsN_I16ToU8,
  kStoreCvtsN_U16ToU8,
  kStoreCvtsN_I32ToI16,
  kStoreCvtsN_U32ToU16,
  kStoreCvtsN_I64ToI32,
  kStoreCvtsN_U64ToU32,
  */

  kMaxValue = kStoreExtractU64
};

enum class OpcodeVV : uint32_t {
  kMov,
  kMovU64,

  kBroadcastU8Z,
  kBroadcastU16Z,
  kBroadcastU8,
  kBroadcastU16,
  kBroadcastU32,
  kBroadcastU64,
  kBroadcastF32,
  kBroadcastF64,
  kBroadcastV128_U32,
  kBroadcastV128_U64,
  kBroadcastV128_F32,
  kBroadcastV128_F64,
  kBroadcastV256_U32,
  kBroadcastV256_U64,
  kBroadcastV256_F32,
  kBroadcastV256_F64,

  kAbsI8,
  kAbsI16,
  kAbsI32,
  kAbsI64,

  kNotU32,
  kNotU64,

  kCvtI8LoToI16,
  kCvtI8HiToI16,
  kCvtU8LoToU16,
  kCvtU8HiToU16,
  kCvtI8ToI32,
  kCvtU8ToU32,
  kCvtI16LoToI32,
  kCvtI16HiToI32,
  kCvtU16LoToU32,
  kCvtU16HiToU32,
  kCvtI32LoToI64,
  kCvtI32HiToI64,
  kCvtU32LoToU64,
  kCvtU32HiToU64,

  kAbsF32,
  kAbsF64,

  kNegF32,
  kNegF64,

  kNotF32,
  kNotF64,

  kTruncF32S,
  kTruncF64S,
  kTruncF32,
  kTruncF64,

  kFloorF32S,
  kFloorF64S,
  kFloorF32,
  kFloorF64,

  kCeilF32S,
  kCeilF64S,
  kCeilF32,
  kCeilF64,

  kRoundF32S,
  kRoundF64S,
  kRoundF32,
  kRoundF64,

  kRcpF32,
  kRcpF64,

  kSqrtF32S,
  kSqrtF64S,
  kSqrtF32,
  kSqrtF64,

  kCvtF32ToF64S,
  kCvtF64ToF32S,
  kCvtI32ToF32,
  kCvtF32LoToF64,
  kCvtF32HiToF64,
  kCvtF64ToF32Lo,
  kCvtF64ToF32Hi,
  kCvtI32LoToF64,
  kCvtI32HiToF64,
  kCvtTruncF32ToI32,
  kCvtTruncF64ToI32Lo,
  kCvtTruncF64ToI32Hi,
  kCvtRoundF32ToI32,
  kCvtRoundF64ToI32Lo,
  kCvtRoundF64ToI32Hi,

  kMaxValue = kCvtRoundF64ToI32Hi
};

enum class OpcodeVVI : uint32_t {
  kSllU16,
  kSllU32,
  kSllU64,
  kSrlU16,
  kSrlU32,
  kSrlU64,
  kSraI16,
  kSraI32,
  kSraI64,
  kSllbU128,
  kSrlbU128,
  kSwizzleU16x4,
  kSwizzleLoU16x4,
  kSwizzleHiU16x4,
  kSwizzleU32x4,
  kSwizzleU64x2,
  kSwizzleF32x4,
  kSwizzleF64x2,
  kSwizzleU64x4,
  kSwizzleF64x4,
  kExtractV128_I32,
  kExtractV128_I64,
  kExtractV128_F32,
  kExtractV128_F64,
  kExtractV256_I32,
  kExtractV256_I64,
  kExtractV256_F32,
  kExtractV256_F64,

#if defined(BL_JIT_ARCH_A64)
  kSrlRndU16,
  kSrlRndU32,
  kSrlRndU64,
  kSrlAccU16,
  kSrlAccU32,
  kSrlAccU64,
  kSrlRndAccU16,
  kSrlRndAccU32,
  kSrlRndAccU64,
  kSrlnLoU16,
  kSrlnHiU16,
  kSrlnLoU32,
  kSrlnHiU32,
  kSrlnLoU64,
  kSrlnHiU64,
  kSrlnRndLoU16,
  kSrlnRndHiU16,
  kSrlnRndLoU32,
  kSrlnRndHiU32,
  kSrlnRndLoU64,
  kSrlnRndHiU64,

  kMaxValue = kSrlnRndHiU64

#elif defined(BL_JIT_ARCH_X86)

  kMaxValue = kExtractV256_F64

#else

  kMaxValue = kExtractV256_F64

#endif // BL_JIT_ARCH_A64
};

enum class OpcodeVVV : uint32_t {
  kAndU32,
  kAndU64,
  kOrU32,
  kOrU64,
  kXorU32,
  kXorU64,
  kAndnU32,
  kAndnU64,
  kBicU32,
  kBicU64,
  kAvgrU8,
  kAvgrU16,
  kAddU8,
  kAddU16,
  kAddU32,
  kAddU64,
  kSubU8,
  kSubU16,
  kSubU32,
  kSubU64,
  kAddsI8,
  kAddsU8,
  kAddsI16,
  kAddsU16,
  kSubsI8,
  kSubsU8,
  kSubsI16,
  kSubsU16,
  kMulU16,
  kMulU32,
  kMulU64,
  kMulhI16,
  kMulhU16,
  kMulU64_LoU32,
  kMHAddI16_I32,
  kMinI8,
  kMinU8,
  kMinI16,
  kMinU16,
  kMinI32,
  kMinU32,
  kMinI64,
  kMinU64,
  kMaxI8,
  kMaxU8,
  kMaxI16,
  kMaxU16,
  kMaxI32,
  kMaxU32,
  kMaxI64,
  kMaxU64,
  kCmpEqU8,
  kCmpEqU16,
  kCmpEqU32,
  kCmpEqU64,
  kCmpGtI8,
  kCmpGtU8,
  kCmpGtI16,
  kCmpGtU16,
  kCmpGtI32,
  kCmpGtU32,
  kCmpGtI64,
  kCmpGtU64,
  kCmpGeI8,
  kCmpGeU8,
  kCmpGeI16,
  kCmpGeU16,
  kCmpGeI32,
  kCmpGeU32,
  kCmpGeI64,
  kCmpGeU64,
  kCmpLtI8,
  kCmpLtU8,
  kCmpLtI16,
  kCmpLtU16,
  kCmpLtI32,
  kCmpLtU32,
  kCmpLtI64,
  kCmpLtU64,
  kCmpLeI8,
  kCmpLeU8,
  kCmpLeI16,
  kCmpLeU16,
  kCmpLeI32,
  kCmpLeU32,
  kCmpLeI64,
  kCmpLeU64,

  kAndF32,
  kAndF64,
  kOrF32,
  kOrF64,
  kXorF32,
  kXorF64,
  kAndnF32,
  kAndnF64,
  kBicF32,
  kBicF64,
  kAddF32S,
  kAddF64S,
  kAddF32,
  kAddF64,
  kSubF32S,
  kSubF64S,
  kSubF32,
  kSubF64,
  kMulF32S,
  kMulF64S,
  kMulF32,
  kMulF64,
  kDivF32S,
  kDivF64S,
  kDivF32,
  kDivF64,
  kMinF32S,
  kMinF64S,
  kMinF32,
  kMinF64,
  kMaxF32S,
  kMaxF64S,
  kMaxF32,
  kMaxF64,
  kCmpEqF32S,
  kCmpEqF64S,
  kCmpEqF32,
  kCmpEqF64,
  kCmpNeF32S,
  kCmpNeF64S,
  kCmpNeF32,
  kCmpNeF64,
  kCmpGtF32S,
  kCmpGtF64S,
  kCmpGtF32,
  kCmpGtF64,
  kCmpGeF32S,
  kCmpGeF64S,
  kCmpGeF32,
  kCmpGeF64,
  kCmpLtF32S,
  kCmpLtF64S,
  kCmpLtF32,
  kCmpLtF64,
  kCmpLeF32S,
  kCmpLeF64S,
  kCmpLeF32,
  kCmpLeF64,
  kCmpOrdF32S,
  kCmpOrdF64S,
  kCmpOrdF32,
  kCmpOrdF64,
  kCmpUnordF32S,
  kCmpUnordF64S,
  kCmpUnordF32,
  kCmpUnordF64,

  kHAddF64,

  kCombineLoHiU64,
  kCombineLoHiF64,
  kCombineHiLoU64,
  kCombineHiLoF64,

  kInterleaveLoU8,
  kInterleaveHiU8,
  kInterleaveLoU16,
  kInterleaveHiU16,
  kInterleaveLoU32,
  kInterleaveHiU32,
  kInterleaveLoU64,
  kInterleaveHiU64,
  kInterleaveLoF32,
  kInterleaveHiF32,
  kInterleaveLoF64,
  kInterleaveHiF64,

  kPacksI16_I8,
  kPacksI16_U8,
  kPacksI32_I16,
  kPacksI32_U16,

  kSwizzlev_U8,

#if defined(BL_JIT_ARCH_A64)

  kMulwLoI8,
  kMulwLoU8,
  kMulwHiI8,
  kMulwHiU8,
  kMulwLoI16,
  kMulwLoU16,
  kMulwHiI16,
  kMulwHiU16,
  kMulwLoI32,
  kMulwLoU32,
  kMulwHiI32,
  kMulwHiU32,

  kMAddwLoI8,
  kMAddwLoU8,
  kMAddwHiI8,
  kMAddwHiU8,
  kMAddwLoI16,
  kMAddwLoU16,
  kMAddwHiI16,
  kMAddwHiU16,
  kMAddwLoI32,
  kMAddwLoU32,
  kMAddwHiI32,
  kMAddwHiU32,

  kMaxValue = kMAddwHiU32

#elif defined(BL_JIT_ARCH_X86)

  kPermuteU8,
  kPermuteU16,
  kPermuteU32,
  kPermuteU64,

  kMaxValue = kPermuteU64

#else

  kMaxValue = kSwizzlev_U8

#endif // BL_JIT_ARCH_A64
};

enum class OpcodeVVVI : uint32_t {
  kAlignr_U128,
  kInterleaveShuffleU32x4,
  kInterleaveShuffleU64x2,
  kInterleaveShuffleF32x4,
  kInterleaveShuffleF64x2,
  kInsertV128_U32,
  kInsertV128_F32,
  kInsertV128_U64,
  kInsertV128_F64,
  kInsertV256_U32,
  kInsertV256_F32,
  kInsertV256_U64,
  kInsertV256_F64,

  kMaxValue = kInsertV256_F64
};

enum class OpcodeVVVV : uint32_t {
  kBlendV_U8,

  kMAddU16,
  kMAddU32,

  kMAddF32S,
  kMAddF64S,
  kMAddF32,
  kMAddF64,

  kMSubF32S,
  kMSubF64S,
  kMSubF32,
  kMSubF64,

  kNMAddF32S,
  kNMAddF64S,
  kNMAddF32,
  kNMAddF64,

  kNMSubF32S,
  kNMSubF64S,
  kNMSubF32,
  kNMSubF64,

  kMaxValue = kNMSubF64
};

//! Pipeline optimization flags used by \ref PipeCompiler.
enum class PipeOptFlags : uint32_t {
  //! No flags.
  kNone = 0x0u,

  //! CPU has instructions that can perform 8-bit masked loads and stores.
  kMaskOps8Bit = 0x00000001u,

  //! CPU has instructions that can perform 16-bit masked loads and stores.
  kMaskOps16Bit = 0x00000002u,

  //! CPU has instructions that can perform 32-bit masked loads and stores.
  kMaskOps32Bit = 0x00000004u,

  //! CPU has instructions that can perform 64-bit masked loads and stores.
  kMaskOps64Bit = 0x00000008u,

  //! CPU provides low-latency 32-bit multiplication (AMD CPUs).
  kFastVpmulld = 0x00000010u,

  //! CPU provides low-latency 64-bit multiplication (AMD CPUs).
  kFastVpmullq = 0x00000020u,

  //! CPU performs hardware gathers faster than a sequence of loads and packing.
  kFastGather = 0x00000040u,

  //! CPU has fast stores with mask.
  //!
  //! \note This is a hint to the compiler to emit a masked store instead of a sequence having branches.
  kFastStoreWithMask = 0x00000080u
};
BL_DEFINE_ENUM_FLAGS(PipeOptFlags)

struct Swizzle2 {
  uint32_t value;

  BL_INLINE_NODEBUG bool operator==(const Swizzle2& other) const noexcept { return value == other.value; }
  BL_INLINE_NODEBUG bool operator!=(const Swizzle2& other) const noexcept { return value != other.value; }
};

struct Swizzle4 {
  uint32_t value;

  BL_INLINE_NODEBUG bool operator==(const Swizzle4& other) const noexcept { return value == other.value; }
  BL_INLINE_NODEBUG bool operator!=(const Swizzle4& other) const noexcept { return value != other.value; }
};

static BL_INLINE_NODEBUG constexpr Swizzle2 swizzle(uint8_t b, uint8_t a) noexcept {
  return Swizzle2{(uint32_t(b) << 8) | a};
}

static BL_INLINE_NODEBUG constexpr Swizzle4 swizzle(uint8_t d, uint8_t c, uint8_t b, uint8_t a) noexcept {
  return Swizzle4{(uint32_t(d) << 24) | (uint32_t(c) << 16) | (uint32_t(b) << 8) | a};
}

//! Condition represents either a condition or an assignment operation that can be checked.
class Condition {
public:
  //! \name Members
  //! \{

  OpcodeCond op;
  CondCode cond;
  Operand a;
  Operand b;

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG Condition(OpcodeCond op, CondCode cond, const Operand& a, const Operand& b) noexcept
    : op(op),
      cond(cond),
      a(a),
      b(b) {}

  BL_INLINE_NODEBUG Condition(const Condition& other) noexcept = default;

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG Condition& operator=(const Condition& other) noexcept = default;

  //! \}
};

static BL_INLINE Condition and_z(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignAnd, CondCode::kZero, a, b); }
static BL_INLINE Condition and_z(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignAnd, CondCode::kZero, a, b); }
static BL_INLINE Condition and_z(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignAnd, CondCode::kZero, a, b); }
static BL_INLINE Condition and_nz(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignAnd, CondCode::kNotZero, a, b); }
static BL_INLINE Condition and_nz(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignAnd, CondCode::kNotZero, a, b); }
static BL_INLINE Condition and_nz(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignAnd, CondCode::kNotZero, a, b); }

static BL_INLINE Condition or_z(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignOr, CondCode::kZero, a, b); }
static BL_INLINE Condition or_z(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignOr, CondCode::kZero, a, b); }
static BL_INLINE Condition or_z(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignOr, CondCode::kZero, a, b); }
static BL_INLINE Condition or_nz(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignOr, CondCode::kNotZero, a, b); }
static BL_INLINE Condition or_nz(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignOr, CondCode::kNotZero, a, b); }
static BL_INLINE Condition or_nz(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignOr, CondCode::kNotZero, a, b); }

static BL_INLINE Condition xor_z(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignXor, CondCode::kZero, a, b); }
static BL_INLINE Condition xor_z(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignXor, CondCode::kZero, a, b); }
static BL_INLINE Condition xor_z(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignXor, CondCode::kZero, a, b); }
static BL_INLINE Condition xor_nz(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignXor, CondCode::kNotZero, a, b); }
static BL_INLINE Condition xor_nz(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignXor, CondCode::kNotZero, a, b); }
static BL_INLINE Condition xor_nz(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignXor, CondCode::kNotZero, a, b); }

static BL_INLINE Condition add_z(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kZero, a, b); }
static BL_INLINE Condition add_z(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kZero, a, b); }
static BL_INLINE Condition add_z(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kZero, a, b); }
static BL_INLINE Condition add_nz(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kNotZero, a, b); }
static BL_INLINE Condition add_nz(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kNotZero, a, b); }
static BL_INLINE Condition add_nz(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kNotZero, a, b); }
static BL_INLINE Condition add_c(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kCarry, a, b); }
static BL_INLINE Condition add_c(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kCarry, a, b); }
static BL_INLINE Condition add_c(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kCarry, a, b); }
static BL_INLINE Condition add_nc(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kNotCarry, a, b); }
static BL_INLINE Condition add_nc(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kNotCarry, a, b); }
static BL_INLINE Condition add_nc(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kNotCarry, a, b); }
static BL_INLINE Condition add_s(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kSign, a, b); }
static BL_INLINE Condition add_s(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kSign, a, b); }
static BL_INLINE Condition add_s(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kSign, a, b); }
static BL_INLINE Condition add_ns(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kNotSign, a, b); }
static BL_INLINE Condition add_ns(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kNotSign, a, b); }
static BL_INLINE Condition add_ns(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignAdd, CondCode::kNotSign, a, b); }

static BL_INLINE Condition sub_z(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kZero, a, b); }
static BL_INLINE Condition sub_z(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kZero, a, b); }
static BL_INLINE Condition sub_z(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kZero, a, b); }
static BL_INLINE Condition sub_nz(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kNotZero, a, b); }
static BL_INLINE Condition sub_nz(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kNotZero, a, b); }
static BL_INLINE Condition sub_nz(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kNotZero, a, b); }
static BL_INLINE Condition sub_c(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kUnsignedLT, a, b); }
static BL_INLINE Condition sub_c(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kUnsignedLT, a, b); }
static BL_INLINE Condition sub_c(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kUnsignedLT, a, b); }
static BL_INLINE Condition sub_nc(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kUnsignedGE, a, b); }
static BL_INLINE Condition sub_nc(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kUnsignedGE, a, b); }
static BL_INLINE Condition sub_nc(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kUnsignedGE, a, b); }
static BL_INLINE Condition sub_s(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kSign, a, b); }
static BL_INLINE Condition sub_s(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kSign, a, b); }
static BL_INLINE Condition sub_s(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kSign, a, b); }
static BL_INLINE Condition sub_ns(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kNotSign, a, b); }
static BL_INLINE Condition sub_ns(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kNotSign, a, b); }
static BL_INLINE Condition sub_ns(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kNotSign, a, b); }

static BL_INLINE Condition sub_ugt(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kUnsignedGT, a, b); }
static BL_INLINE Condition sub_ugt(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kUnsignedGT, a, b); }
static BL_INLINE Condition sub_ugt(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignSub, CondCode::kUnsignedGT, a, b); }

static BL_INLINE Condition shr_z(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignShr, CondCode::kZero, a, b); }
static BL_INLINE Condition shr_z(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignShr, CondCode::kZero, a, b); }
static BL_INLINE Condition shr_z(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignShr, CondCode::kZero, a, b); }
static BL_INLINE Condition shr_nz(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kAssignShr, CondCode::kNotZero, a, b); }
static BL_INLINE Condition shr_nz(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kAssignShr, CondCode::kNotZero, a, b); }
static BL_INLINE Condition shr_nz(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kAssignShr, CondCode::kNotZero, a, b); }

static BL_INLINE Condition cmp_eq(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kEqual, a, b); }
static BL_INLINE Condition cmp_eq(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kEqual, a, b); }
static BL_INLINE Condition cmp_eq(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kEqual, a, b); }
static BL_INLINE Condition cmp_ne(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kNotEqual, a, b); }
static BL_INLINE Condition cmp_ne(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kNotEqual, a, b); }
static BL_INLINE Condition cmp_ne(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kNotEqual, a, b); }
static BL_INLINE Condition scmp_lt(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedLT, a, b); }
static BL_INLINE Condition scmp_lt(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedLT, a, b); }
static BL_INLINE Condition scmp_lt(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedLT, a, b); }
static BL_INLINE Condition scmp_le(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedLE, a, b); }
static BL_INLINE Condition scmp_le(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedLE, a, b); }
static BL_INLINE Condition scmp_le(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedLE, a, b); }
static BL_INLINE Condition scmp_gt(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedGT, a, b); }
static BL_INLINE Condition scmp_gt(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedGT, a, b); }
static BL_INLINE Condition scmp_gt(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedGT, a, b); }
static BL_INLINE Condition scmp_ge(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedGE, a, b); }
static BL_INLINE Condition scmp_ge(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedGE, a, b); }
static BL_INLINE Condition scmp_ge(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kSignedGE, a, b); }
static BL_INLINE Condition ucmp_lt(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedLT, a, b); }
static BL_INLINE Condition ucmp_lt(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedLT, a, b); }
static BL_INLINE Condition ucmp_lt(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedLT, a, b); }
static BL_INLINE Condition ucmp_le(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedLE, a, b); }
static BL_INLINE Condition ucmp_le(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedLE, a, b); }
static BL_INLINE Condition ucmp_le(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedLE, a, b); }
static BL_INLINE Condition ucmp_gt(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedGT, a, b); }
static BL_INLINE Condition ucmp_gt(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedGT, a, b); }
static BL_INLINE Condition ucmp_gt(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedGT, a, b); }
static BL_INLINE Condition ucmp_ge(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedGE, a, b); }
static BL_INLINE Condition ucmp_ge(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedGE, a, b); }
static BL_INLINE Condition ucmp_ge(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kUnsignedGE, a, b); }

static BL_INLINE Condition test_z(const Gp& a) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kEqual, a, Imm(0)); }
static BL_INLINE Condition test_nz(const Gp& a) noexcept { return Condition(OpcodeCond::kCompare, CondCode::kNotEqual, a, Imm(0)); }

static BL_INLINE Condition test_z(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kTest, CondCode::kZero, a, b); }
static BL_INLINE Condition test_z(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kTest, CondCode::kZero, a, b); }
static BL_INLINE Condition test_z(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kTest, CondCode::kZero, a, b); }
static BL_INLINE Condition test_nz(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kTest, CondCode::kNotZero, a, b); }
static BL_INLINE Condition test_nz(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kTest, CondCode::kNotZero, a, b); }
static BL_INLINE Condition test_nz(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kTest, CondCode::kNotZero, a, b); }

static BL_INLINE Condition bt_z(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kBitTest, CondCode::kBTZero, a, b); }
static BL_INLINE Condition bt_z(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kBitTest, CondCode::kBTZero, a, b); }
static BL_INLINE Condition bt_z(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kBitTest, CondCode::kBTZero, a, b); }
static BL_INLINE Condition bt_nz(const Gp& a, const Gp& b) noexcept { return Condition(OpcodeCond::kBitTest, CondCode::kBTNotZero, a, b); }
static BL_INLINE Condition bt_nz(const Gp& a, const Mem& b) noexcept { return Condition(OpcodeCond::kBitTest, CondCode::kBTNotZero, a, b); }
static BL_INLINE Condition bt_nz(const Gp& a, const Imm& b) noexcept { return Condition(OpcodeCond::kBitTest, CondCode::kBTNotZero, a, b); }

//! Pipeline compiler.
class PipeCompiler {
public:
  BL_NONCOPYABLE(PipeCompiler)

  enum : uint32_t { kMaxKRegConstCount = 4 };

  //! \name Constants
  //! \{

  enum class StackId : uint32_t {
    kIndex,
    kCustom,
    kMaxValue = kCustom
  };

#if defined(BL_JIT_ARCH_X86)
  enum class GPExt : uint8_t {
    kADX,
    kBMI,
    kBMI2,
    kLZCNT,
    kMOVBE,
    kPOPCNT,

    kIntrin = 31
  };

  enum class SSEExt : uint8_t {
    kSSE2 = 0,
    kSSE3,
    kSSSE3,
    kSSE4_1,
    kSSE4_2,
    kPCLMULQDQ,

    //! Just to distinguish between a baseline instruction and intrinsic at operation info level.
    kIntrin = 7
  };

  enum class AVXExt : uint8_t  {
    kAVX = 0,
    kAVX2,
    kF16C,
    kFMA,
    kGFNI,
    kVAES,
    kVPCLMULQDQ,
    kAVX_IFMA,
    kAVX_NE_CONVERT,
    kAVX_VNNI,
    kAVX_VNNI_INT8,
    kAVX_VNNI_INT16,
    kAVX512,
    kAVX512_BF16,
    kAVX512_BITALG,
    kAVX512_FP16,
    kAVX512_IFMA,
    kAVX512_VBMI,
    kAVX512_VBMI2,
    kAVX512_VNNI,
    kAVX512_VPOPCNTDQ,

    //! Just to distinguish between a baseline instruction and intrinsic at operation info level.
    kIntrin = 63
  };
#endif // BL_JIT_ARCH_X86

#if defined(BL_JIT_ARCH_A64)
  enum class GPExt : uint8_t {
    kCSSC,
    kFLAGM,
    kFLAGM2,
    kLS64,
    kLS64_V,
    kLSE,
    kLSE128,
    kLSE2,

    kIntrin = 31
  };

  enum class ASIMDExt : uint8_t {
    kASIMD,
    kBF16,
    kDOTPROD,
    kFCMA,
    kFHM,
    kFP16,
    kFP16CONV,
    kFP8,
    kFRINTTS,
    kI8MM,
    kJSCVT,
    kPMULL,
    kRDM,
    kSHA1,
    kSHA256,
    kSHA3,
    kSHA512,
    kSM3,
    kSM4,

    kIntrin = 63
  };

#endif // BL_JIT_ARCH_A64

  //! \}

  //! \name Members
  //! \{

  //! AsmJit compiler.
  AsmCompiler* cc = nullptr;

  const CommonTable& ct;

#if defined(BL_JIT_ARCH_X86)
  //! General purpose extension mask (X86 and X86_64 only).
  uint32_t _gpExtMask {};
  //! SSE extension mask (X86 and X86_64 only).
  uint32_t _sseExtMask {};
  //! AVX extension mask (X86 and X86_64 only).
  uint64_t _avxExtMask {};
#endif // BL_JIT_ARCH_X86

#if defined(BL_JIT_ARCH_A64)
  //! General purpose extension mask (AArch64).
  uint64_t _gpExtMask {};
  //! NEON extensions (AArch64).
  uint64_t _asimdExtMask {};
#endif // BL_JIT_ARCH_A64

  //! The behavior of scalar operations (mostly floating point).
  ScalarOpBehavior _scalarOpBehavior {};
  //! The behavior of floating point min/max operation.
  FMinMaxOpBehavior _fMinMaxOpBehavior {};
  //! The behavior of floating point `madd` operation.
  FMulAddOpBehavior _fMulAddOpBehavior {};

  //! Target CPU features.
  CpuFeatures _features {};
  //! Optimization flags.
  PipeOptFlags _optFlags = PipeOptFlags::kNone;

  //! Number of available vector registers.
  uint32_t _vecRegCount = 0;

  //! Empty predicate, used in cases where a predicate is required, but it's empty.
  PixelPredicate _emptyPredicate {};

  //! SIMD width.
  VecWidth _vecWidth = VecWidth::k128;
  //! SIMD multiplier, derived from `_vecWidth` (1, 2, 4).
  uint8_t _vecMultiplier = 0;
  //! SIMD register type (AsmJit).
  asmjit::RegType _vecRegType = asmjit::RegType::kNone;
  //! SIMD type id (AsmJit).
  asmjit::TypeId _vecTypeId = asmjit::TypeId::kVoid;

  //! Function node.
  asmjit::FuncNode* _funcNode = nullptr;
  //! Function initialization hook.
  asmjit::BaseNode* _funcInit = nullptr;
  //! Function end hook (to add 'unlikely' branches).
  asmjit::BaseNode* _funcEnd = nullptr;

  //! Invalid GP register.
  Gp _gpNone;
  //! Temporary stack used to transfer SIMD regs to GP/MM.
  Mem _tmpStack[size_t(StackId::kMaxValue) + 1];

  //! Offset to the first constant to the `commonTable` global.
  int32_t _commonTableOff = 0;
  //! Pointer to the `commonTable` constant pool (only used in 64-bit mode).
  Gp _commonTablePtr;

#if defined(BL_JIT_ARCH_X86)
  KReg _kReg[kMaxKRegConstCount];
  uint64_t _kImm[kMaxKRegConstCount] {};
#endif // BL_JIT_ARCH_X86

  struct VecConst {
    const void* ptr;
    uint32_t vRegId;
  };

  struct VecConstEx {
    uint8_t data[16];
    uint32_t vRegId;
  };

  asmjit::ZoneVector<VecConst> _vecConsts;
  asmjit::ZoneVector<VecConstEx> _vecConstsEx;

  //! \}

  //! \name Construction & Destruction
  //! \{

  PipeCompiler(AsmCompiler* cc, const asmjit::CpuFeatures& features, PipeOptFlags optFlags) noexcept;
  ~PipeCompiler() noexcept;

  //! \}

  //! \name Allocators
  //! \{

  BL_INLINE_NODEBUG asmjit::ZoneAllocator* zoneAllocator() noexcept { return &cc->_allocator; }

  //! \}

  //! \name CPU Architecture, Features and Optimization Options
  //! \{

  void _initExtensions(const asmjit::CpuFeatures& features) noexcept;

  BL_INLINE_NODEBUG bool is32Bit() const noexcept { return cc->is32Bit(); }
  BL_INLINE_NODEBUG bool is64Bit() const noexcept { return cc->is64Bit(); }
  BL_INLINE_NODEBUG uint32_t registerSize() const noexcept { return cc->registerSize(); }

#if defined(BL_JIT_ARCH_X86)
  BL_INLINE_NODEBUG bool hasGPExt(GPExt ext) const noexcept { return (_gpExtMask & (1u << uint32_t(ext))) != 0; }
  BL_INLINE_NODEBUG bool hasSSEExt(SSEExt ext) const noexcept { return (_sseExtMask & (1u << uint32_t(ext))) != 0; }
  BL_INLINE_NODEBUG bool hasAVXExt(AVXExt ext) const noexcept { return (_avxExtMask & (uint64_t(1) << uint32_t(ext))) != 0; }

  //! Tests whether ADX extension is available.
  BL_INLINE_NODEBUG bool hasADX() const noexcept { return hasGPExt(GPExt::kADX); }
  //! Tests whether BMI extension is available.
  BL_INLINE_NODEBUG bool hasBMI() const noexcept { return hasGPExt(GPExt::kBMI); }
  //! Tests whether BMI2 extension is available.
  BL_INLINE_NODEBUG bool hasBMI2() const noexcept { return hasGPExt(GPExt::kBMI2); }
  //! Tests whether LZCNT extension is available.
  BL_INLINE_NODEBUG bool hasLZCNT() const noexcept { return hasGPExt(GPExt::kLZCNT); }
  //! Tests whether MOVBE extension is available.
  BL_INLINE_NODEBUG bool hasMOVBE() const noexcept { return hasGPExt(GPExt::kMOVBE); }
  //! Tests whether POPCNT extension is available.
  BL_INLINE_NODEBUG bool hasPOPCNT() const noexcept { return hasGPExt(GPExt::kPOPCNT); }

  //! Tests whether SSE2 extensions are available (this should always return true).
  BL_INLINE_NODEBUG bool hasSSE2() const noexcept { return hasSSEExt(SSEExt::kSSE2); }
  //! Tests whether SSE3 extension is available.
  BL_INLINE_NODEBUG bool hasSSE3() const noexcept { return hasSSEExt(SSEExt::kSSE3); }
  //! Tests whether SSSE3 extension is available.
  BL_INLINE_NODEBUG bool hasSSSE3() const noexcept { return hasSSEExt(SSEExt::kSSSE3); }
  //! Tests whether SSE4.1 extension is available.
  BL_INLINE_NODEBUG bool hasSSE4_1() const noexcept { return hasSSEExt(SSEExt::kSSE4_1); }
  //! Tests whether SSE4.2 extension is available.
  BL_INLINE_NODEBUG bool hasSSE4_2() const noexcept { return hasSSEExt(SSEExt::kSSE4_2); }
  //! Tests whether PCLMULQDQ extension is available.
  BL_INLINE_NODEBUG bool hasPCLMULQDQ() const noexcept { return hasSSEExt(SSEExt::kPCLMULQDQ); }

  //! Tests whether AVX extension is available.
  BL_INLINE_NODEBUG bool hasAVX() const noexcept { return hasAVXExt(AVXExt::kAVX); }
  //! Tests whether AVX2 extension is available.
  BL_INLINE_NODEBUG bool hasAVX2() const noexcept { return hasAVXExt(AVXExt::kAVX2); }
  //! Tests whether F16C extension is available.
  BL_INLINE_NODEBUG bool hasF16C() const noexcept { return hasAVXExt(AVXExt::kF16C); }
  //! Tests whether FMA extension is available.
  BL_INLINE_NODEBUG bool hasFMA() const noexcept { return hasAVXExt(AVXExt::kFMA); }
  //! Tests whether GFNI extension is available.
  BL_INLINE_NODEBUG bool hasGFNI() const noexcept { return hasAVXExt(AVXExt::kGFNI); }
  //! Tests whether VPCLMULQDQ extension is available.
  BL_INLINE_NODEBUG bool hasVPCLMULQDQ() const noexcept { return hasAVXExt(AVXExt::kVPCLMULQDQ); }

  //! Tests whether AVX_IFMA extension is available.
  BL_INLINE_NODEBUG bool hasAVX_IFMA() const noexcept { return hasAVXExt(AVXExt::kAVX_IFMA); }
  //! Tests whether AVX_NE_CONVERT extension is available.
  BL_INLINE_NODEBUG bool hasAVX_NE_CONVERT() const noexcept { return hasAVXExt(AVXExt::kAVX_NE_CONVERT); }
  //! Tests whether AVX_VNNI extension is available.
  BL_INLINE_NODEBUG bool hasAVX_VNNI() const noexcept { return hasAVXExt(AVXExt::kAVX_VNNI); }
  //! Tests whether AVX_VNNI_INT8 extension is available.
  BL_INLINE_NODEBUG bool hasAVX_VNNI_INT8() const noexcept { return hasAVXExt(AVXExt::kAVX_VNNI_INT8); }
  //! Tests whether AVX_VNNI_INT16 extension is available.
  BL_INLINE_NODEBUG bool hasAVX_VNNI_INT16() const noexcept { return hasAVXExt(AVXExt::kAVX_VNNI_INT16); }

  //! Tests whether a baseline AVX-512 extensions are available (F, CD, BW, DQ, and VL).
  BL_INLINE_NODEBUG bool hasAVX512() const noexcept { return hasAVXExt(AVXExt::kAVX512); }
  //! Tests whether AVX512_BF16 extension is available.
  BL_INLINE_NODEBUG bool hasAVX512_BF16() const noexcept { return hasAVXExt(AVXExt::kAVX512_BF16); }
  //! Tests whether AVX512_BITALT extension is available.
  BL_INLINE_NODEBUG bool hasAVX512_BITALG() const noexcept { return hasAVXExt(AVXExt::kAVX512_BITALG); }
  //! Tests whether AVX512_FP16 extension is available.
  BL_INLINE_NODEBUG bool hasAVX512_FP16() const noexcept { return hasAVXExt(AVXExt::kAVX512_FP16); }
  //! Tests whether AVX512_IFMA extension is available.
  BL_INLINE_NODEBUG bool hasAVX512_IFMA() const noexcept { return hasAVXExt(AVXExt::kAVX512_IFMA); }
  //! Tests whether AVX512_VBMI extension is available.
  BL_INLINE_NODEBUG bool hasAVX512_VBMI() const noexcept { return hasAVXExt(AVXExt::kAVX512_VBMI); }
  //! Tests whether AVX512_VBMI2 extension is available.
  BL_INLINE_NODEBUG bool hasAVX512_VBMI2() const noexcept { return hasAVXExt(AVXExt::kAVX512_VBMI2); }
  //! Tests whether AVX512_VNNI extension is available.
  BL_INLINE_NODEBUG bool hasAVX512_VNNI() const noexcept { return hasAVXExt(AVXExt::kAVX512_VNNI); }
  //! Tests whether AVX512_VPOPCNTDQ extension is available.
  BL_INLINE_NODEBUG bool hasAVX512_VPOPCNTDQ() const noexcept { return hasAVXExt(AVXExt::kAVX512_VPOPCNTDQ); }

  //! Tests whether the target SIMD ISA provides instructions with non-destructive source operand (AVX+).
  BL_INLINE_NODEBUG bool hasNonDestructiveSrc() const noexcept { return hasAVX(); }

#endif // BL_JIT_ARCH_X86

#if defined(BL_JIT_ARCH_A64)
  BL_INLINE_NODEBUG bool hasGPExt(GPExt ext) const noexcept { return (_gpExtMask & (uint64_t(1) << uint32_t(ext))) != 0; }
  BL_INLINE_NODEBUG bool hasASIMDExt(ASIMDExt ext) const noexcept { return (_asimdExtMask & (uint64_t(1) << uint32_t(ext))) != 0; }

  //! Tests whether CSSC extension is available.
  BL_INLINE_NODEBUG bool hasCSSC() const noexcept { return hasGPExt(GPExt::kCSSC); }
  //! Tests whether FLAGM extension is available.
  BL_INLINE_NODEBUG bool hasFLAGM() const noexcept { return hasGPExt(GPExt::kFLAGM); }
  //! Tests whether FLAGM2 extension is available.
  BL_INLINE_NODEBUG bool hasFLAGM2() const noexcept { return hasGPExt(GPExt::kFLAGM2); }
  //! Tests whether LS64 extension is available.
  BL_INLINE_NODEBUG bool hasLS64() const noexcept { return hasGPExt(GPExt::kLS64); }
  //! Tests whether LS64_V extension is available.
  BL_INLINE_NODEBUG bool hasLS64_V() const noexcept { return hasGPExt(GPExt::kLS64_V); }
  //! Tests whether LSE extension is available.
  BL_INLINE_NODEBUG bool hasLSE() const noexcept { return hasGPExt(GPExt::kLSE); }
  //! Tests whether LSE128 extension is available.
  BL_INLINE_NODEBUG bool hasLSE128() const noexcept { return hasGPExt(GPExt::kLSE128); }
  //! Tests whether LSE2 extension is available.
  BL_INLINE_NODEBUG bool hasLSE2() const noexcept { return hasGPExt(GPExt::kLSE2); }

  //! Tests whether ASIMD extension is available (must always return true).
  BL_INLINE_NODEBUG bool hasASIMD() const noexcept { return hasASIMDExt(ASIMDExt::kASIMD); }
  //! Tests whether BF16 extension is available.
  BL_INLINE_NODEBUG bool hasBF16() const noexcept { return hasASIMDExt(ASIMDExt::kBF16); }
  //! Tests whether DOTPROD extension is available.
  BL_INLINE_NODEBUG bool hasDOTPROD() const noexcept { return hasASIMDExt(ASIMDExt::kDOTPROD); }
  //! Tests whether FCMA extension is available.
  BL_INLINE_NODEBUG bool hasFCMA() const noexcept { return hasASIMDExt(ASIMDExt::kFCMA); }
  //! Tests whether FHM extension is available.
  BL_INLINE_NODEBUG bool hasFHM() const noexcept { return hasASIMDExt(ASIMDExt::kFHM); }
  //! Tests whether FP16 extension is available.
  BL_INLINE_NODEBUG bool hasFP16() const noexcept { return hasASIMDExt(ASIMDExt::kFP16); }
  //! Tests whether FP16CONV extension is available.
  BL_INLINE_NODEBUG bool hasFP16CONV() const noexcept { return hasASIMDExt(ASIMDExt::kFP16CONV); }
  //! Tests whether FP8 extension is available.
  BL_INLINE_NODEBUG bool hasFP8() const noexcept { return hasASIMDExt(ASIMDExt::kFP8); }
  //! Tests whether FRINTTS extension is available.
  BL_INLINE_NODEBUG bool hasFRINTTS() const noexcept { return hasASIMDExt(ASIMDExt::kFRINTTS); }
  //! Tests whether I8MM extension is available.
  BL_INLINE_NODEBUG bool hasI8MM() const noexcept { return hasASIMDExt(ASIMDExt::kI8MM); }
  //! Tests whether JSCVT extension is available.
  BL_INLINE_NODEBUG bool hasJSCVT() const noexcept { return hasASIMDExt(ASIMDExt::kJSCVT); }
  //! Tests whether PMULL extension is available.
  BL_INLINE_NODEBUG bool hasPMULL() const noexcept { return hasASIMDExt(ASIMDExt::kPMULL); }
  //! Tests whether RDM extension is available.
  BL_INLINE_NODEBUG bool hasRDM() const noexcept { return hasASIMDExt(ASIMDExt::kRDM); }
  //! Tests whether SHA1 extension is available.
  BL_INLINE_NODEBUG bool hasSHA1() const noexcept { return hasASIMDExt(ASIMDExt::kSHA1); }
  //! Tests whether SHA256 extension is available.
  BL_INLINE_NODEBUG bool hasSHA256() const noexcept { return hasASIMDExt(ASIMDExt::kSHA256); }
  //! Tests whether SHA3 extension is available.
  BL_INLINE_NODEBUG bool hasSHA3() const noexcept { return hasASIMDExt(ASIMDExt::kSHA3); }
  //! Tests whether SHA512 extension is available.
  BL_INLINE_NODEBUG bool hasSHA512() const noexcept { return hasASIMDExt(ASIMDExt::kSHA512); }
  //! Tests whether SM3 extension is available.
  BL_INLINE_NODEBUG bool hasSM3() const noexcept { return hasASIMDExt(ASIMDExt::kSM3); }
  //! Tests whether SM4 extension is available.
  BL_INLINE_NODEBUG bool hasSM4() const noexcept { return hasASIMDExt(ASIMDExt::kSM4); }

  //! Tests whether the target SIMD ISA provides instructions with non-destructive destination (always on AArch64).
  BL_INLINE_NODEBUG bool hasNonDestructiveSrc() const noexcept { return true; }

#endif // BL_JIT_ARCH_A64

  //! Returns a native register signature, either 32-bit or 64-bit depending on the target architecture).
  BL_INLINE_NODEBUG OperandSignature gpSignature() const noexcept { return cc->gpSignature(); }
  //! Clones the given `reg` register into a native register (either 32-bit or 64-bit depending on the target architecture).
  BL_INLINE_NODEBUG Gp gpz(const Gp& reg) const noexcept { return cc->gpz(reg); }

  //! Returns the behavior of scalar operations (mostly floating point).
  BL_INLINE_NODEBUG ScalarOpBehavior scalarOpBehavior() const noexcept { return _scalarOpBehavior; }
  //! Returns the behavior of floating point min/max operations.
  BL_INLINE_NODEBUG FMinMaxOpBehavior fMinMaxOpBehavior() const noexcept { return _fMinMaxOpBehavior; }
  //! Returns the behavior of floating point mul+add (`madd`) operations.
  BL_INLINE_NODEBUG FMulAddOpBehavior fMulAddOpBehavior() const noexcept { return _fMulAddOpBehavior; }

  //! Tests whether a scalar operation is zeroing the rest of the destination register (AArch64).
  BL_INLINE_NODEBUG bool isScalarOpZeroing() const noexcept { return _scalarOpBehavior == ScalarOpBehavior::kZeroing; }
  //! Tests whether a scalar operation is preserving the low 128-bit part of the destination register (X86, X86_64).
  BL_INLINE_NODEBUG bool isScalarOpPreservingVec128() const noexcept { return _scalarOpBehavior == ScalarOpBehavior::kPreservingVec128; }

  //! Tests whether a floating point min/max operation selects a finite value if one of the values is NaN (AArch64).
  BL_INLINE_NODEBUG bool isFMinMaxFinite() const noexcept { return _fMinMaxOpBehavior == FMinMaxOpBehavior::kFiniteValue; }
  //! Tests whether a floating point min/max operation works as a ternary if - `if a <|> b ? a : b` (X86, X86_64).
  BL_INLINE_NODEBUG bool isFMinMaxTernary() const noexcept { return _fMinMaxOpBehavior == FMinMaxOpBehavior::kTernaryLogic; }

  //! Tests whether a floating point mul+add operation is fused (uses FMA).
  BL_INLINE_NODEBUG bool isMAddFused() const noexcept { return _fMulAddOpBehavior != FMulAddOpBehavior::kNoFMA; }
  //! Tests whether a FMA operation is available and that it can store the result to any register (true of X86).
  BL_INLINE_NODEBUG bool isFMAStoringToAnyRegister() const noexcept { return _fMulAddOpBehavior == FMulAddOpBehavior::kFMAStoreToAny; }
  //! Tests whether a FMA operation is available and that it only stores the result to accumulator register.
  BL_INLINE_NODEBUG bool isFMAStoringToAccumulator() const noexcept { return _fMulAddOpBehavior == FMulAddOpBehavior::kFMAStoreToAccumulator; }

  BL_INLINE_NODEBUG PipeOptFlags optFlags() const noexcept { return _optFlags; }
  BL_INLINE_NODEBUG bool hasOptFlag(PipeOptFlags flag) const noexcept { return blTestFlag(_optFlags, flag); }

  BL_INLINE_NODEBUG uint32_t vecRegCount() const noexcept { return _vecRegCount; }

  VecWidth maxVecWidthFromCpuFeatures() noexcept;
  void initVecWidth(VecWidth vw) noexcept;

  bool hasMaskedAccessOf(uint32_t dataSize) const noexcept;

  //! \}

  //! \name CPU SIMD Width and SIMD Width Utilities
  //! \{

  //! Returns the current SIMD width (in bytes) that this compiler and all its parts must use.
  //!
  //! \note The returned width is in bytes and it's calculated from the maximum supported widths of all pipeline parts.
  //! This means that SIMD width returned could be actually lower than a SIMD width supported by the target CPU.
  BL_INLINE_NODEBUG VecWidth vecWidth() const noexcept { return _vecWidth; }

  //! Returns whether the compiler and all parts use 256-bit SIMD.
  BL_INLINE_NODEBUG bool use256BitSimd() const noexcept { return _vecWidth >= VecWidth::k256; }
  //! Returns whether the compiler and all parts use 512-bit SIMD.
  BL_INLINE_NODEBUG bool use512BitSimd() const noexcept { return _vecWidth >= VecWidth::k512; }

  //! Returns a constant that can be used to multiply a baseline SIMD width to get the value returned by `vecWidth()`.
  //!
  //! \note A baseline SIMD width would be 16 bytes on most platforms.
  BL_INLINE_NODEBUG uint32_t vecMultiplier() const noexcept { return _vecMultiplier; }

  BL_INLINE_NODEBUG VecWidth vecWidthOf(DataWidth dataWidth, uint32_t n) const noexcept { return VecWidthUtils::vecWidthOf(vecWidth(), dataWidth, n); }
  BL_INLINE_NODEBUG uint32_t vecCountOf(DataWidth dataWidth, uint32_t n) const noexcept { return VecWidthUtils::vecCountOf(vecWidth(), dataWidth, n); }

  BL_INLINE_NODEBUG VecWidth vecWidthOf(DataWidth dataWidth, PixelCount pixelCount) const noexcept { return VecWidthUtils::vecWidthOf(vecWidth(), dataWidth, pixelCount.value()); }
  BL_INLINE_NODEBUG uint32_t vecCountOf(DataWidth dataWidth, PixelCount pixelCount) const noexcept { return VecWidthUtils::vecCountOf(vecWidth(), dataWidth, pixelCount.value()); }

  //! \}

  //! \name Function
  //! \{

  void initFunction(asmjit::FuncNode* funcNode) noexcept;

  //! \}

  //! \name Miscellaneous Helpers
  //! \{

  BL_INLINE void rename(const OpArray& opArray, const char* name) noexcept {
    for (uint32_t i = 0; i < opArray.size(); i++)
      cc->rename(opArray[i].as<asmjit::BaseReg>(), "%s%u", name, unsigned(i));
  }

  BL_INLINE void rename(const OpArray& opArray, const char* prefix, const char* name) noexcept {
    for (uint32_t i = 0; i < opArray.size(); i++)
      cc->rename(opArray[i].as<asmjit::BaseReg>(), "%s%s%u", prefix, name, unsigned(i));
  }

  //! \}

  //! \name Utilities
  //! \{

  BL_INLINE Label newLabel() const noexcept { return cc->newLabel(); }
  BL_INLINE PixelPredicate& emptyPredicate() noexcept { return _emptyPredicate; }

  BL_INLINE void align(AlignMode alignMode, uint32_t alignment) noexcept { cc->align(alignMode, alignment); }
  BL_INLINE void bind(const Label& label) noexcept { cc->bind(label); }

  //! \}

  //! \name Virtual Registers & Memory (Target Independent)
  //! \{

  BL_INLINE Gp newGp32() noexcept { return cc->newUInt32(); }
  BL_INLINE Gp newGp64() noexcept { return cc->newUInt64(); }
  BL_INLINE Gp newGpPtr() noexcept { return cc->newUIntPtr(); }

  template<typename... Args>
  BL_INLINE Gp newGp32(const char* name, Args&&... args) noexcept { return cc->newUInt32(name, BLInternal::forward<Args>(args)...); }
  template<typename... Args>
  BL_INLINE Gp newGp64(const char* name, Args&&... args) noexcept { return cc->newUInt64(name, BLInternal::forward<Args>(args)...); }
  template<typename... Args>
  BL_INLINE Gp newGpPtr(const char* name, Args&&... args) noexcept { return cc->newUIntPtr(name, BLInternal::forward<Args>(args)...); }

  template<typename RegT>
  BL_INLINE RegT newSimilarReg(const RegT& ref) noexcept { return cc->newSimilarReg(ref); }
  template<typename RegT, typename... Args>
  BL_INLINE RegT newSimilarReg(const RegT& ref, Args&&... args) noexcept { return cc->newSimilarReg(ref, BLInternal::forward<Args>(args)...); }

  template<typename... Args>
  BL_INLINE Vec newVec(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, _vecTypeId, name, BLInternal::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE Vec newVec(VecWidth vw, const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, VecWidthUtils::typeIdOf(vw), name, BLInternal::forward<Args>(args)...);
    return reg;
  }

  BL_NOINLINE void newRegArray(OpArray& dst, uint32_t n, asmjit::TypeId typeId, const char* name) noexcept {
    BL_ASSERT(n <= OpArray::kMaxSize);
    dst._size = n;
    for (uint32_t i = 0; i < n; i++) {
      cc->_newRegFmt(&dst[i].as<asmjit::BaseReg>(), typeId, "%s%u", name, i);
    }
  }

  BL_NOINLINE void newRegArray(OpArray& dst, uint32_t n, asmjit::TypeId typeId, const char* prefix, const char* name) noexcept {
    BL_ASSERT(n <= OpArray::kMaxSize);
    dst._size = n;
    for (uint32_t i = 0; i < n; i++) {
      cc->_newRegFmt(&dst[i].as<asmjit::BaseReg>(), typeId, "%s%s%u", prefix, name, i);
    }
  }

  BL_NOINLINE void newRegArray(OpArray& dst, uint32_t n, const asmjit::BaseReg& ref, const char* name) noexcept {
    BL_ASSERT(n <= OpArray::kMaxSize);
    dst._size = n;
    for (uint32_t i = 0; i < n; i++) {
      cc->_newRegFmt(&dst[i].as<asmjit::BaseReg>(), ref, "%s%u", name, i);
    }
  }

  BL_NOINLINE void newRegArray(OpArray& dst, uint32_t n, const asmjit::BaseReg& ref, const char* prefix, const char* name) noexcept {
    BL_ASSERT(n <= OpArray::kMaxSize);
    dst._size = n;
    for (uint32_t i = 0; i < n; i++) {
      cc->_newRegFmt(&dst[i].as<asmjit::BaseReg>(), ref, "%s%s%u", prefix, name, i);
    }
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, VecWidth vw, const char* name) noexcept {
    newRegArray(dst, n, VecWidthUtils::typeIdOf(vw), name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, VecWidth vw, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, VecWidthUtils::typeIdOf(vw), prefix, name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, const Vec& ref, const char* name) noexcept {
    newRegArray(dst, n, ref, name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, const Vec& ref, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, ref, prefix, name);
  }

  Mem tmpStack(StackId id, uint32_t size) noexcept;

  //! \}

  //! \name Compiler Utilities
  //! \{

  void embedJumpTable(const Label* jumpTable, size_t jumpTableSize, const Label& jumpTableBase, uint32_t entrySize) noexcept;

  //! \}

  void _initCommonTablePtr() noexcept;

  //! \name Virtual Registers
  //! \{

#if defined(BL_JIT_ARCH_X86)

  BL_INLINE Vec newV128() noexcept {
    Vec reg;
    cc->_newReg(&reg, asmjit::TypeId::kInt32x4);
    return reg;
  }

  BL_INLINE Vec newV32_F32() noexcept {
    Vec reg;
    cc->_newReg(&reg, asmjit::TypeId::kFloat32x1);
    return reg;
  }

  BL_INLINE Vec newV64_F64() noexcept {
    Vec reg;
    cc->_newReg(&reg, asmjit::TypeId::kFloat64x1);
    return reg;
  }

  BL_INLINE Vec newV128_F32() noexcept {
    Vec reg;
    cc->_newReg(&reg, asmjit::TypeId::kFloat32x4);
    return reg;
  }

  BL_INLINE Vec newV128_F64() noexcept {
    Vec reg;
    cc->_newReg(&reg, asmjit::TypeId::kFloat64x2);
    return reg;
  }

  template<typename... Args>
  BL_INLINE Vec newV128(Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kInt32x4, BLInternal::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE Vec newV32_F32(Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kFloat32x1, BLInternal::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE Vec newV64_F64(Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kFloat64x1, BLInternal::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE Vec newV128_F32(Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kFloat32x4, BLInternal::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE Vec newV128_F64(Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kFloat64x2, BLInternal::forward<Args>(args)...);
    return reg;
  }

  BL_INLINE void newV128Array(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x4, name);
  }

  BL_INLINE void newV128Array(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x4, prefix, name);
  }

  template<typename... Args>
  BL_INLINE Vec newV256(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kInt32x8, name, BLInternal::forward<Args>(args)...);
    return reg;
  }

  BL_INLINE void newV256Array(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x8, name);
  }

  BL_INLINE void newV256Array(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x8, prefix, name);
  }

  template<typename... Args>
  BL_INLINE Vec newV512(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kInt32x16, name, BLInternal::forward<Args>(args)...);
    return reg;
  }

  BL_INLINE void newV512Array(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x16, name);
  }

  BL_INLINE void newV512Array(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x16, prefix, name);
  }

#endif // BL_JIT_ARCH_X86

#if defined(BL_JIT_ARCH_A64)

  template<typename... Args>
  BL_INLINE Vec newV128(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kInt32x4, name, BLInternal::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE Vec newV32_F32(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kFloat32x1, name, BLInternal::forward<Args>(args)...);
    return reg.v128();
  }

  template<typename... Args>
  BL_INLINE Vec newV64_F64(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kFloat64x1, name, BLInternal::forward<Args>(args)...);
    return reg.v128();
  }

  template<typename... Args>
  BL_INLINE Vec newV128_F32(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kFloat32x4, name, BLInternal::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE Vec newV128_F64(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kFloat64x2, name, BLInternal::forward<Args>(args)...);
    return reg;
  }

  BL_INLINE void newV128Array(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x4, name);
  }

  BL_INLINE void newV128Array(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x4, prefix, name);
  }

#endif

  //! \}

  //! \name Constants (X86)
  //! \{

#if defined(BL_JIT_ARCH_X86)
  KReg kConst(uint64_t value) noexcept;
#endif // BL_JIT_ARCH_X86

  Operand simdConst(const void* c, Bcst bcstWidth, VecWidth constWidth) noexcept;
  Operand simdConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept;
  Operand simdConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept;

  Vec simdVecConst(const void* c, Bcst bcstWidth, VecWidth constWidth) noexcept;
  Vec simdVecConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept;
  Vec simdVecConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept;

  Mem simdMemConst(const void* c, Bcst bcstWidth, VecWidth constWidth) noexcept;
  Mem simdMemConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept;
  Mem simdMemConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept;

  Mem _getMemConst(const void* c) noexcept;
  Vec _newVecConst(const void* c, bool isUniqueConst) noexcept;

#if defined(BL_JIT_ARCH_A64)
  Vec simdConst16B(const void* data16) noexcept;
#endif // BL_JIT_ARCH_A64

#if defined(BL_JIT_ARCH_A64)
  inline Vec simdVecZero(const Vec& similarTo) noexcept { return simdVecConst(&ct.i_0000000000000000, Bcst::k32, similarTo); }
#endif // BL_JIT_ARCH_A64

  //! \}

  //! \name Emit - General Purpose Instructions
  //! \{

  void emit_mov(const Gp& dst, const Operand_& src) noexcept;
  void emit_m(OpcodeM op, const Mem& m) noexcept;
  void emit_rm(OpcodeRM op, const Gp& dst, const Mem& src) noexcept;
  void emit_mr(OpcodeMR op, const Mem& dst, const Gp& src) noexcept;
  void emit_cmov(const Gp& dst, const Operand_& sel, const Condition& condition) noexcept;
  void emit_select(const Gp& dst, const Operand_& sel1_, const Operand_& sel2_, const Condition& condition) noexcept;
  void emit_2i(OpcodeRR op, const Gp& dst, const Operand_& src_) noexcept;
  void emit_3i(OpcodeRRR op, const Gp& dst, const Operand_& src1_, const Operand_& src2_) noexcept;
  void emit_j(const Operand_& target) noexcept;
  void emit_j_if(const Label& target, const Condition& condition) noexcept;

  BL_INLINE void mov(const Gp& dst, const Gp& src) noexcept { return emit_mov(dst, src); }
  BL_INLINE void mov(const Gp& dst, const Imm& src) noexcept { return emit_mov(dst, src); }

  BL_INLINE void load(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadReg, dst, src); }
  BL_INLINE void load_i8(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadI8, dst, src); }
  BL_INLINE void load_u8(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadU8, dst, src); }
  BL_INLINE void load_i16(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadI16, dst, src); }
  BL_INLINE void load_u16(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadU16, dst, src); }
  BL_INLINE void load_i32(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadI32, dst, src); }
  BL_INLINE void load_u32(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadU32, dst, src); }
  BL_INLINE void load_i64(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadI64, dst, src); }
  BL_INLINE void load_u64(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadU64, dst, src); }

  BL_INLINE void load_merge_u8(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadMergeU8, dst, src); }
  BL_INLINE void load_shift_u8(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadShiftU8, dst, src); }
  BL_INLINE void load_merge_u16(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadMergeU16, dst, src); }
  BL_INLINE void load_shift_u16(const Gp& dst, const Mem& src) noexcept { return emit_rm(OpcodeRM::kLoadShiftU16, dst, src); }

  BL_INLINE void store(const Mem& dst, const Gp& src) noexcept { return emit_mr(OpcodeMR::kStoreReg, dst, src); }
  BL_INLINE void store_u8(const Mem& dst, const Gp& src) noexcept { return emit_mr(OpcodeMR::kStoreU8, dst, src); }
  BL_INLINE void store_u16(const Mem& dst, const Gp& src) noexcept { return emit_mr(OpcodeMR::kStoreU16, dst, src); }
  BL_INLINE void store_u32(const Mem& dst, const Gp& src) noexcept { return emit_mr(OpcodeMR::kStoreU32, dst, src); }
  BL_INLINE void store_u64(const Mem& dst, const Gp& src) noexcept { return emit_mr(OpcodeMR::kStoreU64, dst, src); }

  BL_INLINE void store_zero_reg(const Mem& dst) noexcept { return emit_m(OpcodeM::kStoreZeroReg, dst); }
  BL_INLINE void store_zero_u8(const Mem& dst) noexcept { return emit_m(OpcodeM::kStoreZeroU8, dst); }
  BL_INLINE void store_zero_u16(const Mem& dst) noexcept { return emit_m(OpcodeM::kStoreZeroU16, dst); }
  BL_INLINE void store_zero_u32(const Mem& dst) noexcept { return emit_m(OpcodeM::kStoreZeroU32, dst); }
  BL_INLINE void store_zero_u64(const Mem& dst) noexcept { return emit_m(OpcodeM::kStoreZeroU64, dst); }

  BL_INLINE void mem_add(const Mem& dst, const Gp& src) noexcept { return emit_mr(OpcodeMR::kAddReg, dst, src); }
  BL_INLINE void mem_add_u8(const Mem& dst, const Gp& src) noexcept { return emit_mr(OpcodeMR::kAddU8, dst, src); }
  BL_INLINE void mem_add_u16(const Mem& dst, const Gp& src) noexcept { return emit_mr(OpcodeMR::kAddU16, dst, src); }
  BL_INLINE void mem_add_u32(const Mem& dst, const Gp& src) noexcept { return emit_mr(OpcodeMR::kAddU32, dst, src); }
  BL_INLINE void mem_add_u64(const Mem& dst, const Gp& src) noexcept { return emit_mr(OpcodeMR::kAddU64, dst, src); }

  BL_INLINE void cmov(const Gp& dst, const Gp& sel, const Condition& condition) noexcept { emit_cmov(dst, sel, condition); }
  BL_INLINE void cmov(const Gp& dst, const Mem& sel, const Condition& condition) noexcept { emit_cmov(dst, sel, condition); }

  template<typename Sel1, typename Sel2>
  BL_INLINE void select(const Gp& dst, const Sel1& sel1, const Sel2& sel2, const Condition& condition) noexcept { emit_select(dst, sel1, sel2, condition); }

  BL_INLINE void abs(const Gp& dst, const Gp& src) noexcept { emit_2i(OpcodeRR::kAbs, dst, src); }
  BL_INLINE void abs(const Gp& dst, const Mem& src) noexcept { emit_2i(OpcodeRR::kAbs, dst, src); }

  BL_INLINE void neg(const Gp& dst, const Gp& src) noexcept { emit_2i(OpcodeRR::kNeg, dst, src); }
  BL_INLINE void neg(const Gp& dst, const Mem& src) noexcept { emit_2i(OpcodeRR::kNeg, dst, src); }

  BL_INLINE void not_(const Gp& dst, const Gp& src) noexcept { emit_2i(OpcodeRR::kNot, dst, src); }
  BL_INLINE void not_(const Gp& dst, const Mem& src) noexcept { emit_2i(OpcodeRR::kNot, dst, src); }

  BL_INLINE void bswap(const Gp& dst, const Gp& src) noexcept { emit_2i(OpcodeRR::kBSwap, dst, src); }
  BL_INLINE void bswap(const Gp& dst, const Mem& src) noexcept { emit_2i(OpcodeRR::kBSwap, dst, src); }

  BL_INLINE void clz(const Gp& dst, const Gp& src) noexcept { emit_2i(OpcodeRR::kCLZ, dst, src); }
  BL_INLINE void clz(const Gp& dst, const Mem& src) noexcept { emit_2i(OpcodeRR::kCLZ, dst, src); }

  BL_INLINE void ctz(const Gp& dst, const Gp& src) noexcept { emit_2i(OpcodeRR::kCTZ, dst, src); }
  BL_INLINE void ctz(const Gp& dst, const Mem& src) noexcept { emit_2i(OpcodeRR::kCTZ, dst, src); }

  BL_INLINE void reflect(const Gp& dst, const Gp& src) noexcept { emit_2i(OpcodeRR::kReflect, dst, src); }
  BL_INLINE void reflect(const Gp& dst, const Mem& src) noexcept { emit_2i(OpcodeRR::kReflect, dst, src); }

  BL_INLINE void inc(const Gp& dst) noexcept { emit_3i(OpcodeRRR::kAdd, dst, dst, Imm(1)); }
  BL_INLINE void dec(const Gp& dst) noexcept { emit_3i(OpcodeRRR::kSub, dst, dst, Imm(1)); }

  BL_INLINE void and_(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kAnd, dst, src1, src2); }
  BL_INLINE void and_(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kAnd, dst, src1, src2); }
  BL_INLINE void and_(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kAnd, dst, src1, src2); }
  BL_INLINE void and_(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kAnd, dst, src1, src2); }
  BL_INLINE void and_(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kAnd, dst, src1, src2); }

  BL_INLINE void or_(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kOr, dst, src1, src2); }
  BL_INLINE void or_(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kOr, dst, src1, src2); }
  BL_INLINE void or_(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kOr, dst, src1, src2); }
  BL_INLINE void or_(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kOr, dst, src1, src2); }
  BL_INLINE void or_(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kOr, dst, src1, src2); }

  BL_INLINE void xor_(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kXor, dst, src1, src2); }
  BL_INLINE void xor_(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kXor, dst, src1, src2); }
  BL_INLINE void xor_(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kXor, dst, src1, src2); }
  BL_INLINE void xor_(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kXor, dst, src1, src2); }
  BL_INLINE void xor_(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kXor, dst, src1, src2); }

  BL_INLINE void bic(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kBic, dst, src1, src2); }
  BL_INLINE void bic(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kBic, dst, src1, src2); }
  BL_INLINE void bic(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kBic, dst, src1, src2); }
  BL_INLINE void bic(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kBic, dst, src1, src2); }
  BL_INLINE void bic(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kBic, dst, src1, src2); }

  BL_INLINE void add(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kAdd, dst, src1, src2); }
  BL_INLINE void add(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kAdd, dst, src1, src2); }
  BL_INLINE void add(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kAdd, dst, src1, src2); }
  BL_INLINE void add(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kAdd, dst, src1, src2); }
  BL_INLINE void add(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kAdd, dst, src1, src2); }

  BL_INLINE void sub(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSub, dst, src1, src2); }
  BL_INLINE void sub(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kSub, dst, src1, src2); }
  BL_INLINE void sub(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSub, dst, src1, src2); }
  BL_INLINE void sub(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSub, dst, src1, src2); }
  BL_INLINE void sub(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSub, dst, src1, src2); }

  BL_INLINE void mul(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kMul, dst, src1, src2); }
  BL_INLINE void mul(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kMul, dst, src1, src2); }
  BL_INLINE void mul(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kMul, dst, src1, src2); }
  BL_INLINE void mul(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kMul, dst, src1, src2); }
  BL_INLINE void mul(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kMul, dst, src1, src2); }

  BL_INLINE void udiv(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kUDiv, dst, src1, src2); }
  BL_INLINE void udiv(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kUDiv, dst, src1, src2); }
  BL_INLINE void udiv(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kUDiv, dst, src1, src2); }
  BL_INLINE void udiv(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kUDiv, dst, src1, src2); }
  BL_INLINE void udiv(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kUDiv, dst, src1, src2); }

  BL_INLINE void umod(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kUMod, dst, src1, src2); }
  BL_INLINE void umod(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kUMod, dst, src1, src2); }
  BL_INLINE void umod(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kUMod, dst, src1, src2); }
  BL_INLINE void umod(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kUMod, dst, src1, src2); }
  BL_INLINE void umod(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kUMod, dst, src1, src2); }

  BL_INLINE void smin(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSMin, dst, src1, src2); }
  BL_INLINE void smin(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kSMin, dst, src1, src2); }
  BL_INLINE void smin(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSMin, dst, src1, src2); }
  BL_INLINE void smin(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSMin, dst, src1, src2); }
  BL_INLINE void smin(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSMin, dst, src1, src2); }

  BL_INLINE void smax(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSMax, dst, src1, src2); }
  BL_INLINE void smax(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kSMax, dst, src1, src2); }
  BL_INLINE void smax(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSMax, dst, src1, src2); }
  BL_INLINE void smax(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSMax, dst, src1, src2); }
  BL_INLINE void smax(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSMax, dst, src1, src2); }

  BL_INLINE void umin(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kUMin, dst, src1, src2); }
  BL_INLINE void umin(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kUMin, dst, src1, src2); }
  BL_INLINE void umin(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kUMin, dst, src1, src2); }
  BL_INLINE void umin(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kUMin, dst, src1, src2); }
  BL_INLINE void umin(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kUMin, dst, src1, src2); }

  BL_INLINE void umax(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kUMax, dst, src1, src2); }
  BL_INLINE void umax(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kUMax, dst, src1, src2); }
  BL_INLINE void umax(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kUMax, dst, src1, src2); }
  BL_INLINE void umax(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kUMax, dst, src1, src2); }
  BL_INLINE void umax(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kUMax, dst, src1, src2); }

  BL_INLINE void shl(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSll, dst, src1, src2); }
  BL_INLINE void shl(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSll, dst, src1, src2); }
  BL_INLINE void shl(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSll, dst, src1, src2); }
  BL_INLINE void shl(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSll, dst, src1, src2); }

  BL_INLINE void shr(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSrl, dst, src1, src2); }
  BL_INLINE void shr(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSrl, dst, src1, src2); }
  BL_INLINE void shr(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSrl, dst, src1, src2); }
  BL_INLINE void shr(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSrl, dst, src1, src2); }

  BL_INLINE void sar(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSra, dst, src1, src2); }
  BL_INLINE void sar(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSra, dst, src1, src2); }
  BL_INLINE void sar(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSra, dst, src1, src2); }
  BL_INLINE void sar(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kSra, dst, src1, src2); }

  BL_INLINE void rol(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kRol, dst, src1, src2); }
  BL_INLINE void rol(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kRol, dst, src1, src2); }
  BL_INLINE void rol(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kRol, dst, src1, src2); }
  BL_INLINE void rol(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kRol, dst, src1, src2); }

  BL_INLINE void ror(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kRor, dst, src1, src2); }
  BL_INLINE void ror(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kRor, dst, src1, src2); }
  BL_INLINE void ror(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kRor, dst, src1, src2); }
  BL_INLINE void ror(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_3i(OpcodeRRR::kRor, dst, src1, src2); }

  BL_INLINE void sbound(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_3i(OpcodeRRR::kSBound, dst, src1, src2); }
  BL_INLINE void sbound(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_3i(OpcodeRRR::kSBound, dst, src1, src2); }

  BL_INLINE void j(const Gp& target) noexcept { emit_j(target); }
  BL_INLINE void j(const Label& target) noexcept { emit_j(target); }
  BL_INLINE void j(const Label& target, const Condition& condition) noexcept { emit_j_if(target, condition); }

  void adds_u8(const Gp& dst, const Gp& src1, const Gp& src2) noexcept;

  void inv_u8(const Gp& dst, const Gp& src) noexcept;
  void div_255_u32(const Gp& dst, const Gp& src) noexcept;
  void mul_257_hu16(const Gp& dst, const Gp& src) noexcept;

  void add_scaled(const Gp& dst, const Gp& a, int b) noexcept;
  void add_ext(const Gp& dst, const Gp& src_, const Gp& idx_, uint32_t scale, int32_t disp = 0) noexcept;

#if 1
  inline void i_prefetch(const Mem& mem) noexcept { blUnused(mem); }
#else
  inline void i_prefetch(const Mem& mem) noexcept { cc->prefetcht0(mem); }
#endif

  void lea(const Gp& dst, const Mem& src) noexcept;

  //! \}

  //! \name Emit - Vector Instructions
  //! \{

  void emit_2v(OpcodeVV op, const Operand_& dst_, const Operand_& src_) noexcept;
  void emit_2v(OpcodeVV op, const OpArray& dst_, const Operand_& src_) noexcept;
  void emit_2v(OpcodeVV op, const OpArray& dst_, const OpArray& src_) noexcept;

  void emit_2vi(OpcodeVVI op, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void emit_2vi(OpcodeVVI op, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void emit_2vi(OpcodeVVI op, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept;

  void emit_2vs(OpcodeVR op, const Operand_& dst_, const Operand_& src_, uint32_t idx = 0) noexcept;

  void emit_vm(OpcodeVM op, const Vec& dst_, const Mem& src_, uint32_t alignment, uint32_t idx = 0) noexcept;
  void emit_vm(OpcodeVM op, const OpArray& dst_, const Mem& src_, uint32_t alignment, uint32_t idx = 0) noexcept;

  void emit_mv(OpcodeMV op, const Mem& dst_, const Vec& src_, uint32_t alignment, uint32_t idx = 0) noexcept;
  void emit_mv(OpcodeMV op, const Mem& dst_, const OpArray& src_, uint32_t alignment, uint32_t idx = 0) noexcept;

  void emit_3v(OpcodeVVV op, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_) noexcept;
  void emit_3v(OpcodeVVV op, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_) noexcept;
  void emit_3v(OpcodeVVV op, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_) noexcept;
  void emit_3v(OpcodeVVV op, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_) noexcept;

  void emit_3vi(OpcodeVVVI op, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, uint32_t imm) noexcept;
  void emit_3vi(OpcodeVVVI op, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_, uint32_t imm) noexcept;
  void emit_3vi(OpcodeVVVI op, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_, uint32_t imm) noexcept;
  void emit_3vi(OpcodeVVVI op, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, uint32_t imm) noexcept;

  void emit_4v(OpcodeVVVV op, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, const Operand_& src3_) noexcept;
  void emit_4v(OpcodeVVVV op, const OpArray& dst_, const Operand_& src1_, const Operand_& src2_, const OpArray& src3_) noexcept;
  void emit_4v(OpcodeVVVV op, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_, const Operand& src3_) noexcept;
  void emit_4v(OpcodeVVVV op, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_, const OpArray& src3_) noexcept;
  void emit_4v(OpcodeVVVV op, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_, const Operand& src3_) noexcept;
  void emit_4v(OpcodeVVVV op, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_, const OpArray& src3_) noexcept;
  void emit_4v(OpcodeVVVV op, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, const Operand& src3_) noexcept;
  void emit_4v(OpcodeVVVV op, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, const OpArray& src3_) noexcept;

  #define DEFINE_OP_2V(name, op) \
    template<typename Dst, typename Src> \
    BL_INLINE void name(const Dst& dst, const Src& src) noexcept { emit_2v(op, dst, src); }

  #define DEFINE_OP_2VI(name, op) \
    template<typename Dst, typename Src> \
    BL_INLINE void name(const Dst& dst, const Src& src, uint32_t imm) noexcept { emit_2vi(op, dst, src, imm); }

  #define DEFINE_OP_2VI_WRAP(name, imm_wrapper, op) \
    template<typename Dst, typename Src> \
    BL_INLINE void name(const Dst& dst, const Src& src, const imm_wrapper& imm) noexcept { emit_2vi(op, dst, src, imm.value); }

  #define DEFINE_OP_VM_U(name, op, alignment) \
    BL_INLINE void name(const Vec& dst, const Mem& src) noexcept { emit_vm(op, dst, src, alignment); } \
    BL_INLINE void name(const VecArray& dst, const Mem& src) noexcept { emit_vm(op, dst, src, alignment); }

  #define DEFINE_OP_VM_A(name, op, default_alignment) \
    BL_INLINE void name(const Vec& dst, const Mem& src, Alignment alignment = Alignment{default_alignment}) noexcept { emit_vm(op, dst, src, alignment._v); } \
    BL_INLINE void name(const VecArray& dst, const Mem& src, Alignment alignment = Alignment{default_alignment}) noexcept { emit_vm(op, dst, src, alignment._v); }

  #define DEFINE_OP_VM_I(name, op, default_alignment) \
    BL_INLINE void name(const Vec& dst, const Mem& src, uint32_t idx) noexcept { emit_vm(op, dst, src, default_alignment, idx); } \
    BL_INLINE void name(const VecArray& dst, const Mem& src, uint32_t idx) noexcept { emit_vm(op, dst, src, default_alignment, idx); }

  #define DEFINE_OP_MV_U(name, op, alignment) \
    BL_INLINE void name(const Mem& dst, const Vec& src) noexcept { emit_mv(op, dst, src, alignment); } \
    BL_INLINE void name(const Mem& dst, const VecArray& src) noexcept { emit_mv(op, dst, src, alignment); }

  #define DEFINE_OP_MV_A(name, op, default_alignment) \
    BL_INLINE void name(const Mem& dst, const Vec& src, Alignment alignment = Alignment{default_alignment}) noexcept { emit_mv(op, dst, src, alignment._v); } \
    BL_INLINE void name(const Mem& dst, const VecArray& src, Alignment alignment = Alignment{default_alignment}) noexcept { emit_mv(op, dst, src, alignment._v); }

  #define DEFINE_OP_MV_I(name, op, default_alignment) \
    BL_INLINE void name(const Mem& dst, const Vec& src, uint32_t idx) noexcept { emit_mv(op, dst, src, default_alignment, idx); } \
    BL_INLINE void name(const Mem& dst, const VecArray& src, uint32_t idx) noexcept { emit_mv(op, dst, src, default_alignment, idx); }

  #define DEFINE_OP_3V(name, op) \
    template<typename Dst, typename Src1, typename Src2> \
    BL_INLINE void name(const Dst& dst, const Src1& src1, const Src2& src2) noexcept { emit_3v(op, dst, src1, src2); }

  #define DEFINE_OP_3VI(name, op) \
    template<typename Dst, typename Src1, typename Src2> \
    BL_INLINE void name(const Dst& dst, const Src1& src1, const Src2& src2, uint32_t imm) noexcept { emit_3vi(op, dst, src1, src2, imm); }

  #define DEFINE_OP_3VI_WRAP(name, imm_wrapper, op) \
    template<typename Dst, typename Src1, typename Src2> \
    BL_INLINE void name(const Dst& dst, const Src1& src1, const Src2& src2, const imm_wrapper& imm) noexcept { emit_3vi(op, dst, src1, src2, imm.value); }

  #define DEFINE_OP_4V(name, op) \
    template<typename Dst, typename Src1, typename Src2, typename Src3> \
    BL_INLINE void name(const Dst& dst, const Src1& src1, const Src2& src2, const Src3& src3) noexcept { emit_4v(op, dst, src1, src2, src3); }

  BL_INLINE void s_mov(const Gp& dst, const Vec& src) noexcept { emit_2vs(OpcodeVR::kMov, dst, src); }
  BL_INLINE void s_mov(const Vec& dst, const Gp& src) noexcept { emit_2vs(OpcodeVR::kMov, dst, src); }

  BL_INLINE void s_mov_u32(const Gp& dst, const Vec& src) noexcept { emit_2vs(OpcodeVR::kMovU32, dst, src); }
  BL_INLINE void s_mov_u32(const Vec& dst, const Gp& src) noexcept { emit_2vs(OpcodeVR::kMovU32, dst, src); }

  BL_INLINE void s_mov_u64(const Gp& dst, const Vec& src) noexcept { emit_2vs(OpcodeVR::kMovU64, dst, src); }
  BL_INLINE void s_mov_u64(const Vec& dst, const Gp& src) noexcept { emit_2vs(OpcodeVR::kMovU64, dst, src); }

  BL_INLINE void s_insert_u8(const Vec& dst, const Gp& src, uint32_t idx) noexcept { emit_2vs(OpcodeVR::kInsertU8, dst, src, idx); }
  BL_INLINE void s_insert_u16(const Vec& dst, const Gp& src, uint32_t idx) noexcept { emit_2vs(OpcodeVR::kInsertU16, dst, src, idx); }
  BL_INLINE void s_insert_u32(const Vec& dst, const Gp& src, uint32_t idx) noexcept { emit_2vs(OpcodeVR::kInsertU32, dst, src, idx); }
  BL_INLINE void s_insert_u64(const Vec& dst, const Gp& src, uint32_t idx) noexcept { emit_2vs(OpcodeVR::kInsertU64, dst, src, idx); }

  BL_INLINE void s_extract_u8(const Gp& dst, const Vec& src, uint32_t idx) noexcept { emit_2vs(OpcodeVR::kExtractU8, dst, src, idx); }
  BL_INLINE void s_extract_u16(const Gp& dst, const Vec& src, uint32_t idx) noexcept { emit_2vs(OpcodeVR::kExtractU16, dst, src, idx); }
  BL_INLINE void s_extract_u32(const Gp& dst, const Vec& src, uint32_t idx) noexcept { emit_2vs(OpcodeVR::kExtractU32, dst, src, idx); }
  BL_INLINE void s_extract_u64(const Gp& dst, const Vec& src, uint32_t idx) noexcept { emit_2vs(OpcodeVR::kExtractU64, dst, src, idx); }

  BL_INLINE void s_cvt_int_to_f32(const Vec& dst, const Gp& src) noexcept { emit_2vs(OpcodeVR::kCvtIntToF32, dst, src); }
  BL_INLINE void s_cvt_int_to_f32(const Vec& dst, const Mem& src) noexcept { emit_2vs(OpcodeVR::kCvtIntToF32, dst, src); }
  BL_INLINE void s_cvt_int_to_f64(const Vec& dst, const Gp& src) noexcept { emit_2vs(OpcodeVR::kCvtIntToF64, dst, src); }
  BL_INLINE void s_cvt_int_to_f64(const Vec& dst, const Mem& src) noexcept { emit_2vs(OpcodeVR::kCvtIntToF64, dst, src); }

  BL_INLINE void s_cvt_trunc_f32_to_int(const Gp& dst, const Vec& src) noexcept { emit_2vs(OpcodeVR::kCvtTruncF32ToInt, dst, src); }
  BL_INLINE void s_cvt_trunc_f32_to_int(const Gp& dst, const Mem& src) noexcept { emit_2vs(OpcodeVR::kCvtTruncF32ToInt, dst, src); }
  BL_INLINE void s_cvt_round_f32_to_int(const Gp& dst, const Vec& src) noexcept { emit_2vs(OpcodeVR::kCvtRoundF32ToInt, dst, src); }
  BL_INLINE void s_cvt_round_f32_to_int(const Gp& dst, const Mem& src) noexcept { emit_2vs(OpcodeVR::kCvtRoundF32ToInt, dst, src); }
  BL_INLINE void s_cvt_trunc_f64_to_int(const Gp& dst, const Vec& src) noexcept { emit_2vs(OpcodeVR::kCvtTruncF64ToInt, dst, src); }
  BL_INLINE void s_cvt_trunc_f64_to_int(const Gp& dst, const Mem& src) noexcept { emit_2vs(OpcodeVR::kCvtTruncF64ToInt, dst, src); }
  BL_INLINE void s_cvt_round_f64_to_int(const Gp& dst, const Vec& src) noexcept { emit_2vs(OpcodeVR::kCvtRoundF64ToInt, dst, src); }
  BL_INLINE void s_cvt_round_f64_to_int(const Gp& dst, const Mem& src) noexcept { emit_2vs(OpcodeVR::kCvtRoundF64ToInt, dst, src); }

  DEFINE_OP_2V(v_mov, OpcodeVV::kMov)
  DEFINE_OP_2V(v_mov_u64, OpcodeVV::kMovU64)
  DEFINE_OP_2V(v_broadcast_u8z, OpcodeVV::kBroadcastU8Z)
  DEFINE_OP_2V(v_broadcast_u16z, OpcodeVV::kBroadcastU16Z)
  DEFINE_OP_2V(v_broadcast_u8, OpcodeVV::kBroadcastU8)
  DEFINE_OP_2V(v_broadcast_u16, OpcodeVV::kBroadcastU16)
  DEFINE_OP_2V(v_broadcast_u32, OpcodeVV::kBroadcastU32)
  DEFINE_OP_2V(v_broadcast_u64, OpcodeVV::kBroadcastU64)
  DEFINE_OP_2V(v_broadcast_f32, OpcodeVV::kBroadcastF32)
  DEFINE_OP_2V(v_broadcast_f64, OpcodeVV::kBroadcastF64)
  DEFINE_OP_2V(v_broadcast_v128_u32, OpcodeVV::kBroadcastV128_U32)
  DEFINE_OP_2V(v_broadcast_v128_u64, OpcodeVV::kBroadcastV128_U64)
  DEFINE_OP_2V(v_broadcast_v128_f32, OpcodeVV::kBroadcastV128_F32)
  DEFINE_OP_2V(v_broadcast_v128_f64, OpcodeVV::kBroadcastV128_F64)
  DEFINE_OP_2V(v_broadcast_v256_u32, OpcodeVV::kBroadcastV256_U32)
  DEFINE_OP_2V(v_broadcast_v256_u64, OpcodeVV::kBroadcastV256_U64)
  DEFINE_OP_2V(v_broadcast_v256_f32, OpcodeVV::kBroadcastV256_F32)
  DEFINE_OP_2V(v_broadcast_v256_f64, OpcodeVV::kBroadcastV256_F64)
  DEFINE_OP_2V(v_abs_i8, OpcodeVV::kAbsI8)
  DEFINE_OP_2V(v_abs_i16, OpcodeVV::kAbsI16)
  DEFINE_OP_2V(v_abs_i32, OpcodeVV::kAbsI32)
  DEFINE_OP_2V(v_abs_i64, OpcodeVV::kAbsI64)
  DEFINE_OP_2V(v_not_u32, OpcodeVV::kNotU32)
  DEFINE_OP_2V(v_not_u64, OpcodeVV::kNotU64)
  DEFINE_OP_2V(v_cvt_i8_lo_to_i16, OpcodeVV::kCvtI8LoToI16)
  DEFINE_OP_2V(v_cvt_i8_hi_to_i16, OpcodeVV::kCvtI8HiToI16)
  DEFINE_OP_2V(v_cvt_u8_lo_to_u16, OpcodeVV::kCvtU8LoToU16)
  DEFINE_OP_2V(v_cvt_u8_hi_to_u16, OpcodeVV::kCvtU8HiToU16)
  DEFINE_OP_2V(v_cvt_i8_to_i32, OpcodeVV::kCvtI8ToI32)
  DEFINE_OP_2V(v_cvt_u8_to_u32, OpcodeVV::kCvtU8ToU32)
  DEFINE_OP_2V(v_cvt_i16_lo_to_i32, OpcodeVV::kCvtI16LoToI32)
  DEFINE_OP_2V(v_cvt_i16_hi_to_i32, OpcodeVV::kCvtI16HiToI32)
  DEFINE_OP_2V(v_cvt_u16_lo_to_u32, OpcodeVV::kCvtU16LoToU32)
  DEFINE_OP_2V(v_cvt_u16_hi_to_u32, OpcodeVV::kCvtU16HiToU32)
  DEFINE_OP_2V(v_cvt_i32_lo_to_i64, OpcodeVV::kCvtI32LoToI64)
  DEFINE_OP_2V(v_cvt_i32_hi_to_i64, OpcodeVV::kCvtI32HiToI64)
  DEFINE_OP_2V(v_cvt_u32_lo_to_u64, OpcodeVV::kCvtU32LoToU64)
  DEFINE_OP_2V(v_cvt_u32_hi_to_u64, OpcodeVV::kCvtU32HiToU64)
  DEFINE_OP_2V(v_abs_f32, OpcodeVV::kAbsF32)
  DEFINE_OP_2V(v_abs_f64, OpcodeVV::kAbsF64)
  DEFINE_OP_2V(v_neg_f32, OpcodeVV::kNegF32)
  DEFINE_OP_2V(v_neg_f64, OpcodeVV::kNegF64)
  DEFINE_OP_2V(v_not_f32, OpcodeVV::kNotF32)
  DEFINE_OP_2V(v_not_f64, OpcodeVV::kNotF64)
  DEFINE_OP_2V(s_trunc_f32, OpcodeVV::kTruncF32S)
  DEFINE_OP_2V(s_trunc_f64, OpcodeVV::kTruncF64S)
  DEFINE_OP_2V(v_trunc_f32, OpcodeVV::kTruncF32)
  DEFINE_OP_2V(v_trunc_f64, OpcodeVV::kTruncF64)
  DEFINE_OP_2V(s_floor_f32, OpcodeVV::kFloorF32S)
  DEFINE_OP_2V(s_floor_f64, OpcodeVV::kFloorF64S)
  DEFINE_OP_2V(v_floor_f32, OpcodeVV::kFloorF32)
  DEFINE_OP_2V(v_floor_f64, OpcodeVV::kFloorF64)
  DEFINE_OP_2V(s_ceil_f32, OpcodeVV::kCeilF32S)
  DEFINE_OP_2V(s_ceil_f64, OpcodeVV::kCeilF64S)
  DEFINE_OP_2V(v_ceil_f32, OpcodeVV::kCeilF32)
  DEFINE_OP_2V(v_ceil_f64, OpcodeVV::kCeilF64)
  DEFINE_OP_2V(s_round_f32, OpcodeVV::kRoundF32S)
  DEFINE_OP_2V(s_round_f64, OpcodeVV::kRoundF64S)
  DEFINE_OP_2V(v_round_f32, OpcodeVV::kRoundF32)
  DEFINE_OP_2V(v_round_f64, OpcodeVV::kRoundF64)
  DEFINE_OP_2V(v_rcp_f32, OpcodeVV::kRcpF32)
  DEFINE_OP_2V(v_rcp_f64, OpcodeVV::kRcpF64)
  DEFINE_OP_2V(s_sqrt_f32, OpcodeVV::kSqrtF32S)
  DEFINE_OP_2V(s_sqrt_f64, OpcodeVV::kSqrtF64S)
  DEFINE_OP_2V(v_sqrt_f32, OpcodeVV::kSqrtF32)
  DEFINE_OP_2V(v_sqrt_f64, OpcodeVV::kSqrtF64)
  DEFINE_OP_2V(s_cvt_f32_to_f64, OpcodeVV::kCvtF32ToF64S)
  DEFINE_OP_2V(s_cvt_f64_to_f32, OpcodeVV::kCvtF64ToF32S)
  DEFINE_OP_2V(v_cvt_i32_to_f32, OpcodeVV::kCvtI32ToF32)
  DEFINE_OP_2V(v_cvt_f32_lo_to_f64, OpcodeVV::kCvtF32LoToF64)
  DEFINE_OP_2V(v_cvt_f32_hi_to_f64, OpcodeVV::kCvtF32HiToF64)
  DEFINE_OP_2V(v_cvt_f64_to_f32_lo, OpcodeVV::kCvtF64ToF32Lo)
  DEFINE_OP_2V(v_cvt_f64_to_f32_hi, OpcodeVV::kCvtF64ToF32Hi)
  DEFINE_OP_2V(v_cvt_i32_lo_to_f64, OpcodeVV::kCvtI32LoToF64)
  DEFINE_OP_2V(v_cvt_i32_hi_to_f64, OpcodeVV::kCvtI32HiToF64)
  DEFINE_OP_2V(v_cvt_trunc_f32_to_i32, OpcodeVV::kCvtTruncF32ToI32)
  DEFINE_OP_2V(v_cvt_trunc_f64_to_i32_lo, OpcodeVV::kCvtTruncF64ToI32Lo)
  DEFINE_OP_2V(v_cvt_trunc_f64_to_i32_hi, OpcodeVV::kCvtTruncF64ToI32Hi)
  DEFINE_OP_2V(v_cvt_round_f32_to_i32, OpcodeVV::kCvtRoundF32ToI32)
  DEFINE_OP_2V(v_cvt_round_f64_to_i32_lo, OpcodeVV::kCvtRoundF64ToI32Lo)
  DEFINE_OP_2V(v_cvt_round_f64_to_i32_hi, OpcodeVV::kCvtRoundF64ToI32Hi)

  DEFINE_OP_2VI(v_slli_i16, OpcodeVVI::kSllU16)
  DEFINE_OP_2VI(v_slli_u16, OpcodeVVI::kSllU16)
  DEFINE_OP_2VI(v_slli_i32, OpcodeVVI::kSllU32)
  DEFINE_OP_2VI(v_slli_u32, OpcodeVVI::kSllU32)
  DEFINE_OP_2VI(v_slli_i64, OpcodeVVI::kSllU64)
  DEFINE_OP_2VI(v_slli_u64, OpcodeVVI::kSllU64)
  DEFINE_OP_2VI(v_srli_u16, OpcodeVVI::kSrlU16)
  DEFINE_OP_2VI(v_srli_u32, OpcodeVVI::kSrlU32)
  DEFINE_OP_2VI(v_srli_u64, OpcodeVVI::kSrlU64)
  DEFINE_OP_2VI(v_srai_i16, OpcodeVVI::kSraI16)
  DEFINE_OP_2VI(v_srai_i32, OpcodeVVI::kSraI32)
  DEFINE_OP_2VI(v_srai_i64, OpcodeVVI::kSraI64)
  DEFINE_OP_2VI(v_sllb_u128, OpcodeVVI::kSllbU128)
  DEFINE_OP_2VI(v_srlb_u128, OpcodeVVI::kSrlbU128)
  DEFINE_OP_2VI_WRAP(v_swizzle_u16x4, Swizzle4, OpcodeVVI::kSwizzleU16x4)
  DEFINE_OP_2VI_WRAP(v_swizzle_lo_u16x4, Swizzle4, OpcodeVVI::kSwizzleLoU16x4)
  DEFINE_OP_2VI_WRAP(v_swizzle_hi_u16x4, Swizzle4, OpcodeVVI::kSwizzleHiU16x4)
  DEFINE_OP_2VI_WRAP(v_swizzle_u32x4, Swizzle4, OpcodeVVI::kSwizzleU32x4)
  DEFINE_OP_2VI_WRAP(v_swizzle_u64x2, Swizzle2, OpcodeVVI::kSwizzleU64x2)
  DEFINE_OP_2VI_WRAP(v_swizzle_f32x4, Swizzle4, OpcodeVVI::kSwizzleF32x4)
  DEFINE_OP_2VI_WRAP(v_swizzle_f64x2, Swizzle2, OpcodeVVI::kSwizzleF64x2)
  DEFINE_OP_2VI_WRAP(v_swizzle_u64x4, Swizzle4, OpcodeVVI::kSwizzleU64x4)
  DEFINE_OP_2VI_WRAP(v_swizzle_f64x4, Swizzle4, OpcodeVVI::kSwizzleF64x4)
  DEFINE_OP_2VI(v_extract_v128, OpcodeVVI::kExtractV128_I32)
  DEFINE_OP_2VI(v_extract_v128_i32, OpcodeVVI::kExtractV128_I32)
  DEFINE_OP_2VI(v_extract_v128_i64, OpcodeVVI::kExtractV128_I64)
  DEFINE_OP_2VI(v_extract_v128_f32, OpcodeVVI::kExtractV128_F32)
  DEFINE_OP_2VI(v_extract_v128_f64, OpcodeVVI::kExtractV128_F64)
  DEFINE_OP_2VI(v_extract_v256, OpcodeVVI::kExtractV256_I32)
  DEFINE_OP_2VI(v_extract_v256_i32, OpcodeVVI::kExtractV256_I32)
  DEFINE_OP_2VI(v_extract_v256_i64, OpcodeVVI::kExtractV256_I64)
  DEFINE_OP_2VI(v_extract_v256_f32, OpcodeVVI::kExtractV256_F32)
  DEFINE_OP_2VI(v_extract_v256_f64, OpcodeVVI::kExtractV256_F64)

#if defined(BL_JIT_ARCH_A64)
  DEFINE_OP_2VI(v_srli_rnd_u16, OpcodeVVI::kSrlRndU16)
  DEFINE_OP_2VI(v_srli_rnd_u32, OpcodeVVI::kSrlRndU32)
  DEFINE_OP_2VI(v_srli_rnd_u64, OpcodeVVI::kSrlRndU64)
  DEFINE_OP_2VI(v_srli_acc_u16, OpcodeVVI::kSrlAccU16)
  DEFINE_OP_2VI(v_srli_acc_u32, OpcodeVVI::kSrlAccU32)
  DEFINE_OP_2VI(v_srli_acc_u64, OpcodeVVI::kSrlAccU64)
  DEFINE_OP_2VI(v_srli_rnd_acc_u16, OpcodeVVI::kSrlRndAccU16)
  DEFINE_OP_2VI(v_srli_rnd_acc_u32, OpcodeVVI::kSrlRndAccU32)
  DEFINE_OP_2VI(v_srli_rnd_acc_u64, OpcodeVVI::kSrlRndAccU64)

  DEFINE_OP_2VI(v_srlni_lo_u16, OpcodeVVI::kSrlnLoU16)
  DEFINE_OP_2VI(v_srlni_hi_u16, OpcodeVVI::kSrlnHiU16)
  DEFINE_OP_2VI(v_srlni_lo_u32, OpcodeVVI::kSrlnLoU32)
  DEFINE_OP_2VI(v_srlni_hi_u32, OpcodeVVI::kSrlnHiU32)
  DEFINE_OP_2VI(v_srlni_lo_u64, OpcodeVVI::kSrlnLoU64)
  DEFINE_OP_2VI(v_srlni_hi_u64, OpcodeVVI::kSrlnHiU64)

  DEFINE_OP_2VI(v_srlni_rnd_lo_u16, OpcodeVVI::kSrlnRndLoU16)
  DEFINE_OP_2VI(v_srlni_rnd_hi_u16, OpcodeVVI::kSrlnRndHiU16)
  DEFINE_OP_2VI(v_srlni_rnd_lo_u32, OpcodeVVI::kSrlnRndLoU32)
  DEFINE_OP_2VI(v_srlni_rnd_hi_u32, OpcodeVVI::kSrlnRndHiU32)
  DEFINE_OP_2VI(v_srlni_rnd_lo_u64, OpcodeVVI::kSrlnRndLoU64)
  DEFINE_OP_2VI(v_srlni_rnd_hi_u64, OpcodeVVI::kSrlnRndHiU64)
#endif // BL_JIT_ARCH_A64

  DEFINE_OP_VM_U(v_load8, OpcodeVM::kLoad8, 1)
  DEFINE_OP_VM_U(v_loadu16, OpcodeVM::kLoad16_U16, 1)
  DEFINE_OP_VM_A(v_loada16, OpcodeVM::kLoad16_U16, 2)
  DEFINE_OP_VM_U(v_loadu32, OpcodeVM::kLoad32_U32, 1)
  DEFINE_OP_VM_A(v_loada32, OpcodeVM::kLoad32_U32, 4)
  DEFINE_OP_VM_U(v_loadu32_u32, OpcodeVM::kLoad32_U32, 1)
  DEFINE_OP_VM_A(v_loada32_u32, OpcodeVM::kLoad32_U32, 4)
  DEFINE_OP_VM_U(v_loadu32_f32, OpcodeVM::kLoad32_F32, 1)
  DEFINE_OP_VM_A(v_loada32_f32, OpcodeVM::kLoad32_F32, 4)
  DEFINE_OP_VM_U(v_loadu64, OpcodeVM::kLoad64_U32, 1)
  DEFINE_OP_VM_A(v_loada64, OpcodeVM::kLoad64_U32, 8)
  DEFINE_OP_VM_U(v_loadu64_u32, OpcodeVM::kLoad64_U32, 1)
  DEFINE_OP_VM_A(v_loada64_u32, OpcodeVM::kLoad64_U32, 8)
  DEFINE_OP_VM_U(v_loadu64_u64, OpcodeVM::kLoad64_U64, 1)
  DEFINE_OP_VM_A(v_loada64_u64, OpcodeVM::kLoad64_U64, 8)
  DEFINE_OP_VM_U(v_loadu64_f32, OpcodeVM::kLoad64_F32, 1)
  DEFINE_OP_VM_A(v_loada64_f32, OpcodeVM::kLoad64_F32, 8)
  DEFINE_OP_VM_U(v_loadu64_f64, OpcodeVM::kLoad64_F64, 1)
  DEFINE_OP_VM_A(v_loada64_f64, OpcodeVM::kLoad64_F64, 8)
  DEFINE_OP_VM_U(v_loadu128, OpcodeVM::kLoad128_U32, 1)
  DEFINE_OP_VM_A(v_loada128, OpcodeVM::kLoad128_U32, 16)
  DEFINE_OP_VM_U(v_loadu128_u32, OpcodeVM::kLoad128_U32, 1)
  DEFINE_OP_VM_A(v_loada128_u32, OpcodeVM::kLoad128_U32, 16)
  DEFINE_OP_VM_U(v_loadu128_u64, OpcodeVM::kLoad128_U64, 1)
  DEFINE_OP_VM_A(v_loada128_u64, OpcodeVM::kLoad128_U64, 16)
  DEFINE_OP_VM_U(v_loadu128_f32, OpcodeVM::kLoad128_F32, 1)
  DEFINE_OP_VM_A(v_loada128_f32, OpcodeVM::kLoad128_F32, 16)
  DEFINE_OP_VM_U(v_loadu128_f64, OpcodeVM::kLoad128_F64, 1)
  DEFINE_OP_VM_A(v_loada128_f64, OpcodeVM::kLoad128_F64, 16)
  DEFINE_OP_VM_U(v_loadu256, OpcodeVM::kLoad256_U32, 1)
  DEFINE_OP_VM_A(v_loada256, OpcodeVM::kLoad256_U32, 32)
  DEFINE_OP_VM_U(v_loadu256_u32, OpcodeVM::kLoad256_U32, 1)
  DEFINE_OP_VM_A(v_loada256_u32, OpcodeVM::kLoad256_U32, 32)
  DEFINE_OP_VM_U(v_loadu256_u64, OpcodeVM::kLoad256_U64, 1)
  DEFINE_OP_VM_A(v_loada256_u64, OpcodeVM::kLoad256_U64, 32)
  DEFINE_OP_VM_U(v_loadu256_f32, OpcodeVM::kLoad256_F32, 1)
  DEFINE_OP_VM_A(v_loada256_f32, OpcodeVM::kLoad256_F32, 32)
  DEFINE_OP_VM_U(v_loadu256_f64, OpcodeVM::kLoad256_F64, 1)
  DEFINE_OP_VM_A(v_loada256_f64, OpcodeVM::kLoad256_F64, 32)
  DEFINE_OP_VM_U(v_loadu512, OpcodeVM::kLoad512_U32, 1)
  DEFINE_OP_VM_A(v_loada512, OpcodeVM::kLoad512_U32, 64)
  DEFINE_OP_VM_U(v_loadu512_u32, OpcodeVM::kLoad512_U32, 1)
  DEFINE_OP_VM_A(v_loada512_u32, OpcodeVM::kLoad512_U32, 64)
  DEFINE_OP_VM_U(v_loadu512_u64, OpcodeVM::kLoad512_U64, 1)
  DEFINE_OP_VM_A(v_loada512_u64, OpcodeVM::kLoad512_U64, 64)
  DEFINE_OP_VM_U(v_loadu512_f32, OpcodeVM::kLoad512_F32, 1)
  DEFINE_OP_VM_A(v_loada512_f32, OpcodeVM::kLoad512_F32, 64)
  DEFINE_OP_VM_U(v_loadu512_f64, OpcodeVM::kLoad512_F64, 1)
  DEFINE_OP_VM_A(v_loada512_f64, OpcodeVM::kLoad512_F64, 64)
  DEFINE_OP_VM_U(v_loaduvec, OpcodeVM::kLoadN_U32, 1)
  DEFINE_OP_VM_A(v_loadavec, OpcodeVM::kLoadN_U32, 0)
  DEFINE_OP_VM_U(v_loaduvec_u32, OpcodeVM::kLoadN_U32, 1)
  DEFINE_OP_VM_A(v_loadavec_u32, OpcodeVM::kLoadN_U32, 0)
  DEFINE_OP_VM_U(v_loaduvec_u64, OpcodeVM::kLoadN_U64, 1)
  DEFINE_OP_VM_A(v_loadavec_u64, OpcodeVM::kLoadN_U64, 0)
  DEFINE_OP_VM_U(v_loaduvec_f32, OpcodeVM::kLoadN_F32, 1)
  DEFINE_OP_VM_A(v_loadavec_f32, OpcodeVM::kLoadN_F32, 0)
  DEFINE_OP_VM_U(v_loaduvec_f64, OpcodeVM::kLoadN_F64, 1)
  DEFINE_OP_VM_A(v_loadavec_f64, OpcodeVM::kLoadN_F64, 0)

  DEFINE_OP_VM_A(v_loadu16_u8_to_u64, OpcodeVM::kLoadCvt16_U8ToU64, 1)
  DEFINE_OP_VM_A(v_loada16_u8_to_u64, OpcodeVM::kLoadCvt16_U8ToU64, 2)
  DEFINE_OP_VM_A(v_loadu32_u8_to_u64, OpcodeVM::kLoadCvt32_U8ToU64, 1)
  DEFINE_OP_VM_A(v_loada32_u8_to_u64, OpcodeVM::kLoadCvt32_U8ToU64, 2)
  DEFINE_OP_VM_A(v_loadu64_u8_to_u64, OpcodeVM::kLoadCvt64_U8ToU64, 1)
  DEFINE_OP_VM_A(v_loada64_u8_to_u64, OpcodeVM::kLoadCvt64_U8ToU64, 2)

  DEFINE_OP_VM_A(v_loadu32_i8_to_i16, OpcodeVM::kLoadCvt32_I8ToI16, 1)
  DEFINE_OP_VM_A(v_loada32_i8_to_i16, OpcodeVM::kLoadCvt32_I8ToI16, 4)
  DEFINE_OP_VM_A(v_loadu32_u8_to_u16, OpcodeVM::kLoadCvt32_U8ToU16, 1)
  DEFINE_OP_VM_A(v_loada32_u8_to_u16, OpcodeVM::kLoadCvt32_U8ToU16, 4)
  DEFINE_OP_VM_A(v_loadu32_i8_to_i32, OpcodeVM::kLoadCvt32_I8ToI32, 1)
  DEFINE_OP_VM_A(v_loada32_i8_to_i32, OpcodeVM::kLoadCvt32_I8ToI32, 4)
  DEFINE_OP_VM_A(v_loadu32_u8_to_u32, OpcodeVM::kLoadCvt32_U8ToU32, 1)
  DEFINE_OP_VM_A(v_loada32_u8_to_u32, OpcodeVM::kLoadCvt32_U8ToU32, 4)
  DEFINE_OP_VM_A(v_loadu32_i16_to_i32, OpcodeVM::kLoadCvt32_I16ToI32, 1)
  DEFINE_OP_VM_A(v_loada32_i16_to_i32, OpcodeVM::kLoadCvt32_I16ToI32, 4)
  DEFINE_OP_VM_A(v_loadu32_u16_to_u32, OpcodeVM::kLoadCvt32_U16ToU32, 1)
  DEFINE_OP_VM_A(v_loada32_u16_to_u32, OpcodeVM::kLoadCvt32_U16ToU32, 4)
  DEFINE_OP_VM_A(v_loadu32_i32_to_i64, OpcodeVM::kLoadCvt32_I32ToI64, 1)
  DEFINE_OP_VM_A(v_loada32_i32_to_i64, OpcodeVM::kLoadCvt32_I32ToI64, 4)
  DEFINE_OP_VM_A(v_loadu32_u32_to_u64, OpcodeVM::kLoadCvt32_U32ToU64, 1)
  DEFINE_OP_VM_A(v_loada32_u32_to_u64, OpcodeVM::kLoadCvt32_U32ToU64, 4)
  DEFINE_OP_VM_A(v_loadu64_i8_to_i16, OpcodeVM::kLoadCvt64_I8ToI16, 1)
  DEFINE_OP_VM_A(v_loada64_i8_to_i16, OpcodeVM::kLoadCvt64_I8ToI16, 8)
  DEFINE_OP_VM_A(v_loadu64_u8_to_u16, OpcodeVM::kLoadCvt64_U8ToU16, 1)
  DEFINE_OP_VM_A(v_loada64_u8_to_u16, OpcodeVM::kLoadCvt64_U8ToU16, 8)
  DEFINE_OP_VM_A(v_loadu64_i8_to_i32, OpcodeVM::kLoadCvt64_I8ToI32, 1)
  DEFINE_OP_VM_A(v_loada64_i8_to_i32, OpcodeVM::kLoadCvt64_I8ToI32, 8)
  DEFINE_OP_VM_A(v_loadu64_u8_to_u32, OpcodeVM::kLoadCvt64_U8ToU32, 1)
  DEFINE_OP_VM_A(v_loada64_u8_to_u32, OpcodeVM::kLoadCvt64_U8ToU32, 8)
  DEFINE_OP_VM_A(v_loadu64_i16_to_i32, OpcodeVM::kLoadCvt64_I16ToI32, 1)
  DEFINE_OP_VM_A(v_loada64_i16_to_i32, OpcodeVM::kLoadCvt64_I16ToI32, 8)
  DEFINE_OP_VM_A(v_loadu64_u16_to_u32, OpcodeVM::kLoadCvt64_U16ToU32, 1)
  DEFINE_OP_VM_A(v_loada64_u16_to_u32, OpcodeVM::kLoadCvt64_U16ToU32, 8)
  DEFINE_OP_VM_A(v_loadu64_i32_to_i64, OpcodeVM::kLoadCvt64_I32ToI64, 1)
  DEFINE_OP_VM_A(v_loada64_i32_to_i64, OpcodeVM::kLoadCvt64_I32ToI64, 8)
  DEFINE_OP_VM_A(v_loadu64_u32_to_u64, OpcodeVM::kLoadCvt64_U32ToU64, 1)
  DEFINE_OP_VM_A(v_loada64_u32_to_u64, OpcodeVM::kLoadCvt64_U32ToU64, 8)
  DEFINE_OP_VM_A(v_loadu128_i8_to_i16, OpcodeVM::kLoadCvt128_I8ToI16, 1)
  DEFINE_OP_VM_A(v_loada128_i8_to_i16, OpcodeVM::kLoadCvt128_I8ToI16, 16)
  DEFINE_OP_VM_A(v_loadu128_u8_to_u16, OpcodeVM::kLoadCvt128_U8ToU16, 1)
  DEFINE_OP_VM_A(v_loada128_u8_to_u16, OpcodeVM::kLoadCvt128_U8ToU16, 16)
  DEFINE_OP_VM_A(v_loadu128_i8_to_i32, OpcodeVM::kLoadCvt128_I8ToI32, 1)
  DEFINE_OP_VM_A(v_loada128_i8_to_i32, OpcodeVM::kLoadCvt128_I8ToI32, 16)
  DEFINE_OP_VM_A(v_loadu128_u8_to_u32, OpcodeVM::kLoadCvt128_U8ToU32, 1)
  DEFINE_OP_VM_A(v_loada128_u8_to_u32, OpcodeVM::kLoadCvt128_U8ToU32, 16)
  DEFINE_OP_VM_A(v_loadu128_i16_to_i32, OpcodeVM::kLoadCvt128_I16ToI32, 1)
  DEFINE_OP_VM_A(v_loada128_i16_to_i32, OpcodeVM::kLoadCvt128_I16ToI32, 16)
  DEFINE_OP_VM_A(v_loadu128_u16_to_u32, OpcodeVM::kLoadCvt128_U16ToU32, 1)
  DEFINE_OP_VM_A(v_loada128_u16_to_u32, OpcodeVM::kLoadCvt128_U16ToU32, 16)
  DEFINE_OP_VM_A(v_loadu128_i32_to_i64, OpcodeVM::kLoadCvt128_I32ToI64, 1)
  DEFINE_OP_VM_A(v_loada128_i32_to_i64, OpcodeVM::kLoadCvt128_I32ToI64, 16)
  DEFINE_OP_VM_A(v_loadu128_u32_to_u64, OpcodeVM::kLoadCvt128_U32ToU64, 1)
  DEFINE_OP_VM_A(v_loada128_u32_to_u64, OpcodeVM::kLoadCvt128_U32ToU64, 16)
  DEFINE_OP_VM_A(v_loadu256_i8_to_i16, OpcodeVM::kLoadCvt256_I8ToI16, 1)
  DEFINE_OP_VM_A(v_loada256_i8_to_i16, OpcodeVM::kLoadCvt256_I8ToI16, 32)
  DEFINE_OP_VM_A(v_loadu256_u8_to_u16, OpcodeVM::kLoadCvt256_U8ToU16, 1)
  DEFINE_OP_VM_A(v_loada256_u8_to_u16, OpcodeVM::kLoadCvt256_U8ToU16, 32)
  DEFINE_OP_VM_A(v_loadu256_i16_to_i32, OpcodeVM::kLoadCvt256_I16ToI32, 1)
  DEFINE_OP_VM_A(v_loada256_i16_to_i32, OpcodeVM::kLoadCvt256_I16ToI32, 32)
  DEFINE_OP_VM_A(v_loadu256_u16_to_u32, OpcodeVM::kLoadCvt256_U16ToU32, 1)
  DEFINE_OP_VM_A(v_loada256_u16_to_u32, OpcodeVM::kLoadCvt256_U16ToU32, 32)
  DEFINE_OP_VM_A(v_loadu256_i32_to_i64, OpcodeVM::kLoadCvt256_I32ToI64, 1)
  DEFINE_OP_VM_A(v_loada256_i32_to_i64, OpcodeVM::kLoadCvt256_I32ToI64, 32)
  DEFINE_OP_VM_A(v_loadu256_u32_to_u64, OpcodeVM::kLoadCvt256_U32ToU64, 1)
  DEFINE_OP_VM_A(v_loada256_u32_to_u64, OpcodeVM::kLoadCvt256_U32ToU64, 32)
  DEFINE_OP_VM_A(v_loaduvec_i8_to_i16, OpcodeVM::kLoadCvtN_I8ToI16, 1)
  DEFINE_OP_VM_A(v_loadavec_i8_to_i16, OpcodeVM::kLoadCvtN_I8ToI16, 0)
  DEFINE_OP_VM_A(v_loaduvec_u8_to_u16, OpcodeVM::kLoadCvtN_U8ToU16, 1)
  DEFINE_OP_VM_A(v_loadavec_u8_to_u16, OpcodeVM::kLoadCvtN_U8ToU16, 0)
  DEFINE_OP_VM_A(v_loaduvec_i8_to_i32, OpcodeVM::kLoadCvtN_I8ToI32, 1)
  DEFINE_OP_VM_A(v_loadavec_i8_to_i32, OpcodeVM::kLoadCvtN_I8ToI32, 0)
  DEFINE_OP_VM_A(v_loaduvec_u8_to_u32, OpcodeVM::kLoadCvtN_U8ToU32, 1)
  DEFINE_OP_VM_A(v_loadavec_u8_to_u32, OpcodeVM::kLoadCvtN_U8ToU32, 0)
  DEFINE_OP_VM_A(v_loaduvec_u8_to_u64, OpcodeVM::kLoadCvtN_U8ToU64, 1)
  DEFINE_OP_VM_A(v_loadavec_u8_to_u64, OpcodeVM::kLoadCvtN_U8ToU64, 0)
  DEFINE_OP_VM_A(v_loaduvec_i16_to_i32, OpcodeVM::kLoadCvtN_I16ToI32, 1)
  DEFINE_OP_VM_A(v_loadavec_i16_to_i32, OpcodeVM::kLoadCvtN_I16ToI32, 0)
  DEFINE_OP_VM_A(v_loaduvec_u16_to_u32, OpcodeVM::kLoadCvtN_U16ToU32, 1)
  DEFINE_OP_VM_A(v_loadavec_u16_to_u32, OpcodeVM::kLoadCvtN_U16ToU32, 0)
  DEFINE_OP_VM_A(v_loaduvec_i32_to_i64, OpcodeVM::kLoadCvtN_I32ToI64, 1)
  DEFINE_OP_VM_A(v_loadavec_i32_to_i64, OpcodeVM::kLoadCvtN_I32ToI64, 0)
  DEFINE_OP_VM_A(v_loaduvec_u32_to_u64, OpcodeVM::kLoadCvtN_U32ToU64, 1)
  DEFINE_OP_VM_A(v_loadavec_u32_to_u64, OpcodeVM::kLoadCvtN_U32ToU64, 0)

  DEFINE_OP_VM_I(v_insert_u8, OpcodeVM::kLoadInsertU8, 1)
  DEFINE_OP_VM_I(v_insert_u16, OpcodeVM::kLoadInsertU16, 1)
  DEFINE_OP_VM_I(v_insert_u32, OpcodeVM::kLoadInsertU32, 1)
  DEFINE_OP_VM_I(v_insert_u64, OpcodeVM::kLoadInsertU64, 1)
  DEFINE_OP_VM_I(v_insert_f32, OpcodeVM::kLoadInsertF32, 1)
  DEFINE_OP_VM_I(v_insert_f32x2, OpcodeVM::kLoadInsertF32x2, 1)
  DEFINE_OP_VM_I(v_insert_f64, OpcodeVM::kLoadInsertF64, 1)

  DEFINE_OP_MV_U(v_store8, OpcodeMV::kStore8, 1)
  DEFINE_OP_MV_U(v_storeu16, OpcodeMV::kStore16_U16, 1)
  DEFINE_OP_MV_A(v_storea16, OpcodeMV::kStore16_U16, 2)
  DEFINE_OP_MV_U(v_storeu32, OpcodeMV::kStore32_U32, 1)
  DEFINE_OP_MV_A(v_storea32, OpcodeMV::kStore32_U32, 4)
  DEFINE_OP_MV_U(v_storeu32_u32, OpcodeMV::kStore32_U32, 1)
  DEFINE_OP_MV_A(v_storea32_u32, OpcodeMV::kStore32_U32, 4)
  DEFINE_OP_MV_U(v_storeu32_f32, OpcodeMV::kStore32_F32, 1)
  DEFINE_OP_MV_A(v_storea32_f32, OpcodeMV::kStore32_F32, 4)
  DEFINE_OP_MV_U(v_storeu64, OpcodeMV::kStore64_U32, 1)
  DEFINE_OP_MV_A(v_storea64, OpcodeMV::kStore64_U32, 8)
  DEFINE_OP_MV_U(v_storeu64_u32, OpcodeMV::kStore64_U32, 1)
  DEFINE_OP_MV_A(v_storea64_u32, OpcodeMV::kStore64_U32, 8)
  DEFINE_OP_MV_U(v_storeu64_u64, OpcodeMV::kStore64_U64, 1)
  DEFINE_OP_MV_A(v_storea64_u64, OpcodeMV::kStore64_U64, 8)
  DEFINE_OP_MV_U(v_storeu64_f32, OpcodeMV::kStore64_F32, 1)
  DEFINE_OP_MV_A(v_storea64_f32, OpcodeMV::kStore64_F32, 8)
  DEFINE_OP_MV_U(v_storeu64_f64, OpcodeMV::kStore64_F64, 1)
  DEFINE_OP_MV_A(v_storea64_f64, OpcodeMV::kStore64_F64, 8)
  DEFINE_OP_MV_U(v_storeu128, OpcodeMV::kStore128_U32, 1)
  DEFINE_OP_MV_A(v_storea128, OpcodeMV::kStore128_U32, 16)
  DEFINE_OP_MV_U(v_storeu128_u32, OpcodeMV::kStore128_U32, 1)
  DEFINE_OP_MV_A(v_storea128_u32, OpcodeMV::kStore128_U32, 16)
  DEFINE_OP_MV_U(v_storeu128_u64, OpcodeMV::kStore128_U64, 1)
  DEFINE_OP_MV_A(v_storea128_u64, OpcodeMV::kStore128_U64, 16)
  DEFINE_OP_MV_U(v_storeu128_f32, OpcodeMV::kStore128_F32, 1)
  DEFINE_OP_MV_A(v_storea128_f32, OpcodeMV::kStore128_F32, 16)
  DEFINE_OP_MV_U(v_storeu128_f64, OpcodeMV::kStore128_F64, 1)
  DEFINE_OP_MV_A(v_storea128_f64, OpcodeMV::kStore128_F64, 16)
  DEFINE_OP_MV_U(v_storeu256, OpcodeMV::kStore256_U32, 1)
  DEFINE_OP_MV_A(v_storea256, OpcodeMV::kStore256_U32, 32)
  DEFINE_OP_MV_U(v_storeu256_u32, OpcodeMV::kStore256_U32, 1)
  DEFINE_OP_MV_A(v_storea256_u32, OpcodeMV::kStore256_U32, 32)
  DEFINE_OP_MV_U(v_storeu256_u64, OpcodeMV::kStore256_U64, 1)
  DEFINE_OP_MV_A(v_storea256_u64, OpcodeMV::kStore256_U64, 32)
  DEFINE_OP_MV_U(v_storeu256_f32, OpcodeMV::kStore256_F32, 1)
  DEFINE_OP_MV_A(v_storea256_f32, OpcodeMV::kStore256_F32, 32)
  DEFINE_OP_MV_U(v_storeu256_f64, OpcodeMV::kStore256_F64, 1)
  DEFINE_OP_MV_A(v_storea256_f64, OpcodeMV::kStore256_F64, 32)
  DEFINE_OP_MV_U(v_storeu512, OpcodeMV::kStore512_U32, 1)
  DEFINE_OP_MV_A(v_storea512, OpcodeMV::kStore512_U32, 64)
  DEFINE_OP_MV_U(v_storeu512_u32, OpcodeMV::kStore512_U32, 1)
  DEFINE_OP_MV_A(v_storea512_u32, OpcodeMV::kStore512_U32, 64)
  DEFINE_OP_MV_U(v_storeu512_u64, OpcodeMV::kStore512_U64, 1)
  DEFINE_OP_MV_A(v_storea512_u64, OpcodeMV::kStore512_U64, 64)
  DEFINE_OP_MV_U(v_storeu512_f32, OpcodeMV::kStore512_F32, 1)
  DEFINE_OP_MV_A(v_storea512_f32, OpcodeMV::kStore512_F32, 64)
  DEFINE_OP_MV_U(v_storeu512_f64, OpcodeMV::kStore512_F64, 1)
  DEFINE_OP_MV_A(v_storea512_f64, OpcodeMV::kStore512_F64, 64)
  DEFINE_OP_MV_U(v_storeuvec, OpcodeMV::kStoreN_U32, 1)
  DEFINE_OP_MV_A(v_storeavec, OpcodeMV::kStoreN_U32, 0)
  DEFINE_OP_MV_U(v_storeuvec_u32, OpcodeMV::kStoreN_U32, 1)
  DEFINE_OP_MV_A(v_storeavec_u32, OpcodeMV::kStoreN_U32, 0)
  DEFINE_OP_MV_U(v_storeuvec_u64, OpcodeMV::kStoreN_U64, 1)
  DEFINE_OP_MV_A(v_storeavec_u64, OpcodeMV::kStoreN_U64, 0)
  DEFINE_OP_MV_U(v_storeuvec_f32, OpcodeMV::kStoreN_F32, 1)
  DEFINE_OP_MV_A(v_storeavec_f32, OpcodeMV::kStoreN_F32, 0)
  DEFINE_OP_MV_U(v_storeuvec_f64, OpcodeMV::kStoreN_F64, 1)
  DEFINE_OP_MV_A(v_storeavec_f64, OpcodeMV::kStoreN_F64, 0)

  DEFINE_OP_MV_I(v_store_extract_u16, OpcodeMV::kStoreExtractU16, 1)
  DEFINE_OP_MV_I(v_store_extract_u32, OpcodeMV::kStoreExtractU32, 1)
  DEFINE_OP_MV_I(v_store_extract_u64, OpcodeMV::kStoreExtractU64, 1)

  DEFINE_OP_3V(v_and_i32, OpcodeVVV::kAndU32)
  DEFINE_OP_3V(v_and_u32, OpcodeVVV::kAndU32)
  DEFINE_OP_3V(v_and_i64, OpcodeVVV::kAndU64)
  DEFINE_OP_3V(v_and_u64, OpcodeVVV::kAndU64)
  DEFINE_OP_3V(v_or_i32, OpcodeVVV::kOrU32)
  DEFINE_OP_3V(v_or_u32, OpcodeVVV::kOrU32)
  DEFINE_OP_3V(v_or_i64, OpcodeVVV::kOrU64)
  DEFINE_OP_3V(v_or_u64, OpcodeVVV::kOrU64)
  DEFINE_OP_3V(v_xor_i32, OpcodeVVV::kXorU32)
  DEFINE_OP_3V(v_xor_u32, OpcodeVVV::kXorU32)
  DEFINE_OP_3V(v_xor_i64, OpcodeVVV::kXorU64)
  DEFINE_OP_3V(v_xor_u64, OpcodeVVV::kXorU64)
  DEFINE_OP_3V(v_andn_i32, OpcodeVVV::kAndnU32)
  DEFINE_OP_3V(v_andn_u32, OpcodeVVV::kAndnU32)
  DEFINE_OP_3V(v_andn_i64, OpcodeVVV::kAndnU64)
  DEFINE_OP_3V(v_andn_u64, OpcodeVVV::kAndnU64)
  DEFINE_OP_3V(v_bic_i32, OpcodeVVV::kBicU32)
  DEFINE_OP_3V(v_bic_u32, OpcodeVVV::kBicU32)
  DEFINE_OP_3V(v_bic_i64, OpcodeVVV::kBicU64)
  DEFINE_OP_3V(v_bic_u64, OpcodeVVV::kBicU64)
  DEFINE_OP_3V(v_avgr_u8, OpcodeVVV::kAvgrU8)
  DEFINE_OP_3V(v_avgr_u16, OpcodeVVV::kAvgrU16)
  DEFINE_OP_3V(v_add_i8, OpcodeVVV::kAddU8)
  DEFINE_OP_3V(v_add_u8, OpcodeVVV::kAddU8)
  DEFINE_OP_3V(v_add_i16, OpcodeVVV::kAddU16)
  DEFINE_OP_3V(v_add_u16, OpcodeVVV::kAddU16)
  DEFINE_OP_3V(v_add_i32, OpcodeVVV::kAddU32)
  DEFINE_OP_3V(v_add_u32, OpcodeVVV::kAddU32)
  DEFINE_OP_3V(v_add_i64, OpcodeVVV::kAddU64)
  DEFINE_OP_3V(v_add_u64, OpcodeVVV::kAddU64)
  DEFINE_OP_3V(v_sub_i8, OpcodeVVV::kSubU8)
  DEFINE_OP_3V(v_sub_u8, OpcodeVVV::kSubU8)
  DEFINE_OP_3V(v_sub_i16, OpcodeVVV::kSubU16)
  DEFINE_OP_3V(v_sub_u16, OpcodeVVV::kSubU16)
  DEFINE_OP_3V(v_sub_i32, OpcodeVVV::kSubU32)
  DEFINE_OP_3V(v_sub_u32, OpcodeVVV::kSubU32)
  DEFINE_OP_3V(v_sub_i64, OpcodeVVV::kSubU64)
  DEFINE_OP_3V(v_sub_u64, OpcodeVVV::kSubU64)
  DEFINE_OP_3V(v_adds_i8, OpcodeVVV::kAddsI8)
  DEFINE_OP_3V(v_adds_i16, OpcodeVVV::kAddsI16)
  DEFINE_OP_3V(v_adds_u8, OpcodeVVV::kAddsU8)
  DEFINE_OP_3V(v_adds_u16, OpcodeVVV::kAddsU16)
  DEFINE_OP_3V(v_subs_i8, OpcodeVVV::kSubsI8)
  DEFINE_OP_3V(v_subs_i16, OpcodeVVV::kSubsI16)
  DEFINE_OP_3V(v_subs_u8, OpcodeVVV::kSubsU8)
  DEFINE_OP_3V(v_subs_u16, OpcodeVVV::kSubsU16)
  DEFINE_OP_3V(v_mul_i16, OpcodeVVV::kMulU16)
  DEFINE_OP_3V(v_mul_u16, OpcodeVVV::kMulU16)
  DEFINE_OP_3V(v_mul_i32, OpcodeVVV::kMulU32)
  DEFINE_OP_3V(v_mul_u32, OpcodeVVV::kMulU32)
  DEFINE_OP_3V(v_mul_i64, OpcodeVVV::kMulU64)
  DEFINE_OP_3V(v_mul_u64, OpcodeVVV::kMulU64)
  DEFINE_OP_3V(v_mul_u64_lo_u32, OpcodeVVV::kMulU64_LoU32)
  DEFINE_OP_3V(v_mulh_i16, OpcodeVVV::kMulhI16)
  DEFINE_OP_3V(v_mulh_u16, OpcodeVVV::kMulhU16)
  DEFINE_OP_3V(v_mhadd_i16_to_i32, OpcodeVVV::kMHAddI16_I32)
  DEFINE_OP_3V(v_min_i8, OpcodeVVV::kMinI8)
  DEFINE_OP_3V(v_min_i16, OpcodeVVV::kMinI16)
  DEFINE_OP_3V(v_min_i32, OpcodeVVV::kMinI32)
  DEFINE_OP_3V(v_min_i64, OpcodeVVV::kMinI64)
  DEFINE_OP_3V(v_min_u8, OpcodeVVV::kMinU8)
  DEFINE_OP_3V(v_min_u16, OpcodeVVV::kMinU16)
  DEFINE_OP_3V(v_min_u32, OpcodeVVV::kMinU32)
  DEFINE_OP_3V(v_min_u64, OpcodeVVV::kMinU64)
  DEFINE_OP_3V(v_max_i8, OpcodeVVV::kMaxI8)
  DEFINE_OP_3V(v_max_i16, OpcodeVVV::kMaxI16)
  DEFINE_OP_3V(v_max_i32, OpcodeVVV::kMaxI32)
  DEFINE_OP_3V(v_max_i64, OpcodeVVV::kMaxI64)
  DEFINE_OP_3V(v_max_u8, OpcodeVVV::kMaxU8)
  DEFINE_OP_3V(v_max_u16, OpcodeVVV::kMaxU16)
  DEFINE_OP_3V(v_max_u32, OpcodeVVV::kMaxU32)
  DEFINE_OP_3V(v_max_u64, OpcodeVVV::kMaxU64)
  DEFINE_OP_3V(v_cmp_eq_i8, OpcodeVVV::kCmpEqU8)
  DEFINE_OP_3V(v_cmp_eq_u8, OpcodeVVV::kCmpEqU8)
  DEFINE_OP_3V(v_cmp_eq_i16, OpcodeVVV::kCmpEqU16)
  DEFINE_OP_3V(v_cmp_eq_u16, OpcodeVVV::kCmpEqU16)
  DEFINE_OP_3V(v_cmp_eq_i32, OpcodeVVV::kCmpEqU32)
  DEFINE_OP_3V(v_cmp_eq_u32, OpcodeVVV::kCmpEqU32)
  DEFINE_OP_3V(v_cmp_eq_i64, OpcodeVVV::kCmpEqU64)
  DEFINE_OP_3V(v_cmp_eq_u64, OpcodeVVV::kCmpEqU64)
  DEFINE_OP_3V(v_cmp_gt_i8, OpcodeVVV::kCmpGtI8)
  DEFINE_OP_3V(v_cmp_gt_u8, OpcodeVVV::kCmpGtU8)
  DEFINE_OP_3V(v_cmp_gt_i16, OpcodeVVV::kCmpGtI16)
  DEFINE_OP_3V(v_cmp_gt_u16, OpcodeVVV::kCmpGtU16)
  DEFINE_OP_3V(v_cmp_gt_i32, OpcodeVVV::kCmpGtI32)
  DEFINE_OP_3V(v_cmp_gt_u32, OpcodeVVV::kCmpGtU32)
  DEFINE_OP_3V(v_cmp_gt_i64, OpcodeVVV::kCmpGtI64)
  DEFINE_OP_3V(v_cmp_gt_u64, OpcodeVVV::kCmpGtU64)
  DEFINE_OP_3V(v_cmp_ge_i8, OpcodeVVV::kCmpGeI8)
  DEFINE_OP_3V(v_cmp_ge_u8, OpcodeVVV::kCmpGeU8)
  DEFINE_OP_3V(v_cmp_ge_i16, OpcodeVVV::kCmpGeI16)
  DEFINE_OP_3V(v_cmp_ge_u16, OpcodeVVV::kCmpGeU16)
  DEFINE_OP_3V(v_cmp_ge_i32, OpcodeVVV::kCmpGeI32)
  DEFINE_OP_3V(v_cmp_ge_u32, OpcodeVVV::kCmpGeU32)
  DEFINE_OP_3V(v_cmp_ge_i64, OpcodeVVV::kCmpGeI64)
  DEFINE_OP_3V(v_cmp_ge_u64, OpcodeVVV::kCmpGeU64)
  DEFINE_OP_3V(v_cmp_lt_i8, OpcodeVVV::kCmpLtI8)
  DEFINE_OP_3V(v_cmp_lt_u8, OpcodeVVV::kCmpLtU8)
  DEFINE_OP_3V(v_cmp_lt_i16, OpcodeVVV::kCmpLtI16)
  DEFINE_OP_3V(v_cmp_lt_u16, OpcodeVVV::kCmpLtU16)
  DEFINE_OP_3V(v_cmp_lt_i32, OpcodeVVV::kCmpLtI32)
  DEFINE_OP_3V(v_cmp_lt_u32, OpcodeVVV::kCmpLtU32)
  DEFINE_OP_3V(v_cmp_lt_i64, OpcodeVVV::kCmpLtI64)
  DEFINE_OP_3V(v_cmp_lt_u64, OpcodeVVV::kCmpLtU64)
  DEFINE_OP_3V(v_cmp_le_i8, OpcodeVVV::kCmpLeI8)
  DEFINE_OP_3V(v_cmp_le_u8, OpcodeVVV::kCmpLeU8)
  DEFINE_OP_3V(v_cmp_le_i16, OpcodeVVV::kCmpLeI16)
  DEFINE_OP_3V(v_cmp_le_u16, OpcodeVVV::kCmpLeU16)
  DEFINE_OP_3V(v_cmp_le_i32, OpcodeVVV::kCmpLeI32)
  DEFINE_OP_3V(v_cmp_le_u32, OpcodeVVV::kCmpLeU32)
  DEFINE_OP_3V(v_cmp_le_i64, OpcodeVVV::kCmpLeI64)
  DEFINE_OP_3V(v_cmp_le_u64, OpcodeVVV::kCmpLeU64)
  DEFINE_OP_3V(v_and_f32, OpcodeVVV::kAndF32)
  DEFINE_OP_3V(v_and_f64, OpcodeVVV::kAndF64)
  DEFINE_OP_3V(v_or_f32, OpcodeVVV::kOrF32)
  DEFINE_OP_3V(v_or_f64, OpcodeVVV::kOrF64)
  DEFINE_OP_3V(v_xor_f32, OpcodeVVV::kXorF32)
  DEFINE_OP_3V(v_xor_f64, OpcodeVVV::kXorF64)
  DEFINE_OP_3V(v_andn_f32, OpcodeVVV::kAndnF32)
  DEFINE_OP_3V(v_andn_f64, OpcodeVVV::kAndnF64)
  DEFINE_OP_3V(v_bic_f32, OpcodeVVV::kBicF32)
  DEFINE_OP_3V(v_bic_f64, OpcodeVVV::kBicF64)
  DEFINE_OP_3V(s_add_f32, OpcodeVVV::kAddF32S)
  DEFINE_OP_3V(s_add_f64, OpcodeVVV::kAddF64S)
  DEFINE_OP_3V(v_add_f32, OpcodeVVV::kAddF32)
  DEFINE_OP_3V(v_add_f64, OpcodeVVV::kAddF64)
  DEFINE_OP_3V(s_sub_f32, OpcodeVVV::kSubF32S)
  DEFINE_OP_3V(s_sub_f64, OpcodeVVV::kSubF64S)
  DEFINE_OP_3V(v_sub_f32, OpcodeVVV::kSubF32)
  DEFINE_OP_3V(v_sub_f64, OpcodeVVV::kSubF64)
  DEFINE_OP_3V(s_mul_f32, OpcodeVVV::kMulF32S)
  DEFINE_OP_3V(s_mul_f64, OpcodeVVV::kMulF64S)
  DEFINE_OP_3V(v_mul_f32, OpcodeVVV::kMulF32)
  DEFINE_OP_3V(v_mul_f64, OpcodeVVV::kMulF64)
  DEFINE_OP_3V(s_div_f32, OpcodeVVV::kDivF32S)
  DEFINE_OP_3V(s_div_f64, OpcodeVVV::kDivF64S)
  DEFINE_OP_3V(v_div_f32, OpcodeVVV::kDivF32)
  DEFINE_OP_3V(v_div_f64, OpcodeVVV::kDivF64)
  DEFINE_OP_3V(s_min_f32, OpcodeVVV::kMinF32S)
  DEFINE_OP_3V(s_min_f64, OpcodeVVV::kMinF64S)
  DEFINE_OP_3V(v_min_f32, OpcodeVVV::kMinF32)
  DEFINE_OP_3V(v_min_f64, OpcodeVVV::kMinF64)
  DEFINE_OP_3V(s_max_f32, OpcodeVVV::kMaxF32S)
  DEFINE_OP_3V(s_max_f64, OpcodeVVV::kMaxF64S)
  DEFINE_OP_3V(v_max_f32, OpcodeVVV::kMaxF32)
  DEFINE_OP_3V(v_max_f64, OpcodeVVV::kMaxF64)
  DEFINE_OP_3V(s_cmp_eq_f32, OpcodeVVV::kCmpEqF32S)
  DEFINE_OP_3V(s_cmp_eq_f64, OpcodeVVV::kCmpEqF64S)
  DEFINE_OP_3V(v_cmp_eq_f32, OpcodeVVV::kCmpEqF32)
  DEFINE_OP_3V(v_cmp_eq_f64, OpcodeVVV::kCmpEqF64)
  DEFINE_OP_3V(s_cmp_ne_f32, OpcodeVVV::kCmpNeF32S)
  DEFINE_OP_3V(s_cmp_ne_f64, OpcodeVVV::kCmpNeF64S)
  DEFINE_OP_3V(v_cmp_ne_f32, OpcodeVVV::kCmpNeF32)
  DEFINE_OP_3V(v_cmp_ne_f64, OpcodeVVV::kCmpNeF64)
  DEFINE_OP_3V(s_cmp_gt_f32, OpcodeVVV::kCmpGtF32S)
  DEFINE_OP_3V(s_cmp_gt_f64, OpcodeVVV::kCmpGtF64S)
  DEFINE_OP_3V(v_cmp_gt_f32, OpcodeVVV::kCmpGtF32)
  DEFINE_OP_3V(v_cmp_gt_f64, OpcodeVVV::kCmpGtF64)
  DEFINE_OP_3V(s_cmp_ge_f32, OpcodeVVV::kCmpGeF32S)
  DEFINE_OP_3V(s_cmp_ge_f64, OpcodeVVV::kCmpGeF64S)
  DEFINE_OP_3V(v_cmp_ge_f32, OpcodeVVV::kCmpGeF32)
  DEFINE_OP_3V(v_cmp_ge_f64, OpcodeVVV::kCmpGeF64)
  DEFINE_OP_3V(s_cmp_lt_f32, OpcodeVVV::kCmpLtF32S)
  DEFINE_OP_3V(s_cmp_lt_f64, OpcodeVVV::kCmpLtF64S)
  DEFINE_OP_3V(v_cmp_lt_f32, OpcodeVVV::kCmpLtF32)
  DEFINE_OP_3V(v_cmp_lt_f64, OpcodeVVV::kCmpLtF64)
  DEFINE_OP_3V(s_cmp_le_f32, OpcodeVVV::kCmpLeF32S)
  DEFINE_OP_3V(s_cmp_le_f64, OpcodeVVV::kCmpLeF64S)
  DEFINE_OP_3V(v_cmp_le_f32, OpcodeVVV::kCmpLeF32)
  DEFINE_OP_3V(v_cmp_le_f64, OpcodeVVV::kCmpLeF64)
  DEFINE_OP_3V(s_cmp_ord_f32, OpcodeVVV::kCmpOrdF32S)
  DEFINE_OP_3V(s_cmp_ord_f64, OpcodeVVV::kCmpOrdF64S)
  DEFINE_OP_3V(v_cmp_ord_f32, OpcodeVVV::kCmpOrdF32)
  DEFINE_OP_3V(v_cmp_ord_f64, OpcodeVVV::kCmpOrdF64)
  DEFINE_OP_3V(s_cmp_unord_f32, OpcodeVVV::kCmpUnordF32S)
  DEFINE_OP_3V(s_cmp_unord_f64, OpcodeVVV::kCmpUnordF64S)
  DEFINE_OP_3V(v_cmp_unord_f32, OpcodeVVV::kCmpUnordF32)
  DEFINE_OP_3V(v_cmp_unord_f64, OpcodeVVV::kCmpUnordF64)
  DEFINE_OP_3V(v_hadd_f64, OpcodeVVV::kHAddF64);
  DEFINE_OP_3V(v_combine_lo_hi_u64, OpcodeVVV::kCombineLoHiU64)
  DEFINE_OP_3V(v_combine_lo_hi_f64, OpcodeVVV::kCombineLoHiF64)
  DEFINE_OP_3V(v_combine_hi_lo_u64, OpcodeVVV::kCombineHiLoU64)
  DEFINE_OP_3V(v_combine_hi_lo_f64, OpcodeVVV::kCombineHiLoF64)
  DEFINE_OP_3V(v_interleave_lo_u8, OpcodeVVV::kInterleaveLoU8)
  DEFINE_OP_3V(v_interleave_hi_u8, OpcodeVVV::kInterleaveHiU8)
  DEFINE_OP_3V(v_interleave_lo_u16, OpcodeVVV::kInterleaveLoU16)
  DEFINE_OP_3V(v_interleave_hi_u16, OpcodeVVV::kInterleaveHiU16)
  DEFINE_OP_3V(v_interleave_lo_u32, OpcodeVVV::kInterleaveLoU32)
  DEFINE_OP_3V(v_interleave_hi_u32, OpcodeVVV::kInterleaveHiU32)
  DEFINE_OP_3V(v_interleave_lo_u64, OpcodeVVV::kInterleaveLoU64)
  DEFINE_OP_3V(v_interleave_hi_u64, OpcodeVVV::kInterleaveHiU64)
  DEFINE_OP_3V(v_interleave_lo_f32, OpcodeVVV::kInterleaveLoF32)
  DEFINE_OP_3V(v_interleave_hi_f32, OpcodeVVV::kInterleaveHiF32)
  DEFINE_OP_3V(v_interleave_lo_f64, OpcodeVVV::kInterleaveLoF64)
  DEFINE_OP_3V(v_interleave_hi_f64, OpcodeVVV::kInterleaveHiF64)
  DEFINE_OP_3V(v_packs_i16_i8, OpcodeVVV::kPacksI16_I8)
  DEFINE_OP_3V(v_packs_i16_u8, OpcodeVVV::kPacksI16_U8)
  DEFINE_OP_3V(v_packs_i32_i16, OpcodeVVV::kPacksI32_I16)
  DEFINE_OP_3V(v_packs_i32_u16, OpcodeVVV::kPacksI32_U16)
  DEFINE_OP_3V(v_swizzlev_u8, OpcodeVVV::kSwizzlev_U8)

#if defined(BL_JIT_ARCH_A64)
  DEFINE_OP_3V(v_mulw_lo_i8, OpcodeVVV::kMulwLoI8)
  DEFINE_OP_3V(v_mulw_lo_u8, OpcodeVVV::kMulwLoU8)
  DEFINE_OP_3V(v_mulw_hi_i8, OpcodeVVV::kMulwHiI8)
  DEFINE_OP_3V(v_mulw_hi_u8, OpcodeVVV::kMulwHiU8)
  DEFINE_OP_3V(v_mulw_lo_i16, OpcodeVVV::kMulwLoI16)
  DEFINE_OP_3V(v_mulw_lo_u16, OpcodeVVV::kMulwLoU16)
  DEFINE_OP_3V(v_mulw_hi_i16, OpcodeVVV::kMulwHiI16)
  DEFINE_OP_3V(v_mulw_hi_u16, OpcodeVVV::kMulwHiU16)
  DEFINE_OP_3V(v_mulw_lo_i32, OpcodeVVV::kMulwLoI32)
  DEFINE_OP_3V(v_mulw_lo_u32, OpcodeVVV::kMulwLoU32)
  DEFINE_OP_3V(v_mulw_hi_i32, OpcodeVVV::kMulwHiI32)
  DEFINE_OP_3V(v_mulw_hi_u32, OpcodeVVV::kMulwHiU32)
  DEFINE_OP_3V(v_maddw_lo_i8, OpcodeVVV::kMAddwLoI8)
  DEFINE_OP_3V(v_maddw_lo_u8, OpcodeVVV::kMAddwLoU8)
  DEFINE_OP_3V(v_maddw_hi_i8, OpcodeVVV::kMAddwHiI8)
  DEFINE_OP_3V(v_maddw_hi_u8, OpcodeVVV::kMAddwHiU8)
  DEFINE_OP_3V(v_maddw_lo_i16, OpcodeVVV::kMAddwLoI16)
  DEFINE_OP_3V(v_maddw_lo_u16, OpcodeVVV::kMAddwLoU16)
  DEFINE_OP_3V(v_maddw_hi_i16, OpcodeVVV::kMAddwHiI16)
  DEFINE_OP_3V(v_maddw_hi_u16, OpcodeVVV::kMAddwHiU16)
  DEFINE_OP_3V(v_maddw_lo_i32, OpcodeVVV::kMAddwLoI32)
  DEFINE_OP_3V(v_maddw_lo_u32, OpcodeVVV::kMAddwLoU32)
  DEFINE_OP_3V(v_maddw_hi_i32, OpcodeVVV::kMAddwHiI32)
  DEFINE_OP_3V(v_maddw_hi_u32, OpcodeVVV::kMAddwHiU32)
#endif // BL_JIT_ARCH_A64

#if defined(BL_JIT_ARCH_X86)
  DEFINE_OP_3V(v_permute_u8, OpcodeVVV::kPermuteU8)
  DEFINE_OP_3V(v_permute_u16, OpcodeVVV::kPermuteU16)
  DEFINE_OP_3V(v_permute_u32, OpcodeVVV::kPermuteU32)
  DEFINE_OP_3V(v_permute_u64, OpcodeVVV::kPermuteU64)
#endif // BL_JIT_ARCH_X86

  DEFINE_OP_3VI(v_alignr_u128, OpcodeVVVI::kAlignr_U128)
  DEFINE_OP_3VI_WRAP(v_interleave_shuffle_u32x4, Swizzle4, OpcodeVVVI::kInterleaveShuffleU32x4)
  DEFINE_OP_3VI_WRAP(v_interleave_shuffle_u64x2, Swizzle2, OpcodeVVVI::kInterleaveShuffleU64x2)
  DEFINE_OP_3VI_WRAP(v_interleave_shuffle_f32x4, Swizzle4, OpcodeVVVI::kInterleaveShuffleF32x4)
  DEFINE_OP_3VI_WRAP(v_interleave_shuffle_f64x2, Swizzle2, OpcodeVVVI::kInterleaveShuffleF64x2)
  DEFINE_OP_3VI(v_insert_v128, OpcodeVVVI::kInsertV128_U32)
  DEFINE_OP_3VI(v_insert_v128_u32, OpcodeVVVI::kInsertV128_U32)
  DEFINE_OP_3VI(v_insert_v128_f32, OpcodeVVVI::kInsertV128_F32)
  DEFINE_OP_3VI(v_insert_v128_u64, OpcodeVVVI::kInsertV128_U64)
  DEFINE_OP_3VI(v_insert_v128_f64, OpcodeVVVI::kInsertV128_F64)
  DEFINE_OP_3VI(v_insert_v256, OpcodeVVVI::kInsertV256_U32)
  DEFINE_OP_3VI(v_insert_v256_u32, OpcodeVVVI::kInsertV256_U32)
  DEFINE_OP_3VI(v_insert_v256_f32, OpcodeVVVI::kInsertV256_F32)
  DEFINE_OP_3VI(v_insert_v256_u64, OpcodeVVVI::kInsertV256_U64)
  DEFINE_OP_3VI(v_insert_v256_f64, OpcodeVVVI::kInsertV256_F64)


  DEFINE_OP_4V(v_blendv_u8, OpcodeVVVV::kBlendV_U8)
  DEFINE_OP_4V(v_madd_i16, OpcodeVVVV::kMAddU16)
  DEFINE_OP_4V(v_madd_u16, OpcodeVVVV::kMAddU16)
  DEFINE_OP_4V(v_madd_i32, OpcodeVVVV::kMAddU32)
  DEFINE_OP_4V(v_madd_u32, OpcodeVVVV::kMAddU32)
  DEFINE_OP_4V(s_madd_f32, OpcodeVVVV::kMAddF32S)
  DEFINE_OP_4V(s_madd_f64, OpcodeVVVV::kMAddF64S)
  DEFINE_OP_4V(v_madd_f32, OpcodeVVVV::kMAddF32)
  DEFINE_OP_4V(v_madd_f64, OpcodeVVVV::kMAddF64)
  DEFINE_OP_4V(s_msub_f32, OpcodeVVVV::kMSubF32S)
  DEFINE_OP_4V(s_msub_f64, OpcodeVVVV::kMSubF64S)
  DEFINE_OP_4V(v_msub_f32, OpcodeVVVV::kMSubF32)
  DEFINE_OP_4V(v_msub_f64, OpcodeVVVV::kMSubF64)
  DEFINE_OP_4V(s_nmadd_f32, OpcodeVVVV::kNMAddF32S)
  DEFINE_OP_4V(s_nmadd_f64, OpcodeVVVV::kNMAddF64S)
  DEFINE_OP_4V(v_nmadd_f32, OpcodeVVVV::kNMAddF32)
  DEFINE_OP_4V(v_nmadd_f64, OpcodeVVVV::kNMAddF64)
  DEFINE_OP_4V(s_nmsub_f32, OpcodeVVVV::kNMSubF32S)
  DEFINE_OP_4V(s_nmsub_f64, OpcodeVVVV::kNMSubF64S)
  DEFINE_OP_4V(v_nmsub_f32, OpcodeVVVV::kNMSubF32)
  DEFINE_OP_4V(v_nmsub_f64, OpcodeVVVV::kNMSubF64)

  #undef DEFINE_OP_4V
  #undef DEFINE_OP_3VI_WRAP
  #undef DEFINE_OP_3VI
  #undef DEFINE_OP_3V
  #undef DEFINE_OP_MV_A
  #undef DEFINE_OP_MV_U
  #undef DEFINE_OP_MV_I
  #undef DEFINE_OP_VM_A
  #undef DEFINE_OP_VM_U
  #undef DEFINE_OP_VM_I
  #undef DEFINE_OP_2VI_WRAP
  #undef DEFINE_OP_2VI
  #undef DEFINE_OP_2V

  template<typename DstT, typename SrcT> BL_INLINE void v_swap_u32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u32x4(dst, src, swizzle(2, 3, 0, 1)); }
  template<typename DstT, typename SrcT> BL_INLINE void v_swap_u64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u64x2(dst, src, swizzle(0, 1)); }
  template<typename DstT, typename SrcT> BL_INLINE void v_swap_f32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_f32x4(dst, src, swizzle(2, 3, 0, 1)); }
  template<typename DstT, typename SrcT> BL_INLINE void v_swap_f64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_f64x2(dst, src, swizzle(0, 1)); }

  template<typename DstT, typename SrcT> BL_INLINE void v_dup_lo_u32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u32x4(dst, src, swizzle(2, 2, 0, 0)); }
  template<typename DstT, typename SrcT> BL_INLINE void v_dup_hi_u32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u32x4(dst, src, swizzle(3, 3, 1, 1)); }
  template<typename DstT, typename SrcT> BL_INLINE void v_dup_lo_u64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u64x2(dst, src, swizzle(0, 0)); }
  template<typename DstT, typename SrcT> BL_INLINE void v_dup_hi_u64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u64x2(dst, src, swizzle(1, 1)); }
  template<typename DstT, typename SrcT> BL_INLINE void v_dup_lo_f64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_f64x2(dst, src, swizzle(0, 0)); }
  template<typename DstT, typename SrcT> BL_INLINE void v_dup_hi_f64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_f64x2(dst, src, swizzle(1, 1)); }

  template<typename T> BL_INLINE void v_zero_i(const T& dst) noexcept { v_xor_i32(dst, dst, dst); }
  template<typename T> BL_INLINE void v_zero_f(const T& dst) noexcept { v_xor_f32(dst, dst, dst); }
  template<typename T> BL_INLINE void v_zero_d(const T& dst) noexcept { v_xor_f64(dst, dst, dst); }
  template<typename T> BL_INLINE void v_ones_i(const T& dst) noexcept { v_cmp_eq_u8(dst, dst, dst); }

  //! \}

  //! \name Memory Loads & Stores
  //! \{

  BL_NOINLINE void v_load_u8_u16_2x(const Vec& dst, const Mem& lo, const Mem& hi) noexcept {
#if defined(BL_JIT_ARCH_X86)
    Gp reg = newGp32("@tmp");
    Mem mLo(lo);
    Mem mHi(hi);

    mLo.setSize(1);
    mHi.setSize(1);

    load_u8(reg, mHi);
    shl(reg, reg, 16);
    cc->mov(reg.r8(), mLo);
    s_mov_u32(dst.xmm(), reg);
#elif defined(BL_JIT_ARCH_A64)
    Gp tmp_a = newGp32("@tmp_a");
    Gp tmp_b = newGp32("@tmp_b");

    load_u8(tmp_a, lo);
    load_u8(tmp_b, hi);
    cc->orr(tmp_a, tmp_a, tmp_b, a64::lsl(16));
    s_mov_u32(dst, tmp_a);
#endif
  }

  //! \}

  //! \name Memory Loads & Stores with Parameterized Size
  //! \{

  BL_NOINLINE void v_load_iany(const Vec& dst, const Mem& src, uint32_t nBytes, Alignment alignment) noexcept {
    switch (nBytes) {
      case 1: v_load8(dst, src); break;
      case 2: v_loada16(dst, src, alignment); break;
      case 4: v_loada32(dst, src, alignment); break;
      case 8: v_loada64(dst, src, alignment); break;
      case 16: v_loada128(dst, src, alignment); break;
      case 32: v_loada256(dst, src, alignment); break;
      case 64: v_loada512(dst, src, alignment); break;

      default:
        BL_NOT_REACHED();
    }
  }

  BL_NOINLINE void v_store_iany(const Mem& dst, const Vec& src, uint32_t nBytes, Alignment alignment) noexcept {
    switch (nBytes) {
      case 1: v_store8(dst, src); break;
      case 2: v_storea16(dst, src, alignment); break;
      case 4: v_storea32(dst, src, alignment); break;
      case 8: v_storea64(dst, src, alignment); break;
      case 16: v_storea128(dst, src, alignment); break;
      case 32: v_storea256(dst, src, alignment); break;
      case 64: v_storea512(dst, src, alignment); break;

      default:
        BL_NOT_REACHED();
    }
  }

  //! \}

  //! \name Utilities
  //! \{

  template<typename Dst, typename Src>
  BL_INLINE void shiftOrRotateLeft(const Dst& dst, const Src& src, uint32_t n) noexcept {
  #if defined(BL_JIT_ARCH_X86)
    if ((n & 3) == 0)
      v_alignr_u128(dst, src, src, (16u - n) & 15);
    else
      v_sllb_u128(dst, src, n);
  #else
    // This doesn't rely on a zero constant on AArch64, which is okay as we don't care what's shifted in.
    v_alignr_u128(dst, src, src, (16u - n) & 15);
  #endif
  }

  template<typename Dst, typename Src>
  BL_INLINE void shiftOrRotateRight(const Dst& dst, const Src& src, uint32_t n) noexcept {
  #if defined(BL_JIT_ARCH_X86)
    if ((n & 3) == 0)
      v_alignr_u128(dst, src, src, n);
    else
      v_srlb_u128(dst, src, n);
  #else
    // This doesn't rely on a zero constant on AArch64, which is okay as we don't care what's shifted in.
    v_alignr_u128(dst, src, src, n);
  #endif
  }

  template<typename DstT, typename SrcT>
  inline void v_inv255_u16(const DstT& dst, const SrcT& src) noexcept {
    Operand u16_255 = simdConst(&ct.i_00FF00FF00FF00FF, Bcst::k32, dst);
    v_xor_i32(dst, src, u16_255);
  }

  template<typename DstT, typename SrcT>
  BL_NOINLINE void v_mul257_hi_u16(const DstT& dst, const SrcT& src) {
#if defined(BL_JIT_ARCH_X86)
    v_mulh_u16(dst, src, simdConst(&commonTable.i_0101010101010101, Bcst::kNA, dst));
#elif defined(BL_JIT_ARCH_A64)
    v_srli_acc_u16(dst, src, 8);
    v_srli_u16(dst, dst, 8);
#endif
  }

  // TODO: [JIT] Consolidate this to only one implementation.
  template<typename DstSrcT>
  BL_NOINLINE void v_div255_u16(const DstSrcT& x) {
#if defined(BL_JIT_ARCH_X86)
    Operand i_0080008000800080 = simdConst(&commonTable.i_0080008000800080, Bcst::kNA, x);

    v_add_i16(x, x, i_0080008000800080);
    v_mul257_hi_u16(x, x);
#elif defined(BL_JIT_ARCH_A64)
    v_srli_rnd_acc_u16(x, x, 8);
    v_srli_rnd_u16(x, x, 8);
#endif
  }

  template<typename DstSrcT>
  BL_NOINLINE void v_div255_u16_2x(const DstSrcT& v0, const DstSrcT& v1) noexcept {
#if defined(BL_JIT_ARCH_X86)
    Operand i_0080008000800080 = simdConst(&commonTable.i_0080008000800080, Bcst::kNA, v0);
    Operand i_0101010101010101 = simdConst(&commonTable.i_0101010101010101, Bcst::kNA, v0);

    v_add_i16(v0, v0, i_0080008000800080);
    v_add_i16(v1, v1, i_0080008000800080);

    v_mulh_u16(v0, v0, i_0101010101010101);
    v_mulh_u16(v1, v1, i_0101010101010101);
#elif defined(BL_JIT_ARCH_A64)
    v_srli_rnd_acc_u16(v0, v0, 8);
    v_srli_rnd_acc_u16(v1, v1, 8);
    v_srli_rnd_u16(v0, v0, 8);
    v_srli_rnd_u16(v1, v1, 8);
#endif
  }

  // d = int(floor(a / b) * b).
  template<typename VecOrMem>
  BL_NOINLINE void v_mod_pd(const Vec& d, const Vec& a, const VecOrMem& b) noexcept {
#if defined(BL_JIT_ARCH_X86)
    if (!hasSSE4_1()) {
      Vec t = newV128("vModTmp");

      v_div_f64(d, a, b);
      v_cvt_trunc_f64_to_i32_lo(t, d);
      v_cvt_i32_lo_to_f64(d, t);
      v_mul_f64(d, d, b);
    }
    else
#endif // BL_JIT_ARCH_X86
    {
      v_div_f64(d, a, b);
      v_trunc_f64(d, d);
      v_mul_f64(d, d, b);
    }
  }

  //! \}

#if defined(BL_JIT_ARCH_X86)
  //! \name Memory Loads & Stores with Predicate
  //! \{

  KReg makeMaskPredicate(PixelPredicate& predicate, uint32_t lastN) noexcept;
  KReg makeMaskPredicate(PixelPredicate& predicate, uint32_t lastN, const Gp& adjustedCount) noexcept;

  Vec makeVecPredicate32(PixelPredicate& predicate, uint32_t lastN) noexcept;
  Vec makeVecPredicate32(PixelPredicate& predicate, uint32_t lastN, const Gp& adjustedCount) noexcept;

  BL_NOINLINE void v_load_predicated_u8(const Vec& dst, const Mem& src, uint32_t n, PixelPredicate& predicate) noexcept{
    if (hasAVX512()) {
      KReg kPred = makeMaskPredicate(predicate, n);
      cc->k(kPred).z().vmovdqu8(dst, src);
    }
    else {
      BL_NOT_REACHED();
    }
  }

  BL_NOINLINE void v_store_predicated_u8(const Mem& dst, const Vec& src, uint32_t n, PixelPredicate& predicate) noexcept{
    if (hasAVX512()) {
      KReg kPred = makeMaskPredicate(predicate, n);
      cc->k(kPred).vmovdqu8(dst, src);
    }
    else {
      BL_NOT_REACHED();
    }
  }

  BL_NOINLINE void v_load_predicated_u32(const Vec& dst, const Mem& src, uint32_t n, PixelPredicate& predicate) noexcept{
    if (hasAVX512()) {
      KReg kPred = makeMaskPredicate(predicate, n);
      cc->k(kPred).z().vmovdqu32(dst, src);
    }
    else if (hasAVX()) {
      Vec vPred = makeVecPredicate32(predicate, n);
      InstId instId = hasAVX2() ? x86::Inst::kIdVpmaskmovd : x86::Inst::kIdVmaskmovps;
      cc->emit(instId, dst, vPred, src);
    }
    else {
      BL_NOT_REACHED();
    }
  }

  BL_NOINLINE void v_store_predicated_u32(const Mem& dst, const Vec& src, uint32_t n, PixelPredicate& predicate) noexcept{
    if (hasAVX512()) {
      KReg kPred = makeMaskPredicate(predicate, n);
      cc->k(kPred).vmovdqu32(dst, src);
    }
    else if (hasAVX()) {
      Vec vPred = makeVecPredicate32(predicate, n);
      InstId instId = hasAVX2() ? x86::Inst::kIdVpmaskmovd : x86::Inst::kIdVmaskmovps;
      cc->emit(instId, dst, vPred, src);
    }
    else {
      BL_NOT_REACHED();
    }
  }

  //! \}

#endif // BL_JIT_ARCH_X86

  //! \name Emit - 'X' High Level Functionality
  //! \{

  // Kind of a hack - if we don't have SSE4.1 we have to load the byte into GP register first and then we use 'PINSRW',
  // which is provided by baseline SSE2. If we have SSE4.1 then it's much easier as we can load the byte by 'PINSRB'.
  void x_insert_word_or_byte(const Vec& dst, const Mem& src, uint32_t wordIndex) noexcept {
#if defined(BL_JIT_ARCH_X86)
    if (hasSSE4_1()) {
      Mem m = src;
      m.setSize(1);
      v_insert_u8(dst, m, wordIndex * 2u);
    }
    else {
      Gp tmp = newGp32("@tmp");
      load_u8(tmp, src);
      s_insert_u16(dst, tmp, wordIndex);
    }
#else
    v_insert_u8(dst, src, wordIndex * 2);
#endif
  }

  //! \}

  //! \name Emit - Pixel Processing Utilities
  //! \{

  //! Pack 16-bit integers to unsigned 8-bit integers in an AVX2 and AVX512 aware way.
  template<typename Dst, typename Src1, typename Src2>
  BL_NOINLINE void x_packs_i16_u8(const Dst& d, const Src1& s1, const Src2& s2) noexcept {
#if defined(BL_JIT_ARCH_X86)
    if (s1.isVec128()) {
      v_packs_i16_u8(d, s1, s2);
    }
    else {
      const Vec& vType = JitUtils::firstOp(s1).template as<Vec>();
      v_packs_i16_u8(d, s1, s2);
      v_swizzle_u64x4(d.cloneAs(vType), d.cloneAs(vType), swizzle(3, 1, 2, 0));
    }
#else
    v_packs_i16_u8(d, s1, s2);
#endif
  }

  BL_NOINLINE void xStorePixel(const Gp& dPtr, const Vec& vSrc, uint32_t count, uint32_t bpp, Alignment alignment) noexcept {
    v_store_iany(mem_ptr(dPtr), vSrc, count * bpp, alignment);
  }

  inline void xStore32_ARGB(const Mem& dst, const Vec& vSrc) noexcept {
    v_storea32(dst, vSrc);
  }

  BL_NOINLINE void xMovzxBW_LoHi(const Vec& d0, const Vec& d1, const Vec& s) noexcept {
    BL_ASSERT(d0.id() != d1.id());

#if defined(BL_JIT_ARCH_X86)
    if (hasSSE4_1()) {
      if (d0.id() == s.id()) {
        v_swizzle_u32x4(d1, d0, swizzle(1, 0, 3, 2));
        v_cvt_u8_lo_to_u16(d0, d0);
        v_cvt_u8_lo_to_u16(d1, d1);
      }
      else {
        v_cvt_u8_lo_to_u16(d0, s);
        v_swizzle_u32x4(d1, s, swizzle(1, 0, 3, 2));
        v_cvt_u8_lo_to_u16(d1, d1);
      }
    }
    else {
      Vec zero = simdVecConst(&commonTable.i_0000000000000000, Bcst::k32, s);
      if (d1.id() != s.id()) {
        v_interleave_hi_u8(d1, s, zero);
        v_interleave_lo_u8(d0, s, zero);
      }
      else {
        v_interleave_lo_u8(d0, s, zero);
        v_interleave_hi_u8(d1, s, zero);
      }
    }
#elif defined(BL_JIT_ARCH_A64)
    if (d0.id() == s.id()) {
      cc->sshll2(d1, s, 0);
      cc->sshll(d0, s, 0);
    }
    else {
      cc->sshll(d0, s, 0);
      cc->sshll2(d1, s, 0);
    }
#endif
  }

  template<typename Dst, typename Src>
  inline void vExpandAlphaLo16(const Dst& d, const Src& s) noexcept { v_swizzle_lo_u16x4(d, s, swizzle(3, 3, 3, 3)); }

  template<typename Dst, typename Src>
  inline void vExpandAlphaHi16(const Dst& d, const Src& s) noexcept { v_swizzle_hi_u16x4(d, s, swizzle(3, 3, 3, 3)); }

  template<typename Dst, typename Src>
  inline void v_expand_alpha_16(const Dst& d, const Src& s, uint32_t useHiPart = 1) noexcept {
#if defined(BL_JIT_ARCH_X86)
    if (useHiPart) {
      if (hasAVX() || (hasSSSE3() && d == s)) {
        v_swizzlev_u8(d, s, simdConst(&commonTable.swizu8_32xxxxxx10xxxxxx_to_3232323210101010, Bcst::kNA, d));
      }
      else {
        vExpandAlphaHi16(d, s);
        vExpandAlphaLo16(d, d);
      }
    }
    else {
      vExpandAlphaLo16(d, s);
    }
#elif defined(BL_JIT_ARCH_A64)
    blUnused(useHiPart);
    v_swizzle_u16x4(d, s, swizzle(3, 3, 3, 3));
#endif
  }

  template<typename Dst, typename Src>
  inline void vExpandAlphaPS(const Dst& d, const Src& s) noexcept { v_swizzle_u32x4(d, s, swizzle(3, 3, 3, 3)); }

  template<typename DstT, typename SrcT>
  inline void vFillAlpha255B(const DstT& dst, const SrcT& src) noexcept { v_or_i32(dst, src, simdConst(&commonTable.i_FF000000FF000000, Bcst::k32, dst)); }
  template<typename DstT, typename SrcT>
  inline void vFillAlpha255W(const DstT& dst, const SrcT& src) noexcept { v_or_i64(dst, src, simdConst(&commonTable.i_00FF000000000000, Bcst::k64, dst)); }

  template<typename DstT, typename SrcT>
  inline void vZeroAlphaB(const DstT& dst, const SrcT& src) noexcept { v_and_i32(dst, src, simdMemConst(&commonTable.i_00FFFFFF00FFFFFF, Bcst::k32, dst)); }

  template<typename DstT, typename SrcT>
  inline void vZeroAlphaW(const DstT& dst, const SrcT& src) noexcept { v_and_i64(dst, src, simdMemConst(&commonTable.i_0000FFFFFFFFFFFF, Bcst::k64, dst)); }

  template<typename DstT, typename SrcT>
  inline void vNegAlpha8B(const DstT& dst, const SrcT& src) noexcept { v_xor_i32(dst, src, simdConst(&commonTable.i_FF000000FF000000, Bcst::k32, dst)); }
  template<typename DstT, typename SrcT>
  inline void vNegAlpha8W(const DstT& dst, const SrcT& src) noexcept { v_xor_i64(dst, src, simdConst(&commonTable.i_00FF000000000000, Bcst::k64, dst)); }

  template<typename DstT, typename SrcT>
  inline void vNegRgb8B(const DstT& dst, const SrcT& src) noexcept { v_xor_i32(dst, src, simdConst(&commonTable.i_00FFFFFF00FFFFFF, Bcst::k32, dst)); }
  template<typename DstT, typename SrcT>
  inline void vNegRgb8W(const DstT& dst, const SrcT& src) noexcept { v_xor_i64(dst, src, simdConst(&commonTable.i_000000FF00FF00FF, Bcst::k64, dst)); }

  // Performs 32-bit unsigned modulo of 32-bit `a` (hi DWORD) with 32-bit `b` (lo DWORD).
  template<typename VecOrMem_A, typename VecOrMem_B>
  BL_NOINLINE void xModI64HIxU64LO(const Vec& d, const VecOrMem_A& a, const VecOrMem_B& b) noexcept {
    Vec t0 = newV128("t0");
    Vec t1 = newV128("t1");

    v_swizzle_u32x4(t1, b, swizzle(3, 3, 2, 0));
    v_swizzle_u32x4(d , a, swizzle(2, 0, 3, 1));

    v_cvt_i32_lo_to_f64(t1, t1);
    v_cvt_i32_lo_to_f64(t0, d);
    v_mod_pd(t0, t0, t1);
    v_cvt_trunc_f64_to_i32_lo(t0, t0);

    v_sub_i32(d, d, t0);
    v_swizzle_u32x4(d, d, swizzle(1, 3, 0, 2));
  }

  // Performs 32-bit unsigned modulo of 32-bit `a` (hi DWORD) with 64-bit `b` (DOUBLE).
  template<typename VecOrMem_A, typename VecOrMem_B>
  BL_NOINLINE void xModI64HIxDouble(const Vec& d, const VecOrMem_A& a, const VecOrMem_B& b) noexcept {
    Vec t0 = newV128("t0");

    v_swizzle_u32x4(d, a, swizzle(2, 0, 3, 1));
    v_cvt_i32_lo_to_f64(t0, d);
    v_mod_pd(t0, t0, b);
    v_cvt_trunc_f64_to_i32_lo(t0, t0);

    v_sub_i32(d, d, t0);
    v_swizzle_u32x4(d, d, swizzle(1, 3, 0, 2));
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_1(const Vec& d, const Vec& s) noexcept {
    v_swizzle_lo_u16x4(d, s, swizzle(1, 1, 1, 1));
    v_srli_u16(d, d, 8);
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_2(const Vec& d, const Vec& s) noexcept {
#if defined(BL_JIT_ARCH_X86)
    if (!hasSSSE3()) {
      v_swizzle_lo_u16x4(d, s, swizzle(3, 3, 1, 1));
      v_swizzle_u32x4(d, d, swizzle(1, 1, 0, 0));
      v_srli_u16(d, d, 8);
      return;
    }
#endif

    v_swizzlev_u8(d, s, simdConst(&commonTable.swizu8_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d));
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_4(const Vec& d0, const Vec& d1, const Vec& s) noexcept {
    BL_ASSERT(d0.id() != d1.id());

#if defined(BL_JIT_ARCH_X86)
    if (!hasSSSE3()) {
      if (d1.id() != s.id()) {
        v_swizzle_hi_u16x4(d1, s, swizzle(3, 3, 1, 1));
        v_swizzle_lo_u16x4(d0, s, swizzle(3, 3, 1, 1));

        v_swizzle_u32x4(d1, d1, swizzle(3, 3, 2, 2));
        v_swizzle_u32x4(d0, d0, swizzle(1, 1, 0, 0));

        v_srli_u16(d1, d1, 8);
        v_srli_u16(d0, d0, 8);
      }
      else {
        v_swizzle_lo_u16x4(d0, s, swizzle(3, 3, 1, 1));
        v_swizzle_hi_u16x4(d1, s, swizzle(3, 3, 1, 1));

        v_swizzle_u32x4(d0, d0, swizzle(1, 1, 0, 0));
        v_swizzle_u32x4(d1, d1, swizzle(3, 3, 2, 2));

        v_srli_u16(d0, d0, 8);
        v_srli_u16(d1, d1, 8);
      }
      return;
    }
#endif

    if (d0.id() == s.id()) {
      v_swizzlev_u8(d1, s, simdConst(&ct.swizu8_1xxx0xxxxxxxxxxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d1));
      v_swizzlev_u8(d0, s, simdConst(&ct.swizu8_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d0));
    }
    else {
      v_swizzlev_u8(d0, s, simdConst(&ct.swizu8_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d0));
      v_swizzlev_u8(d1, s, simdConst(&ct.swizu8_1xxx0xxxxxxxxxxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d1));
    }
  }

  BL_NOINLINE void xPackU32ToU16Lo(const Vec& d0, const Vec& s0) noexcept {
#if defined(BL_JIT_ARCH_X86)
    if (hasSSE4_1()) {
      v_packs_i32_u16(d0, s0, s0);
    }
    else if (hasSSSE3()) {
      v_swizzlev_u8(d0, s0, simdConst(&commonTable.swizu8_xx76xx54xx32xx10_to_7654321076543210, Bcst::kNA, d0));
    }
    else {
      // Sign extend and then use `packssdw()`.
      v_slli_i32(d0, s0, 16);
      v_srai_i32(d0, d0, 16);
      v_packs_i32_i16(d0, d0, d0);
    }
#elif defined(BL_JIT_ARCH_A64)
    cc->sqxtun(d0.h4(), s0.s4());
#endif
  }

  BL_NOINLINE void xPackU32ToU16Lo(const VecArray& d0, const VecArray& s0) noexcept {
    for (uint32_t i = 0; i < d0.size(); i++)
      xPackU32ToU16Lo(d0[i], s0[i]);
  }
};

class PipeInjectAtTheEnd {
public:
  ScopedInjector _injector;

  BL_INLINE PipeInjectAtTheEnd(PipeCompiler* pc) noexcept
    : _injector(pc->cc, &pc->_funcEnd) {}
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPECOMPILER_P_H_INCLUDED
