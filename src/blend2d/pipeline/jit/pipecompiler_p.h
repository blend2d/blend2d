// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPECOMPILER_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPECOMPILER_P_H_INCLUDED

#include "../../runtime_p.h"
#include "../../pipeline/jit/jitbase_p.h"
#include "../../pipeline/jit/pipegencore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {

//! Condition represents either a condition or an assignment operation that can be checked.
class Condition {
public:
  enum class Op : uint8_t {
    kAssignAnd,
    kAssignOr,
    kAssignXor,
    kAssignAdd,
    kAssignSub,
    kAssignSHR,
    kTest,
    kBitTest,
    kCompare,
    kMaxValue = kCompare
  };

  Op op;
  CondCode cond;
  Operand a;
  Operand b;

  BL_INLINE_NODEBUG Condition(Op op, CondCode cond, const Operand& a, const Operand& b) noexcept
    : op(op),
      cond(cond),
      a(a),
      b(b) {}

  BL_INLINE_NODEBUG Condition(const Condition& other) noexcept = default;
  BL_INLINE_NODEBUG Condition& operator=(const Condition& other) noexcept = default;
};

static BL_INLINE Condition and_z(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kZ, a, b); }
static BL_INLINE Condition and_z(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kZ, a, b); }
static BL_INLINE Condition and_z(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kZ, a, b); }
static BL_INLINE Condition and_nz(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kNZ, a, b); }
static BL_INLINE Condition and_nz(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kNZ, a, b); }
static BL_INLINE Condition and_nz(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kNZ, a, b); }
static BL_INLINE Condition and_c(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kC, a, b); }
static BL_INLINE Condition and_c(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kC, a, b); }
static BL_INLINE Condition and_c(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kC, a, b); }
static BL_INLINE Condition and_nc(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kNC, a, b); }
static BL_INLINE Condition and_nc(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kNC, a, b); }
static BL_INLINE Condition and_nc(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignAnd, CondCode::kNC, a, b); }

static BL_INLINE Condition or_z(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kZ, a, b); }
static BL_INLINE Condition or_z(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kZ, a, b); }
static BL_INLINE Condition or_z(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kZ, a, b); }
static BL_INLINE Condition or_nz(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kNZ, a, b); }
static BL_INLINE Condition or_nz(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kNZ, a, b); }
static BL_INLINE Condition or_nz(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kNZ, a, b); }
static BL_INLINE Condition or_c(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kC, a, b); }
static BL_INLINE Condition or_c(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kC, a, b); }
static BL_INLINE Condition or_c(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kC, a, b); }
static BL_INLINE Condition or_nc(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kNC, a, b); }
static BL_INLINE Condition or_nc(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kNC, a, b); }
static BL_INLINE Condition or_nc(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignOr, CondCode::kNC, a, b); }

static BL_INLINE Condition xor_z(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kZ, a, b); }
static BL_INLINE Condition xor_z(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kZ, a, b); }
static BL_INLINE Condition xor_z(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kZ, a, b); }
static BL_INLINE Condition xor_nz(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kNZ, a, b); }
static BL_INLINE Condition xor_nz(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kNZ, a, b); }
static BL_INLINE Condition xor_nz(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kNZ, a, b); }
static BL_INLINE Condition xor_c(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kC, a, b); }
static BL_INLINE Condition xor_c(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kC, a, b); }
static BL_INLINE Condition xor_c(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kC, a, b); }
static BL_INLINE Condition xor_nc(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kNC, a, b); }
static BL_INLINE Condition xor_nc(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kNC, a, b); }
static BL_INLINE Condition xor_nc(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignXor, CondCode::kNC, a, b); }

static BL_INLINE Condition add_z(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kZ, a, b); }
static BL_INLINE Condition add_z(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kZ, a, b); }
static BL_INLINE Condition add_z(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kZ, a, b); }
static BL_INLINE Condition add_nz(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kNZ, a, b); }
static BL_INLINE Condition add_nz(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kNZ, a, b); }
static BL_INLINE Condition add_nz(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kNZ, a, b); }
static BL_INLINE Condition add_c(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kC, a, b); }
static BL_INLINE Condition add_c(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kC, a, b); }
static BL_INLINE Condition add_c(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kC, a, b); }
static BL_INLINE Condition add_nc(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kNC, a, b); }
static BL_INLINE Condition add_nc(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kNC, a, b); }
static BL_INLINE Condition add_nc(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kNC, a, b); }
static BL_INLINE Condition add_s(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kS, a, b); }
static BL_INLINE Condition add_s(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kS, a, b); }
static BL_INLINE Condition add_s(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kS, a, b); }
static BL_INLINE Condition add_ns(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kNS, a, b); }
static BL_INLINE Condition add_ns(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kNS, a, b); }
static BL_INLINE Condition add_ns(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignAdd, CondCode::kNS, a, b); }

static BL_INLINE Condition sub_z(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kZ, a, b); }
static BL_INLINE Condition sub_z(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kZ, a, b); }
static BL_INLINE Condition sub_z(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kZ, a, b); }
static BL_INLINE Condition sub_nz(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kNZ, a, b); }
static BL_INLINE Condition sub_nz(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kNZ, a, b); }
static BL_INLINE Condition sub_nz(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kNZ, a, b); }
static BL_INLINE Condition sub_c(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kC, a, b); }
static BL_INLINE Condition sub_c(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kC, a, b); }
static BL_INLINE Condition sub_c(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kC, a, b); }
static BL_INLINE Condition sub_nc(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kNC, a, b); }
static BL_INLINE Condition sub_nc(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kNC, a, b); }
static BL_INLINE Condition sub_nc(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kNC, a, b); }
static BL_INLINE Condition sub_s(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kS, a, b); }
static BL_INLINE Condition sub_s(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kS, a, b); }
static BL_INLINE Condition sub_s(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kS, a, b); }
static BL_INLINE Condition sub_ns(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kNS, a, b); }
static BL_INLINE Condition sub_ns(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kNS, a, b); }
static BL_INLINE Condition sub_ns(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignSub, CondCode::kNS, a, b); }

static BL_INLINE Condition shr_z(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kZ, a, b); }
static BL_INLINE Condition shr_z(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kZ, a, b); }
static BL_INLINE Condition shr_z(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kZ, a, b); }
static BL_INLINE Condition shr_nz(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kNZ, a, b); }
static BL_INLINE Condition shr_nz(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kNZ, a, b); }
static BL_INLINE Condition shr_nz(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kNZ, a, b); }
static BL_INLINE Condition shr_c(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kC, a, b); }
static BL_INLINE Condition shr_c(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kC, a, b); }
static BL_INLINE Condition shr_c(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kC, a, b); }
static BL_INLINE Condition shr_nc(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kNC, a, b); }
static BL_INLINE Condition shr_nc(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kNC, a, b); }
static BL_INLINE Condition shr_nc(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kAssignSHR, CondCode::kNC, a, b); }

static BL_INLINE Condition bt_z(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kBitTest, CondCode::kNC, a, b); }
static BL_INLINE Condition bt_z(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kBitTest, CondCode::kNC, a, b); }
static BL_INLINE Condition bt_z(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kBitTest, CondCode::kNC, a, b); }
static BL_INLINE Condition bt_nz(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kBitTest, CondCode::kC, a, b); }
static BL_INLINE Condition bt_nz(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kBitTest, CondCode::kC, a, b); }
static BL_INLINE Condition bt_nz(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kBitTest, CondCode::kC, a, b); }

static BL_INLINE Condition test_z(const Gp& a) noexcept { return Condition(Condition::Op::kCompare, CondCode::kEqual, a, Imm(0)); }
static BL_INLINE Condition test_nz(const Gp& a) noexcept { return Condition(Condition::Op::kCompare, CondCode::kNotEqual, a, Imm(0)); }

static BL_INLINE Condition test_z(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kTest, CondCode::kZ, a, b); }
static BL_INLINE Condition test_z(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kTest, CondCode::kZ, a, b); }
static BL_INLINE Condition test_z(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kTest, CondCode::kZ, a, b); }
static BL_INLINE Condition test_nz(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kTest, CondCode::kNZ, a, b); }
static BL_INLINE Condition test_nz(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kTest, CondCode::kNZ, a, b); }
static BL_INLINE Condition test_nz(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kTest, CondCode::kNZ, a, b); }

static BL_INLINE Condition cmp_eq(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kEqual, a, b); }
static BL_INLINE Condition cmp_eq(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kEqual, a, b); }
static BL_INLINE Condition cmp_eq(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kEqual, a, b); }
static BL_INLINE Condition cmp_ne(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kNotEqual, a, b); }
static BL_INLINE Condition cmp_ne(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kNotEqual, a, b); }
static BL_INLINE Condition cmp_ne(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kNotEqual, a, b); }
static BL_INLINE Condition scmp_lt(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedLT, a, b); }
static BL_INLINE Condition scmp_lt(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedLT, a, b); }
static BL_INLINE Condition scmp_lt(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedLT, a, b); }
static BL_INLINE Condition scmp_le(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedLE, a, b); }
static BL_INLINE Condition scmp_le(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedLE, a, b); }
static BL_INLINE Condition scmp_le(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedLE, a, b); }
static BL_INLINE Condition scmp_gt(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedGT, a, b); }
static BL_INLINE Condition scmp_gt(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedGT, a, b); }
static BL_INLINE Condition scmp_gt(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedGT, a, b); }
static BL_INLINE Condition scmp_ge(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedGE, a, b); }
static BL_INLINE Condition scmp_ge(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedGE, a, b); }
static BL_INLINE Condition scmp_ge(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kSignedGE, a, b); }
static BL_INLINE Condition ucmp_lt(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedLT, a, b); }
static BL_INLINE Condition ucmp_lt(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedLT, a, b); }
static BL_INLINE Condition ucmp_lt(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedLT, a, b); }
static BL_INLINE Condition ucmp_le(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedLE, a, b); }
static BL_INLINE Condition ucmp_le(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedLE, a, b); }
static BL_INLINE Condition ucmp_le(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedLE, a, b); }
static BL_INLINE Condition ucmp_gt(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedGT, a, b); }
static BL_INLINE Condition ucmp_gt(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedGT, a, b); }
static BL_INLINE Condition ucmp_gt(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedGT, a, b); }
static BL_INLINE Condition ucmp_ge(const Gp& a, const Gp& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedGE, a, b); }
static BL_INLINE Condition ucmp_ge(const Gp& a, const Mem& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedGE, a, b); }
static BL_INLINE Condition ucmp_ge(const Gp& a, const Imm& b) noexcept { return Condition(Condition::Op::kCompare, CondCode::kUnsignedGE, a, b); }

//! Pipeline compiler.
class PipeCompiler {
public:
  BL_NONCOPYABLE(PipeCompiler)

  enum : uint32_t { kMaxKRegConstCount = 4 };

  //! \name Members
  //! \{

  //! AsmJit compiler.
  AsmCompiler* cc = nullptr;

  const CommonTable& ct;

  //! Target CPU features.
  CpuFeatures _features {};
  //! Optimization flags.
  PipeOptFlags _optFlags = PipeOptFlags::kNone;

  //! Empty predicate, used in cases where a predicate is required, but it's empty.
  PixelPredicate _emptyPredicate {};

  //! SIMD width.
  SimdWidth _simdWidth = SimdWidth::k128;
  //! SIMD multiplier, derived from `_simdWidth` (1, 2, 4).
  uint8_t _simdMultiplier = 0;
  //! SIMD register type (AsmJit).
  asmjit::RegType _simdRegType = asmjit::RegType::kNone;
  //! SIMD type id (AsmJit).
  asmjit::TypeId _simdTypeId = asmjit::TypeId::kVoid;

  //! Function node.
  asmjit::FuncNode* _funcNode = nullptr;
  //! Function initialization hook.
  asmjit::BaseNode* _funcInit = nullptr;
  //! Function end hook (to add 'unlikely' branches).
  asmjit::BaseNode* _funcEnd = nullptr;

  //! Invalid GP register.
  Gp _gpNone;
  //! Holds `ctxData` argument.
  Gp _ctxData;
  //! Holds `fillData` argument.
  Gp _fillData;
  //! Holds `fetchData` argument.
  Gp _fetchData;
  //! Temporary stack used to transfer SIMD regs to GP/MM.
  Mem _tmpStack;

  //! Offset to the first constant to the `commonTable` global.
  int32_t _commonTableOff = 0;
  //! Pointer to the `commonTable` constant pool (only used in 64-bit mode).
  Gp _commonTablePtr;

#if defined(BL_JIT_ARCH_X86)
  KReg _kReg[kMaxKRegConstCount];
  uint64_t _kImm[kMaxKRegConstCount] {};
#endif

  struct VecConst {
    const void* ptr;
    uint32_t vRegId;
  };

  asmjit::ZoneVector<VecConst> _vecConsts;

  //! \}

  //! \name Construction & Destruction
  //! \{

  PipeCompiler(AsmCompiler* cc, const asmjit::CpuFeatures& features, PipeOptFlags optFlags) noexcept;
  ~PipeCompiler() noexcept;

  //! \}

  //! \name Allocators
  //! \{

  inline asmjit::ZoneAllocator* zoneAllocator() noexcept { return &cc->_allocator; }

  //! \}

  //! \name CPU SIMD Width and SIMD Width Utilities
  //! \{

  //! Returns the current SIMD width (in bytes) that this compiler and all its parts must use.
  //!
  //! \note The returned width is in bytes and it's calculated from the maximum supported widths of all pipeline parts.
  //! This means that SIMD width returned could be actually lower than a SIMD width supported by the target CPU.
  inline SimdWidth simdWidth() const noexcept { return _simdWidth; }

  //! Returns whether the compiler and all parts use 256-bit SIMD.
  inline bool use256BitSimd() const noexcept { return _simdWidth >= SimdWidth::k256; }
  //! Returns whether the compiler and all parts use 512-bit SIMD.
  inline bool use512BitSimd() const noexcept { return _simdWidth >= SimdWidth::k512; }

  //! Returns a constant that can be used to multiply a baseline SIMD width to get the value returned by `simdWidth()`.
  //!
  //! \note A baseline SIMD width would be 16 bytes on most platforms.
  inline uint32_t simdMultiplier() const noexcept { return _simdMultiplier; }

  inline SimdWidth simdWidthOf(DataWidth dataWidth, uint32_t n) const noexcept { return SimdWidthUtils::simdWidthOf(simdWidth(), dataWidth, n); }
  inline uint32_t regCountOf(DataWidth dataWidth, uint32_t n) const noexcept { return SimdWidthUtils::regCountOf(simdWidth(), dataWidth, n); }

  inline SimdWidth simdWidthOf(DataWidth dataWidth, PixelCount pixelCount) const noexcept { return SimdWidthUtils::simdWidthOf(simdWidth(), dataWidth, pixelCount.value()); }
  inline uint32_t regCountOf(DataWidth dataWidth, PixelCount pixelCount) const noexcept { return SimdWidthUtils::regCountOf(simdWidth(), dataWidth, pixelCount.value()); }

  //! \}

  //! \name CPU Features and Optimization Options
  //! \{

