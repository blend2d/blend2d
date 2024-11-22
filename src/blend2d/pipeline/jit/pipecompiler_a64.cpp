// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if defined(BL_JIT_ARCH_A64)

#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipepart_p.h"
#include "../../support/intops_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

using GPExt = PipeCompiler::GPExt;
using ASIMDExt = PipeCompiler::ASIMDExt;
namespace Inst { using namespace a64::Inst; }

// bl::Pipeline::PipeCompiler - Construction & Destruction
// =======================================================

PipeCompiler::PipeCompiler(AsmCompiler* cc, const CpuFeatures& features, PipeOptFlags optFlags) noexcept
  : cc(cc),
    ct(commonTable),
    _features(features),
    _optFlags(optFlags),
    _vecRegCount(32),
    _commonTableOff(512 + 128) {

  _scalarOpBehavior = ScalarOpBehavior::kZeroing;
  _fMinMaxOpBehavior = FMinMaxOpBehavior::kFiniteValue;
  _fMulAddOpBehavior = FMulAddOpBehavior::kFMAStoreToAccumulator;
  _initExtensions(features);
}

PipeCompiler::~PipeCompiler() noexcept {}

// bl::Pipeline::PipeCompiler - CPU Architecture, Features and Optimization Options
// ================================================================================

void PipeCompiler::_initExtensions(const asmjit::CpuFeatures& features) noexcept {
  uint64_t gpExtMask = 0;
  uint64_t asimdExtMask = 0;

  if (features.arm().hasCSSC()    ) gpExtMask |= uint64_t(1) << uint32_t(GPExt::kCSSC);
  if (features.arm().hasFLAGM()   ) gpExtMask |= uint64_t(1) << uint32_t(GPExt::kFLAGM);
  if (features.arm().hasFLAGM2()  ) gpExtMask |= uint64_t(1) << uint32_t(GPExt::kFLAGM2);
  if (features.arm().hasLS64()    ) gpExtMask |= uint64_t(1) << uint32_t(GPExt::kLS64);
  if (features.arm().hasLS64_V()  ) gpExtMask |= uint64_t(1) << uint32_t(GPExt::kLS64_V);
  if (features.arm().hasLSE()     ) gpExtMask |= uint64_t(1) << uint32_t(GPExt::kLSE);
  if (features.arm().hasLSE128()  ) gpExtMask |= uint64_t(1) << uint32_t(GPExt::kLSE128);
  if (features.arm().hasLSE2()    ) gpExtMask |= uint64_t(1) << uint32_t(GPExt::kLSE2);

  if (features.arm().hasASIMD()   ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kASIMD);
  if (features.arm().hasBF16()    ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kBF16);
  if (features.arm().hasDOTPROD() ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kDOTPROD);
  if (features.arm().hasFCMA()    ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kFCMA);
  if (features.arm().hasFHM()     ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kFHM);
  if (features.arm().hasFP16()    ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kFP16);
  if (features.arm().hasFP16CONV()) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kFP16CONV);
  if (features.arm().hasFP8()     ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kFP8);
  if (features.arm().hasFRINTTS() ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kFRINTTS);
  if (features.arm().hasI8MM()    ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kI8MM);
  if (features.arm().hasJSCVT()   ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kJSCVT);
  if (features.arm().hasPMULL()   ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kPMULL);
  if (features.arm().hasRDM()     ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kRDM);
  if (features.arm().hasSHA1()    ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kSHA1);
  if (features.arm().hasSHA256()  ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kSHA256);
  if (features.arm().hasSHA3()    ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kSHA3);
  if (features.arm().hasSHA512()  ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kSHA512);
  if (features.arm().hasSM3()     ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kSM3);
  if (features.arm().hasSM4()     ) asimdExtMask |= uint64_t(1) << uint32_t(ASIMDExt::kSM4);

  _gpExtMask = gpExtMask;
  _asimdExtMask = asimdExtMask;
}

VecWidth PipeCompiler::maxVecWidthFromCpuFeatures() noexcept {
  return VecWidth::k128;
}

void PipeCompiler::initVecWidth(VecWidth vw) noexcept {
  BL_ASSERT(vw == VecWidth::k128);
  blUnused(vw);

  _vecWidth = VecWidth::k128;
  _vecRegType = RegType::kVec128;
  _vecTypeId = asmjit::TypeId::kInt32x4;
  _vecMultiplier = 1u;
}

bool PipeCompiler::hasMaskedAccessOf(uint32_t dataSize) const noexcept {
  switch (dataSize) {
    case 1: return hasOptFlag(PipeOptFlags::kMaskOps8Bit);
    case 2: return hasOptFlag(PipeOptFlags::kMaskOps16Bit);
    case 4: return hasOptFlag(PipeOptFlags::kMaskOps32Bit);
    case 8: return hasOptFlag(PipeOptFlags::kMaskOps64Bit);

    default:
      return false;
  }
}

// bl::Pipeline::PipeCompiler - Function
// =====================================

void PipeCompiler::initFunction(asmjit::FuncNode* funcNode) noexcept {
  cc->addFunc(funcNode);

  _funcNode = funcNode;
  _funcInit = cc->cursor();
  _funcEnd = funcNode->endNode()->prev();
}

// bl::Pipeline::PipeCompiler - Constants
// ======================================

void PipeCompiler::_initCommonTablePtr() noexcept {
  const void* global = &commonTable;

  if (!_commonTablePtr.isValid()) {
    ScopedInjector injector(cc, &_funcInit);
    _commonTablePtr = newGpPtr("commonTablePtr");
    cc->mov(_commonTablePtr, (int64_t)global + _commonTableOff);
  }
}

Operand PipeCompiler::simdConst(const void* c, Bcst bcstWidth, VecWidth constWidth) noexcept {
  return simdVecConst(c, bcstWidth, constWidth);
}

Operand PipeCompiler::simdConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept {
  blUnused(similarTo);
  return simdVecConst(c, bcstWidth, VecWidth::k128);
}

Operand PipeCompiler::simdConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept {
  BL_ASSERT(!similarTo.empty());
  blUnused(bcstWidth, similarTo);

  return simdVecConst(c, bcstWidth, VecWidth::k128);
}

Vec PipeCompiler::simdVecConst(const void* c, Bcst bcstWidth, VecWidth constWidth) noexcept {
  blUnused(bcstWidth);
  blUnused(constWidth);

  size_t n = _vecConsts.size();
  for (size_t i = 0; i < n; i++)
    if (_vecConsts[i].ptr == c)
      return Vec(OperandSignature{a64::VecV::kSignature}, _vecConsts[i].vRegId);

  return Vec(OperandSignature{a64::VecV::kSignature}, _newVecConst(c, true).id());
}

Vec PipeCompiler::simdVecConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept {
  blUnused(similarTo);
  return simdVecConst(c, bcstWidth, VecWidth::k128);
}

Vec PipeCompiler::simdVecConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept {
  blUnused(similarTo);
  return simdVecConst(c, bcstWidth, VecWidth::k128);
}

Mem PipeCompiler::simdMemConst(const void* c, Bcst bcstWidth, VecWidth constWidth) noexcept {
  blUnused(bcstWidth, constWidth);
  return _getMemConst(c);
}

Mem PipeCompiler::simdMemConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept {
  blUnused(bcstWidth, similarTo);
  return _getMemConst(c);
}

Mem PipeCompiler::simdMemConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept {
  blUnused(bcstWidth, similarTo);
  return _getMemConst(c);
}

Mem PipeCompiler::_getMemConst(const void* c) noexcept {
  // Make sure we are addressing a constant from the `commonTable` constant pool.
  const void* global = &commonTable;
  BL_ASSERT((uintptr_t)c >= (uintptr_t)global &&
            (uintptr_t)c <  (uintptr_t)global + sizeof(CommonTable));

  // One GP register is sacrificed to hold the pointer to the `commonTable`.
  _initCommonTablePtr();

  int32_t disp = int32_t((intptr_t)c - (intptr_t)global);
  return mem_ptr(_commonTablePtr, disp - _commonTableOff);
}

Vec PipeCompiler::_newVecConst(const void* c, bool isUniqueConst) noexcept {
  blUnused(isUniqueConst);

  Vec vReg;
  const char* specialConstName = nullptr;

  if (c == commonTable.swizu8_dither_rgba64_lo.data) {
    specialConstName = "swizu8_dither_rgba64_lo";
  }
  else if (c == commonTable.swizu8_dither_rgba64_hi.data) {
    specialConstName = "swizu8_dither_rgba64_hi";
  }

  if (specialConstName) {
    vReg = newVec(vecWidth(), specialConstName);
  }
  else {
    uint64_t u0 = static_cast<const uint64_t*>(c)[0];
    uint64_t u1 = static_cast<const uint64_t*>(c)[1];

    if (u0 != u1)
      vReg = newVec(vecWidth(), "c_0x%016llX%016llX", (unsigned long long)u1, (unsigned long long)u0);
    else if ((u0 >> 32) != (u0 & 0xFFFFFFFFu))
      vReg = newVec(vecWidth(), "c_0x%016llX", (unsigned long long)u0);
    else if (((u0 >> 16) & 0xFFFFu) != (u0 & 0xFFFFu))
      vReg = newVec(vecWidth(), "c_0x%08X", (unsigned)(u0 & 0xFFFFFFFFu));
    else
      vReg = newVec(vecWidth(), "c_0x%04X", (unsigned)(u0 & 0xFFFFu));
  }

  VecConst vConst;
  vConst.ptr = c;
  vConst.vRegId = vReg.id();
  _vecConsts.append(zoneAllocator(), vConst);

  if (c == &commonTable.i_0000000000000000) {
    ScopedInjector inject(cc, &_funcInit);
    v_zero_i(vReg.v128());
  }
  else {
    // NOTE: _getMemConst() must be outside of injected code as it uses injection too.
    Mem m = _getMemConst(c);

    ScopedInjector inject(cc, &_funcInit);
    v_loadavec(vReg, m);
  }

  return vReg;
}

Vec PipeCompiler::simdConst16B(const void* data16) noexcept {
  size_t n = _vecConstsEx.size();

  for (size_t i = 0; i < n; i++) {
    if (memcmp(_vecConstsEx[i].data, data16, 16) == 0) {
      return Vec(OperandSignature{a64::VecV::kSignature}, _vecConstsEx[i].vRegId);
    }
  }

  Vec vReg = newVec(VecWidth::k128, "const");
  VecConstEx entry;

  memcpy(entry.data, data16, 16);
  entry.vRegId = vReg.id();
  _vecConstsEx.append(zoneAllocator(), entry);

  Mem mem = cc->newConst(asmjit::ConstPoolScope::kLocal, data16, 16);
  {
    ScopedInjector inject(cc, &_funcInit);
    v_loadavec(vReg, mem);
  }

  return vReg;
}

// bl::Pipeline::PipeCompiler - Stack
// ==================================

Mem PipeCompiler::tmpStack(StackId id, uint32_t size) noexcept {
  BL_ASSERT(IntOps::isPowerOf2(size));
  BL_ASSERT(size <= 32);

  // Only used by asserts.
  blUnused(size);

  Mem& stack = _tmpStack[size_t(id)];
  if (!stack.baseId()) {
    stack = cc->newStack(32, 16, "tmpStack");
  }
  return stack;
}

// bl::Pipeline::PipeCompiler - Utilities
// ======================================

void PipeCompiler::embedJumpTable(const Label* jumpTable, size_t jumpTableSize, const Label& jumpTableBase, uint32_t entrySize) noexcept {
  static const uint8_t zeros[8] {};

  for (size_t i = 0; i < jumpTableSize; i++) {
    if (jumpTable[i].isValid()) {
      cc->embedLabelDelta(jumpTable[i], jumpTableBase, entrySize);
    }
    else {
      cc->embed(zeros, entrySize);
    }
  }
}

// bl::Pipeline::PipeCompiler - General Purpose Instructions - Utilities
// =====================================================================

struct MemInst {
  uint16_t instId;
  uint16_t memSize;
};

static BL_NOINLINE void gp_emit_mem_op(PipeCompiler* pc, const Gp& r, Mem m, MemInst ii) noexcept {
  AsmCompiler* cc = pc->cc;
  InstId instId = ii.instId;

  if (m.hasIndex() && m.hasShift()) {
    // AArch64 limitation: shift can be the same size as the size of the read operation - HWord << 1, Word << 2, etc...
    // Other shift operations are not supported at the architectural level, so we have to perform it explicitly.
    uint32_t memSize = ii.memSize ? uint32_t(ii.memSize) : r.size();
    uint32_t shift = m.shift();

    if (memSize != (1u << shift)) {
      Gp tmp = pc->newGpPtr("@mem_addr");
      cc->add(tmp, m.baseReg().as<Gp>(), m.indexReg().as<Gp>(), a64::Shift(m.shiftOp(), shift));
      m = a64::ptr(tmp);
    }
  }

  cc->emit(instId, r, m);
}

static constexpr Gp gp_zero_regs[2] = { a64::wzr, a64::xzr };

static BL_INLINE const Gp& gp_zero_as(const Gp& ref) noexcept {
  return gp_zero_regs[size_t(ref.isGpX())];
}

static BL_NOINLINE Gp gp_force_reg(PipeCompiler* pc, const Operand_& op, const Gp& ref) noexcept {
  BL_ASSERT(op.isGp() || op.isMem() || op.isImm());

  Gp reg;

  if (op.isGp()) {
    reg = op.as<Gp>();
    reg.setSignature(ref.signature());
    return reg;
  }

  if (op.isImm() && op.as<Imm>().value() == 0) {
    return gp_zero_as(ref);
  }

  AsmCompiler* cc = pc->cc;
  reg = pc->newSimilarReg(ref, "@tmp");

  if (op.isMem()) {
    gp_emit_mem_op(pc, reg, op.as<Mem>(), MemInst{uint16_t(Inst::kIdLdr), uint16_t(reg.size())});
  }
  else {
    cc->mov(reg, op.as<Imm>());
  }
  return reg;
}

// bl::Pipeline::PipeCompiler - General Purpose Instructions - Conditions
// ======================================================================

struct ConditionOpInfo {
  uint16_t instId;
  uint16_t reserved;
};

static constexpr ConditionOpInfo condition_op_info[size_t(OpcodeCond::kMaxValue) + 1] = {
  { Inst::kIdAnds, 0 }, // OpcodeCond::kAssignAnd
  { Inst::kIdOrr , 0 }, // OpcodeCond::kAssignOr
  { Inst::kIdEor , 0 }, // OpcodeCond::kAssignXor
  { Inst::kIdAdds, 0 }, // OpcodeCond::kAssignAdd
  { Inst::kIdSubs, 0 }, // OpcodeCond::kAssignSub
  { Inst::kIdNone, 0 }, // OpcodeCond::kAssignShr
  { Inst::kIdTst , 0 }, // OpcodeCond::kTest
  { Inst::kIdNone, 0 }, // OpcodeCond::kBitTest
  { Inst::kIdCmp , 0 }  // OpcodeCond::kCompare
};

class ConditionApplier : public Condition {
public:
  BL_INLINE ConditionApplier(const Condition& condition) noexcept : Condition(condition) {
    // The first operand must always be a register.
    BL_ASSERT(a.isGp());
  }

  BL_NOINLINE void optimize(PipeCompiler* pc) noexcept {
    blUnused(pc);

    switch (op) {
      case OpcodeCond::kCompare:
        if (b.isImm() && b.as<Imm>().value() == 0 && (cond == CondCode::kEqual || cond == CondCode::kNotEqual)) {
          op = OpcodeCond::kTest;
          b = a;
          reverse();
        }
        break;

      case OpcodeCond::kBitTest: {
        if (b.isImm()) {
          uint64_t bitIndex = b.as<Imm>().valueAs<uint64_t>();
          op = OpcodeCond::kTest;
          b = Imm(1u << bitIndex);
        }
        break;
      }

      default:
        break;
    }
  }

  BL_INLINE void reverse() noexcept {
    cond = a64::reverseCond(cond);
  }