  BL_INLINE_NODEBUG bool is32Bit() const noexcept { return cc->is32Bit(); }
  BL_INLINE_NODEBUG bool is64Bit() const noexcept { return cc->is64Bit(); }
  BL_INLINE_NODEBUG uint32_t registerSize() const noexcept { return cc->registerSize(); }

#if defined(BL_JIT_ARCH_X86)
  //! Tests whether SSE2 extensions are available (this should always return true as Blend2D requires SSE2).
  inline bool hasSSE2() const noexcept { return _features.x86().hasSSE2(); }
  //! Tests whether SSE3 extensions are available.
  inline bool hasSSE3() const noexcept { return _features.x86().hasSSE3(); }
  //! Tests whether SSSE3 extensions are available.
  inline bool hasSSSE3() const noexcept { return _features.x86().hasSSSE3(); }
  //! Tests whether SSE4.1 extensions are available.
  inline bool hasSSE4_1() const noexcept { return _features.x86().hasSSE4_1(); }
  //! Tests whether SSE4.2 extensions are available.
  inline bool hasSSE4_2() const noexcept { return _features.x86().hasSSE4_2(); }
  //! Tests whether AVX extensions are available.
  inline bool hasAVX() const noexcept { return _features.x86().hasAVX(); }
  //! Tests whether AVX2 extensions are available.
  inline bool hasAVX2() const noexcept { return _features.x86().hasAVX2(); }
  //! Tests whether FMA extensions are available.
  inline bool hasFMA() const noexcept { return _features.x86().hasFMA(); }
  //! Tests whether a baseline AVX-512 extensions are available.
  //!
  //! \note Baseline for us is slightly more than AVX512-F, however, there are no CPUs that would implement
  //! AVX512-F without other extensions that we consider baseline, so we only check AVX512_BW as it's enough
  //! to verify that the CPU has all the required features.
  inline bool hasAVX512() const noexcept { return _features.x86().hasAVX512_BW(); }

  inline bool hasADX() const noexcept { return _features.x86().hasADX(); }
  inline bool hasBMI() const noexcept { return _features.x86().hasBMI(); }
  inline bool hasBMI2() const noexcept { return _features.x86().hasBMI2(); }
  inline bool hasLZCNT() const noexcept { return _features.x86().hasLZCNT(); }
  inline bool hasPOPCNT() const noexcept { return _features.x86().hasPOPCNT(); }
#endif

  inline PipeOptFlags optFlags() const noexcept { return _optFlags; }
  inline bool hasOptFlag(PipeOptFlags flag) const noexcept { return blTestFlag(_optFlags, flag); }

  bool hasMaskedAccessOf(uint32_t dataSize) const noexcept;

  //! \}

  //! \name Function Definition
  //! \{

  void beginFunction() noexcept;
  void endFunction() noexcept;

  //! \}

  //! \name Parts Management
  //! \{

  // TODO: [PIPEGEN] There should be a getter on asmjit side that will return
  // the `ZoneAllocator` object that can be used for these kind of purposes.
  // It doesn't make sense to create another ZoneAllocator.
  template<typename T>
  inline T* newPartT() noexcept {
    void* p = cc->_codeZone.alloc(sizeof(T), 8);
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(BLInternal::PlacementNew{p}) T(this);
  }

  template<typename T, typename... Args>
  inline T* newPartT(Args&&... args) noexcept {
    void* p = cc->_codeZone.alloc(sizeof(T), 8);
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(BLInternal::PlacementNew{p}) T(this, std::forward<Args>(args)...);
  }

  FillPart* newFillPart(FillType fillType, FetchPart* dstPart, CompOpPart* compOpPart) noexcept;
  FetchPart* newFetchPart(FetchType fetchType, FormatExt format) noexcept;
  CompOpPart* newCompOpPart(CompOpExt compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept;

  //! \}

  //! \name Initialization
  //! \{

  void _initSimdWidth(PipePart* root) noexcept;
  void initPipeline(PipePart* root) noexcept;

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

  template<typename... Args>
  BL_INLINE Gp newGp32(const char* name, Args&&... args) noexcept { return cc->newUInt32(name, std::forward<Args>(args)...); }

  template<typename... Args>
  BL_INLINE Gp newGp64(const char* name, Args&&... args) noexcept { return cc->newUInt64(name, std::forward<Args>(args)...); }

  template<typename... Args>
  BL_INLINE Gp newGpPtr(const char* name, Args&&... args) noexcept { return cc->newUIntPtr(name, std::forward<Args>(args)...); }

  template<typename RegT, typename... Args>
  BL_INLINE RegT newSimilarReg(const RegT& ref, Args&&... args) noexcept { return cc->newSimilarReg(ref, std::forward<Args>(args)...); }

  template<typename... Args>
  BL_INLINE Vec newVec(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, _simdTypeId, name, std::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE Vec newVec(SimdWidth simdWidth, const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, SimdWidthUtils::typeIdOf(simdWidth), name, std::forward<Args>(args)...);
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

  // TODO: This should be removed - this can lead to bugs.
  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, _simdTypeId, name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, _simdTypeId, prefix, name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, SimdWidth simdWidth, const char* name) noexcept {
    newRegArray(dst, n, SimdWidthUtils::typeIdOf(simdWidth), name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, SimdWidth simdWidth, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, SimdWidthUtils::typeIdOf(simdWidth), prefix, name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, const Vec& ref, const char* name) noexcept {
    newRegArray(dst, n, ref, name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, const Vec& ref, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, ref, prefix, name);
  }

  Mem tmpStack(uint32_t size) noexcept;

  //! \}

  //! \name Compiler Utilities
  //! \{

  void embedJumpTable(const Label* jumpTable, size_t jumpTableSize, const Label& jumpTableBase, uint32_t entrySize) noexcept;

  //! \}

  void _initCommonTablePtr() noexcept;

#if defined(BL_JIT_ARCH_X86)

  //! \name Virtual Registers (X86)
  //! \{

  template<typename... Args>
  BL_INLINE Vec newXmm(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kInt32x4, name, std::forward<Args>(args)...);
    return reg;
  }

  BL_INLINE void newXmmArray(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x4, name);
  }