  BL_NOINLINE void emit(PipeCompiler* pc) noexcept {
    AsmCompiler* cc = pc->cc;
    ConditionOpInfo info = condition_op_info[size_t(op)];

    Gp aGp = a.as<Gp>();

    switch (op) {
      case OpcodeCond::kAssignAnd: {
        if (b.isImm() && a64::Utils::isLogicalImm(b.as<Imm>().valueAs<uint64_t>(), aGp.size() * 8)) {
          cc->emit(info.instId, aGp, aGp, b.as<Imm>());
        }
        else {
          cc->emit(info.instId, aGp, aGp, gp_force_reg(pc, b, aGp));
        }
        return;
      }

      case OpcodeCond::kAssignAdd:
      case OpcodeCond::kAssignSub: {
        if (b.isImm() && a64::Utils::isAddSubImm(b.as<Imm>().valueAs<uint64_t>())) {
          cc->emit(info.instId, aGp, aGp, b.as<Imm>());
        }
        else {
          cc->emit(info.instId, aGp, aGp, gp_force_reg(pc, b, aGp));
        }
        return;
      }

      case OpcodeCond::kAssignXor:
        if (b.isImm()) {
          const Imm& bImm = b.as<Imm>();
          if (bImm.value() == -1 || (aGp.size() == 4 && bImm.valueAs<uint32_t>() == 0xFFFFFFFFu)) {
            cc->mvn(aGp, aGp);
            cc->tst(aGp, aGp);
            return;
          }
        }
        BL_FALLTHROUGH
      case OpcodeCond::kAssignOr: {
        if (b.isImm() && a64::Utils::isLogicalImm(b.as<Imm>().valueAs<uint64_t>(), aGp.size() * 8)) {
          cc->emit(info.instId, aGp, aGp, b.as<Imm>());
          cc->tst(aGp, aGp);
        }
        else {
          cc->emit(info.instId, aGp, aGp, gp_force_reg(pc, b, aGp));
          cc->tst(aGp, aGp);
        }
        return;
      }

      case OpcodeCond::kAssignShr: {
        if (b.isImm()) {
          cc->adds(aGp, gp_zero_as(aGp), aGp, a64::lsr(b.as<Imm>().valueAs<uint32_t>()));
        }
        else {
          cc->lsr(aGp, aGp, gp_force_reg(pc, b, aGp));
          cc->tst(aGp, aGp);
        }
        return;
      }

      case OpcodeCond::kTest: {
        if (b.isImm() && a64::Utils::isLogicalImm(b.as<Imm>().valueAs<uint64_t>(), aGp.size() * 8)) {
          cc->emit(info.instId, aGp, b.as<Imm>());
        }
        else {
          cc->emit(info.instId, aGp, gp_force_reg(pc, b, aGp));
        }
        return;
      }

      case OpcodeCond::kCompare: {
        if (b.isImm() && a64::Utils::isAddSubImm(b.as<Imm>().valueAs<uint64_t>())) {
          cc->emit(info.instId, aGp, b.as<Imm>());
        }
        else {
          cc->emit(info.instId, aGp, gp_force_reg(pc, b, aGp));
        }
        return;
      }

      case OpcodeCond::kBitTest: {
        Gp tmp = pc->newSimilarReg(aGp);
        cc->lsr(tmp, aGp, gp_force_reg(pc, b, aGp));
        cc->tst(tmp, Imm(1));
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }
};

// bl::Pipeline::PipeCompiler - General Purpose Instructions - Emit
// ================================================================

void PipeCompiler::emit_mov(const Gp& dst, const Operand_& src) noexcept {
  if (src.isMem()) {
    gp_emit_mem_op(this, dst, src.as<Mem>(), MemInst{uint16_t(Inst::kIdLdr), uint16_t(dst.size())});
  }
  else {
    cc->emit(Inst::kIdMov, dst, src);
  }
}

void PipeCompiler::emit_m(OpcodeM op, const Mem& m_) noexcept {
  static constexpr MemInst st_inst[] = {
    { Inst::kIdStr , 0 }, // kStoreZeroReg
    { Inst::kIdStrb, 1 }, // kStoreZeroU8
    { Inst::kIdStrh, 2 }, // kStoreZeroU16
    { Inst::kIdStr , 4 }, // kStoreZeroU32
    { Inst::kIdStr , 8 }  // kStoreZeroU64
  };

  Gp zero = gp_zero_regs[size_t(op == OpcodeM::kStoreZeroReg || op == OpcodeM::kStoreZeroU64)];
  MemInst ii = st_inst[size_t(op)];

  gp_emit_mem_op(this, zero, m_, ii);
}

void PipeCompiler::emit_rm(OpcodeRM op, const Gp& dst, const Mem& src) noexcept {
  static constexpr MemInst ld_inst[] = {
    { Inst::kIdLdr  , 0 }, // kLoadReg
    { Inst::kIdLdrsb, 1 }, // kLoadI8
    { Inst::kIdLdrb , 1 }, // kLoadU8
    { Inst::kIdLdrsh, 2 }, // kLoadI16
    { Inst::kIdLdrh , 2 }, // kLoadU16
    { Inst::kIdLdr  , 4 }, // kLoadI32
    { Inst::kIdLdr  , 4 }, // kLoadU32
    { Inst::kIdLdr  , 8 }, // kLoadI64
    { Inst::kIdLdr  , 8 }, // kLoadU64
    { Inst::kIdLdrb , 1 }, // kLoadMergeU8
    { Inst::kIdLdrb , 1 }, // kLoadShiftU8
    { Inst::kIdLdrh , 2 }, // kLoadMergeU16
    { Inst::kIdLdrh , 2 }  // kLoadShiftU16
  };

  static constexpr uint32_t ld_32_mask =
    (1u << uint32_t(uint32_t(OpcodeRM::kLoadU8 ))) |
    (1u << uint32_t(uint32_t(OpcodeRM::kLoadU16))) |
    (1u << uint32_t(uint32_t(OpcodeRM::kLoadU32))) ;

  Gp r(dst);
  Mem m(src);
  MemInst ii = ld_inst[size_t(op)];

  switch (op) {
    case OpcodeRM::kLoadReg:
    case OpcodeRM::kLoadI8:
    case OpcodeRM::kLoadU8:
    case OpcodeRM::kLoadI16:
    case OpcodeRM::kLoadU16:
    case OpcodeRM::kLoadI32:
    case OpcodeRM::kLoadU32:
    case OpcodeRM::kLoadI64:
    case OpcodeRM::kLoadU64: {
      if (op == OpcodeRM::kLoadI32 && dst.isGpX()) {
        ii.instId = uint16_t(Inst::kIdLdrsw);
      }

      if ((ld_32_mask >> uint32_t(op)) & 1) {
        r = r.w();
      }

      gp_emit_mem_op(this, r, m, ii);
      return;
    }

    case OpcodeRM::kLoadShiftU8:
    case OpcodeRM::kLoadShiftU16: {
      Gp tmp = newSimilarReg(r);
      gp_emit_mem_op(this, tmp.r32(), m, ii);
      cc->orr(r, tmp, r, a64::lsl(ii.memSize * 8));
      return;
    }

    case OpcodeRM::kLoadMergeU8:
    case OpcodeRM::kLoadMergeU16: {
      Gp tmp = newSimilarReg(r);
      gp_emit_mem_op(this, tmp.r32(), m, ii);
      cc->orr(r, r, tmp);
      return;
    }

    default: {
      BL_NOT_REACHED();
    }
  }
}

struct OpcodeMRInfo {
  uint32_t opInst : 14;
  uint32_t storeInst : 14;
  uint32_t size : 4;
};

void PipeCompiler::emit_mr(OpcodeMR op, const Mem& dst, const Gp& src) noexcept {
  static constexpr OpcodeMRInfo op_info_table[] = {
    { Inst::kIdNone, Inst::kIdStr , 0 }, // kStoreReg
    { Inst::kIdNone, Inst::kIdStrb, 1 }, // kStoreU8
    { Inst::kIdNone, Inst::kIdStrh, 2 }, // kStoreU16
    { Inst::kIdNone, Inst::kIdStr , 4 }, // kStoreU32
    { Inst::kIdNone, Inst::kIdStr , 8 }, // kStoreU64
    { Inst::kIdAdd , Inst::kIdStr , 0 }, // kStoreReg
    { Inst::kIdAdd , Inst::kIdStrb, 1 }, // kStoreU8
    { Inst::kIdAdd , Inst::kIdStrh, 2 }, // kStoreU16
    { Inst::kIdAdd , Inst::kIdStr , 4 }, // kStoreU32
    { Inst::kIdAdd , Inst::kIdStr , 8 }  // kStoreU64
  };

  static constexpr uint32_t st_32_mask =
    (1u << uint32_t(uint32_t(OpcodeMR::kStoreU8 ))) |
    (1u << uint32_t(uint32_t(OpcodeMR::kStoreU16))) |
    (1u << uint32_t(uint32_t(OpcodeMR::kStoreU32))) ;

  Mem m(dst);
  Gp r(src);
  const OpcodeMRInfo& opInfo = op_info_table[size_t(op)];

  if ((st_32_mask >> uint32_t(op)) & 1)
    r = r.w();

  if (opInfo.opInst == Inst::kIdNone) {
    cc->emit(opInfo.storeInst, r, m);
  }
  else {
    Gp tmp = newSimilarReg(r, "@tmp");
    switch (opInfo.size) {
      case 0: load(tmp, dst); break;
      case 1: load_u8(tmp, dst); break;
      case 2: load_u16(tmp, dst); break;
      case 4: load_u32(tmp, dst); break;
      case 8: load_u64(tmp, dst); break;
    }
    cc->emit(opInfo.opInst, tmp, tmp, r);
    cc->emit(opInfo.storeInst, tmp, m);
  }
}

void PipeCompiler::emit_cmov(const Gp& dst, const Operand_& sel, const Condition& condition) noexcept {
  ConditionApplier ca(condition);
  ca.optimize(this);
  ca.emit(this);
  cc->csel(dst, gp_force_reg(this, sel, dst), dst, condition.cond);
}

void PipeCompiler::emit_select(const Gp& dst, const Operand_& sel1_, const Operand_& sel2_, const Condition& condition) noexcept {
  ConditionApplier ca(condition);
  ca.optimize(this);
  ca.emit(this);

  Gp sel1 = gp_force_reg(this, sel1_, dst);
  Gp sel2 = gp_force_reg(this, sel2_, dst);
  cc->csel(dst, sel1, sel2, condition.cond);
}

void PipeCompiler::emit_2i(OpcodeRR op, const Gp& dst, const Operand_& src_) noexcept {
  // ArithOp Reg, Any
  // ----------------

  if (src_.isRegOrMem()) {
    Gp src = gp_force_reg(this, src_, dst);

    switch (op) {
      case OpcodeRR::kAbs: {
        //if (hasCSSC()) {
        //  cc->abs(dst, src);
        //}
        //else {
          cc->cmp(src, 0);
          cc->cneg(dst, src, CondCode::kMI);
        //}
        return;
      }

      case OpcodeRR::kNeg: {
        cc->neg(dst, src);
        return;
      }

      case OpcodeRR::kNot: {
        cc->mvn(dst, src);
        return;
      }

      case OpcodeRR::kBSwap: {
        cc->rev(dst, src);
        return;
      }

      case OpcodeRR::kCLZ: {
        cc->clz(dst, src);
        return;
      }

      case OpcodeRR::kCTZ: {
        //if (hasCSSC()) {
        //  cc->ctz(dst, src);
        //}
        //else {
          cc->rbit(dst, src);
          cc->clz(dst, dst);
        //}
        return;
      }

      case OpcodeRR::kReflect: {
        cc->eor(dst, src, src, a64::asr(dst.size() * 8u - 1));
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }

  // Everything should be handled, so this should never be reached!
  BL_NOT_REACHED();
}

static constexpr uint64_t kOp3ICommutativeMask =
  (uint64_t(1) << unsigned(OpcodeRRR::kAnd )) |
  (uint64_t(1) << unsigned(OpcodeRRR::kOr  )) |
  (uint64_t(1) << unsigned(OpcodeRRR::kXor )) |
  (uint64_t(1) << unsigned(OpcodeRRR::kAdd )) |
  (uint64_t(1) << unsigned(OpcodeRRR::kMul )) |
  (uint64_t(1) << unsigned(OpcodeRRR::kSMin)) |
  (uint64_t(1) << unsigned(OpcodeRRR::kSMax)) |
  (uint64_t(1) << unsigned(OpcodeRRR::kUMin)) |
  (uint64_t(1) << unsigned(OpcodeRRR::kUMax)) ;

static BL_INLINE_NODEBUG bool isOp3ICommutative(OpcodeRRR op) noexcept {
  return (kOp3ICommutativeMask & (uint64_t(1) << unsigned(op))) != 0;
}

void PipeCompiler::emit_3i(OpcodeRRR op, const Gp& dst, const Operand_& src1_, const Operand_& src2_) noexcept {
  Operand src1(src1_);
  Operand src2(src2_);

  if (!src1.isReg()) {
    if (src2.isReg() && isOp3ICommutative(op)) {
      BL_ASSERT(src2.isGp());
      BLInternal::swap(src1, src2);
    }
    else {
      src1 = gp_force_reg(this, src1, dst);
    }
  }

  static constexpr uint16_t addsub_inst[2] = { Inst::kIdAdd, Inst::kIdSub };
  // static constexpr uint16_t sminmax_inst[2] = { Inst::kIdSMin, Inst::kIdSMax };
  // static constexpr uint16_t uminmax_inst[2] = { Inst::kIdUMin, Inst::kIdUMax };
  static constexpr uint16_t logical_inst[4] = { Inst::kIdAnd, Inst::kIdOrr, Inst::kIdEor, Inst::kIdBic };
  static constexpr uint16_t shift_inst[3] = { Inst::kIdLsl, Inst::kIdLsr, Inst::kIdAsr };

  // ArithOp Reg, Reg, Imm
  // ---------------------

  if (src2.isImm()) {
    Gp a = src1.as<Gp>().cloneAs(dst);
    Imm b = src2.as<Imm>();

    switch (op) {
      case OpcodeRRR::kXor:
        if (b.value() == -1 || (b.valueAs<uint32_t>() == 0xFFFFFFFFu && dst.size() == 4)) {
          cc->mvn(dst, a);
          return;
        }
        BL_FALLTHROUGH
      case OpcodeRRR::kAnd:
      case OpcodeRRR::kOr:
      case OpcodeRRR::kBic: {
        if (a64::Utils::isLogicalImm(b.value(), a.size() * 8u)) {
          cc->emit(logical_inst[size_t(op) - size_t(OpcodeRRR::kAnd)], dst, a, b);
          return;
        }

        // If the immediate value is not encodable, we have to use a register.
        break;
      }

      case OpcodeRRR::kAdd:
      case OpcodeRRR::kSub: {
        uint64_t value = b.valueAs<uint64_t>();
        unsigned reverse = int64_t(value) < 0;

        if (reverse)
          value = uint64_t(0) - value;

        if (op == OpcodeRRR::kSub)
          reverse = reverse ^ 1u;

        // TODO: [JIT] Just testing the idea of patching the previous instruction to have a post-index addressing.
        if (!reverse && uint64_t(value) < 256 && dst.id() == a.id()) {
          if (cc->cursor()->type() == asmjit::NodeType::kInst) {
            asmjit::InstNode* prevInst = cc->cursor()->as<asmjit::InstNode>();
            if (prevInst->id() == Inst::kIdLdr || prevInst->id() == Inst::kIdStr ||
                prevInst->id() == Inst::kIdLdr_v || prevInst->id() == Inst::kIdStr_v) {
              Mem& memOp = prevInst->op(prevInst->opCount() - 1).as<Mem>();
              if (memOp.baseReg() == a && !memOp.hasIndex() && !memOp.hasOffset()) {
                memOp.setOffsetMode(asmjit::arm::OffsetMode::kPostIndex);
                memOp.addOffset(int64_t(value));
                return;
              }
            }
          }
        }

        if (asmjit::Support::isUInt12(value)) {
          cc->emit(addsub_inst[reverse], dst, a, Imm(value));
          return;
        }

        // If the immediate value is not encodable, we have to use a register.
        break;
      }

      case OpcodeRRR::kMul: {
        uint64_t value = b.valueAs<uint64_t>();
        if (value > 0u) {
          if (asmjit::Support::isPowerOf2(value)) {
            uint32_t shift = asmjit::Support::ctz(value);
            cc->lsl(dst, a, Imm(shift));
            return;
          }

          // We can still support multiplication with powerOf2 + 1
          if (asmjit::Support::isPowerOf2(--value)) {
            uint32_t shift = asmjit::Support::ctz(value);
            cc->add(dst, a, a, a64::lsl(shift));
            return;
          }
        }
        break;
      }

      case OpcodeRRR::kSMin:
      case OpcodeRRR::kSMax: {
        int64_t value = b.valueAs<int64_t>();

        if (value == 0 || value == -1) {
          uint32_t shift = dst.size() * 8u - 1u;

          // Signed min/max against -1 and 0 can be implemented by using a variation of bitwise instructions
          // with the input value combined with its signs (that's why arithmetic shift right is used).
          static constexpr uint16_t inst_table[4] = {
            Inst::kIdAnd, // smin(a, 0)  == and(a, expand_msb(a))
            Inst::kIdOrr, // smin(a, -1) == orr(a, expand_msb(a))
            Inst::kIdBic, // smax(a, 0)  == bic(a, expand_msb(a))
            Inst::kIdOrn  // smax(a, -1) == orn(a, expand_msb(a))
          };

          InstId instId = inst_table[(size_t(op) - size_t(OpcodeRRR::kSMin)) * 2u + size_t(value == -1)];
          cc->emit(instId, dst, a, a, Imm(a64::asr(shift)));
          return;
        }

        //if (hasCSSC() && Support::isInt8(value)) {
        //  cc->emit(sminmax_inst[size_t(op) - size_t(OpcodeRRR::kSMin)], dst, a, b);
        //  return;
        //}
        break;
      }

      case OpcodeRRR::kUMin:
      case OpcodeRRR::kUMax: {
        //uint64_t value = b.valueAs<uint64_t>();
        //if (hasCSSC() && Support::isUInt8(value)) {
        //  cc->emit(uminmax_inst[size_t(op) - size_t(OpcodeRRR::kUMin)], dst, a, b);
        //  return;
        //}
        break;
      }

      case OpcodeRRR::kSll:
      case OpcodeRRR::kSrl:
      case OpcodeRRR::kSra: {
        cc->emit(shift_inst[size_t(op) - size_t(OpcodeRRR::kSll)], dst, a, b);
        return;
      }

      case OpcodeRRR::kRol:
      case OpcodeRRR::kRor: {
        uint32_t width = dst.size() * 8u;
        uint32_t value = b.valueAs<uint32_t>() & (width - 1);

        if (op == OpcodeRRR::kRol)
          value = width - value;

        cc->ror(dst, a, Imm(value));
        return;
      }

      case OpcodeRRR::kSBound: {
        // if (hasCSSC() && Support::isUInt8(value)) {
        // }
        break;
      }

      default:
        // Unhandled instruction means to use a register instead of immediate.
        break;
    }
  }

  // ArithOp Reg, Reg, Reg
  // ---------------------

  {
    src2 = gp_force_reg(this, src2, dst);

    Gp a = src1.as<Gp>();
    Gp b = src2.as<Gp>();

    switch (op) {
      case OpcodeRRR::kAnd:
      case OpcodeRRR::kOr:
      case OpcodeRRR::kXor:
      case OpcodeRRR::kBic: {
        cc->emit(logical_inst[size_t(op) - size_t(OpcodeRRR::kAnd)], dst, a, b);
        return;
      }

      case OpcodeRRR::kAdd:
      case OpcodeRRR::kSub: {
        cc->emit(addsub_inst[size_t(op) - size_t(OpcodeRRR::kAdd)], dst, a, b);
        return;
      }

      case OpcodeRRR::kMul: {
        cc->mul(dst, a, b);
        return;
      }

      case OpcodeRRR::kUDiv: {
        cc->udiv(dst, a, b);
        return;
      }

      case OpcodeRRR::kUMod: {
        Gp tmp = newSimilarReg(dst, "@tmp");
        cc->udiv(tmp, a, b);
        cc->mul(tmp, tmp, b);
        cc->sub(dst, a, tmp);
        return;
      }

      case OpcodeRRR::kSMin:
      case OpcodeRRR::kSMax: {
        //if (hasCSSC()) {
        //  cc->emit(sminmax_inst[size_t(op) - size_t(OpcodeRRR::kSMin)], dst, a, b);
        //}
        //else {
          cc->cmp(a, b);
          cc->csel(dst, a, b, op == OpcodeRRR::kSMin ? CondCode::kLT : CondCode::kGT);
        //}
        return;
      }

      case OpcodeRRR::kUMin:
      case OpcodeRRR::kUMax: {
        //if (hasCSSC()) {
        //  cc->emit(uminmax_inst[size_t(op) - size_t(OpcodeRRR::kUMin)], dst, a, b);
        //}
        //else {
          cc->cmp(a, b);
          cc->csel(dst, a, b, op == OpcodeRRR::kUMin ? CondCode::kLO : CondCode::kHI);
        //}
        return;
      }

      case OpcodeRRR::kSll:
      case OpcodeRRR::kSrl:
      case OpcodeRRR::kSra: {
        cc->emit(shift_inst[size_t(op) - size_t(OpcodeRRR::kSll)], dst, a, b);
        return;
      }

      case OpcodeRRR::kRol: {
        Gp tmp = newSimilarReg(dst, "@rol_rev");
        cc->neg(tmp, b);
        cc->ror(dst, a, tmp);
        return;
      }

      case OpcodeRRR::kRor: {
        cc->ror(dst, a, b);
        return;
      }

      case OpcodeRRR::kSBound: {
        cc->cmp(a, b);
        cc->bic(dst, a, a, a64::asr(dst.size() * 8u - 1));
        cc->csel(dst, dst, b, CondCode::kSignedLT);
        return;
      }

      default:
        // Everything should be handled, so this should never be reached!
        BL_NOT_REACHED();
    }
  }
}

void PipeCompiler::emit_j(const Operand_& target) noexcept {
  cc->emit(Inst::kIdB, target);
}

void PipeCompiler::emit_j_if(const Label& target, const Condition& condition) noexcept {
  ConditionApplier ca(condition);
  ca.optimize(this);
  ca.emit(this);
  cc->b(ca.cond, target);
}

void PipeCompiler::adds_u8(const Gp& dst, const Gp& src1, const Gp& src2) noexcept {
  BL_ASSERT(dst.size() == src1.size());
  BL_ASSERT(dst.size() == src2.size());

  cc->add(dst, src1, src2);
  //if (hasCSSC()) {
  //  cc->umin(dst, dst, 255);
  //  return;
  //}

  Gp tmp = newSimilarReg(dst, "@tmp");
  cc->cmp(dst, 255);
  cc->mov(tmp, 255);
  cc->csel(dst, dst, tmp, CondCode::kLO);
}

void PipeCompiler::inv_u8(const Gp& dst, const Gp& src) noexcept {
  cc->eor(dst, src, 0xFF);
}

void PipeCompiler::div_255_u32(const Gp& dst, const Gp& src) noexcept {
  BL_ASSERT(dst.size() == src.size());

  // dst = src + 128;
  // dst = (dst + (dst >> 8)) >> 8
  cc->add(dst, src, 128);
  cc->add(dst, dst, dst, a64::lsr(8));
  cc->lsr(dst, dst, 8);
}

void PipeCompiler::mul_257_hu16(const Gp& dst, const Gp& src) noexcept {
  BL_ASSERT(dst.size() == src.size());
  cc->add(dst, src, src, a64::lsl(8));
  cc->lsr(dst, dst, 16);
}

void PipeCompiler::add_scaled(const Gp& dst, const Gp& a_, int b) noexcept {
  Gp a = a_.cloneAs(dst);

  if (asmjit::Support::isPowerOf2(b)) {
    uint32_t shift = asmjit::Support::ctz(b);
    cc->add(dst, dst, a, a64::lsl(shift));
  }
  else if (b == 3 && dst.id() == a.id()) {
    cc->add(dst, dst, dst, a64::lsl(1));
  }
  else {
    Gp tmp = newSimilarReg(dst, "@tmp");
    cc->mov(tmp, b);
    cc->madd(dst, a, tmp, dst);
  }
}

void PipeCompiler::add_ext(const Gp& dst, const Gp& src_, const Gp& idx_, uint32_t scale, int32_t disp) noexcept {
  BL_ASSERT(scale != 0);

  Gp src = src_.cloneAs(dst);
  Gp idx = idx_.cloneAs(dst);

  if (asmjit::Support::isPowerOf2(scale)) {
    cc->add(dst, src, idx, a64::lsl(asmjit::Support::ctz(scale)));
  }
  else {
    Gp tmp = newSimilarReg(dst, "@tmp");

    if (scale == 3) {
      cc->add(tmp, src, idx, a64::lsl(1));
      cc->add(dst, tmp, idx);
    }
    else {
      cc->mov(tmp, scale);
      cc->mul(tmp, tmp, idx);
      cc->add(dst, src, tmp);
    }
  }

  if (disp) {
    if (disp > 0)
      cc->add(dst, dst, disp);
    else
      cc->sub(dst, dst, -disp);
  }
}

void PipeCompiler::lea(const Gp& dst, const Mem& src) noexcept {
  Gp base = src.baseReg().as<Gp>();

  if (src.hasIndex()) {
    add_ext(dst, base, src.indexReg().as<Gp>(), 1u << src.shift(), src.offsetLo32());
  }
  else if (src.offsetLo32()) {
    cc->add(dst, base, src.offsetLo32());
  }
  else {
    cc->mov(dst, base);
  }
}

// bl::Pipeline::PipeCompiler - Vector Instructions - Constants
// ============================================================

//! Floating point mode is used in places that are generic and implement various functionality that needs more
//! than a single instruction. Typically implementing either higher level concepts or missing functionality.
enum FloatMode : uint32_t {
  //! Used by non-floating point instructions.
  kNone = 0,

  kF32S = 1,
  kF64S = 2,
  kF32V = 3,
  kF64V = 4
};

enum class ElementSize : uint32_t {
  kNA = 0,
  k8 = 0,
  k16 = 1,
  k32 = 2,
  k64 = 3
};

enum class SameVecOp : uint32_t {
  kNone = 0,
  kZero = 1,
  kOnes = 2,
  kSrc = 3
};

enum class VecPart : uint32_t {
  kNA = 0,
  kLo = 1,
  kHi = 2
};

enum class NarrowingOp : uint32_t  {
  kNone,
  kI16ToI8,
  kI16ToU8,
  kU16ToU8,
  kI32ToI16,
  kI32ToU16,
  kU32ToU16,
  kI64ToI32,
  kI64ToU32,
  kU64ToU32
};

enum class NarrowingMode : uint32_t {
  kTruncate,
  kSaturateSToU,
  kSaturateSToS,
  kSaturateUToU
};

// bl::Pipeline::PipeCompiler - Vector Instructions - Opcode Information
// =====================================================================

struct OpcodeVInfo {
  //! \name Members
  //! \{

  uint32_t instId        : 13;
  ASIMDExt asimdExt      : 6;
  uint32_t commutative   : 1;
  uint32_t comparison    : 1;
  uint32_t reverse       : 1;
  SameVecOp sameVecOp    : 2;
  FloatMode floatMode    : 3;
  ElementSize dstElement : 3;
  VecPart dstPart        : 2;
  ElementSize srcElement : 3;
  VecPart srcPart        : 2;
  uint32_t imm           : 8;
  uint32_t reserved2     : 19;

  //! \}
};

#define DEFINE_OP(instId, ext, commutative, comparison, reverse, sameVecOp, floatMode, dstElement, dstPart, srcElement, srcPart, imm) \
  OpcodeVInfo {              \
    instId,                  \
    ASIMDExt::ext,           \
    commutative,             \
    comparison,              \
    reverse,                 \
    SameVecOp::sameVecOp,    \
    FloatMode::floatMode,    \
    ElementSize::dstElement, \
    VecPart::dstPart,        \
    ElementSize::srcElement, \
    VecPart::srcPart,        \
    imm,                     \
    0                        \
  }

static constexpr OpcodeVInfo opcodeInfo2V[size_t(OpcodeVV::kMaxValue) + 1] = {
  DEFINE_OP(Inst::kIdMov_v          , kIntrin, 0, 0, 0, kNone, kNone, kNA, kNA, kNA, kNA, 0x00u), // kMov.
  DEFINE_OP(Inst::kIdMov_v          , kIntrin, 0, 0, 0, kNone, kNone, k64, kLo, k64, kLo, 0x00u), // kMovU64.
  DEFINE_OP(Inst::kIdDup_v          , kIntrin, 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kBroadcastU8Z.
  DEFINE_OP(Inst::kIdDup_v          , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kBroadcastU16Z.
  DEFINE_OP(Inst::kIdDup_v          , kIntrin, 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kBroadcastU8.
  DEFINE_OP(Inst::kIdDup_v          , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kBroadcastU16.
  DEFINE_OP(Inst::kIdDup_v          , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kBroadcastU32.
  DEFINE_OP(Inst::kIdDup_v          , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kBroadcastU64.
  DEFINE_OP(Inst::kIdDup_v          , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kBroadcastF32.
  DEFINE_OP(Inst::kIdDup_v          , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kBroadcastF64.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kBroadcastV128_U32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kBroadcastV128_U64.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kBroadcastV128_F32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kBroadcastV128_F64.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kBroadcastV256_U32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kBroadcastV256_U64.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kBroadcastV256_F32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kBroadcastV256_F64.
  DEFINE_OP(Inst::kIdAbs_v          , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kAbsI8.
  DEFINE_OP(Inst::kIdAbs_v          , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kAbsI16.
  DEFINE_OP(Inst::kIdAbs_v          , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kAbsI32.
  DEFINE_OP(Inst::kIdAbs_v          , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kAbsI64.
  DEFINE_OP(Inst::kIdMvn_v          , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kNotU32.
  DEFINE_OP(Inst::kIdMvn_v          , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kNotU64.
  DEFINE_OP(Inst::kIdSshll_v        , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k8 , kLo, 0x00u), // kCvtI8LoToI16
  DEFINE_OP(Inst::kIdSshll2_v       , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k8 , kHi, 0x00u), // kCvtI8HiToI16
  DEFINE_OP(Inst::kIdUshll_v        , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k8 , kLo, 0x00u), // kCvtU8LoToU16
  DEFINE_OP(Inst::kIdUshll2_v       , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k8 , kHi, 0x00u), // kCvtU8HiToU16
  DEFINE_OP(Inst::kIdSshll_v        , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k8 , kLo, 0x00u), // kCvtI8ToI32
  DEFINE_OP(Inst::kIdUshll_v        , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k8 , kLo, 0x00u), // kCvtU8ToU32
  DEFINE_OP(Inst::kIdSshll_v        , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k16, kLo, 0x00u), // kCvtI16LoToI32
  DEFINE_OP(Inst::kIdSshll2_v       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k16, kHi, 0x00u), // kCvtI16HiToI32
  DEFINE_OP(Inst::kIdUshll_v        , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k16, kLo, 0x00u), // kCvtU16LoToU32
  DEFINE_OP(Inst::kIdUshll2_v       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k16, kHi, 0x00u), // kCvtU16HiToU32
  DEFINE_OP(Inst::kIdSshll_v        , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k32, kLo, 0x00u), // kCvtI32LoToI64
  DEFINE_OP(Inst::kIdSshll2_v       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k32, kHi, 0x00u), // kCvtI32HiToI64
  DEFINE_OP(Inst::kIdUshll_v        , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k32, kLo, 0x00u), // kCvtU32LoToU64
  DEFINE_OP(Inst::kIdUshll2_v       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k32, kHi, 0x00u), // kCvtU32HiToU64
  DEFINE_OP(Inst::kIdFabs_v         , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kAbsF32.
  DEFINE_OP(Inst::kIdFabs_v         , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kAbsF64.
  DEFINE_OP(Inst::kIdFneg_v         , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kNegF32.
  DEFINE_OP(Inst::kIdFneg_v         , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kNegF64.
  DEFINE_OP(Inst::kIdMvn_v          , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kNotF32.
  DEFINE_OP(Inst::kIdMvn_v          , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kNotF64.
  DEFINE_OP(Inst::kIdFrintz_v       , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kTruncF32S.
  DEFINE_OP(Inst::kIdFrintz_v       , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kTruncF64S.
  DEFINE_OP(Inst::kIdFrintz_v       , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kTruncF32.
  DEFINE_OP(Inst::kIdFrintz_v       , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kTruncF64.
  DEFINE_OP(Inst::kIdFrintm_v       , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kFloorF32S.
  DEFINE_OP(Inst::kIdFrintm_v       , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kFloorF64S.
  DEFINE_OP(Inst::kIdFrintm_v       , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kFloorF32.
  DEFINE_OP(Inst::kIdFrintm_v       , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kFloorF64.
  DEFINE_OP(Inst::kIdFrintp_v       , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kCeilF32S.
  DEFINE_OP(Inst::kIdFrintp_v       , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kCeilF64S.
  DEFINE_OP(Inst::kIdFrintp_v       , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCeilF32.
  DEFINE_OP(Inst::kIdFrintp_v       , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kCeilF64.
  DEFINE_OP(Inst::kIdFrintn_v       , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kRoundF32S.
  DEFINE_OP(Inst::kIdFrintn_v       , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kRoundF64S.
  DEFINE_OP(Inst::kIdFrintn_v       , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kRoundF32.
  DEFINE_OP(Inst::kIdFrintn_v       , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kRoundF64.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kRcpF32.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kRcpF64.
  DEFINE_OP(Inst::kIdFsqrt_v        , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kSqrtF32S.
  DEFINE_OP(Inst::kIdFsqrt_v        , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kSqrtF64S.
  DEFINE_OP(Inst::kIdFsqrt_v        , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kSqrtF32.
  DEFINE_OP(Inst::kIdFsqrt_v        , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kSqrtF64.
  DEFINE_OP(Inst::kIdFcvt_v         , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k32, kNA, 0x00u), // kCvtF32ToF64S.
  DEFINE_OP(Inst::kIdFcvt_v         , kASIMD , 0, 0, 0, kNone, kF64S, k32, kNA, k64, kNA, 0x00u), // kCvtF64ToF32S.
  DEFINE_OP(Inst::kIdScvtf_v        , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCvtI32ToF32.
  DEFINE_OP(Inst::kIdFcvtl_v        , kASIMD , 0, 0, 0, kNone, kF32V, k64, kNA, k32, kLo, 0x00u), // kCvtF32LoToF64.
  DEFINE_OP(Inst::kIdFcvtl2_v       , kASIMD , 0, 0, 0, kNone, kF32V, k64, kNA, k32, kHi, 0x00u), // kCvtF32HiToF64.
  DEFINE_OP(Inst::kIdFcvtn_v        , kASIMD , 0, 0, 0, kNone, kF64V, k32, kLo, k64, kNA, 0x00u), // kCvtF64ToF32Lo.
  DEFINE_OP(Inst::kIdFcvtn2_v       , kASIMD , 0, 0, 0, kNone, kF64V, k32, kHi, k64, kNA, 0x00u), // kCvtF64ToF32Hi.
  DEFINE_OP(Inst::kIdSshll_v        , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k32, kLo, 0x00u), // kCvtI32LoToF64.
  DEFINE_OP(Inst::kIdSshll2_v       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k32, kHi, 0x00u), // kCvtI32HiToF64.
  DEFINE_OP(Inst::kIdFcvtzs_v       , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCvtTruncF32ToI32.
  DEFINE_OP(Inst::kIdFcvtzs_v       , kIntrin, 0, 0, 0, kNone, kF64V, k32, kLo, k64, kLo, 0x00u), // kCvtTruncF64ToI32Lo.
  DEFINE_OP(Inst::kIdFcvtzs_v       , kIntrin, 0, 0, 0, kNone, kF64V, k32, kHi, k64, kHi, 0x00u), // kCvtTruncF64ToI32Hi.
  DEFINE_OP(Inst::kIdFcvtns_v       , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCvtRoundF32ToI32.
  DEFINE_OP(Inst::kIdFcvtns_v       , kIntrin, 0, 0, 0, kNone, kF64V, k32, kLo, k64, kLo, 0x00u), // kCvtRoundF64ToI32Lo.
  DEFINE_OP(Inst::kIdFcvtns_v       , kIntrin, 0, 0, 0, kNone, kF64V, k32, kHi, k64, kHi, 0x00u)  // kCvtRoundF64ToI32Hi.
};

static constexpr OpcodeVInfo opcodeInfo2VS[size_t(OpcodeVR::kMaxValue) + 1] = {
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kMov.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kMovU32.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kMovU64.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kInsertU8.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kInsertU16.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInsertU32.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kInsertU64.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kExtractU8.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kExtractU16.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kExtractU32.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kExtractU64.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, kNA, kNA, 0x00u), // kCvtIntToF32.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, kNA, kNA, 0x00u), // kCvtIntToF64.
  DEFINE_OP(Inst::kIdFcvtzs_v       , kASIMD , 0, 0, 0, kNone, kF32S, kNA, kNA, k32, kNA, 0x00u), // kCvtTruncF32ToInt.
  DEFINE_OP(Inst::kIdFcvtns_v       , kASIMD , 0, 0, 0, kNone, kF32S, kNA, kNA, k32, kNA, 0x00u), // kCvtRoundF32ToInt.
  DEFINE_OP(Inst::kIdFcvtzs_v       , kASIMD , 0, 0, 0, kNone, kF64S, kNA, kNA, k64, kNA, 0x00u), // kCvtTruncF64ToInt.
  DEFINE_OP(Inst::kIdFcvtns_v       , kASIMD , 0, 0, 0, kNone, kF64S, kNA, kNA, k64, kNA, 0x00u)  // kCvtRoundF64ToInt.
};

static constexpr OpcodeVInfo opcodeInfo2VI[size_t(OpcodeVVI::kMaxValue) + 1] = {
  DEFINE_OP(Inst::kIdShl_v          , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kSllU16.
  DEFINE_OP(Inst::kIdShl_v          , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kSllU32.
  DEFINE_OP(Inst::kIdShl_v          , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kSllU64.
  DEFINE_OP(Inst::kIdUshr_v         , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kSrlU16.
  DEFINE_OP(Inst::kIdUshr_v         , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kSrlU32.
  DEFINE_OP(Inst::kIdUshr_v         , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kSrlU64.
  DEFINE_OP(Inst::kIdSshr_v         , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kSraI16.
  DEFINE_OP(Inst::kIdSshr_v         , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kSraI32.
  DEFINE_OP(Inst::kIdSshr_v         , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kSraI64.
  DEFINE_OP(Inst::kIdExt_v          , kIntrin, 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kSllbU128.
  DEFINE_OP(Inst::kIdExt_v          , kIntrin, 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kSrlbU128.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kSwizzleU16x4.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kSwizzleLoU16x4.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kSwizzleHiU16x4.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kSwizzleU32x4.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kSwizzleU64x2.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kSwizzleF32x4.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kSwizzleF64x2.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kSwizzleU64x4.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kSwizzleF64x4.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kExtractV128_I32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kExtractV128_I64.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kExtractV128_F32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kExtractV128_F64.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kExtractV256_I32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kExtractV256_I64.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kExtractV256_F32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kExtractV256_F64.
  DEFINE_OP(Inst::kIdUrshr_v        , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kSrlRndU16.
  DEFINE_OP(Inst::kIdUrshr_v        , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kSrlRndU32.
  DEFINE_OP(Inst::kIdUrshr_v        , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kSrlRndU64.
  DEFINE_OP(Inst::kIdUsra_v         , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kSrlAccU16.
  DEFINE_OP(Inst::kIdUsra_v         , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kSrlAccU32.
  DEFINE_OP(Inst::kIdUsra_v         , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kSrlAccU64.
  DEFINE_OP(Inst::kIdUrsra_v        , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kSrlRndAccU16.
  DEFINE_OP(Inst::kIdUrsra_v        , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kSrlRndAccU32.
  DEFINE_OP(Inst::kIdUrsra_v        , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kSrlRndAccU64.
  DEFINE_OP(Inst::kIdShrn_v         , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k16, kLo, 0x00u), // kSrlnLoU16.
  DEFINE_OP(Inst::kIdShrn2_v        , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k16, kHi, 0x00u), // kSrlnHiU16.
  DEFINE_OP(Inst::kIdShrn_v         , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k32, kLo, 0x00u), // kSrlnLoU32.
  DEFINE_OP(Inst::kIdShrn2_v        , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k32, kHi, 0x00u), // kSrlnHiU32.
  DEFINE_OP(Inst::kIdShrn_v         , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k64, kLo, 0x00u), // kSrlnLoU64.
  DEFINE_OP(Inst::kIdShrn2_v        , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k64, kHi, 0x00u), // kSrlnHiU64.
  DEFINE_OP(Inst::kIdRshrn_v        , kASIMD , 0, 0, 0, kNone, kNone, k8 , kLo, k16, kNA, 0x00u), // kSrlnRndLoU16.
  DEFINE_OP(Inst::kIdRshrn2_v       , kASIMD , 0, 0, 0, kNone, kNone, k8 , kHi, k16, kNA, 0x00u), // kSrlnRndHiU16.
  DEFINE_OP(Inst::kIdRshrn_v        , kASIMD , 0, 0, 0, kNone, kNone, k16, kLo, k32, kNA, 0x00u), // kSrlnRndLoU32.
  DEFINE_OP(Inst::kIdRshrn2_v       , kASIMD , 0, 0, 0, kNone, kNone, k16, kHi, k32, kNA, 0x00u), // kSrlnRndHiU32.
  DEFINE_OP(Inst::kIdRshrn_v        , kASIMD , 0, 0, 0, kNone, kNone, k32, kLo, k64, kNA, 0x00u), // kSrlnRndLoU64.
  DEFINE_OP(Inst::kIdRshrn2_v       , kASIMD , 0, 0, 0, kNone, kNone, k32, kHi, k64, kNA, 0x00u), // kSrlnRndHiU64.
};

static constexpr OpcodeVInfo opcodeInfo3V[size_t(OpcodeVVV::kMaxValue) + 1] = {
  DEFINE_OP(Inst::kIdAnd_v          , kASIMD , 1, 0, 0, kSrc , kNone, k8 , kNA, k8 , kNA, 0x00u), // kAndU32.
  DEFINE_OP(Inst::kIdAnd_v          , kASIMD , 1, 0, 0, kSrc , kNone, k8 , kNA, k8 , kNA, 0x00u), // kAndU64.
  DEFINE_OP(Inst::kIdOrr_v          , kASIMD , 1, 0, 0, kSrc , kNone, k8 , kNA, k8 , kNA, 0x00u), // kOrU32.
  DEFINE_OP(Inst::kIdOrr_v          , kASIMD , 1, 0, 0, kSrc , kNone, k8 , kNA, k8 , kNA, 0x00u), // kOrU64.
  DEFINE_OP(Inst::kIdEor_v          , kASIMD , 1, 0, 0, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kXorU32.
  DEFINE_OP(Inst::kIdEor_v          , kASIMD , 1, 0, 0, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kXorU64.
  DEFINE_OP(Inst::kIdBic_v          , kASIMD , 0, 0, 1, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kAndnU32.
  DEFINE_OP(Inst::kIdBic_v          , kASIMD , 0, 0, 1, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kAndnU64.
  DEFINE_OP(Inst::kIdBic_v          , kASIMD , 0, 0, 0, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kBicU32.
  DEFINE_OP(Inst::kIdBic_v          , kASIMD , 0, 0, 0, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kBicU64.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 1, 0, 0, kSrc , kNone, k8 , kNA, k8 , kNA, 0x00u), // kAvgrU8.
  DEFINE_OP(Inst::kIdNone           , kASIMD , 1, 0, 0, kSrc , kNone, k16, kNA, k16, kNA, 0x00u), // kAvgrU16.
  DEFINE_OP(Inst::kIdAdd_v          , kASIMD , 1, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kAddU8.
  DEFINE_OP(Inst::kIdAdd_v          , kASIMD , 1, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kAddU16.
  DEFINE_OP(Inst::kIdAdd_v          , kASIMD , 1, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kAddU32.
  DEFINE_OP(Inst::kIdAdd_v          , kASIMD , 1, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kAddU64.
  DEFINE_OP(Inst::kIdSub_v          , kASIMD , 0, 0, 0, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kSubU8.
  DEFINE_OP(Inst::kIdSub_v          , kASIMD , 0, 0, 0, kZero, kNone, k16, kNA, k16, kNA, 0x00u), // kSubU16.
  DEFINE_OP(Inst::kIdSub_v          , kASIMD , 0, 0, 0, kZero, kNone, k32, kNA, k32, kNA, 0x00u), // kSubU32.
  DEFINE_OP(Inst::kIdSub_v          , kASIMD , 0, 0, 0, kZero, kNone, k64, kNA, k64, kNA, 0x00u), // kSubU64.
  DEFINE_OP(Inst::kIdSqadd_v        , kASIMD , 1, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kAddsI8.
  DEFINE_OP(Inst::kIdUqadd_v        , kASIMD , 1, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kAddsU8.
  DEFINE_OP(Inst::kIdSqadd_v        , kASIMD , 1, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kAddsI16.
  DEFINE_OP(Inst::kIdUqadd_v        , kASIMD , 1, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kAddsU16.
  DEFINE_OP(Inst::kIdSqsub_v        , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kSubsI8.
  DEFINE_OP(Inst::kIdUqsub_v        , kASIMD , 0, 0, 0, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kSubsU8.
  DEFINE_OP(Inst::kIdSqsub_v        , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kSubsI16.
  DEFINE_OP(Inst::kIdUqsub_v        , kASIMD , 0, 0, 0, kZero, kNone, k16, kNA, k16, kNA, 0x00u), // kSubsU16.
  DEFINE_OP(Inst::kIdMul_v          , kASIMD , 1, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kMulU16.
  DEFINE_OP(Inst::kIdMul_v          , kASIMD , 1, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kMulU32.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 1, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kMulU64.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 1, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kMulhI16.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 1, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kMulhU16.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 1, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kMulU64_LoU32.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 1, 0, 0, kNone, kNone, k32, kNA, k16, kNA, 0x00u), // kMHAddI16_I32.
  DEFINE_OP(Inst::kIdSmin_v         , kASIMD , 1, 0, 0, kSrc , kNone, k8 , kNA, k8 , kNA, 0x00u), // kMinI8.
  DEFINE_OP(Inst::kIdUmin_v         , kASIMD , 1, 0, 0, kSrc , kNone, k8 , kNA, k8 , kNA, 0x00u), // kMinU8.
  DEFINE_OP(Inst::kIdSmin_v         , kASIMD , 1, 0, 0, kSrc , kNone, k16, kNA, k16, kNA, 0x00u), // kMinI16.
  DEFINE_OP(Inst::kIdUmin_v         , kASIMD , 1, 0, 0, kSrc , kNone, k16, kNA, k16, kNA, 0x00u), // kMinU16.
  DEFINE_OP(Inst::kIdSmin_v         , kASIMD , 1, 0, 0, kSrc , kNone, k32, kNA, k32, kNA, 0x00u), // kMinI32.
  DEFINE_OP(Inst::kIdUmin_v         , kASIMD , 1, 0, 0, kSrc , kNone, k32, kNA, k32, kNA, 0x00u), // kMinU32.
  DEFINE_OP(Inst::kIdCmgt_v         , kIntrin, 1, 0, 0, kSrc , kNone, k64, kNA, k64, kNA, 0x00u), // kMinI64.
  DEFINE_OP(Inst::kIdCmhi_v         , kIntrin, 1, 0, 0, kSrc , kNone, k64, kNA, k64, kNA, 0x00u), // kMinU64.
  DEFINE_OP(Inst::kIdSmax_v         , kASIMD , 1, 0, 0, kSrc , kNone, k8 , kNA, k8 , kNA, 0x00u), // kMaxI8.
  DEFINE_OP(Inst::kIdUmax_v         , kASIMD , 1, 0, 0, kSrc , kNone, k8 , kNA, k8 , kNA, 0x00u), // kMaxU8.
  DEFINE_OP(Inst::kIdSmax_v         , kASIMD , 1, 0, 0, kSrc , kNone, k16, kNA, k16, kNA, 0x00u), // kMaxI16.
  DEFINE_OP(Inst::kIdUmax_v         , kASIMD , 1, 0, 0, kSrc , kNone, k16, kNA, k16, kNA, 0x00u), // kMaxU16.
  DEFINE_OP(Inst::kIdSmax_v         , kASIMD , 1, 0, 0, kSrc , kNone, k32, kNA, k32, kNA, 0x00u), // kMaxI32.
  DEFINE_OP(Inst::kIdUmax_v         , kASIMD , 1, 0, 0, kSrc , kNone, k32, kNA, k32, kNA, 0x00u), // kMaxU32.
  DEFINE_OP(Inst::kIdCmgt_v         , kIntrin, 1, 0, 0, kSrc , kNone, k64, kNA, k64, kNA, 0x01u), // kMaxI64.
  DEFINE_OP(Inst::kIdCmhi_v         , kIntrin, 1, 0, 0, kSrc , kNone, k64, kNA, k64, kNA, 0x01u), // kMaxU64.
  DEFINE_OP(Inst::kIdCmeq_v         , kASIMD , 1, 1, 0, kOnes, kNone, k8 , kNA, k8 , kNA, 0x00u), // kCmpEqU8.
  DEFINE_OP(Inst::kIdCmeq_v         , kASIMD , 1, 1, 0, kOnes, kNone, k16, kNA, k16, kNA, 0x00u), // kCmpEqU16.
  DEFINE_OP(Inst::kIdCmeq_v         , kASIMD , 1, 1, 0, kOnes, kNone, k32, kNA, k32, kNA, 0x00u), // kCmpEqU32.
  DEFINE_OP(Inst::kIdCmeq_v         , kASIMD , 1, 1, 0, kOnes, kNone, k64, kNA, k64, kNA, 0x00u), // kCmpEqU64.
  DEFINE_OP(Inst::kIdCmgt_v         , kASIMD , 0, 1, 0, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kCmpGtI8.
  DEFINE_OP(Inst::kIdCmhi_v         , kASIMD , 0, 1, 0, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kCmpGtU8.
  DEFINE_OP(Inst::kIdCmgt_v         , kASIMD , 0, 1, 0, kZero, kNone, k16, kNA, k16, kNA, 0x00u), // kCmpGtI16.
  DEFINE_OP(Inst::kIdCmhi_v         , kASIMD , 0, 1, 0, kZero, kNone, k16, kNA, k16, kNA, 0x00u), // kCmpGtU16.
  DEFINE_OP(Inst::kIdCmgt_v         , kASIMD , 0, 1, 0, kZero, kNone, k32, kNA, k32, kNA, 0x00u), // kCmpGtI32.
  DEFINE_OP(Inst::kIdCmhi_v         , kASIMD , 0, 1, 0, kZero, kNone, k32, kNA, k32, kNA, 0x00u), // kCmpGtU32.
  DEFINE_OP(Inst::kIdCmgt_v         , kASIMD , 0, 1, 0, kZero, kNone, k64, kNA, k64, kNA, 0x00u), // kCmpGtI64.
  DEFINE_OP(Inst::kIdCmhi_v         , kASIMD , 0, 1, 0, kZero, kNone, k64, kNA, k64, kNA, 0x00u), // kCmpGtU64.
  DEFINE_OP(Inst::kIdCmge_v         , kASIMD , 0, 1, 0, kOnes, kNone, k8 , kNA, k8 , kNA, 0x00u), // kCmpGeI8.
  DEFINE_OP(Inst::kIdCmhs_v         , kASIMD , 0, 1, 0, kOnes, kNone, k8 , kNA, k8 , kNA, 0x00u), // kCmpGeU8.
  DEFINE_OP(Inst::kIdCmge_v         , kASIMD , 0, 1, 0, kOnes, kNone, k16, kNA, k16, kNA, 0x00u), // kCmpGeI16.
  DEFINE_OP(Inst::kIdCmhs_v         , kASIMD , 0, 1, 0, kOnes, kNone, k16, kNA, k16, kNA, 0x00u), // kCmpGeU16.
  DEFINE_OP(Inst::kIdCmge_v         , kASIMD , 0, 1, 0, kOnes, kNone, k32, kNA, k32, kNA, 0x00u), // kCmpGeI32.
  DEFINE_OP(Inst::kIdCmhs_v         , kASIMD , 0, 1, 0, kOnes, kNone, k32, kNA, k32, kNA, 0x00u), // kCmpGeU32.
  DEFINE_OP(Inst::kIdCmge_v         , kASIMD , 0, 1, 0, kOnes, kNone, k64, kNA, k64, kNA, 0x00u), // kCmpGeI64.
  DEFINE_OP(Inst::kIdCmhs_v         , kASIMD , 0, 1, 0, kOnes, kNone, k64, kNA, k64, kNA, 0x00u), // kCmpGeU64.
  DEFINE_OP(Inst::kIdCmgt_v         , kASIMD , 0, 1, 1, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kCmpLtI8.
  DEFINE_OP(Inst::kIdCmhi_v         , kASIMD , 0, 1, 1, kZero, kNone, k8 , kNA, k8 , kNA, 0x00u), // kCmpLtU8.
  DEFINE_OP(Inst::kIdCmgt_v         , kASIMD , 0, 1, 1, kZero, kNone, k16, kNA, k16, kNA, 0x00u), // kCmpLtI16.
  DEFINE_OP(Inst::kIdCmhi_v         , kASIMD , 0, 1, 1, kZero, kNone, k16, kNA, k16, kNA, 0x00u), // kCmpLtU16.
  DEFINE_OP(Inst::kIdCmgt_v         , kASIMD , 0, 1, 1, kZero, kNone, k32, kNA, k32, kNA, 0x00u), // kCmpLtI32.
  DEFINE_OP(Inst::kIdCmhi_v         , kASIMD , 0, 1, 1, kZero, kNone, k32, kNA, k32, kNA, 0x00u), // kCmpLtU32.
  DEFINE_OP(Inst::kIdCmgt_v         , kASIMD , 0, 1, 1, kZero, kNone, k64, kNA, k64, kNA, 0x00u), // kCmpLtI64.
  DEFINE_OP(Inst::kIdCmhi_v         , kASIMD , 0, 1, 1, kZero, kNone, k64, kNA, k64, kNA, 0x00u), // kCmpLtU64.
  DEFINE_OP(Inst::kIdCmge_v         , kASIMD , 0, 1, 1, kOnes, kNone, k8 , kNA, k8 , kNA, 0x00u), // kCmpLeI8.
  DEFINE_OP(Inst::kIdCmhs_v         , kASIMD , 0, 1, 1, kOnes, kNone, k8 , kNA, k8 , kNA, 0x00u), // kCmpLeU8.
  DEFINE_OP(Inst::kIdCmge_v         , kASIMD , 0, 1, 1, kOnes, kNone, k16, kNA, k16, kNA, 0x00u), // kCmpLeI16.
  DEFINE_OP(Inst::kIdCmhs_v         , kASIMD , 0, 1, 1, kOnes, kNone, k16, kNA, k16, kNA, 0x00u), // kCmpLeU16.
  DEFINE_OP(Inst::kIdCmge_v         , kASIMD , 0, 1, 1, kOnes, kNone, k32, kNA, k32, kNA, 0x00u), // kCmpLeI32.
  DEFINE_OP(Inst::kIdCmhs_v         , kASIMD , 0, 1, 1, kOnes, kNone, k32, kNA, k32, kNA, 0x00u), // kCmpLeU32.
  DEFINE_OP(Inst::kIdCmge_v         , kASIMD , 0, 1, 1, kOnes, kNone, k64, kNA, k64, kNA, 0x00u), // kCmpLeI64.
  DEFINE_OP(Inst::kIdCmhs_v         , kASIMD , 0, 1, 1, kOnes, kNone, k64, kNA, k64, kNA, 0x00u), // kCmpLeU64.
  DEFINE_OP(Inst::kIdAnd_v          , kASIMD , 1, 0, 0, kSrc , kF32V, k8 , kNA, k8 , kNA, 0x00u), // kAndF32.
  DEFINE_OP(Inst::kIdAnd_v          , kASIMD , 1, 0, 0, kSrc , kF64V, k8 , kNA, k8 , kNA, 0x00u), // kAndF64.
  DEFINE_OP(Inst::kIdOrr_v          , kASIMD , 1, 0, 0, kSrc , kF32V, k8 , kNA, k8 , kNA, 0x00u), // kOrF32.
  DEFINE_OP(Inst::kIdOrr_v          , kASIMD , 1, 0, 0, kSrc , kF64V, k8 , kNA, k8 , kNA, 0x00u), // kOrF64.
  DEFINE_OP(Inst::kIdEor_v          , kASIMD , 1, 0, 0, kZero, kF32V, k8 , kNA, k8 , kNA, 0x00u), // kXorF32.
  DEFINE_OP(Inst::kIdEor_v          , kASIMD , 1, 0, 0, kZero, kF64V, k8 , kNA, k8 , kNA, 0x00u), // kXorF64.
  DEFINE_OP(Inst::kIdBic_v          , kASIMD , 0, 0, 1, kZero, kF32V, k8 , kNA, k8 , kNA, 0x00u), // kAndnF32.
  DEFINE_OP(Inst::kIdBic_v          , kASIMD , 0, 0, 1, kZero, kF64V, k8 , kNA, k8 , kNA, 0x00u), // kAndnF64.
  DEFINE_OP(Inst::kIdBic_v          , kASIMD , 0, 0, 0, kZero, kF32V, k8 , kNA, k8 , kNA, 0x00u), // kBicF32.
  DEFINE_OP(Inst::kIdBic_v          , kASIMD , 0, 0, 0, kZero, kF64V, k8 , kNA, k8 , kNA, 0x00u), // kBicF64.
  DEFINE_OP(Inst::kIdFadd_v         , kASIMD , 1, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kAddF32S.
  DEFINE_OP(Inst::kIdFadd_v         , kASIMD , 1, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kAddF64S.
  DEFINE_OP(Inst::kIdFadd_v         , kASIMD , 1, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kAddF32.
  DEFINE_OP(Inst::kIdFadd_v         , kASIMD , 1, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kAddF64.
  DEFINE_OP(Inst::kIdFsub_v         , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kSubF32S.
  DEFINE_OP(Inst::kIdFsub_v         , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kSubF64S.
  DEFINE_OP(Inst::kIdFsub_v         , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kSubF32.
  DEFINE_OP(Inst::kIdFsub_v         , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kSubF64.
  DEFINE_OP(Inst::kIdFmul_v         , kASIMD , 1, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kMulF32S.
  DEFINE_OP(Inst::kIdFmul_v         , kASIMD , 1, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kMulF64S.
  DEFINE_OP(Inst::kIdFmul_v         , kASIMD , 1, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kMulF32.
  DEFINE_OP(Inst::kIdFmul_v         , kASIMD , 1, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kMulF64.
  DEFINE_OP(Inst::kIdFdiv_v         , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kDivF32S.
  DEFINE_OP(Inst::kIdFdiv_v         , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kDivF64S.
  DEFINE_OP(Inst::kIdFdiv_v         , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kDivF32.
  DEFINE_OP(Inst::kIdFdiv_v         , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kDivF64.
  DEFINE_OP(Inst::kIdFminnm_v       , kASIMD , 1, 0, 0, kSrc , kF32S, k32, kNA, k32, kNA, 0x00u), // kMinF32S.
  DEFINE_OP(Inst::kIdFminnm_v       , kASIMD , 1, 0, 0, kSrc , kF64S, k64, kNA, k64, kNA, 0x00u), // kMinF64S.
  DEFINE_OP(Inst::kIdFminnm_v       , kASIMD , 1, 0, 0, kSrc , kF32V, k32, kNA, k32, kNA, 0x00u), // kMinF32.
  DEFINE_OP(Inst::kIdFminnm_v       , kASIMD , 1, 0, 0, kSrc , kF64V, k64, kNA, k64, kNA, 0x00u), // kMinF64.
  DEFINE_OP(Inst::kIdFmaxnm_v       , kASIMD , 1, 0, 0, kSrc , kF32S, k32, kNA, k32, kNA, 0x00u), // kMaxF32S.
  DEFINE_OP(Inst::kIdFmaxnm_v       , kASIMD , 1, 0, 0, kSrc , kF64S, k64, kNA, k64, kNA, 0x00u), // kMaxF64S.
  DEFINE_OP(Inst::kIdFmaxnm_v       , kASIMD , 1, 0, 0, kSrc , kF32V, k32, kNA, k32, kNA, 0x00u), // kMaxF32.
  DEFINE_OP(Inst::kIdFmaxnm_v       , kASIMD , 1, 0, 0, kSrc , kF64V, k64, kNA, k64, kNA, 0x00u), // kMaxF64.
  DEFINE_OP(Inst::kIdFcmeq_v        , kASIMD , 1, 1, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kCmpEqF32S    (eq ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kASIMD , 1, 1, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kCmpEqF64S    (eq ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kASIMD , 1, 1, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCmpEqF32     (eq ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kASIMD , 1, 1, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kCmpEqF64     (eq ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kCmpNeF32S    (ne ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kCmpNeF64S    (ne ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCmpNeF32     (ne ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kCmpNeF64     (ne ordered quiet).
  DEFINE_OP(Inst::kIdFcmgt_v        , kASIMD , 0, 1, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kCmpGtF32S    (gt ordered quiet).
  DEFINE_OP(Inst::kIdFcmgt_v        , kASIMD , 0, 1, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kCmpGtF64S    (gt ordered quiet).
  DEFINE_OP(Inst::kIdFcmgt_v        , kASIMD , 0, 1, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCmpGtF32     (gt ordered quiet).
  DEFINE_OP(Inst::kIdFcmgt_v        , kASIMD , 0, 1, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kCmpGtF64     (gt ordered quiet).
  DEFINE_OP(Inst::kIdFcmge_v        , kASIMD , 0, 1, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kCmpGeF32S    (ge ordered quiet).
  DEFINE_OP(Inst::kIdFcmge_v        , kASIMD , 0, 1, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kCmpGeF64S    (ge ordered quiet).
  DEFINE_OP(Inst::kIdFcmge_v        , kASIMD , 0, 1, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCmpGeF32     (ge ordered quiet).
  DEFINE_OP(Inst::kIdFcmge_v        , kASIMD , 0, 1, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kCmpGeF64     (ge ordered quiet).
  DEFINE_OP(Inst::kIdFcmgt_v        , kASIMD , 0, 1, 1, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kCmpLtF32S    (lt ordered quiet).
  DEFINE_OP(Inst::kIdFcmgt_v        , kASIMD , 0, 1, 1, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kCmpLtF64S    (lt ordered quiet).
  DEFINE_OP(Inst::kIdFcmgt_v        , kASIMD , 0, 1, 1, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCmpLtF32     (lt ordered quiet).
  DEFINE_OP(Inst::kIdFcmgt_v        , kASIMD , 0, 1, 1, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kCmpLtF64     (lt ordered quiet).
  DEFINE_OP(Inst::kIdFcmge_v        , kASIMD , 0, 1, 1, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kCmpLeF32S    (le ordered quiet).
  DEFINE_OP(Inst::kIdFcmge_v        , kASIMD , 0, 1, 1, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kCmpLeF64S    (le ordered quiet).
  DEFINE_OP(Inst::kIdFcmge_v        , kASIMD , 0, 1, 1, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCmpLeF32     (le ordered quiet).
  DEFINE_OP(Inst::kIdFcmge_v        , kASIMD , 0, 1, 1, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kCmpLeF64     (le ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kCmpOrdF32S   (ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kCmpOrdF64S   (ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kCmpOrdF32    (ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kCmpOrdF64    (ordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x01u), // kCmpUnordF32S (unordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x01u), // kCmpUnordF64S (unordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x01u), // kCmpUnordF32  (unordered quiet).
  DEFINE_OP(Inst::kIdFcmeq_v        , kIntrin, 1, 1, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x01u), // kCmpUnordF64  (unordered quiet).
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kHAddF64.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kCombineLoHiU64.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kCombineLoHiF64.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kSrc , kNone, k64, kNA, k64, kNA, 0x00u), // kCombineHiLoU64.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kSrc , kNone, k64, kNA, k64, kNA, 0x00u), // kCombineHiLoF64.
  DEFINE_OP(Inst::kIdZip1_v         , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kInterleaveLoU8.
  DEFINE_OP(Inst::kIdZip2_v         , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kInterleaveHiU8.
  DEFINE_OP(Inst::kIdZip1_v         , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kInterleaveLoU16.
  DEFINE_OP(Inst::kIdZip2_v         , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kInterleaveHiU16.
  DEFINE_OP(Inst::kIdZip1_v         , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInterleaveLoU32.
  DEFINE_OP(Inst::kIdZip2_v         , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInterleaveHiU32.
  DEFINE_OP(Inst::kIdZip1_v         , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kInterleaveLoU64.
  DEFINE_OP(Inst::kIdZip2_v         , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kInterleaveHiU64.
  DEFINE_OP(Inst::kIdZip1_v         , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInterleaveLoF32.
  DEFINE_OP(Inst::kIdZip2_v         , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInterleaveHiF32.
  DEFINE_OP(Inst::kIdZip1_v         , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kInterleaveLoF64.
  DEFINE_OP(Inst::kIdZip2_v         , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kInterleaveHiF64.
  DEFINE_OP(Inst::kIdSqxtn_v        , kIntrin, 0, 0, 0, kNone, kNone, k8 , kNA, k16, kNA, 0x00u), // kPacksI16_I8.
  DEFINE_OP(Inst::kIdSqxtun_v       , kIntrin, 0, 0, 0, kNone, kNone, k8 , kNA, k16, kNA, 0x00u), // kPacksI16_U8.
  DEFINE_OP(Inst::kIdSqxtn_v        , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k32, kNA, 0x00u), // kPacksI32_I16.
  DEFINE_OP(Inst::kIdSqxtun_v       , kIntrin, 0, 0, 0, kNone, kNone, k16, kNA, k32, kNA, 0x00u), // kPacksI32_U16.
  DEFINE_OP(Inst::kIdTbl_v          , kASIMD , 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kSwizzlev_U8.
  DEFINE_OP(Inst::kIdSmull_v        , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k8 , kLo, 0x00u), // kMulwLoI8.
  DEFINE_OP(Inst::kIdUmull_v        , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k8 , kLo, 0x00u), // kMulwLoU8.
  DEFINE_OP(Inst::kIdSmull2_v       , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k8 , kHi, 0x00u), // kMulwHiI8.
  DEFINE_OP(Inst::kIdUmull2_v       , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k8 , kHi, 0x00u), // kMulwHiU8.
  DEFINE_OP(Inst::kIdSmull_v        , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k16, kLo, 0x00u), // kMulwLoI16.
  DEFINE_OP(Inst::kIdUmull_v        , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k16, kLo, 0x00u), // kMulwLoU16.
  DEFINE_OP(Inst::kIdSmull2_v       , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k16, kHi, 0x00u), // kMulwHiI16.
  DEFINE_OP(Inst::kIdUmull2_v       , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k16, kHi, 0x00u), // kMulwHiU16.
  DEFINE_OP(Inst::kIdSmull_v        , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k32, kLo, 0x00u), // kMulwLoI32.
  DEFINE_OP(Inst::kIdUmull_v        , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k32, kLo, 0x00u), // kMulwLoU32.
  DEFINE_OP(Inst::kIdSmull2_v       , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k32, kHi, 0x00u), // kMulwHiI32.
  DEFINE_OP(Inst::kIdUmull2_v       , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k32, kHi, 0x00u), // kMulwHiU32.
  DEFINE_OP(Inst::kIdSmlal_v        , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k8 , kLo, 0x00u), // kMAddwLoI8.
  DEFINE_OP(Inst::kIdUmlal_v        , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k8 , kLo, 0x00u), // kMAddwLoU8.
  DEFINE_OP(Inst::kIdSmlal2_v       , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k8 , kHi, 0x00u), // kMAddwHiI8.
  DEFINE_OP(Inst::kIdUmlal2_v       , kASIMD , 0, 0, 0, kNone, kNone, k16, kNA, k8 , kHi, 0x00u), // kMAddwHiU8.
  DEFINE_OP(Inst::kIdSmlal_v        , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k16, kLo, 0x00u), // kMAddwLoI16.
  DEFINE_OP(Inst::kIdUmlal_v        , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k16, kLo, 0x00u), // kMAddwLoU16.
  DEFINE_OP(Inst::kIdSmlal2_v       , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k16, kHi, 0x00u), // kMAddwHiI16.
  DEFINE_OP(Inst::kIdUmlal2_v       , kASIMD , 0, 0, 0, kNone, kNone, k32, kNA, k16, kHi, 0x00u), // kMAddwHiU16.
  DEFINE_OP(Inst::kIdSmlal_v        , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k32, kLo, 0x00u), // kMAddwLoI32.
  DEFINE_OP(Inst::kIdUmlal_v        , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k32, kLo, 0x00u), // kMAddwLoU32.
  DEFINE_OP(Inst::kIdSmlal2_v       , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k32, kHi, 0x00u), // kMAddwHiI32.
  DEFINE_OP(Inst::kIdUmlal2_v       , kASIMD , 0, 0, 0, kNone, kNone, k64, kNA, k32, kHi, 0x00u)  // kMAddwHiU32.
};

static constexpr OpcodeVInfo opcodeInfo3VI[size_t(OpcodeVVVI::kMaxValue) + 1] = {
  DEFINE_OP(Inst::kIdExt_v          , kIntrin, 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kAlignr_U128.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInterleaveShuffleU32x4.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kInterleaveShuffleU64x2.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInterleaveShuffleF32x4.
  DEFINE_OP(Inst::kIdNone           , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kInterleaveShuffleF64x2.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInsertV128_U32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInsertV128_F32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kInsertV128_U64.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kInsertV128_F64.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInsertV256_U32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kInsertV256_F32.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u), // kInsertV256_U64.
  DEFINE_OP(0                       , kIntrin, 0, 0, 0, kNone, kNone, k64, kNA, k64, kNA, 0x00u)  // kInsertV256_F64.
};

static constexpr OpcodeVInfo opcodeInfo4V[size_t(OpcodeVVV::kMaxValue) + 1] = {
  DEFINE_OP(Inst::kIdBsl_v          , kIntrin, 0, 0, 0, kNone, kNone, k8 , kNA, k8 , kNA, 0x00u), // kBlendV_U8.
  DEFINE_OP(Inst::kIdMla_v          , kIntrin, 1, 0, 0, kNone, kNone, k16, kNA, k16, kNA, 0x00u), // kMAddU16.
  DEFINE_OP(Inst::kIdMla_v          , kIntrin, 1, 0, 0, kNone, kNone, k32, kNA, k32, kNA, 0x00u), // kMAddU32.
  DEFINE_OP(Inst::kIdFmadd_v        , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kMAddF32S.
  DEFINE_OP(Inst::kIdFmadd_v        , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kMAddF64S.
  DEFINE_OP(Inst::kIdFmla_v         , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kMAddF32.
  DEFINE_OP(Inst::kIdFmla_v         , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kMAddF64.
  DEFINE_OP(Inst::kIdFnmsub_v       , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kMSubF32S.
  DEFINE_OP(Inst::kIdFnmsub_v       , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kMSubF64S.
  DEFINE_OP(Inst::kIdFmla_v         , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x01u), // kMSubF32.
  DEFINE_OP(Inst::kIdFmla_v         , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x01u), // kMSubF64.
  DEFINE_OP(Inst::kIdFmsub_v        , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kNMAddF32S.
  DEFINE_OP(Inst::kIdFmsub_v        , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kNMAddF64S.
  DEFINE_OP(Inst::kIdFmls_v         , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x00u), // kNMAddF32.
  DEFINE_OP(Inst::kIdFmls_v         , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x00u), // kNMAddF64.
  DEFINE_OP(Inst::kIdFnmadd_v       , kASIMD , 0, 0, 0, kNone, kF32S, k32, kNA, k32, kNA, 0x00u), // kNMSubF32S.
  DEFINE_OP(Inst::kIdFnmadd_v       , kASIMD , 0, 0, 0, kNone, kF64S, k64, kNA, k64, kNA, 0x00u), // kNMSubF64S.
  DEFINE_OP(Inst::kIdFmls_v         , kASIMD , 0, 0, 0, kNone, kF32V, k32, kNA, k32, kNA, 0x01u), // kNMSubF32.
  DEFINE_OP(Inst::kIdFmls_v         , kASIMD , 0, 0, 0, kNone, kF64V, k64, kNA, k64, kNA, 0x01u)  // kNMSubF64.
};

#undef DEFINE_OP

struct OpcodeVMInfo {
  //! \name Members
  //! \{

  uint32_t cvtOp      : 16;
  uint32_t memSize    : 8;
  ElementSize element : 3;

  //! \}
};

#define DEFINE_OP(cvtOp, size, element) OpcodeVMInfo { uint32_t(cvtOp), size, ElementSize::element }

static constexpr OpcodeVMInfo opcodeInfo2VM[size_t(OpcodeVM::kMaxValue) + 1] = {
  DEFINE_OP(0                       ,  1, k8 ), // kLoad8.
  DEFINE_OP(0                       ,  2, k16), // kLoad16_U16.
  DEFINE_OP(0                       ,  4, k32), // kLoad32_U32.
  DEFINE_OP(0                       ,  4, k32), // kLoad32_F32.
  DEFINE_OP(0                       ,  8, k32), // kLoad64_U32.
  DEFINE_OP(0                       ,  8, k64), // kLoad64_U64.
  DEFINE_OP(0                       ,  8, k32), // kLoad64_F32.
  DEFINE_OP(0                       ,  8, k64), // kLoad64_F64.
  DEFINE_OP(0                       , 16, k32), // kLoad128_U32.
  DEFINE_OP(0                       , 16, k64), // kLoad128_U64.
  DEFINE_OP(0                       , 16, k32), // kLoad128_F32.
  DEFINE_OP(0                       , 16, k64), // kLoad128_F64.
  DEFINE_OP(0                       , 32, k32), // kLoad256_U32.
  DEFINE_OP(0                       , 32, k64), // kLoad256_U64.
  DEFINE_OP(0                       , 32, k32), // kLoad256_F32.
  DEFINE_OP(0                       , 32, k64), // kLoad256_F64.
  DEFINE_OP(0                       , 64, k32), // kLoad512_U32.
  DEFINE_OP(0                       , 64, k64), // kLoad512_U64.
  DEFINE_OP(0                       , 64, k32), // kLoad512_F32.
  DEFINE_OP(0                       , 64, k64), // kLoad512_F64.
  DEFINE_OP(0                       ,  0, k32), // kLoadN_U32.
  DEFINE_OP(0                       ,  0, k64), // kLoadN_U64.
  DEFINE_OP(0                       ,  0, k32), // kLoadN_F32.
  DEFINE_OP(0                       ,  0, k64), // kLoadN_F64.
  DEFINE_OP(1                       ,  2, kNA), // kLoadCvt16_U8ToU64.
  DEFINE_OP(1                       ,  4, kNA), // kLoadCvt32_U8ToU64.
  DEFINE_OP(1                       ,  8, kNA), // kLoadCvt64_U8ToU64.
  DEFINE_OP(OpcodeVV::kCvtI8LoToI16 ,  4, kNA), // kLoadCvt32_I8ToI16.
  DEFINE_OP(OpcodeVV::kCvtU8LoToU16 ,  4, kNA), // kLoadCvt32_U8ToU16.
  DEFINE_OP(OpcodeVV::kCvtI8ToI32   ,  4, kNA), // kLoadCvt32_I8ToI32.
  DEFINE_OP(OpcodeVV::kCvtU8ToU32   ,  4, kNA), // kLoadCvt32_U8ToU32.
  DEFINE_OP(OpcodeVV::kCvtI16LoToI32,  4, kNA), // kLoadCvt32_I16ToI32.
  DEFINE_OP(OpcodeVV::kCvtU16LoToU32,  4, kNA), // kLoadCvt32_U16ToU32.
  DEFINE_OP(OpcodeVV::kCvtI32LoToI64,  4, kNA), // kLoadCvt32_I32ToI64.
  DEFINE_OP(OpcodeVV::kCvtU32LoToU64,  4, kNA), // kLoadCvt32_U32ToU64.
  DEFINE_OP(OpcodeVV::kCvtI8LoToI16 ,  8, kNA), // kLoadCvt64_I8ToI16.
  DEFINE_OP(OpcodeVV::kCvtU8LoToU16 ,  8, kNA), // kLoadCvt64_U8ToU16.
  DEFINE_OP(OpcodeVV::kCvtI8ToI32   ,  8, kNA), // kLoadCvt64_I8ToI32.
  DEFINE_OP(OpcodeVV::kCvtU8ToU32   ,  8, kNA), // kLoadCvt64_U8ToU32.
  DEFINE_OP(OpcodeVV::kCvtI16LoToI32,  8, kNA), // kLoadCvt64_I16ToI32.
  DEFINE_OP(OpcodeVV::kCvtU16LoToU32,  8, kNA), // kLoadCvt64_U16ToU32.
  DEFINE_OP(OpcodeVV::kCvtI32LoToI64,  8, kNA), // kLoadCvt64_I32ToI64.
  DEFINE_OP(OpcodeVV::kCvtU32LoToU64,  8, kNA), // kLoadCvt64_U32ToU64.
  DEFINE_OP(OpcodeVV::kCvtI8LoToI16 , 16, kNA), // kLoadCvt128_I8ToI16.
  DEFINE_OP(OpcodeVV::kCvtU8LoToU16 , 16, kNA), // kLoadCvt128_U8ToU16.
  DEFINE_OP(OpcodeVV::kCvtI8ToI32   , 16, kNA), // kLoadCvt128_I8ToI32.
  DEFINE_OP(OpcodeVV::kCvtU8ToU32   , 16, kNA), // kLoadCvt128_U8ToU32.
  DEFINE_OP(OpcodeVV::kCvtI16LoToI32, 16, kNA), // kLoadCvt128_I16ToI32.
  DEFINE_OP(OpcodeVV::kCvtU16LoToU32, 16, kNA), // kLoadCvt128_U16ToU32.
  DEFINE_OP(OpcodeVV::kCvtI32LoToI64, 16, kNA), // kLoadCvt128_I32ToI64.
  DEFINE_OP(OpcodeVV::kCvtU32LoToU64, 16, kNA), // kLoadCvt128_U32ToU64.
  DEFINE_OP(OpcodeVV::kCvtI8LoToI16 , 32, kNA), // kLoadCvt256_I8ToI16.
  DEFINE_OP(OpcodeVV::kCvtU8LoToU16 , 32, kNA), // kLoadCvt256_U8ToU16.
  DEFINE_OP(OpcodeVV::kCvtI16LoToI32, 32, kNA), // kLoadCvt256_I16ToI32.
  DEFINE_OP(OpcodeVV::kCvtU16LoToU32, 32, kNA), // kLoadCvt256_U16ToU32.
  DEFINE_OP(OpcodeVV::kCvtI32LoToI64, 32, kNA), // kLoadCvt256_I32ToI64.
  DEFINE_OP(OpcodeVV::kCvtU32LoToU64, 32, kNA), // kLoadCvt256_U32ToU64.
  DEFINE_OP(1                       ,  0, kNA), // kLoadCvtN_U8ToU64.
  DEFINE_OP(OpcodeVV::kCvtI8LoToI16 ,  0, kNA), // kLoadCvtN_I8ToI16.
  DEFINE_OP(OpcodeVV::kCvtU8LoToU16 ,  0, kNA), // kLoadCvtN_U8ToU16.
  DEFINE_OP(OpcodeVV::kCvtI8ToI32   ,  0, kNA), // kLoadCvtN_I8ToI32.
  DEFINE_OP(OpcodeVV::kCvtU8ToU32   ,  0, kNA), // kLoadCvtN_U8ToU32.
  DEFINE_OP(OpcodeVV::kCvtI16LoToI32,  0, kNA), // kLoadCvtN_I16ToI32.
  DEFINE_OP(OpcodeVV::kCvtU16LoToU32,  0, kNA), // kLoadCvtN_U16ToU32.
  DEFINE_OP(OpcodeVV::kCvtI32LoToI64,  0, kNA), // kLoadCvtN_I32ToI64.
  DEFINE_OP(OpcodeVV::kCvtU32LoToU64,  0, kNA), // kLoadCvtN_U32ToU64.
  DEFINE_OP(0                       ,  1, k8 ), // kLoadInsertU8.
  DEFINE_OP(0                       ,  2, k16), // kLoadInsertU16.
  DEFINE_OP(0                       ,  4, k32), // kLoadInsertU32.
  DEFINE_OP(0                       ,  8, k64), // kLoadInsertU64.
  DEFINE_OP(0                       ,  4, k32), // kLoadInsertF32.
  DEFINE_OP(0                       ,  8, k64), // kLoadInsertF32x2.
  DEFINE_OP(0                       ,  8, k64)  // kLoadInsertF64.
};

static constexpr OpcodeVMInfo opcodeInfo2MV[size_t(OpcodeMV::kMaxValue) + 1] = {
  DEFINE_OP(NarrowingOp::kNone      ,  1, k8 ), // kStore8.
  DEFINE_OP(NarrowingOp::kNone      ,  2, k16), // kStore16_U16.
  DEFINE_OP(NarrowingOp::kNone      ,  4, k32), // kStore32_U32.
  DEFINE_OP(NarrowingOp::kNone      ,  4, k32), // kStore32_F32.
  DEFINE_OP(NarrowingOp::kNone      ,  8, k32), // kStore64_U32.
  DEFINE_OP(NarrowingOp::kNone      ,  8, k64), // kStore64_U64.
  DEFINE_OP(NarrowingOp::kNone      ,  8, k32), // kStore64_F32.
  DEFINE_OP(NarrowingOp::kNone      ,  8, k64), // kStore64_F64.
  DEFINE_OP(NarrowingOp::kNone      , 16, k32), // kStore128_U32.
  DEFINE_OP(NarrowingOp::kNone      , 16, k64), // kStore128_U64.
  DEFINE_OP(NarrowingOp::kNone      , 16, k32), // kStore128_F32.
  DEFINE_OP(NarrowingOp::kNone      , 16, k64), // kStore128_F64.
  DEFINE_OP(NarrowingOp::kNone      , 32, k32), // kStore256_U32.
  DEFINE_OP(NarrowingOp::kNone      , 32, k64), // kStore256_U64.
  DEFINE_OP(NarrowingOp::kNone      , 32, k32), // kStore256_F32.
  DEFINE_OP(NarrowingOp::kNone      , 32, k64), // kStore256_F64.
  DEFINE_OP(NarrowingOp::kNone      , 64, k32), // kStore512_U32.
  DEFINE_OP(NarrowingOp::kNone      , 64, k64), // kStore512_U64.
  DEFINE_OP(NarrowingOp::kNone      , 64, k32), // kStore512_F32.
  DEFINE_OP(NarrowingOp::kNone      , 64, k64), // kStore512_F64.
  DEFINE_OP(NarrowingOp::kNone      ,  0, k32), // kStoreN_U32.
  DEFINE_OP(NarrowingOp::kNone      ,  0, k64), // kStoreN_U64.
  DEFINE_OP(NarrowingOp::kNone      ,  0, k32), // kStoreN_F32.
  DEFINE_OP(NarrowingOp::kNone      ,  0, k64)  // kStoreN_F64.
  /*
  DEFINE_OP(NarrowingOp::kU16ToU8   ,  8, kNA), // kStoreCvtz64_U16ToU8.
  DEFINE_OP(NarrowingOp::kU32ToU16  ,  8, kNA), // kStoreCvtz64_U32ToU16.
  DEFINE_OP(NarrowingOp::kU64ToU32  ,  8, kNA), // kStoreCvtz64_U64ToU32.
  DEFINE_OP(NarrowingOp::kI16ToI8   ,  8, kNA), // kStoreCvts64_I16ToI8.
  DEFINE_OP(NarrowingOp::kI16ToU8   ,  8, kNA), // kStoreCvts64_I16ToU8.
  DEFINE_OP(NarrowingOp::kU16ToU8   ,  8, kNA), // kStoreCvts64_U16ToU8.
  DEFINE_OP(NarrowingOp::kI32ToI16  ,  8, kNA), // kStoreCvts64_I32ToI16.
  DEFINE_OP(NarrowingOp::kU32ToU16  ,  8, kNA), // kStoreCvts64_U32ToU16.
  DEFINE_OP(NarrowingOp::kI64ToI32  ,  8, kNA), // kStoreCvts64_I64ToI32.
  DEFINE_OP(NarrowingOp::kU64ToU32  ,  8, kNA), // kStoreCvts64_U64ToU32.
  DEFINE_OP(NarrowingOp::kU16ToU8   , 16, kNA), // kStoreCvtz128_U16ToU8.
  DEFINE_OP(NarrowingOp::kU32ToU16  , 16, kNA), // kStoreCvtz128_U32ToU16.
  DEFINE_OP(NarrowingOp::kU64ToU32  , 16, kNA), // kStoreCvtz128_U64ToU32.
  DEFINE_OP(NarrowingOp::kI16ToI8   , 16, kNA), // kStoreCvts128_I16ToI8.
  DEFINE_OP(NarrowingOp::kI16ToU8   , 16, kNA), // kStoreCvts128_I16ToU8.
  DEFINE_OP(NarrowingOp::kU16ToU8   , 16, kNA), // kStoreCvts128_U16ToU8.
  DEFINE_OP(NarrowingOp::kI32ToI16  , 16, kNA), // kStoreCvts128_I32ToI16.
  DEFINE_OP(NarrowingOp::kU32ToU16  , 16, kNA), // kStoreCvts128_U32ToU16.
  DEFINE_OP(NarrowingOp::kI64ToI32  , 16, kNA), // kStoreCvts128_I64ToI32.
  DEFINE_OP(NarrowingOp::kU64ToU32  , 16, kNA), // kStoreCvts128_U64ToU32.
  DEFINE_OP(NarrowingOp::kU16ToU8   , 32, kNA), // kStoreCvtz256_U16ToU8.
  DEFINE_OP(NarrowingOp::kU32ToU16  , 32, kNA), // kStoreCvtz256_U32ToU16.
  DEFINE_OP(NarrowingOp::kU64ToU32  , 32, kNA), // kStoreCvtz256_U64ToU32.
  DEFINE_OP(NarrowingOp::kI16ToI8   , 32, kNA), // kStoreCvts256_I16ToI8.
  DEFINE_OP(NarrowingOp::kI16ToU8   , 32, kNA), // kStoreCvts256_I16ToU8.
  DEFINE_OP(NarrowingOp::kU16ToU8   , 32, kNA), // kStoreCvts256_U16ToU8.
  DEFINE_OP(NarrowingOp::kI32ToI16  , 32, kNA), // kStoreCvts256_I32ToI16.
  DEFINE_OP(NarrowingOp::kU32ToU16  , 32, kNA), // kStoreCvts256_U32ToU16.
  DEFINE_OP(NarrowingOp::kI64ToI32  , 32, kNA), // kStoreCvts256_I64ToI32.
  DEFINE_OP(NarrowingOp::kU64ToU32  , 32, kNA), // kStoreCvts256_U64ToU32.
  DEFINE_OP(NarrowingOp::kU16ToU8   ,  0, kNA), // kStoreCvtzN_U16ToU8.
  DEFINE_OP(NarrowingOp::kU32ToU16  ,  0, kNA), // kStoreCvtzN_U32ToU16.
  DEFINE_OP(NarrowingOp::kU64ToU32  ,  0, kNA), // kStoreCvtzN_U64ToU32.
  DEFINE_OP(NarrowingOp::kI16ToI8   ,  0, kNA), // kStoreCvtsN_I16ToI8.
  DEFINE_OP(NarrowingOp::kI16ToU8   ,  0, kNA), // kStoreCvtsN_I16ToU8.
  DEFINE_OP(NarrowingOp::kU16ToU8   ,  0, kNA), // kStoreCvtsN_U16ToU8.
  DEFINE_OP(NarrowingOp::kI32ToI16  ,  0, kNA), // kStoreCvtsN_I32ToI16.
  DEFINE_OP(NarrowingOp::kU32ToU16  ,  0, kNA), // kStoreCvtsN_U32ToU16.
  DEFINE_OP(NarrowingOp::kI64ToI32  ,  0, kNA), // kStoreCvtsN_I64ToI32.
  DEFINE_OP(NarrowingOp::kU64ToU32  ,  0, kNA)  // kStoreCvtsN_U64ToU32.
  */
};

#undef DEFINE_OP

// bl::Pipeline::PipeCompiler - Vector Instructions - Utility Functions
// ====================================================================

static constexpr uint32_t float_mode_mem_size_table[5] = { 0, 4, 8, 0, 0 };

static BL_INLINE bool isSameVec(const Vec& a, const Operand_& b) noexcept {
  return b.isVec() && a.id() == b.id();
}

static BL_INLINE void vec_set_vec_type(Vec& vec, ElementSize sz) noexcept {
  static constexpr uint32_t signatures[5] = {
    a64::VecB::kSignature,
    a64::VecH::kSignature,
    a64::VecS::kSignature,
    a64::VecD::kSignature,
    a64::VecV::kSignature
  };
  vec.setSignature(OperandSignature{signatures[size_t(sz)]});
}

static BL_INLINE void vec_set_type(Vec& vec, ElementSize sz) noexcept {
  vec.setElementType(a64::VecElementType(uint32_t(sz) + 1));
}

static BL_INLINE void vec_set_type_and_index(Vec& vec, ElementSize sz, uint32_t idx) noexcept {
  vec.setElementType(a64::VecElementType(uint32_t(sz) + 1));
  vec.setElementIndex(idx);
}

static BL_NOINLINE void vec_load_mem(PipeCompiler* pc, const Vec& dst, Mem src, uint32_t memSize) noexcept {
  AsmCompiler* cc = pc->cc;

  if (src.hasIndex() && src.hasShift()) {
    // AArch64 limitation: index shift can be the same size as the size of the read operation, so H << 1, S << 2,
    // etc... Other shift values are not supported at the architectural level, so we have to precalculate the address.
    uint32_t shift = src.shift();
    if (memSize != (1u << shift) || src.hasOffset()) {
      Gp base = src.baseReg().as<Gp>();
      Gp index = src.indexReg().as<Gp>();

      if (src.isPreIndex()) {
        cc->add(base, base, index, a64::Shift(src.shiftOp(), shift));
        src = a64::ptr(base, src.offsetLo32());
      }
      else {
        Gp tmp = pc->newGpPtr("@mem_addr");
        cc->add(tmp, base, index, a64::Shift(src.shiftOp(), shift));
        src = a64::ptr(tmp, src.offsetLo32());
      }
    }
  }

  switch (memSize) {
    case  1: cc->ldr(dst.b(), src); break;
    case  2: cc->ldr(dst.h(), src); break;
    case  4: cc->ldr(dst.s(), src); break;
    case  8: cc->ldr(dst.d(), src); break;
    case 16: cc->ldr(dst.q(), src); break;
    default:
      BL_NOT_REACHED();
  }
}

static BL_NOINLINE Vec vec_from_mem(PipeCompiler* pc, const Mem& op, const Vec& ref, uint32_t memSize = 0) noexcept {
  Vec vec = pc->newV128("@tmp");
  if (memSize == 0)
    memSize = ref.size();
  vec_load_mem(pc, vec, op, memSize);
  return vec.cloneAs(ref);
}

static BL_INLINE Vec as_vec(PipeCompiler* pc, const Operand_& op, const Vec& ref, uint32_t memSize = 0) noexcept {
  if (op.isVec())
    return op.as<Vec>().cloneAs(ref);
  else
    return vec_from_mem(pc, op.as<Mem>(), ref, memSize);
}

static BL_INLINE Vec as_vec(PipeCompiler* pc, const Operand_& op, const Vec& ref, FloatMode fm) noexcept {
  if (op.isVec())
    return op.as<Vec>().cloneAs(ref);
  else
    return vec_from_mem(pc, op.as<Mem>(), ref, float_mode_mem_size_table[size_t(fm)]);
}

static BL_NOINLINE Vec vec_mov(PipeCompiler* pc, const Vec& dst_, const Operand_& src_) noexcept {
  AsmCompiler* cc = pc->cc;

  Vec dst(dst_);
  vec_set_type(dst, ElementSize::k8);

  if (src_.isVec()) {
    if (dst.id() != src_.id()) {
      Vec src = src_.as<Vec>();
      vec_set_type(src, ElementSize::k8);
      cc->mov(dst, src.as<Vec>());
    }
    return dst;
  }

  if (src_.isMem()) {
    vec_load_mem(pc, dst, src_.as<Mem>(), dst.size());
    return dst;
  }

  BL_NOT_REACHED();
}

static BL_NOINLINE void vec_neg(PipeCompiler* pc, const Vec& dst, const Vec& src, FloatMode fm) noexcept {
  AsmCompiler* cc = pc->cc;

  if (fm == FloatMode::kF32S)
    cc->mvn(dst.s(), src.s());
  else if (fm == FloatMode::kF64S)
    cc->mvn(dst.d(), src.d());
  else
    cc->mvn(dst.q(), src.q());
}

// bl::Pipeline::PipeCompiler - Vector Instructions - Swizzle 32 Impl
// ==================================================================

// [DCBA] <- Mov    (DCBA, dcba)
// [AAAA] <- Dup0   (DCBA, dcba)
// [BBBB] <- Dup1   (DCBA, dcba)
// [CCCC] <- Dup2   (DCBA, dcba)
// [DDDD] <- Dup3   (DCBA, dcba)
// [CDAB] <- Rev64  (DCBA, dcba)
// [aDCB] <- Ext4   (DCBA, dcba) [dcb|aDCB|A  ]
// [baDC] <- Ext8   (DCBA, dcba) [ dc|baDC|BA ]
// [cbaD] <- Ext12  (DCBA, dcba) [  d|cbaD|CBA]
// [bBaA] <- Zip1_4S(DCBA, dcba)
// [baBA] <- Zip1_2D(DCBA, dcba)
// [dDcC] <- Zip2_4S(DCBA, dcba)
// [dcDC] <- Zip2_2D(DCBA, dcba)
// [caCA] <- Uzp1_4S(DCBA, dcba)
// [baBA] <- Uzp1_2D(DCBA, dcba)
// [dbDB] <- Uzp2_4S(DCBA, dcba)
// [dcDC] <- Uzp2_2D(DCBA, dcba)
// [cCaA] <- Trn1_4S(DCBA, dcba)
// [dDbB] <- Trn2_4S(DCBA, dcba)
struct Swizzle32Data {
  enum class OpTarget : uint8_t {
    kDst = 0,
    k1   = 1,
    k2   = 2,
    kA   = 3,

    k_   = kDst
  };

  struct Op {
    //! Swizzle operation does nothing - 'mov' (this can be only the first operation).
    static constexpr uint8_t kMov = 1;
    //! Swizzle operation performs an insert - moves a value from one lane to another.
    static constexpr uint8_t kIns = 2;
    //! Swizzle operation duplicates a lane across all others - 'dup'.
    static constexpr uint8_t kDup = 3;
    //! Swizzle operation rotates a vector - 'ext'.
    static constexpr uint8_t kExt = 4;
    //! Swizzle operation swaps lo/hi elements of 64-bit lanes - 'rev64'.
    static constexpr uint8_t kRev64 = 5;
    //! Swizzle operation can be implemented as a single zip[1|2], uzp[1|2], or trn[1|2] instruction with 32-bit or 64-bit elements.
    static constexpr uint8_t kZipUnzip = 7;

    static constexpr uint8_t kIns0To1  = uint8_t(kIns     ) | (0 << 4) | (1 << 6);
    static constexpr uint8_t kIns0To2  = uint8_t(kIns     ) | (0 << 4) | (2 << 6);
    static constexpr uint8_t kIns0To3  = uint8_t(kIns     ) | (0 << 4) | (3 << 6);
    static constexpr uint8_t kIns1To0  = uint8_t(kIns     ) | (1 << 4) | (0 << 6);
    static constexpr uint8_t kIns1To2  = uint8_t(kIns     ) | (1 << 4) | (2 << 6);
    static constexpr uint8_t kIns1To3  = uint8_t(kIns     ) | (1 << 4) | (3 << 6);
    static constexpr uint8_t kIns2To0  = uint8_t(kIns     ) | (2 << 4) | (0 << 6);
    static constexpr uint8_t kIns2To1  = uint8_t(kIns     ) | (2 << 4) | (1 << 6);
    static constexpr uint8_t kIns2To3  = uint8_t(kIns     ) | (2 << 4) | (3 << 6);
    static constexpr uint8_t kIns3To0  = uint8_t(kIns     ) | (3 << 4) | (0 << 6);
    static constexpr uint8_t kIns3To1  = uint8_t(kIns     ) | (3 << 4) | (1 << 6);
    static constexpr uint8_t kIns3To2  = uint8_t(kIns     ) | (3 << 4) | (2 << 6);
    static constexpr uint8_t kDup0     = uint8_t(kDup     ) | (0 << 4);
    static constexpr uint8_t kDup1     = uint8_t(kDup     ) | (1 << 4);
    static constexpr uint8_t kDup2     = uint8_t(kDup     ) | (2 << 4);
    static constexpr uint8_t kDup3     = uint8_t(kDup     ) | (3 << 4);
    static constexpr uint8_t kExt4     = uint8_t(kExt     ) | (1 << 4);
    static constexpr uint8_t kExt8     = uint8_t(kExt     ) | (2 << 4);
    static constexpr uint8_t kExt12    = uint8_t(kExt     ) | (3 << 4);
    static constexpr uint8_t kZip1_4S  = uint8_t(kZipUnzip) | (0 << 4) | (0 << 7);
    static constexpr uint8_t kZip1_2D  = uint8_t(kZipUnzip) | (0 << 4) | (1 << 7);
    static constexpr uint8_t kZip2_4S  = uint8_t(kZipUnzip) | (1 << 4) | (0 << 7);
    static constexpr uint8_t kZip2_2D  = uint8_t(kZipUnzip) | (1 << 4) | (1 << 7);
    static constexpr uint8_t kUzp1_4S  = uint8_t(kZipUnzip) | (2 << 4) | (0 << 7);
    static constexpr uint8_t kUzp1_2D  = uint8_t(kZipUnzip) | (2 << 4) | (1 << 7);
    static constexpr uint8_t kUzp2_4S  = uint8_t(kZipUnzip) | (3 << 4) | (0 << 7);
    static constexpr uint8_t kUzp2_2D  = uint8_t(kZipUnzip) | (3 << 4) | (1 << 7);
    static constexpr uint8_t kTrn1_4S  = uint8_t(kZipUnzip) | (4 << 4);
    static constexpr uint8_t kTrn2_4S  = uint8_t(kZipUnzip) | (5 << 4);

    // Alias to nothing to make the table easier to read.
    static constexpr uint8_t k_ = 0;

    uint8_t data;

    BL_INLINE_NODEBUG bool isValid() const noexcept { return data != 0; }
    BL_INLINE_NODEBUG uint32_t type() const noexcept { return data & 0xF; }

    BL_INLINE_NODEBUG uint32_t dupIdx() const noexcept { return (data >> 4) & 0x3; }
    BL_INLINE_NODEBUG uint32_t extImm() const noexcept { return (data >> 2) & (0x3 << 2); }

    BL_INLINE_NODEBUG uint32_t zipOp() const noexcept { return (data >> 4) & 0x7; }
    BL_INLINE_NODEBUG bool zipS4() const noexcept { return (data & (1 << 7)) == 0; }

    BL_INLINE_NODEBUG uint32_t insSrc() const noexcept { return (data >> 4) & 0x3; }
    BL_INLINE_NODEBUG uint32_t insDst() const noexcept { return (data >> 6) & 0x3; }
  };

  //! \name Members
  //! \{

  Op ops[3];
  uint8_t flags;

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG bool isDefined() const noexcept { return ops[0].data != 0; }
  BL_INLINE_NODEBUG Op op(uint32_t index) const noexcept { return ops[index]; }
  BL_INLINE_NODEBUG OpTarget opTarget(uint32_t index) const noexcept { return OpTarget((flags >> (index * 2)) & 0x3); }

  //! \}
};

// This table provides all combinations for all possible 32-bit swizzles (there is 256 combinations in total).
// It prioritizes lane moves, and then operations that can have either one or two inputs. Each operation has
// a target, which specifies whether it replaces the destination or one or both sources that are then passed
// to a next operation. The last operation must always be `OpTarget::kDst` so the result ends up in the right
// register.
//
// In general the decomposition of operations needed for all swizzles is as follows:
//
//   - 1 Op Swizzles: 17
//   - 2 Op Swizzles: 156
//   - 3 Op Swizzles: 83
//
// Which means that luckily most used swizzles would fall into 1 or 2 operations.
//
// NOTE: Moves (InsXToY) operations only happen on the destination as they are destructive, which is perfectly
// okay as moving them into earlier steps didn't really improve anything.
#define OP(swiz, op0, target0, op1, target1, op2, target2) { \
  {                                                            \
    {Swizzle32Data::Op::k##op0},                               \
    {Swizzle32Data::Op::k##op1},                               \
    {Swizzle32Data::Op::k##op2}                                \
  },                                                           \
  (                                                            \
    (uint16_t(Swizzle32Data::OpTarget::k##target0) << 0) |     \
    (uint16_t(Swizzle32Data::OpTarget::k##target1) << 2) |     \
    (uint16_t(Swizzle32Data::OpTarget::k##target2) << 4)       \
  )                                                            \
}

static constexpr Swizzle32Data swizzle_32_data[256] = {
  OP(0000, Dup0   , _, _      , _, _      , _), OP(0001, Rev64  , _, Ins1To2, _, Ins1To3, _), OP(0002, Ext8   , _, Ins2To1, _, Ins2To3, _), OP(0003, Dup0   , 2, Ext12  , _, _      , _),
  OP(0010, Zip1_2D, _, Ins0To3, _, _      , _), OP(0011, Rev64  , A, Zip1_4S, _, _      , _), OP(0012, Ext8   , _, Ins3To1, _, Ins2To3, _), OP(0013, Rev64  , 2, Ext12  , _, Ins2To3, _),
  OP(0020, Uzp1_4S, _, Ins0To3, _, _      , _), OP(0021, Ext4   , _, Ins3To2, _, _      , _), OP(0022, Ext4   , A, Trn2_4S, _, _      , _), OP(0023, Ext4   , _, Ins2To0, _, Ins3To2, _),
  OP(0030, Ext8   , _, Ins2To0, _, Ins2To3, _), OP(0031, Dup0   , 2, Uzp2_4S, _, _      , _), OP(0032, Ext8   , _, Ins2To3, _, _      , _), OP(0033, Ext4   , A, Zip2_4S, _, _      , _),
  OP(0100, Zip1_4S, _, Ins0To3, _, _      , _), OP(0101, Rev64  , A, Zip1_2D, _, _      , _), OP(0102, Ext12  , _, Ins3To0, _, Ins1To3, _), OP(0103, Ext12  , _, Ins1To3, _, _      , _),
  OP(0110, Rev64  , 2, Zip1_4S, _, _      , _), OP(0111, Dup1   , 1, Ext4   , _, _      , _), OP(0112, Rev64  , 2, Ext8   , _, Ins2To1, _), OP(0113, Ext12  , _, Ins1To3, _, Ins2To1, _),
  OP(0120, Ext4   , _, Ins0To2, _, Ins3To0, _), OP(0121, Ext4   , _, Ins0To2, _, _      , _), OP(0122, Ext4   , _, Ins0To2, _, Ins1To0, _), OP(0123, Rev64  , A, Ext8   , _, _      , _),
  OP(0130, Ext12  , 2, Zip1_4S, _, _      , _), OP(0131, Ext4   , _, Ins2To1, _, Ins0To2, _), OP(0132, Rev64  , 2, Ext8   , _, _      , _), OP(0133, Ext12  , _, Ins1To3, _, Ins0To1, _),
  OP(0200, Trn1_4S, _, Ins0To3, _, _      , _), OP(0201, Rev64  , _, Ins3To2, _, Ins1To3, _), OP(0202, Ext4   , A, Uzp2_4S, _, _      , _), OP(0203, Uzp1_4S, 2, Ext12  , _, _      , _),
  OP(0210, Mov    , _, Ins0To3, _, _      , _), OP(0211, Mov    , _, Ins0To3, _, Ins1To0, _), OP(0212, Mov    , _, Ins0To3, _, Ins2To0, _), OP(0213, Rev64  , A, Ext8   , 1, Zip1_4S, _),
  OP(0220, Ext8   , 2, Uzp1_4S, _, _      , _), OP(0221, Ext4   , _, Ins1To2, _, _      , _), OP(0222, Dup2   , 1, Ext4   , _, _      , _), OP(0223, Ext4   , _, Ins2To0, _, Ins1To2, _),
  OP(0230, Rev64  , 1, Ext4   , _, _      , _), OP(0231, Ext4   , 2, Uzp2_4S, _, _      , _), OP(0232, Ext8   , _, Ins2To3, _, Ins0To2, _), OP(0233, Rev64  , 1, Ext4   , _, Ins1To0, _),
  OP(0300, Rev64  , _, Ins1To0, _, Ins1To3, _), OP(0301, Rev64  , _, Ins1To3, _, _      , _), OP(0302, Dup0   , 2, Zip2_4S, _, _      , _), OP(0303, Ext4   , A, Zip2_2D, _, _      , _),
  OP(0310, Ext12  , 2, Zip1_2D, _, _      , _), OP(0311, Ext4   , _, Ins0To1, _, _      , _), OP(0312, Dup0   , 2, Zip1_4S, 2, Zip2_4S, _), OP(0313, Uzp2_4S, 1, Ext4   , _, _      , _),
  OP(0320, Ext4   , _, Ins3To0, _, _      , _), OP(0321, Ext4   , _, _      , _, _      , _), OP(0322, Ext4   , _, Ins1To0, _, _      , _), OP(0323, Ext4   , _, Ins2To0, _, _      , _),
  OP(0330, Ext4   , _, Ins2To1, _, Ins3To0, _), OP(0331, Ext4   , _, Ins2To1, _, _      , _), OP(0332, Ext4   , 2, Zip2_4S, _, _      , _), OP(0333, Dup3   , 1, Ext4   , _, _      , _),
  OP(1000, Zip1_4S, _, Ins0To2, _, _      , _), OP(1001, Rev64  , 1, Zip1_4S, _, _      , _), OP(1002, Ext8   , _, Ins2To1, _, _      , _), OP(1003, Ext4   , 1, Ext8   , _, _      , _),
  OP(1010, Zip1_2D, _, _      , _, _      , _), OP(1011, Zip1_2D, _, Ins1To0, _, _      , _), OP(1012, Ext8   , _, Ins3To1, _, _      , _), OP(1013, Ext8   , _, Ins1To0, _, Ins3To1, _),
  OP(1020, Zip1_4S, 2, Uzp1_4S, _, _      , _), OP(1021, Ext4   , 1, Zip1_2D, _, _      , _), OP(1022, Ext8   , _, Ins0To1, _, _      , _), OP(1023, Rev64  , 1, Ext8   , _, _      , _),
  OP(1030, Ext8   , _, Ins2To0, _, _      , _), OP(1031, Ext8   , _, Ins3To0, _, _      , _), OP(1032, Ext8   , _, _      , _, _      , _), OP(1033, Ext8   , _, Ins1To0, _, _      , _),
  OP(1100, Zip1_4S, _, _      , _, _      , _), OP(1101, Zip1_4S, _, Ins2To0, _, _      , _), OP(1102, Ext8   , _, Ins2To1, _, Ins3To2, _), OP(1103, Ext12  , _, Ins2To3, _, _      , _),
  OP(1110, Zip1_4S, _, Ins2To1, _, _      , _), OP(1111, Dup1   , _, _      , _, _      , _), OP(1112, Ext8   , _, Ins3To1, _, Ins3To2, _), OP(1113, Dup1   , 2, Ext12  , _, _      , _),
  OP(1120, Dup1   , 2, Uzp1_4S, _, _      , _), OP(1121, Ext4   , _, Ins0To2, _, Ins0To3, _), OP(1122, Ext8   , _, Ins0To1, _, Ins3To2, _), OP(1123, Ext12  , _, Ins3To1, _, Ins2To3, _),
  OP(1130, Ext8   , _, Ins2To0, _, Ins3To2, _), OP(1131, Uzp2_4S, _, Ins0To3, _, _      , _), OP(1132, Ext8   , _, Ins3To2, _, _      , _), OP(1133, Ext8   , A, Trn2_4S, _, _      , _),
  OP(1200, Zip1_4S, 2, Trn1_4S, _, _      , _), OP(1201, Ext4   , 1, Zip1_4S, _, _      , _), OP(1202, Dup2   , 1, Zip1_4S, _, _      , _), OP(1203, Dup1   , 2, Uzp1_4S, 2, Ext12  , _),
  OP(1210, Mov    , _, Ins1To3, _, _      , _), OP(1211, Mov    , _, Ins1To0, _, Ins1To3, _), OP(1212, Mov    , _, Ins1To3, _, Ins2To0, _), OP(1213, Mov    , _, Ins3To0, _, Ins1To3, _),
  OP(1220, Mov    , _, Ins1To3, _, Ins2To1, _), OP(1221, Ext4   , _, Ins0To3, _, Ins1To2, _), OP(1222, Ext8   , _, Ins0To1, _, Ins0To2, _), OP(1223, Rev64  , 1, Ext8   , _, Ins1To2, _),
  OP(1230, Rev64  , A, Ext4   , _, _      , _), OP(1231, Ext8   , _, Ins0To2, _, Ins3To0, _), OP(1232, Ext8   , _, Ins0To2, _, _      , _), OP(1233, Ext8   , _, Ins0To2, _, Ins1To0, _),
  OP(1300, Rev64  , _, Ins0To3, _, Ins1To0, _), OP(1301, Rev64  , _, Ins0To3, _, _      , _), OP(1302, Ext8   , 1, Zip1_4S, _, _      , _), OP(1303, Dup3   , 1, Zip1_4S, _, _      , _),
  OP(1310, Mov    , _, Ins3To2, _, Ins1To3, _), OP(1311, Trn2_4S, _, Ins0To3, _, _      , _), OP(1312, Dup1   , 2, Zip2_4S, _, _      , _), OP(1313, Ext8   , A, Uzp2_4S, _, _      , _),
  OP(1320, Ext12  , 2, Uzp1_4S, _, _      , _), OP(1321, Ext4   , _, Ins0To3, _, _      , _), OP(1322, Ext4   , _, Ins0To3, _, Ins1To0, _), OP(1323, Ext4   , _, Ins0To3, _, Ins2To0, _),
  OP(1330, Ext8   , _, Ins2To0, _, Ins1To2, _), OP(1331, Ext8   , 2, Uzp2_4S, _, _      , _), OP(1332, Ext8   , _, Ins1To2, _, _      , _), OP(1333, Ext8   , _, Ins1To0, _, Ins1To2, _),
  OP(2000, Uzp1_4S, _, Ins0To1, _, _      , _), OP(2001, Rev64  , _, Ins1To2, _, _      , _), OP(2002, Ext8   , 1, Uzp1_4S, _, _      , _), OP(2003, Ext12  , _, Ins1To2, _, _      , _),
  OP(2010, Zip1_4S, 1, Uzp1_4S, _, _      , _), OP(2011, Dup1   , 1, Uzp1_4S, _, _      , _), OP(2012, Ext8   , _, Ins3To1, _, Ins0To3, _), OP(2013, Ext12  , 1, Uzp1_4S, _, _      , _),
  OP(2020, Uzp1_4S, _, _      , _, _      , _), OP(2021, Rev64  , _, Ins1To2, _, Ins3To1, _), OP(2022, Uzp1_4S, _, Ins1To0, _, _      , _), OP(2023, Ext12  , _, Ins1To2, _, Ins3To1, _),
  OP(2030, Ext8   , _, Ins0To3, _, Ins2To0, _), OP(2031, Rev64  , 1, Uzp1_4S, _, _      , _), OP(2032, Ext8   , _, Ins0To3, _, _      , _), OP(2033, Dup3   , 1, Uzp1_4S, _, _      , _),
  OP(2100, Ext12  , _, Ins1To0, _, _      , _), OP(2101, Rev64  , _, Ins0To2, _, _      , _), OP(2102, Ext12  , _, Ins3To0, _, _      , _), OP(2103, Ext12  , _, _      , _, _      , _),
  OP(2110, Ext4   , 2, Zip1_4S, _, _      , _), OP(2111, Rev64  , _, Ins0To1, _, Ins0To2, _), OP(2112, Ext12  , _, Ins2To1, _, Ins3To0, _), OP(2113, Ext12  , _, Ins2To1, _, _      , _),
  OP(2120, Dup2   , 2, Zip1_4S, _, _      , _), OP(2121, Ext4   , A, Zip1_2D, _, _      , _), OP(2122, Ext12  , _, Ins3To0, _, Ins3To1, _), OP(2123, Ext12  , _, Ins3To1, _, _      , _),
  OP(2130, Dup2   , 2, Ext12  , 2, Zip1_4S, _), OP(2131, Rev64  , _, Ins2To1, _, Ins0To2, _), OP(2132, Ext4   , 2, Ext8   , _, _      , _), OP(2133, Ext12  , _, Ins0To1, _, _      , _),
  OP(2200, Trn1_4S, _, _      , _, _      , _), OP(2201, Rev64  , _, Ins3To2, _, _      , _), OP(2202, Trn1_4S, _, Ins2To0, _, _      , _), OP(2203, Ext12  , _, Ins3To2, _, _      , _),
  OP(2210, Mov    , _, Ins2To3, _, _      , _), OP(2211, Ext4   , A, Zip1_4S, _, _      , _), OP(2212, Mov    , _, Ins2To0, _, Ins2To3, _), OP(2213, Mov    , _, Ins3To0, _, Ins2To3, _),
  OP(2220, Uzp1_4S, _, Ins1To2, _, _      , _), OP(2221, Rev64  , _, Ins3To1, _, Ins3To2, _), OP(2222, Dup2   , _, _      , _, _      , _), OP(2223, Dup2   , 2, Ext12  , _, _      , _),
  OP(2230, Mov    , _, Ins3To1, _, Ins2To3, _), OP(2231, Dup2   , 2, Uzp2_4S, _, _      , _), OP(2232, Zip2_2D, _, Ins0To3, _, _      , _), OP(2233, Rev64  , A, Zip2_4S, _, _      , _),
  OP(2300, Rev64  , _, Ins1To0, _, _      , _), OP(2301, Rev64  , _, _      , _, _      , _), OP(2302, Rev64  , _, Ins3To0, _, _      , _), OP(2303, Rev64  , _, Ins2To0, _, _      , _),
  OP(2310, Dup2   , 2, Ext12  , 2, Zip1_2D, _), OP(2311, Rev64  , _, Ins0To1, _, _      , _), OP(2312, Ext12  , 2, Zip2_4S, _, _      , _), OP(2313, Rev64  , _, Ins0To1, _, Ins2To0, _),
  OP(2320, Rev64  , _, Ins1To0, _, Ins3To1, _), OP(2321, Rev64  , _, Ins3To1, _, _      , _), OP(2322, Zip2_4S, _, Ins0To3, _, _      , _), OP(2323, Rev64  , A, Zip2_2D, _, _      , _),
  OP(2330, Rev64  , _, Ins1To0, _, Ins2To1, _), OP(2331, Rev64  , _, Ins2To1, _, _      , _), OP(2332, Rev64  , 2, Zip2_4S, _, _      , _), OP(2333, Rev64  , _, Ins2To0, _, Ins2To1, _),
  OP(3000, Mov    , _, Ins0To1, _, Ins0To2, _), OP(3001, Rev64  , _, Ins2To3, _, Ins1To2, _), OP(3002, Ext8   , _, Ins1To3, _, Ins2To1, _), OP(3003, Ext12  , _, Ins0To3, _, Ins1To2, _),
  OP(3010, Mov    , _, Ins0To2, _, _      , _), OP(3011, Mov    , _, Ins0To2, _, Ins1To0, _), OP(3012, Rev64  , A, Ext12  , _, _      , _), OP(3013, Rev64  , 2, Ext12  , _, _      , _),
  OP(3020, Dup0   , 1, Zip2_4S, _, _      , _), OP(3021, Dup1   , 1, Ext4   , 1, Zip2_4S, _), OP(3022, Ext8   , _, Ins1To3, _, Ins0To1, _), OP(3023, Ext4   , 1, Zip2_4S, _, _      , _),
  OP(3030, Mov    , _, Ins0To2, _, Ins3To1, _), OP(3031, Ext8   , _, Ins3To0, _, Ins1To3, _), OP(3032, Ext8   , _, Ins1To3, _, _      , _), OP(3033, Ext8   , _, Ins1To0, _, Ins1To3, _),
  OP(3100, Dup0   , 1, Uzp2_4S, _, _      , _), OP(3101, Rev64  , _, Ins2To3, _, Ins0To2, _), OP(3102, Ext4   , 1, Uzp2_4S, _, _      , _), OP(3103, Ext12  , _, Ins0To3, _, _      , _),
  OP(3110, Mov    , _, Ins1To2, _, _      , _), OP(3111, Uzp2_4S, _, Ins0To1, _, _      , _), OP(3112, Mov    , _, Ins2To0, _, Ins1To2, _), OP(3113, Ext8   , 1, Uzp2_4S, _, _      , _),
  OP(3120, Rev64  , 2, Uzp1_4S, _, _      , _), OP(3121, Dup1   , 1, Zip2_4S, _, _      , _), OP(3122, Dup2   , 1, Uzp2_4S, _, _      , _), OP(3123, Ext12  , _, Ins3To1, _, Ins0To3, _),
  OP(3130, Dup3   , 2, Zip1_4S, _, _      , _), OP(3131, Uzp2_4S, _, _      , _, _      , _), OP(3132, Zip2_4S, 1, Uzp2_4S, _, _      , _), OP(3133, Uzp2_4S, _, Ins1To0, _, _      , _),
  OP(3200, Mov    , _, Ins0To1, _, _      , _), OP(3201, Dup1   , 1, Ext4   , 1, Zip2_2D, _), OP(3202, Mov    , _, Ins0To1, _, Ins2To0, _), OP(3203, Ext4   , 1, Zip2_2D, _, _      , _),
  OP(3210, Mov    , _, _      , _, _      , _), OP(3211, Mov    , _, Ins1To0, _, _      , _), OP(3212, Mov    , _, Ins2To0, _, _      , _), OP(3213, Mov    , _, Ins3To0, _, _      , _),
  OP(3220, Mov    , _, Ins2To1, _, _      , _), OP(3221, Ext12  , 1, Zip2_4S, _, _      , _), OP(3222, Zip2_4S, _, Ins0To2, _, _      , _), OP(3223, Rev64  , 1, Zip2_4S, _, _      , _),
  OP(3230, Mov    , _, Ins3To1, _, _      , _), OP(3231, Zip2_4S, 2, Uzp2_4S, _, _      , _), OP(3232, Zip2_2D, _, _      , _, _      , _), OP(3233, Zip2_2D, _, Ins1To0, _, _      , _),
  OP(3300, Mov    , _, Ins0To1, _, Ins3To2, _), OP(3301, Rev64  , _, Ins2To3, _, _      , _), OP(3302, Rev64  , _, Ins3To0, _, Ins2To3, _), OP(3303, Rev64  , _, Ins2To0, _, Ins2To3, _),
  OP(3310, Mov    , _, Ins3To2, _, _      , _), OP(3311, Trn2_4S, _, _      , _, _      , _), OP(3312, Zip2_4S, 1, Trn2_4S, _, _      , _), OP(3313, Trn2_4S, _, Ins2To0, _, _      , _),
  OP(3320, Dup3   , 2, Uzp1_4S, _, _      , _), OP(3321, Ext4   , _, Ins2To3, _, _      , _), OP(3322, Zip2_4S, _, _      , _, _      , _), OP(3323, Zip2_4S, _, Ins2To0, _, _      , _),
  OP(3330, Mov    , _, Ins3To1, _, Ins3To2, _), OP(3331, Uzp2_4S, _, Ins1To2, _, _      , _), OP(3332, Zip2_4S, _, Ins2To1, _, _      , _), OP(3333, Dup3   , _, _      , _, _      , _)
};

#undef OP

static void emit_swizzle32_impl(PipeCompiler* pc, const Vec& dst, const Vec& src, uint32_t imm) noexcept {
  BL_ASSERT((imm & 0xFCFCFCFC) == 0);

  AsmCompiler* cc = pc->cc;

  uint32_t tableIndex = ((imm & 0x03000000) >> (24 - 6)) |
                        ((imm & 0x00030000) >> (16 - 4)) |
                        ((imm & 0x00000300) >> (8  - 2)) | (imm & 0x00000003);
  Swizzle32Data swiz = swizzle_32_data[tableIndex];

  if (swiz.isDefined()) {
    Vec opSrc[2] = { src, src };
    Vec opDst;

    for (uint32_t i = 0; i < 3; i++) {
      Swizzle32Data::Op op = swiz.op(i);
      Swizzle32Data::OpTarget target = swiz.opTarget(i);

      if (!op.isValid())
        break;

      if (target == Swizzle32Data::OpTarget::kDst) {
        opDst = dst;
      }
      else {
        opDst = pc->newSimilarReg(dst, "@tmp");
      }

      switch (op.type()) {
        case Swizzle32Data::Op::kMov: {
          vec_mov(pc, opDst, opSrc[0]);
          break;
        }

        case Swizzle32Data::Op::kIns: {
          uint32_t srcLane = op.insSrc();
          uint32_t dstLane = op.insDst();
          // Insert is always the last operation that only uses the destination register.
          cc->mov(opDst.s(dstLane), opDst.s(srcLane));
          break;
        }

        case Swizzle32Data::Op::kDup: {
          // Use `dup` if the swizzle is actually a broadcast of a single element.
          uint32_t idx = op.dupIdx();
          cc->dup(opDst.s4(), opSrc[0].s(idx));
          break;
        }

        case Swizzle32Data::Op::kExt: {
          // Use `ext` if the swizzle is rotating the vector.
          uint32_t n = op.extImm();
          cc->ext(opDst.b16(), opSrc[0].b16(), opSrc[1].b16(), n);
          break;
        }

        case Swizzle32Data::Op::kRev64: {
          // Use `rev64` to swap lo/hi elements of 64-bit lanes.
          cc->rev64(opDst.s4(), opSrc[0].s4());
          break;
        }

        case Swizzle32Data::Op::kZipUnzip: {
          // Use `zip[1|2]`, 'uzp[1|2]', or 'trn[1|2]` if the swizzle can be implemented this way.
          static constexpr uint16_t zip_unzip_inst[8] = {
            Inst::kIdZip1_v,
            Inst::kIdZip2_v,
            Inst::kIdUzp1_v,
            Inst::kIdUzp2_v,
            Inst::kIdTrn1_v,
            Inst::kIdTrn2_v
          };

          InstId instId = zip_unzip_inst[op.zipOp()];
          if (op.zipS4())
            cc->emit(instId, opDst.s4(), opSrc[0].s4(), opSrc[1].s4());
          else
            cc->emit(instId, opDst.d2(), opSrc[0].d2(), opSrc[1].d2());
          break;
        }

        default:
          BL_NOT_REACHED();
      }

      if (uint32_t(target) & uint32_t(Swizzle32Data::OpTarget::k1)) opSrc[0] = opDst;
      if (uint32_t(target) & uint32_t(Swizzle32Data::OpTarget::k2)) opSrc[1] = opDst;
    }
  }
  else {
    // NOTE: This code is never used at the moment. It's kept if for some reason we would want to avoid using
    // more than 1 or 2 instructions to perform the swizzle. For example on hardware where TBL is faster than
    // other operations combined.
    uint8_t predData[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };

    uint32_t d = (imm >> 22) & (0x3 << 2);
    uint32_t c = (imm >> 14) & (0x3 << 2);
    uint32_t b = (imm >>  6) & (0x3 << 2);
    uint32_t a = (imm <<  2) & (0x3 << 2);

    predData[ 0] = a;
    predData[ 1] = a + 1;
    predData[ 2] = a + 2;
    predData[ 3] = a + 3;
    predData[ 4] = b;
    predData[ 5] = b + 1;
    predData[ 6] = b + 2;
    predData[ 7] = b + 3;
    predData[ 8] = c;
    predData[ 9] = c + 1;
    predData[10] = c + 2;
    predData[11] = c + 3;
    predData[12] = d;
    predData[13] = d + 1;
    predData[14] = d + 2;
    predData[15] = d + 3;

    Vec pred = pc->simdConst16B(predData);
    cc->tbl(dst.b16(), src.b16(), pred.b16());
  }
}

// bl::Pipeline::PipeCompiler - Vector Instructions - Interleaved Shuffle 32 Impl
// ==============================================================================

struct InterleavedShuffle32Ops {
  struct Op {
    //! Operation swaps lo/hi elements of 64-bit lanes - 'rev64'.
    static constexpr uint32_t kOpRev64 = 0;
    //! Operation performs a 32-bit insert - moves a value from a source lane to a destination lane.
    static constexpr uint32_t kOpInsS = 1;
    //! Operation performs a 64-bit insert - moves a value from a source lane to a destination lane.
    static constexpr uint32_t kOpInsD = 2;
    //! Operation can be implemented as a single zip[1|2], uzp[1|2], trn[1|2], or ext instruction with 32-bit or 64-bit elements.
    static constexpr uint32_t kOpPerm = 3;
    //! Operation duplicates a lane across all others - 'dup'.
    static constexpr uint32_t kOpDup = 4;

    static constexpr uint32_t kInsS_0To0 = (kOpInsS << 8) | (0 << 0) | (0 << 2);
    static constexpr uint32_t kInsS_0To1 = (kOpInsS << 8) | (0 << 0) | (1 << 2);
    static constexpr uint32_t kInsS_0To2 = (kOpInsS << 8) | (0 << 0) | (2 << 2);
    static constexpr uint32_t kInsS_0To3 = (kOpInsS << 8) | (0 << 0) | (3 << 2);
    static constexpr uint32_t kInsS_1To0 = (kOpInsS << 8) | (1 << 0) | (0 << 2);
    static constexpr uint32_t kInsS_1To1 = (kOpInsS << 8) | (1 << 0) | (1 << 2);
    static constexpr uint32_t kInsS_1To2 = (kOpInsS << 8) | (1 << 0) | (2 << 2);
    static constexpr uint32_t kInsS_1To3 = (kOpInsS << 8) | (1 << 0) | (3 << 2);
    static constexpr uint32_t kInsS_2To0 = (kOpInsS << 8) | (2 << 0) | (0 << 2);
    static constexpr uint32_t kInsS_2To1 = (kOpInsS << 8) | (2 << 0) | (1 << 2);
    static constexpr uint32_t kInsS_2To2 = (kOpInsS << 8) | (2 << 0) | (2 << 2);
    static constexpr uint32_t kInsS_2To3 = (kOpInsS << 8) | (2 << 0) | (3 << 2);
    static constexpr uint32_t kInsS_3To0 = (kOpInsS << 8) | (3 << 0) | (0 << 2);
    static constexpr uint32_t kInsS_3To1 = (kOpInsS << 8) | (3 << 0) | (1 << 2);
    static constexpr uint32_t kInsS_3To2 = (kOpInsS << 8) | (3 << 0) | (2 << 2);
    static constexpr uint32_t kInsS_3To3 = (kOpInsS << 8) | (3 << 0) | (3 << 2);
    static constexpr uint32_t kInsD_0To0 = (kOpInsD << 8) | (0 << 0) | (0 << 2);
    static constexpr uint32_t kInsD_0To1 = (kOpInsD << 8) | (0 << 0) | (1 << 2);
    static constexpr uint32_t kInsD_1To0 = (kOpInsD << 8) | (1 << 0) | (0 << 2);
    static constexpr uint32_t kInsD_1To1 = (kOpInsD << 8) | (1 << 0) | (1 << 2);

    static constexpr uint32_t kZip1_4S   = (kOpPerm << 8) | (0 << 0) | (0 << 3);
    static constexpr uint32_t kZip1_2D   = (kOpPerm << 8) | (0 << 0) | (1 << 3);
    static constexpr uint32_t kZip2_4S   = (kOpPerm << 8) | (1 << 0) | (0 << 3);
    static constexpr uint32_t kZip2_2D   = (kOpPerm << 8) | (1 << 0) | (1 << 3);
    static constexpr uint32_t kUzp1_4S   = (kOpPerm << 8) | (2 << 0) | (0 << 3);
    static constexpr uint32_t kUzp1_2D   = (kOpPerm << 8) | (2 << 0) | (1 << 3);
    static constexpr uint32_t kUzp2_4S   = (kOpPerm << 8) | (3 << 0) | (0 << 3);
    static constexpr uint32_t kUzp2_2D   = (kOpPerm << 8) | (3 << 0) | (1 << 3);
    static constexpr uint32_t kTrn1_4S   = (kOpPerm << 8) | (4 << 0);
    static constexpr uint32_t kTrn2_4S   = (kOpPerm << 8) | (5 << 0);
    static constexpr uint32_t kExt4      = (kOpPerm << 8) | (7 << 0) | (0 << 3);
    static constexpr uint32_t kExt8      = (kOpPerm << 8) | (6 << 0) | (1 << 3);
    static constexpr uint32_t kExt12     = (kOpPerm << 8) | (7 << 0) | (1 << 3);

    static constexpr uint32_t kRev64     = (kOpRev64 << 8) | (1 << 0); // Dummy bit so we don't end up having all zeros.

    static constexpr uint32_t kDup0      = (kOpDup  << 8) | (0 << 0);
    static constexpr uint32_t kDup1      = (kOpDup  << 8) | (1 << 0);
    static constexpr uint32_t kDup2      = (kOpDup  << 8) | (2 << 0);
    static constexpr uint32_t kDup3      = (kOpDup  << 8) | (3 << 0);

    static constexpr uint32_t kSrcAA = (0 << 4) | (0 << 6);
    static constexpr uint32_t kSrcAB = (0 << 4) | (1 << 6);
    static constexpr uint32_t kSrcAC = (0 << 4) | (2 << 6);
    static constexpr uint32_t kSrcAD = (0 << 4) | (3 << 6);
    static constexpr uint32_t kSrcBA = (1 << 4) | (0 << 6);
    static constexpr uint32_t kSrcBB = (1 << 4) | (1 << 6);
    static constexpr uint32_t kSrcBC = (1 << 4) | (2 << 6);
    static constexpr uint32_t kSrcBD = (1 << 4) | (3 << 6);
    static constexpr uint32_t kSrcCA = (2 << 4) | (0 << 6);
    static constexpr uint32_t kSrcCB = (2 << 4) | (1 << 6);
    static constexpr uint32_t kSrcCC = (2 << 4) | (2 << 6);
    static constexpr uint32_t kSrcCD = (2 << 4) | (3 << 6);
    static constexpr uint32_t kSrcDA = (3 << 4) | (0 << 6);
    static constexpr uint32_t kSrcDB = (3 << 4) | (1 << 6);
    static constexpr uint32_t kSrcDC = (3 << 4) | (2 << 6);
    static constexpr uint32_t kSrcDD = (3 << 4) | (3 << 6);

    static constexpr uint32_t kSrcA = kSrcAA;
    static constexpr uint32_t kSrcB = kSrcBB;
    static constexpr uint32_t kSrcC = kSrcCC;
    static constexpr uint32_t kSrcD = kSrcDD;

    // Alias to nothing to make the table easier to read.
    static constexpr uint32_t k_ = 0;
    static constexpr uint32_t kSrc_ = 0;

    uint32_t data;

    BL_INLINE_NODEBUG bool isValid() const noexcept { return data != 0u; }
    BL_INLINE_NODEBUG uint32_t op() const noexcept { return data >> 8; }
    BL_INLINE_NODEBUG uint32_t isInsOp() const noexcept { return op() == kOpInsS || op() == kOpInsD; }

    BL_INLINE_NODEBUG uint32_t dupIdx() const noexcept { return data & 0x3u; }

    BL_INLINE_NODEBUG uint32_t permOp() const noexcept { return data & 0x7u; }
    BL_INLINE_NODEBUG bool permS4() const noexcept { return (data & (1u << 3)) == 0u; }
    BL_INLINE_NODEBUG uint32_t permExtImm() const noexcept { return ((data & 0x1) << 2) + (data & 0x8); }

    BL_INLINE_NODEBUG uint32_t insSrc() const noexcept { return (data >> 0) & 0x3u; }
    BL_INLINE_NODEBUG uint32_t insDst() const noexcept { return (data >> 2) & 0x3u; }

    BL_INLINE_NODEBUG uint32_t srcA() const noexcept { return (data >> 4) & 0x3u; }
    BL_INLINE_NODEBUG uint32_t srcB() const noexcept { return (data >> 6) & 0x3u; }
  };

  //! \name Members
  //! \{

  uint32_t combined;

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG uint32_t count() const noexcept {
    return 1u + uint32_t(((combined >> 11) & 0x7FF) != 0u) + uint32_t((combined >> 22) != 0u);
  }

  BL_INLINE_NODEBUG Op op(uint32_t index) const noexcept {
    return Op{(combined >> (index * 11u)) & 0x7FFu};
  }

  //! \}
};

// These tables provide all combinations for all possible 32-bit interleaved shuffles (there is 256 combinations
// in total). It prioritizes lane moves, and then operations that can have either one or two inputs. Initially,
// there are two sources (A, B), which can be used by any shuffle operation, which result is then referred as C.
// Then, all consecutive shuffles can use any of A, B, and C as their operands. The last operation is the result.
//
// Statistics:
//
//   - 1 Instruction: 5
//   - 2 Instructions: 113
//   - 3 Instructions: 138

#define OP(swiz, op0, src0, op1, src1, op2, src2) {                                                   \
  ((InterleavedShuffle32Ops::Op::k##op0) <<  0) | ((InterleavedShuffle32Ops::Op::kSrc##src0) <<  0) | \
  ((InterleavedShuffle32Ops::Op::k##op1) << 11) | ((InterleavedShuffle32Ops::Op::kSrc##src1) << 11) | \
  ((InterleavedShuffle32Ops::Op::k##op2) << 22) | ((InterleavedShuffle32Ops::Op::kSrc##src2) << 22)   \
}

static constexpr InterleavedShuffle32Ops interleaved_shuffle32_ops[256] = {
  OP(4400, Trn1_4S  , AB, Zip1_4S  , CC, _        , _ ), OP(4401, Rev64    , A , Dup0     , B , Zip1_2D  , CD), OP(4402, Ext8     , AA, Dup0     , B , Uzp1_4S  , CD), OP(4403, Ext12    , AA, Dup0     , B , Zip1_2D  , CD),
  OP(4410, Dup0     , B , Zip1_2D  , AC, _        , _ ), OP(4411, Dup1     , A , Dup0     , B , Uzp2_4S  , CD), OP(4412, Ext4     , AB, InsS_3To2, C , Rev64    , C ), OP(4413, Ext12    , AA, Dup0     , B , Uzp1_4S  , CD),
  OP(4420, Dup0     , B , Uzp1_4S  , AC, _        , _ ), OP(4421, Ext4     , AB, InsS_3To2, C , _        , _ ), OP(4422, Ext8     , AB, Trn1_4S  , CC, _        , _ ), OP(4423, Rev64    , A , Dup0     , B , Zip2_2D  , CD),
  OP(4430, Trn1_4S  , AB, Ext12    , AB, Zip1_4S  , CD), OP(4431, Dup0     , B , Uzp2_4S  , AC, _        , _ ), OP(4432, Dup0     , B , Zip2_2D  , AC, _        , _ ), OP(4433, Ext12    , AB, Zip1_4S  , CC, _        , _ ),
  OP(4500, Rev64    , B , Dup0     , A , Zip1_2D  , DC), OP(4501, Zip1_2D  , AB, Rev64    , C , _        , _ ), OP(4502, Uzp1_4S  , BA, Ext8     , CB, Rev64    , D ), OP(4503, Ext12    , AA, Rev64    , B , Zip1_2D  , CD),
  OP(4510, Rev64    , B , Zip1_2D  , AC, _        , _ ), OP(4511, Rev64    , B , Dup1     , A , Zip1_2D  , DC), OP(4512, Uzp2_4S  , AB, Uzp1_4S  , AC, Ext4     , DB), OP(4513, Trn2_4S  , AB, Zip1_2D  , CB, Ext12    , AD),
  OP(4520, Dup1     , B , Uzp1_4S  , CB, Uzp1_4S  , AD), OP(4521, Ext4     , AB, InsS_1To2, B , _        , _ ), OP(4522, Rev64    , B , Dup2     , A , Zip1_2D  , DC), OP(4523, Ext8     , AB, Rev64    , C , _        , _ ),
  OP(4530, Ext12    , AA, Zip1_2D  , CB, Rev64    , D ), OP(4531, Uzp2_4S  , AB, InsS_0To3, B , _        , _ ), OP(4532, Rev64    , B , Ext8     , AC, _        , _ ), OP(4533, Rev64    , B , Dup3     , A , Zip1_2D  , DC),
  OP(4600, Ext8     , BB, Dup0     , A , Uzp1_4S  , DC), OP(4601, Uzp1_4S  , BB, Zip1_2D  , AC, Rev64    , D ), OP(4602, Uzp1_4S  , AB, Rev64    , C , _        , _ ), OP(4603, Uzp1_4S  , AB, InsS_3To1, C , Ext12    , AC),
  OP(4610, Dup2     , B , Trn1_4S  , CB, Zip1_2D  , AD), OP(4611, Ext8     , BB, Dup1     , A , Uzp1_4S  , DC), OP(4612, Trn1_4S  , AB, Zip1_4S  , AC, Zip2_4S  , CD), OP(4613, Rev64    , B , Uzp2_4S  , AC, Rev64    , D ),
  OP(4620, Ext8     , BB, Uzp1_4S  , AC, _        , _ ), OP(4621, Ext4     , AB, InsS_2To2, B , _        , _ ), OP(4622, Ext8     , BB, Dup2     , A , Uzp1_4S  , DC), OP(4623, Trn1_4S  , AB, Zip2_2D  , AC, Ext4     , DB),
  OP(4630, Ext12    , AB, Zip1_4S  , AC, InsS_3To2, C ), OP(4631, Ext4     , BB, Uzp2_4S  , AC, _        , _ ), OP(4632, Zip2_2D  , AB, InsS_0To3, B , _        , _ ), OP(4633, Ext8     , BB, Dup3     , A , Uzp1_4S  , DC),
  OP(4700, Ext12    , BB, Dup0     , A , Zip1_2D  , DC), OP(4701, Ext12    , BB, Rev64    , A , Zip1_2D  , DC), OP(4702, Uzp1_4S  , AB, InsS_3To3, B , Rev64    , C ), OP(4703, Ext12    , BB, Ext12    , AA, Zip1_2D  , DC),
  OP(4710, Ext12    , BB, Zip1_2D  , AC, _        , _ ), OP(4711, Ext12    , BB, Dup1     , A , Zip1_2D  , DC), OP(4712, Trn2_4S  , BA, InsS_2To0, A , InsS_0To3, B ), OP(4713, Uzp2_4S  , AB, InsS_0To2, C , Ext4     , CB),
  OP(4720, Dup3     , B , Uzp1_4S  , CB, Uzp1_4S  , AD), OP(4721, Ext4     , AB, InsS_3To2, B , _        , _ ), OP(4722, Ext12    , BB, Dup2     , A , Zip1_2D  , DC), OP(4723, Ext12    , BB, Rev64    , A , Ext8     , DC),
  OP(4730, Rev64    , A , InsS_3To3, B , Ext4     , CB), OP(4731, Dup3     , B , Ext4     , CB, Uzp2_4S  , AD), OP(4732, Ext12    , BB, Ext8     , AC, _        , _ ), OP(4733, Ext12    , BB, Dup3     , A , Zip1_2D  , DC),
  OP(5400, Dup0     , A , Zip1_2D  , CB, _        , _ ), OP(5401, Rev64    , A , Zip1_2D  , CB, _        , _ ), OP(5402, Ext8     , AB, InsS_0To1, A , _        , _ ), OP(5403, Ext12    , AA, Zip1_2D  , CB, _        , _ ),
  OP(5410, Zip1_2D  , AB, _        , _ , _        , _ ), OP(5411, Dup1     , A , Zip1_2D  , CB, _        , _ ), OP(5412, Zip1_2D  , AB, InsS_2To0, A , _        , _ ), OP(5413, Zip1_2D  , AB, InsS_3To0, A , _        , _ ),
  OP(5420, Uzp1_4S  , BA, Ext8     , CB, _        , _ ), OP(5421, Ext12    , BA, Ext8     , CB, _        , _ ), OP(5422, Dup2     , A , Zip1_2D  , CB, _        , _ ), OP(5423, Rev64    , A , Ext8     , CB, _        , _ ),
  OP(5430, Zip1_2D  , AB, InsS_3To1, A , _        , _ ), OP(5431, Uzp2_4S  , BA, Ext8     , CB, _        , _ ), OP(5432, Ext8     , AB, _        , _ , _        , _ ), OP(5433, Dup3     , A , Zip1_2D  , CB, _        , _ ),
  OP(5500, Dup1     , B , Dup0     , A , Uzp2_4S  , DC), OP(5501, Rev64    , A , Dup1     , B , Zip1_2D  , CD), OP(5502, Ext8     , AA, Dup1     , B , Uzp1_4S  , CD), OP(5503, Ext12    , AA, Dup1     , B , Zip1_2D  , CD),
  OP(5510, Dup1     , B , Zip1_2D  , AC, _        , _ ), OP(5511, Trn2_4S  , AB, Zip1_4S  , CC, _        , _ ), OP(5512, Uzp2_4S  , AB, Zip2_2D  , AC, Trn1_4S  , DC), OP(5513, Ext12    , AA, Dup1     , B , Uzp1_4S  , CD),
  OP(5520, Dup1     , B , Uzp1_4S  , AC, _        , _ ), OP(5521, Ext12    , BA, Dup1     , B , Zip2_2D  , CD), OP(5522, Dup2     , A , Dup1     , B , Uzp2_4S  , CD), OP(5523, Rev64    , A , Dup1     , B , Zip2_2D  , CD),
  OP(5530, Ext12    , AB, Trn1_4S  , AC, InsS_3To2, D ), OP(5531, Dup1     , B , Uzp2_4S  , AC, _        , _ ), OP(5532, Dup1     , B , Zip2_2D  , AC, _        , _ ), OP(5533, Ext12    , AB, Trn1_4S  , CC, _        , _ ),
  OP(5600, Trn1_4S  , BA, InsS_1To3, B , InsS_1To0, C ), OP(5601, Ext4     , BB, Zip1_2D  , AC, Rev64    , D ), OP(5602, Uzp1_4S  , AB, InsS_1To2, B , Rev64    , C ), OP(5603, Ext12    , AA, Zip1_2D  , CB, InsS_2To2, B ),
  OP(5610, Zip1_2D  , AB, InsS_2To2, B , _        , _ ), OP(5611, Zip1_4S  , AB, Trn1_4S  , CB, Zip2_4S  , DC), OP(5612, Trn2_4S  , AB, Zip2_4S  , AB, Zip1_4S  , DC), OP(5613, Trn2_4S  , AB, Zip2_4S  , CB, Zip1_4S  , DC),
  OP(5620, Dup2     , B , Uzp2_4S  , CB, Uzp1_4S  , AD), OP(5621, Rev64    , B , Ext4     , AC, InsS_3To2, C ), OP(5622, Uzp1_4S  , AB, Zip1_2D  , CB, Trn2_4S  , CD), OP(5623, Uzp1_4S  , AB, Uzp2_4S  , CB, Ext12    , AD),
  OP(5630, Ext12    , AB, Trn1_4S  , AC, InsS_3To2, C ), OP(5631, Dup2     , B , Zip1_2D  , CB, Uzp2_4S  , AD), OP(5632, Zip2_2D  , AB, InsS_1To3, B , _        , _ ), OP(5633, Ext12    , AB, InsS_3To1, C , Ext12    , AC),
  OP(5700, Ext12    , BB, Dup0     , A , Uzp1_4S  , DC), OP(5701, Uzp2_4S  , BB, Zip1_2D  , AC, Rev64    , D ), OP(5702, Rev64    , B , Uzp1_4S  , AC, Rev64    , D ), OP(5703, Uzp2_4S  , BB, InsS_0To0, A , Ext12    , AC),
  OP(5710, Zip1_2D  , AB, InsS_3To2, B , _        , _ ), OP(5711, Ext12    , BB, Dup1     , A , Uzp1_4S  , DC), OP(5712, Uzp2_4S  , AB, InsS_2To1, A , Rev64    , C ), OP(5713, Uzp2_4S  , AB, Rev64    , C , _        , _ ),
  OP(5720, Ext12    , BB, Uzp1_4S  , AC, _        , _ ), OP(5721, Rev64    , B , Ext4     , AC, InsS_2To2, C ), OP(5722, Ext12    , BB, Dup2     , A , Uzp1_4S  , DC), OP(5723, Uzp2_4S  , BB, Zip2_2D  , AC, Rev64    , D ),
  OP(5730, Ext12    , AB, Trn1_4S  , AC, InsS_3To2, B ), OP(5731, Ext8     , BB, Uzp2_4S  , AC, _        , _ ), OP(5732, Ext8     , AB, InsS_3To2, B , _        , _ ), OP(5733, Ext12    , BB, Dup3     , A , Uzp1_4S  , DC),
  OP(6400, Dup0     , A , Uzp1_4S  , CB, _        , _ ), OP(6401, Dup1     , A , Uzp1_4S  , CA, Uzp1_4S  , DB), OP(6402, Ext8     , AA, Uzp1_4S  , CB, _        , _ ), OP(6403, Dup3     , A , Uzp1_4S  , CA, Uzp1_4S  , DB),
  OP(6410, Uzp1_4S  , BB, Zip1_2D  , AC, _        , _ ), OP(6411, Dup1     , A , Uzp1_4S  , CB, _        , _ ), OP(6412, Dup2     , A , Uzp2_4S  , CA, Uzp1_4S  , DB), OP(6413, Ext12    , AA, Uzp1_4S  , CB, _        , _ ),
  OP(6420, Uzp1_4S  , AB, _        , _ , _        , _ ), OP(6421, Uzp1_4S  , AB, InsS_1To0, A , _        , _ ), OP(6422, Dup2     , A , Uzp1_4S  , CB, _        , _ ), OP(6423, Uzp1_4S  , AB, InsS_3To0, A , _        , _ ),
  OP(6430, Uzp1_4S  , AB, InsS_3To1, A , _        , _ ), OP(6431, Rev64    , B , Uzp2_4S  , AC, _        , _ ), OP(6432, Uzp1_4S  , BB, Zip2_2D  , AC, _        , _ ), OP(6433, Dup3     , A , Uzp1_4S  , CB, _        , _ ),
  OP(6500, Ext12    , BB, Dup0     , A , Zip2_2D  , DC), OP(6501, Ext4     , BB, Rev64    , A , Zip1_2D  , DC), OP(6502, Rev64    , A , Ext12    , CB, InsS_1To1, C ), OP(6503, Ext12    , AB, InsS_0To1, A , _        , _ ),
  OP(6510, Ext4     , BB, Zip1_2D  , AC, _        , _ ), OP(6511, Ext12    , BB, Dup1     , A , Zip2_2D  , DC), OP(6512, Rev64    , A , Ext12    , CB, InsS_0To1, C ), OP(6513, Ext12    , AB, InsS_1To1, A , _        , _ ),
  OP(6520, Uzp1_4S  , AB, InsS_1To2, B , _        , _ ), OP(6521, Trn2_4S  , AB, Zip2_4S  , AB, Zip1_4S  , CD), OP(6522, Ext12    , BB, Dup2     , A , Zip2_2D  , DC), OP(6523, Ext12    , AB, InsS_2To1, A , _        , _ ),
  OP(6530, Ext4     , BA, Trn2_4S  , CA, Ext8     , DC), OP(6531, Uzp2_4S  , AB, InsS_2To3, B , _        , _ ), OP(6532, Ext12    , BB, Zip2_2D  , AC, _        , _ ), OP(6533, Ext12    , AB, InsS_0To1, C , _        , _ ),
  OP(6600, Dup2     , B , Dup0     , A , Uzp2_4S  , DC), OP(6601, Rev64    , A , Dup2     , B , Zip1_2D  , CD), OP(6602, Ext8     , AA, Dup2     , B , Uzp1_4S  , CD), OP(6603, Ext12    , AA, Dup2     , B , Zip1_2D  , CD),
  OP(6610, Dup2     , B , Zip1_2D  , AC, _        , _ ), OP(6611, Dup2     , B , Dup1     , A , Uzp2_4S  , DC), OP(6612, Zip2_4S  , AB, Trn2_4S  , AC, Zip1_4S  , CD), OP(6613, Ext12    , AA, Dup2     , B , Uzp1_4S  , CD),
  OP(6620, Dup2     , B , Uzp1_4S  , AC, _        , _ ), OP(6621, Ext12    , BA, Dup2     , B , Zip2_2D  , CD), OP(6622, Trn1_4S  , AB, Zip2_4S  , CC, _        , _ ), OP(6623, Rev64    , A , Dup2     , B , Zip2_2D  , CD),
  OP(6630, Trn1_4S  , AB, InsS_3To1, A , InsS_3To2, C ), OP(6631, Dup2     , B , Uzp2_4S  , AC, _        , _ ), OP(6632, Dup2     , B , Zip2_2D  , AC, _        , _ ), OP(6633, Dup3     , A , Dup2     , B , Uzp2_4S  , CD),
  OP(6700, Rev64    , B , Dup0     , A , Zip2_2D  , DC), OP(6701, Ext8     , BA, Rev64    , C , Ext8     , DD), OP(6702, Uzp1_4S  , BA, Zip2_2D  , CB, Rev64    , D ), OP(6703, Ext4     , AA, Rev64    , B , Zip2_2D  , CD),
  OP(6710, Rev64    , B , InsD_0To0, A , _        , _ ), OP(6711, Rev64    , B , Dup1     , A , Zip2_2D  , DC), OP(6712, Ext12    , BA, Zip2_2D  , CB, Rev64    , D ), OP(6713, Uzp2_4S  , BA, Zip2_2D  , CB, Rev64    , D ),
  OP(6720, Uzp1_4S  , AB, InsS_3To2, B , _        , _ ), OP(6721, Ext12    , BA, Rev64    , B , Zip2_2D  , CD), OP(6722, Rev64    , B , Dup2     , A , Zip2_2D  , DC), OP(6723, Zip2_2D  , AB, Rev64    , C , _        , _ ),
  OP(6730, Ext4     , AA, Zip2_2D  , CB, Rev64    , D ), OP(6731, Dup3     , B , Uzp1_4S  , CB, Uzp2_4S  , AD), OP(6732, Rev64    , B , Zip2_2D  , AC, _        , _ ), OP(6733, Rev64    , B , Dup3     , A , Zip2_2D  , DC),
  OP(7400, Uzp1_4S  , AB, InsS_3To3, B , InsS_0To1, C ), OP(7401, Ext12    , BB, Zip1_2D  , AC, Rev64    , D ), OP(7402, Ext8     , AB, Trn1_4S  , CA, InsS_3To3, B ), OP(7403, Rev64    , B , InsS_0To0, A , Ext12    , AC),
  OP(7410, Zip1_2D  , AB, InsS_3To3, B , _        , _ ), OP(7411, Trn2_4S  , AB, InsS_0To2, B , InsS_0To1, C ), OP(7412, Ext4     , AB, Trn2_4S  , CB, InsS_0To1, C ), OP(7413, Ext4     , AB, Zip2_4S  , CB, InsS_0To1, C ),
  OP(7420, Uzp1_4S  , AB, InsS_3To3, B , _        , _ ), OP(7421, Ext12    , BA, Trn1_4S  , BC, Ext8     , CD), OP(7422, Ext4     , AB, Trn2_4S  , CB, InsS_0To1, D ), OP(7423, Ext12    , BB, Ext8     , AC, Rev64    , D ),
  OP(7430, Uzp2_4S  , AB, InsS_0To0, A , InsS_0To2, B ), OP(7431, Uzp2_4S  , AB, InsS_0To2, B , _        , _ ), OP(7432, Zip2_2D  , AB, InsS_0To2, B , _        , _ ), OP(7433, Trn2_4S  , AB, Ext4     , AB, Zip2_4S  , DC),
  OP(7500, Dup0     , A , Uzp2_4S  , CB, _        , _ ), OP(7501, Uzp2_4S  , AB, InsS_0To1, A , _        , _ ), OP(7502, Ext4     , AA, Uzp2_4S  , CB, _        , _ ), OP(7503, Dup3     , A , Ext4     , CA, Uzp2_4S  , DB),
  OP(7510, Uzp2_4S  , BB, Zip1_2D  , AC, _        , _ ), OP(7511, Dup1     , A , Uzp2_4S  , CB, _        , _ ), OP(7512, Dup2     , A , Zip1_2D  , CA, Uzp2_4S  , DB), OP(7513, Ext8     , AA, Uzp2_4S  , CB, _        , _ ),
  OP(7520, Rev64    , B , Uzp1_4S  , AC, _        , _ ), OP(7521, Uzp2_4S  , AB, InsS_2To1, A , _        , _ ), OP(7522, Dup2     , A , Uzp2_4S  , CB, _        , _ ), OP(7523, Dup3     , A , Uzp1_4S  , CA, Uzp2_4S  , DB),
  OP(7530, Uzp2_4S  , AB, InsS_0To0, A , _        , _ ), OP(7531, Uzp2_4S  , AB, _        , _ , _        , _ ), OP(7532, Uzp2_4S  , BB, Zip2_2D  , AC, _        , _ ), OP(7533, Dup3     , A , Uzp2_4S  , CB, _        , _ ),
  OP(7600, Dup0     , A , Zip2_2D  , CB, _        , _ ), OP(7601, Rev64    , A , InsD_1To1, B , _        , _ ), OP(7602, Zip2_2D  , AB, InsS_0To1, A , _        , _ ), OP(7603, Ext4     , AA, Zip2_2D  , CB, _        , _ ),
  OP(7610, Zip2_2D  , BB, Zip1_2D  , AC, _        , _ ), OP(7611, Dup1     , A , Zip2_2D  , CB, _        , _ ), OP(7612, Zip2_2D  , AB, InsS_1To1, A , _        , _ ), OP(7613, Dup3     , A , Zip1_4S  , CA, Zip2_2D  , DB),
  OP(7620, Uzp1_4S  , BA, Zip2_2D  , CB, _        , _ ), OP(7621, Ext12    , BA, Zip2_2D  , CB, _        , _ ), OP(7622, Dup2     , A , Zip2_2D  , CB, _        , _ ), OP(7623, Rev64    , A , Zip2_2D  , CB, _        , _ ),
  OP(7630, Zip2_2D  , AB, InsS_0To0, A , _        , _ ), OP(7631, Uzp2_4S  , BA, Zip2_2D  , CB, _        , _ ), OP(7632, Zip2_2D  , AB, _        , _ , _        , _ ), OP(7633, Dup3     , A , Zip2_2D  , CB, _        , _ ),
  OP(7700, Dup3     , B , Dup0     , A , Uzp2_4S  , DC), OP(7701, Rev64    , A , Dup3     , B , Zip1_2D  , CD), OP(7702, Ext8     , AA, Dup3     , B , Uzp1_4S  , CD), OP(7703, Ext12    , AA, Dup3     , B , Zip1_2D  , CD),
  OP(7710, Dup3     , B , Zip1_2D  , AC, _        , _ ), OP(7711, Dup3     , B , Dup1     , A , Uzp2_4S  , DC), OP(7712, Trn2_4S  , BA, InsS_2To0, A , InsS_2To3, C ), OP(7713, Ext12    , AA, Dup3     , B , Uzp1_4S  , CD),
  OP(7720, Dup3     , B , Uzp1_4S  , AC, _        , _ ), OP(7721, Ext12    , BA, Dup3     , B , Zip2_2D  , CD), OP(7722, Dup3     , B , Dup2     , A , Uzp2_4S  , DC), OP(7723, Rev64    , A , Dup3     , B , Zip2_2D  , CD),
  OP(7730, Uzp2_4S  , AB, InsS_0To0, A , InsS_3To2, C ), OP(7731, Dup3     , B , Uzp2_4S  , AC, _        , _ ), OP(7732, Dup3     , B , Zip2_2D  , AC, _        , _ ), OP(7733, Trn2_4S  , AB, Zip2_4S  , CC, _        , _ )
};

static constexpr InterleavedShuffle32Ops interleaved_shuffle32_ops_dst_same_as_b[256] = {
  OP(4400, Trn1_4S  , AB, Zip1_4S  , CC, _        , _ ), OP(4401, Rev64    , A , Dup0     , B , Zip1_2D  , CD), OP(4402, Ext8     , AA, Dup0     , B , Uzp1_4S  , CD), OP(4403, Ext12    , AA, Dup0     , B , Zip1_2D  , CD),
  OP(4410, Dup0     , B , Zip1_2D  , AC, _        , _ ), OP(4411, Dup1     , A , Dup0     , B , Uzp2_4S  , CD), OP(4412, Ext4     , AB, InsS_3To2, C , Rev64    , C ), OP(4413, Ext12    , AA, Dup0     , B , Uzp1_4S  , CD),
  OP(4420, Dup0     , B , Uzp1_4S  , AC, _        , _ ), OP(4421, Ext4     , AB, InsS_3To2, C , _        , _ ), OP(4422, Ext8     , AB, Trn1_4S  , CC, _        , _ ), OP(4423, Rev64    , A , Dup0     , B , Zip2_2D  , CD),
  OP(4430, Trn1_4S  , AB, Ext12    , AB, Zip1_4S  , CD), OP(4431, Dup0     , B , Uzp2_4S  , AC, _        , _ ), OP(4432, Dup0     , B , Zip2_2D  , AC, _        , _ ), OP(4433, Ext12    , AB, Zip1_4S  , CC, _        , _ ),
  OP(4500, Rev64    , B , Dup0     , A , Zip1_2D  , DC), OP(4501, Zip1_2D  , AB, Rev64    , C , _        , _ ), OP(4502, Uzp1_4S  , BA, Ext8     , CB, Rev64    , D ), OP(4503, Ext12    , AA, Rev64    , B , Zip1_2D  , CD),
  OP(4510, Rev64    , B , Zip1_2D  , AC, _        , _ ), OP(4511, Rev64    , B , Dup1     , A , Zip1_2D  , DC), OP(4512, Uzp2_4S  , AB, Uzp1_4S  , AC, Ext4     , DB), OP(4513, Trn2_4S  , AB, Zip1_2D  , CB, Ext12    , AD),
  OP(4520, Dup1     , B , Uzp1_4S  , CB, Uzp1_4S  , AD), OP(4521, Ext4     , AB, InsS_1To2, B , _        , _ ), OP(4522, Rev64    , B , Dup2     , A , Zip1_2D  , DC), OP(4523, Ext8     , AB, Rev64    , C , _        , _ ),
  OP(4530, Ext12    , AA, Zip1_2D  , CB, Rev64    , D ), OP(4531, Uzp2_4S  , AB, InsS_0To3, B , _        , _ ), OP(4532, Rev64    , B , Ext8     , AC, _        , _ ), OP(4533, Rev64    , B , Dup3     , A , Zip1_2D  , DC),
  OP(4600, Ext8     , BB, Dup0     , A , Uzp1_4S  , DC), OP(4601, Uzp1_4S  , BB, Zip1_2D  , AC, Rev64    , D ), OP(4602, Uzp1_4S  , AB, Rev64    , C , _        , _ ), OP(4603, Uzp1_4S  , AB, InsS_3To1, C , Ext12    , AC),
  OP(4610, Dup2     , B , Trn1_4S  , CB, Zip1_2D  , AD), OP(4611, Ext8     , BB, Dup1     , A , Uzp1_4S  , DC), OP(4612, Trn1_4S  , AB, Zip1_4S  , AC, Zip2_4S  , CD), OP(4613, Rev64    , B , Uzp2_4S  , AC, Rev64    , D ),
  OP(4620, Ext8     , BB, Uzp1_4S  , AC, _        , _ ), OP(4621, Ext4     , AB, InsS_2To2, B , _        , _ ), OP(4622, Ext8     , BB, Dup2     , A , Uzp1_4S  , DC), OP(4623, Trn1_4S  , AB, Zip2_2D  , AC, Ext4     , DB),
  OP(4630, Ext12    , AB, Zip1_4S  , AC, InsS_3To2, C ), OP(4631, Ext4     , BB, Uzp2_4S  , AC, _        , _ ), OP(4632, Zip2_2D  , AB, InsS_0To3, B , _        , _ ), OP(4633, Ext8     , BB, Dup3     , A , Uzp1_4S  , DC),
  OP(4700, Ext12    , BB, Dup0     , A , Zip1_2D  , DC), OP(4701, Ext12    , BB, Rev64    , A , Zip1_2D  , DC), OP(4702, Uzp1_4S  , AB, InsS_3To3, B , Rev64    , C ), OP(4703, Ext12    , BB, Ext12    , AA, Zip1_2D  , DC),
  OP(4710, Ext12    , BB, Zip1_2D  , AC, _        , _ ), OP(4711, Ext12    , BB, Dup1     , A , Zip1_2D  , DC), OP(4712, Ext12    , BB, Zip1_2D  , AC, InsS_2To0, A ), OP(4713, Uzp2_4S  , AB, InsS_0To2, C , Ext4     , CB),
  OP(4720, Dup3     , B , Uzp1_4S  , CB, Uzp1_4S  , AD), OP(4721, Ext4     , AB, InsS_3To2, B , _        , _ ), OP(4722, Ext12    , BB, Dup2     , A , Zip1_2D  , DC), OP(4723, Ext12    , BB, Rev64    , A , Ext8     , DC),
  OP(4730, Rev64    , A , InsS_3To3, B , Ext4     , CB), OP(4731, Dup3     , B , Ext4     , CB, Uzp2_4S  , AD), OP(4732, Ext12    , BB, Ext8     , AC, _        , _ ), OP(4733, Ext12    , BB, Dup3     , A , Zip1_2D  , DC),
  OP(5400, Dup0     , A , Zip1_2D  , CB, _        , _ ), OP(5401, Rev64    , A , Zip1_2D  , CB, _        , _ ), OP(5402, Ext8     , AB, InsS_0To1, A , _        , _ ), OP(5403, Ext12    , AA, Zip1_2D  , CB, _        , _ ),
  OP(5410, Zip1_2D  , AB, _        , _ , _        , _ ), OP(5411, Dup1     , A , Zip1_2D  , CB, _        , _ ), OP(5412, Zip1_2D  , AB, InsS_2To0, A , _        , _ ), OP(5413, Zip1_2D  , AB, InsS_3To0, A , _        , _ ),
  OP(5420, Uzp1_4S  , BA, Ext8     , CB, _        , _ ), OP(5421, Ext12    , BA, Ext8     , CB, _        , _ ), OP(5422, Dup2     , A , Zip1_2D  , CB, _        , _ ), OP(5423, Rev64    , A , Ext8     , CB, _        , _ ),
  OP(5430, Zip1_2D  , AB, InsS_3To1, A , _        , _ ), OP(5431, Uzp2_4S  , BA, Ext8     , CB, _        , _ ), OP(5432, Ext8     , AB, _        , _ , _        , _ ), OP(5433, Dup3     , A , Zip1_2D  , CB, _        , _ ),
  OP(5500, Dup1     , B , Dup0     , A , Uzp2_4S  , DC), OP(5501, Rev64    , A , Dup1     , B , Zip1_2D  , CD), OP(5502, Ext8     , AA, Dup1     , B , Uzp1_4S  , CD), OP(5503, Ext12    , AA, Dup1     , B , Zip1_2D  , CD),
  OP(5510, Dup1     , B , Zip1_2D  , AC, _        , _ ), OP(5511, Trn2_4S  , AB, Zip1_4S  , CC, _        , _ ), OP(5512, Uzp2_4S  , AB, Zip2_2D  , AC, Trn1_4S  , DC), OP(5513, Ext12    , AA, Dup1     , B , Uzp1_4S  , CD),
  OP(5520, Dup1     , B , Uzp1_4S  , AC, _        , _ ), OP(5521, Ext12    , BA, Dup1     , B , Zip2_2D  , CD), OP(5522, Dup2     , A , Dup1     , B , Uzp2_4S  , CD), OP(5523, Rev64    , A , Dup1     , B , Zip2_2D  , CD),
  OP(5530, Ext12    , AB, Trn1_4S  , AC, InsS_3To2, D ), OP(5531, Dup1     , B , Uzp2_4S  , AC, _        , _ ), OP(5532, Dup1     , B , Zip2_2D  , AC, _        , _ ), OP(5533, Ext12    , AB, Trn1_4S  , CC, _        , _ ),
  OP(5600, Trn1_4S  , BA, InsS_1To3, B , InsS_1To0, C ), OP(5601, Ext4     , BB, Zip1_2D  , AC, Rev64    , D ), OP(5602, Uzp1_4S  , AB, InsS_1To2, B , Rev64    , C ), OP(5603, Trn1_4S  , BA, InsS_1To3, B , InsS_3To0, A ),
  OP(5610, Zip1_2D  , AB, InsS_2To2, B , _        , _ ), OP(5611, Zip1_4S  , AB, Trn1_4S  , CB, Zip2_4S  , DC), OP(5612, Trn2_4S  , AB, Zip2_4S  , AB, Zip1_4S  , DC), OP(5613, Trn2_4S  , AB, Zip2_4S  , CB, Zip1_4S  , DC),
  OP(5620, Dup2     , B , Uzp2_4S  , CB, Uzp1_4S  , AD), OP(5621, Rev64    , B , Ext4     , AC, InsS_3To2, C ), OP(5622, Uzp1_4S  , AB, Zip1_2D  , CB, Trn2_4S  , CD), OP(5623, Uzp1_4S  , AB, Uzp2_4S  , CB, Ext12    , AD),
  OP(5630, Ext12    , AB, Trn1_4S  , AC, InsS_3To2, C ), OP(5631, Dup2     , B , Zip1_2D  , CB, Uzp2_4S  , AD), OP(5632, Zip2_2D  , AB, InsS_1To3, B , _        , _ ), OP(5633, Ext12    , AB, InsS_3To1, C , Ext12    , AC),
  OP(5700, Ext12    , BB, Dup0     , A , Uzp1_4S  , DC), OP(5701, Uzp2_4S  , BB, Zip1_2D  , AC, Rev64    , D ), OP(5702, Rev64    , B , Uzp1_4S  , AC, Rev64    , D ), OP(5703, Uzp2_4S  , BB, InsS_0To0, A , Ext12    , AC),
  OP(5710, Zip1_2D  , AB, InsS_3To2, B , _        , _ ), OP(5711, Ext12    , BB, Dup1     , A , Uzp1_4S  , DC), OP(5712, Uzp2_4S  , AB, InsS_2To1, A , Rev64    , C ), OP(5713, Uzp2_4S  , AB, Rev64    , C , _        , _ ),
  OP(5720, Ext12    , BB, Uzp1_4S  , AC, _        , _ ), OP(5721, Rev64    , B , Ext4     , AC, InsS_2To2, C ), OP(5722, Ext12    , BB, Dup2     , A , Uzp1_4S  , DC), OP(5723, Uzp2_4S  , BB, Zip2_2D  , AC, Rev64    , D ),
  OP(5730, Ext12    , BB, Uzp1_4S  , AC, InsS_3To1, A ), OP(5731, Ext8     , BB, Uzp2_4S  , AC, _        , _ ), OP(5732, Ext8     , AB, InsS_3To2, B , _        , _ ), OP(5733, Ext12    , BB, Dup3     , A , Uzp1_4S  , DC),
  OP(6400, Dup0     , A , Uzp1_4S  , CB, _        , _ ), OP(6401, Dup1     , A , Uzp1_4S  , CA, Uzp1_4S  , DB), OP(6402, Ext8     , AA, Uzp1_4S  , CB, _        , _ ), OP(6403, Dup3     , A , Uzp1_4S  , CA, Uzp1_4S  , DB),
  OP(6410, Uzp1_4S  , BB, Zip1_2D  , AC, _        , _ ), OP(6411, Dup1     , A , Uzp1_4S  , CB, _        , _ ), OP(6412, Dup2     , A , Uzp2_4S  , CA, Uzp1_4S  , DB), OP(6413, Ext12    , AA, Uzp1_4S  , CB, _        , _ ),
  OP(6420, Uzp1_4S  , AB, _        , _ , _        , _ ), OP(6421, Uzp1_4S  , AB, InsS_1To0, A , _        , _ ), OP(6422, Dup2     , A , Uzp1_4S  , CB, _        , _ ), OP(6423, Uzp1_4S  , AB, InsS_3To0, A , _        , _ ),
  OP(6430, Uzp1_4S  , AB, InsS_3To1, A , _        , _ ), OP(6431, Rev64    , B , Uzp2_4S  , AC, _        , _ ), OP(6432, Uzp1_4S  , BB, Zip2_2D  , AC, _        , _ ), OP(6433, Dup3     , A , Uzp1_4S  , CB, _        , _ ),
  OP(6500, Ext12    , BB, Dup0     , A , Zip2_2D  , DC), OP(6501, Ext4     , BB, Rev64    , A , Zip1_2D  , DC), OP(6502, Rev64    , A , Ext12    , CB, InsS_1To1, C ), OP(6503, Ext12    , AB, InsS_0To1, A , _        , _ ),
  OP(6510, Ext4     , BB, Zip1_2D  , AC, _        , _ ), OP(6511, Ext12    , BB, Dup1     , A , Zip2_2D  , DC), OP(6512, Rev64    , A , Ext12    , CB, InsS_0To1, C ), OP(6513, Ext12    , AB, InsS_1To1, A , _        , _ ),
  OP(6520, Uzp1_4S  , AB, InsS_1To2, B , _        , _ ), OP(6521, Trn2_4S  , AB, Zip2_4S  , AB, Zip1_4S  , CD), OP(6522, Ext12    , BB, Dup2     , A , Zip2_2D  , DC), OP(6523, Ext12    , AB, InsS_2To1, A , _        , _ ),
  OP(6530, Ext4     , BA, Trn2_4S  , CA, Ext8     , DC), OP(6531, Uzp2_4S  , AB, InsS_2To3, B , _        , _ ), OP(6532, Ext12    , BB, Zip2_2D  , AC, _        , _ ), OP(6533, Ext12    , AB, InsS_0To1, C , _        , _ ),
  OP(6600, Dup2     , B , Dup0     , A , Uzp2_4S  , DC), OP(6601, Rev64    , A , Dup2     , B , Zip1_2D  , CD), OP(6602, Ext8     , AA, Dup2     , B , Uzp1_4S  , CD), OP(6603, Ext12    , AA, Dup2     , B , Zip1_2D  , CD),
  OP(6610, Dup2     , B , Zip1_2D  , AC, _        , _ ), OP(6611, Dup2     , B , Dup1     , A , Uzp2_4S  , DC), OP(6612, Zip2_4S  , AB, Trn2_4S  , AC, Zip1_4S  , CD), OP(6613, Ext12    , AA, Dup2     , B , Uzp1_4S  , CD),
  OP(6620, Dup2     , B , Uzp1_4S  , AC, _        , _ ), OP(6621, Ext12    , BA, Dup2     , B , Zip2_2D  , CD), OP(6622, Trn1_4S  , AB, Zip2_4S  , CC, _        , _ ), OP(6623, Rev64    , A , Dup2     , B , Zip2_2D  , CD),
  OP(6630, Trn1_4S  , AB, InsS_3To1, A , InsS_3To2, C ), OP(6631, Dup2     , B , Uzp2_4S  , AC, _        , _ ), OP(6632, Dup2     , B , Zip2_2D  , AC, _        , _ ), OP(6633, Dup3     , A , Dup2     , B , Uzp2_4S  , CD),
  OP(6700, Rev64    , B , Dup0     , A , Zip2_2D  , DC), OP(6701, Ext8     , BA, Rev64    , C , Ext8     , DD), OP(6702, Uzp1_4S  , BA, Zip2_2D  , CB, Rev64    , D ), OP(6703, Ext4     , AA, Rev64    , B , Zip2_2D  , CD),
  OP(6710, Rev64    , B , InsD_0To0, A , _        , _ ), OP(6711, Rev64    , B , Dup1     , A , Zip2_2D  , DC), OP(6712, Ext12    , BA, Zip2_2D  , CB, Rev64    , D ), OP(6713, Uzp2_4S  , BA, Zip2_2D  , CB, Rev64    , D ),
  OP(6720, Uzp1_4S  , AB, InsS_3To2, B , _        , _ ), OP(6721, Ext12    , BA, Rev64    , B , Zip2_2D  , CD), OP(6722, Rev64    , B , Dup2     , A , Zip2_2D  , DC), OP(6723, Zip2_2D  , AB, Rev64    , C , _        , _ ),
  OP(6730, Ext4     , AA, Zip2_2D  , CB, Rev64    , D ), OP(6731, Dup3     , B , Uzp1_4S  , CB, Uzp2_4S  , AD), OP(6732, Rev64    , B , Zip2_2D  , AC, _        , _ ), OP(6733, Rev64    , B , Dup3     , A , Zip2_2D  , DC),
  OP(7400, Uzp1_4S  , AB, InsS_3To3, B , InsS_0To1, C ), OP(7401, Ext12    , BB, Zip1_2D  , AC, Rev64    , D ), OP(7402, Ext4     , AB, Trn2_4S  , CB, InsS_0To1, A ), OP(7403, Rev64    , B , InsS_0To0, A , Ext12    , AC),
  OP(7410, Zip1_2D  , AB, InsS_3To3, B , _        , _ ), OP(7411, Trn2_4S  , AB, InsS_0To2, B , InsS_0To1, C ), OP(7412, Ext4     , AB, Trn2_4S  , CB, InsS_0To1, C ), OP(7413, Ext4     , AB, Zip2_4S  , CB, InsS_0To1, C ),
  OP(7420, Uzp1_4S  , AB, InsS_3To3, B , _        , _ ), OP(7421, Ext12    , BA, Trn1_4S  , BC, Ext8     , CD), OP(7422, Ext4     , AB, Trn2_4S  , CB, InsS_0To1, D ), OP(7423, Ext12    , BB, Ext8     , AC, Rev64    , D ),
  OP(7430, Uzp2_4S  , AB, InsS_0To2, B , InsS_0To0, A ), OP(7431, Uzp2_4S  , AB, InsS_0To2, B , _        , _ ), OP(7432, Zip2_2D  , AB, InsS_0To2, B , _        , _ ), OP(7433, Trn2_4S  , AB, Ext4     , AB, Zip2_4S  , DC),
  OP(7500, Dup0     , A , Uzp2_4S  , CB, _        , _ ), OP(7501, Uzp2_4S  , AB, InsS_0To1, A , _        , _ ), OP(7502, Ext4     , AA, Uzp2_4S  , CB, _        , _ ), OP(7503, Dup3     , A , Ext4     , CA, Uzp2_4S  , DB),
  OP(7510, Uzp2_4S  , BB, Zip1_2D  , AC, _        , _ ), OP(7511, Dup1     , A , Uzp2_4S  , CB, _        , _ ), OP(7512, Dup2     , A , Zip1_2D  , CA, Uzp2_4S  , DB), OP(7513, Ext8     , AA, Uzp2_4S  , CB, _        , _ ),
  OP(7520, Rev64    , B , Uzp1_4S  , AC, _        , _ ), OP(7521, Uzp2_4S  , AB, InsS_2To1, A , _        , _ ), OP(7522, Dup2     , A , Uzp2_4S  , CB, _        , _ ), OP(7523, Dup3     , A , Uzp1_4S  , CA, Uzp2_4S  , DB),
  OP(7530, Uzp2_4S  , AB, InsS_0To0, A , _        , _ ), OP(7531, Uzp2_4S  , AB, _        , _ , _        , _ ), OP(7532, Uzp2_4S  , BB, Zip2_2D  , AC, _        , _ ), OP(7533, Dup3     , A , Uzp2_4S  , CB, _        , _ ),
  OP(7600, Dup0     , A , Zip2_2D  , CB, _        , _ ), OP(7601, Rev64    , A , InsD_1To1, B , _        , _ ), OP(7602, Zip2_2D  , AB, InsS_0To1, A , _        , _ ), OP(7603, Ext4     , AA, Zip2_2D  , CB, _        , _ ),
  OP(7610, Zip2_2D  , BB, Zip1_2D  , AC, _        , _ ), OP(7611, Dup1     , A , Zip2_2D  , CB, _        , _ ), OP(7612, Zip2_2D  , AB, InsS_1To1, A , _        , _ ), OP(7613, Dup3     , A , Zip1_4S  , CA, Zip2_2D  , DB),
  OP(7620, Uzp1_4S  , BA, Zip2_2D  , CB, _        , _ ), OP(7621, Ext12    , BA, Zip2_2D  , CB, _        , _ ), OP(7622, Dup2     , A , Zip2_2D  , CB, _        , _ ), OP(7623, Rev64    , A , Zip2_2D  , CB, _        , _ ),
  OP(7630, Zip2_2D  , AB, InsS_0To0, A , _        , _ ), OP(7631, Uzp2_4S  , BA, Zip2_2D  , CB, _        , _ ), OP(7632, Zip2_2D  , AB, _        , _ , _        , _ ), OP(7633, Dup3     , A , Zip2_2D  , CB, _        , _ ),
  OP(7700, Dup3     , B , Dup0     , A , Uzp2_4S  , DC), OP(7701, Rev64    , A , Dup3     , B , Zip1_2D  , CD), OP(7702, Ext8     , AA, Dup3     , B , Uzp1_4S  , CD), OP(7703, Ext12    , AA, Dup3     , B , Zip1_2D  , CD),
  OP(7710, Dup3     , B , Zip1_2D  , AC, _        , _ ), OP(7711, Dup3     , B , Dup1     , A , Uzp2_4S  , DC), OP(7712, Trn2_4S  , BA, InsS_2To0, A , InsS_2To3, C ), OP(7713, Ext12    , AA, Dup3     , B , Uzp1_4S  , CD),
  OP(7720, Dup3     , B , Uzp1_4S  , AC, _        , _ ), OP(7721, Ext12    , BA, Dup3     , B , Zip2_2D  , CD), OP(7722, Dup3     , B , Dup2     , A , Uzp2_4S  , DC), OP(7723, Rev64    , A , Dup3     , B , Zip2_2D  , CD),
  OP(7730, Uzp2_4S  , AB, InsS_0To0, A , InsS_3To2, C ), OP(7731, Dup3     , B , Uzp2_4S  , AC, _        , _ ), OP(7732, Dup3     , B , Zip2_2D  , AC, _        , _ ), OP(7733, Trn2_4S  , AB, Zip2_4S  , CC, _        , _ )
};

#undef OP

static void emit_interleaved_shuffle32_impl(PipeCompiler* pc, const Vec& dst, const Vec& src1, const Vec& src2, uint32_t imm) noexcept {
  BL_ASSERT((imm & 0xFCFCFCFC) == 0);

  if (src1.id() == src2.id())
    return emit_swizzle32_impl(pc, dst, src1, imm);

  AsmCompiler* cc = pc->cc;

  uint32_t tableIndex = ((imm & 0x03000000) >> (24 - 6)) |
                        ((imm & 0x00030000) >> (16 - 4)) |
                        ((imm & 0x00000300) >> (8  - 2)) | (imm & 0x00000003);

  // By default the cost matrix tries to avoid inserting A to DST to minimize the number of instructions
  // in case that `dst == a`, which is common. However, we have a also a table that penalizes inserting B
  // to DST, so select the best version.
  InterleavedShuffle32Ops ops;
  if (dst.id() != src2.id())
    ops = interleaved_shuffle32_ops[tableIndex];
  else
    ops = interleaved_shuffle32_ops_dst_same_as_b[tableIndex];

  Vec regs[5] = { src1, src2, Vec(), Vec(), Vec() };

  uint32_t count = ops.count();
  uint32_t srcUseMask[4] {};
  uint32_t dstIndex = count - 1;

  {
    uint32_t i = count;
    uint32_t allUseMask = 0;

    while (i != 0) {
      i--;
      InterleavedShuffle32Ops::Op op = ops.op(i);

      // Calculate masks that follow.
      allUseMask |= 1u << op.srcA();
      allUseMask |= 1u << op.srcB();
      srcUseMask[i] = allUseMask;

      // Calculate the last operation where we need a new destination (inserts insert to the last).
      if (op.isInsOp()) {
        // The last destination would be the previous operation (if not another insert).
        dstIndex = i - 1;
      }
    }
  }

  uint32_t opIndex = 2;
  Vec finalDst;

  for (uint32_t i = 0; i < count; i++) {
    InterleavedShuffle32Ops::Op op = ops.op(i);

    if (i > 0 && !op.isInsOp())
      opIndex++;

    // Should not be counted if it's invalid - so let's verify it here...
    BL_ASSERT(op.isValid());

    Vec opDst;
    if (i < dstIndex) {
      if (regs[opIndex].isValid())
        opDst = regs[opIndex];
      else
        opDst = pc->newSimilarReg(dst, "@shuf_tmp_%u", opIndex - 2);
    }
    else {
      if (!finalDst.isValid()) {
        uint32_t msk = 0;
        if (dst.id() == src1.id())
          msk = 0x1u;
        else if (dst.id() == src2.id())
          msk = 0x2u;

        if (srcUseMask[i + 1] & msk) {
          // In this case the destination is in conflict with one of the source registers. We have to
          // create a new virtual register and then move it to the real `dst` to not mess up the shuffle.
          BL_ASSERT(!regs[opIndex].isValid());
          finalDst = pc->newSimilarReg(dst, "@shuf_dst");
        }
        else {
          // Perfect - the destination is not in conflict with any source register.
          finalDst = dst;
        }
      }
      opDst = finalDst;
    }

    uint32_t aIndex = op.srcA();
    uint32_t bIndex = op.srcB();

    switch (op.op()) {
      case InterleavedShuffle32Ops::Op::kOpInsS: {
        uint32_t srcLane = op.insSrc();
        uint32_t dstLane = op.insDst();
        cc->mov(opDst.s(dstLane), regs[aIndex].s(srcLane));
        break;
      }

      case InterleavedShuffle32Ops::Op::kOpInsD: {
        uint32_t srcLane = op.insSrc();
        uint32_t dstLane = op.insDst();
        cc->mov(opDst.d(dstLane), regs[aIndex].d(srcLane));
        break;
      }

      case InterleavedShuffle32Ops::Op::kOpPerm: {
        static constexpr uint16_t perm_inst[8] = {
          Inst::kIdZip1_v,
          Inst::kIdZip2_v,
          Inst::kIdUzp1_v,
          Inst::kIdUzp2_v,
          Inst::kIdTrn1_v,
          Inst::kIdTrn2_v,
          Inst::kIdExt_v,
          Inst::kIdExt_v
        };

        InstId instId = perm_inst[op.permOp()];

        if (instId == Inst::kIdExt_v)
          cc->ext(opDst.b16(), regs[aIndex].b16(), regs[bIndex].b16(), op.permExtImm());
        else if (op.permS4())
          cc->emit(instId, opDst.s4(), regs[aIndex].s4(), regs[bIndex].s4());
        else
          cc->emit(instId, opDst.d2(), regs[aIndex].d2(), regs[bIndex].d2());
        break;
      }

      case InterleavedShuffle32Ops::Op::kOpRev64: {
        cc->rev64(opDst.s4(), regs[aIndex].s4());
        break;
      }

      case InterleavedShuffle32Ops::Op::kOpDup: {
        uint32_t idx = op.dupIdx();
        cc->dup(opDst.s4(), regs[aIndex].s(idx));
        break;
      }

      default:
        BL_NOT_REACHED();
    }

    regs[opIndex] = opDst;
  }

  vec_mov(pc, dst, finalDst);
}

// bl::Pipeline::PipeCompiler - Vector Instructions - OpArray Iterator
// ===================================================================

template<typename T>
class OpArrayIter {
public:
  const T& _op;

  BL_INLINE_NODEBUG OpArrayIter(const T& op) noexcept : _op(op) {}
  BL_INLINE_NODEBUG const T& op() const noexcept { return _op; }
  BL_INLINE_NODEBUG void next() noexcept {}
};

template<>
class OpArrayIter<OpArray> {
public:
  const OpArray& _opArray;
  uint32_t _i {};
  uint32_t _n {};

  BL_INLINE_NODEBUG OpArrayIter(const OpArray& opArray) noexcept : _opArray(opArray), _i(0), _n(opArray.size()) {}
  BL_INLINE_NODEBUG const Operand_& op() const noexcept { return _opArray[_i]; }
  BL_INLINE_NODEBUG void next() noexcept { if (++_i >= _n) _i = 0; }
};

template<typename Src>
static BL_INLINE void emit_2v_t(PipeCompiler* pc, OpcodeVV op, const OpArray& dst_, const Src& src_) noexcept {
  uint32_t n = dst_.size();
  OpArrayIter<Src> src(src_);

  for (uint32_t i = 0; i < n; i++) {
    pc->emit_2v(op, dst_[i], src.op());
    src.next();
  }
}

template<typename Src>
static BL_INLINE void emit_2vi_t(PipeCompiler* pc, OpcodeVVI op, const OpArray& dst_, const Src& src_, uint32_t imm) noexcept {
  uint32_t n = dst_.size();
  OpArrayIter<Src> src(src_);

  for (uint32_t i = 0; i < n; i++) {
    pc->emit_2vi(op, dst_[i], src.op(), imm);
    src.next();
  }
}

template<typename Src1, typename Src2>
static BL_INLINE void emit_3v_t(PipeCompiler* pc, OpcodeVVV op, const OpArray& dst_, const Src1& src1_, const Src2& src2_) noexcept {
  uint32_t n = dst_.size();
  OpArrayIter<Src1> src1(src1_);
  OpArrayIter<Src2> src2(src2_);

  for (uint32_t i = 0; i < n; i++) {
    pc->emit_3v(op, dst_[i], src1.op(), src2.op());
    src1.next();
    src2.next();
  }
}

template<typename Src1, typename Src2>
static BL_INLINE void emit_3vi_t(PipeCompiler* pc, OpcodeVVVI op, const OpArray& dst_, const Src1& src1_, const Src2& src2_, uint32_t imm) noexcept {
  uint32_t n = dst_.size();
  OpArrayIter<Src1> src1(src1_);
  OpArrayIter<Src2> src2(src2_);

  for (uint32_t i = 0; i < n; i++) {
    pc->emit_3vi(op, dst_[i], src1.op(), src2.op(), imm);
    src1.next();
    src2.next();
  }
}

template<typename Src1, typename Src2, typename Src3>
static BL_INLINE void emit_4v_t(PipeCompiler* pc, OpcodeVVVV op, const OpArray& dst_, const Src1& src1_, const Src2& src2_, const Src3& src3_) noexcept {
  uint32_t n = dst_.size();
  OpArrayIter<Src1> src1(src1_);
  OpArrayIter<Src2> src2(src2_);
  OpArrayIter<Src3> src3(src3_);

  for (uint32_t i = 0; i < n; i++) {
    pc->emit_4v(op, dst_[i], src1.op(), src2.op(), src3.op());
    src1.next();
    src2.next();
    src3.next();
  }
}

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 2V
// ==========================================================

void PipeCompiler::emit_2v(OpcodeVV op, const Operand_& dst_, const Operand_& src_) noexcept {
  BL_ASSERT(dst_.isVec());

  Vec dst(dst_.as<Vec>());
  OpcodeVInfo opInfo = opcodeInfo2V[size_t(op)];

  InstId instId = opInfo.instId;

  switch (op) {
    case OpcodeVV::kMov: {
      if (src_.isVec()) {
        Vec src = src_.as<Vec>();
        if (dst.size() < 16 || src.size() < 16)
          cc->mov(dst.b8(), src.b8());
        else
          cc->mov(dst.b16(), src.b16());
        return;
      }

      vec_mov(this, dst, src_);
      return;
    }

    case OpcodeVV::kMovU64: {
      dst = dst.d();
      Vec src = as_vec(this, src_, dst);

      cc->mov(dst.b8(), src.b8());
      return;
    }

    case OpcodeVV::kBroadcastU8Z:
    case OpcodeVV::kBroadcastU16Z:
    case OpcodeVV::kBroadcastU8:
    case OpcodeVV::kBroadcastU16:
    case OpcodeVV::kBroadcastU32:
    case OpcodeVV::kBroadcastF32:
    case OpcodeVV::kBroadcastU64:
    case OpcodeVV::kBroadcastF64: {
      ElementSize elementSize = ElementSize(opInfo.dstElement);
      vec_set_type(dst, elementSize);

      Operand src(src_);

      if (src.isMem()) {
        // NOTE: ld1r instruction is pretty limited - it offers only `[base]` or `[base + offset|index]@` (post-index)
        // addressing. This means that it's sometimes just better to emit a regular load folowed by dup than to actually
        // use ld1r.
        Mem& m = src.as<Mem>();
        if (!m.hasIndex() && !m.hasOffset()) {
          cc->ld1r(dst, m);
          return;
        }

        v_load_iany(dst, src.as<Mem>(), 1u << uint32_t(opInfo.srcElement), Alignment{1});
        src = dst;
      }

      if (src.isVec()) {
        Vec& srcV = src.as<Vec>();
        vec_set_type_and_index(srcV, elementSize, 0);
        cc->dup(dst, srcV);
        return;
      }
      else if (src.isGp()) {
        Gp& srcGp = src.as<Gp>();
        if (elementSize <= ElementSize::k32)
          srcGp = srcGp.r32();
        else
          srcGp = srcGp.r64();
        cc->dup(dst, srcGp);
        return;
      }

      BL_NOT_REACHED();
    }

    case OpcodeVV::kBroadcastV128_U32:
    case OpcodeVV::kBroadcastV128_U64:
    case OpcodeVV::kBroadcastV128_F32:
    case OpcodeVV::kBroadcastV128_F64:
      // 128-bit broadcast is an alias of mov as we have only 128-bit vectors...
      v_mov(dst, src_);
      return;

    case OpcodeVV::kBroadcastV256_U32:
    case OpcodeVV::kBroadcastV256_U64:
    case OpcodeVV::kBroadcastV256_F32:
    case OpcodeVV::kBroadcastV256_F64:
      // Unsupported as NEON has only 128-bit vectors.
      BL_NOT_REACHED();

    case OpcodeVV::kAbsI8:
    case OpcodeVV::kAbsI16:
    case OpcodeVV::kAbsI32:
    case OpcodeVV::kAbsI64:
    case OpcodeVV::kNotU32:
    case OpcodeVV::kNotU64:
    case OpcodeVV::kNotF32:
    case OpcodeVV::kNotF64: {
      Vec src = as_vec(this, src_, dst);

      vec_set_type(dst, opInfo.dstElement);
      vec_set_type(src, opInfo.srcElement);

      cc->emit(instId, dst, src);
      return;
    }

    case OpcodeVV::kCvtI8ToI32:
    case OpcodeVV::kCvtU8ToU32: {
      Vec src = as_vec(this, src_, dst);
      cc->emit(instId, dst.h8(), src.b8(), 0);
      cc->emit(instId, dst.s4(), dst.h4(), 0);
      return;
    }

    case OpcodeVV::kCvtI8LoToI16:
    case OpcodeVV::kCvtI8HiToI16:
    case OpcodeVV::kCvtU8LoToU16:
    case OpcodeVV::kCvtU8HiToU16:
    case OpcodeVV::kCvtI16LoToI32:
    case OpcodeVV::kCvtI16HiToI32:
    case OpcodeVV::kCvtU16LoToU32:
    case OpcodeVV::kCvtU16HiToU32:
    case OpcodeVV::kCvtI32LoToI64:
    case OpcodeVV::kCvtI32HiToI64:
    case OpcodeVV::kCvtU32LoToU64:
    case OpcodeVV::kCvtU32HiToU64: {
      vec_set_type(dst, opInfo.dstElement);

      Vec src;

      if (opInfo.srcPart == VecPart::kLo) {
        src = as_vec(this, src_, dst, 8);
        src = src.v64();
      }
      else if (src_.isVec()) {
        src = src_.as<Vec>();
      }
      else {
        Mem m(src_.as<Mem>());
        m.addOffset(8);
        src = vec_from_mem(this,  m, dst, 8);
        src = src.v64();

        // Since we have loaded from memory, we want to use the low-part variant of the instruction.
        instId = opcodeInfo2V[size_t(op) - 1u].instId;
      }

      vec_set_type(src, opInfo.srcElement);
      cc->emit(instId, dst, src, 0);
      return;
    }

    case OpcodeVV::kRcpF32:
    case OpcodeVV::kRcpF64: {
      // Intrinsic.
      const void* onePtr = op == OpcodeVV::kRcpF32 ? static_cast<const void*>(&ct.f32_1) : static_cast<const void*>(&ct.f64_1);

      Vec one = simdVecConst(onePtr, Bcst::kNA, dst);
      Vec src = as_vec(this, src_, dst);

      vec_set_type(dst, opInfo.dstElement);
      vec_set_type(one, opInfo.dstElement);
      vec_set_type(src, opInfo.dstElement);

      cc->fdiv(dst, one, src);
      return;
    }

    case OpcodeVV::kTruncF32S:
    case OpcodeVV::kFloorF32S:
    case OpcodeVV::kCeilF32S:
    case OpcodeVV::kRoundF32S:
    case OpcodeVV::kSqrtF32S: {
      dst.setSignature(a64::VecS::kSignature);
      Vec src = as_vec(this, src_, dst);

      cc->emit(instId, dst, src);
      return;
    }

    case OpcodeVV::kTruncF64S:
    case OpcodeVV::kFloorF64S:
    case OpcodeVV::kCeilF64S:
    case OpcodeVV::kRoundF64S:
    case OpcodeVV::kSqrtF64S: {
      dst.setSignature(a64::VecD::kSignature);
      Vec src = as_vec(this, src_, dst);

      cc->emit(instId, dst, src);
      return;
    }

    case OpcodeVV::kAbsF32:
    case OpcodeVV::kAbsF64:
    case OpcodeVV::kNegF32:
    case OpcodeVV::kNegF64:
    case OpcodeVV::kTruncF32:
    case OpcodeVV::kTruncF64:
    case OpcodeVV::kFloorF32:
    case OpcodeVV::kFloorF64:
    case OpcodeVV::kCeilF32:
    case OpcodeVV::kCeilF64:
    case OpcodeVV::kRoundF32:
    case OpcodeVV::kRoundF64:
    case OpcodeVV::kSqrtF32:
    case OpcodeVV::kSqrtF64:
    case OpcodeVV::kCvtI32ToF32:
    case OpcodeVV::kCvtRoundF32ToI32:
    case OpcodeVV::kCvtTruncF32ToI32: {
      Vec src = as_vec(this, src_, dst);

      vec_set_type(dst, opInfo.dstElement);
      vec_set_type(src, opInfo.srcElement);

      cc->emit(instId, dst, src);
      return;
    }

    case OpcodeVV::kCvtF32ToF64S:
    case OpcodeVV::kCvtF64ToF32S: {
      Vec src = as_vec(this, src_, dst);

      vec_set_vec_type(dst, opInfo.dstElement);
      vec_set_vec_type(src, opInfo.srcElement);

      cc->fcvt(dst, src);
      return;
    }

    case OpcodeVV::kCvtF32HiToF64:
      if (src_.isMem()) {
        Vec src = as_vec(this, src_.as<Mem>().cloneAdjusted(8), dst, 8).v64();

        vec_set_type(dst, opInfo.dstElement);
        vec_set_type(src, opInfo.srcElement);

        cc->emit(opcodeInfo2V[size_t(op) - 1u].instId, dst, src);
        return;
      }
      BL_FALLTHROUGH
    case OpcodeVV::kCvtF32LoToF64: {
      Vec src = as_vec(this, src_, dst, 8);

      if (opInfo.srcPart == VecPart::kLo)
        src = src.v64();

      vec_set_type(dst, opInfo.dstElement);
      vec_set_type(src, opInfo.srcElement);

      cc->emit(instId, dst, src);
      return;
    }

    case OpcodeVV::kCvtI32HiToF64:
      if (src_.isMem()) {
        Vec src = as_vec(this, src_.as<Mem>().cloneAdjusted(8), dst, 8).v64();

        vec_set_type(dst, opInfo.dstElement);
        vec_set_type(src, opInfo.srcElement);

        cc->emit(opcodeInfo2V[size_t(op) - 1u].instId, dst, src, 0);
        cc->scvtf(dst.d2(), dst.d2());
        return;
      }
      BL_FALLTHROUGH
    case OpcodeVV::kCvtI32LoToF64: {
      Vec src = as_vec(this, src_, dst, 8);

      if (opInfo.srcPart == VecPart::kLo)
        src = src.v64();

      vec_set_type(dst, opInfo.dstElement);
      vec_set_type(src, opInfo.srcElement);

      cc->emit(instId, dst, src, 0);
      cc->scvtf(dst.d2(), dst.d2());
      return;
    }

    case OpcodeVV::kCvtF64ToF32Lo:
    case OpcodeVV::kCvtF64ToF32Hi: {
      dst = dst.q();
      Vec src = as_vec(this, src_, dst);

      if (opInfo.dstPart == VecPart::kLo)
        dst = dst.d();

      vec_set_type(dst, opInfo.dstElement);
      vec_set_type(src, opInfo.srcElement);

      cc->emit(instId, dst, src);
      return;
    }

    case OpcodeVV::kCvtTruncF64ToI32Lo:
    case OpcodeVV::kCvtRoundF64ToI32Lo:
    case OpcodeVV::kCvtTruncF64ToI32Hi:
    case OpcodeVV::kCvtRoundF64ToI32Hi: {
      dst = dst.q();

      Vec src = as_vec(this, src_, dst);
      Vec tmp = newSimilarReg(dst, "@tmp");

      cc->emit(instId, tmp.d2(), src.d2());

      if (opInfo.dstPart == VecPart::kLo)
        cc->sqxtn(dst.s2(), tmp.d2());
      else
        cc->sqxtn2(dst.s4(), tmp.d2());

      return;
    }

    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::emit_2v(OpcodeVV op, const OpArray& dst_, const Operand_& src_) noexcept { emit_2v_t(this, op, dst_, src_); }
void PipeCompiler::emit_2v(OpcodeVV op, const OpArray& dst_, const OpArray& src_) noexcept { emit_2v_t(this, op, dst_, src_); }

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 2VI
// ===========================================================

void PipeCompiler::emit_2vi(OpcodeVVI op, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept {
  BL_ASSERT(dst_.isVec());

  Vec dst(dst_.as<Vec>());
  OpcodeVInfo opInfo = opcodeInfo2VI[size_t(op)];

  InstId instId = opInfo.instId;

  switch (op) {
    case OpcodeVVI::kSllbU128:
    case OpcodeVVI::kSrlbU128: {
      BL_ASSERT(imm < 16);

      Vec src = as_vec(this, src_, dst);

      // If the shift is used to extract a high 64-bit element and zero the rest of the register.
      if (op == OpcodeVVI::kSrlbU128 && imm == 8) {
        cc->dup(dst.d(), src.d(1));
        return;
      }

      // If the shift is used to extract the last 32-bit element and zero the rest of the register.
      if (op == OpcodeVVI::kSrlbU128 && imm == 12) {
        cc->dup(dst.s(), src.s(3));
        return;
      }

      Vec zero = simdVecZero(dst);
      vec_set_type(dst, ElementSize::k8);
      vec_set_type(src, ElementSize::k8);
      vec_set_type(zero, ElementSize::k8);

      if (op == OpcodeVVI::kSllbU128)
        cc->ext(dst, zero, src, 16u - imm);
      else
        cc->ext(dst, src, zero, imm);
      return;
    }

    case OpcodeVVI::kSwizzleU16x4:
    case OpcodeVVI::kSwizzleLoU16x4:
    case OpcodeVVI::kSwizzleHiU16x4: {
      Vec src = as_vec(this, src_, dst);

      uint8_t predData[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };

      uint32_t d = (imm >> 23) & (0x3 << 1);
      uint32_t c = (imm >> 15) & (0x3 << 1);
      uint32_t b = (imm >>  7) & (0x3 << 1);
      uint32_t a = (imm <<  1) & (0x3 << 1);

      if (op != OpcodeVVI::kSwizzleHiU16x4) {
        predData[ 0] = a;
        predData[ 1] = a + 1;
        predData[ 2] = b;
        predData[ 3] = b + 1;
        predData[ 4] = c;
        predData[ 5] = c + 1;
        predData[ 6] = d;
        predData[ 7] = d + 1;
      }

      if (op != OpcodeVVI::kSwizzleLoU16x4) {
        predData[ 8] = a + 8;
        predData[ 9] = a + 9;
        predData[10] = b + 8;
        predData[11] = b + 9;
        predData[12] = c + 8;
        predData[13] = c + 9;
        predData[14] = d + 8;
        predData[15] = d + 9;
      }

      Vec pred = simdConst16B(predData);
      cc->tbl(dst.b16(), src.b16(), pred.b16());
      return;
    }

    case OpcodeVVI::kSwizzleU32x4:
    case OpcodeVVI::kSwizzleF32x4: {
      Vec src = as_vec(this, src_, dst);
      emit_swizzle32_impl(this, dst, src, imm);
      return;
    }

    case OpcodeVVI::kSwizzleU64x2:
    case OpcodeVVI::kSwizzleF64x2: {
      Vec src = as_vec(this, src_, dst);

      // Use `dup` to broadcast one 64-bit elements.
      if (Swizzle2{imm} == swizzle(0, 0) ||
          Swizzle2{imm} == swizzle(1, 1)) {
        uint32_t idx = imm & 0x1;
        cc->dup(dst.d2(), src.d(idx));
        return;
      }

      // Use `ext` to swap two 64-bit elements.
      if (Swizzle2{imm} == swizzle(0, 1)) {
        cc->ext(dst.b16(), src.b16(), src.b16(), 8);
        return;
      }

      // NOP...
      if (Swizzle2{imm} == swizzle(1, 0)) {
        cc->mov(dst.b16(), src.b16());
        return;
      }

      BL_NOT_REACHED();
    }

    case OpcodeVVI::kSwizzleF64x4:
    case OpcodeVVI::kSwizzleU64x4:
    case OpcodeVVI::kExtractV128_I32:
    case OpcodeVVI::kExtractV128_I64:
    case OpcodeVVI::kExtractV128_F32:
    case OpcodeVVI::kExtractV128_F64:
    case OpcodeVVI::kExtractV256_I32:
    case OpcodeVVI::kExtractV256_I64:
    case OpcodeVVI::kExtractV256_F32:
    case OpcodeVVI::kExtractV256_F64:
      // Unsupported as NEON has only 128-bit vectors.
      BL_NOT_REACHED();

    default: {
      Vec src = as_vec(this, src_, dst);

      if (opInfo.dstPart == VecPart::kLo) dst = dst.d();
      if (opInfo.srcPart == VecPart::kLo) src = src.d();

      vec_set_type(dst, opInfo.dstElement);
      vec_set_type(src, opInfo.srcElement);

      cc->emit(instId, dst, src, imm);
      return;
    }
  }
}

void PipeCompiler::emit_2vi(OpcodeVVI op, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept { emit_2vi_t(this, op, dst_, src_, imm); }
void PipeCompiler::emit_2vi(OpcodeVVI op, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept { emit_2vi_t(this, op, dst_, src_, imm); }

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 2VS
// ===========================================================

void PipeCompiler::emit_2vs(OpcodeVR op, const Operand_& dst_, const Operand_& src_, uint32_t idx) noexcept {
  OpcodeVInfo opInfo = opcodeInfo2VS[size_t(op)];

  switch (op) {
    case OpcodeVR::kMov: {
      BL_ASSERT(dst_.isReg());
      BL_ASSERT(src_.isReg());

      if (dst_.isGp() && src_.isVec()) {
        if (dst_.as<Reg>().size() == 4)
          cc->mov(dst_.as<Gp>(), src_.as<Vec>().s(0));
        else
          cc->mov(dst_.as<Gp>(), src_.as<Vec>().d(0));
        return;
      }

      if (dst_.isVec() && src_.isGp()) {
        if (src_.as<Reg>().size() == 4)
          cc->fmov(dst_.as<Vec>().s(), src_.as<Gp>());
        else
          cc->fmov(dst_.as<Vec>().d(), src_.as<Gp>());
        return;
      }

      BL_NOT_REACHED();
    }

    case OpcodeVR::kMovU32:
    case OpcodeVR::kMovU64: {
      BL_ASSERT(dst_.isReg());
      BL_ASSERT(src_.isReg());

      if (dst_.isGp() && src_.isVec()) {
        if (op == OpcodeVR::kMovU32)
          cc->mov(dst_.as<Gp>().r32(), src_.as<Vec>().s(0));
        else
          cc->mov(dst_.as<Gp>().r64(), src_.as<Vec>().d(0));
        return;
      }

      if (dst_.isVec() && src_.isGp()) {
        if (op == OpcodeVR::kMovU32)
          cc->fmov(dst_.as<Vec>().s(), src_.as<Gp>().r32());
        else
          cc->fmov(dst_.as<Vec>().d(), src_.as<Gp>().r64());
        return;
      }

      BL_NOT_REACHED();
    }

    case OpcodeVR::kInsertU8:
    case OpcodeVR::kInsertU16:
    case OpcodeVR::kInsertU32:
    case OpcodeVR::kInsertU64: {
      BL_ASSERT(dst_.isVec());
      BL_ASSERT(src_.isGp());

      Vec dst(dst_.as<Vec>());
      Gp src(src_.as<Gp>());

      vec_set_type_and_index(dst, opInfo.dstElement, idx);
      src.setSignature(op == OpcodeVR::kInsertU64 ? a64::GpX::kSignature : a64::GpW::kSignature);

      cc->mov(dst, src);
      return;
    }

    case OpcodeVR::kExtractU8:
    case OpcodeVR::kExtractU16:
    case OpcodeVR::kExtractU32:
    case OpcodeVR::kExtractU64: {
      BL_ASSERT(dst_.isGp());
      BL_ASSERT(src_.isVec());

      Gp dst(dst_.as<Gp>());
      Vec src(src_.as<Vec>());

      dst.setSignature(op == OpcodeVR::kExtractU64 ? a64::GpX::kSignature : a64::GpW::kSignature);
      vec_set_type_and_index(src, opInfo.dstElement, idx);

      cc->mov(dst, src);
      return;
    }

    case OpcodeVR::kCvtIntToF32:
    case OpcodeVR::kCvtIntToF64: {
      BL_ASSERT(dst_.isVec());
      BL_ASSERT(src_.isGp());

      Vec dst(dst_.as<Vec>());
      vec_set_vec_type(dst, opInfo.dstElement);
      cc->scvtf(dst, src_.as<Gp>());
      return;
    }

    case OpcodeVR::kCvtTruncF32ToInt:
    case OpcodeVR::kCvtRoundF32ToInt:
    case OpcodeVR::kCvtTruncF64ToInt:
    case OpcodeVR::kCvtRoundF64ToInt: {
      BL_ASSERT(dst_.isGp());
      BL_ASSERT(src_.isVec());

      Vec src(src_.as<Vec>());
      vec_set_vec_type(src, opInfo.srcElement);
      cc->emit(opInfo.instId, dst_, src);
      return;
    }

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 2VM
// ===========================================================

void PipeCompiler::emit_vm(OpcodeVM op, const Vec& dst_, const Mem& src_, uint32_t alignment, uint32_t idx) noexcept {
  BL_ASSERT(dst_.isVec());
  BL_ASSERT(src_.isMem());

  blUnused(alignment);

  Vec dst(dst_);
  Mem src(src_);
  OpcodeVMInfo opInfo = opcodeInfo2VM[size_t(op)];

  switch (op) {
    case OpcodeVM::kLoad8:
    case OpcodeVM::kLoad16_U16:
    case OpcodeVM::kLoad32_U32:
    case OpcodeVM::kLoad32_F32:
    case OpcodeVM::kLoad64_U32:
    case OpcodeVM::kLoad64_U64:
    case OpcodeVM::kLoad64_F32:
    case OpcodeVM::kLoad64_F64:
    case OpcodeVM::kLoad128_U32:
    case OpcodeVM::kLoad128_U64:
    case OpcodeVM::kLoad128_F32:
    case OpcodeVM::kLoad128_F64: {
      vec_load_mem(this, dst, src, opInfo.memSize);
      return;
    }

    case OpcodeVM::kLoadN_U32:
    case OpcodeVM::kLoadN_U64:
    case OpcodeVM::kLoadN_F32:
    case OpcodeVM::kLoadN_F64: {
      vec_load_mem(this, dst.q(), src, 16);
      return;
    }

    case OpcodeVM::kLoadCvtN_U8ToU64:
    case OpcodeVM::kLoadCvt16_U8ToU64: {
      Gp tmp = newGp32("@tmp");
      cc->ldrh(tmp, src);
      cc->mov(dst.b(), tmp);
      cc->lsr(tmp, tmp, 8);
      cc->mov(dst.b(8), tmp);
      return;
    }

    case OpcodeVM::kLoadCvt32_I8ToI16:
    case OpcodeVM::kLoadCvt32_U8ToU16:
    case OpcodeVM::kLoadCvt32_I8ToI32:
    case OpcodeVM::kLoadCvt32_U8ToU32:
    case OpcodeVM::kLoadCvt32_I16ToI32:
    case OpcodeVM::kLoadCvt32_U16ToU32:
    case OpcodeVM::kLoadCvt32_I32ToI64:
    case OpcodeVM::kLoadCvt32_U32ToU64:
    case OpcodeVM::kLoadCvt64_I8ToI16:
    case OpcodeVM::kLoadCvt64_U8ToU16:
    case OpcodeVM::kLoadCvt64_I16ToI32:
    case OpcodeVM::kLoadCvt64_U16ToU32:
    case OpcodeVM::kLoadCvt64_I32ToI64:
    case OpcodeVM::kLoadCvt64_U32ToU64: {
      vec_load_mem(this, dst, src, opInfo.memSize);
      emit_2v(OpcodeVV(opInfo.cvtOp), dst, dst);
      return;
    }

    case OpcodeVM::kLoadCvtN_I8ToI16:
    case OpcodeVM::kLoadCvtN_I8ToI32:
    case OpcodeVM::kLoadCvtN_U8ToU16:
    case OpcodeVM::kLoadCvtN_U8ToU32:
    case OpcodeVM::kLoadCvtN_I16ToI32:
    case OpcodeVM::kLoadCvtN_U16ToU32:
    case OpcodeVM::kLoadCvtN_I32ToI64:
    case OpcodeVM::kLoadCvtN_U32ToU64: {
      vec_load_mem(this, dst, src, dst.size() / 2u);
      emit_2v(OpcodeVV(opInfo.cvtOp), dst, dst);
      return;
    }

    case OpcodeVM::kLoadInsertU8:
    case OpcodeVM::kLoadInsertU16:
    case OpcodeVM::kLoadInsertU32:
    case OpcodeVM::kLoadInsertF32:
    case OpcodeVM::kLoadInsertU64:
    case OpcodeVM::kLoadInsertF32x2:
    case OpcodeVM::kLoadInsertF64: {
      if (!src.hasIndex() && !src.hasOffset()) {
        vec_set_type_and_index(dst, opInfo.element, idx);
        cc->ld1(dst, src);
      }
      else {
        Vec tmp = newSimilarReg(dst, "@tmp");
        v_load_iany(tmp, src, opInfo.memSize, Alignment{1});

        vec_set_type_and_index(dst, opInfo.element, idx);
        vec_set_type_and_index(tmp, opInfo.element, 0);
        cc->mov(dst, tmp);
      }
      return;
    }

    case OpcodeVM::kLoad256_U32:
    case OpcodeVM::kLoad256_U64:
    case OpcodeVM::kLoad256_F32:
    case OpcodeVM::kLoad256_F64:
    case OpcodeVM::kLoad512_U32:
    case OpcodeVM::kLoad512_U64:
    case OpcodeVM::kLoad512_F32:
    case OpcodeVM::kLoad512_F64:
    case OpcodeVM::kLoadCvt32_U8ToU64:
    case OpcodeVM::kLoadCvt64_U8ToU64:
    case OpcodeVM::kLoadCvt64_I8ToI32:
    case OpcodeVM::kLoadCvt64_U8ToU32:
    case OpcodeVM::kLoadCvt128_I8ToI32:
    case OpcodeVM::kLoadCvt128_U8ToU32:
    case OpcodeVM::kLoadCvt128_I8ToI16:
    case OpcodeVM::kLoadCvt128_U8ToU16:
    case OpcodeVM::kLoadCvt128_I16ToI32:
    case OpcodeVM::kLoadCvt128_U16ToU32:
    case OpcodeVM::kLoadCvt128_I32ToI64:
    case OpcodeVM::kLoadCvt128_U32ToU64:
    case OpcodeVM::kLoadCvt256_I8ToI16:
    case OpcodeVM::kLoadCvt256_U8ToU16:
    case OpcodeVM::kLoadCvt256_I16ToI32:
    case OpcodeVM::kLoadCvt256_U16ToU32:
    case OpcodeVM::kLoadCvt256_I32ToI64:
    case OpcodeVM::kLoadCvt256_U32ToU64:
      // Unsupported as NEON has only 128-bit vectors.
      BL_NOT_REACHED();

    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::emit_vm(OpcodeVM op, const OpArray& dst_, const Mem& src_, uint32_t alignment, uint32_t idx) noexcept {
  blUnused(alignment);

  uint32_t i = 0;
  uint32_t n = dst_.size();

  if (!n)
    return;

  Mem src(src_);
  OpcodeVMInfo opInfo = opcodeInfo2VM[size_t(op)];

  uint32_t overriddenMemSize = opInfo.memSize;
  uint32_t memSize = overriddenMemSize ? overriddenMemSize : dst_[0].as<Vec>().size();

  if (op <= OpcodeVM::kLoadN_F64 && !src.hasIndex() && !src.hasOffset() && memSize >= 4) {
    while (i + 2 <= n) {
      const Vec& dst0 = dst_[i + 0].as<Vec>();
      const Vec& dst1 = dst_[i + 1].as<Vec>();

      if (memSize == 4)
        cc->ldp(dst0.s(), dst1.s(), src);
      else if (memSize == 8)
        cc->ldp(dst0.d(), dst1.d(), src);
      else
        cc->ldp(dst0.q(), dst1.q(), src);

      src.addOffsetLo32(int32_t(memSize * 2));

      i += 2;
    }
  }

  while (i < n) {
    BL_ASSERT(dst_[i].isVec());

    const Vec& dst = dst_[i].as<Vec>();
    memSize = dst.size();

    emit_vm(op, dst, src, 1, idx);

    src.addOffsetLo32(int32_t(memSize));
    i++;
  }
}

void PipeCompiler::emit_mv(OpcodeMV op, const Mem& dst_, const Vec& src_, uint32_t alignment, uint32_t idx) noexcept {
  BL_ASSERT(dst_.isMem());
  BL_ASSERT(src_.isVec());

  blUnused(alignment);

  Mem dst(dst_);
  Vec src(src_);

  switch (op) {
    case OpcodeMV::kStore8: {
      cc->str(src.b(), dst);
      return;
    }

    case OpcodeMV::kStore16_U16: {
      cc->str(src.h(), dst);
      return;
    }

    case OpcodeMV::kStore32_U32:
    case OpcodeMV::kStore32_F32: {
      cc->str(src.s(), dst);
      return;
    }

    case OpcodeMV::kStore64_U32:
    case OpcodeMV::kStore64_U64:
    case OpcodeMV::kStore64_F32:
    case OpcodeMV::kStore64_F64: {
      cc->str(src.d(), dst);
      return;
    }

    case OpcodeMV::kStore128_U32:
    case OpcodeMV::kStore128_U64:
    case OpcodeMV::kStore128_F32:
    case OpcodeMV::kStore128_F64: {
      cc->str(src.q(), dst);
      return;
    }

    case OpcodeMV::kStoreN_U32:
    case OpcodeMV::kStoreN_U64:
    case OpcodeMV::kStoreN_F32:
    case OpcodeMV::kStoreN_F64: {
      cc->str(src, dst);
      return;
    }

    /*
    case OpcodeMV::kStoreCvtz64_U16ToU8:
    case OpcodeMV::kStoreCvtz64_U32ToU16:
    case OpcodeMV::kStoreCvtz64_U64ToU32:
    case OpcodeMV::kStoreCvts64_I16ToI8:
    case OpcodeMV::kStoreCvts64_I16ToU8:
    case OpcodeMV::kStoreCvts64_U16ToU8:
    case OpcodeMV::kStoreCvts64_I32ToI16:
    case OpcodeMV::kStoreCvts64_U32ToU16:
    case OpcodeMV::kStoreCvts64_I64ToI32:
    case OpcodeMV::kStoreCvts64_U64ToU32:
    case OpcodeMV::kStoreCvtzN_U16ToU8:
    case OpcodeMV::kStoreCvtzN_U32ToU16:
    case OpcodeMV::kStoreCvtzN_U64ToU32:
    case OpcodeMV::kStoreCvtsN_I16ToI8:
    case OpcodeMV::kStoreCvtsN_I16ToU8:
    case OpcodeMV::kStoreCvtsN_U16ToU8:
    case OpcodeMV::kStoreCvtsN_I32ToI16:
    case OpcodeMV::kStoreCvtsN_U32ToU16:
    case OpcodeMV::kStoreCvtsN_I64ToI32:
    case OpcodeMV::kStoreCvtsN_U64ToU32:
      UNIMPLEMENTED();
      return;
    */

    case OpcodeMV::kStore256_U32:
    case OpcodeMV::kStore256_U64:
    case OpcodeMV::kStore256_F32:
    case OpcodeMV::kStore256_F64:
    case OpcodeMV::kStore512_U32:
    case OpcodeMV::kStore512_U64:
    case OpcodeMV::kStore512_F32:
    case OpcodeMV::kStore512_F64:
    /*
    case OpcodeMV::kStoreCvtz128_U16ToU8:
    case OpcodeMV::kStoreCvtz128_U32ToU16:
    case OpcodeMV::kStoreCvtz128_U64ToU32:
    case OpcodeMV::kStoreCvts128_I16ToI8:
    case OpcodeMV::kStoreCvts128_I16ToU8:
    case OpcodeMV::kStoreCvts128_U16ToU8:
    case OpcodeMV::kStoreCvts128_I32ToI16:
    case OpcodeMV::kStoreCvts128_U32ToU16:
    case OpcodeMV::kStoreCvts128_I64ToI32:
    case OpcodeMV::kStoreCvts128_U64ToU32:
    case OpcodeMV::kStoreCvtz256_U16ToU8:
    case OpcodeMV::kStoreCvtz256_U32ToU16:
    case OpcodeMV::kStoreCvtz256_U64ToU32:
    case OpcodeMV::kStoreCvts256_I16ToI8:
    case OpcodeMV::kStoreCvts256_I16ToU8:
    case OpcodeMV::kStoreCvts256_U16ToU8:
    case OpcodeMV::kStoreCvts256_I32ToI16:
    case OpcodeMV::kStoreCvts256_U32ToU16:
    case OpcodeMV::kStoreCvts256_I64ToI32:
    case OpcodeMV::kStoreCvts256_U64ToU32:
    */
      // Unsupported as NEON has only 128-bit vectors.
      BL_NOT_REACHED();

    case OpcodeMV::kStoreExtractU16: {
      if (idx == 0) {
        cc->str(src.h(), dst);
      }
      else {
        cc->st1(src.h(idx), dst);
      }
      return;
    }

    case OpcodeMV::kStoreExtractU32: {
      if (idx == 0) {
        cc->str(src.s(), dst);
      }
      else {
        cc->st1(src.s(idx), dst);
      }
      return;
    }

    case OpcodeMV::kStoreExtractU64: {
      if (idx == 0) {
        cc->str(src.d(), dst);
      }
      else {
        cc->st1(src.d(idx), dst);
      }
      return;
    }

    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::emit_mv(OpcodeMV op, const Mem& dst_, const OpArray& src_, uint32_t alignment, uint32_t idx) noexcept {
  blUnused(alignment);

  uint32_t i = 0;
  uint32_t n = src_.size();

  if (!n)
    return;

  Mem dst(dst_);
  OpcodeVMInfo opInfo = opcodeInfo2MV[size_t(op)];

  uint32_t overriddenMemSize = opInfo.memSize;
  uint32_t memSize = overriddenMemSize ? overriddenMemSize : src_[0].as<Vec>().size();

  if (op <= OpcodeMV::kStoreN_F64 && memSize >= 4) {
    bool goodOffset = (dst.offsetLo32() & (memSize - 1)) == 0;

    if (goodOffset) {
      while (i + 2 <= n) {
        const Vec& src0 = src_[i + 0].as<Vec>();
        const Vec& src1 = src_[i + 1].as<Vec>();

        if (memSize == 4)
          cc->stp(src0.s(), src1.s(), dst);
        else if (memSize == 8)
          cc->stp(src0.d(), src1.d(), dst);
        else
          cc->stp(src0.q(), src1.q(), dst);

        dst.addOffsetLo32(int32_t(memSize * 2));

        i += 2;
      }
    }
  }

  while (i < n) {
    BL_ASSERT(src_[i].isVec());

    const Vec& src = src_[i].as<Vec>();
    emit_mv(op, dst, src, 1, idx);

    dst.addOffsetLo32(int32_t(memSize));
    i++;
  }}

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 3V
// ==========================================================

static void emit_3v_op(
  PipeCompiler* pc,
  InstId instId,
  Vec dst, Vec src1, Operand_ src2_,
  FloatMode floatMode,
  ElementSize dstElement, VecPart dstPart,
  ElementSize srcElement, VecPart srcPart,
  uint32_t reversed) noexcept {

  Vec src2;

  switch (floatMode) {
    case FloatMode::kF32S: {
      dst = dst.s();
      src1 = src1.s();
      src2 = as_vec(pc, src2_, dst, 4);
      break;
    }

    case FloatMode::kF64S: {
      dst = dst.d();
      src1 = src1.d();
      src2 = as_vec(pc, src2_, dst, 8);
      break;
    }

    default: {
      src2 = as_vec(pc, src2_, dst);

      if (dstPart == VecPart::kLo) {
        dst = dst.d();
      }

      if (srcPart == VecPart::kLo) {
        src1 = src1.d();
        src2 = src2.d();
      }

      vec_set_type(dst, dstElement);
      vec_set_type(src1, srcElement);
      vec_set_type(src2, srcElement);
      break;
    }
  }

  AsmCompiler* cc = pc->cc;
  if (reversed)
    cc->emit(instId, dst, src2, src1);
  else
    cc->emit(instId, dst, src1, src2);
}

void PipeCompiler::emit_3v(OpcodeVVV op, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_) noexcept {
  BL_ASSERT(dst_.isVec());
  BL_ASSERT(src1_.isVec());

  Vec dst(dst_.as<Vec>());
  Vec src1(src1_.as<Vec>().cloneAs(dst));
  OpcodeVInfo opInfo = opcodeInfo3V[size_t(op)];

  InstId instId = opInfo.instId;

  if (isSameVec(src1, src2_)) {
    switch (opInfo.sameVecOp) {
      case SameVecOp::kZero: {
        cc->movi(dst.b16(), 0);
        return;
      }

      case SameVecOp::kOnes: {
        cc->movi(dst.b16(), 0xFF);
        return;
      }

      case SameVecOp::kSrc: {
        vec_mov(this, dst, src1);
        return;
      }

      default:
        break;
    }
  }

  switch (op) {
    case OpcodeVVV::kMulU64: {
      Vec src2 = as_vec(this, src2_, dst);
      Vec tmp1 = newSimilarReg(dst, "@tmp1");
      Vec tmp2 = newSimilarReg(dst, "@tmp2");
      Vec tmp3 = newSimilarReg(dst, "@tmp3");

      cc->rev64(tmp1.s4(), src1.s4());
      cc->xtn(tmp2.s2(), src1.d2());
      cc->mul(tmp1.s4(), tmp1.s4(), src2.s4());
      cc->xtn(tmp3.s2(), src2.d2());
      cc->uaddlp(tmp1.d2(), tmp1.s4());
      cc->shl(dst.d2(), tmp1.d2(), 32);
      cc->umlal(dst.d2(), tmp2.s2(), tmp3.s2());

      return;
    }

    case OpcodeVVV::kMulhI16: {
      Vec src2 = as_vec(this, src2_, dst);
      Vec tmp = newSimilarReg(dst, "@tmp");

      cc->smull(tmp.s4(), src1.h4(), src2.h4());
      cc->smull2(dst.s4(), src1.h8(), src2.h8());
      cc->uzp2(dst.h8(), tmp.h8(), dst.h8());
      return;
    }

    case OpcodeVVV::kMulhU16: {
      Vec src2 = as_vec(this, src2_, dst);
      Vec tmp = newSimilarReg(dst, "@tmp");

      cc->umull(tmp.s4(), src1.h4(), src2.h4());
      cc->umull2(dst.s4(), src1.h8(), src2.h8());
      cc->uzp2(dst.h8(), tmp.h8(), dst.h8());
      return;
    }

    case OpcodeVVV::kMulU64_LoU32: {
      Vec src2 = as_vec(this, src2_, dst);
      Vec tmp1 = newSimilarReg(dst, "@tmp1");
      Vec tmp2 = newSimilarReg(dst, "@tmp2");
      Vec tmp3 = dst;

      if (dst.id() == src1.id() || dst.id() == src2.id())
        tmp3 = newSimilarReg(dst, "@tmp3");

      cc->xtn(tmp1.s2(), src1.d2());
      cc->shl(tmp3.d2(), src2.d2(), 32);
      cc->xtn(tmp2.s2(), src2.d2());
      cc->mul(dst.s4(), tmp3.s4(), src1.s4());
      cc->umlal(dst.d2(), tmp1.s2(), tmp2.s2());

      return;
    }

    case OpcodeVVV::kMHAddI16_I32: {
      Vec src2 = as_vec(this, src2_, dst);

      Vec al = newSimilarReg(dst, "@al");
      Vec ah = newSimilarReg(dst, "@ah");
      Vec bl = newSimilarReg(dst, "@bl");
      Vec bh = newSimilarReg(dst, "@bh");

      cc->xtn(al.h4(), src1.s4());
      cc->xtn(bl.h4(), src2.s4());

      cc->shrn(ah.h4(), src1.s4(), 16);
      cc->shrn(bh.h4(), src2.s4(), 16);

      cc->smull(dst.s4(), al.h4(), bl.h4());
      cc->smlal(dst.s4(), ah.h4(), bh.h4());

      return;
    }

    case OpcodeVVV::kMinI64:
    case OpcodeVVV::kMinU64:
    case OpcodeVVV::kMaxI64:
    case OpcodeVVV::kMaxU64: {
      Vec src2 = as_vec(this, src2_, dst);

      // Min/Max is commutative, so let's make dst only overlap src1.
      if (dst.id() == src2.id()) {
        BLInternal::swap(src1, src2);
      }

      bool dstOverlapsSrc = dst.id() == src1.id();

      Vec tmp = dst;
      if (dstOverlapsSrc) {
        tmp = newSimilarReg(dst, "@tmp");
      }

      // Let's emit a nicer sequence for u64 maximum.
      if (op == OpcodeVVV::kMaxU64) {
        cc->uqsub(tmp.d2(), src1.d2(), src2.d2());
        cc->add(dst.d2(), tmp.d2(), src2.d2());
        return;
      }

      cc->emit(instId, tmp.d2(), src1.d2(), src2.d2());

      if (opInfo.imm)
        v_blendv_u8(dst, src2, src1, tmp);
      else
        v_blendv_u8(dst, src1, src2, tmp);

      return;
    }

    case OpcodeVVV::kCmpNeF32S:
    case OpcodeVVV::kCmpNeF64S:
    case OpcodeVVV::kCmpNeF32:
    case OpcodeVVV::kCmpNeF64: {
      emit_3v_op(this, instId, dst, src1, src2_, opInfo.floatMode, opInfo.dstElement, opInfo.dstPart, opInfo.srcElement, opInfo.srcPart, 0);
      vec_neg(this, dst, dst, opInfo.floatMode);
      return;
    }

    case OpcodeVVV::kCmpOrdF32S:
    case OpcodeVVV::kCmpOrdF64S:
    case OpcodeVVV::kCmpOrdF32:
    case OpcodeVVV::kCmpOrdF64:
    case OpcodeVVV::kCmpUnordF32S:
    case OpcodeVVV::kCmpUnordF64S:
    case OpcodeVVV::kCmpUnordF32:
    case OpcodeVVV::kCmpUnordF64: {
      if (isSameVec(src1, src2_)) {
        emit_3v_op(this, Inst::kIdFcmeq_v, dst, src1, src1, opInfo.floatMode, opInfo.dstElement, opInfo.dstPart, opInfo.srcElement, opInfo.srcPart, 0);
      }
      else {
        // This takes advantage of the following:
        //
        // When FPCR.AH is 0, the behavior is as follows (ASSUMED):
        //
        //   - Negative zero compares less than positive zero.
        //   - When FPCR.DN is 0, if either element is a NaN, the result is a quiet NaN.
        //   - When FPCR.DN is 1, if either element is a NaN, the result is Default NaN.
        //
        // When FPCR.AH is 1, the behavior is as follows (USED FOR X86 EMULATION - NOT ASSUMED):
        //
        //   - If both elements are zeros, regardless of the sign of either zero, the result is the second element.
        //   - If either element is a NaN, regardless of the value of FPCR.DN, the result is the second element.
        Vec src2 = as_vec(this, src2_, dst, opInfo.floatMode);
        emit_3v_op(this, Inst::kIdFmin_v, dst, src1, src2, opInfo.floatMode, opInfo.dstElement, opInfo.dstPart, opInfo.srcElement, opInfo.srcPart, 0);
        emit_3v_op(this, Inst::kIdFcmeq_v, dst, dst, dst, opInfo.floatMode, opInfo.dstElement, opInfo.dstPart, opInfo.srcElement, opInfo.srcPart, 0);
      }

      if (opInfo.imm)
        vec_neg(this, dst, dst, opInfo.floatMode);

      return;
    }

    case OpcodeVVV::kHAddF64: {
      Vec tmp = newSimilarReg(dst, "@tmp");
      Vec src2 = as_vec(this, src2_, dst);

      if (src1.id() == src2.id()) {
        cc->ext(tmp.b16(), src1.b16(), src1.b16(), 8);
        cc->fadd(dst.d2(), src1.d2(), tmp.d2());
      }
      else {
        cc->zip1(tmp.d2(), src1.d2(), src2.d2());
        cc->zip2(dst.d2(), src1.d2(), src2.d2());
        cc->fadd(dst.d2(), dst.d2(), tmp.d2());
      }
      return;
    }

    case OpcodeVVV::kCombineLoHiU64:
    case OpcodeVVV::kCombineLoHiF64: {
      // Intrinsic - dst = {src1.u64[0], src2.64[1]} - combining low part of src1 and high part of src1.
      Vec src2 = as_vec(this, src2_, dst);

      vec_set_type(dst, ElementSize::k8);
      vec_set_type(src1, ElementSize::k8);
      vec_set_type(src2, ElementSize::k8);

      // `EXT dst, a, b, #n` -> `dst = b:a >> #n*8`
      cc->ext(dst, src2, src1, 8);
      return;
    }

    case OpcodeVVV::kCombineHiLoU64:
    case OpcodeVVV::kCombineHiLoF64: {
      // Intrinsic - dst = {src1.u64[1], src2.64[0]} - combining high part of src1 and low part of src2.
      Vec src2 = as_vec(this, src2_, dst);

      if (isSameVec(dst, src1)) {
        if (isSameVec(dst, src2))
          return;
        cc->mov(dst.d(0), src2.d(0));
      }
      else if (isSameVec(dst, src2)) {
        cc->mov(dst.d(1), src1.d(1));
      }
      else {
        cc->mov(dst.b16(), src1.b16());
        cc->mov(dst.d(0), src2.d(0));
      }

      return;
    }

    case OpcodeVVV::kPacksI16_I8:
    case OpcodeVVV::kPacksI16_U8:
    case OpcodeVVV::kPacksI32_I16:
    case OpcodeVVV::kPacksI32_U16: {
      static constexpr uint16_t pack_lo_inst[4] = { Inst::kIdSqxtn_v , Inst::kIdSqxtun_v , Inst::kIdSqxtn_v , Inst::kIdSqxtun_v  };
      static constexpr uint16_t pack_hi_inst[4] = { Inst::kIdSqxtn2_v, Inst::kIdSqxtun2_v, Inst::kIdSqxtn2_v, Inst::kIdSqxtun2_v };

      size_t id = size_t(op) - size_t(OpcodeVVV::kPacksI16_I8);

      Vec src2 = as_vec(this, src2_, dst);
      vec_set_type(src1, opInfo.srcElement);
      vec_set_type(src2, opInfo.srcElement);

      if (src1.id() == src2.id()) {
        Vec dstD = dst.d();

        vec_set_type(dst, opInfo.dstElement);
        vec_set_type(dstD, opInfo.dstElement);

        cc->emit(pack_lo_inst[id], dstD, src1);
        cc->mov(dst.d(1), dst.d(0));
      }
      else {
        Vec tmp = dst;
        if (dst.id() == src1.id() || dst.id() == src2.id()) {
          tmp = newSimilarReg(dst, "@tmp");
        }

        a64::VecD tmpD = tmp.d();

        vec_set_type(tmp, opInfo.dstElement);
        vec_set_type(tmpD, opInfo.dstElement);

        cc->emit(pack_lo_inst[id], tmpD, src1);
        cc->emit(pack_hi_inst[id], tmp, src2);

        if (dst.id() != tmp.id()) {
          cc->mov(dst.b16(), tmp.b16());
        }
      }
      return;
    }

    default: {
      emit_3v_op(this, instId, dst, src1, src2_, opInfo.floatMode, opInfo.dstElement, opInfo.dstPart, opInfo.srcElement, opInfo.srcPart, opInfo.reverse);
      return;
    }
  }
}

void PipeCompiler::emit_3v(OpcodeVVV op, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_) noexcept { emit_3v_t(this, op, dst_, src1_, src2_); }
void PipeCompiler::emit_3v(OpcodeVVV op, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_) noexcept { emit_3v_t(this, op, dst_, src1_, src2_); }
void PipeCompiler::emit_3v(OpcodeVVV op, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_) noexcept { emit_3v_t(this, op, dst_, src1_, src2_); }

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 3VI
// ===========================================================

void PipeCompiler::emit_3vi(OpcodeVVVI op, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, uint32_t imm) noexcept {
  BL_ASSERT(dst_.isVec());
  BL_ASSERT(src1_.isVec());

  Vec dst(dst_.as<Vec>());
  Vec src1(src1_.as<Vec>().cloneAs(dst));

  // Not used at the moment - maybe the info is not needed in this case.
  OpcodeVInfo opInfo = opcodeInfo3VI[size_t(op)];
  blUnused(opInfo);

  switch (op) {
    case OpcodeVVVI::kAlignr_U128: {
      BL_ASSERT(imm < 16);

      if (imm == 0) {
        vec_mov(this, dst, src2_);
        return;
      }

      Vec src2 = as_vec(this, src2_, dst);
      vec_set_type(dst, ElementSize::k8);
      vec_set_type(src1, ElementSize::k8);
      vec_set_type(src2, ElementSize::k8);
      cc->ext(dst, src2, src1, imm);
      return;
    }

    case OpcodeVVVI::kInterleaveShuffleU32x4:
    case OpcodeVVVI::kInterleaveShuffleF32x4: {
      BL_ASSERT((imm & 0xFCFCFCFC) == 0);

      Vec src2 = as_vec(this, src2_, dst);
      emit_interleaved_shuffle32_impl(this, dst, src1, src2, imm);
      return;
    }

    case OpcodeVVVI::kInterleaveShuffleU64x2:
    case OpcodeVVVI::kInterleaveShuffleF64x2: {
      BL_ASSERT((imm & 0xFFFCFEFE) == 0);

      Vec src2 = as_vec(this, src2_, dst);

      if (src1.id() == src2.id()) {
        v_swizzle_u64x2(dst, src1, Swizzle2{imm});
        return;
      }

      if (Swizzle2{imm} == swizzle(0, 0))
        cc->zip1(dst.d2(), src1.d2(), src2.d2());
      else if (Swizzle2{imm} == swizzle(1, 1))
        cc->zip2(dst.d2(), src1.d2(), src2.d2());
      else if (Swizzle2{imm} == swizzle(1, 0))
        v_combine_hi_lo_u64(dst, src2, src1);
      else
        v_combine_lo_hi_u64(dst, src2, src1);

      return;
    }

    case OpcodeVVVI::kInsertV128_U32:
    case OpcodeVVVI::kInsertV128_F32:
    case OpcodeVVVI::kInsertV128_U64:
    case OpcodeVVVI::kInsertV128_F64:
    case OpcodeVVVI::kInsertV256_U32:
    case OpcodeVVVI::kInsertV256_F32:
    case OpcodeVVVI::kInsertV256_U64:
    case OpcodeVVVI::kInsertV256_F64:
      // Unsupported as NEON has only 128-bit vectors.
      BL_NOT_REACHED();

    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::emit_3vi(OpcodeVVVI op, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_, uint32_t imm) noexcept { emit_3vi_t(this, op, dst_, src1_, src2_, imm); }
void PipeCompiler::emit_3vi(OpcodeVVVI op, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_, uint32_t imm) noexcept { emit_3vi_t(this, op, dst_, src1_, src2_, imm); }
void PipeCompiler::emit_3vi(OpcodeVVVI op, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, uint32_t imm) noexcept { emit_3vi_t(this, op, dst_, src1_, src2_, imm); }

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 4V
// ==========================================================

void PipeCompiler::emit_4v(OpcodeVVVV op, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, const Operand_& src3_) noexcept {
  BL_ASSERT(dst_.isVec());
  BL_ASSERT(src1_.isVec());

  Vec dst(dst_.as<Vec>());
  Vec src1(src1_.as<Vec>().cloneAs(dst));
  Operand src2(src2_);
  Operand src3(src3_);
  OpcodeVInfo opInfo = opcodeInfo4V[size_t(op)];

  InstId instId = opInfo.instId;

  switch (op) {
    case OpcodeVVVV::kBlendV_U8: {
      Vec src2 = as_vec(this, src2_, dst);
      Vec src3 = as_vec(this, src3_, dst);

      vec_set_type(dst, opInfo.dstElement);
      vec_set_type(src1, opInfo.srcElement);
      vec_set_type(src2, opInfo.srcElement);
      vec_set_type(src3, opInfo.srcElement);

      // We can pick between these depending on register arrangement:
      //   - BSL (bitwise select)
      //   - BIT (bitwise insert if true)
      //   - BIF (bitwise insert if false)

      if (dst.id() == src1.id()) {
        cc->bit(dst, src2, src3);
        return;
      }

      if (dst.id() == src2.id()) {
        cc->bif(dst, src1, src3);
        return;
      }

      vec_mov(this, dst, src3);
      cc->bsl(dst, src2, src1);
      return;
    }

    case OpcodeVVVV::kMAddF32S:
    case OpcodeVVVV::kMAddF64S:
    case OpcodeVVVV::kMSubF32S:
    case OpcodeVVVV::kMSubF64S:
    case OpcodeVVVV::kNMAddF32S:
    case OpcodeVVVV::kNMAddF64S:
    case OpcodeVVVV::kNMSubF32S:
    case OpcodeVVVV::kNMSubF64S: {
      Vec src2;
      Vec src3;

      if (opInfo.floatMode == FloatMode::kF32S) {
        dst = dst.s();
        src1 = src1.s();
        src2 = as_vec(this, src2_, dst, 4);
        src3 = as_vec(this, src3_, dst, 4);
      }
      else {
        dst = dst.d();
        src1 = src1.d();
        src2 = as_vec(this, src2_, dst, 8);
        src3 = as_vec(this, src3_, dst, 8);
      }

      cc->emit(instId, dst, src1, src2, src3);
      return;
    }

    case OpcodeVVVV::kMAddU16:
    case OpcodeVVVV::kMAddU32:
    case OpcodeVVVV::kMAddF32:
    case OpcodeVVVV::kMAddF64:
    case OpcodeVVVV::kMSubF32:
    case OpcodeVVVV::kMSubF64:
    case OpcodeVVVV::kNMAddF32:
    case OpcodeVVVV::kNMAddF64:
    case OpcodeVVVV::kNMSubF32:
    case OpcodeVVVV::kNMSubF64: {
      Vec src2 = as_vec(this, src2_, dst);
      Vec src3;

      bool negateAcc = opInfo.imm != 0;
      bool dstOverlaps = dst.id() == src1.id() || dst.id() == src2.id();
      bool destructible = isSameVec(dst, src3_) || !src3_.isReg();

      if (!dstOverlaps && src3_.isMem()) {
        vec_load_mem(this, dst, src3_.as<Mem>(), dst.size());
        src3 = dst;
      }
      else {
        src3 = as_vec(this, src3_, dst);
      }

      vec_set_type(dst, opInfo.dstElement);
      vec_set_type(src1, opInfo.srcElement);
      vec_set_type(src2, opInfo.srcElement);
      vec_set_type(src3, opInfo.srcElement);

      if (destructible) {
        if (negateAcc)
          cc->fneg(src3, src3);

        cc->emit(instId, src3, src1, src2);

        if (dst.id() != src3.id())
          cc->mov(dst, src3);
        return;
      }

      Vec tmp = dst;

      if (dstOverlaps) {
        tmp = newSimilarReg(dst, "@tmp");
        vec_set_type(tmp, opInfo.dstElement);
      }

      if (negateAcc)
        cc->fneg(tmp, src3);
      else
        cc->mov(tmp, src3);

      cc->emit(instId, tmp, src1, src2);

      if (dst.id() != tmp.id())
        cc->mov(dst, tmp);
      return;
    }

    default: {
      BL_NOT_REACHED();
    }
  }
}

void PipeCompiler::emit_4v(OpcodeVVVV op, const OpArray& dst_, const Operand_& src1_, const Operand_& src2_, const OpArray& src3_) noexcept { emit_4v_t(this, op, dst_, src1_, src2_, src3_); }
void PipeCompiler::emit_4v(OpcodeVVVV op, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_, const Operand& src3_) noexcept { emit_4v_t(this, op, dst_, src1_, src2_, src3_); }
void PipeCompiler::emit_4v(OpcodeVVVV op, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_, const OpArray& src3_) noexcept { emit_4v_t(this, op, dst_, src1_, src2_, src3_); }
void PipeCompiler::emit_4v(OpcodeVVVV op, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_, const Operand& src3_) noexcept { emit_4v_t(this, op, dst_, src1_, src2_, src3_); }
void PipeCompiler::emit_4v(OpcodeVVVV op, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_, const OpArray& src3_) noexcept { emit_4v_t(this, op, dst_, src1_, src2_, src3_); }
void PipeCompiler::emit_4v(OpcodeVVVV op, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, const Operand& src3_) noexcept { emit_4v_t(this, op, dst_, src1_, src2_, src3_); }
void PipeCompiler::emit_4v(OpcodeVVVV op, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, const OpArray& src3_) noexcept { emit_4v_t(this, op, dst_, src1_, src2_, src3_); }

} // {JIT}
} // {Pipeline}
} // {bl}

#endif