  BL_INLINE void newXmmArray(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x4, prefix, name);
  }

  template<typename... Args>
  BL_INLINE Vec newYmm(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kInt32x8, name, std::forward<Args>(args)...);
    return reg;
  }

  BL_INLINE void newYmmArray(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x8, name);
  }

  BL_INLINE void newYmmArray(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x8, prefix, name);
  }

  template<typename... Args>
  BL_INLINE Vec newZmm(const char* name, Args&&... args) noexcept {
    Vec reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kInt32x16, name, std::forward<Args>(args)...);
    return reg;
  }

  BL_INLINE void newZmmArray(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x16, name);
  }

  BL_INLINE void newZmmArray(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x16, prefix, name);
  }

  //! \}

  //! \name Constants (X86)
  //! \{

  KReg kConst(uint64_t value) noexcept;

  Operand simdConst(const void* c, Bcst bcstWidth, SimdWidth constWidth) noexcept;
  Operand simdConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept;
  Operand simdConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept;

  Vec simdVecConst(const void* c, SimdWidth constWidth) noexcept;
  Vec simdVecConst(const void* c, const Vec& similarTo) noexcept;
  Vec simdVecConst(const void* c, const VecArray& similarTo) noexcept;

  Mem simdMemConst(const void* c, Bcst bcstWidth, SimdWidth constWidth) noexcept;
  Mem simdMemConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept;
  Mem simdMemConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept;

  Mem _getMemConst(const void* c) noexcept;
  Vec _newVecConst(const void* c, bool isUniqueConst) noexcept;

  //! \}

  //! \name Emit - Commons
  //! \{

  // Emit helpers used by GP.
  void i_emit_2(InstId instId, const Operand_& op1, int imm) noexcept;
  void i_emit_2(InstId instId, const Operand_& op1, const Operand_& op2) noexcept;
  void i_emit_3(InstId instId, const Operand_& op1, const Operand_& op2, int imm) noexcept;

  //! \}

  //! \name Emit - 'I' General Purpose Instructions
  //! \{

  //! Arithmetic operation having 2 operands (dst, src).
  enum class Arith2Op {
    kAbs,
    kNeg,
    kNot,
    kCLZ,
    kCTZ,
    kReflect,
    kMaxValue = kReflect
  };

  //! Arithmetic operation having 3 operands (dst, src1, src2).
  enum class Arith3Op {
    kAnd,
    kOr,
    kXor,
    kAndN,
    kAdd,
    kSub,
    kMul,
    kUDiv,
    kUMod,
    kSMin,
    kSMax,
    kUMin,
    kUMax,
    kSHL,
    kSHR,
    kSAR,
    kROL,
    kROR,

    kMaxValue = kROR
  };

  void emit_mov(const Gp& dst, const Operand_& src) noexcept;
  void emit_load(const Gp& dst, const Mem& src, uint32_t size) noexcept;
  void emit_store(const Mem& dst, const Gp& src, uint32_t size) noexcept;
  void emit_cmov(const Gp& dst, const Operand_& sel, const Condition& condition) noexcept;
  void emit_select(const Gp& dst, const Operand_& sel1_, const Operand_& sel2_, const Condition& condition) noexcept;
  void emit_arith2(Arith2Op op, const Gp& dst, const Operand_& src_) noexcept;
  void emit_arith3(Arith3Op op, const Gp& dst, const Operand_& src1_, const Operand_& src2_) noexcept;
  void emit_jmp(const Operand_& target) noexcept;
  void emit_jmp_if(const Label& target, const Condition& condition) noexcept;

  BL_INLINE void mov(const Gp& dst, const Gp& src) noexcept { return emit_mov(dst, src); }
  BL_INLINE void mov(const Gp& dst, const Imm& src) noexcept { return emit_mov(dst, src); }

  BL_INLINE void load(const Gp& dst, const Mem& src) noexcept { return emit_load(dst, src, dst.size()); }
  BL_INLINE void load_u8(const Gp& dst, const Mem& src) noexcept { return emit_load(dst, src, 1); }
  BL_INLINE void load_u16(const Gp& dst, const Mem& src) noexcept { return emit_load(dst, src, 2); }
  BL_INLINE void load_u32(const Gp& dst, const Mem& src) noexcept { return emit_load(dst, src, 4); }
  BL_INLINE void load_u64(const Gp& dst, const Mem& src) noexcept { return emit_load(dst, src, 8); }

  BL_INLINE void store(const Mem& dst, const Gp& src) noexcept { return emit_store(dst, src, src.size()); }
  BL_INLINE void store_8(const Mem& dst, const Gp& src) noexcept { return emit_store(dst, src, 1); }
  BL_INLINE void store_16(const Mem& dst, const Gp& src) noexcept { return emit_store(dst, src, 2); }
  BL_INLINE void store_32(const Mem& dst, const Gp& src) noexcept { return emit_store(dst, src, 4); }
  BL_INLINE void store_64(const Mem& dst, const Gp& src) noexcept { return emit_store(dst, src, 8); }

  BL_INLINE void cmov(const Gp& dst, const Gp& sel, const Condition& condition) noexcept { emit_cmov(dst, sel, condition); }
  BL_INLINE void cmov(const Gp& dst, const Mem& sel, const Condition& condition) noexcept { emit_cmov(dst, sel, condition); }

  template<typename Sel1, typename Sel2>
  BL_INLINE void select(const Gp& dst, const Sel1& sel1, const Sel2& sel2, const Condition& condition) noexcept { emit_select(dst, sel1, sel2, condition); }

  BL_INLINE void abs(const Gp& dst, const Gp& src) noexcept { emit_arith2(Arith2Op::kAbs, dst, src); }
  BL_INLINE void abs(const Gp& dst, const Mem& src) noexcept { emit_arith2(Arith2Op::kAbs, dst, src); }

  BL_INLINE void neg(const Gp& dst, const Gp& src) noexcept { emit_arith2(Arith2Op::kNeg, dst, src); }
  BL_INLINE void neg(const Gp& dst, const Mem& src) noexcept { emit_arith2(Arith2Op::kNeg, dst, src); }

  BL_INLINE void not_(const Gp& dst, const Gp& src) noexcept { emit_arith2(Arith2Op::kNot, dst, src); }
  BL_INLINE void not_(const Gp& dst, const Mem& src) noexcept { emit_arith2(Arith2Op::kNot, dst, src); }

  BL_INLINE void clz(const Gp& dst, const Gp& src) noexcept { emit_arith2(Arith2Op::kCLZ, dst, src); }
  BL_INLINE void clz(const Gp& dst, const Mem& src) noexcept { emit_arith2(Arith2Op::kCLZ, dst, src); }

  BL_INLINE void ctz(const Gp& dst, const Gp& src) noexcept { emit_arith2(Arith2Op::kCTZ, dst, src); }
  BL_INLINE void ctz(const Gp& dst, const Mem& src) noexcept { emit_arith2(Arith2Op::kCTZ, dst, src); }

  BL_INLINE void reflect(const Gp& dst, const Gp& src) noexcept { emit_arith2(Arith2Op::kReflect, dst, src); }
  BL_INLINE void reflect(const Gp& dst, const Mem& src) noexcept { emit_arith2(Arith2Op::kReflect, dst, src); }

  BL_INLINE void inc(const Gp& dst) noexcept { emit_arith3(Arith3Op::kAdd, dst, dst, Imm(1)); }
  BL_INLINE void dec(const Gp& dst) noexcept { emit_arith3(Arith3Op::kSub, dst, dst, Imm(1)); }

  BL_INLINE void and_(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kAnd, dst, src1, src2); }
  BL_INLINE void and_(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kAnd, dst, src1, src2); }
  BL_INLINE void and_(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kAnd, dst, src1, src2); }
  BL_INLINE void and_(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kAnd, dst, src1, src2); }
  BL_INLINE void and_(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kAnd, dst, src1, src2); }

  BL_INLINE void or_(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kOr, dst, src1, src2); }
  BL_INLINE void or_(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kOr, dst, src1, src2); }
  BL_INLINE void or_(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kOr, dst, src1, src2); }
  BL_INLINE void or_(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kOr, dst, src1, src2); }
  BL_INLINE void or_(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kOr, dst, src1, src2); }

  BL_INLINE void xor_(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kXor, dst, src1, src2); }
  BL_INLINE void xor_(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kXor, dst, src1, src2); }
  BL_INLINE void xor_(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kXor, dst, src1, src2); }
  BL_INLINE void xor_(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kXor, dst, src1, src2); }
  BL_INLINE void xor_(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kXor, dst, src1, src2); }

  BL_INLINE void andn(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kAndN, dst, src1, src2); }
  BL_INLINE void andn(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kAndN, dst, src1, src2); }
  BL_INLINE void andn(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kAndN, dst, src1, src2); }
  BL_INLINE void andn(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kAndN, dst, src1, src2); }
  BL_INLINE void andn(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kAndN, dst, src1, src2); }

  BL_INLINE void add(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kAdd, dst, src1, src2); }
  BL_INLINE void add(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kAdd, dst, src1, src2); }
  BL_INLINE void add(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kAdd, dst, src1, src2); }
  BL_INLINE void add(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kAdd, dst, src1, src2); }
  BL_INLINE void add(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kAdd, dst, src1, src2); }

  BL_INLINE void sub(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSub, dst, src1, src2); }
  BL_INLINE void sub(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kSub, dst, src1, src2); }
  BL_INLINE void sub(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSub, dst, src1, src2); }
  BL_INLINE void sub(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSub, dst, src1, src2); }
  BL_INLINE void sub(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSub, dst, src1, src2); }

  BL_INLINE void mul(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kMul, dst, src1, src2); }
  BL_INLINE void mul(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kMul, dst, src1, src2); }
  BL_INLINE void mul(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kMul, dst, src1, src2); }
  BL_INLINE void mul(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kMul, dst, src1, src2); }
  BL_INLINE void mul(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kMul, dst, src1, src2); }

  BL_INLINE void udiv(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kUDiv, dst, src1, src2); }
  BL_INLINE void udiv(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kUDiv, dst, src1, src2); }
  BL_INLINE void udiv(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kUDiv, dst, src1, src2); }
  BL_INLINE void udiv(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kUDiv, dst, src1, src2); }
  BL_INLINE void udiv(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kUDiv, dst, src1, src2); }

  BL_INLINE void umod(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kUMod, dst, src1, src2); }
  BL_INLINE void umod(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kUMod, dst, src1, src2); }
  BL_INLINE void umod(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kUMod, dst, src1, src2); }
  BL_INLINE void umod(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kUMod, dst, src1, src2); }
  BL_INLINE void umod(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kUMod, dst, src1, src2); }

  BL_INLINE void smin(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSMin, dst, src1, src2); }
  BL_INLINE void smin(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kSMin, dst, src1, src2); }
  BL_INLINE void smin(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSMin, dst, src1, src2); }
  BL_INLINE void smin(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSMin, dst, src1, src2); }
  BL_INLINE void smin(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSMin, dst, src1, src2); }

  BL_INLINE void smax(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSMax, dst, src1, src2); }
  BL_INLINE void smax(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kSMax, dst, src1, src2); }
  BL_INLINE void smax(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSMax, dst, src1, src2); }
  BL_INLINE void smax(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSMax, dst, src1, src2); }
  BL_INLINE void smax(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSMax, dst, src1, src2); }

  BL_INLINE void umin(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kUMin, dst, src1, src2); }
  BL_INLINE void umin(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kUMin, dst, src1, src2); }
  BL_INLINE void umin(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kUMin, dst, src1, src2); }
  BL_INLINE void umin(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kUMin, dst, src1, src2); }
  BL_INLINE void umin(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kUMin, dst, src1, src2); }

  BL_INLINE void umax(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kUMax, dst, src1, src2); }
  BL_INLINE void umax(const Gp& dst, const Gp& src1, const Mem& src2) noexcept { emit_arith3(Arith3Op::kUMax, dst, src1, src2); }
  BL_INLINE void umax(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kUMax, dst, src1, src2); }
  BL_INLINE void umax(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kUMax, dst, src1, src2); }
  BL_INLINE void umax(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kUMax, dst, src1, src2); }

  BL_INLINE void shl(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSHL, dst, src1, src2); }
  BL_INLINE void shl(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSHL, dst, src1, src2); }
  BL_INLINE void shl(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSHL, dst, src1, src2); }
  BL_INLINE void shl(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSHL, dst, src1, src2); }

  BL_INLINE void shr(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSHR, dst, src1, src2); }
  BL_INLINE void shr(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSHR, dst, src1, src2); }
  BL_INLINE void shr(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSHR, dst, src1, src2); }
  BL_INLINE void shr(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSHR, dst, src1, src2); }

  BL_INLINE void sar(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSAR, dst, src1, src2); }
  BL_INLINE void sar(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSAR, dst, src1, src2); }
  BL_INLINE void sar(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kSAR, dst, src1, src2); }
  BL_INLINE void sar(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kSAR, dst, src1, src2); }

  BL_INLINE void rol(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kROL, dst, src1, src2); }
  BL_INLINE void rol(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kROL, dst, src1, src2); }
  BL_INLINE void rol(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kROL, dst, src1, src2); }
  BL_INLINE void rol(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kROL, dst, src1, src2); }

  BL_INLINE void ror(const Gp& dst, const Gp& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kROR, dst, src1, src2); }
  BL_INLINE void ror(const Gp& dst, const Gp& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kROR, dst, src1, src2); }
  BL_INLINE void ror(const Gp& dst, const Mem& src1, const Gp& src2) noexcept { emit_arith3(Arith3Op::kROR, dst, src1, src2); }
  BL_INLINE void ror(const Gp& dst, const Mem& src1, const Imm& src2) noexcept { emit_arith3(Arith3Op::kROR, dst, src1, src2); }

  BL_INLINE void j(const Gp& target) noexcept { emit_jmp(target); }
  BL_INLINE void j(const Label& target) noexcept { emit_jmp(target); }
  BL_INLINE void j(const Label& target, const Condition& condition) noexcept { emit_jmp_if(target, condition); }

  void adds_u8(const Gp& dst, const Gp& src1, const Gp& src2) noexcept;

  void inv_u8(const Gp& dst, const Gp& src) noexcept;
  void div_255_u32(const Gp& dst, const Gp& src) noexcept;
  void mul_257_hu16(const Gp& dst, const Gp& src) noexcept;

  template<typename LimitT>
  BL_NOINLINE void bound_u(const Gp& dst, const Gp& src, const LimitT& limit) noexcept {
    if (dst.id() == src.id()) {
      Gp zero = newSimilarReg(dst, "@zero");

      cc->xor_(zero, zero);
      cc->cmp(dst, limit);
      cc->cmova(dst, zero);
      cc->cmovg(dst, limit);
    }
    else {
      cc->xor_(dst, dst);
      cc->cmp(src, limit);
      cc->cmovbe(dst, src);
      cc->cmovg(dst, limit);
    }
  }

  //! dst += a * b.
  void add_scaled(const Gp& dst, const Gp& a, int b) noexcept;

  void lea_bpp(const Gp& dst, const Gp& src_, const Gp& idx_, uint32_t scale, int32_t disp = 0) noexcept;

  inline void i_prefetch(const Mem& mem) noexcept { cc->prefetcht0(mem); }

  void lea(const Gp& dst, const Mem& src) noexcept;

  //! \}

  //! \name Packed Instruction (X86)
  //! \{

  //! Packing generic instructions and SSE+AVX instructions into a single 32-bit integer.
  //!
  //! AsmJit has more than 1500 instructions for X86|X64, which means that we need at least 11 bits to represent each.
  //! Typically we need just one instruction ID at a time, however, since SSE and AVX instructions use different IDs
  //! we need a way to pack both SSE and AVX instruction ID into one integer as it's much easier to use a unified
  //! instruction set rather than using specific paths for SSE and AVX code.
  //!
  //! PackedInst allows to specify the following:
  //!
  //!   - SSE instruction ID for SSE+ code generation.
  //!   - AVX instruction ID for AVX+ code generation.
  //!   - Maximum operation width (0=XMM, 1=YMM, 2=ZMM).
  //!   - Special intrinsic used only by PipeCompiler.
  struct PackedInst {
    //! Limit width of operands of vector instructions to Xmm|Ymm|Zmm.
    enum WidthLimit : uint32_t {
      kWidthX = 0,
      kWidthY = 1,
      kWidthZ = 2
    };

    enum Bits : uint32_t {
      kSseIdShift  = 0,
      kSseIdBits   = 0xFFF,

      kAvxIdShift  = 12,
      kAvxIdBits   = 0xFFF,

      kWidthShift  = 24,
      kWidthBits   = 0x3,

      kIntrinShift = 31,
      kIntrinBits  = 0x1
    };

    static ASMJIT_INLINE_NODEBUG constexpr uint32_t packIntrin(uint32_t intrinId, uint32_t width = kWidthZ) noexcept {
      return (intrinId << kSseIdShift ) |
             (width    << kWidthShift ) |
             (1u       << kIntrinShift) ;
    }

    static ASMJIT_INLINE_NODEBUG constexpr uint32_t packAvxSse(uint32_t avxId, uint32_t sseId, uint32_t width = kWidthZ) noexcept {
      return (avxId << kAvxIdShift) |
             (sseId << kSseIdShift) |
             (width << kWidthShift) ;
    }

    static ASMJIT_INLINE_NODEBUG constexpr uint32_t avxId(uint32_t packedId) noexcept { return (packedId >> kAvxIdShift) & kAvxIdBits; }
    static ASMJIT_INLINE_NODEBUG constexpr uint32_t sseId(uint32_t packedId) noexcept { return (packedId >> kSseIdShift) & kSseIdBits; }
    static ASMJIT_INLINE_NODEBUG constexpr uint32_t width(uint32_t packedId) noexcept { return (packedId >> kWidthShift) & kWidthBits; }

    static ASMJIT_INLINE_NODEBUG constexpr uint32_t isIntrin(uint32_t packedId) noexcept { return (packedId & (kIntrinBits << kIntrinShift)) != 0; }
    static ASMJIT_INLINE_NODEBUG constexpr uint32_t intrinId(uint32_t packedId) noexcept { return (packedId >> kSseIdShift) & kSseIdBits; }
  };

  //! Intrinsic ID.
  //!
  //! Some operations are not available as a single instruction or are part
  //! of CPU extensions outside of the baseline instruction set. These are
  //! handled as intrinsics.
  enum IntrinId : uint32_t {
    kIntrin2Vloadi128uRO,

    kIntrin2Vmovu8u16,
    kIntrin2Vmovu8u32,
    kIntrin2Vmovu16u32,
    kIntrin2Vabsi8,
    kIntrin2Vabsi16,
    kIntrin2Vabsi32,
    kIntrin2Vabsi64,
    kIntrin2Vinv255u16,
    kIntrin2Vinv256u16,
    kIntrin2Vinv255u32,
    kIntrin2Vinv256u32,
    kIntrin2Vduplpd,
    kIntrin2Vduphpd,

    kIntrin2VBroadcastU8,
    kIntrin2VBroadcastU16,
    kIntrin2VBroadcastU32,
    kIntrin2VBroadcastU64,
    kIntrin2VBroadcastI32x4,
    kIntrin2VBroadcastI64x2,
    kIntrin2VBroadcastF32x4,
    kIntrin2VBroadcastF64x2,

    kIntrin2iVswizps,
    kIntrin2iVswizpd,

    kIntrin3Vandi32,
    kIntrin3Vandi64,
    kIntrin3Vnandi32,
    kIntrin3Vnandi64,
    kIntrin3Vori32,
    kIntrin3Vori64,
    kIntrin3Vxori32,
    kIntrin3Vxori64,
    kIntrin3Vcombhli64,
    kIntrin3Vcombhld64,
    kIntrin3Vminu16,
    kIntrin3Vmaxu16,
    kIntrin3Vmulu64x32,
    kIntrin3Vhaddpd,

    kIntrin3iVpalignr,

    kIntrin4Vpblendvb,
    kIntrin4VpblendvbDestructive
  };

  // Emit helpers to emit MOVE from SrcT to DstT, used by pre-AVX instructions. The `width` parameter is important
  // as it describes how many bytes to read in case that `src` is a memory location. It's important as some
  // instructions like PMOVZXBW read only 8 bytes, but to make the same operation in pre-SSE4.1 code we need to
  // read 8 bytes from memory and use PUNPCKLBW to interleave that bytes with zero. PUNPCKLBW would read 16 bytes
  // from memory and would require them to be aligned to 16 bytes, if used with memory operand.
  void v_emit_xmov(const Operand_& dst, const Operand_& src, uint32_t width) noexcept;
  void v_emit_xmov(const OpArray& dst, const Operand_& src, uint32_t width) noexcept;
  void v_emit_xmov(const OpArray& dst, const OpArray& src, uint32_t width) noexcept;

  // Emit helpers used by SSE|AVX intrinsics.
  void v_emit_vv_vv(uint32_t packedId, const Operand_& dst_, const Operand_& src_) noexcept;
  void v_emit_vv_vv(uint32_t packedId, const OpArray& dst_, const Operand_& src_) noexcept;
  void v_emit_vv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src_) noexcept;

  void v_emit_vvi_vi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void v_emit_vvi_vi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void v_emit_vvi_vi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept;

  void v_emit_vvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void v_emit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void v_emit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept;

  void v_emit_vvv_vv(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_) noexcept;
  void v_emit_vvv_vv(uint32_t packedId, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_) noexcept;
  void v_emit_vvv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_) noexcept;
  void v_emit_vvv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_) noexcept;

  void v_emit_vvvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, uint32_t imm) noexcept;
  void v_emit_vvvi_vvi(uint32_t packedId, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_, uint32_t imm) noexcept;
  void v_emit_vvvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_, uint32_t imm) noexcept;
  void v_emit_vvvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, uint32_t imm) noexcept;

  void v_emit_vvvv_vvv(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, const Operand_& src3_) noexcept;
  void v_emit_vvvv_vvv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, const Operand_& src3_) noexcept;
  void v_emit_vvvv_vvv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, const OpArray& src3_) noexcept;

  void v_emit_k_vv(InstId instId, const KReg& mask, const Operand_& dst, const Operand_& src) noexcept;
  void v_emit_k_vv(InstId instId, const KReg& mask, OpArray& dst, const Operand_& src) noexcept;
  void v_emit_k_vv(InstId instId, const KReg& mask, OpArray& dst, const OpArray& src) noexcept;

  void v_emit_k_vvi(InstId instId, const KReg& mask, const Operand_& dst, const Operand_& src, uint32_t imm8) noexcept;
  void v_emit_k_vvi(InstId instId, const KReg& mask, OpArray& dst, const Operand_& src, uint32_t imm8) noexcept;
  void v_emit_k_vvi(InstId instId, const KReg& mask, OpArray& dst, const OpArray& src, uint32_t imm8) noexcept;

  void v_emit_k_vvv(InstId instId, const KReg& mask, const Operand_& dst, const Operand_& src1, const Operand_& src2) noexcept;
  void v_emit_k_vvv(InstId instId, const KReg& mask, const OpArray& dst, const Operand_& src1, const OpArray& src2) noexcept;
  void v_emit_k_vvv(InstId instId, const KReg& mask, const OpArray& dst, const OpArray& src1, const Operand_& src2) noexcept;
  void v_emit_k_vvv(InstId instId, const KReg& mask, const OpArray& dst, const OpArray& src1, const OpArray& src2) noexcept;

  void v_emit_k_vvvi(InstId instId, const KReg& mask, const Operand_& dst, const Operand_& src1, const Operand_& src2, uint32_t imm8) noexcept;
  void v_emit_k_vvvi(InstId instId, const KReg& mask, const OpArray& dst, const Operand_& src1, const OpArray& src2, uint32_t imm8) noexcept;
  void v_emit_k_vvvi(InstId instId, const KReg& mask, const OpArray& dst, const OpArray& src1, const Operand_& src2, uint32_t imm8) noexcept;
  void v_emit_k_vvvi(InstId instId, const KReg& mask, const OpArray& dst, const OpArray& src1, const OpArray& src2, uint32_t imm8) noexcept;

  #define V_EMIT_VV_VV(NAME, PACKED_ID)                                       \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src) noexcept {                                \
    v_emit_vv_vv(PACKED_ID, dst, src);                                        \
  }

  #define V_EMIT_VVI_VI(NAME, PACKED_ID)                                      \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src,                                           \
                   uint32_t imm) noexcept {                                   \
    v_emit_vvi_vi(PACKED_ID, dst, src, imm);                                  \
  }

  #define V_EMIT_VVI_VVI(NAME, PACKED_ID)                                     \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src,                                           \
                   uint32_t imm) noexcept {                                   \
    v_emit_vvi_vvi(PACKED_ID, dst, src, imm);                                 \
  }

  #define V_EMIT_VVI_VVI_ENUM(NAME, PACKED_ID, ENUM_TYPE)                     \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src,                                           \
                   ENUM_TYPE imm) noexcept {                                  \
    v_emit_vvi_vvi(PACKED_ID, dst, src, uint32_t(imm));                       \
  }

  #define V_EMIT_VVi_VVi(NAME, PACKED_ID, IMM_VALUE)                          \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src) noexcept {                                \
    v_emit_vvi_vvi(PACKED_ID, dst, src, IMM_VALUE);                           \
  }

  #define V_EMIT_VVV_VV(NAME, PACKED_ID)                                      \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2) noexcept {                              \
    v_emit_vvv_vv(PACKED_ID, dst, src1, src2);                                \
  }

  #define V_EMIT_VVVI_VVI(NAME, PACKED_ID)                                    \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2,                                         \
                   uint32_t imm) noexcept {                                   \
    v_emit_vvvi_vvi(PACKED_ID, dst, src1, src2, imm);                         \
  }

  #define V_EMIT_VVVI_VVI_ENUM(NAME, PACKED_ID, ENUM_TYPE)                    \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2,                                         \
                   ENUM_TYPE imm) noexcept {                                  \
    v_emit_vvvi_vvi(PACKED_ID, dst, src1, src2, uint32_t(imm));               \
  }

  #define V_EMIT_VVVI(NAME, PACKED_ID)                                        \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2,                                         \
                   uint32_t imm) noexcept {                                   \
    v_emit_vvvi_vvi(PACKED_ID, dst, src1, src2, imm);                         \
  }

  #define V_EMIT_VVVi_VVi(NAME, PACKED_ID, IMM_VALUE)                         \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2) noexcept {                              \
    v_emit_vvvi_vvi(PACKED_ID, dst, src1, src2, IMM_VALUE);                   \
  }

  #define V_EMIT_VVVV_VVV(NAME, PACKED_ID)                                    \
  template<typename DstT, typename Src1T, typename Src2T, typename Src3T>     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2,                                         \
                   const Src3T& src3) noexcept {                              \
    v_emit_vvvv_vvv(PACKED_ID, dst, src1, src2, src3);                        \
  }

  #define PACK_AVX_SSE(AVX_ID, SSE_ID, W) \
    PackedInst::packAvxSse(x86::Inst::kId##AVX_ID, x86::Inst::kId##SSE_ID, PackedInst::kWidth##W)

  //! \}

  //! \name Emit - 'V' Vector Instructions (128..512-bit SSE|AVX|AVX512)
  //! \{

  // To make the code generation easier and more parametrizable we support both SSE|AVX through the same interface
  // (always non-destructive source form) and each intrinsic can accept either `Operand_` or `OpArray`, which can
  // hold up to 4 registers to form scalars, pairs and quads. Each 'V' instruction maps directly to the ISA so check
  // the optimization level before using them or use instructions starting with 'x' that are generic and designed to
  // map to the best instruction(s) possible.
  //
  // Also, multiple overloads are provided for convenience, similarly to AsmJit design, we don't want to inline
  // expansion of `OpArray(op)` here so these overloads are implemented in pipecompiler.cpp.

  // SSE instructions that require SSE3+ are suffixed with `_` to make it clear that they are not part of the
  // baseline instruction set. Some instructions like that are always provided don't have such suffix, and will be
  // emulated.

  // Integer SIMD - Core.

  V_EMIT_VV_VV(v_mov               , PACK_AVX_SSE(Vmovaps    , Movaps    , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(v_mov_i64           , PACK_AVX_SSE(Vmovq      , Movq      , X))       // AVX  | SSE2

  V_EMIT_VV_VV(v_mov_i8_i16_       , PACK_AVX_SSE(Vpmovsxbw  , Pmovsxbw  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u8_u16_       , PACK_AVX_SSE(Vpmovzxbw  , Pmovzxbw  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_i8_i32_       , PACK_AVX_SSE(Vpmovsxbd  , Pmovsxbd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u8_u32_       , PACK_AVX_SSE(Vpmovzxbd  , Pmovzxbd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_i8_i64_       , PACK_AVX_SSE(Vpmovsxbq  , Pmovsxbq  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u8_u64_       , PACK_AVX_SSE(Vpmovzxbq  , Pmovzxbq  , Z))       // AVX2 | SSE4.1

  V_EMIT_VV_VV(v_mov_i16_i32_      , PACK_AVX_SSE(Vpmovsxwd  , Pmovsxwd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u16_u32_      , PACK_AVX_SSE(Vpmovzxwd  , Pmovzxwd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_i16_i64_      , PACK_AVX_SSE(Vpmovsxwq  , Pmovsxwq  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u16_u64_      , PACK_AVX_SSE(Vpmovzxwq  , Pmovzxwq  , Z))       // AVX2 | SSE4.1

  V_EMIT_VV_VV(v_mov_i32_i64_      , PACK_AVX_SSE(Vpmovsxdq  , Pmovsxdq  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u32_u64_      , PACK_AVX_SSE(Vpmovzxdq  , Pmovzxdq  , Z))       // AVX2 | SSE4.1

  V_EMIT_VV_VV(v_mov_mask_u8       , PACK_AVX_SSE(Vpmovmskb  , Pmovmskb  , Z))       // AVX2 | SSE2

  V_EMIT_VVVI_VVI(v_insert_u8_     , PACK_AVX_SSE(Vpinsrb    , Pinsrb    , X))       // AVX2 | SSE4_1
  V_EMIT_VVVI_VVI(v_insert_u16     , PACK_AVX_SSE(Vpinsrw    , Pinsrw    , X))       // AVX2 | SSE2
  V_EMIT_VVVI_VVI(v_insert_u32_    , PACK_AVX_SSE(Vpinsrd    , Pinsrd    , X))       // AVX2 | SSE4_1
  V_EMIT_VVVI_VVI(v_insert_u64_    , PACK_AVX_SSE(Vpinsrq    , Pinsrq    , X))       // AVX2 | SSE4_1

  V_EMIT_VVI_VVI(v_extract_u8_     , PACK_AVX_SSE(Vpextrb    , Pextrb    , X))       // AVX2 | SSE4_1
  V_EMIT_VVI_VVI(v_extract_u16     , PACK_AVX_SSE(Vpextrw    , Pextrw    , X))       // AVX2 | SSE2
  V_EMIT_VVI_VVI(v_extract_u32_    , PACK_AVX_SSE(Vpextrd    , Pextrd    , X))       // AVX2 | SSE4_1
  V_EMIT_VVI_VVI(v_extract_u64_    , PACK_AVX_SSE(Vpextrq    , Pextrq    , X))       // AVX2 | SSE4_1

  V_EMIT_VVV_VV(v_and_i32          , PackedInst::packIntrin(kIntrin3Vandi32))        // AVX2 | SSE2
  V_EMIT_VVV_VV(v_and_i64          , PackedInst::packIntrin(kIntrin3Vandi64))        // AVX2 | SSE2
  V_EMIT_VVV_VV(v_and_f32          , PACK_AVX_SSE(Vandps     , Andps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_and_f64          , PACK_AVX_SSE(Vandpd     , Andpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_nand_i32         , PackedInst::packIntrin(kIntrin3Vnandi32))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_nand_i64         , PackedInst::packIntrin(kIntrin3Vnandi64))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_nand_f32         , PACK_AVX_SSE(Vandnps    , Andnps    , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_nand_f64         , PACK_AVX_SSE(Vandnpd    , Andnpd    , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_or_i32           , PackedInst::packIntrin(kIntrin3Vori32))         // AVX2 | SSE2
  V_EMIT_VVV_VV(v_or_i64           , PackedInst::packIntrin(kIntrin3Vori64))         // AVX2 | SSE2
  V_EMIT_VVV_VV(v_or_f32           , PACK_AVX_SSE(Vorps      , Orps      , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_or_f64           , PACK_AVX_SSE(Vorpd      , Orpd      , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_xor_i32          , PackedInst::packIntrin(kIntrin3Vxori32))        // AVX2 | SSE2
  V_EMIT_VVV_VV(v_xor_i64          , PackedInst::packIntrin(kIntrin3Vxori64))        // AVX2 | SSE2
  V_EMIT_VVV_VV(v_xor_f32          , PACK_AVX_SSE(Vxorps     , Xorps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_xor_f64          , PACK_AVX_SSE(Vxorpd     , Xorpd     , Z))       // AVX  | SSE2

  V_EMIT_VVI_VI(v_sll_i16          , PACK_AVX_SSE(Vpsllw     , Psllw     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_sll_i32          , PACK_AVX_SSE(Vpslld     , Pslld     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_sll_i64          , PACK_AVX_SSE(Vpsllq     , Psllq     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_srl_i16          , PACK_AVX_SSE(Vpsrlw     , Psrlw     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_srl_i32          , PACK_AVX_SSE(Vpsrld     , Psrld     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_srl_i64          , PACK_AVX_SSE(Vpsrlq     , Psrlq     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_sra_i16          , PACK_AVX_SSE(Vpsraw     , Psraw     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_sra_i32          , PACK_AVX_SSE(Vpsrad     , Psrad     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_sllb_u128        , PACK_AVX_SSE(Vpslldq    , Pslldq    , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_srlb_u128        , PACK_AVX_SSE(Vpsrldq    , Psrldq    , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_shuffle_i8       , PACK_AVX_SSE(Vpshufb    , Pshufb    , Z))       // AVX2 | SSSE3
  V_EMIT_VVVI_VVI(v_shuffle_f32    , PACK_AVX_SSE(Vshufps    , Shufps    , Z))       // AVX  | SSE
  V_EMIT_VVVI_VVI(v_shuffle_f64    , PACK_AVX_SSE(Vshufpd    , Shufpd    , Z))       // AVX  | SSE2

  V_EMIT_VVI_VVI(v_swizzle_lo_u16  , PACK_AVX_SSE(Vpshuflw   , Pshuflw   , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VVI(v_swizzle_hi_u16  , PACK_AVX_SSE(Vpshufhw   , Pshufhw   , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VVI(v_swizzle_u32     , PACK_AVX_SSE(Vpshufd    , Pshufd    , Z))       // AVX2 | SSE2

  V_EMIT_VVVI_VVI(v_shuffle_u32    , PACK_AVX_SSE(Vshufps    , Shufps    , Z))       // AVX  | SSE
  V_EMIT_VVVI_VVI(v_shuffle_u64    , PACK_AVX_SSE(Vshufpd    , Shufpd    , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(v_interleave_lo_u8 , PACK_AVX_SSE(Vpunpcklbw , Punpcklbw , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_hi_u8 , PACK_AVX_SSE(Vpunpckhbw , Punpckhbw , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_lo_u16, PACK_AVX_SSE(Vpunpcklwd , Punpcklwd , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_hi_u16, PACK_AVX_SSE(Vpunpckhwd , Punpckhwd , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_lo_u32, PACK_AVX_SSE(Vpunpckldq , Punpckldq , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_hi_u32, PACK_AVX_SSE(Vpunpckhdq , Punpckhdq , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_lo_u64, PACK_AVX_SSE(Vpunpcklqdq, Punpcklqdq, Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_hi_u64, PACK_AVX_SSE(Vpunpckhqdq, Punpckhqdq, Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_lo_f32, PACK_AVX_SSE(Vunpcklps  , Unpcklps  , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_interleave_lo_f64, PACK_AVX_SSE(Vunpcklpd  , Unpcklpd  , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_interleave_hi_f32, PACK_AVX_SSE(Vunpckhps  , Unpckhps  , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_interleave_hi_f64, PACK_AVX_SSE(Vunpckhpd  , Unpckhpd  , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(v_packs_i32_i16    , PACK_AVX_SSE(Vpackssdw  , Packssdw  , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_packs_i32_u16_   , PACK_AVX_SSE(Vpackusdw  , Packusdw  , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_packs_i16_i8     , PACK_AVX_SSE(Vpacksswb  , Packsswb  , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_packs_i16_u8     , PACK_AVX_SSE(Vpackuswb  , Packuswb  , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_avg_u8           , PACK_AVX_SSE(Vpavgb     , Pavgb     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_avg_u16          , PACK_AVX_SSE(Vpavgw     , Pavgw     , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_sign_i8_         , PACK_AVX_SSE(Vpsignb    , Psignb    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(v_sign_i16_        , PACK_AVX_SSE(Vpsignw    , Psignw    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(v_sign_i32_        , PACK_AVX_SSE(Vpsignd    , Psignd    , Z))       // AVX2 | SSSE3

  V_EMIT_VVV_VV(v_add_i8           , PACK_AVX_SSE(Vpaddb     , Paddb     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_add_i16          , PACK_AVX_SSE(Vpaddw     , Paddw     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_add_i32          , PACK_AVX_SSE(Vpaddd     , Paddd     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_add_i64          , PACK_AVX_SSE(Vpaddq     , Paddq     , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_sub_i8           , PACK_AVX_SSE(Vpsubb     , Psubb     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_sub_i16          , PACK_AVX_SSE(Vpsubw     , Psubw     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_sub_i32          , PACK_AVX_SSE(Vpsubd     , Psubd     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_sub_i64          , PACK_AVX_SSE(Vpsubq     , Psubq     , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_adds_i8          , PACK_AVX_SSE(Vpaddsb    , Paddsb    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_adds_u8          , PACK_AVX_SSE(Vpaddusb   , Paddusb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_adds_i16         , PACK_AVX_SSE(Vpaddsw    , Paddsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_adds_u16         , PACK_AVX_SSE(Vpaddusw   , Paddusw   , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_subs_i8          , PACK_AVX_SSE(Vpsubsb    , Psubsb    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_subs_i16         , PACK_AVX_SSE(Vpsubsw    , Psubsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_subs_u8          , PACK_AVX_SSE(Vpsubusb   , Psubusb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_subs_u16         , PACK_AVX_SSE(Vpsubusw   , Psubusw   , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_mul_i16          , PACK_AVX_SSE(Vpmullw    , Pmullw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_mul_u16          , PACK_AVX_SSE(Vpmullw    , Pmullw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_mulh_i16         , PACK_AVX_SSE(Vpmulhw    , Pmulhw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_mulh_u16         , PACK_AVX_SSE(Vpmulhuw   , Pmulhuw   , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_mul_i32_         , PACK_AVX_SSE(Vpmulld    , Pmulld    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_mul_u32_         , PACK_AVX_SSE(Vpmulld    , Pmulld    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_mulx_ll_i32_     , PACK_AVX_SSE(Vpmuldq    , Pmuldq    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_mulx_ll_u32_     , PACK_AVX_SSE(Vpmuludq   , Pmuludq   , Z))       // AVX2 | SSE2

  V_EMIT_VVVi_VVi(v_mulx_ll_u64_   , PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x00) // AVX2 | PCLMULQDQ
  V_EMIT_VVVi_VVi(v_mulx_hl_u64_   , PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x01) // AVX2 | PCLMULQDQ
  V_EMIT_VVVi_VVi(v_mulx_lh_u64_   , PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x10) // AVX2 | PCLMULQDQ
  V_EMIT_VVVi_VVi(v_mulx_hh_u64_   , PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x11) // AVX2 | PCLMULQDQ

  V_EMIT_VVV_VV(v_min_i8_          , PACK_AVX_SSE(Vpminsb    , Pminsb    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_min_u8           , PACK_AVX_SSE(Vpminub    , Pminub    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_min_i16          , PACK_AVX_SSE(Vpminsw    , Pminsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_min_i32_         , PACK_AVX_SSE(Vpminsd    , Pminsd    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_min_u32_         , PACK_AVX_SSE(Vpminud    , Pminud    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_max_i8_          , PACK_AVX_SSE(Vpmaxsb    , Pmaxsb    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_max_u8           , PACK_AVX_SSE(Vpmaxub    , Pmaxub    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_max_i16          , PACK_AVX_SSE(Vpmaxsw    , Pmaxsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_max_i32_         , PACK_AVX_SSE(Vpmaxsd    , Pmaxsd    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_max_u32_         , PACK_AVX_SSE(Vpmaxud    , Pmaxud    , Z))       // AVX2 | SSE4.1

  V_EMIT_VVV_VV(v_cmp_eq_i8        , PACK_AVX_SSE(Vpcmpeqb   , Pcmpeqb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_eq_i16       , PACK_AVX_SSE(Vpcmpeqw   , Pcmpeqw   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_eq_i32       , PACK_AVX_SSE(Vpcmpeqd   , Pcmpeqd   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_eq_i64_      , PACK_AVX_SSE(Vpcmpeqq   , Pcmpeqq   , Z))       // AVX2 | SSE4.1

  V_EMIT_VVV_VV(v_cmp_gt_i8        , PACK_AVX_SSE(Vpcmpgtb   , Pcmpgtb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_gt_i16       , PACK_AVX_SSE(Vpcmpgtw   , Pcmpgtw   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_gt_i32       , PACK_AVX_SSE(Vpcmpgtd   , Pcmpgtd   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_gt_i64_      , PACK_AVX_SSE(Vpcmpgtq   , Pcmpgtq   , Z))       // AVX2 | SSE4.2

  V_EMIT_VVVV_VVV(v_blendv_u8_     , PACK_AVX_SSE(Vpblendvb  , Pblendvb  , Z))       // AVX2 | SSE4.1
  V_EMIT_VVVI_VVI(v_blend_u16_     , PACK_AVX_SSE(Vpblendw   , Pblendw   , Z))       // AVX2 | SSE4.1

  V_EMIT_VVV_VV(v_hadd_i16_        , PACK_AVX_SSE(Vphaddw    , Phaddw    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(v_hadd_i32_        , PACK_AVX_SSE(Vphaddd    , Phaddd    , Z))       // AVX2 | SSSE3

  V_EMIT_VVV_VV(v_hsub_i16_        , PACK_AVX_SSE(Vphsubw    , Phsubw    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(v_hsub_i32_        , PACK_AVX_SSE(Vphsubd    , Phsubd    , Z))       // AVX2 | SSSE3

  V_EMIT_VVV_VV(v_hadds_i16_       , PACK_AVX_SSE(Vphaddsw   , Phaddsw   , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(v_hsubs_i16_       , PACK_AVX_SSE(Vphsubsw   , Phsubsw   , Z))       // AVX2 | SSSE3

  // Integer SIMD - Miscellaneous.

  V_EMIT_VV_VV(v_test_             , PACK_AVX_SSE(Vptest     , Ptest     , Z))       // AVX2 | SSE4_1

  // Integer SIMD - Consult X86 manual before using these...

  V_EMIT_VVV_VV(v_sad_u8           , PACK_AVX_SSE(Vpsadbw    , Psadbw    , Z))       // AVX2 | SSE2      [dst.u64[0..X] = SUM{0.7}(ABS(src1.u8[N] - src2.u8[N]))))]
  V_EMIT_VVV_VV(v_mulrh_i16_       , PACK_AVX_SSE(Vpmulhrsw  , Pmulhrsw  , Z))       // AVX2 | SSSE3     [dst.i16[0..X] = ((((src1.i16[0] * src2.i16[0])) >> 14)) + 1)) >> 1))]
  V_EMIT_VVV_VV(v_madds_u8_i8_     , PACK_AVX_SSE(Vpmaddubsw , Pmaddubsw , Z))       // AVX2 | SSSE3     [dst.i16[0..X] = SAT(src1.u8[0] * src2.i8[0] + src1.u8[1] * src2.i8[1]))
  V_EMIT_VVV_VV(v_madd_i16_i32     , PACK_AVX_SSE(Vpmaddwd   , Pmaddwd   , Z))       // AVX2 | SSE2      [dst.i32[0..X] = (src1.i16[0] * src2.i16[0] + src1.i16[1] * src2.i16[1]))
  V_EMIT_VVVI_VVI(v_mpsad_u8_      , PACK_AVX_SSE(Vmpsadbw   , Mpsadbw   , Z))       // AVX2 | SSE4.1
  V_EMIT_VVVI_VVI(v_alignr_u128    , PackedInst::packIntrin(kIntrin3iVpalignr))      // AVX2 | SSSE3
  V_EMIT_VVVI_VVI(v_alignr_u128_   , PACK_AVX_SSE(Vpalignr   , Palignr   , Z))       // AVX2 | SSSE3
  V_EMIT_VV_VV(v_hmin_pos_u16_     , PACK_AVX_SSE(Vphminposuw, Phminposuw, Z))       // AVX2 | SSE4_1

  // Floating Point - Core.

  V_EMIT_VV_VV(vmovaps             , PACK_AVX_SSE(Vmovaps    , Movaps    , Z))       // AVX  | SSE
  V_EMIT_VV_VV(vmovapd             , PACK_AVX_SSE(Vmovapd    , Movapd    , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(vmovups             , PACK_AVX_SSE(Vmovups    , Movups    , Z))       // AVX  | SSE
  V_EMIT_VV_VV(vmovupd             , PACK_AVX_SSE(Vmovupd    , Movupd    , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vmovlps2x          , PACK_AVX_SSE(Vmovlps    , Movlps    , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmovhps2x          , PACK_AVX_SSE(Vmovhps    , Movhps    , X))       // AVX  | SSE

  V_EMIT_VVV_VV(vmovlhps2x         , PACK_AVX_SSE(Vmovlhps   , Movlhps   , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmovhlps2x         , PACK_AVX_SSE(Vmovhlps   , Movhlps   , X))       // AVX  | SSE

  V_EMIT_VVV_VV(vmovlpd            , PACK_AVX_SSE(Vmovlpd    , Movlpd    , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmovhpd            , PACK_AVX_SSE(Vmovhpd    , Movhpd    , X))       // AVX  | SSE

  V_EMIT_VV_VV(vmovduplps_         , PACK_AVX_SSE(Vmovsldup  , Movsldup  , Z))       // AVX  | SSE3
  V_EMIT_VV_VV(vmovduphps_         , PACK_AVX_SSE(Vmovshdup  , Movshdup  , Z))       // AVX  | SSE3

  V_EMIT_VV_VV(vmov_dupl_2xf32_    , PACK_AVX_SSE(Vmovddup   , Movddup   , Z))       // AVX  | SSE3

  V_EMIT_VV_VV(v_move_mask_f32     , PACK_AVX_SSE(Vmovmskps  , Movmskps  , Z))       // AVX  | SSE
  V_EMIT_VV_VV(v_move_mask_f64     , PACK_AVX_SSE(Vmovmskpd  , Movmskpd  , Z))       // AVX  | SSE2

  V_EMIT_VVI_VVI(v_insert_f32_     , PACK_AVX_SSE(Vinsertps  , Insertps  , X))       // AVX  | SSE4_1
  V_EMIT_VVI_VVI(v_extract_f32_    , PACK_AVX_SSE(Vextractps , Extractps , X))       // AVX  | SSE4_1

  V_EMIT_VVV_VV(s_add_f32          , PACK_AVX_SSE(Vaddss     , Addss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_add_f64          , PACK_AVX_SSE(Vaddsd     , Addsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_sub_f32          , PACK_AVX_SSE(Vsubss     , Subss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_sub_f64          , PACK_AVX_SSE(Vsubsd     , Subsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_mul_f32          , PACK_AVX_SSE(Vmulss     , Mulss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_mul_f64          , PACK_AVX_SSE(Vmulsd     , Mulsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_div_f32          , PACK_AVX_SSE(Vdivss     , Divss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_div_f64          , PACK_AVX_SSE(Vdivsd     , Divsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_min_f32          , PACK_AVX_SSE(Vminss     , Minss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_min_f64          , PACK_AVX_SSE(Vminsd     , Minsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_max_f32          , PACK_AVX_SSE(Vmaxss     , Maxss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_max_f64          , PACK_AVX_SSE(Vmaxsd     , Maxsd     , X))       // AVX  | SSE2

  V_EMIT_VVV_VV(s_rcp_f32          , PACK_AVX_SSE(Vrcpss     , Rcpss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_rsqrt_f32        , PACK_AVX_SSE(Vrsqrtss   , Rsqrtss   , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_sqrt_f32         , PACK_AVX_SSE(Vsqrtss    , Sqrtss    , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_sqrt_f64         , PACK_AVX_SSE(Vsqrtsd    , Sqrtsd    , X))       // AVX  | SSE2

  V_EMIT_VVVI_VVI_ENUM(s_round_f32_, PACK_AVX_SSE(Vroundss   , Roundss   , X), x86::RoundImm) // AVX  | SSE4.1
  V_EMIT_VVVI_VVI_ENUM(s_round_f64_, PACK_AVX_SSE(Vroundsd   , Roundsd   , X), x86::RoundImm) // AVX  | SSE4.1
  V_EMIT_VVVI_VVI_ENUM(s_cmp_f32   , PACK_AVX_SSE(Vcmpss     , Cmpss     , X), x86::VCmpImm)  // AVX  | SSE
  V_EMIT_VVVI_VVI_ENUM(s_cmp_f64   , PACK_AVX_SSE(Vcmpsd     , Cmpsd     , X), x86::VCmpImm)  // AVX  | SSE2

  V_EMIT_VVV_VV(v_add_f32          , PACK_AVX_SSE(Vaddps     , Addps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_add_f64          , PACK_AVX_SSE(Vaddpd     , Addpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_sub_f32          , PACK_AVX_SSE(Vsubps     , Subps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_sub_f64          , PACK_AVX_SSE(Vsubpd     , Subpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_mul_f32          , PACK_AVX_SSE(Vmulps     , Mulps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_mul_f64          , PACK_AVX_SSE(Vmulpd     , Mulpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_div_f32          , PACK_AVX_SSE(Vdivps     , Divps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_div_f64          , PACK_AVX_SSE(Vdivpd     , Divpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_min_f32          , PACK_AVX_SSE(Vminps     , Minps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_min_f64          , PACK_AVX_SSE(Vminpd     , Minpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_max_f32          , PACK_AVX_SSE(Vmaxps     , Maxps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_max_f64          , PACK_AVX_SSE(Vmaxpd     , Maxpd     , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(v_rcp_f32           , PACK_AVX_SSE(Vrcpps     , Rcpps     , Z))       // AVX  | SSE
  V_EMIT_VV_VV(v_rsqrt_f32         , PACK_AVX_SSE(Vrsqrtps   , Rsqrtps   , Z))       // AVX  | SSE
  V_EMIT_VV_VV(v_sqrt_f32          , PACK_AVX_SSE(Vsqrtps    , Sqrtps    , Z))       // AVX  | SSE
  V_EMIT_VV_VV(v_sqrt_f64          , PACK_AVX_SSE(Vsqrtpd    , Sqrtpd    , Z))       // AVX  | SSE2

  V_EMIT_VVI_VVI_ENUM(v_round_f32_ , PACK_AVX_SSE(Vroundps   , Roundps   , Z), x86::RoundImm) // AVX  | SSE4.1
  V_EMIT_VVI_VVI_ENUM(v_round_f64_ , PACK_AVX_SSE(Vroundpd   , Roundpd   , Z), x86::RoundImm) // AVX  | SSE4.1
  V_EMIT_VVVI_VVI_ENUM(v_cmp_f32   , PACK_AVX_SSE(Vcmpps     , Cmpps     , Z), x86::VCmpImm)  // AVX  | SSE
  V_EMIT_VVVI_VVI_ENUM(v_cmp_f64   , PACK_AVX_SSE(Vcmppd     , Cmppd     , Z), x86::VCmpImm)  // AVX  | SSE2

  V_EMIT_VVV_VV(v_addsub_f32_      , PACK_AVX_SSE(Vaddsubps  , Addsubps  , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(v_addsub_f64_      , PACK_AVX_SSE(Vaddsubpd  , Addsubpd  , Z))       // AVX  | SSE3
  V_EMIT_VVVI_VVI(v_dot_f32_       , PACK_AVX_SSE(Vdpps      , Dpps      , Z))       // AVX  | SSE4.1
  V_EMIT_VVVI_VVI(v_dot_f64_       , PACK_AVX_SSE(Vdppd      , Dppd      , Z))       // AVX  | SSE4.1

  V_EMIT_VVVV_VVV(v_blendv_f32_    , PACK_AVX_SSE(Vblendvps  , Blendvps  , Z))       // AVX  | SSE4.1
  V_EMIT_VVVV_VVV(v_blendv_f64_    , PACK_AVX_SSE(Vblendvpd  , Blendvpd  , Z))       // AVX  | SSE4.1
  V_EMIT_VVVI_VVI(v_blend_f32_     , PACK_AVX_SSE(Vblendps   , Blendps   , Z))       // AVX  | SSE4.1
  V_EMIT_VVVI_VVI(v_blend_f64_     , PACK_AVX_SSE(Vblendpd   , Blendpd   , Z))       // AVX  | SSE4.1

  V_EMIT_VVV_VV(s_cvt_f32_f64      , PACK_AVX_SSE(Vcvtss2sd  , Cvtss2sd  , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_cvt_f64_f32      , PACK_AVX_SSE(Vcvtsd2ss  , Cvtsd2ss  , X))       // AVX  | SSE2

  V_EMIT_VVV_VV(s_cvt_int_f32      , PACK_AVX_SSE(Vcvtsi2ss  , Cvtsi2ss  , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_cvt_int_f64      , PACK_AVX_SSE(Vcvtsi2sd  , Cvtsi2sd  , X))       // AVX  | SSE2

  V_EMIT_VV_VV(s_cvt_f32_int       , PACK_AVX_SSE(Vcvtss2si  , Cvtss2si  , X))       // AVX  | SSE
  V_EMIT_VV_VV(s_cvt_f64_int       , PACK_AVX_SSE(Vcvtsd2si  , Cvtsd2si  , X))       // AVX  | SSE2

  V_EMIT_VV_VV(s_cvtt_f32_int      , PACK_AVX_SSE(Vcvttss2si , Cvttss2si , X))       // AVX  | SSE
  V_EMIT_VV_VV(s_cvtt_f64_int      , PACK_AVX_SSE(Vcvttsd2si , Cvttsd2si , X))       // AVX  | SSE2

  V_EMIT_VV_VV(v_cvt_f32_f64       , PACK_AVX_SSE(Vcvtps2pd  , Cvtps2pd  , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(v_cvt_f64_f32       , PACK_AVX_SSE(Vcvtpd2ps  , Cvtpd2ps  , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(v_cvt_i32_f32       , PACK_AVX_SSE(Vcvtdq2ps  , Cvtdq2ps  , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(v_cvt_i32_f64       , PACK_AVX_SSE(Vcvtdq2pd  , Cvtdq2pd  , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(v_cvt_f32_i32       , PACK_AVX_SSE(Vcvtps2dq  , Cvtps2dq  , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(v_cvt_f64_i32       , PACK_AVX_SSE(Vcvtpd2dq  , Cvtpd2dq  , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(v_cvtt_f32_i32      , PACK_AVX_SSE(Vcvttps2dq , Cvttps2dq , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(v_cvtt_f64_i32      , PACK_AVX_SSE(Vcvttpd2dq , Cvttpd2dq , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(v_hadd_f32_        , PACK_AVX_SSE(Vhaddps    , Haddps    , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(v_hadd_f64_        , PACK_AVX_SSE(Vhaddpd    , Haddpd    , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(v_hsub_f32_        , PACK_AVX_SSE(Vhsubps    , Hsubps    , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(v_hsub_f64_        , PACK_AVX_SSE(Vhsubpd    , Hsubpd    , Z))       // AVX  | SSE3

  // Floating Point - Miscellaneous.

  V_EMIT_VV_VV(s_comi_f32          , PACK_AVX_SSE(Vcomiss    , Comiss    , X))       // AVX  | SSE
  V_EMIT_VV_VV(s_comi_f64          , PACK_AVX_SSE(Vcomisd    , Comisd    , X))       // AVX  | SSE2
  V_EMIT_VV_VV(s_ucomi_f32         , PACK_AVX_SSE(Vucomiss   , Ucomiss   , X))       // AVX  | SSE
  V_EMIT_VV_VV(s_ucomi_f64         , PACK_AVX_SSE(Vucomisd   , Ucomisd   , X))       // AVX  | SSE2

  // AVX2 and AVX-512.

  V_EMIT_VVI_VVI(v_extract_i128    , PACK_AVX_SSE(Vextracti128 , None    , Y))       // AVX2
  V_EMIT_VVI_VVI(v_extract_i256    , PACK_AVX_SSE(Vextracti32x8, None    , Z))       // AVX2
  V_EMIT_VVI_VVI(v_perm_i64        , PACK_AVX_SSE(Vpermq       , None    , Z))       // AVX2 | AVX512

  V_EMIT_VVVI(v_insert_i256        , PACK_AVX_SSE(Vinserti32x8 , None    , Z))       // AVX2
  V_EMIT_VVVI(v_insert_i128        , PACK_AVX_SSE(Vinserti128  , None    , Y))       // AVX512

  // FMA

  // 132 = Dst = Dst * Src2 + Src1
  // 213 = Dst = Dst * Src1 + Src2
  // 231 = Dst = Dst + Src1 * Src1

  V_EMIT_VVV_VV(s_fmadd_132_f32_   , PACK_AVX_SSE(Vfmadd132ss  , None    , X))       // FMA
  V_EMIT_VVV_VV(s_fmadd_213_f32_   , PACK_AVX_SSE(Vfmadd213ss  , None    , X))       // FMA
  V_EMIT_VVV_VV(s_fmadd_231_f32_   , PACK_AVX_SSE(Vfmadd231ss  , None    , X))       // FMA
  V_EMIT_VVV_VV(s_fmsub_132_f32_   , PACK_AVX_SSE(Vfmsub132ss  , None    , X))       // FMA
  V_EMIT_VVV_VV(s_fmsub_213_f32_   , PACK_AVX_SSE(Vfmsub213ss  , None    , X))       // FMA
  V_EMIT_VVV_VV(s_fmsub_231_f32_   , PACK_AVX_SSE(Vfmsub231ss  , None    , X))       // FMA

  V_EMIT_VVV_VV(v_fmadd_132_f32_   , PACK_AVX_SSE(Vfmadd132ps  , None    , Z))       // FMA
  V_EMIT_VVV_VV(v_fmadd_213_f32_   , PACK_AVX_SSE(Vfmadd213ps  , None    , Z))       // FMA
  V_EMIT_VVV_VV(v_fmadd_231_f32_   , PACK_AVX_SSE(Vfmadd231ps  , None    , Z))       // FMA
  V_EMIT_VVV_VV(v_fmsub_132_f32_   , PACK_AVX_SSE(Vfmsub132ps  , None    , Z))       // FMA
  V_EMIT_VVV_VV(v_fmsub_213_f32_   , PACK_AVX_SSE(Vfmsub213ps  , None    , Z))       // FMA
  V_EMIT_VVV_VV(v_fmsub_231_f32_   , PACK_AVX_SSE(Vfmsub231ps  , None    , Z))       // FMA

  V_EMIT_VVV_VV(s_fnmadd_132_f32_  , PACK_AVX_SSE(Vfnmadd132ss , None    , X))       // FMA
  V_EMIT_VVV_VV(s_fnmadd_213_f32_  , PACK_AVX_SSE(Vfnmadd213ss , None    , X))       // FMA
  V_EMIT_VVV_VV(s_fnmadd_231_f32_  , PACK_AVX_SSE(Vfnmadd231ss , None    , X))       // FMA
  V_EMIT_VVV_VV(s_fnmsub_132_f32_  , PACK_AVX_SSE(Vfnmsub132ss , None    , X))       // FMA
  V_EMIT_VVV_VV(s_fnmsub_213_f32_  , PACK_AVX_SSE(Vfnmsub213ss , None    , X))       // FMA
  V_EMIT_VVV_VV(s_fnmsub_231_f32_  , PACK_AVX_SSE(Vfnmsub231ss , None    , X))       // FMA

  V_EMIT_VVV_VV(v_fnmadd_132_f32_  , PACK_AVX_SSE(Vfnmadd132ps , None    , Z))       // FMA
  V_EMIT_VVV_VV(v_fnmadd_213_f32_  , PACK_AVX_SSE(Vfnmadd213ps , None    , Z))       // FMA
  V_EMIT_VVV_VV(v_fnmadd_231_f32_  , PACK_AVX_SSE(Vfnmadd231ps , None    , Z))       // FMA
  V_EMIT_VVV_VV(v_fnmsub_132_f32_  , PACK_AVX_SSE(Vfnmsub132ps , None    , Z))       // FMA
  V_EMIT_VVV_VV(v_fnmsub_213_f32_  , PACK_AVX_SSE(Vfnmsub213ps , None    , Z))       // FMA
  V_EMIT_VVV_VV(v_fnmsub_231_f32_  , PACK_AVX_SSE(Vfnmsub231ps , None    , Z))       // FMA


  // Initialization.

  inline void v_zero_i(const Operand_& dst) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vpxor , Pxor , Z), dst, dst, dst); }
  inline void v_zero_f(const Operand_& dst) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vxorps, Xorps, Z), dst, dst, dst); }
  inline void v_zero_d(const Operand_& dst) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vxorpd, Xorpd, Z), dst, dst, dst); }

  inline void v_zero_i(const OpArray& dst) noexcept { for (uint32_t i = 0; i < dst.size(); i++) v_zero_i(dst[i]); }
  inline void v_zero_f(const OpArray& dst) noexcept { for (uint32_t i = 0; i < dst.size(); i++) v_zero_f(dst[i]); }
  inline void v_zero_d(const OpArray& dst) noexcept { for (uint32_t i = 0; i < dst.size(); i++) v_zero_d(dst[i]); }

  BL_NOINLINE void v_ones_i(const Operand_& dst) noexcept {
    /*
    if (hasAVX512())
      cc->vpternlogd(dst, dst, dst, 0xFFu);
    */
    v_emit_vvv_vv(PACK_AVX_SSE(Vpcmpeqb, Pcmpeqb, Z), dst, dst, dst);
  }

  // Conversion.

  inline void s_mov_i32(const Vec& dst, const Gp& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void s_mov_i64(const Vec& dst, const Gp& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }

  inline void s_mov_i32(const Gp& dst, const Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void s_mov_i64(const Gp& dst, const Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }

  //! \}

  //! \name Memory Loads & Stores
  //! \{

  BL_NOINLINE void v_load_i8(const Operand_& dst, const Mem& src) noexcept {
    Xmm dst_xmm = dst.as<Vec>().xmm();
    if (hasSSE4_1()) {
      v_zero_i(dst_xmm);
      v_insert_u8_(dst_xmm, dst_xmm, src, 0);
    }
    else {
      Gp tmp = newGp32("@tmp");
      load_u8(tmp, src);
      s_mov_i32(dst_xmm, tmp);
    }
  }

  BL_NOINLINE void v_load_i16(const Operand_& dst, const Mem& src) noexcept {
    Xmm dst_xmm = dst.as<Vec>().xmm();
    if (hasSSE4_1()) {
      v_zero_i(dst_xmm);
      v_insert_u16(dst_xmm, dst_xmm, src, 0);
    }
    else {
      Gp tmp = newGp32("@tmp");
      load_u16(tmp, src);
      s_mov_i32(dst_xmm, tmp);
    }
  }

  BL_NOINLINE void v_load_u8_u16_2x(const Operand_& dst, const Mem& lo, const Mem& hi) noexcept {
    Gp reg = newGp32("@tmp");
    Mem mLo(lo);
    Mem mHi(hi);

    mLo.setSize(1);
    mHi.setSize(1);

    cc->movzx(reg, mHi);
    cc->shl(reg, 16);
    cc->mov(reg.r8(), mLo);
    s_mov_i32(dst.as<Vec>().xmm(), reg);
  }

  inline void v_load_i32(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void v_load_i64(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }
  inline void v_load_f32(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovss, Movss, X), dst, src); }
  inline void v_load_f64(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovsd, Movsd, X), dst, src); }

  inline void v_loadl_2xf32(const Operand_& dst, const Operand_& src1, const Mem& src2) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vmovlps, Movlps, X), dst, src1, src2); }
  inline void v_loadh_2xf32(const Operand_& dst, const Operand_& src1, const Mem& src2) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vmovhps, Movhps, X), dst, src1, src2); }

  inline void v_loadl_f64(const Operand_& dst, const Operand_& src1, const Mem& src2) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vmovlpd, Movlpd, X), dst, src1, src2); }
  inline void v_loadh_f64(const Operand_& dst, const Operand_& src1, const Mem& src2) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vmovhpd, Movhpd, X), dst, src1, src2); }

  inline void v_loada_i128(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, X), dst, src); }
  inline void v_loadu_i128(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, X), dst, src); }
  inline void v_loadu_i128_ro(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vloadi128uRO), dst, src); }

  inline void v_loada_f128(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, X), dst, src); }
  inline void v_loadu_f128(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, X), dst, src); }
  inline void v_loada_d128(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, Movaps, X), dst, src); }
  inline void v_loadu_d128(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, Movups, X), dst, src); }

  inline void v_load_i128(const Operand_& dst, const Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovdqu, Movups, X), PACK_AVX_SSE(Vmovdqa, Movaps, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_load_f128(const Operand_& dst, const Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, Movups, X), PACK_AVX_SSE(Vmovaps, Movaps, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_load_d128(const Operand_& dst, const Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, Movupd, X), PACK_AVX_SSE(Vmovapd, Movapd, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_loada_i256(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, Y), dst, src); }
  inline void v_loadu_i256(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, Y), dst, src); }
  inline void v_loadu_i256_ro(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vloadi128uRO), dst, src); }

  inline void v_loada_f256(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, Y), dst, src); }
  inline void v_loadu_f256(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, Y), dst, src); }
  inline void v_loada_d256(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, Movaps, Y), dst, src); }
  inline void v_loadu_d256(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, Movups, Y), dst, src); }

  inline void v_load_i256(const Operand_& dst, const Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovdqu, None, Y), PACK_AVX_SSE(Vmovdqa, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  inline void v_load_f256(const Operand_& dst, const Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, None, Y), PACK_AVX_SSE(Vmovaps, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  inline void v_load_d256(const Operand_& dst, const Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, None, Y), PACK_AVX_SSE(Vmovapd, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  BL_NOINLINE void v_store_i8(const Mem& dst, const Vec& src) noexcept {
    if (hasSSE4_1()) {
      v_emit_vvi_vvi(PACK_AVX_SSE(Vpextrb, Pextrb, X), dst, src, 0);
    }
    else {
      Gp tVal = newGp32("tVal");
      cc->movd(tVal, src.as<Xmm>());
      cc->mov(dst, tVal.r8());
    }
  }

  BL_NOINLINE void v_store_i16(const Mem& dst, const Vec& src) noexcept {
    if (hasSSE4_1()) {
      v_emit_vvi_vvi(PACK_AVX_SSE(Vpextrw, Pextrw, X), dst, src, 0);
    }
    else {
      Gp tVal = newGp32("tVal");
      cc->movd(tVal, src.as<Xmm>());
      cc->mov(dst, tVal.r16());
    }
  }

  inline void v_store_i32(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void v_store_i64(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }

  inline void v_store_f32(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovss, Movss, X), dst, src); }
  inline void v_store_f64(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovsd, Movsd, X), dst, src); }

  inline void v_storel_2xf32(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovlps, Movlps, X), dst, src); }
  inline void v_storeh_2xf32(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovhps, Movhps, X), dst, src); }

  inline void v_storel_f64(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovsd , Movsd , X), dst, src); }
  inline void v_storeh_f64(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovhpd, Movhpd, X), dst, src); }

  inline void v_storea_i128(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, X), dst, src); }
  inline void v_storeu_i128(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, X), dst, src); }
  inline void v_storea_f128(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, X), dst, src); }
  inline void v_storeu_f128(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, X), dst, src); }
  inline void v_storea_d128(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, Movaps, X), dst, src); }
  inline void v_storeu_d128(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, Movups, X), dst, src); }

  inline void v_store_i128(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovdqu, Movups, X), PACK_AVX_SSE(Vmovdqa, Movaps, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_store_f128(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, Movups, X), PACK_AVX_SSE(Vmovaps, Movaps, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_store_d128(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, Movupd, X), PACK_AVX_SSE(Vmovapd, Movapd, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_storea_i256(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, None, Y), dst, src); }
  inline void v_storeu_i256(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, None, Y), dst, src); }
  inline void v_storea_f256(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, None, Y), dst, src); }
  inline void v_storeu_f256(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, None, Y), dst, src); }
  inline void v_storea_d256(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, None, Y), dst, src); }
  inline void v_storeu_d256(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, None, Y), dst, src); }

  inline void v_store_i256(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovdqu, None, Y), PACK_AVX_SSE(Vmovdqa, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  inline void v_store_f256(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, None, Y), PACK_AVX_SSE(Vmovaps, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  inline void v_store_d256(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, None, Y), PACK_AVX_SSE(Vmovapd, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  inline void v_storea_i512(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa32, None, Z), dst, src); }
  inline void v_storeu_i512(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu32, None, Z), dst, src); }
  inline void v_storea_f512(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, None, Z), dst, src); }
  inline void v_storeu_f512(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, None, Z), dst, src); }
  inline void v_storea_d512(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, None, Z), dst, src); }
  inline void v_storeu_d512(const Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, None, Z), dst, src); }

  inline void v_store_i512(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovdqu32, None, Z), PACK_AVX_SSE(Vmovdqa32, None, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 64)], dst, src);
  }

  inline void v_store_f512(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, None, Z), PACK_AVX_SSE(Vmovaps, None, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 64)], dst, src);
  }

  inline void v_store_d512(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, None, Z), PACK_AVX_SSE(Vmovapd, None, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 64)], dst, src);
  }

  //! \}

  //! \name Memory Loads & Stores with Packing and Unpacking
  //! \{

  inline void v_load_i64_u8u16_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxbw, Pmovzxbw, X), dst, src); }
  inline void v_load_i32_u8u32_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxbd, Pmovzxbd, X), dst, src); }
  inline void v_load_i16_u8u64_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxbq, Pmovzxbq, X), dst, src); }
  inline void v_load_i64_u16u32_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxwd, Pmovzxwd, X), dst, src); }
  inline void v_load_i32_u16u64_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxwq, Pmovzxwq, X), dst, src); }
  inline void v_load_i64_u32u64_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxdq, Pmovzxdq, X), dst, src); }

  inline void v_load_i64_i8i16_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxbw, Pmovsxbw, X), dst, src); }
  inline void v_load_i32_i8i32_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxbd, Pmovsxbd, X), dst, src); }
  inline void v_load_i16_i8i64_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxbq, Pmovsxbq, X), dst, src); }
  inline void v_load_i64_i16i32_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxwd, Pmovsxwd, X), dst, src); }
  inline void v_load_i32_i16i64_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxwq, Pmovsxwq, X), dst, src); }
  inline void v_load_i64_i32i64_(const Operand_& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxdq, Pmovsxdq, X), dst, src); }

  //! \}

  //! \name Memory Loads & Stores of Vector Sizes
  //! \{

  BL_NOINLINE void v_loada_ivec(const Vec& dst, const Mem& src) noexcept {
    if (dst.isZmm())
      cc->vmovdqa32(dst, src);
    else
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, Z), dst, src);
  }

  inline void v_loada_fvec(const Vec& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, Z), dst, src); }
  inline void v_loada_dvec(const Vec& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, Movapd, Z), dst, src); }

  BL_NOINLINE void v_loadu_ivec(const Vec& dst, const Mem& src) noexcept {
    if (dst.isZmm())
      cc->vmovdqu32(dst, src);
    else
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, Z), dst, src);
  }

  inline void v_loadu_fvec(const Vec& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, Z), dst, src); }
  inline void v_loadu_dvec(const Vec& dst, const Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, Movupd, Z), dst, src); }

  BL_NOINLINE void v_load_ivec(const Vec& dst, const Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable512[2] = { PACK_AVX_SSE(Vmovdqu32, None, Z), PACK_AVX_SSE(Vmovdqa32, None, Z) };
    static const uint32_t kMovTable256[2] = { PACK_AVX_SSE(Vmovdqu, Movups, Z), PACK_AVX_SSE(Vmovdqa, Movaps, Z) };

    if (dst.isZmm())
      v_emit_vv_vv(kMovTable512[size_t(alignment >= dst.size())], dst, src);
    else
      v_emit_vv_vv(kMovTable256[size_t(alignment >= dst.size())], dst, src);
  }

  inline void v_load_fvec(const Vec& dst, const Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, Movups, Z), PACK_AVX_SSE(Vmovaps, Movaps, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= dst.size())], dst, src);
  }

  inline void v_load_dvec(const Vec& dst, const Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, Movupd, Z), PACK_AVX_SSE(Vmovapd, Movapd, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= dst.size())], dst, src);
  }

  BL_NOINLINE void v_storea_ivec(const Mem& dst, const Vec& src) noexcept {
    if (src.isZmm())
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa32, None, Z), dst, src);
    else
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, Z), dst, src);
  }

  inline void v_storea_fvec(const Mem& dst, const Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, Z), dst, src); }
  inline void v_storea_dvec(const Mem& dst, const Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, Movapd, Z), dst, src); }

  BL_NOINLINE void v_storeu_ivec(const Mem& dst, const Vec& src) noexcept {
    if (src.isZmm())
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu32, None, Z), dst, src);
    else
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, Z), dst, src);
  }

  inline void v_storeu_fvec(const Mem& dst, const Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, Z), dst, src); }
  inline void v_storeu_dvec(const Mem& dst, const Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, Movupd, Z), dst, src); }

  BL_NOINLINE void v_store_ivec(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable512[2] = { PACK_AVX_SSE(Vmovdqu32, None, Z), PACK_AVX_SSE(Vmovdqa32, None, Z) };
    static const uint32_t kMovTable256[2] = { PACK_AVX_SSE(Vmovdqu, Movups, Z), PACK_AVX_SSE(Vmovdqa, Movaps, Z) };

    if (src.isZmm())
      v_emit_vv_vv(kMovTable512[size_t(alignment >= src.size())], dst, src);
    else
      v_emit_vv_vv(kMovTable256[size_t(alignment >= src.size())], dst, src);
  }

  inline void v_store_fvec(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, Movups, Z), PACK_AVX_SSE(Vmovaps, Movaps, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= src.size())], dst, src);
  }

  inline void v_store_dvec(const Mem& dst, const Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, Movupd, Z), PACK_AVX_SSE(Vmovapd, Movapd, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= src.size())], dst, src);
  }

  inline void v_load_ivec_array(VecArray& dst, const Mem& src, Alignment alignment) noexcept {
    Mem m = src;
    for (size_t i = 0; i < dst.size(); i++) {
      v_load_ivec(dst[i], m, alignment);
      m.addOffsetLo32(dst[i].size());
    }
  }

  inline void v_store_ivec_array(const Mem& dst, const VecArray& src, Alignment alignment) noexcept {
    Mem m = dst;
    for (size_t i = 0; i < src.size(); i++) {
      v_store_ivec(m, src[i], alignment);
      m.addOffsetLo32(src[i].size());
    }
  }

  //! \}

  //! \name Memory Loads & Stores with Parameterized Size
  //! \{

  BL_NOINLINE void v_load_iany(const Vec& dst, const Mem& src, uint32_t nBytes, Alignment alignment) noexcept {
    switch (nBytes) {
      case 1: v_load_i8(dst, src); break;
      case 2: v_load_i16(dst, src); break;
      case 4: v_load_i32(dst, src); break;
      case 8: v_load_i64(dst, src); break;
      case 16: v_load_i128(dst, src, alignment); break;
      case 32: v_load_i256(dst, src, alignment); break;

      default:
        BL_NOT_REACHED();
    }
  }

  BL_NOINLINE void v_store_iany(const Mem& dst, const Vec& src, uint32_t nBytes, Alignment alignment) noexcept {
    switch (nBytes) {
      case 1: v_store_i8(dst, src); break;
      case 2: v_store_i16(dst, src); break;
      case 4: v_store_i32(dst, src); break;
      case 8: v_store_i64(dst, src); break;
      case 16: v_store_i128(dst, src, alignment); break;
      case 32: v_store_i256(dst, src, alignment); break;

      default:
        BL_NOT_REACHED();
    }
  }

  //! \}

  //! \name Memory Loads & Stores with Predicate
  //! \{

  BL_NOINLINE void v_load_predicated_v8(const Vec& dst, const PixelPredicate& pred, const Mem& src) noexcept {
    BL_ASSERT(pred.k.isValid());
    cc->k(pred.k).z().vmovdqu8(dst, src);
  }

  BL_NOINLINE void v_load_predicated_v32(const Vec& dst, const PixelPredicate& pred, const Mem& src) noexcept {
    if (pred.k.isValid())
      cc->k(pred.k).z().vmovdqu32(dst, src);
    else if (hasAVX2())
      cc->vpmaskmovd(dst, pred.v32, src);
    else
      cc->vmaskmovps(dst, pred.v32, src);
  }

  BL_NOINLINE void v_load_predicated_v64(const Vec& dst, const PixelPredicate& pred, const Mem& src) noexcept {
    if (pred.k.isValid())
      cc->k(pred.k).z().vmovdqu64(dst, src);
    else if (hasAVX2())
      cc->vpmaskmovq(dst, pred.v64, src);
    else
      cc->vmaskmovpd(dst, pred.v64, src);
  }

  BL_NOINLINE void v_store_predicated_v8(const Mem& dst, const PixelPredicate& pred, const Vec& src) noexcept {
    BL_ASSERT(pred.k.isValid());
    cc->k(pred.k).vmovdqu8(dst, src);
  }

  BL_NOINLINE void v_store_predicated_v32(const Mem& dst, const PixelPredicate& pred, const Vec& src) noexcept {
    if (pred.k.isValid())
      cc->k(pred.k).vmovdqu32(dst, src);
    else if (hasAVX2())
      cc->vpmaskmovd(dst, pred.v32, src);
    else
      cc->vmaskmovps(dst, pred.v32, src);
  }

  BL_NOINLINE void v_store_predicated_v64(const Mem& dst, const PixelPredicate& pred, const Vec& src) noexcept {
    if (pred.k.isValid())
      cc->k(pred.k).vmovdqu64(dst, src);
    else if (hasAVX2())
      cc->vpmaskmovq(dst, pred.v64, src);
    else
      cc->vmaskmovpd(dst, pred.v64, src);
  }

  //! \}

  // Intrinsics:
  //
  //   - v_mov{x}{y}   - Move with sign or zero extension from {x} to {y}. Similar to instructions like `pmovzx..`,
  //                     `pmovsx..`, and `punpckl..`
  //
  //   - v_swap{x}     - Swap low and high elements. If the vector has more than 2 elements it's divided into 2
  //                     element vectors in which the operation is performed separately.
  //
  //   - v_dup{l|h}{x} - Duplicate either low or high element into both. If there are more than 2 elements in the
  //                     vector it's considered they are separate units. For example a 4-element vector can be
  //                     considered as 2 2-element vectors on which the duplication operation is performed.

  template<typename DstT, typename SrcT>
  inline void v_mov_u8_u16(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vmovu8u16), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_mov_u8_u32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vmovu8u32), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_mov_u16_u32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vmovu16u32), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_abs_i8(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi8), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_abs_i16(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi16), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_abs_i32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi32), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_abs_i64(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi64), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_swap_u32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u32(dst, src, x86::shuffleImm(2, 3, 0, 1)); }

  template<typename DstT, typename SrcT>
  inline void v_swap_u64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u32(dst, src, x86::shuffleImm(1, 0, 3, 2)); }

  template<typename DstT, typename SrcT>
  inline void v_dupl_i32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u32(dst, src, x86::shuffleImm(2, 2, 0, 0)); }

  template<typename DstT, typename SrcT>
  inline void v_duph_i32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u32(dst, src, x86::shuffleImm(3, 3, 1, 1)); }

  template<typename DstT, typename SrcT>
  inline void v_dupl_i64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u32(dst, src, x86::shuffleImm(1, 0, 1, 0)); }

  template<typename DstT, typename SrcT>
  inline void v_duph_i64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_u32(dst, src, x86::shuffleImm(3, 2, 3, 2)); }

  // Dst = (CondBit == 0) ? Src1 : Src2;
  template<typename DstT, typename Src1T, typename Src2T, typename CondT>
  inline void v_blendv_u8(const DstT& dst, const Src1T& src1, const Src2T& src2, const CondT& cond) noexcept {
    v_emit_vvvv_vvv(PackedInst::packIntrin(kIntrin4Vpblendvb), dst, src1, src2, cond);
  }

  // Dst = (CondBit == 0) ? Src1 : Src2;
  template<typename DstT, typename Src1T, typename Src2T, typename CondT>
  inline void v_blendv_u8_destructive(const DstT& dst, const Src1T& src1, const Src2T& src2, const CondT& cond) noexcept {
    v_emit_vvvv_vvv(PackedInst::packIntrin(kIntrin4VpblendvbDestructive), dst, src1, src2, cond);
  }

  template<typename DstT, typename SrcT>
  inline void v_inv255_u16(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv255u16), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_inv256_u16(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv256u16), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_inv255_u32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv255u32), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_inv256_u32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv256u32), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_dupl_f64(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vduplpd), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_duph_f64(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vduphpd), dst, src); }

  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_hadd_f64(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept { v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vhaddpd), dst, src1, src2); }

  template<typename DstT, typename SrcT>
  inline void v_expand_lo_i32(const DstT& dst, const SrcT& src) noexcept {
    v_swizzle_u32(dst, src, x86::shuffleImm(0, 0, 0, 0));
  }

  // dst.u64[0] = src1.u64[1];
  // dst.u64[1] = src2.u64[0];
  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_combine_hl_i64(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vcombhli64), dst, src1, src2);
  }

  // dst.d64[0] = src1.d64[1];
  // dst.d64[1] = src2.d64[0];
  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_combine_hl_f64(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vcombhld64), dst, src1, src2);
  }

  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_min_u16(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vminu16), dst, src1, src2);
  }

  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_max_u16(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vmaxu16), dst, src1, src2);
  }

  // Multiplies packed uint64_t in `src1` with packed low uint32_t int `src2`.
  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_mul_u64_u32_lo(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vmulu64x32), dst, src1, src2);
  }

  template<typename DstT, typename SrcT>
  BL_NOINLINE void v_mul257_hi_u16(const DstT& dst, const SrcT& src) {
    v_mulh_u16(dst, src, simdConst(&commonTable.i_0101010101010101, Bcst::kNA, dst));
  }

  // TODO: [PIPEGEN] Consolidate this to only one implementation.
  template<typename DstSrcT>
  BL_NOINLINE void v_div255_u16(const DstSrcT& x) {
    v_add_i16(x, x, simdConst(&commonTable.i_0080008000800080, Bcst::kNA, x));
    v_mul257_hi_u16(x, x);
  }

  template<typename DstSrcT>
  BL_NOINLINE void v_div255_u16_2x(const DstSrcT& v0, const DstSrcT& v1) noexcept {
    Operand i_0080008000800080 = simdConst(&commonTable.i_0080008000800080, Bcst::kNA, v0);
    Operand i_0101010101010101 = simdConst(&commonTable.i_0101010101010101, Bcst::kNA, v0);

    v_add_i16(v0, v0, i_0080008000800080);
    v_add_i16(v1, v1, i_0080008000800080);

    v_mulh_u16(v0, v0, i_0101010101010101);
    v_mulh_u16(v1, v1, i_0101010101010101);
  }

  template<typename DstT, typename SrcT>
  inline void v_expand_lo_ps(const DstT& dst, const SrcT& src) noexcept {
    v_expand_lo_i32(dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void v_swizzle_f32(const DstT& dst, const SrcT& src, uint32_t imm) noexcept { v_emit_vvi_vi(PackedInst::packIntrin(kIntrin2iVswizps), dst, src, imm); }
  template<typename DstT, typename SrcT>
  inline void v_swizzle_f64(const DstT& dst, const SrcT& src, uint32_t imm) noexcept { v_emit_vvi_vi(PackedInst::packIntrin(kIntrin2iVswizpd), dst, src, imm); }

  template<typename DstT, typename SrcT>
  inline void v_swap_f32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_f32(dst, src, x86::shuffleImm(2, 3, 0, 1)); }
  template<typename DstT, typename SrcT>
  inline void v_swap_f64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_f64(dst, src, x86::shuffleImm(0, 1)); }

  template<typename DstT, typename SrcT>
  inline void v_broadcast_u8(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU8), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_u16(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU16), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_u32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU32), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_u64(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU64), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_broadcast_u32x4(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastI32x4), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_u64x2(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastI64x2), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_f32x4(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastF32x4), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_f64x2(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastF64x2), dst, src); }


  template<typename DstT, typename Src1T, typename Src2T>
  BL_INLINE void v_min_or_max_u8(const DstT& dst, const Src1T& src1, const Src2T& src2, bool isMin) noexcept {
    if (isMin)
      v_min_u8(dst, src1, src2);
    else
      v_max_u8(dst, src1, src2);
  }

  // d = int(floor(a / b) * b).
  template<typename VecOrMem>
  BL_NOINLINE void v_mod_pd(const Vec& d, const Vec& a, const VecOrMem& b) noexcept {
    if (hasSSE4_1()) {
      v_div_f64(d, a, b);
      v_round_f64_(d, d, x86::RoundImm::kTrunc | x86::RoundImm::kSuppress);
      v_mul_f64(d, d, b);
    }
    else {
      Xmm t = cc->newXmm("vModTmp");

      v_div_f64(d, a, b);
      v_cvtt_f64_i32(t, d);
      v_cvt_i32_f64(t, t);
      v_cmp_f64(d, d, t, x86::VCmpImm::kLT_OS);
      v_and_f64(d, d, simdMemConst(&commonTable.f64_m1, Bcst::k64, d));
      v_add_f64(d, d, t);
      v_mul_f64(d, d, b);
    }
  }

  //! \}

  //! \name Emit - FMA
  //! \{

  template<typename DstT, typename SrcA, typename SrcB>
  inline void s_mul_13_add_2(const DstT& d1, const SrcA& a2, const SrcB& b3) noexcept {
    if (hasFMA()) {
      s_fmadd_132_f32_(d1, a2, b3);
    }
    else {
      s_mul_f32(d1, d1, b3);
      s_add_f32(d1, d1, a2);
    }
  }

  template<typename DstT, typename SrcA, typename SrcB>
  inline void v_mul_13_add_2(const DstT& d1, const SrcA& a2, const SrcB& b3) noexcept {
    if (hasFMA()) {
      v_fmadd_132_f32_(d1, a2, b3);
    }
    else {
      v_mul_f32(d1, d1, b3);
      v_add_f32(d1, d1, a2);
    }
  }

  template<typename DstT, typename SrcA, typename SrcB>
  inline void s_mul_12_add_3(const DstT& d1, const SrcA& a2, const SrcB& b3) noexcept {
    if (hasFMA()) {
      s_fmadd_213_f32_(d1, a2, b3);
    }
    else {
      s_mul_f32(d1, d1, a2);
      s_add_f32(d1, d1, b3);
    }
  }

  template<typename DstT, typename SrcA, typename SrcB>
  inline void v_mul_12_add_3(const DstT& d1, const SrcA& a2, const SrcB& b3) noexcept {
    if (hasFMA()) {
      v_fmadd_213_f32_(d1, a2, b3);
    }
    else {
      v_mul_f32(d1, d1, a2);
      v_add_f32(d1, d1, b3);
    }
  }

  template<typename DstT, typename SrcA, typename SrcB>
  inline void s_mul_13_sub_2(const DstT& d1, const SrcA& a2, const SrcB& b3) noexcept {
    if (hasFMA()) {
      s_fmsub_132_f32_(d1, a2, b3);
    }
    else {
      s_mul_f32(d1, d1, b3);
      s_sub_f32(d1, d1, a2);
    }
  }

  template<typename DstT, typename SrcA, typename SrcB>
  inline void v_mul_13_sub_2(const DstT& d1, const SrcA& a2, const SrcB& b3) noexcept {
    if (hasFMA()) {
      v_fmsub_132_f32_(d1, a2, b3);
    }
    else {
      v_mul_f32(d1, d1, b3);
      v_sub_f32(d1, d1, a2);
    }
  }

  template<typename DstT, typename SrcA, typename SrcB>
  inline void s_mul_12_sub_3(const DstT& d1, const SrcA& a2, const SrcB& b3) noexcept {
    if (hasFMA()) {
      s_fmsub_213_f32_(d1, a2, b3);
    }
    else {
      s_mul_f32(d1, d1, a2);
      s_sub_f32(d1, d1, b3);
    }
  }

  template<typename DstT, typename SrcA, typename SrcB>
  inline void v_mul_12_sub_3(const DstT& d1, const SrcA& a2, const SrcB& b3) noexcept {
    if (hasFMA()) {
      v_fmsub_213_f32_(d1, a2, b3);
    }
    else {
      v_mul_f32(d1, d1, a2);
      v_sub_f32(d1, d1, b3);
    }
  }

  //! \}

  //! \name Emit - 'X' High Level Functionality
  //! \{

  void x_make_predicate_v32(const Vec& vmask, const Gp& count) noexcept;

  void x_ensure_predicate_8(PixelPredicate& predicate, uint32_t width) noexcept;
  void x_ensure_predicate_32(PixelPredicate& predicate, uint32_t width) noexcept;

  // Kind of a hack - if we don't have SSE4.1 we have to load the byte into GP register first and then we use 'PINSRW',
  // which is provided by baseline SSE2. If we have SSE4.1 then it's much easier as we can load the byte by 'PINSRB'.
  void x_insert_word_or_byte(const Vec& dst, const Mem& src, uint32_t wordIndex) noexcept {
    Mem m = src;
    m.setSize(1);

    if (hasSSE4_1()) {
      v_insert_u8_(dst, dst, m, wordIndex * 2u);
    }
    else {
      Gp tmp = newGp32("@tmp");
      cc->movzx(tmp, m);
      v_insert_u16(dst, dst, tmp, wordIndex);
    }
  }

  void x_inline_pixel_fill_loop(Gp& dst, Vec& src, Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity) noexcept;
  void x_inline_pixel_copy_loop(Gp& dst, Gp& src, Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity, FormatExt format) noexcept;

  void _x_inline_memcpy_sequence_xmm(
    const Mem& dPtr, bool dstAligned,
    const Mem& sPtr, bool srcAligned, uint32_t numBytes, const Vec& fillMask) noexcept;

  BL_NOINLINE void x_storea_fill(Mem dst, const Vec& src, uint32_t n) noexcept {
    if (src.isZmm() && n >= 64) {
      for (uint32_t j = 0; j < n; j += 64u) {
        v_storea_i512(dst, src);
        dst.addOffsetLo32(64);
      }
    }
    else if (src.isYmm() && n >= 32) {
      for (uint32_t j = 0; j < n; j += 32u) {
        v_storea_i256(dst, src);
        dst.addOffsetLo32(32);
      }
    }
    else {
      Xmm srcXmm = src.xmm();
      for (uint32_t j = 0; j < n; j += 16u) {
        v_storea_i128(dst, srcXmm);
        dst.addOffsetLo32(16);
      }
    }
  }

  BL_NOINLINE void x_storeu_fill(Mem dst, const Vec& src_, uint32_t n) noexcept {
    Vec src = src_;

    if (src.size() > 32 && n <= 32)
      src = src.ymm();

    if (src.size() > 16 && n <= 16)
      src = src.xmm();

    uint32_t vecSize = src.size();
    for (uint32_t i = 0; i < n; i += vecSize) {
      v_storeu_ivec(dst, src);
      dst.addOffsetLo32(vecSize);
    }
  }

  //! \}

  //! \name Emit - Pixel Fetch & Store Utilities
  //! \{

  void x_fetch_mask_a8_advance(VecArray& vm, PixelCount n, PixelType pixelType, const Gp& mPtr, const Vec& globalAlpha) noexcept;

  //! Fetches `n` pixels to vector register(s) in `p` from memory location `src_`.
  void x_fetch_pixel(Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const Mem& src_, Alignment alignment) noexcept;
  void x_fetch_pixel(Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept;

  void _x_fetch_pixel_a8(Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept;
  void _x_fetch_pixel_rgba32(Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept;

  //! Makes sure that the given pixel `p` has all the requrements as specified by `flags`.
  void x_satisfy_pixel(Pixel& p, PixelFlags flags) noexcept;

  void _x_satisfy_pixel_a8(Pixel& p, PixelFlags flags) noexcept;
  void _x_satisfy_pixel_rgba32(Pixel& p, PixelFlags flags) noexcept;

  //! Makes sure that the given pixel `p` has all the requrements as specified by `flags` (solid source only).
  void x_satisfy_solid(Pixel& p, PixelFlags flags) noexcept;

  void _x_satisfy_solid_a8(Pixel& p, PixelFlags flags) noexcept;
  void _x_satisfy_solid_rgba32(Pixel& p, PixelFlags flags) noexcept;

  void _x_pack_pixel(VecArray& px, VecArray& ux, uint32_t n, const char* prefix, const char* pxName) noexcept;
  void _x_unpack_pixel(VecArray& ux, VecArray& px, uint32_t n, const char* prefix, const char* uxName) noexcept;

  void x_fetch_unpacked_a8_2x(const Vec& dst, FormatExt format, const Mem& src1, const Mem& src0) noexcept;

  void x_assign_unpacked_alpha_values(Pixel& p, PixelFlags flags, Vec& vec) noexcept;

  //! Fills alpha channel with 1.
  void x_fill_pixel_alpha(Pixel& p) noexcept;

  void x_store_pixel_advance(const Gp& dPtr, Pixel& p, PixelCount n, uint32_t bpp, Alignment alignment, PixelPredicate& predicate) noexcept;

  //! \}

  //! \name Emit - Pixel Processing Utilities
  //! \{

  //! Pack 16-bit integers to unsigned 8-bit integers in an AVX2 and AVX512 aware way.
  template<typename Dst, typename Src1, typename Src2>
  BL_NOINLINE void x_packs_i16_u8(const Dst& d, const Src1& s1, const Src2& s2) noexcept {
    if (JitUtils::isXmm(s1)) {
      v_packs_i16_u8(d, s1, s2);
    }
    else {
      const Vec& vType = JitUtils::firstOp(s1).template as<Vec>();
      v_packs_i16_u8(d, s1, s2);
      v_perm_i64(d.cloneAs(vType), d.cloneAs(vType), x86::shuffleImm(3, 1, 2, 0));
    }
  }

  BL_NOINLINE void xStorePixel(const Gp& dPtr, const Vec& vSrc, uint32_t count, uint32_t bpp, Alignment alignment) noexcept {
    v_store_iany(x86::ptr(dPtr), vSrc, count * bpp, alignment);
  }

  inline void xStore32_ARGB(const Mem& dst, const Vec& vSrc) noexcept {
    v_store_i32(dst, vSrc);
  }

  BL_NOINLINE void xMovzxBW_LoHi(const Vec& d0, const Vec& d1, const Vec& s) noexcept {
    BL_ASSERT(d0.id() != d1.id());

    if (hasSSE4_1()) {
      if (d0.id() == s.id()) {
        v_swizzle_u32(d1, d0, x86::shuffleImm(1, 0, 3, 2));
        v_mov_u8_u16_(d0, d0);
        v_mov_u8_u16_(d1, d1);
      }
      else {
        v_mov_u8_u16(d0, s);
        v_swizzle_u32(d1, s, x86::shuffleImm(1, 0, 3, 2));
        v_mov_u8_u16(d1, d1);
      }
    }
    else {
      Vec zero = simdVecConst(&commonTable.i_0000000000000000, s);
      if (d1.id() != s.id()) {
        v_interleave_hi_u8(d1, s, zero);
        v_interleave_lo_u8(d0, s, zero);
      }
      else {
        v_interleave_lo_u8(d0, s, zero);
        v_interleave_hi_u8(d1, s, zero);
      }
    }
  }

  template<typename Dst, typename Src>
  inline void vExpandAlphaLo16(const Dst& d, const Src& s) noexcept { v_swizzle_lo_u16(d, s, x86::shuffleImm(3, 3, 3, 3)); }

  template<typename Dst, typename Src>
  inline void vExpandAlphaHi16(const Dst& d, const Src& s) noexcept { v_swizzle_hi_u16(d, s, x86::shuffleImm(3, 3, 3, 3)); }

  template<typename Dst, typename Src>
  inline void v_expand_alpha_16(const Dst& d, const Src& s, uint32_t useHiPart = 1) noexcept {
    if (useHiPart) {
      if (hasAVX() || (hasSSSE3() && d == s)) {
        v_shuffle_i8(d, s, simdConst(&commonTable.pshufb_32xxxxxx10xxxxxx_to_3232323210101010, Bcst::kNA, d));
      }
      else {
        vExpandAlphaHi16(d, s);
        vExpandAlphaLo16(d, d);
      }
    }
    else {
      vExpandAlphaLo16(d, s);
    }
  }

  template<typename Dst, typename Src>
  inline void vExpandAlphaPS(const Dst& d, const Src& s) noexcept { v_swizzle_u32(d, s, x86::shuffleImm(3, 3, 3, 3)); }

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
  BL_NOINLINE void xModI64HIxU64LO(const Xmm& d, const VecOrMem_A& a, const VecOrMem_B& b) noexcept {
    Xmm t0 = cc->newXmm("t0");
    Xmm t1 = cc->newXmm("t1");

    v_swizzle_u32(t1, b, x86::shuffleImm(3, 3, 2, 0));
    v_swizzle_u32(d , a, x86::shuffleImm(2, 0, 3, 1));

    v_cvt_i32_f64(t1, t1);
    v_cvt_i32_f64(t0, d);
    v_mod_pd(t0, t0, t1);
    v_cvtt_f64_i32(t0, t0);

    v_sub_i32(d, d, t0);
    v_swizzle_u32(d, d, x86::shuffleImm(1, 3, 0, 2));
  }

  // Performs 32-bit unsigned modulo of 32-bit `a` (hi DWORD) with 64-bit `b` (DOUBLE).
  template<typename VecOrMem_A, typename VecOrMem_B>
  BL_NOINLINE void xModI64HIxDouble(const Vec& d, const VecOrMem_A& a, const VecOrMem_B& b) noexcept {
    Vec t0 = cc->newXmm("t0");

    v_swizzle_u32(d, a, x86::shuffleImm(2, 0, 3, 1));
    v_cvt_i32_f64(t0, d);
    v_mod_pd(t0, t0, b);
    v_cvtt_f64_i32(t0, t0);

    v_sub_i32(d, d, t0);
    v_swizzle_u32(d, d, x86::shuffleImm(1, 3, 0, 2));
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_1(const Vec& d, const Vec& s) noexcept {
    v_swizzle_lo_u16(d, s, x86::shuffleImm(1, 1, 1, 1));
    v_srl_i16(d, d, 8);
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_2(const Vec& d, const Vec& s) noexcept {
    if (hasSSSE3()) {
      v_shuffle_i8(d, s, simdConst(&commonTable.pshufb_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d));
    }
    else {
      v_swizzle_lo_u16(d, s, x86::shuffleImm(3, 3, 1, 1));
      v_swizzle_u32(d, d, x86::shuffleImm(1, 1, 0, 0));
      v_srl_i16(d, d, 8);
    }
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_4(const Vec& d0, const Vec& d1, const Vec& s) noexcept {
    BL_ASSERT(d0.id() != d1.id());

    if (hasSSSE3()) {
      if (d0.id() == s.id()) {
        v_shuffle_i8(d1, s, simdConst(&ct.pshufb_1xxx0xxxxxxxxxxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d1));
        v_shuffle_i8(d0, s, simdConst(&ct.pshufb_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d0));
      }
      else {
        v_shuffle_i8(d0, s, simdConst(&ct.pshufb_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d0));
        v_shuffle_i8(d1, s, simdConst(&ct.pshufb_1xxx0xxxxxxxxxxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d1));
      }
    }
    else {
      if (d1.id() != s.id()) {
        v_swizzle_hi_u16(d1, s, x86::shuffleImm(3, 3, 1, 1));
        v_swizzle_lo_u16(d0, s, x86::shuffleImm(3, 3, 1, 1));

        v_swizzle_u32(d1, d1, x86::shuffleImm(3, 3, 2, 2));
        v_swizzle_u32(d0, d0, x86::shuffleImm(1, 1, 0, 0));

        v_srl_i16(d1, d1, 8);
        v_srl_i16(d0, d0, 8);
      }
      else {
        v_swizzle_lo_u16(d0, s, x86::shuffleImm(3, 3, 1, 1));
        v_swizzle_hi_u16(d1, s, x86::shuffleImm(3, 3, 1, 1));

        v_swizzle_u32(d0, d0, x86::shuffleImm(1, 1, 0, 0));
        v_swizzle_u32(d1, d1, x86::shuffleImm(3, 3, 2, 2));

        v_srl_i16(d0, d0, 8);
        v_srl_i16(d1, d1, 8);
      }
    }
  }

  BL_NOINLINE void xPackU32ToU16Lo(const Vec& d0, const Vec& s0) noexcept {
    if (hasSSE4_1()) {
      v_packs_i32_u16_(d0, s0, s0);
    }
    else if (hasSSSE3()) {
      v_shuffle_i8(d0, s0, simdConst(&commonTable.pshufb_xx76xx54xx32xx10_to_7654321076543210, Bcst::kNA, d0));
    }
    else {
      // Sign extend and then use `packssdw()`.
      v_sll_i32(d0, s0, 16);
      v_sra_i32(d0, d0, 16);
      v_packs_i32_i16(d0, d0, d0);
    }
  }

  BL_NOINLINE void xPackU32ToU16Lo(const VecArray& d0, const VecArray& s0) noexcept {
    for (uint32_t i = 0; i < d0.size(); i++)
      xPackU32ToU16Lo(d0[i], s0[i]);
  }

  #undef PACK_AVX_SSE
  #undef V_EMIT_VVVV_VVV
  #undef V_EMIT_VVVi_VVi
  #undef V_EMIT_VVVI_VVI
  #undef V_EMIT_VVV_VV
  #undef V_EMIT_VVi_VVi
  #undef V_EMIT_VVI_VVI
  #undef V_EMIT_VVI_VVI
  #undef V_EMIT_VVI_VI
  #undef V_EMIT_VV_VV
#endif // BL_JIT_ARCH_X86

#if defined(BL_JIT_ARCH_A64)
#endif // BL_JIT_ARCH_A64
};

class PipeInjectAtTheEnd {
public:
  ScopedInjector _injector;

  BL_INLINE PipeInjectAtTheEnd(PipeCompiler* pc) noexcept
    : _injector(pc->cc, &pc->_funcEnd) {}
};

//! Provides unpacked global alpha mask; can be used by \ref FillPart and \ref CompOpPart as a global alpha abstraction.
class GlobalAlpha {
public:
  //! \name Members
  //! \{

  //! Pipeline compiler.
  PipeCompiler* _pc = nullptr;
  //! Node where to emit additional code in case `sm` is not initialized, but required.
  asmjit::BaseNode* _hook = nullptr;

  //! Global alpha as scalar (only used by scalar alpha-only processing operations).
  Gp _sm;
  //! Unpacked global alpha as vector.
  Vec _vm;

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE void initFromMem(PipeCompiler* pc, const Mem& mem) noexcept {
    _pc = pc;
    _vm = pc->newVec("ga.vm");
    _pc->v_broadcast_u16(_vm, mem);
    _hook = pc->cc->cursor();
  }

  BL_INLINE void initFromVec(PipeCompiler* pc, const Vec& vm) noexcept {
    _pc = pc;
    _hook = pc->cc->cursor();
    _vm = vm;
  }

  //! Returns whether global alpha is initialized and should be applied
  BL_INLINE_NODEBUG bool isInitialized() const noexcept { return _hook != nullptr; }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG const Vec& vm() const noexcept { return _vm; }

  BL_NOINLINE const Gp& sm() noexcept {
    if (_vm.isValid() && !_sm.isValid()) {
      ScopedInjector injector(_pc->cc, &_hook);
      _sm = _pc->newGp32("ga.sm");
      _pc->v_extract_u16(_sm, _vm, 0u);
    }

    return _sm;
  }

  //! \}
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPECOMPILER_P_H_INCLUDED
