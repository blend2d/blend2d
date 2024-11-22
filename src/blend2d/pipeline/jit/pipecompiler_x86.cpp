// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if defined(BL_JIT_ARCH_X86)

#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipepart_p.h"
#include "../../support/intops_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

using GPExt = PipeCompiler::GPExt;
using SSEExt = PipeCompiler::SSEExt;
using AVXExt = PipeCompiler::AVXExt;
namespace Inst { using namespace x86::Inst; }

// bl::Pipeline::PipeCompiler - Constants
// ======================================

static constexpr OperandSignature signatureOfXmmYmmZmm[] = {
  OperandSignature{x86::Xmm::kSignature},
  OperandSignature{x86::Ymm::kSignature},
  OperandSignature{x86::Zmm::kSignature}
};

static BL_INLINE RegType simdRegTypeFromWidth(VecWidth vw) noexcept {
  return RegType(uint32_t(RegType::kX86_Xmm) + uint32_t(vw));
}

// bl::Pipeline::PipeCompiler - Construction & Destruction
// =======================================================

PipeCompiler::PipeCompiler(AsmCompiler* cc, const CpuFeatures& features, PipeOptFlags optFlags) noexcept
  : cc(cc),
    ct(commonTable),
    _features(features),
    _optFlags(optFlags),
    _vecRegCount(16),
    _commonTableOff(512 + 128) {

  _scalarOpBehavior = ScalarOpBehavior::kPreservingVec128;
  _fMinMaxOpBehavior = FMinMaxOpBehavior::kTernaryLogic;
  _fMulAddOpBehavior = FMulAddOpBehavior::kNoFMA; // Will be changed by _initExtensions() if supported.

  _initExtensions(features);
}

PipeCompiler::~PipeCompiler() noexcept {}

// bl::Pipeline::PipeCompiler - CPU Architecture, Features and Optimization Options
// ================================================================================

void PipeCompiler::_initExtensions(const asmjit::CpuFeatures& features) noexcept {
  uint32_t gpExtMask = 0;
  uint32_t sseExtMask = 0;
  uint64_t avxExtMask = 0;

  if (features.x86().hasADX()) gpExtMask |= 1u << uint32_t(GPExt::kADX);
  if (features.x86().hasBMI()) gpExtMask |= 1u << uint32_t(GPExt::kBMI);
  if (features.x86().hasBMI2()) gpExtMask |= 1u << uint32_t(GPExt::kBMI2);
  if (features.x86().hasLZCNT()) gpExtMask |= 1u << uint32_t(GPExt::kLZCNT);
  if (features.x86().hasMOVBE()) gpExtMask |= 1u << uint32_t(GPExt::kMOVBE);
  if (features.x86().hasPOPCNT()) gpExtMask |= 1u << uint32_t(GPExt::kPOPCNT);

  sseExtMask |= 1u << uint32_t(SSEExt::kSSE2);
  if (features.x86().hasSSE3()) sseExtMask |= 1u << uint32_t(SSEExt::kSSE3);
  if (features.x86().hasSSSE3()) sseExtMask |= 1u << uint32_t(SSEExt::kSSSE3);
  if (features.x86().hasSSE4_1()) sseExtMask |= 1u << uint32_t(SSEExt::kSSE4_1);
  if (features.x86().hasSSE4_2()) sseExtMask |= 1u << uint32_t(SSEExt::kSSE4_2);
  if (features.x86().hasPCLMULQDQ()) sseExtMask |= 1u << uint32_t(SSEExt::kPCLMULQDQ);

  if (features.x86().hasAVX()) {
    avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX);
    if (features.x86().hasAVX2()            ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX2);
    if (features.x86().hasF16C()            ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kF16C);
    if (features.x86().hasFMA()             ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kFMA);
    if (features.x86().hasGFNI()            ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kGFNI);
    if (features.x86().hasVAES()            ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kVAES);
    if (features.x86().hasVPCLMULQDQ()      ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kVPCLMULQDQ);
    if (features.x86().hasAVX_IFMA()        ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX_IFMA);
    if (features.x86().hasAVX_NE_CONVERT()  ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX_NE_CONVERT);
    if (features.x86().hasAVX_VNNI()        ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX_VNNI);
    if (features.x86().hasAVX_VNNI_INT8()   ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX_VNNI_INT8);
    if (features.x86().hasAVX_VNNI_INT16()  ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX_VNNI_INT16);
  }

  if (features.x86().hasAVX2()      && features.x86().hasAVX512_F() &&
      features.x86().hasAVX512_CD() && features.x86().hasAVX512_BW() &&
      features.x86().hasAVX512_DQ() && features.x86().hasAVX512_VL()) {
    _vecRegCount = 32;
    avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX512);
    if (features.x86().hasAVX512_BF16()     ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX512_BF16);
    if (features.x86().hasAVX512_BITALG()   ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX512_BITALG);
    if (features.x86().hasAVX512_FP16()     ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX512_FP16);
    if (features.x86().hasAVX512_IFMA()     ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX512_IFMA);
    if (features.x86().hasAVX512_VBMI()     ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX512_VBMI);
    if (features.x86().hasAVX512_VBMI2()    ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX512_VBMI2);
    if (features.x86().hasAVX512_VNNI()     ) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX512_VNNI);
    if (features.x86().hasAVX512_VPOPCNTDQ()) avxExtMask |= uint64_t(1) << uint32_t(AVXExt::kAVX512_VPOPCNTDQ);
  }

  _gpExtMask = gpExtMask;
  _sseExtMask = sseExtMask;
  _avxExtMask = avxExtMask;

  if (hasFMA()) {
    _fMulAddOpBehavior = FMulAddOpBehavior::kFMAStoreToAny;
  }
}

VecWidth PipeCompiler::maxVecWidthFromCpuFeatures() noexcept {
  // Use 512-bit SIMD width if AVX512 is available and the target is 64-bit. We never use 512-bit SIMD in 32-bit mode
  // as it doesn't have enough registers to hold 512-bit constants and we don't store 512-bit constants in memory
  // (they must be broadcasted to full width).
  if (hasAVX512() && is64Bit())
    return VecWidth::k512;

  // Use 256-bit SIMD width if AVX2 is available.
  if (hasAVX2())
    return VecWidth::k256;

  return VecWidth::k128;
}

void PipeCompiler::initVecWidth(VecWidth vw) noexcept {
  _vecWidth = vw;
  _vecRegType = simdRegTypeFromWidth(vw);
  _vecTypeId = asmjit::ArchTraits::byArch(cc->arch()).regTypeToTypeId(_vecRegType);
  _vecMultiplier = 1u << (uint32_t(_vecRegType) - uint32_t(RegType::kX86_Xmm));
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

  if (hasAVX()) {
    funcNode->frame().setAvxEnabled();
    funcNode->frame().setAvxCleanup();
  }

  if (hasAVX512()) {
    funcNode->frame().setAvx512Enabled();
  }
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

x86::KReg PipeCompiler::kConst(uint64_t value) noexcept {
  uint32_t slot;
  for (slot = 0; slot < kMaxKRegConstCount; slot++)
    if (_kReg[slot].isValid() && _kImm[slot] == value)
      return _kReg[slot];

  asmjit::BaseNode* prevNode = nullptr;
  Gp tmp;
  x86::KReg kReg;

  if (slot < kMaxKRegConstCount) {
    prevNode = cc->setCursor(_funcInit);
  }

  if (value & 0xFFFFFFFF00000000u) {
    tmp = newGp64("kTmp");
    kReg = cc->newKq("k0x%016llX", (unsigned long long)value);
    cc->mov(tmp, value);
    cc->kmovq(kReg, tmp);
  }
  else {
    tmp = newGp32("kTmp");
    kReg = cc->newKd("k0x%08llX", (unsigned long long)value);
    cc->mov(tmp, value);
    cc->kmovd(kReg, tmp);
  }

  if (slot < kMaxKRegConstCount) {
    _kReg[slot] = kReg;
    _funcInit = cc->setCursor(prevNode);
  }

  return kReg;
}

Operand PipeCompiler::simdConst(const void* c, Bcst bcstWidth, VecWidth constWidth) noexcept {
  size_t constCount = _vecConsts.size();

  for (size_t i = 0; i < constCount; i++)
    if (_vecConsts[i].ptr == c)
      return Vec(signatureOfXmmYmmZmm[size_t(constWidth)], _vecConsts[i].vRegId);

  // We don't use memory constants when compiling for AVX-512, because we don't store 64-byte constants and AVX-512
  // has enough registers to hold all the constants that we need. However, in SSE/AVX2 case, we don't want so many
  // constants in registers as that could limit registers that we need during fetching and composition.
  if (!hasAVX512()) {
    bool useVReg = c == &commonTable.i_0000000000000000 || // Required if the CPU doesn't have SSE4.1.
                   c == &commonTable.i_0080008000800080 || // Required by `div255()` and friends.
                   c == &commonTable.i_0101010101010101 || // Required by `div255()` and friends.
                   c == &commonTable.i_FF000000FF000000 ;  // Required by fetching XRGB32 pixels as PRGB32 pixels.

    if (!useVReg)
      return simdMemConst(c, bcstWidth, constWidth);
  }

  return Vec(signatureOfXmmYmmZmm[size_t(constWidth)], _newVecConst(c, bcstWidth == Bcst::kNA_Unique).id());
}

Operand PipeCompiler::simdConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept {
  VecWidth constWidth = VecWidth(uint32_t(similarTo.type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdConst(c, bcstWidth, constWidth);
}

Operand PipeCompiler::simdConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept {
  BL_ASSERT(!similarTo.empty());

  VecWidth constWidth = VecWidth(uint32_t(similarTo[0].type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdConst(c, bcstWidth, constWidth);
}

Vec PipeCompiler::simdVecConst(const void* c, Bcst bcstWidth, VecWidth constWidth) noexcept {
  size_t constCount = _vecConsts.size();

  for (size_t i = 0; i < constCount; i++)
    if (_vecConsts[i].ptr == c)
      return Vec(signatureOfXmmYmmZmm[size_t(constWidth)], _vecConsts[i].vRegId);

  return Vec(signatureOfXmmYmmZmm[size_t(constWidth)], _newVecConst(c, bcstWidth == Bcst::kNA_Unique).id());
}

Vec PipeCompiler::simdVecConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept {
  VecWidth constWidth = VecWidth(uint32_t(similarTo.type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdVecConst(c, bcstWidth, constWidth);
}

Vec PipeCompiler::simdVecConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept {
  BL_ASSERT(!similarTo.empty());

  VecWidth constWidth = VecWidth(uint32_t(similarTo[0].type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdVecConst(c, bcstWidth, constWidth);
}

x86::Mem PipeCompiler::simdMemConst(const void* c, Bcst bcstWidth, VecWidth constWidth) noexcept {
  x86::Mem m = _getMemConst(c);
  if (constWidth != VecWidth::k512)
    return m;

  x86::Mem::Broadcast bcst = x86::Mem::Broadcast::kNone;
  switch (bcstWidth) {
    case Bcst::k8: bcst = x86::Mem::Broadcast::k1To64; break;
    case Bcst::k16: bcst = x86::Mem::Broadcast::k1To32; break;
    case Bcst::k32: bcst = x86::Mem::Broadcast::k1To16; break;
    case Bcst::k64: bcst = x86::Mem::Broadcast::k1To8; break;
    default: bcst = x86::Mem::Broadcast::kNone; break;
  }

  m.setBroadcast(bcst);
  return m;
}

x86::Mem PipeCompiler::simdMemConst(const void* c, Bcst bcstWidth, const Vec& similarTo) noexcept {
  VecWidth constWidth = VecWidth(uint32_t(similarTo.type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdMemConst(c, bcstWidth, constWidth);
}

x86::Mem PipeCompiler::simdMemConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept {
  BL_ASSERT(!similarTo.empty());

  VecWidth constWidth = VecWidth(uint32_t(similarTo[0].type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdMemConst(c, bcstWidth, constWidth);
}

x86::Mem PipeCompiler::_getMemConst(const void* c) noexcept {
  // Make sure we are addressing a constant from the `commonTable` constant pool.
  const void* global = &commonTable;
  BL_ASSERT((uintptr_t)c >= (uintptr_t)global &&
            (uintptr_t)c <  (uintptr_t)global + sizeof(CommonTable));

  if (is32Bit()) {
    // 32-bit mode - These constants will never move in memory so the absolute addressing is a win/win as we can save
    // one GP register that can be used for something else.
    return x86::ptr((uint64_t)c);
  }
  else {
    // 64-bit mode - One GP register is sacrificed to hold the pointer to the `commonTable`. This is probably the
    // safest approach as relying on absolute addressing or anything else could lead to problems or performance issues.
    _initCommonTablePtr();

    int32_t disp = int32_t((intptr_t)c - (intptr_t)global);
    return x86::ptr(_commonTablePtr, disp - _commonTableOff);
  }
}

Vec PipeCompiler::_newVecConst(const void* c, bool isUniqueConst) noexcept {
  Vec vReg;
  const char* specialConstName = nullptr;

  if (c == commonTable.swizu8_dither_rgba64_lo.data)
    specialConstName = "swizu8_dither_rgba64_lo";
  else if (c == commonTable.swizu8_dither_rgba64_hi.data)
    specialConstName = "swizu8_dither_rgba64_hi";

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
    v_zero_i(vReg.xmm());
  }
  else {
    // NOTE: _getMemConst() must be outside of injected code as it uses injection too.
    Mem m = _getMemConst(c);

    ScopedInjector inject(cc, &_funcInit);
    if (hasAVX512() && !vReg.isXmm() && !isUniqueConst)
      cc->vbroadcasti32x4(vReg, m);
    else if (hasAVX2() && vReg.isYmm() && !isUniqueConst)
      cc->vbroadcasti128(vReg, m);
    else if (hasAVX512())
      cc->vmovdqa32(vReg, m); // EVEX prefix has a compressed displacement, which is smaller.
    else
      v_loadavec(vReg, m);
  }
  return vReg;
}

// bl::Pipeline::PipeCompiler - Stack
// ==================================

x86::Mem PipeCompiler::tmpStack(StackId id, uint32_t size) noexcept {
  BL_ASSERT(IntOps::isPowerOf2(size));
  BL_ASSERT(size <= 64);

  // Only used by asserts.
  blUnused(size);

  Mem& stack = _tmpStack[size_t(id)];
  if (!stack.baseId())
    stack = cc->newStack(64, 16, "tmpStack");
  return stack;
}

// bl::Pipeline::PipeCompiler - Utilities
// ======================================

void PipeCompiler::embedJumpTable(const Label* jumpTable, size_t jumpTableSize, const Label& jumpTableBase, uint32_t entrySize) noexcept {
  static const uint8_t zeros[8] {};

  for (size_t i = 0; i < jumpTableSize; i++) {
    if (jumpTable[i].isValid())
      cc->embedLabelDelta(jumpTable[i], jumpTableBase, entrySize);
    else
      cc->embed(zeros, entrySize);
  }
}

// bl::Pipeline::PipeCompiler - General Purpose Instructions - Conditions
// ======================================================================

static constexpr InstId condition_to_inst_id[size_t(OpcodeCond::kMaxValue) + 1] = {
  Inst::kIdAnd,  // OpcodeCond::kAssignAnd
  Inst::kIdOr,   // OpcodeCond::kAssignOr
  Inst::kIdXor,  // OpcodeCond::kAssignXor
  Inst::kIdAdd,  // OpcodeCond::kAssignAdd
  Inst::kIdSub,  // OpcodeCond::kAssignSub
  Inst::kIdShr,  // OpcodeCond::kAssignShr
  Inst::kIdTest, // OpcodeCond::kTest
  Inst::kIdBt,   // OpcodeCond::kBitTest
  Inst::kIdCmp   // OpcodeCond::kCompare
};

class ConditionApplier : public Condition {
public:
  BL_INLINE ConditionApplier(const Condition& condition) noexcept : Condition(condition) {
    // The first operand must always be a register.
    BL_ASSERT(a.isGp());
  }

  BL_NOINLINE void optimize(PipeCompiler* pc) noexcept {
    switch (op) {
      case OpcodeCond::kAssignShr:
        if (b.isImm() && b.as<Imm>().value() == 0) {
          if (a.isGp32()) {
            // Shifting by 0 would not set the flags...
            op = OpcodeCond::kAssignAnd;
            b = a;
          }
          else {
            op = OpcodeCond::kTest;
            b = a;
          }
        }
        break;

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

          // NOTE: AMD has no performance difference between 'test' and 'bt' instructions, however, Intel can execute less
          // 'bt' instructions per cycle than 'test's, so we prefer 'test' if bitIndex is low. Additionally, we only use
          // test on 64-bit hardware as it's guaranteed that any register index is encodable. On 32-bit hardware only the
          // first 4 registers can be used, which could mean that the register would have to be moved just to be tested,
          // which is something we would like to avoid.
          if (pc->is64Bit() && bitIndex < 8) {
            op = OpcodeCond::kTest;
            b = Imm(1u << bitIndex);
            cond = cond == CondCode::kC ? CondCode::kNZ : CondCode::kZ;
          }
        }
        break;
      }

      default:
        break;
    }
  }

  BL_INLINE void reverse() noexcept {
    cond = x86::reverseCond(cond);
  }

  BL_NOINLINE void emit(PipeCompiler* pc) noexcept {
    AsmCompiler* cc = pc->cc;
    InstId instId = condition_to_inst_id[size_t(op)];

    if (instId == Inst::kIdTest && cc->is64Bit()) {
      if (b.isImm() && b.as<Imm>().valueAs<uint64_t>() <= 255u) {
        // Emit 8-bit operation if targeting 64-bit mode and the immediate fits 8 bits.
        cc->test(a.as<Gp>().r8(), b.as<Imm>());
        return;
      }
      else if (a.as<Gp>().size() > 4 && b.isImm() && uint64_t(b.as<Imm>().value()) <= 0xFFFFFFFFu) {
        // Emit 32-bit operation if targeting 64-bit mode and the immediate is lesser than UINT32_MAX.
        // This possibly saves a REX prefix required to promote the instruction to a 64-bit operation.
        cc->test(a.as<Gp>().r32(), b.as<Imm>());
        return;
      }
    }

    if (instId == Inst::kIdShr && b.isReg()) {
      cc->emit(instId, a, b.as<Gp>().r8());
      return;
    }


    cc->emit(instId, a, b);
  }
};

// bl::Pipeline::PipeCompiler - General Purpose Instructions - Emit
// ================================================================

void PipeCompiler::emit_mov(const Gp& dst, const Operand_& src) noexcept {
  if (src.isImm() && src.as<Imm>().value() == 0) {
    Gp r(dst);
    if (r.isGpq())
      r = r.r32();
    cc->xor_(r, r);
  }
  else {
    cc->emit(Inst::kIdMov, dst, src);
  }
}

void PipeCompiler::emit_m(OpcodeM op, const Mem& m_) noexcept {
  static constexpr uint8_t size_table[] = {
    0, // kStoreZeroReg
    1, // kStoreZeroU8
    2, // kStoreZeroU16
    4, // kStoreZeroU32
    8  // kStoreZeroU64
  };

  Mem m(m_);
  uint32_t size = size_table[size_t(op)];
  if (size == 0)
    size = cc->registerSize();

  m.setSize(size);
  cc->mov(m, 0);
}

void PipeCompiler::emit_rm(OpcodeRM op, const Gp& dst, const Mem& src) noexcept {
  static constexpr uint8_t size_table[] = {
    0, // kLoadReg
    1, // kLoadI8
    1, // kLoadU8
    2, // kLoadI16
    2, // kLoadU16
    4, // kLoadI32
    4, // kLoadU32
    8, // kLoadI64
    8, // kLoadU64
    1, // kLoadMergeU8
    1, // kLoadShiftU8
    2, // kLoadMergeU16
    2  // kLoadShiftU16
  };

  Gp r(dst);
  Mem m(src);

  InstId instId = Inst::kIdMov;
  uint32_t size = size_table[size_t(op)];

  switch (op) {
    case OpcodeRM::kLoadReg:
      size = dst.size();
      break;

    case OpcodeRM::kLoadU8:
    case OpcodeRM::kLoadU16:
    case OpcodeRM::kLoadU32:
      r.setSignature(x86::RegTraits<RegType::kGp32>::kSignature);
      if (size < 4)
        instId = Inst::kIdMovzx;
      break;

    case OpcodeRM::kLoadI8:
    case OpcodeRM::kLoadI16:
      instId = Inst::kIdMovsx;
      break;

    case OpcodeRM::kLoadI32:
      instId = dst.isGpq() ? Inst::kIdMovsxd : Inst::kIdMov;
      break;

    case OpcodeRM::kLoadI64:
    case OpcodeRM::kLoadU64:
      BL_ASSERT(dst.isGpq());
      m.setSize(8);
      break;

    case OpcodeRM::kLoadShiftU8:
      cc->shl(r, 8);
      BL_FALLTHROUGH
    case OpcodeRM::kLoadMergeU8:
      r = r.r8();
      break;

    case OpcodeRM::kLoadShiftU16:
      cc->shl(r, 16);
      BL_FALLTHROUGH
    case OpcodeRM::kLoadMergeU16:
      r = r.r16();
      break;

    default:
      BL_NOT_REACHED();
  }

  m.setSize(size);
  cc->emit(instId, r, m);
}

struct OpcodeMRInfo {
  uint16_t instId;
  uint16_t size;
};

void PipeCompiler::emit_mr(OpcodeMR op, const Mem& dst, const Gp& src) noexcept {
  static constexpr OpcodeMRInfo op_info_table[] = {
    { Inst::kIdMov, 0 }, // kStoreReg
    { Inst::kIdMov, 1 }, // kStoreU8
    { Inst::kIdMov, 2 }, // kStoreU16
    { Inst::kIdMov, 4 }, // kStoreU32
    { Inst::kIdMov, 8 }, // kStoreU64
    { Inst::kIdAdd, 0 }, // kAddReg,
    { Inst::kIdAdd, 1 }, // kAddU8,
    { Inst::kIdAdd, 2 }, // kAddU16,
    { Inst::kIdAdd, 4 }, // kAddU32,
    { Inst::kIdAdd, 8 }  // kAddU64
  };

  Mem m(dst);
  Gp r = src;

  const OpcodeMRInfo& opInfo = op_info_table[size_t(op)];

  uint32_t size = opInfo.size;
  switch (size) {
    case 0: size = src.size(); break;
    case 1: r = src.r8(); break;
    case 2: r = src.r16(); break;
    case 4: r = src.r32(); break;
    case 8: r = src.r64(); break;

    default:
      BL_NOT_REACHED();
  }

  m.setSize(size);
  cc->emit(opInfo.instId, m, r);
}

void PipeCompiler::emit_cmov(const Gp& dst, const Operand_& sel, const Condition& condition) noexcept {
  ConditionApplier ca(condition);
  ca.optimize(this);
  ca.emit(this);
  cc->emit(Inst::cmovccFromCond(ca.cond), dst, sel);
}

void PipeCompiler::emit_select(const Gp& dst, const Operand_& sel1_, const Operand_& sel2_, const Condition& condition) noexcept {
  ConditionApplier ca(condition);
  ca.optimize(this);

  bool dstIsA = ca.a.isReg() && dst.id() == ca.a.as<Reg>().id();
  bool dstIsB = ca.b.isReg() && dst.id() == ca.b.as<Reg>().id();

  Operand sel1(sel1_);
  Operand sel2(sel2_);

  // Reverse the condition if we can place the immediate value first or if `dst == sel2`.
  if ((!sel1.isImm() && sel2.isImm()) || (sel2.isReg() && dst.id() == sel2.id())) {
    ca.reverse();
    BLInternal::swap(sel1, sel2);
  }

  bool dstIsSel = sel1.isReg() && dst.id() == sel1.id();
  if (sel1 == sel2) {
    if (!dstIsSel)
      cc->emit(Inst::kIdMov, dst, sel1);
    return;
  }

  if (sel1.isImm() && sel1.as<Imm>().value() == 0 && !dstIsA && !dstIsB && !dstIsSel) {
    cc->xor_(dst, dst);
    ca.emit(this);
  }
  else {
    ca.emit(this);
    if (!dstIsSel)
      cc->emit(Inst::kIdMov, dst, sel1);
  }

  if (sel2.isImm()) {
    int64_t value = sel2.as<Imm>().value();
    Mem sel2Mem = cc->newConst(asmjit::ConstPoolScope::kLocal, &value, dst.size());
    sel2 = sel2Mem;
  }

  cc->emit(Inst::cmovccFromCond(x86::negateCond(ca.cond)), dst, sel2);
}

void PipeCompiler::emit_2i(OpcodeRR op, const Gp& dst, const Operand_& src_) noexcept {
  Operand src(src_);

  // Notes
  //
  //   - CTZ:
  //     - INTEL - No difference, `bsf` and `tzcnt` both have latency ~2.5 cycles.
  //     - AMD   - Big difference, `tzcnt` has only ~1.5 cycle latency while `bsf` has ~2.5 cycles.

  // ArithOp Reg, Any
  // ----------------

  if (src.isRegOrMem()) {
    switch (op) {
      case OpcodeRR::kCLZ: {
        if (hasLZCNT()) {
          cc->emit(Inst::kIdLzcnt, dst, src);
        }
        else {
          uint32_t msk = (dst.size() * 8u) - 1u;
          cc->emit(Inst::kIdBsr, dst, src);
          cc->xor_(dst, msk);
        }
        return;
      }

      case OpcodeRR::kCTZ: {
        cc->emit(hasBMI() ? Inst::kIdTzcnt : Inst::kIdBsf, dst, src);
        return;
      }

      case OpcodeRR::kReflect: {
        int nBits = int(dst.size()) * 8 - 1;

        if (src.isReg() && dst.id() == src.as<Reg>().id()) {
          BL_ASSERT(dst.size() == src.as<Reg>().size());
          Gp copy = newSimilarReg(dst, "@copy");

          cc->mov(copy, dst);
          cc->sar(copy, nBits);
          cc->xor_(dst, copy);
        }
        else {
          cc->emit(Inst::kIdMov, dst, src);
          cc->sar(dst, nBits);
          cc->emit(Inst::kIdXor, dst, src);
        }
        return;
      }

      default:
        break;
    }
  }

  // ArithOp Reg, Mem
  // ----------------

  if (src.isMem()) {
    switch (op) {
      case OpcodeRR::kBSwap: {
        if (hasMOVBE()) {
          cc->movbe(dst, src.as<Mem>());
        }
        else {
          cc->mov(dst, src.as<Mem>());
          cc->bswap(dst);
        }
        return;
      }

      default:
        break;
    }

    Gp srcGp = newSimilarReg(dst, "@src");
    cc->mov(srcGp, src.as<Mem>());
    src = srcGp;
  }

  // ArithOp Reg, Reg
  // ----------------

  if (src.isReg()) {
    const Gp& srcGp = src.as<Gp>();
    bool dstIsSrc = dst.id() == srcGp.id();

    switch (op) {
      case OpcodeRR::kAbs: {
        if (dstIsSrc) {
          Gp tmp = newSimilarReg(dst, "@tmp");
          cc->mov(tmp, dst);
          cc->neg(dst);
          cc->cmovs(dst, tmp);
        }
        else {
          cc->mov(dst, srcGp);
          cc->neg(dst);
          cc->cmovs(dst, srcGp);
        }
        return;
      }

      case OpcodeRR::kBSwap: {
        if (!dstIsSrc)
          cc->mov(dst, srcGp);
        cc->bswap(dst);
        return;
      }

      case OpcodeRR::kNeg:
      case OpcodeRR::kNot: {
        if (!dstIsSrc)
          cc->mov(dst, srcGp);
        cc->emit(op == OpcodeRR::kNeg ? Inst::kIdNeg : Inst::kIdNot, dst);
        return;
      }

      default:
        break;
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

struct OpcodeRRRMinMaxCMovInst { InstId a, b; };

void PipeCompiler::emit_3i(OpcodeRRR op, const Gp& dst, const Operand_& src1_, const Operand_& src2_) noexcept {
  Operand src1(src1_);
  Operand src2(src2_);

  static constexpr OpcodeRRRMinMaxCMovInst arithMinMaxCMovInstTable[4] = {
    { Inst::kIdCmovl, Inst::kIdCmovg }, // MinI
    { Inst::kIdCmovg, Inst::kIdCmovl }, // MaxI
    { Inst::kIdCmovb, Inst::kIdCmova }, // MinU
    { Inst::kIdCmova, Inst::kIdCmovb }  // MaxU
  };

  static constexpr InstId legacyShiftInstTable[5] = {
    Inst::kIdShl,  // SHL
    Inst::kIdShr,  // SHR
    Inst::kIdSar,  // SAR
    Inst::kIdRol,  // ROL
    Inst::kIdRor   // ROR
  };

  static constexpr InstId legacyLogicalInstTable[3] = {
    Inst::kIdAnd,  // AND
    Inst::kIdOr,   // OR
    Inst::kIdXor   // XOR
  };

  static constexpr InstId bmi2ShiftInstTable[5] = {
    Inst::kIdShlx, // SHL
    Inst::kIdShrx, // SHR
    Inst::kIdSarx, // SAR
    Inst::kIdNone, // ROL (doesn't exist).
    Inst::kIdNone  // ROR (can only be used with immediate, special handling).
  };

  // ArithOp Reg, Mem, Imm
  // ---------------------

  if (src1.isMem() && src2.isImm()) {
    const Mem& a = src1.as<Mem>();
    const Imm& b = src2.as<Imm>();

    switch (op) {
      case OpcodeRRR::kMul:
        cc->imul(dst, a, b);
        return;

      default:
        break;
    }

    cc->mov(dst, a);
    src1 = dst;
  }

  if (!src1.isReg() && isOp3ICommutative(op)) {
    BLInternal::swap(src1, src2);
  }

  // ArithOp Reg, Reg, Imm
  // ---------------------

  if (src1.isReg() && src2.isImm()) {
    const Gp& a = src1.as<Gp>();
    const Imm& b = src2.as<Imm>();

    bool dstIsA = dst.id() == a.id();
    BL_ASSERT(dst.size() == a.size());

    switch (op) {
      case OpcodeRRR::kAnd:
      case OpcodeRRR::kOr:
      case OpcodeRRR::kXor: {
        InstId instId = legacyLogicalInstTable[size_t(op) - size_t(OpcodeRRR::kAnd)];
        if (!dstIsA)
          cc->mov(dst, a);
        cc->emit(instId, dst, b);
        return;
      }

      case OpcodeRRR::kBic: {
        if (!dstIsA)
          cc->mov(dst, a);

        Imm nImm(~b.value());
        if (dst.size() <= 4)
          nImm.signExtend32Bits();
        cc->and_(dst, nImm);
        return;
      }

      case OpcodeRRR::kAdd: {
        if (!dstIsA && b.isInt32()) {
          lea(dst, x86::ptr(a, b.valueAs<int32_t>()));
        }
        else {
          if (!dstIsA)
            cc->mov(dst, a);

          if (b.value() == 128) {
            cc->sub(dst, -128);
          }
          else {
            cc->add(dst, b);
          }
        }
        return;
      }

      case OpcodeRRR::kSub: {
        if (!dstIsA) {
          lea(dst, x86::ptr(a, int32_t(0u - b.valueAs<uint32_t>())));
        }
        else {
          cc->sub(dst, b);
        }
        return;
      }

      case OpcodeRRR::kMul: {
        int64_t val = b.value();
        if (dstIsA && IntOps::isPowerOf2(uint64_t(val))) {
          cc->shl(dst, IntOps::ctz(val));
          return;
        }

        switch (b.value()) {
          case 0:
            cc->xor_(dst, dst);
            return;

          case 1:
            if (!dstIsA)
              cc->mov(dst, a);
            return;

          case 2:
            lea(dst, x86::ptr(a, a));
            return;

          case 3:
            lea(dst, x86::ptr(a, a, 1));
            return;

          case 5:
            lea(dst, x86::ptr(a, a, 2));
            return;

          case 9:
            lea(dst, x86::ptr(a, a, 3));
            return;

          default:
            break;
        }

        cc->imul(dst, a, b);
        return;
      }

      case OpcodeRRR::kSMin:
      case OpcodeRRR::kSMax:
      case OpcodeRRR::kUMin:
      case OpcodeRRR::kUMax: {
        const OpcodeRRRMinMaxCMovInst& cmovInst = arithMinMaxCMovInstTable[size_t(op) - size_t(OpcodeRRR::kSMin)];

        if (dstIsA) {
          Gp tmp = newSimilarReg(dst, "@tmp");
          cc->mov(tmp, b);
          cc->cmp(dst, tmp);
          cc->emit(cmovInst.b, dst, tmp);
        }
        else {
          cc->mov(dst, b);
          cc->cmp(dst, a);
          cc->emit(cmovInst.b, dst, a); // cmovInst.b is correct, we have reversed the comparison in this case.
        }
        return;
      }

      case OpcodeRRR::kSll:
        // Optimize `dst = dst << 1`.
        if (b.value() == 1) {
          if (dstIsA) {
            // `dst = dst + dst`.
            cc->add(dst, dst);
          }
          else if (is64Bit()) {
            // `dst = a + a` (using a 64-bit address saves address-override prefix).
            cc->lea(dst, x86::ptr(a.r64(), a.r64()));
          }
          else {
            // `dst = a + a`.
            cc->lea(dst, x86::ptr(a, a));
          }
          return;
        }
        BL_FALLTHROUGH
      case OpcodeRRR::kSrl:
      case OpcodeRRR::kSra: {
        InstId legacyInst = legacyShiftInstTable[size_t(op) - size_t(OpcodeRRR::kSll)];

        if (!dstIsA)
          cc->mov(dst, a);
        cc->emit(legacyInst, dst, b);
        return;
      }

      case OpcodeRRR::kRol: {
        if (hasBMI2()) {
          uint32_t regSize = dst.size() * 8u;
          uint32_t imm = (regSize - b.valueAs<uint32_t>()) & asmjit::Support::lsbMask<uint32_t>(regSize);
          cc->rorx(dst, a, imm);
        }
        else {
          if (!dstIsA)
            cc->mov(dst, a);
          cc->rol(dst, b);
        }
        return;
      }

      case OpcodeRRR::kRor: {
        if (hasBMI2()) {
          cc->rorx(dst, a, b);
        }
        else {
          if (!dstIsA)
            cc->mov(dst, a);
          cc->ror(dst, b);
        }
        return;
      }

      default:
        break;
    }

    Gp bTmp = newSimilarReg(dst, "@bImm");
    cc->mov(bTmp, b);
    src2 = bTmp;
  }

  // ArithOp Reg, Mem, Reg
  // ---------------------

  if (src1.isMem() && src2.isReg()) {
    const Mem& a = src1.as<Mem>();
    const Gp& b = src2.as<Gp>();

    bool dstIsB = dst.id() == b.id();

    switch (op) {
      case OpcodeRRR::kAnd:
      case OpcodeRRR::kOr:
      case OpcodeRRR::kXor:
      case OpcodeRRR::kAdd:
      case OpcodeRRR::kMul:
      case OpcodeRRR::kSMin:
      case OpcodeRRR::kSMax:
      case OpcodeRRR::kUMin:
      case OpcodeRRR::kUMax:
        // These are commutative, so this should never happen as these should have been corrected to `Reg, Reg, Mem`.
        BL_NOT_REACHED();

      case OpcodeRRR::kSub: {
        BL_ASSERT(dst.size() == b.size());

        if (dstIsB) {
          cc->neg(dst);
          cc->add(dst, a);
          return;
        }

        // Bail to `Reg, Reg, Reg` form.
        break;
      }

      case OpcodeRRR::kSll:
      case OpcodeRRR::kSrl:
      case OpcodeRRR::kSra: {
        // Prefer BMI2 variants: SHLX, SHRX, SARX, and RORX.
        if (hasBMI2()) {
          InstId bmi2Inst = bmi2ShiftInstTable[size_t(op) - size_t(OpcodeRRR::kSll)];
          cc->emit(bmi2Inst, dst, a, b.cloneAs(dst));
          return;
        }

        // Bail to `Reg, Reg, Reg` form if BMI2 is not available.
        break;
      }

      default:
        break;
    }

    if (!dstIsB) {
      cc->mov(dst, a);
      src1 = dst;
    }
    else {
      Gp aTmp = newSimilarReg(dst, "@aTmp");
      cc->mov(aTmp, a);
      src1 = aTmp;
    }
  }

  // ArithOp Reg, Reg, Mem
  // ---------------------

  if (src1.isReg() && src2.isMem()) {
    const Gp& a = src1.as<Gp>();
    const Mem& b = src2.as<Mem>();

    bool dstIsA = dst.id() == a.id();
    BL_ASSERT(dst.size() == a.size());

    switch (op) {
      case OpcodeRRR::kAnd:
      case OpcodeRRR::kOr:
      case OpcodeRRR::kXor: {
        InstId instId = legacyLogicalInstTable[size_t(op) - size_t(OpcodeRRR::kAnd)];
        if (!dstIsA)
          cc->mov(dst, a);
        cc->emit(instId, dst, b);
        return;
      }

      case OpcodeRRR::kBic: {
        Gp tmp = newSimilarReg(dst);
        cc->mov(tmp, b);
        cc->not_(tmp);
        if (!dstIsA)
          cc->mov(dst, a);
        cc->and_(dst, tmp);
        return;
      }

      case OpcodeRRR::kAdd: {
        if (!dstIsA)
          cc->mov(dst, a);
        cc->add(dst, b);
        return;
      }

      case OpcodeRRR::kSub: {
        if (!dstIsA)
          cc->mov(dst, a);
        cc->sub(dst, b);
        return;
      }

      case OpcodeRRR::kMul: {
        if (!dstIsA)
          cc->mov(dst, a);
        cc->imul(dst, b);
        return;
      }

      case OpcodeRRR::kUDiv: {
        Gp tmp1 = newSimilarReg(dst, "@tmp1");
        cc->xor_(tmp1, tmp1);

        if (dstIsA) {
          cc->div(tmp1, dst, b);
        }
        else {
          cc->mov(dst, a);
          cc->div(tmp1, dst, b);
        }
        return;
      }

      case OpcodeRRR::kUMod: {
        Gp tmp1 = newSimilarReg(dst, "@tmp1");
        cc->xor_(tmp1, tmp1);

        if (dstIsA) {
          cc->div(tmp1, dst, b);
          cc->mov(dst, tmp1);
        }
        else {
          Gp tmp2 = newSimilarReg(dst, "@tmp2");
          cc->mov(tmp2, a);
          cc->div(tmp1, tmp2, b);
          cc->mov(dst, tmp1);
        }
        return;
      }

      case OpcodeRRR::kSMin:
      case OpcodeRRR::kSMax:
      case OpcodeRRR::kUMin:
      case OpcodeRRR::kUMax: {
        const OpcodeRRRMinMaxCMovInst& cmovInst = arithMinMaxCMovInstTable[size_t(op) - size_t(OpcodeRRR::kSMin)];

        if (dstIsA) {
          cc->cmp(dst, b);
          cc->emit(cmovInst.b, dst, b);
        }
        else {
          cc->mov(dst, b);
          cc->cmp(dst, a);
          cc->emit(cmovInst.b, dst, a); // cmovInst.b is correct, we have reversed the comparison in this case.
        }
        return;
      }

      case OpcodeRRR::kSBound: {
        cc->xor_(dst, dst);
        cc->cmp(a, b);
        cc->cmovbe(dst, a);
        cc->cmovg(dst, b);
        return;
      }

      default:
        break;
    }

    Gp bTmp = newSimilarReg(dst, "@bTmp");
    cc->mov(bTmp, b);
    src2 = bTmp;
  }

  // ArithOp Reg, Reg, Reg
  // ---------------------

  if (src1.isReg() && src2.isReg()) {
    const Gp& a = src1.as<Gp>();
    const Gp& b = src2.as<Gp>();

    bool aIsB = a.id() == b.id();
    bool dstIsA = dst.id() == a.id();
    bool dstIsB = dst.id() == b.id();

    BL_ASSERT(dst.size() == a.size());

    switch (op) {
      case OpcodeRRR::kAnd:
      case OpcodeRRR::kOr:
      case OpcodeRRR::kXor: {
        BL_ASSERT(dst.size() == b.size());

        InstId instId = legacyLogicalInstTable[size_t(op) - size_t(OpcodeRRR::kAnd)];
        if (!dstIsA)
          cc->mov(dst, a);
        cc->emit(instId, dst, b);
        return;
      }

      case OpcodeRRR::kBic: {
        BL_ASSERT(dst.size() == b.size());

        if (hasBMI()) {
          cc->andn(dst, b, a);
        }
        else if (dstIsB) {
          if (dstIsA) {
            cc->mov(dst, 0);
            return;
          }
          cc->not_(dst);
          cc->and_(dst, a);
        }
        else {
          Gp tmp = newSimilarReg(dst, "@tmp");
          cc->mov(tmp, b);
          cc->not_(tmp);
          if (!dstIsA)
            cc->mov(dst, a);
          cc->and_(dst, tmp);
        }
        return;
      }

      case OpcodeRRR::kAdd: {
        BL_ASSERT(dst.size() == b.size());

        if (dstIsA || dstIsB) {
          cc->add(dst, dstIsB ? a : b);
        }
        else if (dst.size() >= 4) {
          if (is64Bit())
            lea(dst, x86::ptr(a.r64(), b.r64()));
          else
            lea(dst, x86::ptr(a, b));
        }
        else {
          cc->mov(dst, a);
          cc->add(dst, b);
        }
        return;
      }

      case OpcodeRRR::kSub: {
        BL_ASSERT(dst.size() == b.size());

        if (aIsB) {
          cc->xor_(dst, dst);
        }
        else if (dstIsA) {
          cc->sub(dst, b);
        }
        else if (dstIsB) {
          cc->neg(dst);
          cc->add(dst, a);
        }
        else {
          cc->mov(dst, a);
          cc->sub(dst, b);
        }
        return;
      }

      case OpcodeRRR::kMul: {
        BL_ASSERT(dst.size() == b.size());

        if (!dstIsA && !dstIsB)
          cc->mov(dst, a);
        cc->imul(dst, dstIsB ? a : b);
        return;
      }

      case OpcodeRRR::kUDiv: {
        BL_ASSERT(dst.size() == b.size());

        Gp tmp1 = newSimilarReg(dst, "@tmp1");
        cc->xor_(tmp1, tmp1);

        if (dstIsA) {
          cc->div(tmp1, dst, b);
        }
        else if (dstIsB) {
          Gp tmp2 = newSimilarReg(dst, "@tmp2");
          cc->mov(tmp2, a);
          cc->div(tmp1, tmp2, b);
          cc->mov(dst, tmp2);
        }
        else {
          cc->mov(dst, a);
          cc->div(tmp1, dst, b);
        }
        return;
      }

      case OpcodeRRR::kUMod: {
        BL_ASSERT(dst.size() == b.size());

        Gp tmp1 = newSimilarReg(dst, "@tmp1");
        cc->xor_(tmp1, tmp1);

        if (dstIsA) {
          cc->div(tmp1, dst, b);
          cc->mov(dst, tmp1);
        }
        else {
          Gp tmp2 = newSimilarReg(dst, "@tmp2");
          cc->mov(tmp2, a);
          cc->div(tmp1, tmp2, b);
          cc->mov(dst, tmp1);
        }
        return;
      }

      case OpcodeRRR::kSMin:
      case OpcodeRRR::kSMax:
      case OpcodeRRR::kUMin:
      case OpcodeRRR::kUMax: {
        BL_ASSERT(dst.size() == b.size());
        const OpcodeRRRMinMaxCMovInst& cmovInst = arithMinMaxCMovInstTable[size_t(op) - size_t(OpcodeRRR::kSMin)];

        cc->cmp(a, b);
        if (dstIsB) {
          cc->emit(cmovInst.a, dst, a);
        }
        else {
          if (!dstIsA)
            cc->mov(dst, a);
          cc->emit(cmovInst.b, dst, b);
        }
        return;
      }

      case OpcodeRRR::kSll:
      case OpcodeRRR::kSrl:
      case OpcodeRRR::kSra:
      case OpcodeRRR::kRol:
      case OpcodeRRR::kRor: {
        // Prefer BMI2 variants: SHLX, SHRX, SARX, and RORX.
        if (hasBMI2()) {
          InstId bmi2Inst = bmi2ShiftInstTable[size_t(op) - size_t(OpcodeRRR::kSll)];
          if (bmi2Inst != Inst::kIdNone) {
            cc->emit(bmi2Inst, dst, a, b.cloneAs(dst));
            return;
          }
        }

        InstId legacyInst = legacyShiftInstTable[size_t(op) - size_t(OpcodeRRR::kSll)];
        if (dstIsA) {
          cc->emit(legacyInst, dst, b.r8());
          return;
        }
        else if (dstIsB) {
          Gp tmp = newGp32("@tmp");
          if (!dstIsA)
            cc->mov(dst, a);
          cc->mov(tmp, b.r32());
          cc->emit(legacyInst, dst, tmp.r8());
        }
        else {
          cc->mov(dst, a);
          cc->emit(legacyInst, dst, b.r8());
        }
        return;
      }

      case OpcodeRRR::kSBound: {
        if (dst.id() == a.id()) {
          Gp zero = newSimilarReg(dst, "@zero");

          cc->xor_(zero, zero);
          cc->cmp(dst, b);
          cc->cmova(dst, zero);
          cc->cmovg(dst, b);
        }
        else {
          cc->xor_(dst, dst);
          cc->cmp(a, b);
          cc->cmovbe(dst, a);
          cc->cmovg(dst, b);
        }
        return;
      }
    }
  }

  // Everything should be handled, so this should never be reached!
  BL_NOT_REACHED();
}

void PipeCompiler::emit_j(const Operand_& target) noexcept {
  cc->emit(Inst::kIdJmp, target);
}

void PipeCompiler::emit_j_if(const Label& target, const Condition& condition) noexcept {
  ConditionApplier ca(condition);
  ca.optimize(this);
  ca.emit(this);
  cc->j(ca.cond, target);
}

void PipeCompiler::adds_u8(const Gp& dst, const Gp& src1, const Gp& src2) noexcept {
  BL_ASSERT(dst.size() == src1.size());
  BL_ASSERT(dst.size() == src2.size());

  if (dst.id() == src1.id()) {
    cc->add(dst.r8(), src2.r8());
  }
  else if (dst.id() == src2.id()) {
    cc->add(dst.r8(), src1.r8());
  }
  else {
    cc->mov(dst, src1);
    cc->add(dst, src2);
  }

  Gp u8_msk = newGp32("@u8_msk");
  cc->sbb(u8_msk, u8_msk);
  cc->or_(dst.r8(), u8_msk.r8());
}

void PipeCompiler::inv_u8(const Gp& dst, const Gp& src) noexcept {
  if (dst.id() != src.id())
    cc->mov(dst, src);
  cc->xor_(dst.r8(), 0xFF);
}

void PipeCompiler::div_255_u32(const Gp& dst, const Gp& src) noexcept {
  BL_ASSERT(dst.size() == src.size());

  if (dst.id() == src.id()) {
    // tmp = src + 128;
    // dst = (tmp + (tmp >> 8)) >> 8
    Gp tmp = newSimilarReg(dst, "@tmp");
    cc->sub(dst, -128);
    cc->mov(tmp, dst);
    cc->shr(tmp, 8);
    cc->add(dst, tmp);
    cc->shr(dst, 8);
  }
  else {
    // dst = (src + 128 + ((src + 128) >> 8)) >> 8
    lea(dst, x86::ptr(src, 128));
    cc->shr(dst, 8);
    lea(dst, x86::ptr(dst, src, 0, 128));
    cc->shr(dst, 8);
  }
}

void PipeCompiler::mul_257_hu16(const Gp& dst, const Gp& src) noexcept {
  BL_ASSERT(dst.size() == src.size());
  cc->imul(dst, src, 257);
  cc->shr(dst, 16);
}

void PipeCompiler::add_scaled(const Gp& dst, const Gp& a, int b) noexcept {
  switch (b) {
    case 1:
      cc->add(dst, a);
      return;

    case 2:
    case 4:
    case 8: {
      uint32_t shift = b == 2 ? 1 :
                       b == 4 ? 2 : 3;
      lea(dst, x86::ptr(dst, a, shift));
      return;
    }

    default: {
      Gp tmp = newSimilarReg(dst, "@tmp");
      cc->imul(tmp, a, b);
      cc->add(dst, tmp);
      return;
    }
  }
}

void PipeCompiler::add_ext(const Gp& dst, const Gp& src_, const Gp& idx_, uint32_t scale, int32_t disp) noexcept {
  BL_ASSERT(scale != 0u);

  Gp src = src_.cloneAs(dst);
  Gp idx = idx_.cloneAs(dst);

  switch (scale) {
    case 1:
      if (dst.id() == src.id() && disp == 0) {
        cc->add(dst, idx);
        return;
      }
      BL_FALLTHROUGH
    case 2:
    case 4:
    case 8:
      lea(dst, x86::ptr(src, idx, asmjit::Support::ctz(scale), disp));
      return;

    default:
      break;
  }

  if (src.id() == idx.id()) {
    cc->imul(dst, src, scale + 1);
    return;
  }

  if (dst.id() != idx.id() && scale == 3) {
    lea(dst, x86::ptr(src, idx, 1, disp));
    cc->add(dst, idx);
    return;
  }

  Gp tmp = newSimilarReg(dst);
  cc->imul(tmp, idx, scale);
  cc->lea(dst, x86::ptr(src, tmp));
}

void PipeCompiler::lea(const Gp& dst, const Mem& src) noexcept {
  Mem m(src);

  if (is64Bit() && dst.size() == 4) {
    if (m.baseType() == asmjit::RegType::kGp32) m.setBaseType(asmjit::RegType::kGp64);
    if (m.indexType() == asmjit::RegType::kGp32) m.setIndexType(asmjit::RegType::kGp64);
  }

  cc->lea(dst, m);
}

// bl::Pipeline::PipeCompiler - Vector Instructions - Constants
// ============================================================

//! Floating point mode is used in places that are generic and implement various functionality that needs more
//! than a single instruction. Typically implementing either higher level concepts or missing functionality.
enum FloatMode : uint8_t {
  //! Scalar 32-bit floating point operation.
  kF32S = 0,
  //! Scalar 64-bit floating point operation.
  kF64S = 1,
  //! Vector 32-bit floating point operation.
  kF32V = 2,
  //! Vector 64-bit floating point operation.
  kF64V = 3,

  //! Used by non-floating point instructions.
  kNone = 4
};

enum class ElementSize : uint8_t {
  k8,
  k16,
  k32,
  k64
};

enum class SameVecOp : uint8_t {
  kNone = 0,
  kZero = 1,
  kOnes = 2,
  kSrc = 3
};

enum class VecPart : uint8_t {
  kNA = 0,
  kLo = 0,
  kHi = 1
};

enum class WideningOp : uint32_t  {
  kNone,
  kI8ToI16,
  kU8ToU16,
  kI8ToI32,
  kU8ToU32,
  kU8ToU64,
  kI16ToI32,
  kU16ToU32,
  kI32ToI64,
  kU32ToU64
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

// bl::Pipeline::PipeCompiler - Vector Instructions - Broadcast / Shuffle Data
// ===========================================================================

static constexpr uint16_t avx512_vinsert_128[] = {
  Inst::kIdVinserti32x4,
  Inst::kIdVinserti64x2,
  Inst::kIdVinsertf32x4,
  Inst::kIdVinsertf64x2
};

static constexpr uint16_t avx512_vshuf_128[] = {
  Inst::kIdVshufi32x4,
  Inst::kIdVshufi64x2,
  Inst::kIdVshuff32x4,
  Inst::kIdVshuff64x2
};

// bl::Pipeline::PipeCompiler - Vector Instructions - Integer Cmp/Min/Max Data
// ===========================================================================

struct CmpMinMaxInst {
  uint16_t peq;
  uint16_t pgt;
  uint16_t pmin;
  uint16_t pmax;
};

static constexpr CmpMinMaxInst sse_cmp_min_max[] = {
  { Inst::kIdPcmpeqb, Inst::kIdPcmpgtb, Inst::kIdPminsb, Inst::kIdPmaxsb },
  { Inst::kIdPcmpeqb, Inst::kIdPcmpgtb, Inst::kIdPminub, Inst::kIdPmaxub },
  { Inst::kIdPcmpeqw, Inst::kIdPcmpgtw, Inst::kIdPminsw, Inst::kIdPmaxsw },
  { Inst::kIdPcmpeqw, Inst::kIdPcmpgtw, Inst::kIdPminuw, Inst::kIdPmaxuw },
  { Inst::kIdPcmpeqd, Inst::kIdPcmpgtd, Inst::kIdPminsd, Inst::kIdPmaxsd },
  { Inst::kIdPcmpeqd, Inst::kIdPcmpgtd, Inst::kIdPminud, Inst::kIdPmaxud },
  { Inst::kIdPcmpeqq, Inst::kIdPcmpgtq, Inst::kIdNone  , Inst::kIdNone   },
  { Inst::kIdPcmpeqq, Inst::kIdPcmpgtq, Inst::kIdNone  , Inst::kIdNone   },
};

static constexpr CmpMinMaxInst avx_cmp_min_max[] = {
  { Inst::kIdVpcmpeqb, Inst::kIdVpcmpgtb, Inst::kIdVpminsb, Inst::kIdVpmaxsb },
  { Inst::kIdVpcmpeqb, Inst::kIdVpcmpgtb, Inst::kIdVpminub, Inst::kIdVpmaxub },
  { Inst::kIdVpcmpeqw, Inst::kIdVpcmpgtw, Inst::kIdVpminsw, Inst::kIdVpmaxsw },
  { Inst::kIdVpcmpeqw, Inst::kIdVpcmpgtw, Inst::kIdVpminuw, Inst::kIdVpmaxuw },
  { Inst::kIdVpcmpeqd, Inst::kIdVpcmpgtd, Inst::kIdVpminsd, Inst::kIdVpmaxsd },
  { Inst::kIdVpcmpeqd, Inst::kIdVpcmpgtd, Inst::kIdVpminud, Inst::kIdVpmaxud },
  { Inst::kIdVpcmpeqq, Inst::kIdVpcmpgtq, Inst::kIdVpminsq, Inst::kIdVpmaxsq },
  { Inst::kIdVpcmpeqq, Inst::kIdVpcmpgtq, Inst::kIdVpminuq, Inst::kIdVpmaxuq },
};

// bl::Pipeline::PipeCompiler - Vector Instructions - Integer Conversion Data
// ==========================================================================

struct WideningOpInfo {
  uint32_t mov         : 16;
  uint32_t unpackLo    : 16;
  uint32_t unpackHi    : 13;
  uint32_t signExtends : 1;
  uint32_t reserved    : 5;
};

struct NarrowingOpInfo {
  uint32_t mov         : 13;
  uint32_t pack        : 13;
  uint32_t sign        : 1;
  uint32_t mode        : 2;
  uint32_t reserved    : 19;
};

static constexpr WideningOpInfo sse_int_widening_op_info[] = {
  { Inst::kIdNone     , Inst::kIdNone      , Inst::kIdNone      , 0, 0 }, // kNone.
  { Inst::kIdPmovsxbw , Inst::kIdPunpcklbw , Inst::kIdPunpckhbw , 1, 0 }, // kI8ToI16.
  { Inst::kIdPmovzxbw , Inst::kIdPunpcklbw , Inst::kIdPunpckhbw , 0, 0 }, // kU8ToU16.
  { Inst::kIdPmovsxbd , Inst::kIdNone      , Inst::kIdNone      , 1, 0 }, // kI8ToI32.
  { Inst::kIdPmovzxbd , Inst::kIdNone      , Inst::kIdNone      , 0, 0 }, // kU8ToU32.
  { Inst::kIdPmovzxbq , Inst::kIdNone      , Inst::kIdNone      , 0, 0 }, // kU8ToU64.
  { Inst::kIdPmovsxwd , Inst::kIdPunpcklwd , Inst::kIdPunpckhwd , 1, 0 }, // kI16ToI32.
  { Inst::kIdPmovzxwd , Inst::kIdPunpcklwd , Inst::kIdPunpckhwd , 0, 0 }, // kU16ToU32.
  { Inst::kIdPmovsxdq , Inst::kIdPunpckldq , Inst::kIdPunpckhdq , 1, 0 }, // kI32ToI64.
  { Inst::kIdPmovzxdq , Inst::kIdPunpckldq , Inst::kIdPunpckhdq , 0, 0 }  // kU32ToU64.
};

// bl::Pipeline::PipeCompiler - Vector Instructions - Float Instruction Data
// =========================================================================

struct FloatInst {
  uint16_t fmovs;
  uint16_t fmov;
  uint16_t fand;
  uint16_t for_;
  uint16_t fxor;
  uint16_t fandn;
  uint16_t fadd;
  uint16_t fsub;
  uint16_t fmul;
  uint16_t fdiv;
  uint16_t fmin;
  uint16_t fmax;
  uint16_t fcmp;
  uint16_t fround;
  uint16_t psrl;
  uint16_t psll;
};

static constexpr FloatInst sse_float_inst[4] = {
  {
    Inst::kIdMovss,
    Inst::kIdMovaps,
    Inst::kIdAndps,
    Inst::kIdOrps,
    Inst::kIdXorps,
    Inst::kIdAndnps,
    Inst::kIdAddss,
    Inst::kIdSubss,
    Inst::kIdMulss,
    Inst::kIdDivss,
    Inst::kIdMinss,
    Inst::kIdMaxss,
    Inst::kIdCmpss,
    Inst::kIdRoundss,
    Inst::kIdPsrld,
    Inst::kIdPslld
  },
  {
    Inst::kIdMovsd,
    Inst::kIdMovaps,
    Inst::kIdAndpd,
    Inst::kIdOrpd,
    Inst::kIdXorpd,
    Inst::kIdAndnpd,
    Inst::kIdAddsd,
    Inst::kIdSubsd,
    Inst::kIdMulsd,
    Inst::kIdDivsd,
    Inst::kIdMinsd,
    Inst::kIdMaxsd,
    Inst::kIdCmpsd,
    Inst::kIdRoundsd,
    Inst::kIdPsrlq,
    Inst::kIdPsllq
  },
  {
    Inst::kIdMovaps,
    Inst::kIdMovaps,
    Inst::kIdAndps,
    Inst::kIdOrps,
    Inst::kIdXorps,
    Inst::kIdAndnps,
    Inst::kIdAddps,
    Inst::kIdSubps,
    Inst::kIdMulps,
    Inst::kIdDivps,
    Inst::kIdMinps,
    Inst::kIdMaxps,
    Inst::kIdCmpps,
    Inst::kIdRoundps,
    Inst::kIdPsrld,
    Inst::kIdPslld
  },
  {
    Inst::kIdMovaps,
    Inst::kIdMovaps,
    Inst::kIdAndpd,
    Inst::kIdOrpd,
    Inst::kIdXorpd,
    Inst::kIdAndnpd,
    Inst::kIdAddpd,
    Inst::kIdSubpd,
    Inst::kIdMulpd,
    Inst::kIdDivpd,
    Inst::kIdMinpd,
    Inst::kIdMaxpd,
    Inst::kIdCmppd,
    Inst::kIdRoundpd,
    Inst::kIdPsrlq,
    Inst::kIdPsllq
  }
};

static constexpr FloatInst avx_float_inst[4] = {
  {
    Inst::kIdVmovss,
    Inst::kIdVmovaps,
    Inst::kIdVandps,
    Inst::kIdVorps,
    Inst::kIdVxorps,
    Inst::kIdVandnps,
    Inst::kIdVaddss,
    Inst::kIdVsubss,
    Inst::kIdVmulss,
    Inst::kIdVdivss,
    Inst::kIdVminss,
    Inst::kIdVmaxss,
    Inst::kIdVcmpss,
    Inst::kIdVroundss,
    Inst::kIdVpsrld,
    Inst::kIdVpslld
  },
  {
    Inst::kIdVmovsd,
    Inst::kIdVmovaps,
    Inst::kIdVandpd,
    Inst::kIdVorpd,
    Inst::kIdVxorpd,
    Inst::kIdVandnpd,
    Inst::kIdVaddsd,
    Inst::kIdVsubsd,
    Inst::kIdVmulsd,
    Inst::kIdVdivsd,
    Inst::kIdVminsd,
    Inst::kIdVmaxsd,
    Inst::kIdVcmpsd,
    Inst::kIdVroundsd,
    Inst::kIdVpsrlq,
    Inst::kIdVpsllq
  },
  {
    Inst::kIdVmovaps,
    Inst::kIdVmovaps,
    Inst::kIdVandps,
    Inst::kIdVorps,
    Inst::kIdVxorps,
    Inst::kIdVandnps,
    Inst::kIdVaddps,
    Inst::kIdVsubps,
    Inst::kIdVmulps,
    Inst::kIdVdivps,
    Inst::kIdVminps,
    Inst::kIdVmaxps,
    Inst::kIdVcmpps,
    Inst::kIdVroundps,
    Inst::kIdVpsrld,
    Inst::kIdVpslld
  },
  {
    Inst::kIdVmovaps,
    Inst::kIdVmovaps,
    Inst::kIdVandpd,
    Inst::kIdVorpd,
    Inst::kIdVxorpd,
    Inst::kIdVandnpd,
    Inst::kIdVaddpd,
    Inst::kIdVsubpd,
    Inst::kIdVmulpd,
    Inst::kIdVdivpd,
    Inst::kIdVminpd,
    Inst::kIdVmaxpd,
    Inst::kIdVcmppd,
    Inst::kIdVroundpd,
    Inst::kIdVpsrlq,
    Inst::kIdVpsllq
  }
};

// bl::Pipeline::PipeCompiler - Vector Instructions - Opcode Information
// =====================================================================

struct OpcodeVInfo {
  //! \name Members
  //! \{

  uint32_t sseInstId   : 13;
  uint32_t sseOpCount  : 3;
  uint32_t sseExt      : 3;
  uint32_t avxInstId   : 13;
  uint32_t avxExt      : 6;
  uint32_t commutative : 1;
  uint32_t comparison  : 1;
  uint32_t sameVecOp   : 3;
  uint32_t useImm      : 1;
  uint32_t imm         : 8;
  uint32_t floatMode   : 3;
  uint32_t elementSize : 2;
  uint32_t bcstSize    : 4;
  uint32_t hi          : 1;
  uint32_t reserved    : 3;

  //! \}
};

#define DEFINE_OP(sseInstId, sseOpCount, sseExt, avxInstId, avxExt, commutative, comparison, sameVecOp, useImm, imm, floatMode, elementSize, bcstSize, vecPart) \
  OpcodeVInfo {                        \
    Inst::sseInstId,                   \
    sseOpCount,                        \
    uint8_t(SSEExt::sseExt),           \
    Inst::avxInstId,                   \
    uint8_t(AVXExt::avxExt),           \
    commutative,                       \
    comparison,                        \
    uint8_t(SameVecOp::sameVecOp),     \
    useImm,                            \
    imm,                               \
    uint8_t(FloatMode::floatMode),     \
    uint8_t(ElementSize::elementSize), \
    bcstSize,                          \
    uint8_t(VecPart::vecPart),         \
    0                                  \
  }

static constexpr OpcodeVInfo opcodeInfo2V[size_t(OpcodeVV::kMaxValue) + 1] = {
  DEFINE_OP(kIdMovaps     , 0, kIntrin, kIdVmovaps        , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kMov.
  DEFINE_OP(kIdMovq       , 0, kIntrin, kIdVmovq          , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kMovU64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpbroadcastb   , kIntrin     , 0, 0, kNone, 0, 0x01u, kNone, k8 , 0, kNA), // kBroadcastU8Z.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpbroadcastw   , kIntrin     , 0, 0, kNone, 0, 0x01u, kNone, k16, 0, kNA), // kBroadcastU16Z.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpbroadcastb   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kBroadcastU8.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpbroadcastw   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kBroadcastU16.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpbroadcastd   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kBroadcastU32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpbroadcastq   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kBroadcastU64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVbroadcastss   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kBroadcastF32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVbroadcastsd   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kBroadcastF64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVbroadcasti32x4, kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kBroadcastV128_U32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVbroadcasti64x2, kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kBroadcastV128_U64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVbroadcastf32x4, kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kBroadcastV128_F32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVbroadcastf64x2, kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kBroadcastV128_F64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVbroadcasti32x8, kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kBroadcastV256_U32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVbroadcasti64x4, kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kBroadcastV256_U64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVbroadcasti32x8, kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kBroadcastV256_F32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVbroadcasti64x4, kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kBroadcastV256_F64.
  DEFINE_OP(kIdPabsb      , 2, kSSSE3 , kIdVpabsb         , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kAbsI8.
  DEFINE_OP(kIdPabsw      , 2, kSSSE3 , kIdVpabsw         , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kAbsI16.
  DEFINE_OP(kIdPabsd      , 2, kSSSE3 , kIdVpabsd         , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kAbsI32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpabsq         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kAbsI64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kNotU32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kNotU64.
  DEFINE_OP(kIdPmovsxbw   , 0, kIntrin, kIdVpmovsxbw      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kCvtI8LoToI16.
  DEFINE_OP(kIdPmovsxbw   , 0, kIntrin, kIdVpmovsxbw      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kCvtI8HiToI16.
  DEFINE_OP(kIdPmovzxbw   , 0, kIntrin, kIdVpmovzxbw      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kCvtU8LoToU16.
  DEFINE_OP(kIdPmovzxbw   , 0, kIntrin, kIdVpmovzxbw      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kCvtU8HiToU16.
  DEFINE_OP(kIdPmovsxbd   , 0, kIntrin, kIdVpmovsxbd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kCvtI8ToI32.
  DEFINE_OP(kIdPmovzxbd   , 0, kIntrin, kIdVpmovzxbd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kCvtU8ToU32.
  DEFINE_OP(kIdPmovsxwd   , 0, kIntrin, kIdVpmovsxwd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kCvtI16LoToI32.
  DEFINE_OP(kIdPmovsxwd   , 0, kIntrin, kIdVpmovsxwd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kCvtI16HiToI32.
  DEFINE_OP(kIdPmovzxwd   , 0, kIntrin, kIdVpmovzxwd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kCvtU16LoToU32.
  DEFINE_OP(kIdPmovzxwd   , 0, kIntrin, kIdVpmovzxwd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kCvtU16HiToU32.
  DEFINE_OP(kIdPmovsxdq   , 0, kIntrin, kIdVpmovsxdq      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kCvtI32LoToI64.
  DEFINE_OP(kIdPmovsxdq   , 0, kIntrin, kIdVpmovsxdq      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kCvtI32HiToI64.
  DEFINE_OP(kIdPmovzxdq   , 0, kIntrin, kIdVpmovzxdq      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kCvtU32LoToU64.
  DEFINE_OP(kIdPmovzxdq   , 0, kIntrin, kIdVpmovzxdq      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kCvtU32HiToU64.
  DEFINE_OP(kIdAndps      , 0, kIntrin, kIdVandps         , kIntrin     , 0, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kAbsF32.
  DEFINE_OP(kIdAndpd      , 0, kIntrin, kIdVandpd         , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k64, 8, kNA), // kAbsF64.
  DEFINE_OP(kIdXorps      , 0, kIntrin, kIdVxorps         , kIntrin     , 0, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kNegF32.
  DEFINE_OP(kIdXorpd      , 0, kIntrin, kIdVxorpd         , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k64, 8, kNA), // kNegF64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kAbsU32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kAbsU64.
  DEFINE_OP(kIdRoundss    , 2, kIntrin, kIdVroundss       , kIntrin     , 0, 0, kNone, 1, 0x0Bu, kF32S, k32, 4, kNA), // kTruncF32S.
  DEFINE_OP(kIdRoundsd    , 2, kIntrin, kIdVroundsd       , kIntrin     , 0, 0, kNone, 1, 0x0Bu, kF64S, k64, 8, kNA), // kTruncF64S.
  DEFINE_OP(kIdRoundps    , 2, kIntrin, kIdVroundps       , kIntrin     , 0, 0, kNone, 1, 0x0Bu, kF32V, k32, 4, kNA), // kTruncF32.
  DEFINE_OP(kIdRoundpd    , 2, kIntrin, kIdVroundpd       , kIntrin     , 0, 0, kNone, 1, 0x0Bu, kF64V, k64, 8, kNA), // kTruncF64.
  DEFINE_OP(kIdRoundss    , 2, kIntrin, kIdVroundss       , kIntrin     , 0, 0, kNone, 1, 0x09u, kF32S, k32, 4, kNA), // kFloorF32S.
  DEFINE_OP(kIdRoundsd    , 2, kIntrin, kIdVroundsd       , kIntrin     , 0, 0, kNone, 1, 0x09u, kF64S, k64, 8, kNA), // kFloorF64S.
  DEFINE_OP(kIdRoundps    , 2, kIntrin, kIdVroundps       , kIntrin     , 0, 0, kNone, 1, 0x09u, kF32V, k32, 4, kNA), // kFloorF32.
  DEFINE_OP(kIdRoundpd    , 2, kIntrin, kIdVroundpd       , kIntrin     , 0, 0, kNone, 1, 0x09u, kF64V, k64, 8, kNA), // kFloorF64.
  DEFINE_OP(kIdRoundss    , 2, kIntrin, kIdVroundss       , kIntrin     , 0, 0, kNone, 1, 0x0Au, kF32S, k32, 4, kNA), // kCeilF32S.
  DEFINE_OP(kIdRoundsd    , 2, kIntrin, kIdVroundsd       , kIntrin     , 0, 0, kNone, 1, 0x0Au, kF64S, k64, 8, kNA), // kCeilF64S.
  DEFINE_OP(kIdRoundps    , 2, kIntrin, kIdVroundps       , kIntrin     , 0, 0, kNone, 1, 0x0Au, kF32V, k32, 4, kNA), // kCeilF32.
  DEFINE_OP(kIdRoundpd    , 2, kIntrin, kIdVroundpd       , kIntrin     , 0, 0, kNone, 1, 0x0Au, kF64V, k64, 8, kNA), // kCeilF64.
  DEFINE_OP(kIdRoundss    , 2, kIntrin, kIdVroundss       , kIntrin     , 0, 0, kNone, 1, 0x08u, kF32S, k32, 4, kNA), // kRoundF32S.
  DEFINE_OP(kIdRoundsd    , 2, kIntrin, kIdVroundsd       , kIntrin     , 0, 0, kNone, 1, 0x08u, kF64S, k64, 8, kNA), // kRoundF64S.
  DEFINE_OP(kIdRoundps    , 2, kIntrin, kIdVroundps       , kIntrin     , 0, 0, kNone, 1, 0x08u, kF32V, k32, 4, kNA), // kRoundF32.
  DEFINE_OP(kIdRoundpd    , 2, kIntrin, kIdVroundpd       , kIntrin     , 0, 0, kNone, 1, 0x08u, kF64V, k64, 8, kNA), // kRoundF64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kRcpF32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kRcpF64.
  DEFINE_OP(kIdSqrtss     , 2, kIntrin, kIdVsqrtss        , kIntrin     , 0, 0, kNone, 0, 0x00u, kF32S, k32, 4, kNA), // kSqrtF32S.
  DEFINE_OP(kIdSqrtsd     , 2, kIntrin, kIdVsqrtsd        , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64S, k64, 8, kNA), // kSqrtF64S.
  DEFINE_OP(kIdSqrtps     , 2, kSSE2  , kIdVsqrtps        , kAVX        , 0, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kSqrtF32.
  DEFINE_OP(kIdSqrtpd     , 2, kSSE2  , kIdVsqrtpd        , kAVX        , 0, 0, kNone, 0, 0x00u, kF64V, k64, 8, kNA), // kSqrtF64.
  DEFINE_OP(kIdCvtss2sd   , 2, kIntrin, kIdVcvtss2sd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64S, k64, 0, kNA), // kCvtF32ToF64S.
  DEFINE_OP(kIdCvtsd2ss   , 2, kIntrin, kIdVcvtsd2ss      , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64S, k32, 0, kNA), // kCvtF64ToF32S.
  DEFINE_OP(kIdCvtdq2ps   , 2, kSSE2  , kIdVcvtdq2ps      , kAVX        , 0, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kCvtI32ToF32.
  DEFINE_OP(kIdCvtps2pd   , 2, kSSE2  , kIdVcvtps2pd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kF32V, k64, 4, kLo), // kCvtF32LoToF64.
  DEFINE_OP(kIdCvtps2pd   , 2, kIntrin, kIdVcvtps2pd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kF32V, k64, 4, kHi), // kCvtF32HiToF64.
  DEFINE_OP(kIdCvtpd2ps   , 2, kSSE2  , kIdVcvtpd2ps      , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k32, 4, kLo), // kCvtF64ToF32Lo.
  DEFINE_OP(kIdCvtpd2ps   , 2, kIntrin, kIdVcvtpd2ps      , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k32, 4, kHi), // kCvtF64ToF32Hi.
  DEFINE_OP(kIdCvtdq2pd   , 2, kSSE2  , kIdVcvtdq2pd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 4, kLo), // kCvtI32LoToF64.
  DEFINE_OP(kIdCvtdq2pd   , 2, kIntrin, kIdVcvtdq2pd      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 4, kHi), // kCvtI32HiToF64.
  DEFINE_OP(kIdCvttps2dq  , 2, kSSE2  , kIdVcvttps2dq     , kAVX        , 0, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kCvtTruncF32ToI32.
  DEFINE_OP(kIdCvttpd2dq  , 2, kSSE2  , kIdVcvttpd2dq     , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k32, 4, kLo), // kCvtTruncF64ToI32Lo.
  DEFINE_OP(kIdCvttpd2dq  , 2, kIntrin, kIdVcvttpd2dq     , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k32, 4, kHi), // kCvtTruncF64ToI32Hi.
  DEFINE_OP(kIdCvtps2dq   , 2, kSSE2  , kIdVcvtps2dq      , kAVX        , 0, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kCvtRoundF32ToI32.
  DEFINE_OP(kIdCvtpd2dq   , 2, kSSE2  , kIdVcvtpd2dq      , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k32, 4, kLo), // kCvtRoundF64ToI32Lo.
  DEFINE_OP(kIdCvtpd2dq   , 2, kIntrin, kIdVcvtpd2dq      , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k32, 4, kHi)  // kCvtRoundF64ToI32Hi.
};

static constexpr OpcodeVInfo opcodeInfo2VS[size_t(OpcodeVR::kMaxValue) + 1] = {
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kMov.
  DEFINE_OP(kIdMovd       , 0, kSSE2  , kIdVmovd          , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kMovU32.
  DEFINE_OP(kIdMovq       , 0, kSSE2  , kIdVmovq          , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kMovU64.
  DEFINE_OP(kIdPinsrb     , 0, kSSE4_1, kIdVpinsrb        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kInsertU8.
  DEFINE_OP(kIdPinsrw     , 0, kSSE2  , kIdVpinsrw        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kInsertU16.
  DEFINE_OP(kIdPinsrd     , 0, kSSE4_1, kIdVpinsrd        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kInsertU32.
  DEFINE_OP(kIdPinsrq     , 0, kSSE4_1, kIdVpinsrq        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kInsertU64.
  DEFINE_OP(kIdPextrb     , 0, kSSE4_1, kIdVpextrb        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kExtractU8.
  DEFINE_OP(kIdPextrw     , 0, kSSE2  , kIdVpextrw        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kExtractU16.
  DEFINE_OP(kIdPextrd     , 0, kSSE4_1, kIdVpextrd        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kExtractU32.
  DEFINE_OP(kIdPextrq     , 0, kSSE4_1, kIdVpextrq        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kExtractU64.
  DEFINE_OP(kIdCvtsi2ss   , 0, kSSE2  , kIdVcvtsi2ss      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kCvtIntToF32.
  DEFINE_OP(kIdCvtsi2sd   , 0, kSSE2  , kIdVcvtsi2sd      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kCvtIntToF64.
  DEFINE_OP(kIdCvttss2si  , 0, kSSE2  , kIdVcvttss2si     , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kCvtTruncF32ToInt.
  DEFINE_OP(kIdCvtss2si   , 0, kSSE2  , kIdVcvtss2si      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kCvtRoundF32ToInt.
  DEFINE_OP(kIdCvttsd2si  , 0, kSSE2  , kIdVcvttsd2si     , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kCvtTruncF64ToInt.
  DEFINE_OP(kIdCvtsd2si   , 0, kSSE2  , kIdVcvtsd2si      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA)  // kCvtRoundF64ToInt.
};

static constexpr OpcodeVInfo opcodeInfo2VI[size_t(OpcodeVVI::kMaxValue) + 1] = {
  DEFINE_OP(kIdPsllw      , 2, kSSE2  , kIdVpsllw         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kSllU16.
  DEFINE_OP(kIdPslld      , 2, kSSE2  , kIdVpslld         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kSllU32.
  DEFINE_OP(kIdPsllq      , 2, kSSE2  , kIdVpsllq         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kSllU64.
  DEFINE_OP(kIdPsrlw      , 2, kSSE2  , kIdVpsrlw         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kSrlU16.
  DEFINE_OP(kIdPsrld      , 2, kSSE2  , kIdVpsrld         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kSrlU32.
  DEFINE_OP(kIdPsrlq      , 2, kSSE2  , kIdVpsrlq         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kSrlU64.
  DEFINE_OP(kIdPsraw      , 2, kSSE2  , kIdVpsraw         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kSraI16.
  DEFINE_OP(kIdPsrad      , 2, kSSE2  , kIdVpsrad         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kSraI32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpsraq         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kSraI64.
  DEFINE_OP(kIdPslldq     , 2, kSSE2  , kIdVpslldq        , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kSllbU128.
  DEFINE_OP(kIdPsrldq     , 2, kSSE2  , kIdVpsrldq        , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kSrlbU128.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kSwizzleU16x4 (intrin).
  DEFINE_OP(kIdPshuflw    , 3, kIntrin, kIdVpshuflw       , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kSwizzleLoU16x4.
  DEFINE_OP(kIdPshufhw    , 3, kIntrin, kIdVpshufhw       , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kSwizzleHiU16x4.
  DEFINE_OP(kIdPshufd     , 3, kIntrin, kIdVpshufd        , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kSwizzleU32x4.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kSwizzleU64x2 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kF32V, k32, 0, kNA), // kSwizzleF32x4 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k64, 0, kNA), // kSwizzleF64x2 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpermq         , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kSwizzleU64x4 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpermq         , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k64, 0, kNA), // kSwizzleF64x4 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kExtractV128_I32 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kExtractV128_I64 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kF32V, k64, 0, kNA), // kExtractV128_F32 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k64, 0, kNA), // kExtractV128_F64 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kExtractV256_I32 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kExtractV256_I64 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kF32V, k64, 0, kNA), // kExtractV256_F32 (intrin).
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k64, 0, kNA)  // kExtractV256_F64 (intrin).
};

static constexpr OpcodeVInfo opcodeInfo3V[size_t(OpcodeVVV::kMaxValue) + 1] = {
  DEFINE_OP(kIdPand       , 2, kSSE2  , kIdVpandd         , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k32, 4, kNA), // kAndU32.
  DEFINE_OP(kIdPand       , 2, kSSE2  , kIdVpandq         , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k64, 8, kNA), // kAndU64.
  DEFINE_OP(kIdPor        , 2, kSSE2  , kIdVpord          , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k32, 4, kNA), // kOrU32.
  DEFINE_OP(kIdPor        , 2, kSSE2  , kIdVporq          , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k64, 8, kNA), // kOrU64.
  DEFINE_OP(kIdPxor       , 2, kSSE2  , kIdVpxord         , kAVX        , 1, 0, kZero, 0, 0x00u, kNone, k32, 4, kNA), // kXorU32.
  DEFINE_OP(kIdPxor       , 2, kSSE2  , kIdVpxorq         , kAVX        , 1, 0, kZero, 0, 0x00u, kNone, k64, 8, kNA), // kXorU64.
  DEFINE_OP(kIdPandn      , 2, kSSE2  , kIdVpandnd        , kAVX        , 0, 0, kZero, 0, 0x00u, kNone, k32, 4, kNA), // kAndnU32.
  DEFINE_OP(kIdPandn      , 2, kSSE2  , kIdVpandnq        , kAVX        , 0, 0, kZero, 0, 0x00u, kNone, k64, 8, kNA), // kAndnU64.
  DEFINE_OP(kIdPandn      , 0, kIntrin, kIdVpandnd        , kIntrin     , 0, 0, kZero, 0, 0x00u, kNone, k32, 4, kNA), // kBicU32.
  DEFINE_OP(kIdPandn      , 0, kIntrin, kIdVpandnq        , kIntrin     , 0, 0, kZero, 0, 0x00u, kNone, k64, 8, kNA), // kBicU64.
  DEFINE_OP(kIdPavgb      , 2, kSSE2  , kIdVpavgb         , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k8 , 0, kNA), // kAvgrU8.
  DEFINE_OP(kIdPavgw      , 2, kSSE2  , kIdVpavgw         , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k16, 0, kNA), // kAvgrU16.
  DEFINE_OP(kIdPaddb      , 2, kSSE2  , kIdVpaddb         , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kAddU8.
  DEFINE_OP(kIdPaddw      , 2, kSSE2  , kIdVpaddw         , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kAddU16.
  DEFINE_OP(kIdPaddd      , 2, kSSE2  , kIdVpaddd         , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kAddU32.
  DEFINE_OP(kIdPaddq      , 2, kSSE2  , kIdVpaddq         , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kAddU64.
  DEFINE_OP(kIdPsubb      , 2, kSSE2  , kIdVpsubb         , kAVX        , 0, 0, kZero, 0, 0x00u, kNone, k8 , 0, kNA), // kSubU8.
  DEFINE_OP(kIdPsubw      , 2, kSSE2  , kIdVpsubw         , kAVX        , 0, 0, kZero, 0, 0x00u, kNone, k16, 0, kNA), // kSubU16.
  DEFINE_OP(kIdPsubd      , 2, kSSE2  , kIdVpsubd         , kAVX        , 0, 0, kZero, 0, 0x00u, kNone, k32, 4, kNA), // kSubU32.
  DEFINE_OP(kIdPsubq      , 2, kSSE2  , kIdVpsubq         , kAVX        , 0, 0, kZero, 0, 0x00u, kNone, k64, 8, kNA), // kSubU64.
  DEFINE_OP(kIdPaddsb     , 2, kSSE2  , kIdVpaddsb        , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kAddsI8.
  DEFINE_OP(kIdPaddusb    , 2, kSSE2  , kIdVpaddusb       , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kAddsU8.
  DEFINE_OP(kIdPaddsw     , 2, kSSE2  , kIdVpaddsw        , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kAddsI16.
  DEFINE_OP(kIdPaddusw    , 2, kSSE2  , kIdVpaddusw       , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kAddsU16.
  DEFINE_OP(kIdPsubsb     , 2, kSSE2  , kIdVpsubsb        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kSubsI8.
  DEFINE_OP(kIdPsubusb    , 2, kSSE2  , kIdVpsubusb       , kAVX        , 0, 0, kZero, 0, 0x00u, kNone, k8 , 0, kNA), // kSubsU8.
  DEFINE_OP(kIdPsubsw     , 2, kSSE2  , kIdVpsubsw        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kSubsI16.
  DEFINE_OP(kIdPsubusw    , 2, kSSE2  , kIdVpsubusw       , kAVX        , 0, 0, kZero, 0, 0x00u, kNone, k16, 0, kNA), // kSubsU16.
  DEFINE_OP(kIdPmullw     , 2, kSSE2  , kIdVpmullw        , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kMulU16.
  DEFINE_OP(kIdPmulld     , 2, kSSE4_1, kIdVpmulld        , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kMulU32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpmullq        , kAVX512     , 1, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kMulU64.
  DEFINE_OP(kIdPmulhw     , 2, kSSE2  , kIdVpmulhw        , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kMulhI16.
  DEFINE_OP(kIdPmulhuw    , 2, kSSE2  , kIdVpmulhuw       , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kMulhU16.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kMulU64_LoU32.
  DEFINE_OP(kIdPmaddwd    , 2, kSSE2  , kIdVpmaddwd       , kAVX        , 1, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kMHAddI16_I32.
  DEFINE_OP(kIdPminsb     , 2, kSSE4_1, kIdVpminsb        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k8 , 0, kNA), // kMinI8.
  DEFINE_OP(kIdPminub     , 2, kSSE2  , kIdVpminub        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k8 , 0, kNA), // kMinU8.
  DEFINE_OP(kIdPminsw     , 2, kSSE2  , kIdVpminsw        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k16, 0, kNA), // kMinI16.
  DEFINE_OP(kIdPminuw     , 2, kSSE4_1, kIdVpminuw        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k16, 0, kNA), // kMinU16.
  DEFINE_OP(kIdPminsd     , 2, kSSE4_1, kIdVpminsd        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k32, 4, kNA), // kMinI32.
  DEFINE_OP(kIdPminud     , 2, kSSE4_1, kIdVpminud        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k32, 4, kNA), // kMinU32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpminsq        , kAVX512     , 1, 0, kSrc , 0, 0x00u, kNone, k64, 8, kNA), // kMinI64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpminuq        , kAVX512     , 1, 0, kSrc , 0, 0x00u, kNone, k64, 8, kNA), // kMinU64.
  DEFINE_OP(kIdPmaxsb     , 2, kSSE4_1, kIdVpmaxsb        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k8 , 0, kNA), // kMaxI8.
  DEFINE_OP(kIdPmaxub     , 2, kSSE2  , kIdVpmaxub        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k8 , 0, kNA), // kMaxU8.
  DEFINE_OP(kIdPmaxsw     , 2, kSSE2  , kIdVpmaxsw        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k16, 0, kNA), // kMaxI16.
  DEFINE_OP(kIdPmaxuw     , 2, kSSE4_1, kIdVpmaxuw        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k16, 0, kNA), // kMaxU16.
  DEFINE_OP(kIdPmaxsd     , 2, kSSE4_1, kIdVpmaxsd        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k32, 4, kNA), // kMaxI32.
  DEFINE_OP(kIdPmaxud     , 2, kSSE4_1, kIdVpmaxud        , kAVX        , 1, 0, kSrc , 0, 0x00u, kNone, k32, 4, kNA), // kMaxU32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpmaxsq        , kAVX512     , 1, 0, kSrc , 0, 0x00u, kNone, k64, 8, kNA), // kMaxI64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpmaxuq        , kAVX512     , 1, 0, kSrc , 0, 0x00u, kNone, k64, 8, kNA), // kMaxU64.
  DEFINE_OP(kIdPcmpeqb    , 2, kSSE2  , kIdVpcmpeqb       , kAVX        , 1, 1, kOnes, 0, 0x00u, kNone, k8 , 0, kNA), // kCmpEqU8.
  DEFINE_OP(kIdPcmpeqw    , 2, kSSE2  , kIdVpcmpeqw       , kAVX        , 1, 1, kOnes, 0, 0x00u, kNone, k16, 0, kNA), // kCmpEqU16.
  DEFINE_OP(kIdPcmpeqd    , 2, kSSE2  , kIdVpcmpeqd       , kAVX        , 1, 1, kOnes, 0, 0x00u, kNone, k32, 4, kNA), // kCmpEqU32.
  DEFINE_OP(kIdPcmpeqq    , 2, kSSE4_1, kIdVpcmpeqq       , kAVX        , 1, 1, kOnes, 0, 0x00u, kNone, k64, 8, kNA), // kCmpEqU64.
  DEFINE_OP(kIdPcmpgtb    , 2, kSSE2  , kIdVpcmpgtb       , kAVX        , 0, 1, kZero, 0, 0x00u, kNone, k8 , 0, kNA), // kCmpGtI8.
  DEFINE_OP(kIdPcmpgtb    , 0, kIntrin, kIdVpcmpub        , kAVX512     , 0, 1, kZero, 1, 0x06u, kNone, k8 , 0, kNA), // kCmpGtU8.
  DEFINE_OP(kIdPcmpgtw    , 2, kSSE2  , kIdVpcmpgtw       , kAVX        , 0, 1, kZero, 0, 0x00u, kNone, k16, 0, kNA), // kCmpGtI16.
  DEFINE_OP(kIdPcmpgtw    , 0, kIntrin, kIdVpcmpuw        , kAVX512     , 0, 1, kZero, 1, 0x06u, kNone, k16, 0, kNA), // kCmpGtU16.
  DEFINE_OP(kIdPcmpgtd    , 2, kSSE2  , kIdVpcmpgtd       , kAVX        , 0, 1, kZero, 0, 0x00u, kNone, k32, 4, kNA), // kCmpGtI32.
  DEFINE_OP(kIdPcmpgtd    , 0, kIntrin, kIdVpcmpud        , kAVX512     , 0, 1, kZero, 1, 0x06u, kNone, k32, 4, kNA), // kCmpGtU32.
  DEFINE_OP(kIdPcmpgtq    , 2, kSSE4_2, kIdVpcmpgtq       , kAVX        , 0, 1, kZero, 0, 0x00u, kNone, k64, 8, kNA), // kCmpGtI64.
  DEFINE_OP(kIdPcmpgtq    , 0, kIntrin, kIdVpcmpuq        , kAVX512     , 0, 1, kZero, 1, 0x06u, kNone, k64, 8, kNA), // kCmpGtU64.
  DEFINE_OP(kIdPcmpgtb    , 0, kIntrin, kIdVpcmpb         , kAVX512     , 0, 1, kOnes, 1, 0x05u, kNone, k8 , 0, kNA), // kCmpGeI8.
  DEFINE_OP(kIdPcmpgtb    , 0, kIntrin, kIdVpcmpub        , kAVX512     , 0, 1, kOnes, 1, 0x05u, kNone, k8 , 0, kNA), // kCmpGeU8.
  DEFINE_OP(kIdPcmpgtw    , 0, kIntrin, kIdVpcmpw         , kAVX512     , 0, 1, kOnes, 1, 0x05u, kNone, k16, 0, kNA), // kCmpGeI16.
  DEFINE_OP(kIdPcmpgtw    , 0, kIntrin, kIdVpcmpuw        , kAVX512     , 0, 1, kOnes, 1, 0x05u, kNone, k16, 0, kNA), // kCmpGeU16.
  DEFINE_OP(kIdPcmpgtd    , 0, kIntrin, kIdVpcmpd         , kAVX512     , 0, 1, kOnes, 1, 0x05u, kNone, k32, 4, kNA), // kCmpGeI32.
  DEFINE_OP(kIdPcmpgtd    , 0, kIntrin, kIdVpcmpud        , kAVX512     , 0, 1, kOnes, 1, 0x05u, kNone, k32, 4, kNA), // kCmpGeU32.
  DEFINE_OP(kIdPcmpgtq    , 0, kIntrin, kIdVpcmpq         , kAVX512     , 0, 1, kOnes, 1, 0x05u, kNone, k64, 8, kNA), // kCmpGeI64.
  DEFINE_OP(kIdPcmpgtq    , 0, kIntrin, kIdVpcmpuq        , kAVX512     , 0, 1, kOnes, 1, 0x05u, kNone, k64, 8, kNA), // kCmpGeU64.
  DEFINE_OP(kIdPcmpgtb    , 0, kIntrin, kIdVpcmpb         , kAVX512     , 0, 1, kZero, 1, 0x01u, kNone, k8 , 0, kNA), // kCmpLtI8.
  DEFINE_OP(kIdPcmpgtb    , 0, kIntrin, kIdVpcmpub        , kAVX512     , 0, 1, kZero, 1, 0x01u, kNone, k8 , 0, kNA), // kCmpLtU8.
  DEFINE_OP(kIdPcmpgtw    , 0, kIntrin, kIdVpcmpw         , kAVX512     , 0, 1, kZero, 1, 0x01u, kNone, k16, 0, kNA), // kCmpLtI16.
  DEFINE_OP(kIdPcmpgtw    , 0, kIntrin, kIdVpcmpuw        , kAVX512     , 0, 1, kZero, 1, 0x01u, kNone, k16, 0, kNA), // kCmpLtU16.
  DEFINE_OP(kIdPcmpgtd    , 0, kIntrin, kIdVpcmpd         , kAVX512     , 0, 1, kZero, 1, 0x01u, kNone, k32, 4, kNA), // kCmpLtI32.
  DEFINE_OP(kIdPcmpgtd    , 0, kIntrin, kIdVpcmpud        , kAVX512     , 0, 1, kZero, 1, 0x01u, kNone, k32, 4, kNA), // kCmpLtU32.
  DEFINE_OP(kIdPcmpgtq    , 0, kIntrin, kIdVpcmpq         , kAVX512     , 0, 1, kZero, 1, 0x01u, kNone, k64, 8, kNA), // kCmpLtI64.
  DEFINE_OP(kIdPcmpgtq    , 0, kIntrin, kIdVpcmpuq        , kAVX512     , 0, 1, kZero, 1, 0x01u, kNone, k64, 8, kNA), // kCmpLtU64.
  DEFINE_OP(kIdPcmpgtb    , 0, kIntrin, kIdVpcmpb         , kAVX512     , 0, 1, kOnes, 1, 0x02u, kNone, k8 , 0, kNA), // kCmpLeI8.
  DEFINE_OP(kIdPcmpgtb    , 0, kIntrin, kIdVpcmpub        , kAVX512     , 0, 1, kOnes, 1, 0x02u, kNone, k8 , 0, kNA), // kCmpLeU8.
  DEFINE_OP(kIdPcmpgtw    , 0, kIntrin, kIdVpcmpw         , kAVX512     , 0, 1, kOnes, 1, 0x02u, kNone, k16, 0, kNA), // kCmpLeI16.
  DEFINE_OP(kIdPcmpgtw    , 0, kIntrin, kIdVpcmpuw        , kAVX512     , 0, 1, kOnes, 1, 0x02u, kNone, k16, 0, kNA), // kCmpLeU16.
  DEFINE_OP(kIdPcmpgtd    , 0, kIntrin, kIdVpcmpd         , kAVX512     , 0, 1, kOnes, 1, 0x02u, kNone, k32, 4, kNA), // kCmpLeI32.
  DEFINE_OP(kIdPcmpgtd    , 0, kIntrin, kIdVpcmpud        , kAVX512     , 0, 1, kOnes, 1, 0x02u, kNone, k32, 4, kNA), // kCmpLeU32.
  DEFINE_OP(kIdPcmpgtq    , 0, kIntrin, kIdVpcmpq         , kAVX512     , 0, 1, kOnes, 1, 0x02u, kNone, k64, 8, kNA), // kCmpLeI64.
  DEFINE_OP(kIdPcmpgtq    , 0, kIntrin, kIdVpcmpuq        , kAVX512     , 0, 1, kOnes, 1, 0x02u, kNone, k64, 8, kNA), // kCmpLeU64.
  DEFINE_OP(kIdAndps      , 2, kSSE2  , kIdVandps         , kAVX        , 1, 0, kSrc , 0, 0x00u, kF32V, k32, 4, kNA), // kAndF32.
  DEFINE_OP(kIdAndpd      , 2, kSSE2  , kIdVandpd         , kAVX        , 1, 0, kSrc , 0, 0x00u, kF64V, k64, 8, kNA), // kAndF64.
  DEFINE_OP(kIdOrps       , 2, kSSE2  , kIdVorps          , kAVX        , 1, 0, kSrc , 0, 0x00u, kF32V, k32, 4, kNA), // kOrF32.
  DEFINE_OP(kIdOrpd       , 2, kSSE2  , kIdVorpd          , kAVX        , 1, 0, kSrc , 0, 0x00u, kF64V, k64, 8, kNA), // kOrF64.
  DEFINE_OP(kIdXorps      , 2, kSSE2  , kIdVxorps         , kAVX        , 1, 0, kZero, 0, 0x00u, kF32V, k32, 4, kNA), // kXorF32.
  DEFINE_OP(kIdXorpd      , 2, kSSE2  , kIdVxorpd         , kAVX        , 1, 0, kZero, 0, 0x00u, kF64V, k64, 8, kNA), // kXorF64.
  DEFINE_OP(kIdAndnps     , 2, kSSE2  , kIdVandnps        , kAVX        , 0, 0, kZero, 0, 0x00u, kF32V, k32, 4, kNA), // kAndnF32.
  DEFINE_OP(kIdAndnpd     , 2, kSSE2  , kIdVandnpd        , kAVX        , 0, 0, kZero, 0, 0x00u, kF64V, k64, 8, kNA), // kAndnF64.
  DEFINE_OP(kIdAndnps     , 0, kIntrin, kIdVandnps        , kIntrin     , 0, 0, kZero, 0, 0x00u, kF32V, k32, 4, kNA), // kBicF32.
  DEFINE_OP(kIdAndnpd     , 0, kIntrin, kIdVandnpd        , kIntrin     , 0, 0, kZero, 0, 0x00u, kF64V, k64, 8, kNA), // kBicF64.
  DEFINE_OP(kIdAddss      , 2, kSSE2  , kIdVaddss         , kAVX        , 0, 0, kNone, 0, 0x00u, kF32S, k32, 4, kNA), // kAddF32S.
  DEFINE_OP(kIdAddsd      , 2, kSSE2  , kIdVaddsd         , kAVX        , 0, 0, kNone, 0, 0x00u, kF64S, k64, 8, kNA), // kAddF64S.
  DEFINE_OP(kIdAddps      , 2, kSSE2  , kIdVaddps         , kAVX        , 1, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kAddF32.
  DEFINE_OP(kIdAddpd      , 2, kSSE2  , kIdVaddpd         , kAVX        , 1, 0, kNone, 0, 0x00u, kF64V, k64, 8, kNA), // kAddF64.
  DEFINE_OP(kIdSubss      , 2, kSSE2  , kIdVsubss         , kAVX        , 0, 0, kNone, 0, 0x00u, kF32S, k32, 4, kNA), // kSubF32S.
  DEFINE_OP(kIdSubsd      , 2, kSSE2  , kIdVsubsd         , kAVX        , 0, 0, kNone, 0, 0x00u, kF64S, k64, 8, kNA), // kSubF64S.
  DEFINE_OP(kIdSubps      , 2, kSSE2  , kIdVsubps         , kAVX        , 0, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kSubF32.
  DEFINE_OP(kIdSubpd      , 2, kSSE2  , kIdVsubpd         , kAVX        , 0, 0, kNone, 0, 0x00u, kF64V, k64, 8, kNA), // kSubF64.
  DEFINE_OP(kIdMulss      , 2, kSSE2  , kIdVmulss         , kAVX        , 0, 0, kNone, 0, 0x00u, kF32S, k32, 4, kNA), // kMulF32S.
  DEFINE_OP(kIdMulsd      , 2, kSSE2  , kIdVmulsd         , kAVX        , 0, 0, kNone, 0, 0x00u, kF64S, k64, 8, kNA), // kMulF64S.
  DEFINE_OP(kIdMulps      , 2, kSSE2  , kIdVmulps         , kAVX        , 1, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kMulF32.
  DEFINE_OP(kIdMulpd      , 2, kSSE2  , kIdVmulpd         , kAVX        , 1, 0, kNone, 0, 0x00u, kF64V, k64, 8, kNA), // kMulF64.
  DEFINE_OP(kIdDivss      , 2, kSSE2  , kIdVdivss         , kAVX        , 0, 0, kNone, 0, 0x00u, kF32S, k32, 4, kNA), // kDivF32S.
  DEFINE_OP(kIdDivsd      , 2, kSSE2  , kIdVdivsd         , kAVX        , 0, 0, kNone, 0, 0x00u, kF64S, k64, 8, kNA), // kDivF64S.
  DEFINE_OP(kIdDivps      , 2, kSSE2  , kIdVdivps         , kAVX        , 0, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kDivF32.
  DEFINE_OP(kIdDivpd      , 2, kSSE2  , kIdVdivpd         , kAVX        , 0, 0, kNone, 0, 0x00u, kF64V, k64, 8, kNA), // kDivF64.
  DEFINE_OP(kIdMinss      , 2, kSSE2  , kIdVminss         , kAVX        , 0, 0, kSrc , 0, 0x00u, kF32S, k32, 4, kNA), // kMinF32S.
  DEFINE_OP(kIdMinsd      , 2, kSSE2  , kIdVminsd         , kAVX        , 0, 0, kSrc , 0, 0x00u, kF64S, k64, 8, kNA), // kMinF64S.
  DEFINE_OP(kIdMinps      , 2, kSSE2  , kIdVminps         , kAVX        , 0, 0, kSrc , 0, 0x00u, kF32V, k32, 4, kNA), // kMinF32.
  DEFINE_OP(kIdMinpd      , 2, kSSE2  , kIdVminpd         , kAVX        , 0, 0, kSrc , 0, 0x00u, kF64V, k64, 8, kNA), // kMinF64.
  DEFINE_OP(kIdMaxss      , 2, kSSE2  , kIdVmaxss         , kAVX        , 0, 0, kSrc , 0, 0x00u, kF32S, k32, 4, kNA), // kMaxF32S.
  DEFINE_OP(kIdMaxsd      , 2, kSSE2  , kIdVmaxsd         , kAVX        , 0, 0, kSrc , 0, 0x00u, kF64S, k64, 8, kNA), // kMaxF64S.
  DEFINE_OP(kIdMaxps      , 2, kSSE2  , kIdVmaxps         , kAVX        , 0, 0, kSrc , 0, 0x00u, kF32V, k32, 4, kNA), // kMaxF32.
  DEFINE_OP(kIdMaxpd      , 2, kSSE2  , kIdVmaxpd         , kAVX        , 0, 0, kSrc , 0, 0x00u, kF64V, k64, 8, kNA), // kMaxF64.
  DEFINE_OP(kIdCmpss      , 2, kIntrin, kIdVcmpss         , kAVX        , 1, 1, kNone, 1, 0x00u, kF32S, k32, 4, kNA), // kCmpEqF32S    (eq ordered quiet).
  DEFINE_OP(kIdCmpsd      , 2, kIntrin, kIdVcmpsd         , kAVX        , 1, 1, kNone, 1, 0x00u, kF64S, k64, 8, kNA), // kCmpEqF64S    (eq ordered quiet).
  DEFINE_OP(kIdCmpps      , 2, kIntrin, kIdVcmpps         , kAVX        , 1, 1, kNone, 1, 0x00u, kF32V, k32, 4, kNA), // kCmpEqF32     (eq ordered quiet).
  DEFINE_OP(kIdCmppd      , 2, kIntrin, kIdVcmppd         , kAVX        , 1, 1, kNone, 1, 0x00u, kF64V, k64, 8, kNA), // kCmpEqF64     (eq ordered quiet).
  DEFINE_OP(kIdCmpss      , 2, kIntrin, kIdVcmpss         , kAVX        , 1, 1, kNone, 1, 0x04u, kF32S, k32, 4, kNA), // kCmpNeF32S    (ne unordered quiet).
  DEFINE_OP(kIdCmpsd      , 2, kIntrin, kIdVcmpsd         , kAVX        , 1, 1, kNone, 1, 0x04u, kF64S, k64, 8, kNA), // kCmpNeF64S    (ne unordered quiet).
  DEFINE_OP(kIdCmpps      , 2, kIntrin, kIdVcmpps         , kAVX        , 1, 1, kNone, 1, 0x04u, kF32V, k32, 4, kNA), // kCmpNeF32     (ne unordered quiet).
  DEFINE_OP(kIdCmppd      , 2, kIntrin, kIdVcmppd         , kAVX        , 1, 1, kNone, 1, 0x04u, kF64V, k64, 8, kNA), // kCmpNeF64     (ne unordered quiet).
  DEFINE_OP(kIdCmpss      , 2, kIntrin, kIdVcmpss         , kAVX        , 0, 1, kNone, 1, 0x1Eu, kF32S, k32, 4, kNA), // kCmpGtF32S    (gt ordered quiet).
  DEFINE_OP(kIdCmpsd      , 2, kIntrin, kIdVcmpsd         , kAVX        , 0, 1, kNone, 1, 0x1Eu, kF64S, k64, 8, kNA), // kCmpGtF64S    (gt ordered quiet).
  DEFINE_OP(kIdCmpps      , 2, kIntrin, kIdVcmpps         , kAVX        , 0, 1, kNone, 1, 0x1Eu, kF32V, k32, 4, kNA), // kCmpGtF32     (gt ordered quiet).
  DEFINE_OP(kIdCmppd      , 2, kIntrin, kIdVcmppd         , kAVX        , 0, 1, kNone, 1, 0x1Eu, kF64V, k64, 8, kNA), // kCmpGtF64     (gt ordered quiet).
  DEFINE_OP(kIdCmpss      , 2, kIntrin, kIdVcmpss         , kAVX        , 0, 1, kNone, 1, 0x1Du, kF32S, k32, 4, kNA), // kCmpGeF32S    (ge ordered quiet).
  DEFINE_OP(kIdCmpsd      , 2, kIntrin, kIdVcmpsd         , kAVX        , 0, 1, kNone, 1, 0x1Du, kF64S, k64, 8, kNA), // kCmpGeF64S    (ge ordered quiet).
  DEFINE_OP(kIdCmpps      , 2, kIntrin, kIdVcmpps         , kAVX        , 0, 1, kNone, 1, 0x1Du, kF32V, k32, 4, kNA), // kCmpGeF32     (ge ordered quiet).
  DEFINE_OP(kIdCmppd      , 2, kIntrin, kIdVcmppd         , kAVX        , 0, 1, kNone, 1, 0x1Du, kF64V, k64, 8, kNA), // kCmpGeF64     (ge ordered quiet).
  DEFINE_OP(kIdCmpss      , 2, kIntrin, kIdVcmpss         , kAVX        , 0, 1, kNone, 1, 0x11u, kF32S, k32, 4, kNA), // kCmpLtF32S    (lt ordered quiet).
  DEFINE_OP(kIdCmpsd      , 2, kIntrin, kIdVcmpsd         , kAVX        , 0, 1, kNone, 1, 0x11u, kF64S, k64, 8, kNA), // kCmpLtF64S    (lt ordered quiet).
  DEFINE_OP(kIdCmpps      , 2, kIntrin, kIdVcmpps         , kAVX        , 0, 1, kNone, 1, 0x11u, kF32V, k32, 4, kNA), // kCmpLtF32     (lt ordered quiet).
  DEFINE_OP(kIdCmppd      , 2, kIntrin, kIdVcmppd         , kAVX        , 0, 1, kNone, 1, 0x11u, kF64V, k64, 8, kNA), // kCmpLtF64     (lt ordered quiet).
  DEFINE_OP(kIdCmpss      , 2, kIntrin, kIdVcmpss         , kAVX        , 0, 1, kNone, 1, 0x12u, kF32S, k32, 4, kNA), // kCmpLeF32S    (le ordered quiet).
  DEFINE_OP(kIdCmpsd      , 2, kIntrin, kIdVcmpsd         , kAVX        , 0, 1, kNone, 1, 0x12u, kF64S, k64, 8, kNA), // kCmpLeF64S    (le ordered quiet).
  DEFINE_OP(kIdCmpps      , 2, kIntrin, kIdVcmpps         , kAVX        , 0, 1, kNone, 1, 0x12u, kF32V, k32, 4, kNA), // kCmpLeF32     (le ordered quiet).
  DEFINE_OP(kIdCmppd      , 2, kIntrin, kIdVcmppd         , kAVX        , 0, 1, kNone, 1, 0x12u, kF64V, k64, 8, kNA), // kCmpLeF64     (le ordered quiet).
  DEFINE_OP(kIdCmpss      , 2, kIntrin, kIdVcmpss         , kAVX        , 1, 1, kNone, 1, 0x07u, kF32S, k32, 4, kNA), // kCmpOrdF32S   (ordered quiet).
  DEFINE_OP(kIdCmpsd      , 2, kIntrin, kIdVcmpsd         , kAVX        , 1, 1, kNone, 1, 0x07u, kF64S, k64, 8, kNA), // kCmpOrdF64S   (ordered quiet).
  DEFINE_OP(kIdCmpps      , 2, kIntrin, kIdVcmpps         , kAVX        , 1, 1, kNone, 1, 0x07u, kF32V, k32, 4, kNA), // kCmpOrdF32    (ordered quiet).
  DEFINE_OP(kIdCmppd      , 2, kIntrin, kIdVcmppd         , kAVX        , 1, 1, kNone, 1, 0x07u, kF64V, k64, 8, kNA), // kCmpOrdF64    (ordered quiet).
  DEFINE_OP(kIdCmpss      , 2, kIntrin, kIdVcmpss         , kAVX        , 1, 1, kNone, 1, 0x03u, kF32S, k32, 4, kNA), // kCmpUnordF32S (unordered quiet).
  DEFINE_OP(kIdCmpsd      , 2, kIntrin, kIdVcmpsd         , kAVX        , 1, 1, kNone, 1, 0x03u, kF64S, k64, 8, kNA), // kCmpUnordF64S (unordered quiet).
  DEFINE_OP(kIdCmpps      , 2, kIntrin, kIdVcmpps         , kAVX        , 1, 1, kNone, 1, 0x03u, kF32V, k32, 4, kNA), // kCmpUnordF32  (unordered quiet).
  DEFINE_OP(kIdCmppd      , 2, kIntrin, kIdVcmppd         , kAVX        , 1, 1, kNone, 1, 0x03u, kF64V, k64, 8, kNA), // kCmpUnordF64  (unordered quiet).
  DEFINE_OP(kIdHaddpd     , 2, kSSE3  , kIdVhaddpd        , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k64, 0, kNA), // kHAddF64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kCombineLoHiU64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA), // kCombineLoHiF64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kSrc , 0, 0x00u, kNone, k64, 0, kNA), // kCombineHiLoU64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kSrc , 0, 0x00u, kNone, k64, 0, kNA), // kCombineHiLoF64.
  DEFINE_OP(kIdPunpcklbw  , 2, kSSE2  , kIdVpunpcklbw     , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kLo), // kInterleaveLoU8.
  DEFINE_OP(kIdPunpckhbw  , 2, kSSE2  , kIdVpunpckhbw     , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kHi), // kInterleaveHiU8.
  DEFINE_OP(kIdPunpcklwd  , 2, kSSE2  , kIdVpunpcklwd     , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kLo), // kInterleaveLoU16.
  DEFINE_OP(kIdPunpckhwd  , 2, kSSE2  , kIdVpunpckhwd     , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kHi), // kInterleaveHiU16.
  DEFINE_OP(kIdPunpckldq  , 2, kSSE2  , kIdVpunpckldq     , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kLo), // kInterleaveLoU32.
  DEFINE_OP(kIdPunpckhdq  , 2, kSSE2  , kIdVpunpckhdq     , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kHi), // kInterleaveHiU32.
  DEFINE_OP(kIdPunpcklqdq , 2, kSSE2  , kIdVpunpcklqdq    , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kLo), // kInterleaveLoU64.
  DEFINE_OP(kIdPunpckhqdq , 2, kSSE2  , kIdVpunpckhqdq    , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kHi), // kInterleaveHiU64.
  DEFINE_OP(kIdUnpcklps   , 2, kSSE2  , kIdVunpcklps      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kLo), // kInterleaveLoF32.
  DEFINE_OP(kIdUnpckhps   , 2, kSSE2  , kIdVunpckhps      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kHi), // kInterleaveHiF32.
  DEFINE_OP(kIdUnpcklpd   , 2, kSSE2  , kIdVunpcklpd      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kLo), // kInterleaveLoF64.
  DEFINE_OP(kIdUnpckhpd   , 2, kSSE2  , kIdVunpckhpd      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kHi), // kInterleaveHiF64.
  DEFINE_OP(kIdPacksswb   , 2, kSSE2  , kIdVpacksswb      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kPacksI16_I8.
  DEFINE_OP(kIdPackuswb   , 2, kSSE2  , kIdVpackuswb      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kPacksI16_U8.
  DEFINE_OP(kIdPackssdw   , 2, kSSE2  , kIdVpackssdw      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kPacksI32_I16.
  DEFINE_OP(kIdPackusdw   , 2, kSSE4_1, kIdVpackusdw      , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kPacksI32_U16.
  DEFINE_OP(kIdPshufb     , 2, kSSSE3 , kIdVpshufb        , kAVX        , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kSwizzlev_U8.

  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpermb         , kAVX512_VBMI, 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kPermuteU8.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpermw         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kPermuteU16.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpermd         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k32, 0, kNA), // kPermuteU32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVpermq         , kAVX512     , 0, 0, kNone, 0, 0x00u, kNone, k64, 0, kNA)  // kPermuteU64.
};

static constexpr OpcodeVInfo opcodeInfo3VI[size_t(OpcodeVVVI::kMaxValue) + 1] = {
  DEFINE_OP(kIdPalignr    , 2, kIntrin, kIdVpalignr       , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kAlignr_U128.
  DEFINE_OP(kIdShufps     , 2, kIntrin, kIdVshufps        , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kInterleaveShuffleU32x4.
  DEFINE_OP(kIdShufpd     , 2, kIntrin, kIdVshufpd        , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kInterleaveShuffleU64x2.
  DEFINE_OP(kIdShufps     , 2, kIntrin, kIdVshufps        , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kInterleaveShuffleF32x4.
  DEFINE_OP(kIdShufpd     , 2, kIntrin, kIdVshufpd        , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k64, 8, kNA), // kInterleaveShuffleF64x2.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVinserti32x4   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kInsertV128_U32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVinserti64x2   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kInsertV128_F32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVinsertf32x4   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kInsertV128_U64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVinsertf64x2   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kInsertV128_F64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVinserti32x8   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kInsertV256_U32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVinsertf32x8   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kInsertV256_F32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVinserti64x4   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kInsertV256_U64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdVinsertf64x4   , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA)  // kInsertV256_F64.
};

static constexpr OpcodeVInfo opcodeInfo4V[size_t(OpcodeVVV::kMaxValue) + 1] = {
  DEFINE_OP(kIdPblendvb   , 0, kIntrin, kIdVpblendvb      , kIntrin     , 0, 0, kNone, 0, 0x00u, kNone, k8 , 0, kNA), // kBlendV_U8.
  DEFINE_OP(kIdPmullw     , 0, kIntrin, kIdVpmullw        , kIntrin     , 1, 0, kNone, 0, 0x00u, kNone, k16, 0, kNA), // kMAddU16.
  DEFINE_OP(kIdPmulld     , 0, kIntrin, kIdVpmulld        , kIntrin     , 1, 0, kNone, 0, 0x00u, kNone, k32, 4, kNA), // kMAddU32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kF32S, k32, 4, kNA), // kMAddF32S.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64S, k64, 8, kNA), // kMAddF64S.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kF32V, k32, 4, kNA), // kMAddF32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x00u, kF64V, k64, 8, kNA), // kMAddF64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x01u, kF32S, k32, 4, kNA), // kMSubF32S.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x01u, kF64S, k64, 8, kNA), // kMSubF64S.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x01u, kF32V, k32, 4, kNA), // kMSubF32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x01u, kF64V, k64, 8, kNA), // kMSubF64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x02u, kF32S, k32, 4, kNA), // kNMAddF32S.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x02u, kF64S, k64, 8, kNA), // kNMAddF64S.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x02u, kF32V, k32, 4, kNA), // kNMAddF32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x02u, kF64V, k64, 8, kNA), // kNMAddF64.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x03u, kF32S, k32, 4, kNA), // kNMSubF32S.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x03u, kF64S, k64, 8, kNA), // kNMSubF64S.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x03u, kF32V, k32, 4, kNA), // kNMSubF32.
  DEFINE_OP(kIdNone       , 0, kIntrin, kIdNone           , kIntrin     , 0, 0, kNone, 0, 0x03u, kF64V, k64, 8, kNA)  // kNMSubF64.
};

#undef DEFINE_OP

struct OpcodeVMInfo {
  //! \name Members
  //! \{

  uint32_t sseInstId    : 13;
  uint32_t avxInstId    : 13;
  uint32_t reserved1    : 6;
  uint32_t cvt          : 5;
  uint32_t memSize      : 8;
  uint32_t memSizeShift : 3;
  uint32_t reserved2    : 3;

  //! \}
};

#define DEFINE_OP(sseInstId, avxInstId, cvt, memSize, memSizeShift) \
  OpcodeVMInfo {                                                    \
    Inst::sseInstId,                                                \
    Inst::avxInstId,                                                \
    0,                                                              \
    uint8_t(WideningOp::cvt),                                       \
    memSize,                                                        \
    memSizeShift,                                                   \
    0                                                               \
  }

static constexpr OpcodeVMInfo opcodeInfo2VM[size_t(OpcodeVM::kMaxValue) + 1] = {
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  1, 0), // kLoad8.
  DEFINE_OP(kIdNone          , kIdVmovsh         , kNone    ,  2, 0), // kLoad16_U16.
  DEFINE_OP(kIdMovd          , kIdVmovd          , kNone    ,  4, 0), // kLoad32_U32.
  DEFINE_OP(kIdMovss         , kIdVmovss         , kNone    ,  4, 0), // kLoad32_F32.
  DEFINE_OP(kIdMovq          , kIdVmovq          , kNone    ,  8, 0), // kLoad64_U32.
  DEFINE_OP(kIdMovq          , kIdVmovq          , kNone    ,  8, 0), // kLoad64_U64.
  DEFINE_OP(kIdMovq          , kIdVmovq          , kNone    ,  8, 0), // kLoad64_F32.
  DEFINE_OP(kIdMovsd         , kIdVmovsd         , kNone    ,  8, 0), // kLoad64_F64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 16, 0), // kLoad128_U32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 16, 0), // kLoad128_U64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 16, 0), // kLoad128_F32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 16, 0), // kLoad128_F64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 32, 0), // kLoad256_U32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 32, 0), // kLoad256_U64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 32, 0), // kLoad256_F32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 32, 0), // kLoad256_F64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 64, 0), // kLoad512_U32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 64, 0), // kLoad512_U64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 64, 0), // kLoad512_F32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 64, 0), // kLoad512_F64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  0, 0), // kLoadN_U32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  0, 0), // kLoadN_U64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  0, 0), // kLoadN_F32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  0, 0), // kLoadN_F64.
  DEFINE_OP(kIdPmovzxbq      , kIdVpmovzxbq      , kU8ToU64 ,  2, 3), // kLoadCvt16_U8ToU64.
  DEFINE_OP(kIdPmovzxbq      , kIdVpmovzxbq      , kU8ToU64 ,  4, 3), // kLoadCvt32_U8ToU64.
  DEFINE_OP(kIdPmovzxbq      , kIdVpmovzxbq      , kU8ToU64 ,  8, 3), // kLoadCvt64_U8ToU64.
  DEFINE_OP(kIdPmovsxbw      , kIdVpmovsxbw      , kI8ToI16 ,  4, 1), // kLoadCvt32_I8ToI16.
  DEFINE_OP(kIdPmovzxbw      , kIdVpmovzxbw      , kU8ToU16 ,  4, 1), // kLoadCvt32_U8ToU16.
  DEFINE_OP(kIdPmovsxbd      , kIdVpmovsxbd      , kI8ToI32 ,  4, 2), // kLoadCvt32_I8ToI32.
  DEFINE_OP(kIdPmovzxbd      , kIdVpmovzxbd      , kU8ToU32 ,  4, 2), // kLoadCvt32_U8ToU32.
  DEFINE_OP(kIdPmovsxwd      , kIdVpmovsxwd      , kI16ToI32,  4, 1), // kLoadCvt32_I16ToI32.
  DEFINE_OP(kIdPmovzxwd      , kIdVpmovzxwd      , kU16ToU32,  4, 1), // kLoadCvt32_U16ToU32.
  DEFINE_OP(kIdPmovsxdq      , kIdVpmovsxdq      , kI32ToI64,  4, 1), // kLoadCvt32_I32ToI64.
  DEFINE_OP(kIdPmovzxdq      , kIdVpmovzxdq      , kU32ToU64,  4, 1), // kLoadCvt32_U32ToU64.
  DEFINE_OP(kIdPmovsxbw      , kIdVpmovsxbw      , kI8ToI16 ,  8, 1), // kLoadCvt64_I8ToI16.
  DEFINE_OP(kIdPmovzxbw      , kIdVpmovzxbw      , kU8ToU16 ,  8, 1), // kLoadCvt64_U8ToU16.
  DEFINE_OP(kIdPmovsxbd      , kIdVpmovsxbd      , kI8ToI32 ,  8, 2), // kLoadCvt64_I8ToI32.
  DEFINE_OP(kIdPmovzxbd      , kIdVpmovzxbd      , kU8ToU32 ,  8, 2), // kLoadCvt64_U8ToU32.
  DEFINE_OP(kIdPmovsxwd      , kIdVpmovsxwd      , kI16ToI32,  8, 1), // kLoadCvt64_I16ToI32.
  DEFINE_OP(kIdPmovzxwd      , kIdVpmovzxwd      , kU16ToU32,  8, 1), // kLoadCvt64_U16ToU32.
  DEFINE_OP(kIdPmovsxdq      , kIdVpmovsxdq      , kI32ToI64,  8, 1), // kLoadCvt64_I32ToI64.
  DEFINE_OP(kIdPmovzxdq      , kIdVpmovzxdq      , kU32ToU64,  8, 1), // kLoadCvt64_U32ToU64.
  DEFINE_OP(kIdNone          , kIdVpmovsxbw      , kI8ToI16 , 16, 3), // kLoadCvt128_I8ToI16.
  DEFINE_OP(kIdNone          , kIdVpmovzxbw      , kU8ToU16 , 16, 3), // kLoadCvt128_U8ToU16.
  DEFINE_OP(kIdNone          , kIdVpmovsxbd      , kI8ToI32 , 16, 2), // kLoadCvt128_I8ToI32.
  DEFINE_OP(kIdNone          , kIdVpmovzxbd      , kU8ToU32 , 16, 2), // kLoadCvt128_U8ToU32.
  DEFINE_OP(kIdNone          , kIdVpmovsxwd      , kI16ToI32, 16, 1), // kLoadCvt128_I16ToI32.
  DEFINE_OP(kIdNone          , kIdVpmovzxwd      , kU16ToU32, 16, 1), // kLoadCvt128_U16ToU32.
  DEFINE_OP(kIdNone          , kIdVpmovsxdq      , kI32ToI64, 16, 1), // kLoadCvt128_I32ToI64.
  DEFINE_OP(kIdNone          , kIdVpmovzxdq      , kU32ToU64, 16, 1), // kLoadCvt128_U32ToU64.
  DEFINE_OP(kIdNone          , kIdVpmovsxbw      , kI8ToI16 , 32, 1), // kLoadCvt256_I8ToI16.
  DEFINE_OP(kIdNone          , kIdVpmovzxbw      , kU8ToU16 , 32, 1), // kLoadCvt256_U8ToU16.
  DEFINE_OP(kIdNone          , kIdVpmovsxwd      , kI16ToI32, 32, 1), // kLoadCvt256_I16ToI32.
  DEFINE_OP(kIdNone          , kIdVpmovzxwd      , kU16ToU32, 32, 1), // kLoadCvt256_U16ToU32.
  DEFINE_OP(kIdNone          , kIdVpmovsxdq      , kI32ToI64, 32, 1), // kLoadCvt256_I32ToI64.
  DEFINE_OP(kIdNone          , kIdVpmovzxdq      , kU32ToU64, 32, 1), // kLoadCvt256_U32ToU64.
  DEFINE_OP(kIdPmovzxbq      , kIdVpmovzxbq      , kU8ToU64 ,  0, 3), // kLoadCvtN_U8ToU64.
  DEFINE_OP(kIdPmovsxbw      , kIdVpmovsxbw      , kI8ToI16 ,  0, 1), // kLoadCvtN_I8ToI16.
  DEFINE_OP(kIdPmovzxbw      , kIdVpmovzxbw      , kU8ToU16 ,  0, 1), // kLoadCvtN_U8ToU16.
  DEFINE_OP(kIdPmovsxbd      , kIdVpmovsxbd      , kI8ToI32 ,  0, 2), // kLoadCvtN_I8ToI32.
  DEFINE_OP(kIdPmovzxbd      , kIdVpmovzxbd      , kU8ToU32 ,  0, 2), // kLoadCvtN_U8ToU32.
  DEFINE_OP(kIdPmovsxwd      , kIdVpmovsxwd      , kI16ToI32,  0, 1), // kLoadCvtN_I16ToI32.
  DEFINE_OP(kIdPmovzxwd      , kIdVpmovzxwd      , kU16ToU32,  0, 1), // kLoadCvtN_U16ToU32.
  DEFINE_OP(kIdPmovsxdq      , kIdVpmovsxdq      , kI32ToI64,  0, 1), // kLoadCvtN_I32ToI64.
  DEFINE_OP(kIdPmovzxdq      , kIdVpmovzxdq      , kU32ToU64,  0, 1), // kLoadCvtN_U32ToU64.
  DEFINE_OP(kIdPinsrb        , kIdVpinsrb        , kNone    ,  1, 0), // kLoadInsertU8.
  DEFINE_OP(kIdPinsrw        , kIdVpinsrw        , kNone    ,  2, 0), // kLoadInsertU16.
  DEFINE_OP(kIdPinsrd        , kIdVpinsrd        , kNone    ,  4, 0), // kLoadInsertU32.
  DEFINE_OP(kIdPinsrq        , kIdVpinsrq        , kNone    ,  8, 0), // kLoadInsertU64.
  DEFINE_OP(kIdInsertps      , kIdVinsertps      , kNone    ,  4, 0), // kLoadInsertF32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  8, 0), // kLoadInsertF32x2.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  8, 0)  // kLoadInsertF64.
};

#undef DEFINE_OP

#define DEFINE_OP(sseInstId, avxInstId, cvt, memSize, memSizeShift) \
  OpcodeVMInfo {                                                    \
    Inst::sseInstId,                                                \
    Inst::avxInstId,                                                \
    0,                                                              \
    uint8_t(NarrowingOp::cvt),                                      \
    memSize,                                                        \
    memSizeShift,                                                   \
    0                                                               \
  }

static constexpr OpcodeVMInfo opcodeInfo2MV[size_t(OpcodeMV::kMaxValue) + 1] = {
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  1, 0), // kStore8.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  2, 0), // kStore16_U16.
  DEFINE_OP(kIdMovd          , kIdVmovd          , kNone    ,  4, 0), // kStore32_U32.
  DEFINE_OP(kIdMovss         , kIdVmovss         , kNone    ,  4, 0), // kStore32_F32.
  DEFINE_OP(kIdMovq          , kIdVmovq          , kNone    ,  8, 0), // kStore64_U32.
  DEFINE_OP(kIdMovq          , kIdVmovq          , kNone    ,  8, 0), // kStore64_U64.
  DEFINE_OP(kIdMovq          , kIdVmovq          , kNone    ,  8, 0), // kStore64_F32.
  DEFINE_OP(kIdMovsd         , kIdVmovsd         , kNone    ,  8, 0), // kStore64_F64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 16, 0), // kStore128_U32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 16, 0), // kStore128_U64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 16, 0), // kStore128_F32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 16, 0), // kStore128_F64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 32, 0), // kStore256_U32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 32, 0), // kStore256_U64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 32, 0), // kStore256_F32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 32, 0), // kStore256_F64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 64, 0), // kStore512_U32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 64, 0), // kStore512_U64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 64, 0), // kStore512_F32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    , 64, 0), // kStore512_F64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  0, 0), // kStoreN_U32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  0, 0), // kStoreN_U64.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  0, 0), // kStoreN_F32.
  DEFINE_OP(kIdNone          , kIdNone           , kNone    ,  0, 0), // kStoreN_F64.
  /*
  DEFINE_OP(kIdNone          , kIdNone           , kU16ToU8 ,  8, 1), // kStoreCvtz64_U16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kU32ToU16,  8, 1), // kStoreCvtz64_U32ToU16.
  DEFINE_OP(kIdNone          , kIdNone           , kU64ToU32,  8, 1), // kStoreCvtz64_U64ToU32.
  DEFINE_OP(kIdNone          , kIdNone           , kI16ToI8 ,  8, 1), // kStoreCvts64_I16ToI8.
  DEFINE_OP(kIdNone          , kIdNone           , kI16ToU8 ,  8, 1), // kStoreCvts64_I16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kU16ToU8 ,  8, 1), // kStoreCvts64_U16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kI32ToI16,  8, 1), // kStoreCvts64_I32ToI16.
  DEFINE_OP(kIdNone          , kIdNone           , kU32ToU16,  8, 1), // kStoreCvts64_U32ToU16.
  DEFINE_OP(kIdNone          , kIdNone           , kI64ToI32,  8, 1), // kStoreCvts64_I64ToI32.
  DEFINE_OP(kIdNone          , kIdNone           , kU64ToU32,  8, 1), // kStoreCvts64_U64ToU32.
  DEFINE_OP(kIdNone          , kIdNone           , kU16ToU8 , 16, 1), // kStoreCvtz128_U16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kU32ToU16, 16, 1), // kStoreCvtz128_U32ToU16.
  DEFINE_OP(kIdNone          , kIdNone           , kU64ToU32, 16, 1), // kStoreCvtz128_U64ToU32.
  DEFINE_OP(kIdNone          , kIdNone           , kI16ToI8 , 16, 1), // kStoreCvts128_I16ToI8.
  DEFINE_OP(kIdNone          , kIdNone           , kI16ToU8 , 16, 1), // kStoreCvts128_I16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kU16ToU8 , 16, 1), // kStoreCvts128_U16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kI32ToI16, 16, 1), // kStoreCvts128_I32ToI16.
  DEFINE_OP(kIdNone          , kIdNone           , kU32ToU16, 16, 1), // kStoreCvts128_U32ToU16.
  DEFINE_OP(kIdNone          , kIdNone           , kI64ToI32, 16, 1), // kStoreCvts128_I64ToI32.
  DEFINE_OP(kIdNone          , kIdNone           , kU64ToU32, 16, 1), // kStoreCvts128_U64ToU32.
  DEFINE_OP(kIdNone          , kIdNone           , kU16ToU8 , 32, 1), // kStoreCvtz256_U16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kU32ToU16, 32, 1), // kStoreCvtz256_U32ToU16.
  DEFINE_OP(kIdNone          , kIdNone           , kU64ToU32, 32, 1), // kStoreCvtz256_U64ToU32.
  DEFINE_OP(kIdNone          , kIdNone           , kI16ToI8 , 32, 1), // kStoreCvts256_I16ToI8.
  DEFINE_OP(kIdNone          , kIdNone           , kI16ToU8 , 32, 1), // kStoreCvts256_I16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kU16ToU8 , 32, 1), // kStoreCvts256_U16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kI32ToI16, 32, 1), // kStoreCvts256_I32ToI16.
  DEFINE_OP(kIdNone          , kIdNone           , kU32ToU16, 32, 1), // kStoreCvts256_U32ToU16.
  DEFINE_OP(kIdNone          , kIdNone           , kI64ToI32, 32, 1), // kStoreCvts256_I64ToI32.
  DEFINE_OP(kIdNone          , kIdNone           , kU64ToU32, 32, 1), // kStoreCvts256_U64ToU32.
  DEFINE_OP(kIdNone          , kIdNone           , kU16ToU8 ,  0, 1), // kStoreCvtzN_U16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kU32ToU16,  0, 1), // kStoreCvtzN_U32ToU16.
  DEFINE_OP(kIdNone          , kIdNone           , kU64ToU32,  0, 1), // kStoreCvtzN_U64ToU32.
  DEFINE_OP(kIdNone          , kIdNone           , kI16ToI8 ,  0, 1), // kStoreCvtsN_I16ToI8.
  DEFINE_OP(kIdNone          , kIdNone           , kI16ToU8 ,  0, 1), // kStoreCvtsN_I16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kU16ToU8 ,  0, 1), // kStoreCvtsN_U16ToU8.
  DEFINE_OP(kIdNone          , kIdNone           , kI32ToI16,  0, 1), // kStoreCvtsN_I32ToI16.
  DEFINE_OP(kIdNone          , kIdNone           , kU32ToU16,  0, 1), // kStoreCvtsN_U32ToU16.
  DEFINE_OP(kIdNone          , kIdNone           , kI64ToI32,  0, 1), // kStoreCvtsN_I64ToI32.
  DEFINE_OP(kIdNone          , kIdNone           , kU64ToU32,  0, 1)  // kStoreCvtsN_U64ToU32.
  */
  DEFINE_OP(kIdPextrw        , kIdVpextrw        , kNone    ,  2, 0), // kStoreExtractU16.
  DEFINE_OP(kIdPextrd        , kIdVpextrd        , kNone    ,  4, 0), // kStoreExtractU32.
  DEFINE_OP(kIdPextrq        , kIdVpextrq        , kNone    ,  8, 0)  // kStoreExtractU64.
};

#undef DEFINE_OP

// bl::Pipeline::PipeCompiler - Vector Instructions - Utility Functions
// ====================================================================

static BL_NOINLINE void PipeCompiler_loadInto(PipeCompiler* pc, const Vec& vec, const Mem& mem, uint32_t bcstSize = 0) noexcept {
  AsmCompiler* cc = pc->cc;
  Mem m(mem);

  if (mem.hasBroadcast() && bcstSize) {
    m.resetBroadcast();
    switch (bcstSize) {
      case 1: cc->vpbroadcastb(vec, m); break;
      case 2: cc->vpbroadcastw(vec, m); break;
      case 4: cc->vpbroadcastd(vec, m); break;
      case 8: cc->vpbroadcastq(vec, m); break;
      default:
        BL_NOT_REACHED();
    }
  }
  else {
    m.setSize(vec.size());
    if (vec.isZmm())
      cc->vmovdqu32(vec, m);
    else if (pc->hasAVX())
      cc->vmovdqu(vec, m);
    else
      cc->movdqu(vec.as<Xmm>(), m);
  }
}

static BL_NOINLINE void PipeCompiler_moveToDst(PipeCompiler* pc, const Vec& dst, const Operand_& src, uint32_t bcstSize = 0) noexcept {
  if (src.isReg()) {
    BL_ASSERT(src.isVec());
    if (dst.id() != src.as<Reg>().id()) {
      pc->v_mov(dst, src);
    }
  }
  else if (src.isMem()) {
    PipeCompiler_loadInto(pc, dst, src.as<Mem>(), bcstSize);
  }
  else {
    BL_NOT_REACHED();
  }
}

static BL_NOINLINE Vec PipeCompiler_loadNew(PipeCompiler* pc, const Vec& ref, const Mem& mem, uint32_t bcstSize = 0) noexcept {
  Vec vec = pc->newSimilarReg(ref, "@vecM");
  PipeCompiler_loadInto(pc, vec, mem, bcstSize);
  return vec;
}

static BL_INLINE bool isSameVec(const Vec& a, const Operand_& b) noexcept {
  return b.isReg() && a.id() == b.as<Reg>().id();
}

static BL_NOINLINE void sseMov(PipeCompiler* pc, const Vec& dst, const Operand_& src) noexcept {
  AsmCompiler* cc = pc->cc;
  if (src.isMem())
    cc->emit(Inst::kIdMovups, dst, src);
  else if (dst.id() != src.id())
    cc->emit(Inst::kIdMovaps, dst, src);
}

static BL_NOINLINE void sseFMov(PipeCompiler* pc, const Vec& dst, const Operand_& src, FloatMode fm) noexcept {
  AsmCompiler* cc = pc->cc;
  if (src.isReg()) {
    if (dst.id() != src.id())
      cc->emit(Inst::kIdMovaps, dst, src);
  }
  else {
    cc->emit(sse_float_inst[size_t(fm)].fmovs, dst, src);
  }
}

static BL_NOINLINE Vec sseCopy(PipeCompiler* pc, const Vec& vec, const char* name) noexcept {
  Vec copy = pc->newSimilarReg(vec, name);
  pc->cc->emit(Inst::kIdMovaps, copy, vec);
  return copy;
}

static BL_NOINLINE void sseMakeVec(PipeCompiler* pc, Operand_& op, const char* name) noexcept {
  if (op.isMem()) {
    Vec tmp = pc->newV128(name);
    sseMov(pc, tmp, op);
    op = tmp;
  }
}

static BL_INLINE uint32_t shufImm2FromSwizzle(Swizzle2 s) noexcept {
  return x86::shuffleImm((s.value >> 8) & 0x1, s.value & 0x1);
}

static BL_INLINE uint32_t shufImm2FromSwizzleWithWidth(Swizzle2 s, VecWidth w) noexcept {
  static constexpr uint32_t multipliers[] = { 0x1, 0x5, 0x55 };
  return shufImm2FromSwizzle(s) * multipliers[size_t(w)];
}

static BL_INLINE uint32_t shufImm4FromSwizzle(Swizzle4 s) noexcept {
  return x86::shuffleImm((s.value >> 24 & 0x3), (s.value >> 16) & 0x3, (s.value >> 8) & 0x3, s.value & 0x3);
}

static BL_INLINE uint32_t shufImm4FromSwizzle(Swizzle2 s) noexcept {
  uint32_t imm0 = uint32_t(s.value     ) & 1u;
  uint32_t imm1 = uint32_t(s.value >> 8) & 1u;
  return x86::shuffleImm(imm1 * 2u + 1u, imm1 * 2u, imm0 * 2u + 1u, imm0 * 2u);
}

static BL_NOINLINE void sseBitNot(PipeCompiler* pc, const Vec& dst, const Operand_& src) noexcept {
  AsmCompiler* cc = pc->cc;

  sseMov(pc, dst, src);
  Operand ones = pc->simdConst(&pc->ct.i_FFFFFFFFFFFFFFFF, Bcst::k32, dst);
  cc->emit(Inst::kIdPxor, dst, ones);
}

static BL_NOINLINE void sseMsbFlip(PipeCompiler* pc, const Vec& dst, const Operand_& src, ElementSize sz) noexcept {
  AsmCompiler* cc = pc->cc;
  const void* mskData {};

  switch (sz) {
    case ElementSize::k8 : mskData = &pc->ct.i_8080808080808080; break;
    case ElementSize::k16: mskData = &pc->ct.i_8000800080008000; break;
    case ElementSize::k32: mskData = &pc->ct.f32_sgn; break;
    case ElementSize::k64: mskData = &pc->ct.f64_sgn; break;

    default:
      BL_NOT_REACHED();
  }

  Operand msk = pc->simdConst(mskData, Bcst::kNA, dst);
  sseMov(pc, dst, src);
  cc->emit(Inst::kIdPxor, dst, msk);
}

static BL_NOINLINE void sseFSignFlip(PipeCompiler* pc, const Vec& dst, const Operand_& src, FloatMode fm) noexcept {
  AsmCompiler* cc = pc->cc;

  const FloatInst& fi = sse_float_inst[size_t(fm)];
  Operand msk;

  switch (fm) {
    case FloatMode::kF32S: msk = pc->simdConst(&pc->ct.f32_sgn_scalar, Bcst::k32, dst); break;
    case FloatMode::kF64S: msk = pc->simdConst(&pc->ct.f64_sgn_scalar, Bcst::k64, dst); break;
    case FloatMode::kF32V: msk = pc->simdConst(&pc->ct.f32_sgn, Bcst::k32, dst); break;
    case FloatMode::kF64V: msk = pc->simdConst(&pc->ct.f64_sgn, Bcst::k64, dst); break;

    default:
      BL_NOT_REACHED();
  }

  sseFMov(pc, dst, src, fm);
  cc->emit(fi.fxor, dst, msk);
}

// Possibly the best solution:
//   https://stackoverflow.com/questions/65166174/how-to-simulate-pcmpgtq-on-sse2
static BL_NOINLINE void sseCmpGtI64(PipeCompiler* pc, const Vec& dst, const Operand_& a, const Operand_& b) noexcept {
  AsmCompiler* cc = pc->cc;

  if (pc->hasSSE4_2()) {
    if (isSameVec(dst, a)) {
      cc->emit(Inst::kIdPcmpgtq, dst, b);
    }
    else {
      Operand_ second = b;
      if (isSameVec(dst, b)) {
        second = cc->newSimilarReg(dst, "@tmp");
        sseMov(pc, second.as<Vec>(), b);
      }
      sseMov(pc, dst, a);
      cc->emit(Inst::kIdPcmpgtq, dst, second);
    }
  }
  else {
    Vec tmp1 = cc->newSimilarReg(dst, "@tmp1");
    Vec tmp2 = cc->newSimilarReg(dst, "@tmp2");

    cc->emit(Inst::kIdMovdqa, tmp1, a);
    cc->emit(Inst::kIdMovdqa, tmp2, b);
    cc->emit(Inst::kIdPcmpeqd, tmp1, tmp2);
    cc->emit(Inst::kIdPsubq, tmp2, a);
    cc->emit(Inst::kIdPand, tmp1, tmp2);

    if (!isSameVec(dst, b)) {
      sseMov(pc, dst, a);
      cc->emit(Inst::kIdPcmpgtd, dst, b);
      cc->emit(Inst::kIdPor, dst, tmp1);
      cc->emit(Inst::kIdPshufd, dst, dst, x86::shuffleImm(3, 3, 1, 1));
    }
    else {
      sseMov(pc, tmp2, a);
      cc->emit(Inst::kIdPcmpgtd, tmp2, b);
      cc->emit(Inst::kIdPor, tmp2, tmp1);
      cc->emit(Inst::kIdPshufd, dst, tmp2, x86::shuffleImm(3, 3, 1, 1));
    }
  }
}

// Possibly the best solution:
//   https://stackoverflow.com/questions/65441496/what-is-the-most-efficient-way-to-do-unsigned-64-bit-comparison-on-sse2
static BL_NOINLINE void sseCmpGtU64(PipeCompiler* pc, const Vec& dst, const Operand_& a, const Operand_& b) noexcept {
  AsmCompiler* cc = pc->cc;

  if (pc->hasSSE4_2()) {
    Operand msk = pc->simdConst(&pc->ct.f64_sgn, Bcst::k64, dst);
    Vec tmp = cc->newSimilarReg(dst, "@tmp");

    if (isSameVec(dst, a)) {
      sseMov(pc, tmp, msk);
      cc->emit(Inst::kIdPxor, dst, tmp);
      cc->emit(Inst::kIdPxor, tmp, b);
      cc->emit(Inst::kIdPcmpgtq, dst, tmp);
    }
    else {
      sseMov(pc, tmp, b);
      sseMov(pc, dst, a);
      cc->emit(Inst::kIdPxor, dst, msk);
      cc->emit(Inst::kIdPxor, tmp, msk);
      cc->emit(Inst::kIdPcmpgtq, dst, tmp);
    }
  }
  else {
    Vec tmp1 = cc->newSimilarReg(dst, "@tmp1");
    Vec tmp2 = cc->newSimilarReg(dst, "@tmp2");
    Vec tmp3 = cc->newSimilarReg(dst, "@tmp3");

    sseMov(pc, tmp1, b);                   // tmp1 = b;
    sseMov(pc, tmp2, a);                   // tmp2 = a;
    cc->emit(Inst::kIdMovaps, tmp3, tmp1); // tmp3 = b;
    cc->emit(Inst::kIdPsubq, tmp3, tmp2);  // tmp3 = b - a
    cc->emit(Inst::kIdPxor, tmp2, tmp1);   // tmp2 = b ^ a
    cc->emit(Inst::kIdPandn, tmp1, a);     // tmp1 =~b & a
    cc->emit(Inst::kIdPandn, tmp2, tmp3);  // tmp2 =~(b ^ a) & (b - a)
    cc->emit(Inst::kIdPor, tmp1, tmp2);    // tmp2 =~(b ^ a) & (b - a) | (~b & a)
    cc->emit(Inst::kIdPsrad, tmp1, 31);    // tmp1 =~(b ^ a) & (b - a) | (~b & a) - repeated MSB bits in 32-bit lanes
    cc->emit(Inst::kIdPshufd, dst, tmp1, x86::shuffleImm(3, 3, 1, 1));
  }
}

static BL_NOINLINE void sseSelect(PipeCompiler* pc, const Vec& dst, const Vec& a, const Operand_& b, const Vec& msk) noexcept {
  AsmCompiler* cc = pc->cc;
  sseMov(pc, dst, a);
  cc->emit(Inst::kIdPand, dst, msk);
  cc->emit(Inst::kIdPandn, msk, b);
  cc->emit(Inst::kIdPor, dst, msk);
}

static BL_NOINLINE void sse_int_widen(PipeCompiler* pc, const Vec& dst, const Vec& src, WideningOp cvt) noexcept {
  AsmCompiler* cc = pc->cc;
  WideningOpInfo cvtInfo = sse_int_widening_op_info[size_t(cvt)];

  if (pc->hasSSE4_1()) {
    cc->emit(cvtInfo.mov, dst, src);
    return;
  }

  if (!cvtInfo.signExtends && cvtInfo.unpackLo != Inst::kIdNone) {
    Operand zero = pc->simdConst(&pc->ct.i_0000000000000000, Bcst::kNA, dst);
    sseMov(pc, dst, src);
    cc->emit(cvtInfo.unpackLo, dst, zero);
    return;
  }

  switch (cvt) {
    case WideningOp::kI8ToI16: {
      cc->overwrite().emit(cvtInfo.unpackLo, dst, src);
      cc->psraw(dst.as<Xmm>(), 8);
      return;
    }

    case WideningOp::kI8ToI32: {
      cc->overwrite().emit(Inst::kIdPunpcklbw, dst, src);
      cc->punpcklwd(dst.as<Xmm>(), dst.as<Xmm>());
      cc->psrad(dst.as<Xmm>(), 24);
      return;
    }

    case WideningOp::kU8ToU32: {
      Operand zero = pc->simdConst(&pc->ct.i_0000000000000000, Bcst::kNA, dst);
      sseMov(pc, dst, src);

      cc->emit(Inst::kIdPunpcklbw, dst, zero);
      cc->emit(Inst::kIdPunpcklwd, dst, zero);
      return;
    }

    case WideningOp::kU8ToU64: {
      Operand zero = pc->simdConst(&pc->ct.i_0000000000000000, Bcst::kNA, dst);
      sseMov(pc, dst, src);

      cc->emit(Inst::kIdPunpcklbw, dst, zero);
      cc->emit(Inst::kIdPunpcklwd, dst, zero);
      cc->emit(Inst::kIdPunpckldq, dst, zero);
      return;
    }

    case WideningOp::kI16ToI32: {
      cc->overwrite().emit(cvtInfo.unpackLo, dst, src);
      cc->psrad(dst.as<Xmm>(), 16);
      return;
    }

    case WideningOp::kI32ToI64: {
      Vec tmp = pc->newSimilarReg(dst, "@tmp");
      sseMov(pc, tmp, src);
      sseMov(pc, dst, src);
      cc->psrad(tmp.as<Xmm>(), 31);
      cc->punpckldq(dst.as<Xmm>(), tmp.as<Xmm>());
      return;
    }

    default:
      BL_NOT_REACHED();
  }
}

static BL_NOINLINE void sseRound(PipeCompiler* pc, const Vec& dst, const Operand& src, FloatMode fm, x86::RoundImm roundMode) noexcept {
  AsmCompiler* cc = pc->cc;

  uint32_t isF32 = fm == FloatMode::kF32S || fm == FloatMode::kF32V;
  const FloatInst& fi = sse_float_inst[size_t(fm)];

  // NOTE: This may be dead code as the compiler handles this case well, however, if this function is
  // called as a helper we don't want to emit a longer sequence if we can just use a single instruction.
  if (pc->hasSSE4_1()) {
    cc->emit(fi.fround, dst, src, roundMode | x86::RoundImm::kSuppress);
    return;
  }

  Operand maxn;

  // round_max (f32) == 0x4B000000
  // round_max (f64) == 0x4330000000000000
  if (fm == FloatMode::kF32S || fm == FloatMode::kF32V)
    maxn = pc->simdConst(&pc->ct.f32_round_max, Bcst::k32, dst);
  else
    maxn = pc->simdConst(&pc->ct.f64_round_max, Bcst::k64, dst);

  Vec t1 = pc->newSimilarReg(dst, "@t1");
  Vec t2 = pc->newSimilarReg(dst, "@t2");
  Vec t3 = pc->newSimilarReg(dst, "@t3");

  // Special cases first - float32/float64 truncation can use float32->int32->float32 conversion.
  if (roundMode == x86::RoundImm::kTrunc) {
    if (fm == FloatMode::kF32S || (fm == FloatMode::kF64S && cc->is64Bit())) {
      Gp r;
      Operand msb;

      if (fm == FloatMode::kF32S) {
        r = pc->newGp32("@gpTmp");
        msb = pc->simdConst(&pc->ct.f32_sgn, Bcst::k32, dst);
      }
      else {
        r = pc->newGp64("@gpTmp");
        msb = pc->simdConst(&pc->ct.f64_sgn, Bcst::k64, dst);
      }

      sseFMov(pc, dst, src, fm);

      if (fm == FloatMode::kF32S)
        cc->cvttss2si(r, dst.as<Xmm>());
      else
        cc->cvttsd2si(r, dst.as<Xmm>());

      cc->emit(fi.fmov, t2, msb);
      cc->emit(fi.fandn, t2, dst);
      cc->emit(fi.fxor, t1, t1);

      if (fm == FloatMode::kF32S)
        cc->cvtsi2ss(t1.as<Xmm>(), r);
      else
        cc->cvtsi2sd(t1.as<Xmm>(), r);

      cc->emit(fi.fcmp, t2, maxn, x86::CmpImm::kLT);
      cc->emit(fi.fand, t1, t2);
      cc->emit(fi.fandn, t2, dst);
      cc->emit(fi.for_, t2, t1);
      cc->emit(fi.fmovs, dst, t2);
      return;
    }
  }

  if (roundMode == x86::RoundImm::kNearest) {
    // Pure SSE2 round-to-even implementation:
    //
    //   float roundeven(float x) {
    //     float magic = x >= 0 ? pow(2, 22) : pow(2, 22) + pow(2, 21);
    //     return x >= magic ? x : x + magic - magic;
    //   }
    //
    //   double roundeven(double x) {
    //     double magic = x >= 0 ? pow(2, 52) : pow(2, 52) + pow(2, 51);
    //     return x >= magic ? x : x + magic - magic;
    //   }
    sseFMov(pc, dst, src, fm);
    cc->emit(fi.fmov, t3, dst);
    cc->emit(fi.psrl, t3, Imm(isF32 ? 31 : 63));
    cc->emit(fi.psll, t3, Imm(isF32 ? 23 : 51));
    cc->emit(fi.for_, t3, maxn);

    cc->emit(fi.fmov, t1, dst);
    cc->emit(fi.fcmp, t1, t3, x86::CmpImm::kLT);
    cc->emit(fi.fand, t1, t3);

    cc->emit(fi.fadd, dst, t1);
    cc->emit(fi.fsub, dst, t1);
    return;
  }

  Operand one;
  if (fm == FloatMode::kF32S || fm == FloatMode::kF32V)
    one = pc->simdConst(&pc->ct.f32_1, Bcst::k32, dst);
  else
    one = pc->simdConst(&pc->ct.f64_1, Bcst::k64, dst);

  if (roundMode == x86::RoundImm::kTrunc) {
    // Should be handled earlier.
    BL_ASSERT(fm != FloatMode::kF32S);

    Operand msb;

    if (fm == FloatMode::kF32V) {
      msb = pc->simdConst(&pc->ct.f32_sgn, Bcst::k32, dst);
      sseFMov(pc, dst, src, fm);

      cc->cvttps2dq(t1.as<Xmm>(), dst.as<Xmm>());
      cc->emit(fi.fmov, t2, msb);
      cc->emit(fi.fandn, t2, dst);
      cc->cvtdq2ps(t1.as<Xmm>(), t1.as<Xmm>());

      cc->emit(fi.fcmp, t2, maxn, x86::CmpImm::kLT);
      cc->emit(fi.fand, t1, t2);
      cc->emit(fi.fandn, t2, dst);
      cc->emit(fi.for_, t2, t1);
      cc->emit(fi.fmov, dst, t2);
    }
    else {
      msb = pc->simdConst(&pc->ct.f64_sgn, Bcst::k64, dst);

      sseFMov(pc, dst, src, fm);
      cc->emit(fi.fmov, t3, msb);
      cc->emit(fi.fandn, t3, dst);
      cc->emit(fi.fmov, t2, t3);
      cc->emit(fi.fcmp, t2, maxn, x86::CmpImm::kLT);
      cc->emit(fi.fand, t2, maxn);
      cc->emit(fi.fmov, t1, t3);
      cc->emit(fi.fadd, t1, t2);
      cc->emit(fi.fsub, t1, t2);
      cc->emit(fi.fcmp, t3, t1, x86::CmpImm::kLT);
      cc->emit(fi.fand, t3, one);
      cc->emit(fi.fsub, t1, t3);

      cc->emit(fi.fand, dst, msb);
      cc->emit(fi.for_, dst, t1);
      return;
    }
    return;
  }

  // Round up & down needs a correction as adding and subtracting magic number rounds to nearest.
  if (roundMode == x86::RoundImm::kDown || roundMode == x86::RoundImm::kUp) {
    InstId correctionInstId = roundMode == x86::RoundImm::kDown ? fi.fsub : fi.fadd;
    x86::CmpImm correctionPredicate = roundMode == x86::RoundImm::kDown ? x86::CmpImm::kLT : x86::CmpImm::kNLE;

    sseFMov(pc, dst, src, fm);

    // maxn (f32) == 0x4B000000 (f64) == 0x4330000000000000
    // t3   (f32) == 0x00800000 (f64) == 0x0008000000000000

    cc->emit(fi.fmov, t3, dst);
    cc->emit(fi.psrl, t3, Imm(isF32 ? 31 : 63));
    cc->emit(fi.psll, t3, Imm(isF32 ? 23 : 51));
    cc->emit(fi.for_, t3, maxn);

    cc->emit(fi.fmov, t1, dst);
    cc->emit(fi.fmov, t2, dst);
    cc->emit(fi.fadd, t2, t3);
    cc->emit(fi.fsub, t2, t3);

    cc->emit(fi.fcmp, t1, t3, x86::CmpImm::kNLT);
    cc->emit(fi.fmov, t3, dst);
    cc->emit(fi.fcmp, t3, t2, correctionPredicate);
    cc->emit(fi.fand, t3, one);

    cc->emit(fi.fand, dst, t1);
    cc->emit(correctionInstId, t2, t3);

    cc->emit(fi.fandn, t1, t2);
    cc->emit(fi.for_, dst, t1);
    return;
  }

  BL_NOT_REACHED();
}

static BL_NOINLINE void avxMov(PipeCompiler* pc, const Vec& dst, const Operand_& src) noexcept {
  AsmCompiler* cc = pc->cc;
  InstId instId = 0;

  if (dst.isZmm())
    instId = src.isMem() ? Inst::kIdVmovdqu32 : Inst::kIdVmovdqa32;
  else
    instId = src.isMem() ? Inst::kIdVmovdqu : Inst::kIdVmovdqa;

  cc->emit(instId, dst, src);
}

static BL_NOINLINE void avxFMov(PipeCompiler* pc, const Vec& dst, const Operand_& src, FloatMode fm) noexcept {
  AsmCompiler* cc = pc->cc;
  if (src.isReg()) {
    if (dst.id() != src.id()) {
      if (fm <= FloatMode::kF64S)
        cc->emit(Inst::kIdVmovaps, dst.xmm(), src);
      else
        cc->emit(Inst::kIdVmovaps, dst, src);
    }
  }
  else {
    cc->emit(avx_float_inst[size_t(fm)].fmovs, dst, src);
  }
}

static BL_NOINLINE void avxMakeVec(PipeCompiler* pc, Operand_& op, const Vec& ref, const char* name) noexcept {
  if (op.isMem()) {
    Vec tmp = pc->newSimilarReg(ref, name);
    avxMov(pc, tmp, op);
    op = tmp;
  }
}

static BL_NOINLINE void avxZero(PipeCompiler* pc, const Vec& dst) noexcept {
  AsmCompiler* cc = pc->cc;
  Vec x = dst.xmm();
  cc->vpxor(x, x, x);
  return;
}

static BL_NOINLINE void avxOnes(PipeCompiler* pc, const Vec& dst) noexcept {
  AsmCompiler* cc = pc->cc;
  if (pc->hasAVX512())
    cc->emit(Inst::kIdVpternlogd, dst, dst, dst, 0xFF);
  else
    cc->emit(Inst::kIdVpcmpeqb, dst, dst, dst);
}

static BL_NOINLINE void avxBitNot(PipeCompiler* pc, const Vec& dst, const Operand_& src) noexcept {
  AsmCompiler* cc = pc->cc;

  if (pc->hasAVX512()) {
    if (src.isReg())
      cc->overwrite().emit(Inst::kIdVpternlogd, dst, src, src, 0x55);
    else
      cc->overwrite().emit(Inst::kIdVpternlogd, dst, dst, src, 0x55);
    return;
  }

  Operand ones = pc->simdConst(&pc->ct.i_FFFFFFFFFFFFFFFF, Bcst::k32, dst);
  if (!src.isReg()) {
    if (ones.isReg()) {
      cc->emit(Inst::kIdVpxor, dst, ones, src);
    }
    else {
      avxMov(pc, dst, src);
      cc->emit(Inst::kIdVpxor, dst, dst, ones);
    }
  }
  else {
    cc->emit(Inst::kIdVpxor, dst, src, ones);
  }
}

static BL_NOINLINE void avxISignFlip(PipeCompiler* pc, const Vec& dst, const Operand_& src, ElementSize sz) noexcept {
  AsmCompiler* cc = pc->cc;
  Operand msk;

  InstId xor_ = (pc->hasAVX512() && dst.isZmm()) ? Inst::kIdVpxord : Inst::kIdVpxor;

  switch (sz) {
    case ElementSize::k8: msk = pc->simdConst(&pc->ct.i_8080808080808080, Bcst::kNA, dst); break;
    case ElementSize::k16: msk = pc->simdConst(&pc->ct.i_8000800080008000, Bcst::kNA, dst); break;
    case ElementSize::k32: msk = pc->simdConst(&pc->ct.f32_sgn, Bcst::k32, dst); break;
    case ElementSize::k64: msk = pc->simdConst(&pc->ct.f64_sgn, Bcst::k64, dst); break;
  }

  if (src.isReg()) {
    cc->emit(xor_, dst, src, msk);
  }
  else if (msk.isReg()) {
    cc->emit(xor_, dst, msk, src);
  }
  else {
    avxMov(pc, dst, src);
    cc->emit(xor_, dst, dst, msk);
  }
}

static BL_NOINLINE void avxFSignFlip(PipeCompiler* pc, const Vec& dst, const Operand_& src, FloatMode fm) noexcept {
  AsmCompiler* cc = pc->cc;

  const FloatInst& fi = avx_float_inst[size_t(fm)];
  Operand msk;

  switch (fm) {
    case FloatMode::kF32S: msk = pc->simdConst(&pc->ct.f32_sgn_scalar, Bcst::k32, dst); break;
    case FloatMode::kF64S: msk = pc->simdConst(&pc->ct.f64_sgn_scalar, Bcst::k64, dst); break;
    case FloatMode::kF32V: msk = pc->simdConst(&pc->ct.f32_sgn, Bcst::k32, dst); break;
    case FloatMode::kF64V: msk = pc->simdConst(&pc->ct.f64_sgn, Bcst::k64, dst); break;

    default:
      BL_NOT_REACHED();
  }

  if (src.isReg()) {
    cc->emit(fi.fxor, dst, src, msk);
  }
  else if (msk.isReg() && fm >= FloatMode::kF32V) {
    cc->emit(fi.fxor, dst, msk, src);
  }
  else {
    avxFMov(pc, dst, src, fm);
    cc->emit(fi.fxor, dst, dst, msk);
  }
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
  Operand src(src_);
  OpcodeVInfo opInfo = opcodeInfo2V[size_t(op)];

  if (hasAVX()) {
    // AVX Implementation
    // ------------------

    InstId instId = opInfo.avxInstId;

    if (hasAVXExt(AVXExt(opInfo.avxExt))) {
      BL_ASSERT(instId != Inst::kIdNone);

      if (opInfo.useImm)
        cc->emit(instId, dst, src, Imm(opInfo.imm));
      else
        cc->emit(instId, dst, src);
      return;
    }

    switch (op) {
      case OpcodeVV::kMov: {
        cc->emit(Inst::kIdVmovaps, dst, src);
        return;
      }

      case OpcodeVV::kMovU64: {
        if (src.isVec())
          src = src.as<Vec>().xmm();
        cc->emit(Inst::kIdVmovq, dst.xmm(), src);
        return;
      }

      case OpcodeVV::kBroadcastU8Z:
      case OpcodeVV::kBroadcastU16Z:
      case OpcodeVV::kBroadcastU8:
      case OpcodeVV::kBroadcastU16:
      case OpcodeVV::kBroadcastU32:
      case OpcodeVV::kBroadcastU64:
      case OpcodeVV::kBroadcastF32:
      case OpcodeVV::kBroadcastF64: {
        // Intrinsic - 32/64-bit broadcasts require AVX, 8/16-bit broadcasts require AVX2/AVX512.
        BL_ASSERT(src.isReg() || src.isMem());
        ElementSize elementSize = ElementSize(opInfo.elementSize);

        if (src.isGp()) {
          Gp srcGp = src.as<Gp>();
          if (elementSize <= ElementSize::k32)
            srcGp = srcGp.r32();
          else
            srcGp = srcGp.r64();

          // AVX512 provides broadcast instructions for both GP, XMM, and memory sources, however, from GP register
          // only VP instructions are available, so we have to convert VBROADCAST[SS|SD] to VPBROADCAST[D|Q].
          if (hasAVX512()) {
            if (op == OpcodeVV::kBroadcastF32) instId = Inst::kIdVpbroadcastd;
            if (op == OpcodeVV::kBroadcastF64) instId = Inst::kIdVpbroadcastq;
            cc->emit(instId, dst, srcGp);
            return;
          }

          // We can handle BroadcastU[8|16]Z differently when AVX2 is not present. Since the opcode has guaranteed
          // source, which has zerod the rest of the register, we are going to multiply with a constant to extend
          // the data into 32 bits, and then we can just use VBROADCASTSS, which would do the rest.
          if (!hasAVX2() && elementSize <= ElementSize::k16 && opInfo.imm == 0x01u) {
            Gp expanded = newGp32("@expanded");
            cc->imul(expanded, srcGp, elementSize == ElementSize::k8 ? 0x01010101u : 0x00010001u);
            cc->vmovd(dst.xmm(), expanded);
            cc->vpshufd(dst.xmm(), dst.xmm(), x86::shuffleImm(0, 0, 0, 0));

            if (!dst.isXmm())
              cc->emit(Inst::kIdVinsertf128, dst, dst, dst.xmm(), 0);
            return;
          }

          // AVX/AVX2 doesn't provide broadcast from GP to XMM, we have to move to XMM first.
          InstId mov = elementSize <= ElementSize::k32 ? Inst::kIdVmovd : Inst::kIdVmovq;
          cc->emit(mov, dst.xmm(), srcGp);
          src = dst.xmm();
        }

        // We have ether a broadcast from memory or an XMM register - AVX2 requires special handling from here...
        if (!hasAVX2()) {
          Vec dstXmm = dst.xmm();

          if (elementSize <= ElementSize::k16) {
            // AVX doesn't provide 8-bit and 16-bit broadcasts - the simplest way is to just use VPSHUFB to repeat the byte.
            InstId insertInstId = elementSize == ElementSize::k8 ? Inst::kIdVpinsrb : Inst::kIdVpinsrw;

            const void* predData = elementSize == ElementSize::k8 ? static_cast<const void*>(&ct.i_0000000000000000)
                                                                  : static_cast<const void*>(&ct.i_0100010001000100);
            Vec pred = simdVecConst(predData, Bcst::k32, dstXmm);

            if (src.isMem()) {
              cc->emit(insertInstId, dstXmm, pred, src, 0);
              cc->vpshufb(dstXmm, dstXmm, pred);
            }
            else {
              cc->vpshufb(dstXmm, src.as<Vec>().xmm(), pred);
            }
          }
          else {
            // AVX doesn't have VPBROADCAST[D|Q], but it has VBROADCAST[SS|SD], which do the same. However,
            // these cannot be used when the source is a register - initially these instructions only allowed
            // broadcasting from memory, then with AVX2 a version that broadcasts from a register was added.
            if (src.isMem()) {
              InstId bcstInstId = (elementSize == ElementSize::k32) ? Inst::kIdVbroadcastss : Inst::kIdVbroadcastsd;
              if (dst.isXmm() && bcstInstId == Inst::kIdVbroadcastsd)
                bcstInstId = Inst::kIdVmovddup;
              cc->emit(bcstInstId, dst, src.as<Mem>());
              return;
            }

            Vec srcXmm = src.as<Vec>().xmm();
            if (elementSize == ElementSize::k32)
              cc->vpshufd(dstXmm, srcXmm, x86::shuffleImm(0, 0, 0, 0));
            else
              cc->vmovddup(dstXmm, srcXmm);
          }

          if (!dst.isXmm())
            cc->emit(Inst::kIdVinsertf128, dst, dst, dstXmm, 0);
          return;
        }

        // VBROADCASTSD cannot be used when XMM is a destination, in that case we must use VMOVDDUP.
        if (dst.isXmm() && instId == Inst::kIdVbroadcastsd)
          instId = Inst::kIdVmovddup;

        if (src.isMem()) {
          Mem m = src.as<Mem>();
          m.setSize(1u << opInfo.elementSize);
          cc->emit(instId, dst, m);
        }
        else {
          cc->emit(instId, dst, src.as<Vec>().xmm());
        }
        return;
      }

      case OpcodeVV::kBroadcastV128_U32:
      case OpcodeVV::kBroadcastV128_U64:
      case OpcodeVV::kBroadcastV128_F32:
      case OpcodeVV::kBroadcastV128_F64: {
        if (src.isReg()) {
          BL_ASSERT(src.isVec());
          src = src.as<Vec>().xmm();
        }

        // 128-bit broadcast is like 128-bit mov in this case as we don't have a wider destination.
        if (dst.isXmm()) {
          avxMov(this, dst, src);
          return;
        }

        // Broadcast instructions only work when the source is a memory operand.
        if (src.isMem()) {
          if (!hasAVX512()) {
            BL_ASSERT(dst.isYmm());
            instId = (op >= OpcodeVV::kBroadcastV128_F32 || !hasAVX2()) ? Inst::kIdVbroadcastf128 : Inst::kIdVbroadcasti128;
          }

          cc->emit(instId, dst, src);
          return;
        }

        // Broadcast with a register source operand is implemented via insert in AVX/AVX2 case.
        if (dst.isYmm()) {
          if (!hasAVX512())
            instId = (op >= OpcodeVV::kBroadcastV128_F32 || !hasAVX2()) ? Inst::kIdVinsertf128 : Inst::kIdVinserti128;
          else
            instId = avx512_vinsert_128[size_t(op) - size_t(OpcodeVV::kBroadcastV128_U32)];

          cc->emit(instId, dst, src.as<Vec>().ymm(), src, 1);
          return;
        }

        // Broadcast with a register to 512-bits is implemented via 128-bit shuffle.
        BL_ASSERT(dst.isZmm());

        instId = avx512_vshuf_128[size_t(op) - size_t(OpcodeVV::kBroadcastV128_U32)];
        src = src.as<Vec>().zmm();
        cc->emit(instId, dst, src, src, x86::shuffleImm(0, 0, 0, 0));
        return;
      }

      case OpcodeVV::kBroadcastV256_U32:
      case OpcodeVV::kBroadcastV256_U64:
      case OpcodeVV::kBroadcastV256_F32:
      case OpcodeVV::kBroadcastV256_F64: {
        if (src.isReg()) {
          BL_ASSERT(src.isVec());
          src = src.as<Vec>().ymm();
        }

        // Cannot broadcast 256-bit vector to a 128-bit or 256-bit vector...
        if (!dst.isZmm()) {
          avxMov(this, dst.ymm(), src);
          return;
        }

        if (src.isMem()) {
          cc->emit(instId, dst, src);
          return;
        }

        instId = avx512_vshuf_128[size_t(op) - size_t(OpcodeVV::kBroadcastV256_U32)];
        src = src.as<Vec>().zmm();
        cc->emit(instId, dst, src, src, x86::shuffleImm(1, 0, 1, 0));
        return;
      }

      case OpcodeVV::kAbsI64: {
        // Native operation requires AVX512, which is not supported by the target.
        Vec tmp = newSimilarReg(dst, "@tmp");
        cc->vpxor(tmp, tmp, tmp);
        cc->emit(Inst::kIdVpsubq, tmp, tmp, src);
        cc->emit(Inst::kIdVblendvpd, dst, tmp, src, tmp);
        return;
      }

      case OpcodeVV::kNotU32:
      case OpcodeVV::kNotU64:
      case OpcodeVV::kNotF32:
      case OpcodeVV::kNotF64: {
        avxBitNot(this, dst, src);
        return;
      }

      case OpcodeVV::kCvtI8ToI32:
      case OpcodeVV::kCvtU8ToU32: {
        if (src.isReg())
          src.as<Vec>().setSignature(signatureOfXmmYmmZmm[0]);
        else
          src.as<Mem>().setSize(dst.size() / 4u);

        cc->emit(instId, dst, src);
        return;
      }

      case OpcodeVV::kCvtI8HiToI16:
      case OpcodeVV::kCvtU8HiToU16:
      case OpcodeVV::kCvtI16HiToI32:
      case OpcodeVV::kCvtU16HiToU32:
      case OpcodeVV::kCvtI32HiToI64:
      case OpcodeVV::kCvtU32HiToU64:
        if (src.isVec()) {
          if (dst.isXmm()) {
            Vec tmp = newV128("@tmp");
            cc->vpshufd(tmp, src.as<Vec>(), x86::shuffleImm(3, 2, 3, 2));
            src = tmp;
          }
          else if (dst.isYmm()) {
            Vec tmp = newV128("@tmp");
            cc->vextractf128(tmp, src.as<Vec>().ymm(), 1u);
            src = tmp;
          }
          else if (dst.isZmm()) {
            Vec tmp = newV256("@tmp");
            cc->vextracti32x8(tmp, src.as<Vec>().zmm(), 1u);
            src = tmp;
          }
          else {
            BL_NOT_REACHED();
          }
        }
        else if (src.isMem()) {
          src.as<Mem>().addOffset(dst.size() / 2u);
        }
        else {
          BL_NOT_REACHED();
        }
        BL_FALLTHROUGH
      case OpcodeVV::kCvtI8LoToI16:
      case OpcodeVV::kCvtU8LoToU16:
      case OpcodeVV::kCvtI16LoToI32:
      case OpcodeVV::kCvtU16LoToU32:
      case OpcodeVV::kCvtI32LoToI64:
      case OpcodeVV::kCvtU32LoToU64: {
        if (src.isReg())
          src.as<Vec>().setSignature(signatureOfXmmYmmZmm[dst.size() >> 6]);
        else
          src.as<Mem>().setSize(dst.size() / 2u);

        cc->emit(instId, dst, src);
        return;
      }

      case OpcodeVV::kAbsF32:
      case OpcodeVV::kAbsF64:
      case OpcodeVV::kNegF32:
      case OpcodeVV::kNegF64: {
        // Intrinsic.
        const void* mskData = op == OpcodeVV::kAbsF32 ? static_cast<const void*>(&ct.f32_abs) :
                              op == OpcodeVV::kAbsF64 ? static_cast<const void*>(&ct.f64_abs) :
                              op == OpcodeVV::kNegF32 ? static_cast<const void*>(&ct.f32_sgn) :
                                                        static_cast<const void*>(&ct.f64_sgn);
        Operand msk = simdConst(mskData, Bcst(opInfo.bcstSize), dst);

        if (src.isMem() && msk.isMem()) {
          avxMov(this, dst, msk);
          cc->emit(instId, dst, dst, src);
        }
        else if (src.isMem()) {
          cc->emit(instId, dst, msk, src);
        }
        else {
          cc->emit(instId, dst, src, msk);
        }
        return;
      }

      case OpcodeVV::kRcpF32: {
        // Intrinsic.
        Vec one = simdVecConst(&ct.f32_1, Bcst::k32, dst);
        cc->emit(Inst::kIdVdivps, dst, one, src);
        return;
      }

      case OpcodeVV::kRcpF64: {
        // Intrinsic.
        Vec one = simdVecConst(&ct.f64_1, Bcst::k32, dst);
        cc->emit(Inst::kIdVdivpd, dst, one, src);
        return;
      }

      case OpcodeVV::kTruncF32S:
      case OpcodeVV::kTruncF64S:
      case OpcodeVV::kTruncF32:
      case OpcodeVV::kTruncF64:
      case OpcodeVV::kFloorF32S:
      case OpcodeVV::kFloorF64S:
      case OpcodeVV::kFloorF32:
      case OpcodeVV::kFloorF64:
      case OpcodeVV::kCeilF32S:
      case OpcodeVV::kCeilF64S:
      case OpcodeVV::kCeilF32:
      case OpcodeVV::kCeilF64:
      case OpcodeVV::kRoundF32S:
      case OpcodeVV::kRoundF64S:
      case OpcodeVV::kRoundF32:
      case OpcodeVV::kRoundF64: {
        if (hasAVX512()) {
          // AVX512 uses a different name.
          constexpr uint16_t avx512_rndscale[4] = {
            Inst::kIdVrndscaless,
            Inst::kIdVrndscalesd,
            Inst::kIdVrndscaleps,
            Inst::kIdVrndscalepd
          };
          instId = avx512_rndscale[(size_t(op) - size_t(OpcodeVV::kTruncF32S)) & 0x3];
        }

        FloatMode fm = FloatMode(opInfo.floatMode);

        if (fm == FloatMode::kF32S || fm == FloatMode::kF64S) {
          dst = dst.xmm();
          // These instructions use 3 operand form for historical reasons.
          if (src.isMem()) {
            cc->emit(avx_float_inst[size_t(opInfo.floatMode)].fmovs, dst, src);
            cc->emit(instId, dst, dst, dst, uint32_t(opInfo.imm));
          }
          else {
            src = src.as<Vec>().xmm();
            cc->emit(instId, dst, src, src, uint32_t(opInfo.imm));
          }
        }
        else {
          cc->emit(instId, dst, src, uint32_t(opInfo.imm));
        }
        return;
      }

      case OpcodeVV::kSqrtF32S:
      case OpcodeVV::kSqrtF64S: {
        dst = dst.xmm();

        // Intrinsic - these instructions use 3 operand form for historical reasons.
        if (src.isMem()) {
          avxFMov(this, dst, src, FloatMode(opInfo.floatMode));
          cc->emit(instId, dst, dst, dst);
        }
        else {
          src = src.as<Vec>().xmm();
          cc->emit(instId, dst, src, src);
        }
        return;
      }

      case OpcodeVV::kCvtF32ToF64S:
      case OpcodeVV::kCvtF64ToF32S: {
        dst = dst.xmm();
        if (src.isVec())
          src = src.as<Vec>().xmm();

        // Intrinsic - these instructions use 3 operand form for historical reasons.
        Vec zeros = simdVecConst(&ct.i128_0000000000000000, Bcst::k32, dst);
        cc->emit(instId, dst, zeros, src);
        return;
      }

      case OpcodeVV::kCvtF32LoToF64:
      case OpcodeVV::kCvtI32LoToF64: {
        // Intrinsic - widening conversion - low part conversions are native, high part emulated.
        if (src.isReg()) {
          uint32_t w = dst.size() >> 6;
          src.setSignature(signatureOfXmmYmmZmm[w]);
        }
        else {
          uint32_t w = dst.size() >> 4;
          src.as<Mem>().setSize(w * 8u);
        }

        cc->emit(instId, dst, src);
        return;
      }

      case OpcodeVV::kCvtF32HiToF64:
      case OpcodeVV::kCvtI32HiToF64: {
        if (src.isReg()) {
          uint32_t w = dst.size() >> 6;
          Vec tmp = newVec(VecWidth(w), "@tmp");

          src.setSignature(signatureOfXmmYmmZmm[w]);
          if (dst.isZmm()) {
            cc->vextracti32x8(tmp, src.as<Vec>().zmm(), 1u);
            cc->emit(instId, dst, tmp);
          }
          else if (dst.isYmm()) {
            if (hasAVX512())
              cc->vextracti32x4(tmp, src.as<Vec>().ymm(), 1u);
            else
              cc->vextracti128(tmp, src.as<Vec>().ymm(), 1u);
            cc->emit(instId, dst, tmp);
          }
          else {
            cc->vpshufd(tmp, src.as<Vec>(), x86::shuffleImm(3, 2, 3, 2));
            cc->emit(instId, dst, tmp);
          }
        }
        else {
          uint32_t w = dst.size() >> 4;
          src.as<Mem>().setSize(w * 8u);
          src.as<Mem>().addOffset(w * 8u);
          cc->emit(instId, dst, src);
        }

        return;
      }

      case OpcodeVV::kCvtF64ToF32Lo:
      case OpcodeVV::kCvtTruncF64ToI32Lo:
      case OpcodeVV::kCvtRoundF64ToI32Lo: {
        // Intrinsic - narrowing conversion - low part conversions are native, high part emulated.
        uint32_t dstSize = blMax(dst.size() / 2u, src.x86RmSize());
        uint32_t w = dstSize >> 5;

        dst.setSignature(signatureOfXmmYmmZmm[w ? w - 1u : 0u]);

        if (src.isReg())
          src.setSignature(signatureOfXmmYmmZmm[w]);
        else if (src.x86RmSize() == 0)
          src.as<Mem>().setSize(w * 32u);

        cc->emit(instId, dst, src);
        return;
      }

      case OpcodeVV::kCvtF64ToF32Hi:
      case OpcodeVV::kCvtTruncF64ToI32Hi:
      case OpcodeVV::kCvtRoundF64ToI32Hi: {
        uint32_t w = dst.size() >> 6;
        Vec tmp = newVec(VecWidth(w), "@tmp");

        if (src.isMem())
          src.as<Mem>().setSize(dst.size());

        cc->emit(instId, tmp, src);

        if (dst.isZmm())
          cc->vinserti32x8(dst, dst, tmp.ymm(), 1);
        else if (dst.isYmm())
          cc->vinserti128(dst, dst, tmp.xmm(), 1);
        else
          cc->vunpcklpd(dst, dst, tmp);
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else {
    // SSE Implementation
    // ------------------

    InstId instId = opInfo.sseInstId;

    if (hasSSEExt(SSEExt(opInfo.sseExt))) {
      BL_ASSERT(instId != Inst::kIdNone);

      if (opInfo.useImm)
        cc->emit(instId, dst, src, Imm(opInfo.imm));
      else
        cc->emit(instId, dst, src);
      return;
    }

    switch (op) {
      case OpcodeVV::kMov: {
        cc->emit(Inst::kIdMovaps, dst, src);
        return;
      }

      case OpcodeVV::kMovU64: {
        cc->emit(Inst::kIdMovq, dst, src);
        return;
      }

      case OpcodeVV::kBroadcastU8Z:
      case OpcodeVV::kBroadcastU16Z:
      case OpcodeVV::kBroadcastU8:
      case OpcodeVV::kBroadcastU16: {
        // Intrinsic - 8/16-bit broadcasts are generally not available in SSE mode - we have to emulate.
        BL_ASSERT(src.isReg() || src.isMem());
        ElementSize elementSize = ElementSize(opInfo.elementSize);

        if (src.isMem() || src.isGp()) {
          Gp tmp = newGp32("@tmp");
          uint32_t mulBy = elementSize == ElementSize::k8 ? 0x01010101u : 0x00010001u;

          if (src.isMem()) {
            src.as<Mem>().setSize(elementSize == ElementSize::k8 ? 1 : 2);
            cc->movzx(tmp, src.as<Mem>());
            cc->imul(tmp, tmp, mulBy);
          }
          else if (opInfo.imm == 0x01) {
            // OPTIMIZATION: If it's guaranteed that the unused part of the register is zero, we can imul without zero extending.
            cc->imul(tmp, src.as<Gp>().r32(), mulBy);
          }
          else {
            OperandSignature srcSgn = OperandSignature{
              elementSize == ElementSize::k8 ? x86::RegTraits<RegType::kGp8Lo>::kSignature : x86::RegTraits<RegType::kGp16>::kSignature};
            src.as<Gp>().setSignature(srcSgn);
            cc->movzx(tmp, src.as<Gp>());
            cc->imul(tmp, tmp, mulBy);
          }

          cc->emit(Inst::kIdMovd, dst, tmp);
          cc->emit(Inst::kIdPshufd, dst, dst, x86::shuffleImm(0, 0, 0, 0));
          return;
        }

        BL_ASSERT(src.isVec());

        if (hasSSSE3()) {
          if (elementSize == ElementSize::k8 || (elementSize == ElementSize::k16 && isSameVec(dst, src))) {
            Operand predicate = elementSize == ElementSize::k8 ? simdConst(&ct.i_0000000000000000, Bcst::kNA, dst.as<Vec>())
                                                               : simdConst(&ct.i_0100010001000100, Bcst::kNA, dst.as<Vec>());
            sseMov(this, dst, src);
            cc->emit(Inst::kIdPshufb, dst, predicate);
            return;
          }
        }

        if (elementSize == ElementSize::k8) {
          sseMov(this, dst, src);
          cc->emit(Inst::kIdPunpcklbw, dst, dst);
          src = dst;
        }

        cc->emit(Inst::kIdPshuflw, dst, src, x86::shuffleImm(0, 0, 0, 0));
        cc->emit(Inst::kIdPshufd, dst, dst, x86::shuffleImm(0, 0, 0, 0));
        return;
      }

      case OpcodeVV::kBroadcastU32:
      case OpcodeVV::kBroadcastF32: {
        // Intrinsic - 32-bit broadcast is generally not available in SSE mode - we have to emulate.
        BL_ASSERT(src.isReg() || src.isMem());

        if (src.isGp()) {
          cc->emit(Inst::kIdMovd, dst, src.as<Gp>().r32());
          src = dst;
        }

        if (src.isReg()) {
          cc->emit(Inst::kIdPshufd, dst, src, x86::shuffleImm(0, 0, 0, 0));
        }
        else {
          cc->emit(Inst::kIdMovd, dst, src);
          cc->emit(Inst::kIdPshufd, dst, dst, x86::shuffleImm(0, 0, 0, 0));
        }

        return;
      }

      case OpcodeVV::kBroadcastU64:
      case OpcodeVV::kBroadcastF64: {
        // Intrinsic - 64-bit broadcast is generally not available in SSE mode - we have to emulate.
        BL_ASSERT(src.isReg() || src.isMem());

        if (src.isGp()) {
          cc->emit(Inst::kIdMovq, dst, src.as<Gp>().r64());
          src = dst;
        }

        if (hasSSE3()) {
          cc->emit(Inst::kIdMovddup, dst, src);
        }
        else if (src.isReg()) {
          cc->emit(Inst::kIdPshufd, dst, src, x86::shuffleImm(1, 0, 1, 0));
        }
        else {
          cc->emit(Inst::kIdMovq, dst, src);
          cc->emit(Inst::kIdPshufd, dst, dst, x86::shuffleImm(1, 0, 1, 0));
        }

        return;
      }

      case OpcodeVV::kBroadcastV128_U32:
      case OpcodeVV::kBroadcastV128_U64:
      case OpcodeVV::kBroadcastV128_F32:
      case OpcodeVV::kBroadcastV128_F64: {
        // 128-bit broadcast is like 128-bit mov in this case as we don't have wider vectors.
        sseMov(this, dst, src);
        return;
      }

      case OpcodeVV::kAbsI8: {
        // Native operation requires SSSE3, which is not supported by the target.
        if (isSameVec(dst, src)) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          cc->emit(Inst::kIdPxor, tmp, tmp);
          cc->emit(Inst::kIdPsubb, tmp, dst);
          cc->emit(Inst::kIdPminub, dst, tmp);
        }
        else {
          cc->emit(Inst::kIdPxor, dst, dst);
          cc->emit(Inst::kIdPsubb, dst, src);
          cc->emit(Inst::kIdPminub, dst, src);
        }
        return;
      }

      case OpcodeVV::kAbsI16: {
        // Native operation requires SSSE3, which is not supported by the target.
        if (isSameVec(dst, src)) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          cc->emit(Inst::kIdPxor, tmp, tmp);
          cc->emit(Inst::kIdPsubw, tmp, dst);
          cc->emit(Inst::kIdPmaxsw, dst, tmp);
        }
        else {
          cc->emit(Inst::kIdPxor, dst, dst);
          cc->emit(Inst::kIdPsubw, dst, src);
          cc->emit(Inst::kIdPmaxsw, dst, src);
        }
        return;
      }

      case OpcodeVV::kAbsI32: {
        // Native operation requires SSSE3, which is not supported by the target.
        Vec tmp = newSimilarReg(dst, "@tmp");
        cc->emit(Inst::kIdMovaps, tmp, src);
        cc->emit(Inst::kIdPsrad, tmp, 31);
        sseMov(this, dst, src);
        cc->emit(Inst::kIdPxor, dst, tmp);
        cc->emit(Inst::kIdPsubd, dst, tmp);
        return;
      }

      case OpcodeVV::kAbsI64: {
        // Native operation requires AVX512, which is not supported by the target.
        Vec tmp = newSimilarReg(dst, "@tmp");
        cc->emit(Inst::kIdPshufd, tmp, src, x86::shuffleImm(3, 3, 1, 1));
        cc->emit(Inst::kIdPsrad, tmp, 31);
        sseMov(this, dst, src);
        cc->emit(Inst::kIdPxor, dst, tmp);
        cc->emit(Inst::kIdPsubq, dst, tmp);
        return;
      }

      case OpcodeVV::kNotU32:
      case OpcodeVV::kNotU64:
      case OpcodeVV::kNotF32:
      case OpcodeVV::kNotF64: {
        sseBitNot(this, dst, src);
        return;
      }

      case OpcodeVV::kCvtI8ToI32:
      case OpcodeVV::kCvtU8ToU32: {
        if (src.isMem())
          src.as<Mem>().setSize(4u);

        if (hasSSE4_1()) {
          cc->emit(instId, dst, src);
          return;
        }

        if (src.isMem()) {
          cc->movd(dst.as<Xmm>(), src.as<Mem>());
          src = dst;
        }

        WideningOp cvt = (op == OpcodeVV::kCvtI8ToI32) ? WideningOp::kI8ToI32 : WideningOp::kU8ToU32;
        sse_int_widen(this, dst, src.as<Vec>(), cvt);
        return;
      }

      case OpcodeVV::kCvtU8HiToU16:
      case OpcodeVV::kCvtU16HiToU32:
      case OpcodeVV::kCvtU32HiToU64:
        if (src.isVec() && dst.id() != src.id() && hasSSE4_1()) {
          cc->pshufd(dst.as<Xmm>(), src.as<Xmm>(), x86::shuffleImm(3, 2, 3, 2));
          cc->emit(instId, dst, dst);
          return;
        }
        BL_FALLTHROUGH
      case OpcodeVV::kCvtI8HiToI16:
      case OpcodeVV::kCvtI16HiToI32:
      case OpcodeVV::kCvtI32HiToI64:
        if (src.isVec()) {
          sseMov(this, dst, src);

          switch (op) {
            case OpcodeVV::kCvtI8HiToI16: {
              cc->punpckhbw(dst.as<Xmm>(), dst.as<Xmm>());
              cc->psraw(dst.as<Xmm>(), 8);
              break;
            }

            case OpcodeVV::kCvtU8HiToU16: {
              cc->emit(Inst::kIdPunpckhbw, dst.as<Xmm>(), simdConst(&ct.i_0000000000000000, Bcst::kNA, dst));
              break;
            }

            case OpcodeVV::kCvtI16HiToI32: {
              cc->punpckhwd(dst.as<Xmm>(), dst.as<Xmm>());
              cc->psrad(dst.as<Xmm>(), 16);
              break;
            }

            case OpcodeVV::kCvtU16HiToU32: {
              cc->emit(Inst::kIdPunpckhwd, dst.as<Xmm>(), simdConst(&ct.i_0000000000000000, Bcst::kNA, dst));
              break;
            }

            case OpcodeVV::kCvtI32HiToI64: {
              Vec tmp = newV128("@tmp");
              sseMov(this, tmp, dst);
              cc->psrad(tmp.as<Xmm>(), 31);
              cc->punpckhdq(dst.as<Xmm>(), tmp.as<Xmm>());
              break;
            }

            case OpcodeVV::kCvtU32HiToU64: {
              cc->emit(Inst::kIdPunpckhdq, dst.as<Xmm>(), simdConst(&ct.i_0000000000000000, Bcst::kNA, dst));
              break;
            }

            default:
              BL_NOT_REACHED();
          }
          return;
        }
        else if (src.isMem()) {
          src.as<Mem>().addOffset(8u);
          op = OpcodeVV(uint32_t(op) - 1);
        }
        else {
          BL_NOT_REACHED();
        }
        BL_FALLTHROUGH
      case OpcodeVV::kCvtI8LoToI16:
      case OpcodeVV::kCvtU8LoToU16:
      case OpcodeVV::kCvtI16LoToI32:
      case OpcodeVV::kCvtU16LoToU32:
      case OpcodeVV::kCvtI32LoToI64:
      case OpcodeVV::kCvtU32LoToU64: {
        if (src.isMem())
          src.as<Mem>().setSize(8u);

        if (hasSSE4_1()) {
          cc->emit(instId, dst, src);
          return;
        }

        if (src.isMem()) {
          cc->movq(dst.as<Xmm>(), src.as<Mem>());
          src = dst;
        }

        WideningOp cvt {};
        switch (op) {
          case OpcodeVV::kCvtI8LoToI16 : cvt = WideningOp::kI8ToI16; break;
          case OpcodeVV::kCvtU8LoToU16 : cvt = WideningOp::kU8ToU16; break;
          case OpcodeVV::kCvtI16LoToI32: cvt = WideningOp::kI16ToI32; break;
          case OpcodeVV::kCvtU16LoToU32: cvt = WideningOp::kU16ToU32; break;
          case OpcodeVV::kCvtI32LoToI64: cvt = WideningOp::kI32ToI64; break;
          case OpcodeVV::kCvtU32LoToU64: cvt = WideningOp::kU32ToU64; break;
          default:
            BL_NOT_REACHED();
        }

        sse_int_widen(this, dst, src.as<Vec>(), cvt);
        return;
      }

      case OpcodeVV::kTruncF32:
      case OpcodeVV::kTruncF64:
      case OpcodeVV::kFloorF32:
      case OpcodeVV::kFloorF64:
      case OpcodeVV::kCeilF32:
      case OpcodeVV::kCeilF64:
      case OpcodeVV::kRoundF32:
      case OpcodeVV::kRoundF64:
        // Native operation requires SSE4.1.
        if (hasSSE4_1()) {
          cc->emit(instId, dst, src, Imm(opInfo.imm));
          return;
        }
        BL_FALLTHROUGH
      case OpcodeVV::kTruncF32S:
      case OpcodeVV::kTruncF64S:
      case OpcodeVV::kFloorF32S:
      case OpcodeVV::kFloorF64S:
      case OpcodeVV::kCeilF32S:
      case OpcodeVV::kCeilF64S:
      case OpcodeVV::kRoundF32S:
      case OpcodeVV::kRoundF64S: {
        // Native operation requires SSE4.1.
        if (hasSSE4_1()) {
          if (!isSameVec(dst, src))
            sseFMov(this, dst, src, FloatMode(opInfo.floatMode));
          cc->emit(instId, dst, dst, Imm(opInfo.imm));
          return;
        }

        sseRound(this, dst, src, FloatMode(opInfo.floatMode), x86::RoundImm(opInfo.imm & 0x7));
        return;
      }

      case OpcodeVV::kAbsF32:
      case OpcodeVV::kAbsF64:
      case OpcodeVV::kNegF32:
      case OpcodeVV::kNegF64: {
        // Intrinsic.
        const void* mskData = op == OpcodeVV::kAbsF32 ? static_cast<const void*>(&ct.f32_abs) :
                              op == OpcodeVV::kAbsF64 ? static_cast<const void*>(&ct.f64_abs) :
                              op == OpcodeVV::kNegF32 ? static_cast<const void*>(&ct.f32_sgn) :
                                                        static_cast<const void*>(&ct.f64_sgn);
        Operand msk = simdConst(mskData, Bcst::k32, dst);

        if (!isSameVec(dst, src))
          sseMov(this, dst, src);

        cc->emit(instId, dst, msk);
        return;
      }

      case OpcodeVV::kRcpF32: {
        Operand one = simdConst(&ct.f32_1, Bcst::k32, dst);
        if (isSameVec(dst, src)) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          sseMov(this, tmp, one);
          cc->emit(Inst::kIdDivps, tmp, src);
          sseMov(this, dst, tmp);
        }
        else {
          sseMov(this, dst, one);
          cc->emit(Inst::kIdDivps, dst, src);
        }
        return;
      }

      case OpcodeVV::kRcpF64: {
        Operand one = simdConst(&ct.f64_1, Bcst::k64, dst);
        if (isSameVec(dst, src)) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          sseMov(this, tmp, one);
          cc->emit(Inst::kIdDivpd, tmp, src);
          sseMov(this, dst, tmp);
        }
        else {
          sseMov(this, dst, one);
          cc->emit(Inst::kIdDivpd, dst, src);
        }
        return;
      }

      case OpcodeVV::kSqrtF32S:
      case OpcodeVV::kSqrtF64S: {
        sseMov(this, dst, src);
        cc->emit(instId, dst, dst);
        return;
      }

      case OpcodeVV::kCvtF32ToF64S:
      case OpcodeVV::kCvtF64ToF32S: {
        if (isSameVec(dst, src)) {
          cc->emit(instId, dst, src);
        }
        else {
          cc->emit(Inst::kIdXorps, dst, dst);
          cc->emit(instId, dst, src);
        }
        return;
      }

      case OpcodeVV::kCvtF32HiToF64:
      case OpcodeVV::kCvtI32HiToF64: {
        if (src.isMem()) {
          Mem mem(src.as<Mem>());
          mem.addOffset(8);
          cc->emit(instId, dst, mem);
        }
        else {
          if (isSameVec(dst, src))
            cc->emit(Inst::kIdMovhlps, dst, src);
          else
            cc->emit(Inst::kIdPshufd, dst, src, x86::shuffleImm(3, 2, 3, 2));
          cc->emit(instId, dst, dst);
        }
        return;
      }

      case OpcodeVV::kCvtF64ToF32Hi:
      case OpcodeVV::kCvtTruncF64ToI32Hi:
      case OpcodeVV::kCvtRoundF64ToI32Hi: {
        Vec tmp = newV128("@tmp");

        if (src.isMem())
          src.as<Mem>().setSize(dst.size());

        cc->emit(instId, tmp, src);
        cc->emit(Inst::kIdUnpcklpd, dst, tmp);
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }
}

void PipeCompiler::emit_2v(OpcodeVV op, const OpArray& dst_, const Operand_& src_) noexcept { emit_2v_t(this, op, dst_, src_); }
void PipeCompiler::emit_2v(OpcodeVV op, const OpArray& dst_, const OpArray& src_) noexcept { emit_2v_t(this, op, dst_, src_); }

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 2VI
// ===========================================================

void PipeCompiler::emit_2vi(OpcodeVVI op, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept {
  BL_ASSERT(dst_.isVec());

  Vec dst(dst_.as<Vec>());
  Operand src(src_);
  OpcodeVInfo opInfo = opcodeInfo2VI[size_t(op)];

  if (hasAVX()) {
    // AVX Implementation
    // ------------------

    InstId instId = opInfo.avxInstId;

    if (hasAVXExt(AVXExt(opInfo.avxExt))) {
      BL_ASSERT(instId != Inst::kIdNone);

      cc->emit(instId, dst, src, Imm(imm));
      return;
    }

    switch (op) {
      case OpcodeVVI::kSllU16:
      case OpcodeVVI::kSllU32:
      case OpcodeVVI::kSllU64:
      case OpcodeVVI::kSrlU16:
      case OpcodeVVI::kSrlU32:
      case OpcodeVVI::kSrlU64:
      case OpcodeVVI::kSraI16:
      case OpcodeVVI::kSraI32:
      case OpcodeVVI::kSllbU128:
      case OpcodeVVI::kSrlbU128: {
        // This instruction requires AVX-512 if the source is a memory operand.
        if (src.isMem()) {
          avxMov(this, dst, src);
          cc->emit(instId, dst, dst, imm);
        }
        else {
          cc->emit(instId, dst, src, imm);
        }
        return;
      }

      case OpcodeVVI::kSraI64: {
        // Native operation requires AVX-512, which is not supported by the target.
        if (imm == 0) {
          avxMov(this, dst, src);
          return;
        }

        if (imm == 63) {
          cc->emit(Inst::kIdVpshufd, dst, src, x86::shuffleImm(3, 3, 1, 1));
          cc->emit(Inst::kIdVpsrad, dst, dst, 31);
          return;
        }

        Vec tmp = newSimilarReg(dst, "@tmp");

        if (src.isMem()) {
          avxMov(this, dst, src);
          src = dst;
        }

        if (imm <= 32) {
          cc->emit(Inst::kIdVpsrad, tmp, src, blMin<uint32_t>(imm, 31u));
          cc->emit(Inst::kIdVpsrlq, dst, src, imm);
          cc->emit(Inst::kIdVpblendw, dst, dst, tmp, 0xCC);
          return;
        }

        cc->emit(Inst::kIdVpshufd, tmp, src, x86::shuffleImm(3, 3, 1, 1));
        cc->emit(Inst::kIdVpsrad, tmp, tmp, 31);
        cc->emit(Inst::kIdVpsrlq, dst, src, imm);
        cc->emit(Inst::kIdVpsllq, tmp, tmp, 64u - imm);
        cc->emit(Inst::kIdVpor, dst, dst, tmp);
        return;
      }

      case OpcodeVVI::kSwizzleU16x4: {
        // Intrinsic.

        // TODO: [JIT] OPTIMIZATION: Use VPSHUFB instead where appropriate.
        uint32_t shufImm = shufImm4FromSwizzle(Swizzle4{imm});
        cc->emit(Inst::kIdVpshuflw, dst, src, shufImm);
        cc->emit(Inst::kIdVpshufhw, dst, dst, shufImm);
        return;
      }

      case OpcodeVVI::kSwizzleLoU16x4:
      case OpcodeVVI::kSwizzleHiU16x4:
      case OpcodeVVI::kSwizzleU32x4: {
        // Intrinsic (AVX | AVX512).
        BL_ASSERT(instId != Inst::kIdNone);

        uint32_t shufImm = shufImm4FromSwizzle(Swizzle4{imm});
        cc->emit(instId, dst, src, shufImm);
        return;
      }

      case OpcodeVVI::kSwizzleU64x2: {
        // Intrinsic (AVX | AVX512).
        if (Swizzle2{imm} == swizzle(0, 0)) {
          cc->emit(Inst::kIdVmovddup, dst, src);
        }
        else if (Swizzle2{imm} == swizzle(0, 0) && src.isReg()) {
          cc->emit(Inst::kIdVpunpcklqdq, dst, src, src);
        }
        else if (Swizzle2{imm} == swizzle(1, 1) && src.isReg()) {
          cc->emit(Inst::kIdVpunpckhqdq, dst, src, src);
        }
        else {
          uint32_t shufImm = shufImm4FromSwizzle(Swizzle2{imm});
          cc->emit(Inst::kIdVpshufd, dst, src, shufImm);
        }
        return;
      }

      case OpcodeVVI::kSwizzleF32x4: {
        // Intrinsic (AVX | AVX512).
        uint32_t shufImm = shufImm4FromSwizzle(Swizzle4{imm});
        if (src.isReg())
          cc->emit(Inst::kIdVshufps, dst, src, src, shufImm);
        else
          cc->emit(Inst::kIdVpshufd, dst, src, shufImm);
        return;
      }

      case OpcodeVVI::kSwizzleF64x2: {
        // Intrinsic (AVX | AVX512).
        if (Swizzle2{imm} == swizzle(0, 0) && !dst.isZmm()) {
          cc->emit(Inst::kIdVmovddup, dst, src);
        }
        else if (Swizzle2{imm} == swizzle(0, 0) && src.isReg()) {
          cc->emit(Inst::kIdVunpcklpd, dst, src, src);
        }
        else if (Swizzle2{imm} == swizzle(1, 1) && src.isReg()) {
          cc->emit(Inst::kIdVunpckhpd, dst, src, src);
        }
        else if (src.isReg()) {
          uint32_t shufImm = shufImm2FromSwizzleWithWidth(Swizzle2{imm}, VecWidthUtils::vecWidthOf(dst));
          cc->emit(Inst::kIdVshufpd, dst, src, src, shufImm);
        }
        else {
          uint32_t shufImm = shufImm4FromSwizzle(Swizzle2{imm});
          cc->emit(Inst::kIdVpshufd, dst, src, shufImm);
        }
        return;
      }

      case OpcodeVVI::kSwizzleF64x4:
      case OpcodeVVI::kSwizzleU64x4: {
        uint32_t shufImm = shufImm4FromSwizzle(Swizzle4{imm});
        cc->emit(opInfo.avxInstId, dst, src, shufImm);
        return;
      }

      case OpcodeVVI::kExtractV128_I32:
      case OpcodeVVI::kExtractV128_I64:
      case OpcodeVVI::kExtractV128_F32:
      case OpcodeVVI::kExtractV128_F64: {
        // Intrinsic (AVX | AVX512).
        BL_ASSERT(imm < 4);
        dst.setSignature(signatureOfXmmYmmZmm[0]);

        if (src.isMem()) {
          src.as<Mem>().addOffset(imm * 16u);
          v_loadu128(dst.as<Xmm>(), src.as<Mem>());
          return;
        }

        if (src.as<Vec>().isZmm()) {
          BL_ASSERT(imm < 4);
          cc->vextracti32x4(dst, src.as<Vec>(), imm);
        }
        else if (src.as<Vec>().isYmm()) {
          BL_ASSERT(imm < 2);
          cc->vextractf128(dst, src.as<Vec>(), imm);
        }
        else {
          BL_NOT_REACHED();
        }

        return;
      }

      case OpcodeVVI::kExtractV256_I32:
      case OpcodeVVI::kExtractV256_I64:
      case OpcodeVVI::kExtractV256_F32:
      case OpcodeVVI::kExtractV256_F64: {
        // Intrinsic (AVX | AVX512).
        BL_ASSERT(imm < 2);
        dst.setSignature(signatureOfXmmYmmZmm[1]);

        if (src.isMem()) {
          src.as<Mem>().addOffset(imm * 32u);
          v_loadu256(dst, src.as<Mem>());
          return;
        }

        BL_ASSERT(src.as<Vec>().isZmm());
        cc->vextracti32x8(dst, src.as<Vec>(), imm);
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else {
    // SSE Implementation
    // ------------------

    InstId instId = opInfo.sseInstId;

    if (hasSSEExt(SSEExt(opInfo.sseExt))) {
      BL_ASSERT(instId != Inst::kIdNone);

      if (opInfo.sseOpCount == 2) {
        sseMov(this, dst, src);
        cc->emit(instId, dst, imm);
        return;
      }
      else if (opInfo.sseOpCount == 3) {
        cc->emit(instId, dst, src, imm);
        return;
      }

      BL_NOT_REACHED();
    }

    switch (op) {
      case OpcodeVVI::kSraI64: {
        // Intrinsic (SSE2).
        if (imm == 0) {
          sseMov(this, dst, src);
          return;
        }

        if (imm == 63) {
          cc->emit(Inst::kIdPshufd, dst, src, x86::shuffleImm(3, 3, 1, 1));
          cc->emit(Inst::kIdPsrad, dst, 31);
          return;
        }

        Vec tmp = newSimilarReg(dst, "@tmp");

        if (imm <= 32 && hasSSE4_1()) {
          sseMov(this, dst, src);
          sseMov(this, tmp, src.isReg() ? src.as<Vec>() : dst);
          cc->emit(Inst::kIdPsrad, tmp, blMin<uint32_t>(imm, 31u));
          cc->emit(Inst::kIdPsrlq, dst, imm);
          cc->emit(Inst::kIdPblendw, dst, tmp, 0xCC);
          return;
        }

        sseMov(this, dst, src);
        cc->emit(Inst::kIdPshufd, tmp, src.isReg() ? src.as<Vec>() : dst, x86::shuffleImm(3, 3, 1, 1));
        cc->emit(Inst::kIdPsrad, tmp, 31);
        cc->emit(Inst::kIdPsrlq, dst, imm);
        cc->emit(Inst::kIdPsllq, tmp, 64u - imm);
        cc->emit(Inst::kIdPor, dst, tmp);
        return;
      }

      case OpcodeVVI::kSwizzleU16x4: {
        // Intrinsic (SSE2).

        // TODO: [JIT] OPTIMIZATION: Use VPSHUFB instead where appropriate.
        uint32_t shufImm = shufImm4FromSwizzle(Swizzle4{imm});
        cc->emit(Inst::kIdPshuflw, dst, src, shufImm);
        cc->emit(Inst::kIdPshufhw, dst, dst, shufImm);
        return;
      }

      case OpcodeVVI::kSwizzleLoU16x4:
      case OpcodeVVI::kSwizzleHiU16x4:
      case OpcodeVVI::kSwizzleU32x4: {
        // Intrinsic (SSE2).
        BL_ASSERT(instId != Inst::kIdNone);

        uint32_t shufImm = shufImm4FromSwizzle(Swizzle4{imm});
        cc->emit(instId, dst, src, shufImm);
        return;
      }

      case OpcodeVVI::kSwizzleU64x2: {
        // Intrinsic (SSE2 | SSE3).
        if (Swizzle2{imm} == swizzle(1, 0)) {
          sseMov(this, dst, src);
        }
        else if (Swizzle2{imm} == swizzle(0, 0) && hasSSE3()) {
          cc->emit(Inst::kIdMovddup, dst, src);
        }
        else if (Swizzle2{imm} == swizzle(0, 0) && isSameVec(dst, src)) {
          cc->emit(Inst::kIdPunpcklqdq, dst, src);
        }
        else if (Swizzle2{imm} == swizzle(1, 1) && isSameVec(dst, src)) {
          cc->emit(Inst::kIdPunpckhqdq, dst, src);
        }
        else {
          uint32_t shufImm = shufImm4FromSwizzle(Swizzle2{imm});
          cc->emit(Inst::kIdPshufd, dst, src, shufImm);
        }
        return;
      }

      case OpcodeVVI::kSwizzleF32x4: {
        // Intrinsic (SSE2).
        uint32_t shufImm = shufImm4FromSwizzle(Swizzle4{imm});
        if (isSameVec(dst, src))
          cc->emit(Inst::kIdShufps, dst, dst, shufImm);
        else
          cc->emit(Inst::kIdPshufd, dst, src, shufImm);
        return;
      }

      case OpcodeVVI::kSwizzleF64x2: {
        // Intrinsic (SSE2 | SSE3).
        if (Swizzle2{imm} == swizzle(1, 0)) {
          sseMov(this, dst, src);
        }
        else if (Swizzle2{imm} == swizzle(0, 0) && hasSSE3()) {
          cc->emit(Inst::kIdMovddup, dst, src);
        }
        else if (Swizzle2{imm} == swizzle(0, 0) && isSameVec(dst, src)) {
          cc->emit(Inst::kIdUnpcklpd, dst, src);
        }
        else if (Swizzle2{imm} == swizzle(1, 1) && isSameVec(dst, src)) {
          cc->emit(Inst::kIdUnpckhpd, dst, src);
        }
        else if (isSameVec(dst, src)) {
          uint32_t shufImm = shufImm2FromSwizzle(Swizzle2{imm});
          cc->emit(Inst::kIdShufpd, dst, dst, shufImm);
        }
        else {
          uint32_t shufImm = shufImm4FromSwizzle(Swizzle2{imm});
          cc->emit(Inst::kIdPshufd, dst, src, shufImm);
        }
        return;
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
        // Not supported in SSE mode.
        BL_NOT_REACHED();

      default:
        BL_NOT_REACHED();
    }
  }
}

void PipeCompiler::emit_2vi(OpcodeVVI op, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept { emit_2vi_t(this, op, dst_, src_, imm); }
void PipeCompiler::emit_2vi(OpcodeVVI op, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept { emit_2vi_t(this, op, dst_, src_, imm); }

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 2VS
// ===========================================================

void PipeCompiler::emit_2vs(OpcodeVR op, const Operand_& dst_, const Operand_& src_, uint32_t idx) noexcept {
  OpcodeVInfo opInfo = opcodeInfo2VS[size_t(op)];

  Operand src(src_);
  Operand dst(dst_);

  if (hasAVX()) {
    // AVX Implementation
    // ------------------

    switch (op) {
      case OpcodeVR::kMov: {
        BL_ASSERT(dst.isReg());
        BL_ASSERT(src.isReg());

        if (dst.isGp() && src.isVec()) {
          if (dst.as<Reg>().size() <= 4)
            cc->emit(Inst::kIdVmovd, dst.as<Gp>().r32(), src.as<Vec>().xmm());
          else
            cc->emit(Inst::kIdVmovq, dst.as<Gp>().r64(), src.as<Vec>().xmm());
          return;
        }

        if (dst.isVec() && src.isGp()) {
          if (src.as<Reg>().size() <= 4)
            cc->emit(Inst::kIdVmovd, dst.as<Vec>().xmm(), src.as<Gp>().r32());
          else
            cc->emit(Inst::kIdVmovq, dst.as<Vec>().xmm(), src.as<Gp>().r64());
          return;
        }

        BL_NOT_REACHED();
      }

      case OpcodeVR::kMovU32:
      case OpcodeVR::kMovU64: {
        BL_ASSERT(dst.isReg());
        BL_ASSERT(src.isReg());

        if (dst.isGp() && src.isVec()) {
          if (op == OpcodeVR::kMovU32)
            cc->emit(Inst::kIdVmovd, dst.as<Gp>().r32(), src.as<Vec>().xmm());
          else
            cc->emit(Inst::kIdVmovq, dst.as<Gp>().r64(), src.as<Vec>().xmm());
          return;
        }

        if (dst.isVec() && src.isGp()) {
          if (op == OpcodeVR::kMovU32)
            cc->emit(Inst::kIdVmovd, dst.as<Vec>().xmm(), src.as<Gp>().r32());
          else
            cc->emit(Inst::kIdVmovq, dst.as<Vec>().xmm(), src.as<Gp>().r64());
          return;
        }

        BL_NOT_REACHED();
      }

      case OpcodeVR::kInsertU8:
      case OpcodeVR::kInsertU16:
      case OpcodeVR::kInsertU32:
      case OpcodeVR::kInsertU64: {
        BL_ASSERT(dst.isVec());
        BL_ASSERT(src.isGp());

        dst = dst.as<Vec>().xmm();

        if (op != OpcodeVR::kInsertU64)
          src = src.as<Gp>().r32();

        cc->emit(opInfo.avxInstId, dst, dst, src, idx);
        return;
      }

      case OpcodeVR::kExtractU8:
      case OpcodeVR::kExtractU16:
      case OpcodeVR::kExtractU32:
      case OpcodeVR::kExtractU64: {
        BL_ASSERT(dst.isGp());
        BL_ASSERT(src.isVec());

        src = src.as<Vec>().xmm();

        if (op != OpcodeVR::kExtractU64)
          dst = dst.as<Gp>().r32();

        if (op == OpcodeVR::kExtractU32 && idx == 0) {
          cc->vmovd(dst.as<Gp>(), src.as<Xmm>());
          return;
        }

        if (op == OpcodeVR::kExtractU64) {
          cc->vmovq(dst.as<Gp>(), src.as<Xmm>());
          return;
        }

        cc->emit(opInfo.avxInstId, dst, src, idx);
        return;
      }

      case OpcodeVR::kCvtIntToF32:
      case OpcodeVR::kCvtIntToF64: {
        dst = dst.as<Vec>().xmm();
        cc->emit(Inst::kIdVpxor, dst, dst, dst);
        cc->emit(opInfo.avxInstId, dst, dst, src);
        return;
      }

      case OpcodeVR::kCvtTruncF32ToInt:
      case OpcodeVR::kCvtRoundF32ToInt:
      case OpcodeVR::kCvtTruncF64ToInt:
      case OpcodeVR::kCvtRoundF64ToInt: {
        if (src.isVec())
          src = src.as<Vec>().xmm();

        cc->emit(opInfo.avxInstId, dst, src);
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else {
    // SSE Implementation
    // ------------------

    switch (op) {
      case OpcodeVR::kMov: {
        BL_ASSERT(dst.isReg());
        BL_ASSERT(src.isReg());

        if (dst.isGp() && src.isVec()) {
          if (dst.as<Reg>().size() <= 4)
            cc->emit(Inst::kIdMovd, dst.as<Gp>().r32(), src.as<Vec>().xmm());
          else
            cc->emit(Inst::kIdMovq, dst.as<Gp>().r64(), src.as<Vec>().xmm());
          return;
        }

        if (dst.isVec() && src.isGp()) {
          if (src.as<Reg>().size() <= 4)
            cc->emit(Inst::kIdMovd, dst.as<Vec>().xmm(), src.as<Gp>().r32());
          else
            cc->emit(Inst::kIdMovq, dst.as<Vec>().xmm(), src.as<Gp>().r64());
          return;
        }

        BL_NOT_REACHED();
      }

      case OpcodeVR::kMovU32:
      case OpcodeVR::kMovU64: {
        BL_ASSERT(dst.isReg());
        BL_ASSERT(src.isReg());

        if (dst.isGp() && src.isVec()) {
          if (op == OpcodeVR::kMovU32)
            cc->emit(Inst::kIdMovd, dst.as<Gp>().r32(), src.as<Vec>().xmm());
          else
            cc->emit(Inst::kIdMovq, dst.as<Gp>().r64(), src.as<Vec>().xmm());
          return;
        }

        if (dst.isVec() && src.isGp()) {
          if (op == OpcodeVR::kMovU32)
            cc->emit(Inst::kIdMovd, dst.as<Vec>().xmm(), src.as<Gp>().r32());
          else
            cc->emit(Inst::kIdMovq, dst.as<Vec>().xmm(), src.as<Gp>().r64());
          return;
        }

        BL_NOT_REACHED();
      }

      case OpcodeVR::kInsertU8:
      case OpcodeVR::kInsertU16:
      case OpcodeVR::kInsertU32:
      case OpcodeVR::kInsertU64: {
        BL_ASSERT(dst.isVec());
        BL_ASSERT(src.isGp());

        if (op != OpcodeVR::kInsertU64)
          src = src.as<Gp>().r32();

        if (hasSSEExt(SSEExt(opInfo.sseExt))) {
          cc->emit(opInfo.sseInstId, dst, src, idx);
        }
        else if (op == OpcodeVR::kInsertU8) {
          Gp tmp = newGp32("@tmp");
          cc->pextrw(tmp, dst.as<Xmm>(), idx / 2u);
          if (idx & 1)
            cc->mov(tmp.r8Hi(), src.as<Gp>().r8());
          else
            cc->mov(tmp.r8(), src.as<Gp>().r8());
          cc->pinsrw(dst.as<Xmm>(), tmp, idx / 2u);
        }
        else if (op == OpcodeVR::kInsertU32) {
          if (idx == 0) {
            Vec tmp = newV128("@tmp");
            cc->movd(tmp.as<Xmm>(), src.as<Gp>());
            cc->movss(dst.as<Xmm>(), tmp.as<Xmm>());
          }
          else {
            Gp tmp = newGp32("@tmp");
            cc->pinsrw(dst.as<Xmm>(), src.as<Gp>(), idx * 2u);
            cc->mov(tmp.as<Gp>(), src.as<Gp>());
            cc->shr(tmp.as<Gp>(), 16);
            cc->pinsrw(dst.as<Xmm>(), tmp, idx * 2u + 1u);
          }
        }
        else {
          Vec tmp = newV128("@tmp");
          cc->movq(tmp.as<Xmm>(), src.as<Gp>());

          if (idx == 0)
            cc->movsd(dst.as<Xmm>(), tmp.as<Xmm>());
          else
            cc->punpcklqdq(dst.as<Xmm>(), tmp.as<Xmm>());
        }

        return;
      }

      case OpcodeVR::kExtractU8:
      case OpcodeVR::kExtractU16:
      case OpcodeVR::kExtractU32:
      case OpcodeVR::kExtractU64: {
        BL_ASSERT(dst.isGp());
        BL_ASSERT(src.isVec());

        if (op != OpcodeVR::kExtractU64)
          dst = dst.as<Gp>().r32();

        if (op == OpcodeVR::kExtractU32 && idx == 0) {
          cc->movd(dst.as<Gp>(), src.as<Xmm>());
        }
        else if (op == OpcodeVR::kExtractU64 && idx == 0) {
          cc->movq(dst.as<Gp>(), src.as<Xmm>());
        }
        else if (hasSSEExt(SSEExt(opInfo.sseExt))) {
          cc->emit(opInfo.sseInstId, dst, src, idx);
        }
        else if (op == OpcodeVR::kExtractU8) {
          cc->pextrw(dst.as<Gp>(), src.as<Xmm>(), idx / 2u);
          if (idx & 1)
            cc->shr(dst.as<Gp>(), 8);
          else
            cc->and_(dst.as<Gp>(), 0xFF);
        }
        else if (op == OpcodeVR::kExtractU32) {
          Vec tmp = newSimilarReg(dst.as<Vec>(), "@tmp");
          cc->pshufd(tmp.as<Xmm>(), src.as<Xmm>(), x86::shuffleImm(idx, idx, idx, idx));
          cc->movd(dst.as<Gp>(), tmp.as<Xmm>());
        }
        else {
          Vec tmp = newSimilarReg(dst.as<Vec>(), "@tmp");
          cc->pshufd(tmp.as<Xmm>(), src.as<Xmm>(), x86::shuffleImm(3, 2, 3, 2));
          cc->movq(dst.as<Gp>(), tmp.as<Xmm>());
        }

        return;
      }

      case OpcodeVR::kCvtIntToF32:
      case OpcodeVR::kCvtIntToF64: {
        dst = dst.as<Vec>().xmm();
        cc->pxor(dst.as<Xmm>(), dst.as<Xmm>());
        cc->emit(opInfo.sseInstId, dst, src);
        return;
      }

      case OpcodeVR::kCvtTruncF32ToInt:
      case OpcodeVR::kCvtRoundF32ToInt:
      case OpcodeVR::kCvtTruncF64ToInt:
      case OpcodeVR::kCvtRoundF64ToInt: {
        cc->emit(opInfo.sseInstId, dst, src);
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }
}

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 2VM
// ===========================================================

void PipeCompiler::emit_vm(OpcodeVM op, const Vec& dst_, const Mem& src_, uint32_t alignment, uint32_t idx) noexcept {
  BL_ASSERT(dst_.isVec());
  BL_ASSERT(src_.isMem());

  Vec dst(dst_);
  Mem src(src_);
  OpcodeVMInfo opInfo = opcodeInfo2VM[size_t(op)];

  if (hasAVX()) {
    // AVX Implementation
    // ------------------

    switch (op) {
      case OpcodeVM::kLoad8: {
        dst = dst.xmm();
        src.setSize(1);
        avxZero(this, dst);
        cc->vpinsrb(dst.as<Xmm>(), dst.as<Xmm>(), src, 0);
        return;
      }

      case OpcodeVM::kLoad16_U16:
        if (!hasAVX512_FP16()) {
          dst = dst.xmm();
          src.setSize(1);
          avxZero(this, dst);
          cc->vpinsrw(dst.as<Xmm>(), dst.as<Xmm>(), src, 0);
        }
        BL_FALLTHROUGH
      case OpcodeVM::kLoad32_U32:
      case OpcodeVM::kLoad32_F32:
      case OpcodeVM::kLoad64_U32:
      case OpcodeVM::kLoad64_U64:
      case OpcodeVM::kLoad64_F32:
      case OpcodeVM::kLoad64_F64: {
        dst.setSignature(signatureOfXmmYmmZmm[0]);
        src.setSize(opInfo.memSize);
        cc->emit(opInfo.avxInstId, dst, src);
        return;
      }

      case OpcodeVM::kLoad128_U32:
      case OpcodeVM::kLoad128_U64:
      case OpcodeVM::kLoad128_F32:
      case OpcodeVM::kLoad128_F64:
      case OpcodeVM::kLoad256_U32:
      case OpcodeVM::kLoad256_U64:
      case OpcodeVM::kLoad256_F32:
      case OpcodeVM::kLoad256_F64:
      case OpcodeVM::kLoad512_U32:
      case OpcodeVM::kLoad512_U64:
      case OpcodeVM::kLoad512_F32:
      case OpcodeVM::kLoad512_F64:
        BL_ASSERT(dst.size() >= opInfo.memSize);
        dst.setSignature(signatureOfXmmYmmZmm[opInfo.memSize >> 5]);
        BL_FALLTHROUGH
      case OpcodeVM::kLoadN_U32:
      case OpcodeVM::kLoadN_U64:
      case OpcodeVM::kLoadN_F32:
      case OpcodeVM::kLoadN_F64: {
        src.setSize(dst.size());
        cc->emit((alignment == 0 || alignment >= dst.size()) ? Inst::kIdVmovaps : Inst::kIdVmovups, dst, src);
        return;
      }

      case OpcodeVM::kLoadCvt16_U8ToU64:
      case OpcodeVM::kLoadCvt32_U8ToU64:
      case OpcodeVM::kLoadCvt64_U8ToU64:
        dst.setSignature(signatureOfXmmYmmZmm[opInfo.memSize >> 2]);
        BL_FALLTHROUGH
      case OpcodeVM::kLoadCvtN_U8ToU64: {
        BL_ASSERT(dst.size() >= opInfo.memSize * 8u);
        src.setSize(dst.size() / 8u);
        cc->emit(opInfo.avxInstId, dst, src);
        return;
      }

      case OpcodeVM::kLoadCvt32_I8ToI32:
      case OpcodeVM::kLoadCvt32_U8ToU32:
      case OpcodeVM::kLoadCvt64_I8ToI32:
      case OpcodeVM::kLoadCvt64_U8ToU32:
      case OpcodeVM::kLoadCvt128_I8ToI32:
      case OpcodeVM::kLoadCvt128_U8ToU32:
        dst.setSignature(signatureOfXmmYmmZmm[opInfo.memSize >> 3]);
        BL_FALLTHROUGH
      case OpcodeVM::kLoadCvtN_I8ToI32:
      case OpcodeVM::kLoadCvtN_U8ToU32: {
        BL_ASSERT(dst.size() >= opInfo.memSize * 4u);
        src.setSize(dst.size() / 4u);
        cc->emit(opInfo.avxInstId, dst, src);
        return;
      }

      case OpcodeVM::kLoadCvt32_I8ToI16:
      case OpcodeVM::kLoadCvt32_U8ToU16:
      case OpcodeVM::kLoadCvt32_I16ToI32:
      case OpcodeVM::kLoadCvt32_U16ToU32:
      case OpcodeVM::kLoadCvt32_I32ToI64:
      case OpcodeVM::kLoadCvt32_U32ToU64: {
        dst.setSignature(signatureOfXmmYmmZmm[0]);
        src.setSize(4);
        cc->vmovd(dst.as<Xmm>(), src);
        cc->emit(opInfo.avxInstId, dst, dst);
        return;
      }

      case OpcodeVM::kLoadCvt64_I8ToI16:
      case OpcodeVM::kLoadCvt64_U8ToU16:
      case OpcodeVM::kLoadCvt64_I16ToI32:
      case OpcodeVM::kLoadCvt64_U16ToU32:
      case OpcodeVM::kLoadCvt64_I32ToI64:
      case OpcodeVM::kLoadCvt64_U32ToU64:
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
        BL_ASSERT(dst.size() >= opInfo.memSize * 2u);
        dst.setSignature(signatureOfXmmYmmZmm[opInfo.memSize >> 4]);
        BL_FALLTHROUGH
      case OpcodeVM::kLoadCvtN_I8ToI16:
      case OpcodeVM::kLoadCvtN_U8ToU16:
      case OpcodeVM::kLoadCvtN_I16ToI32:
      case OpcodeVM::kLoadCvtN_U16ToU32:
      case OpcodeVM::kLoadCvtN_I32ToI64:
      case OpcodeVM::kLoadCvtN_U32ToU64: {
        src.setSize(dst.size() / 2u);
        cc->emit(opInfo.avxInstId, dst, src);
        return;
      }

      case OpcodeVM::kLoadInsertU8:
      case OpcodeVM::kLoadInsertU16:
      case OpcodeVM::kLoadInsertU32:
      case OpcodeVM::kLoadInsertF32: {
        dst = dst.as<Vec>().xmm();
        cc->emit(opInfo.avxInstId, dst, dst, src, idx);
        return;
      }

      case OpcodeVM::kLoadInsertU64: {
        dst = dst.as<Vec>().xmm();
        if (is64Bit()) {
          cc->emit(opInfo.avxInstId, dst, dst, src, idx);
        }
        else {
          if (idx == 0)
            cc->vmovlpd(dst.as<Xmm>(), dst.as<Xmm>(), src);
          else
            cc->vmovhpd(dst.as<Xmm>(), dst.as<Xmm>(), src);
        }
        return;
      }

      case OpcodeVM::kLoadInsertF32x2: {
        if (idx == 0)
          cc->emit(Inst::kIdVmovlps, dst, dst, src);
        else
          cc->emit(Inst::kIdVmovhps, dst, dst, src);
        return;
      }

      case OpcodeVM::kLoadInsertF64: {
        if (idx == 0)
          cc->emit(Inst::kIdVmovlpd, dst, dst, src);
        else
          cc->emit(Inst::kIdVmovhpd, dst, dst, src);
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else {
    // SSE Implementation
    // ------------------

    BL_ASSERT(dst.isXmm());

    switch (op) {
      case OpcodeVM::kLoad8: {
        src.setSize(1);

        if (hasSSE4_1()) {
          cc->xorps(dst.as<Xmm>(), dst.as<Xmm>());
          cc->pinsrb(dst.as<Xmm>(), src, 0);
        }
        else {
          Gp tmp = newGp32("@tmp");
          cc->movzx(tmp, src);
          cc->movd(dst.as<Xmm>(), tmp);
        }
        return;
      }

      case OpcodeVM::kLoad16_U16: {
        src.setSize(2);
        cc->xorps(dst.as<Xmm>(), dst.as<Xmm>());
        cc->pinsrw(dst.as<Xmm>(), src, 0);
        return;
      }

      case OpcodeVM::kLoad32_U32:
      case OpcodeVM::kLoad32_F32:
      case OpcodeVM::kLoad64_U32:
      case OpcodeVM::kLoad64_U64:
      case OpcodeVM::kLoad64_F32:
      case OpcodeVM::kLoad64_F64: {
        src.setSize(opInfo.memSize);
        cc->emit(opInfo.sseInstId, dst, src);
        return;
      }

      case OpcodeVM::kLoad128_U32:
      case OpcodeVM::kLoad128_U64:
      case OpcodeVM::kLoad128_F32:
      case OpcodeVM::kLoad128_F64:
      case OpcodeVM::kLoadN_U32:
      case OpcodeVM::kLoadN_U64:
      case OpcodeVM::kLoadN_F32:
      case OpcodeVM::kLoadN_F64: {
        src.setSize(16);
        cc->emit((alignment == 0 || alignment >= 16) ? Inst::kIdMovaps : Inst::kIdMovups, dst, src);
        return;
      }

      case OpcodeVM::kLoadCvt16_U8ToU64:
      case OpcodeVM::kLoadCvtN_U8ToU64: {
        if (hasSSE4_1()) {
          src.setSize(2);
          cc->emit(opInfo.avxInstId, dst, src);
        }
        else {
          src.setSize(1);
          Gp tmp = newGp32("@tmp");
          cc->movzx(tmp, src);
          cc->movd(dst.as<Xmm>(), tmp);

          src.addOffset(1);
          cc->movzx(tmp, src);
          cc->pinsrw(dst.as<Xmm>(), src, 4);
        }
        return;
      }

      case OpcodeVM::kLoadCvt32_I8ToI32:
      case OpcodeVM::kLoadCvt32_U8ToU32:
      case OpcodeVM::kLoadCvtN_I8ToI32:
      case OpcodeVM::kLoadCvtN_U8ToU32:
        if (hasSSE4_1()) {
          src.setSize(4);
          cc->emit(opInfo.sseInstId, dst, src);
          return;
        }
        BL_FALLTHROUGH
      case OpcodeVM::kLoadCvt32_I8ToI16:
      case OpcodeVM::kLoadCvt32_U8ToU16:
      case OpcodeVM::kLoadCvt32_I16ToI32:
      case OpcodeVM::kLoadCvt32_U16ToU32:
      case OpcodeVM::kLoadCvt32_I32ToI64:
      case OpcodeVM::kLoadCvt32_U32ToU64: {
        src.setSize(4);
        cc->vmovd(dst.as<Xmm>(), src);
        sse_int_widen(this, dst, dst, WideningOp(opInfo.cvt));
        return;
      }

      case OpcodeVM::kLoadCvt64_I8ToI16:
      case OpcodeVM::kLoadCvt64_U8ToU16:
      case OpcodeVM::kLoadCvt64_I16ToI32:
      case OpcodeVM::kLoadCvt64_U16ToU32:
      case OpcodeVM::kLoadCvt64_I32ToI64:
      case OpcodeVM::kLoadCvt64_U32ToU64:
      case OpcodeVM::kLoadCvtN_I8ToI16:
      case OpcodeVM::kLoadCvtN_U8ToU16:
      case OpcodeVM::kLoadCvtN_I16ToI32:
      case OpcodeVM::kLoadCvtN_U16ToU32:
      case OpcodeVM::kLoadCvtN_I32ToI64:
      case OpcodeVM::kLoadCvtN_U32ToU64: {
        src.setSize(8);
        if (hasSSE4_1()) {
          InstId inst = opInfo.sseInstId;
          cc->emit(inst, dst, src);
        }
        else {
          cc->movq(dst.as<Xmm>(), src);
          sse_int_widen(this, dst, dst, WideningOp(opInfo.cvt));
        }
        return;
      }

      case OpcodeVM::kLoadInsertU16: {
        cc->emit(opInfo.sseInstId, dst, dst, idx);
        return;
      }

      case OpcodeVM::kLoadInsertF32:
        op = OpcodeVM::kLoadInsertU32;
        BL_FALLTHROUGH
      case OpcodeVM::kLoadInsertU8:
      case OpcodeVM::kLoadInsertU32:
      case OpcodeVM::kLoadInsertU64: {
        if (hasSSE4_1() && (op != OpcodeVM::kLoadInsertU64 || is64Bit())) {
          cc->emit(opInfo.sseInstId, dst, src, idx);
          return;
        }

        if (op == OpcodeVM::kLoadInsertU8) {
          Gp tmp = newGp32("@tmp");
          src.setSize(1);
          cc->pextrw(tmp, dst.as<Xmm>(), idx / 2u);
          if (idx & 1)
            cc->mov(tmp.r8Hi(), src);
          else
            cc->mov(tmp.r8(), src);
          cc->pinsrw(dst.as<Xmm>(), tmp, idx / 2u);
          return;
        }

        if (op == OpcodeVM::kLoadInsertU32) {
          if (idx == 0) {
            Vec tmp = newV128("@tmp");
            cc->movd(tmp.as<Xmm>(), src);
            cc->movss(dst.as<Xmm>(), tmp.as<Xmm>());
          }
          else {
            cc->pinsrw(dst.as<Xmm>(), src, idx * 2u);
            src.addOffset(2);
            cc->pinsrw(dst.as<Xmm>(), src, idx * 2u + 1);
          }
          return;
        }

        BL_ASSERT(op == OpcodeVM::kLoadInsertU64);
        if (idx == 0)
          cc->movlpd(dst.as<Xmm>(), src);
        else
          cc->movhpd(dst.as<Xmm>(), src);

        return;
      }

      case OpcodeVM::kLoadInsertF32x2: {
        if (idx == 0)
          cc->movlps(dst.as<Xmm>(), src);
        else
          cc->movhps(dst.as<Xmm>(), src);
        return;
      }

      case OpcodeVM::kLoadInsertF64: {
        if (idx == 0)
          cc->movlpd(dst.as<Xmm>(), src);
        else
          cc->movhpd(dst.as<Xmm>(), src);
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
      case OpcodeVM::kLoadCvt128_I8ToI16:
      case OpcodeVM::kLoadCvt128_U8ToU16:
      case OpcodeVM::kLoadCvt128_I8ToI32:
      case OpcodeVM::kLoadCvt128_U8ToU32:
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
        BL_NOT_REACHED();

      default:
        BL_NOT_REACHED();
    }
  }
}

void PipeCompiler::emit_vm(OpcodeVM op, const OpArray& dst_, const Mem& src_, uint32_t alignment, uint32_t idx) noexcept {
  Mem src(src_);

  OpcodeVMInfo opInfo = opcodeInfo2VM[size_t(op)];
  uint32_t memSize = opInfo.memSize;

  if (memSize == 0) {
    uint32_t memSizeShift = opInfo.memSizeShift;
    for (uint32_t i = 0, n = dst_.size(); i < n; i++) {
      BL_ASSERT(dst_[i].isReg() && dst_[i].isVec());

      const Vec& dst = dst_[i].as<Vec>();
      memSize = dst.size() >> memSizeShift;

      emit_vm(op, dst, src, alignment > 0 ? alignment : memSize, idx);
      src.addOffsetLo32(int32_t(memSize));
    }
  }
  else {
    if (alignment == 0)
      alignment = memSize;

    for (uint32_t i = 0, n = dst_.size(); i < n; i++) {
      BL_ASSERT(dst_[i].isReg() && dst_[i].isVec());

      const Vec& dst = dst_[i].as<Vec>();
      emit_vm(op, dst, src, alignment, idx);
      src.addOffsetLo32(int32_t(memSize));
    }
  }
}

void PipeCompiler::emit_mv(OpcodeMV op, const Mem& dst_, const Vec& src_, uint32_t alignment, uint32_t idx) noexcept {
  BL_ASSERT(dst_.isMem());
  BL_ASSERT(src_.isReg() && src_.isVec());

  Mem dst(dst_);
  Vec src(src_);
  OpcodeVMInfo opInfo = opcodeInfo2MV[size_t(op)];

  if (hasAVX()) {
    // AVX Implementation
    // ------------------

    switch (op) {
      case OpcodeMV::kStore8: {
        dst.setSize(1);
        cc->vpextrb(dst, src.xmm(), 0);
        return;
      }

      case OpcodeMV::kStore16_U16: {
        dst.setSize(2);
        cc->vpextrw(dst, src.xmm(), 0);
        return;
      }

      case OpcodeMV::kStore32_U32:
      case OpcodeMV::kStore32_F32:
      case OpcodeMV::kStore64_U32:
      case OpcodeMV::kStore64_U64:
      case OpcodeMV::kStore64_F32:
      case OpcodeMV::kStore64_F64: {
        dst.setSize(opInfo.memSize);
        cc->emit(opInfo.avxInstId, dst, src.xmm());
        return;
      }

      case OpcodeMV::kStore128_U32:
      case OpcodeMV::kStore128_U64:
      case OpcodeMV::kStore128_F32:
      case OpcodeMV::kStore128_F64:
      case OpcodeMV::kStore256_U32:
      case OpcodeMV::kStore256_U64:
      case OpcodeMV::kStore256_F32:
      case OpcodeMV::kStore256_F64:
      case OpcodeMV::kStore512_U32:
      case OpcodeMV::kStore512_U64:
      case OpcodeMV::kStore512_F32:
      case OpcodeMV::kStore512_F64:
        BL_ASSERT(src.size() >= opInfo.memSize);
        src.setSignature(signatureOfXmmYmmZmm[opInfo.memSize >> 5]);
        BL_FALLTHROUGH
      case OpcodeMV::kStoreN_U32:
      case OpcodeMV::kStoreN_U64:
      case OpcodeMV::kStoreN_F32:
      case OpcodeMV::kStoreN_F64: {
        InstId inst = (alignment == 0 || alignment >= src.size()) ? Inst::kIdVmovaps : Inst::kIdVmovups;
        dst.setSize(src.size());
        cc->emit(inst, dst, src);
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
      */

      case OpcodeMV::kStoreExtractU16:
      case OpcodeMV::kStoreExtractU32:
      case OpcodeMV::kStoreExtractU64: {
        src = src.xmm();

        if (op == OpcodeMV::kStoreExtractU32) {
          if (idx == 0) {
            cc->vmovd(dst, src.as<Xmm>());
            return;
          }
        }

        if (op == OpcodeMV::kStoreExtractU64) {
          if (idx == 0) {
            cc->vmovq(dst, src.as<Xmm>());
            return;
          }
          else if (!is64Bit()) {
            cc->vmovhpd(dst, src.as<Xmm>());
            return;
          }
        }

        cc->emit(opInfo.avxInstId, dst, src, idx);
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else {
    // SSE Implementation
    // ------------------

    BL_ASSERT(src.isXmm());

    switch (op) {
      case OpcodeMV::kStore8: {
        dst.setSize(1);

        if (hasSSE4_1()) {
          cc->pextrb(dst, src.as<Xmm>(), 0);
        }
        else {
          Gp tmp = newGp32("@tmp");
          cc->movd(tmp, src.as<Xmm>());
          cc->mov(dst, tmp.r8());
        }
        return;
      }

      case OpcodeMV::kStore16_U16: {
        dst.setSize(2);
        if (hasSSE4_1()) {
          cc->pextrw(dst, src.as<Xmm>(), 0);
        }
        else {
          Gp tmp = newGp32("@tmp");
          cc->movd(tmp, src.as<Xmm>());
          cc->mov(dst, tmp.r16());
        }
        return;
      }

      case OpcodeMV::kStore32_U32:
      case OpcodeMV::kStore32_F32:
      case OpcodeMV::kStore64_U32:
      case OpcodeMV::kStore64_U64:
      case OpcodeMV::kStore64_F32:
      case OpcodeMV::kStore64_F64: {
        dst.setSize(opInfo.memSize);
        cc->emit(opInfo.sseInstId, dst, src);
        return;
      }

      case OpcodeMV::kStore128_U32:
      case OpcodeMV::kStore128_U64:
      case OpcodeMV::kStore128_F32:
      case OpcodeMV::kStore128_F64:
      case OpcodeMV::kStoreN_U32:
      case OpcodeMV::kStoreN_U64:
      case OpcodeMV::kStoreN_F32:
      case OpcodeMV::kStoreN_F64: {
        InstId inst = (alignment == 0 || alignment >= 16) ? Inst::kIdMovaps : Inst::kIdMovups;
        dst.setSize(16);
        cc->emit(inst, dst, src);
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
      case OpcodeMV::kStoreCvtsN_U64ToU32: {
        UNIMPLEMENTED();
        return;
      }
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
        BL_NOT_REACHED();

      case OpcodeMV::kStoreExtractU16:
      case OpcodeMV::kStoreExtractU32:
      case OpcodeMV::kStoreExtractU64: {
        if (op == OpcodeMV::kStoreExtractU32) {
          if (idx == 0) {
            cc->movd(dst, src.as<Xmm>());
            return;
          }
        }

        if (op == OpcodeMV::kStoreExtractU64) {
          if (idx == 0) {
            cc->movq(dst, src.as<Xmm>());
            return;
          }

          if (idx == 1) {
            cc->movhps(dst, src.as<Xmm>());
            return;
          }
        }

        if (hasSSE4_1()) {
          cc->emit(opInfo.sseInstId, dst, src, idx);
          return;
        }

        // SSE4.1 not available - only required when extracting 16-bit and 32-bit quantities as 64-bit quantities
        // were already handled. Additionally, there is no PEXTRW instruction in SSE2 that would extract to memory,
        // this instruction was added by SSE4.1 as well (there are actually two forms of PEXTRW).
        if (op == OpcodeMV::kStoreExtractU16) {
          Gp tmp = newGp32("@pextrw_tmp");
          cc->pextrw(tmp, src.as<Xmm>(), idx);
          cc->mov(dst, tmp);
          return;
        }

        if (op == OpcodeMV::kStoreExtractU32) {
          Vec tmp = newV128("@pextrd_tmp");
          cc->pshufd(tmp.as<Xmm>(), src.as<Xmm>(), x86::shuffleImm(idx, idx, idx, idx));
          cc->movd(dst, tmp.as<Xmm>());
          return;
        }

        BL_NOT_REACHED();
      }

      default:
        BL_NOT_REACHED();
    }
  }
}

void PipeCompiler::emit_mv(OpcodeMV op, const Mem& dst_, const OpArray& src_, uint32_t alignment, uint32_t idx) noexcept {
  blUnused(idx);

  Mem dst(dst_);

  OpcodeVMInfo opInfo = opcodeInfo2MV[size_t(op)];
  uint32_t memSize = opInfo.memSize;

  if (memSize == 0) {
    for (uint32_t i = 0, n = src_.size(); i < n; i++) {
      BL_ASSERT(src_[i].isReg() && src_[i].isVec());

      const Vec& src = src_[i].as<Vec>();
      memSize = src.size();

      emit_mv(op, dst, src, alignment > 0 ? alignment : memSize);
      dst.addOffsetLo32(int32_t(memSize));
    }
  }
  else {
    if (alignment == 0)
      alignment = memSize;

    for (uint32_t i = 0, n = src_.size(); i < n; i++) {
      BL_ASSERT(src_[i].isReg() && src_[i].isVec());

      const Vec& src = src_[i].as<Vec>();
      emit_mv(op, dst, src, alignment);
      dst.addOffsetLo32(int32_t(memSize));
    }
  }
}

// bl::Pipeline::PipeCompiler - Vector Instructions - Emit 3V
// ==========================================================

void PipeCompiler::emit_3v(OpcodeVVV op, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_) noexcept {
  BL_ASSERT(dst_.isVec());
  BL_ASSERT(src1_.isVec());

  Vec dst(dst_.as<Vec>());
  Vec src1v(src1_.as<Vec>().cloneAs(dst));
  Operand src2(src2_);
  OpcodeVInfo opInfo = opcodeInfo3V[size_t(op)];

  if (hasAVX()) {
    // AVX Implementation
    // ------------------

    InstId instId = opInfo.avxInstId;

    static constexpr InstId avxVpmovm2vTable[] = {
      Inst::kIdVpmovm2b,
      Inst::kIdVpmovm2w,
      Inst::kIdVpmovm2d,
      Inst::kIdVpmovm2q
    };

    if (isSameVec(src1v, src2)) {
      switch (SameVecOp(opInfo.sameVecOp)) {
        case SameVecOp::kZero: avxZero(this, dst); return;
        case SameVecOp::kOnes: avxOnes(this, dst); return;
        case SameVecOp::kSrc: avxMov(this, dst, src1v); return;

        default:
          break;
      }
    }

    if (hasAVXExt(AVXExt(opInfo.avxExt))) {
      BL_ASSERT(instId != Inst::kIdNone);

      FloatMode fm = FloatMode(opInfo.floatMode);

      if (fm == FloatMode::kF32S || fm == FloatMode::kF64S) {
        dst.setSignature(signatureOfXmmYmmZmm[0]);
        src1v.setSignature(signatureOfXmmYmmZmm[0]);

        if (src2.isVec())
          src2.as<Vec>().setSignature(signatureOfXmmYmmZmm[0]);
      }

      if (op >= OpcodeVVV::kAndU32 && op <= OpcodeVVV::kAndnU64 && !hasAVX512()) {
        static constexpr uint16_t avx512ToAvxBitwiseMap[] = {
          Inst::kIdVpand , Inst::kIdVpand ,
          Inst::kIdVpor  , Inst::kIdVpor  ,
          Inst::kIdVpxor , Inst::kIdVpxor ,
          Inst::kIdVpandn, Inst::kIdVpandn
        };
        instId = avx512ToAvxBitwiseMap[size_t(op) - size_t(OpcodeVVV::kAndU32)];
      }

      if (opInfo.comparison && ((dst.isZmm()) ||
                                (src2.isMem() && src2.as<Mem>().hasBroadcast()) ||
                                (AVXExt(opInfo.avxExt) == AVXExt::kAVX512))) {
        // AVX-512 instructions change semantics when it comes to comparisons. Instead of having a VEC destination
        // we need a K destination. To not change semantics to our users we just convert the predicate to a VEC mask.
        KReg kTmp = cc->newKq("@kTmp");
        InstId kMovM = avxVpmovm2vTable[opInfo.elementSize];

        if (opInfo.useImm)
          cc->emit(instId, kTmp, src1v, src2, Imm(opInfo.imm));
        else
          cc->emit(instId, kTmp, src1v, src2);

        cc->emit(kMovM, dst, kTmp);
        return;
      }

      if (opInfo.useImm)
        cc->emit(instId, dst, src1v, src2, Imm(opInfo.imm));
      else
        cc->emit(instId, dst, src1v, src2);
      return;
    }

    switch (op) {
      case OpcodeVVV::kBicU32:
      case OpcodeVVV::kBicU64:
      case OpcodeVVV::kBicF32:
      case OpcodeVVV::kBicF64: {
        if (hasAVX512()) {
          uint32_t ternlogInst = ElementSize(opInfo.elementSize) == ElementSize::k32 ? Inst::kIdVpternlogd : Inst::kIdVpternlogq;
          if (src2.isMem())
            cc->emit(ternlogInst, dst, src1v, src2.as<Mem>(), 0x44);
          else
            cc->emit(instId, dst, src2, src1v);
          return;
        }

        if (op <= OpcodeVVV::kBicU64)
          instId = Inst::kIdVpandn;

        if (src2.isMem()) {
          src2 = PipeCompiler_loadNew(this, dst, src2.as<Mem>(), opInfo.bcstSize);
        }

        cc->emit(instId, dst, src2, src1v);
        return;
      }

      case OpcodeVVV::kMulU64: {
        // Native operation requires AVX512, which is not supported by the target.
        if (src2.isMem()) {
          src2 = PipeCompiler_loadNew(this, dst, src2.as<Mem>(), opInfo.bcstSize);
        }

        Vec src2v = src2.as<Vec>().cloneAs(dst);
        Vec al_bh = newSimilarReg(dst, "@al_bh");
        Vec ah_bl = newSimilarReg(dst, "@ah_bl");
        Vec hi_part = newSimilarReg(dst, "@hi_part");

        cc->vpsrlq(al_bh, src2v, 32);
        cc->vpsrlq(ah_bl, src1v, 32);

        cc->vpmuludq(al_bh, al_bh, src1v);
        cc->vpmuludq(ah_bl, ah_bl, src2v);
        cc->vpmuludq(dst, src1v, src2v);

        cc->vpaddq(hi_part, al_bh, ah_bl);
        cc->vpsllq(hi_part, hi_part, 32);
        cc->vpaddq(dst, dst, hi_part);

        return;
      }

      case OpcodeVVV::kMulU64_LoU32: {
        // Intrinsic.
        Vec tmp = newSimilarReg(dst.as<Vec>(), "@tmp");

        if (hasAVX512()) {
          Vec msk = simdVecConst(&ct.i_FFFFFFFF00000000, Bcst::k64, dst);
          cc->emit(Inst::kIdVpandnq, tmp, msk, src2);
          cc->emit(Inst::kIdVpmullq, dst, src1v, tmp);
        }
        else {
          cc->emit(Inst::kIdVpshufd, tmp, src1v, x86::shuffleImm(2, 3, 0, 1));
          cc->emit(Inst::kIdVpmuludq, tmp, tmp, src2);
          cc->emit(Inst::kIdVpmuludq, dst, src1v, src2);
          cc->emit(Inst::kIdVpsllq, tmp, tmp, 32);
          cc->emit(Inst::kIdVpaddq, dst, dst, tmp);
        }
        return;
      }

      case OpcodeVVV::kMinI64:
      case OpcodeVVV::kMaxI64: {
        // Native operation requires AVX512, which is not supported by the target.
        if (src2.isMem()) {
          src2 = PipeCompiler_loadNew(this, dst, src2.as<Mem>(), opInfo.bcstSize);
        }

        BL_ASSERT(src2.isVec());
        Vec src2v = src2.as<Vec>().cloneAs(dst);

        Vec msk = dst;
        if (dst.id() == src1v.id() || dst.id() == src2v.id()) {
          msk = newSimilarReg(dst, "@msk");
        }

        cc->vpcmpgtq(msk, src1v, src2v);          // msk = src1 > src2
        if (op == OpcodeVVV::kMinI64)
          cc->vblendvpd(dst, src1v, src2v, msk);  // dst = msk == 0 ? src1 : src2;
        else
          cc->vblendvpd(dst, src2v, src1v, msk);  // dst = msk == 0 ? src2 : src1;
        return;
      }

      case OpcodeVVV::kMinU64:
      case OpcodeVVV::kMaxU64: {
        if (src2.isMem()) {
          src2 = PipeCompiler_loadNew(this, dst, src2.as<Mem>(), opInfo.bcstSize);
        }

        BL_ASSERT(src2.isVec());
        Vec src2v = src2.as<Vec>().cloneAs(dst);

        Vec tmp1 = dst;
        Vec tmp2 = newSimilarReg(dst, "@tmp2");

        if (dst.id() == src1v.id() || dst.id() == src2v.id()) {
          tmp1 = newSimilarReg(dst, "@tmp1");
        }

        avxISignFlip(this, tmp1, src1v, ElementSize::k64);
        avxISignFlip(this, tmp2, src2v, ElementSize::k64);

        cc->vpcmpgtq(tmp1, tmp1, tmp2);           // tmp1 = src1 > src2
        if (op == OpcodeVVV::kMinU64)
          cc->vblendvpd(dst, src1v, src2v, tmp1); // dst = tmp1 == 0 ? src1 : src2;
        else
          cc->vblendvpd(dst, src2v, src1v, tmp1); // dst = tmp1 == 0 ? src2 : src1;
        return;
      }

      case OpcodeVVV::kCmpGtU8:
      case OpcodeVVV::kCmpGtU16:
      case OpcodeVVV::kCmpGtU32: {
        // Native operation requires AVX512, which is not supported by the target.
        CmpMinMaxInst inst = avx_cmp_min_max[(size_t(op) - size_t(OpcodeVVV::kCmpGtI8)) & 0x7u];
        if (isSameVec(dst, src1v)) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          cc->emit(inst.pmin, tmp, src1v, src2);
          cc->emit(inst.peq, dst, dst, tmp);
        }
        else {
          cc->emit(inst.pmin, dst, src1v, src2);
          cc->emit(inst.peq, dst, dst, src1v);
        }
        avxBitNot(this, dst, dst);
        return;
      }

      case OpcodeVVV::kCmpGtU64:
      case OpcodeVVV::kCmpLeU64: {
        Vec tmp = newSimilarReg(dst, "@tmp");
        avxISignFlip(this, tmp, src2, ElementSize::k64);
        avxISignFlip(this, dst, src1v, ElementSize::k64);
        cc->emit(Inst::kIdVpcmpgtq, dst, dst, tmp);

        if (op == OpcodeVVV::kCmpLeU64) {
          avxBitNot(this, dst, dst);
        }
        return;
      }

      case OpcodeVVV::kCmpGeI8:
      case OpcodeVVV::kCmpGeU8:
      case OpcodeVVV::kCmpGeI16:
      case OpcodeVVV::kCmpGeU16:
      case OpcodeVVV::kCmpGeI32:
      case OpcodeVVV::kCmpGeU32: {
        CmpMinMaxInst inst = avx_cmp_min_max[(size_t(op) - size_t(OpcodeVVV::kCmpGeI8)) & 0x7u];

        if (dst.id() == src1v.id()) {
          if (!src2.isReg()) {
            Vec tmp = newSimilarReg(dst, "@tmp");
            cc->emit(inst.pmax, tmp, src1v, src2);
            cc->emit(inst.peq, dst, tmp, src1v);
          }
          else {
            cc->emit(inst.pmin, dst, src1v, src2);
            cc->emit(inst.peq, dst, dst, src2);
          }
        }
        else {
          cc->emit(inst.pmax, dst, src1v, src2);
          cc->emit(inst.peq, dst, dst, src1v);
        }

        return;
      }

      case OpcodeVVV::kCmpLtI8:
      case OpcodeVVV::kCmpLtI16:
      case OpcodeVVV::kCmpLtI32:
      case OpcodeVVV::kCmpLtI64:
      case OpcodeVVV::kCmpGeI64: {
        if (!src2.isReg()) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          avxMov(this, tmp, src2);
          src2 = tmp;
        }

        CmpMinMaxInst inst = avx_cmp_min_max[(size_t(op) - size_t(OpcodeVVV::kCmpLtI8)) & 0x7u];
        cc->emit(inst.pgt, dst, src2, src1v);

        if (op == OpcodeVVV::kCmpGeI64) {
          avxBitNot(this, dst, dst);
        }
        return;
      }

      case OpcodeVVV::kCmpLtU8:
      case OpcodeVVV::kCmpLtU16:
      case OpcodeVVV::kCmpLtU32:
      case OpcodeVVV::kCmpLtU64:
      case OpcodeVVV::kCmpGeU64: {
        Vec tmp = newSimilarReg(dst, "@tmp");
        avxISignFlip(this, tmp, src2, ElementSize(opInfo.elementSize));
        avxISignFlip(this, dst, src1v, ElementSize(opInfo.elementSize));

        CmpMinMaxInst inst = avx_cmp_min_max[(size_t(op) - size_t(OpcodeVVV::kCmpLtI8)) & 0x7u];
        cc->emit(inst.pgt, dst, tmp, dst);

        if (op == OpcodeVVV::kCmpGeU64) {
          avxBitNot(this, dst, dst);
        }
        return;
      }

      case OpcodeVVV::kCmpLeI8:
      case OpcodeVVV::kCmpLeU8:
      case OpcodeVVV::kCmpLeI16:
      case OpcodeVVV::kCmpLeU16:
      case OpcodeVVV::kCmpLeI32:
      case OpcodeVVV::kCmpLeU32: {
        CmpMinMaxInst inst = avx_cmp_min_max[(size_t(op) - size_t(OpcodeVVV::kCmpLeI8)) & 0x7u];

        if (dst.id() == src1v.id()) {
          if (!src2.isReg()) {
            Vec tmp = newSimilarReg(dst, "@tmp");
            cc->emit(inst.pmin, tmp, src1v, src2);
            cc->emit(inst.peq, dst, tmp, src1v);
          }
          else {
            cc->emit(inst.pmax, dst, src1v, src2);
            cc->emit(inst.peq, dst, dst, src2);
          }
        }
        else {
          cc->emit(inst.pmin, dst, src1v, src2);
          cc->emit(inst.peq, dst, dst, src1v);
        }

        return;
      }

      case OpcodeVVV::kCmpLeI64: {
        cc->emit(Inst::kIdVpcmpgtq, dst, src1v, src2);

        avxBitNot(this, dst, dst);
        return;
      }

      case OpcodeVVV::kHAddF64: {
        if (hasAVX512() && dst.isVec512()) {
          // [B A]    [C A]
          // [D C] -> [D B]
          Vec tmp = newSimilarReg(dst, "@tmp");

          cc->emit(Inst::kIdVunpckhpd, tmp, src1v, src2);
          cc->emit(Inst::kIdVunpcklpd, dst, src1v, src2);
          cc->vaddpd(dst, dst, tmp);
        }
        else {
          cc->emit(instId, dst, src1v, src2);
        }
        return;
      }

      case OpcodeVVV::kCombineLoHiU64:
      case OpcodeVVV::kCombineLoHiF64: {
        // Intrinsic - dst = {src1.u64[0], src2.64[1]} - combining low part of src1 and high part of src1.
        if (!src2.isReg()) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          avxMov(this, tmp, src2);
          src2 = tmp;
        }

        uint32_t shufImm = shufImm2FromSwizzleWithWidth(swizzle(0, 1), VecWidthUtils::vecWidthOf(dst));
        cc->emit(Inst::kIdVshufpd, dst, src2, src1v, shufImm);
        return;
      }

      case OpcodeVVV::kCombineHiLoU64:
      case OpcodeVVV::kCombineHiLoF64: {
        // Intrinsic - dst = {src1.u64[1], src2.u64[0]} - combining high part of src1 and low part of src2.
        if (dst.isXmm()) {
          if (src2.isVec())
            cc->emit(Inst::kIdVmovsd, dst, src1v.xmm(), src2.as<Vec>().xmm());
          else
            cc->emit(Inst::kIdVmovlpd, dst, src1v.xmm(), src2);
          return;
        }

        if (!src2.isReg()) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          avxMov(this, tmp, src2);
          src2 = tmp;
        }

        uint32_t shufImm = shufImm2FromSwizzleWithWidth(swizzle(1, 0), VecWidthUtils::vecWidthOf(dst));
        cc->emit(Inst::kIdVshufpd, dst, src2, src1v, shufImm);
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else {
    // SSE Implementation
    // ------------------

    InstId instId = opInfo.sseInstId;

    // SSE floating point comparison cannot use the extended predicates as introduced by AVX.
    static constexpr uint8_t sse_fcmp_imm_table[] = {
      0x00u, // kCmpEq    (eq ordered quiet).
      0x04u, // kCmpNe    (ne ordered quiet).
      0x01u, // kCmpGt    (lt ordered quiet <reversed>).
      0x02u, // kCmpGe    (le ordered quiet <reversed>).
      0x01u, // kCmpLt    (lt ordered quiet).
      0x02u, // kCmpLe    (le ordered quiet).
      0x07u, // kCmpOrd   (ordered quiet).
      0x03u  // kCmpUnord (unordered quiet).
    };

    if (isSameVec(dst, src2) && opInfo.commutative) {
      BLInternal::swap(src1v, src2.as<Vec>());
    }

    if (isSameVec(src1v, src2)) {
      switch (SameVecOp(opInfo.sameVecOp)) {
        case SameVecOp::kZero:
          cc->emit(Inst::kIdPxor, dst, dst);
          return;

        case SameVecOp::kOnes:
          cc->emit(Inst::kIdPcmpeqb, dst, dst);
          return;

        case SameVecOp::kSrc:
          sseMov(this, dst, src1v);
          return;

        default:
          break;
      }
    }

    if (hasSSEExt(SSEExt(opInfo.sseExt))) {
      BL_ASSERT(instId != Inst::kIdNone);

      if (!isSameVec(dst, src1v)) {
        if (isSameVec(dst, src2)) {
          Vec tmp = newSimilarReg(dst, "tmp");
          sseMov(this, tmp, src2);
          src2 = tmp;
        }

        sseMov(this, dst, src1v);
      }

      if (opInfo.useImm)
        cc->emit(instId, dst, src2, Imm(opInfo.imm));
      else
        cc->emit(instId, dst, src2);
      return;
    }

    switch (op) {
      case OpcodeVVV::kBicU32:
      case OpcodeVVV::kBicU64:
      case OpcodeVVV::kBicF32:
      case OpcodeVVV::kBicF64: {
        if (isSameVec(dst, src2)) {
          cc->emit(instId, dst, src1v);
          return;
        }

        if (isSameVec(dst, src1v)) {
          Vec tmp = newSimilarReg(dst);
          sseMov(this, tmp, src1v);
          src1v = tmp;
        }

        sseMov(this, dst, src2);
        cc->emit(instId, dst, src1v);
        return;
      }

      case OpcodeVVV::kMulU32: {
        // Native operation requires SSE4.1, which is not supported by the target.
        Vec tmp1 = newSimilarReg(dst, "tmp1");
        Vec tmp2 = newSimilarReg(dst, "tmp2");

        cc->emit(Inst::kIdPshufd, tmp1, src1v, x86::shuffleImm(3, 3, 1, 1));
        cc->emit(Inst::kIdPshufd, tmp2, src2, x86::shuffleImm(3, 3, 1, 1));
        cc->emit(Inst::kIdPmuludq, tmp1, tmp2);

        sseMov(this, dst, src1v);
        cc->emit(Inst::kIdPmuludq, dst, src2);
        cc->emit(Inst::kIdShufps, dst, tmp1, x86::shuffleImm(2, 0, 2, 0));
        cc->emit(Inst::kIdPshufd, dst, dst, x86::shuffleImm(3, 1, 2, 0));
        return;
      }

      case OpcodeVVV::kMulU64: {
        // Native operation requires AVX512, which is not supported by the target.
        Vec al_bh = newSimilarReg(dst, "@al_bh");
        Vec ah_bl = newSimilarReg(dst, "@ah_bl");

        cc->emit(Inst::kIdPshufd, al_bh, src2, x86::shuffleImm(3, 3, 1, 1));
        cc->emit(Inst::kIdPshufd, ah_bl, src1v, x86::shuffleImm(3, 3, 1, 1));

        cc->emit(Inst::kIdPmuludq, al_bh, src1v);
        cc->emit(Inst::kIdPmuludq, ah_bl, src2);
        cc->emit(Inst::kIdPaddq, al_bh, ah_bl);

        sseMov(this, dst, src1v);
        cc->emit(Inst::kIdPmuludq, dst, src2);
        cc->emit(Inst::kIdPsllq, al_bh, 32);
        cc->emit(Inst::kIdPaddq, dst, al_bh);
        return;
      }

      case OpcodeVVV::kMulU64_LoU32: {
        Vec tmp = newSimilarReg(dst.as<Vec>(), "@tmp");

        cc->emit(Inst::kIdPshufd, tmp, src1v, x86::shuffleImm(2, 3, 0, 1));
        cc->emit(Inst::kIdPmuludq, tmp, src2);

        if (dst.id() == src2.id()) {
          cc->emit(Inst::kIdPmuludq, dst, src1v);
        }
        else {
          sseMov(this, dst, src1v);
          cc->emit(Inst::kIdPmuludq, dst, src2);
        }
        cc->emit(Inst::kIdPsllq, tmp, 32);
        cc->emit(Inst::kIdPaddq, dst, tmp);

        return;
      }

      // Native operation requires AVX512, which is not supported by the target.
      case OpcodeVVV::kMinI64:
        if (!hasSSE4_2()) {
          Vec msk = newV128("@msk");
          sseCmpGtI64(this, msk, src2, src1v);
          sseSelect(this, dst, src1v, src2, msk);
          return;
        }
        BL_FALLTHROUGH
      case OpcodeVVV::kMinI8:
      case OpcodeVVV::kMinI32: {
        // Native operation requires SSE4.1, which is not supported by the target.
        InstId cmpInstId = op == OpcodeVVV::kMinI8  ? Inst::kIdPcmpgtb :
                           op == OpcodeVVV::kMinI32 ? Inst::kIdPcmpgtd : Inst::kIdPcmpgtq;
        Vec msk = newV128("@msk");
        cc->emit(Inst::kIdMovaps, msk, src2);
        cc->emit(cmpInstId, msk, src1v);
        sseSelect(this, dst, src1v, src2, msk);
        return;
      }

      case OpcodeVVV::kMaxI64:
        // Native operation requires AVX512, which is not supported by the target.
        if (!hasSSE4_2()) {
          Vec msk = newV128("@msk");
          sseCmpGtI64(this, msk, src1v, src2);
          sseSelect(this, dst, src1v, src2, msk);
          return;
        }
        BL_FALLTHROUGH
      case OpcodeVVV::kMaxI8:
      case OpcodeVVV::kMaxI32: {
        // Native operation requires SSE4.1, which is not supported by the target.
        InstId cmpInstId = op == OpcodeVVV::kMaxI8  ? Inst::kIdPcmpgtb :
                           op == OpcodeVVV::kMaxI32 ? Inst::kIdPcmpgtd : Inst::kIdPcmpgtq;
        Vec msk = newV128("@msk");
        cc->emit(Inst::kIdMovaps, msk, src1v);
        cc->emit(cmpInstId, msk, src2);
        sseSelect(this, dst, src1v, src2, msk);
        return;
      }

      case OpcodeVVV::kMinU16: {
        // Native operation requires SSE4.1, which is not supported by the target.
        Vec tmp = newV128("@tmp");
        cc->emit(Inst::kIdMovaps, tmp, src1v);
        cc->emit(Inst::kIdPsubusw, tmp, src2);
        sseMov(this, dst, src1v);
        cc->emit(Inst::kIdPsubw, dst, tmp);
        return;
      }

      case OpcodeVVV::kMaxU16: {
        // Native operation requires SSE4.1, which is not supported by the target.
        sseMov(this, dst, src1v);
        cc->emit(Inst::kIdPsubusw, dst, src2);
        cc->emit(Inst::kIdPaddw, dst, src2);
        return;
      }

      case OpcodeVVV::kMinU32:
      case OpcodeVVV::kMaxU32: {
        // Native operation requires SSE4.1, which is not supported by the target.
        Operand flipMsk = simdConst(&ct.f32_sgn, Bcst::kNA, dst);
        Vec tmp1 = newSimilarReg(dst, "@tmp1");
        Vec tmp2 = newSimilarReg(dst, "@tmp2");

        if (op == OpcodeVVV::kMinU32) {
          sseMov(this, tmp1, src2);
          sseMov(this, tmp2, src1v);
        }
        else {
          sseMov(this, tmp1, src1v);
          sseMov(this, tmp2, src2);
        }

        cc->emit(Inst::kIdPxor, tmp1, flipMsk);
        cc->emit(Inst::kIdPxor, tmp2, flipMsk);
        cc->emit(Inst::kIdPcmpgtd, tmp1, tmp2);

        sseSelect(this, dst, src1v, src2, tmp1);
        return;
      }

      case OpcodeVVV::kMinU64: {
        // Native operation requires AVX512, which is not supported by the target.
        Vec msk = newSimilarReg(dst, "@tmp1");
        sseCmpGtU64(this, msk, src2, src1v);
        sseSelect(this, dst, src1v, src2, msk);
        return;
      }

      case OpcodeVVV::kMaxU64: {
        // Native operation requires AVX512, which is not supported by the target.
        Vec msk = newSimilarReg(dst, "@tmp1");
        sseCmpGtU64(this, msk, src1v, src2);
        sseSelect(this, dst, src1v, src2, msk);
        return;
      }

      case OpcodeVVV::kCmpEqU64: {
        // Native operation requires SSE4.1, which is not supported by the target.
        Vec tmp = newSimilarReg(dst, "@tmp");
        sseMov(this, dst, src1v);
        cc->emit(Inst::kIdPcmpeqd, dst, src2);
        cc->emit(Inst::kIdPshufd, tmp, dst, x86::shuffleImm(2, 3, 0, 1));
        cc->emit(Inst::kIdPand, dst, tmp);
        return;
      }

      case OpcodeVVV::kCmpGtI64: {
        // Native operation requires SSE4.2, which is not supported by the target.
        sseCmpGtI64(this, dst, src1v, src2);
        return;
      }

      case OpcodeVVV::kCmpGtU8:
      case OpcodeVVV::kCmpGtU16:
      case OpcodeVVV::kCmpGtU32: {
        CmpMinMaxInst inst = sse_cmp_min_max[size_t(op) - size_t(OpcodeVVV::kCmpGtI8)];

        if (hasSSE4_1() || op == OpcodeVVV::kCmpGtU8) {
          if (dst.id() == src1v.id()) {
            Vec tmp = newSimilarReg(dst, "@tmp");
            cc->emit(Inst::kIdMovaps, tmp, src1v);
            cc->emit(inst.pmin, tmp, src2);
            cc->emit(inst.peq, dst, tmp);
          }
          else if (isSameVec(dst, src2)) {
            cc->emit(inst.pmin, dst, src1v);
            cc->emit(inst.peq, dst, src1v);
          }
          else {
            cc->emit(Inst::kIdMovaps, dst, src1v);
            cc->emit(inst.pmin, dst, src2);
            cc->emit(inst.peq, dst, src1v);
          }

          sseBitNot(this, dst, dst);
          return;
        }

        Vec tmp = newSimilarReg(dst, "@tmp");
        sseMsbFlip(this, tmp, src2, ElementSize(opInfo.elementSize));
        sseMsbFlip(this, dst, src1v, ElementSize(opInfo.elementSize));
        cc->emit(inst.pgt, dst, tmp);
        return;
      }

      case OpcodeVVV::kCmpGtU64: {
        // Native operation requires AVX512, which is not supported by the target.
        sseCmpGtU64(this, dst, src1v, src2);
        return;
      }

      case OpcodeVVV::kCmpGeI8:
      case OpcodeVVV::kCmpGeU8:
      case OpcodeVVV::kCmpGeI16:
      case OpcodeVVV::kCmpGeU16:
      case OpcodeVVV::kCmpGeI32:
      case OpcodeVVV::kCmpGeU32:
        // Native operation requires AVX512, which is not supported by the target.
        if (hasSSE4_1() || op == OpcodeVVV::kCmpGeU8 || op == OpcodeVVV::kCmpGeI16) {
          CmpMinMaxInst inst = sse_cmp_min_max[size_t(op) - size_t(OpcodeVVV::kCmpGeI8)];

          if (dst.id() == src1v.id()) {
            Vec tmp = newSimilarReg(dst, "@tmp");
            cc->emit(Inst::kIdMovaps, tmp, src1v);
            cc->emit(inst.pmax, tmp, src2);
            cc->emit(inst.peq, dst, tmp);
          }
          else if (isSameVec(dst, src2)) {
            cc->emit(inst.pmax, dst, src1v);
            cc->emit(inst.peq, dst, src1v);
          }
          else {
            cc->emit(Inst::kIdMovaps, dst, src1v);
            cc->emit(inst.pmax, dst, src2);
            cc->emit(inst.peq, dst, src1v);
          }
          return;
        }

        if (op == OpcodeVVV::kCmpGeU16) {
          Vec tmp = newSimilarReg(dst, "@tmp");

          sseMov(this, tmp, src1v);
          cc->emit(Inst::kIdPsubusw, tmp, src2);
          cc->emit(Inst::kIdPaddw, tmp, src2);

          sseMov(this, dst, src1v);
          cc->emit(Inst::kIdPcmpeqw, dst, tmp);
          return;
        }
        BL_FALLTHROUGH
      case OpcodeVVV::kCmpGeI64:
      case OpcodeVVV::kCmpGeU64:
        // Native operation requires AVX512, which is not supported by the target.
        if (src2.isMem()) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          sseMov(this, tmp, src2);
          src2 = tmp;
        }

        switch (op) {
          case OpcodeVVV::kCmpGeI8: v_cmp_gt_i8(dst, src2, src1v); break;
          case OpcodeVVV::kCmpGeI32: v_cmp_gt_i32(dst, src2, src1v); break;
          case OpcodeVVV::kCmpGeU32: v_cmp_gt_u32(dst, src2, src1v); break;
          case OpcodeVVV::kCmpGeI64: v_cmp_gt_i64(dst, src2, src1v); break;
          case OpcodeVVV::kCmpGeU64: v_cmp_gt_u64(dst, src2, src1v); break;

          default:
            BL_NOT_REACHED();
        }

        sseBitNot(this, dst, dst);
        return;

      case OpcodeVVV::kCmpLtI8:
      case OpcodeVVV::kCmpLtI16:
      case OpcodeVVV::kCmpLtI32: {
        if (isSameVec(dst, src1v)) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          sseMov(this, tmp, src1v);
          src1v = tmp;
        }

        sseMov(this, dst, src2);
        cc->emit(instId, dst, src1v);
        return;
      }

      case OpcodeVVV::kCmpLtU8:
      case OpcodeVVV::kCmpLtU16:
      case OpcodeVVV::kCmpLtU32: {
        Vec tmp = newSimilarReg(dst, "@tmp");
        sseMov(this, tmp, src1v);
        sseMsbFlip(this, tmp, src1v, ElementSize(opInfo.elementSize));
        sseMsbFlip(this, dst, src2, ElementSize(opInfo.elementSize));
        cc->emit(instId, dst, tmp);
        return;
      }

      case OpcodeVVV::kCmpLtI64: {
        // Native operation requires AVX512, which is not supported by the target.
        sseCmpGtI64(this, dst, src2, src1v);
        return;
      }

      case OpcodeVVV::kCmpLtU64: {
        // Native operation requires AVX512, which is not supported by the target.
        sseCmpGtU64(this, dst, src2, src1v);
        return;
      }

      case OpcodeVVV::kCmpLeU8: {
        if (isSameVec(dst, src2)) {
          Vec tmp = newSimilarReg(dst, "@tmp");
          sseMov(this, tmp, src2);
          src2 = tmp;
        }

        sseMov(this, dst, src1v);
        cc->emit(Inst::kIdPsubusb, dst, src2);

        Vec zeros = simdVecConst(&ct.i128_0000000000000000, Bcst::k32, dst);
        cc->emit(Inst::kIdPcmpeqb, dst, zeros);
        return;
      }

      case OpcodeVVV::kCmpLeI8:
      case OpcodeVVV::kCmpLeI16:
      case OpcodeVVV::kCmpLeU16:
      case OpcodeVVV::kCmpLeI32:
      case OpcodeVVV::kCmpLeU32:
        if (hasSSE4_1() || op == OpcodeVVV::kCmpLeU8 || op == OpcodeVVV::kCmpLeI16) {
          CmpMinMaxInst inst = sse_cmp_min_max[size_t(op) - size_t(OpcodeVVV::kCmpLeI8)];

          if (dst.id() == src1v.id()) {
            Vec tmp = newSimilarReg(dst, "@tmp");
            cc->emit(Inst::kIdMovaps, tmp, src1v);
            cc->emit(inst.pmin, tmp, src2);
            cc->emit(inst.peq, dst, tmp);
          }
          else if (isSameVec(dst, src2)) {
            cc->emit(inst.pmin, dst, src1v);
            cc->emit(inst.peq, dst, src1v);
          }
          else {
            cc->emit(Inst::kIdMovaps, dst, src1v);
            cc->emit(inst.pmin, dst, src2);
            cc->emit(inst.peq, dst, src1v);
          }
          return;
        }
        BL_FALLTHROUGH
      case OpcodeVVV::kCmpLeI64:
      case OpcodeVVV::kCmpLeU64:
        switch (op) {
          case OpcodeVVV::kCmpLeI8: v_cmp_gt_i8(dst, src1v, src2); break;
          case OpcodeVVV::kCmpLeU16: v_cmp_gt_u16(dst, src1v, src2); break;
          case OpcodeVVV::kCmpLeI32: v_cmp_gt_i32(dst, src1v, src2); break;
          case OpcodeVVV::kCmpLeU32: v_cmp_gt_u32(dst, src1v, src2); break;
          case OpcodeVVV::kCmpLeI64: v_cmp_gt_i64(dst, src1v, src2); break;
          case OpcodeVVV::kCmpLeU64: v_cmp_gt_u64(dst, src1v, src2); break;

          default:
            BL_NOT_REACHED();
        }

        sseBitNot(this, dst, dst);
        return;

      case OpcodeVVV::kCmpLtF32S:
      case OpcodeVVV::kCmpLtF64S:
      case OpcodeVVV::kCmpLtF32:
      case OpcodeVVV::kCmpLtF64:
      case OpcodeVVV::kCmpLeF32S:
      case OpcodeVVV::kCmpLeF64S:
      case OpcodeVVV::kCmpLeF32:
      case OpcodeVVV::kCmpLeF64:
        if (isSameVec(dst, src2)) {
          uint8_t pred = sse_fcmp_imm_table[(size_t(op) - size_t(OpcodeVVV::kCmpEqF32S)) / 4u];

          // Unfortunately we have to do two moves, because there are no predicates that
          // we could use in case of reversed operands (AVX is much better in this regard).
          Vec tmp = newSimilarReg(dst, "@tmp");
          sseMov(this, tmp, src2);
          sseMov(this, dst, src1v);
          cc->emit(instId, dst, tmp, pred);
          return;
        }
        BL_FALLTHROUGH
      case OpcodeVVV::kCmpEqF32S:
      case OpcodeVVV::kCmpEqF64S:
      case OpcodeVVV::kCmpEqF32:
      case OpcodeVVV::kCmpEqF64:
      case OpcodeVVV::kCmpNeF32S:
      case OpcodeVVV::kCmpNeF64S:
      case OpcodeVVV::kCmpNeF32:
      case OpcodeVVV::kCmpNeF64:
      case OpcodeVVV::kCmpOrdF32S:
      case OpcodeVVV::kCmpOrdF64S:
      case OpcodeVVV::kCmpOrdF32:
      case OpcodeVVV::kCmpOrdF64:
      case OpcodeVVV::kCmpUnordF32S:
      case OpcodeVVV::kCmpUnordF64S:
      case OpcodeVVV::kCmpUnordF32:
      case OpcodeVVV::kCmpUnordF64: {
        uint8_t pred = sse_fcmp_imm_table[(size_t(op) - size_t(OpcodeVVV::kCmpEqF32S)) / 4u];
        sseMov(this, dst, src1v);
        cc->emit(instId, dst, src2, pred);
        return;
      }

      case OpcodeVVV::kCmpGtF32S:
      case OpcodeVVV::kCmpGtF64S:
      case OpcodeVVV::kCmpGtF32:
      case OpcodeVVV::kCmpGtF64:
      case OpcodeVVV::kCmpGeF32S:
      case OpcodeVVV::kCmpGeF64S:
      case OpcodeVVV::kCmpGeF32:
      case OpcodeVVV::kCmpGeF64: {
        // Since SSE compare doesn't provide these modes natively, we have to reverse the operands.
        uint8_t pred = sse_fcmp_imm_table[(size_t(op) - size_t(OpcodeVVV::kCmpEqF32S)) / 4u];

        if (dst.id() != src1v.id()) {
          sseMov(this, dst, src2);
          cc->emit(instId, dst, src1v, pred);
        }
        else {
          Vec tmp = newSimilarReg(dst, "@tmp");
          sseMov(this, tmp, src2);
          cc->emit(instId, tmp, src1v, pred);
          sseMov(this, dst, tmp);
        }
        return;
      }

      case OpcodeVVV::kHAddF64: {
        // Native operation requires SSE3, which is not supported by the target.
        if (isSameVec(src1v, src2)) {
          if (isSameVec(dst, src1v)) {
            Vec tmp = cc->newSimilarReg(dst, "@tmp");
            v_swap_f64(tmp, dst);
            cc->addpd(dst.as<Xmm>(), tmp.as<Xmm>());
          }
          else {
            v_swap_f64(dst, src1v);
            cc->addpd(dst.as<Xmm>(), src1v.as<Xmm>());
          }
        }
        else {
          // [B A]    [C A]
          // [D C] -> [D B]
          Vec tmp = newSimilarReg(dst, "@tmp");
          if (src2.isMem()) {
            Mem m(src2.as<Mem>());

            sseMov(this, dst, src1v);
            v_swap_f64(tmp, dst);
            cc->movhpd(dst.as<Xmm>(), m);

            m.addOffset(8);
            cc->movhpd(tmp.as<Xmm>(), m);
            cc->addpd(dst.as<Xmm>(), tmp.as<Xmm>());
          }
          else if (isSameVec(dst, src2)) {
            sseMov(this, tmp, src1v);
            cc->unpcklpd(tmp.as<Xmm>(), src2.as<Xmm>());
            cc->movhlps(dst.as<Xmm>(), src1v.as<Xmm>());
            cc->addpd(dst.as<Xmm>(), tmp.as<Xmm>());
          }
          else {
            sseMov(this, tmp, src1v);
            cc->unpckhpd(tmp.as<Xmm>(), src2.as<Xmm>());

            sseMov(this, dst, src1v);
            cc->unpcklpd(dst.as<Xmm>(), src2.as<Xmm>());

            cc->addpd(dst.as<Xmm>(), tmp.as<Xmm>());
          }
        }
        return;
      }

      case OpcodeVVV::kCombineLoHiU64:
      case OpcodeVVV::kCombineLoHiF64: {
        // Intrinsic - dst = {src1.u64[0], src2.64[1]} - combining low part of src1 and high part of src1.
        if (src2.isMem()) {
          Mem m = src2.as<Mem>().cloneAdjusted(8);
          cc->emit(Inst::kIdPshufd, dst, src1v, x86::shuffleImm(1, 0, 1, 0));
          cc->emit(Inst::kIdMovlpd, dst, m);
          return;
        }

        if (isSameVec(dst, src2)) {
          // dst = {src1.u64[0], dst.u64[1]}
          cc->emit(Inst::kIdShufpd, dst, src1v, x86::shuffleImm(0, 1));
          return;
        }
        else if (isSameVec(dst, src1v)) {
          // dst = {dst.u64[0], src2.u64[1]}
          if (hasSSSE3()) {
            cc->emit(Inst::kIdPalignr, dst, src2, 8);
            return;
          }
        }

        if (hasSSE3())
          cc->emit(Inst::kIdMovddup, dst, src1v);
        else
          cc->emit(Inst::kIdPshufd, dst, src1v, x86::shuffleImm(1, 0, 1, 0));

        cc->emit(Inst::kIdMovhlps, dst, src2);
        return;
      }

      case OpcodeVVV::kCombineHiLoU64:
      case OpcodeVVV::kCombineHiLoF64: {
        // Intrinsic - dst = {src1.u64[1], src2.64[0]} - combining high part of src1 and low part of src2.
        if (src2.isMem()) {
          sseMov(this, dst, src1v);
          cc->emit(Inst::kIdMovlpd, dst, src2);
        }
        else if (isSameVec(dst, src2)) {
          // dst = {src1.u64[1], dst.u64[0]}
          cc->emit(Inst::kIdShufpd, dst, src1v, 0x2);
        }
        else {
          // dst = {src1.u64[1], src2.u64[0]}
          sseMov(this, dst, src1v);
          cc->emit(Inst::kIdMovsd, dst, src2);
        }
        return;
      }

      case OpcodeVVV::kPacksI32_U16: {
        // Native operation requires SSE4.1, which is not supported by the target.

        // NOTE: This one is generally tricky and involves a lot of operations. There are hacks available to shorten the
        // sequence, but then it would not cover all the inputs, so this is essentially a code necessary to handle all of
        // them. The trick here is to perform unsigned saturation first (that's why we fill one reg with MSB bits of the
        // input and then use ANDN), and then to bias the input in a way to make the result use signed saturation. The
        // last step is to convert the biased value back.
        //
        // In general, if you hit this code-path (not having SSE4.1 and still needing exactly this instruction) I would
        // recommend using a different strategy in this case, completely avoiding this code path. Usually, inputs are not
        // arbitrary and knowing the range could help a lot to reduce the approach to use a native 'packssdw' instruction.
        Operand bias = simdConst(&ct.i_0000800000008000, Bcst::kNA, dst);
        Operand unbias = simdConst(&ct.i_8000800080008000, Bcst::kNA, dst);

        if (isSameVec(src1v, src2)) {
          Vec tmp = dst;
          if (isSameVec(dst, src1v))
            tmp = newSimilarReg(dst, "@tmp1");

          sseMov(this, tmp, src1v);

          cc->emit(Inst::kIdPsrad, tmp, 31);
          cc->emit(Inst::kIdPandn, tmp, src1v);
          cc->emit(Inst::kIdPsubd, tmp, bias);
          cc->emit(Inst::kIdPackssdw, tmp, tmp);
          cc->emit(Inst::kIdPaddw, tmp, unbias);

          sseMov(this, dst, tmp);
        }
        else {
          Vec tmp1 = newSimilarReg(dst, "@tmp1");
          Vec tmp2 = newSimilarReg(dst, "@tmp2");

          sseMov(this, tmp1, src1v);
          sseMov(this, tmp2, src2);

          cc->emit(Inst::kIdPsrad, tmp1, 31);
          cc->emit(Inst::kIdPsrad, tmp2, 31);
          cc->emit(Inst::kIdPandn, tmp1, src1v);
          cc->emit(Inst::kIdPandn, tmp2, src2);
          cc->emit(Inst::kIdPsubd, tmp1, bias);
          cc->emit(Inst::kIdPsubd, tmp2, bias);
          cc->emit(Inst::kIdPackssdw, tmp1, tmp2);
          cc->emit(Inst::kIdPaddw, tmp1, unbias);

          sseMov(this, dst, tmp1);
        }
        return;
      }

      case OpcodeVVV::kSwizzlev_U8: {
        // Native operation requires SSSE3, which is not supported by the target.
        //
        // NOTE: This is basically a very slow emulation as there is no way how to implement this operation with SSE2 SIMD.
        Mem m_data = tmpStack(StackId::kCustom, 64);
        Mem m_pred = m_data.cloneAdjusted(32);

        m_data.setSize(1);
        m_pred.setSize(1);

        cc->movaps(m_data, src1v.as<Xmm>());

        // The trick is to AND all indexes by 0x0F and then to do unsigned minimum so all indexes are in [0, 17) range,
        // where index 16 maps to zero.
        Vec tmp = newSimilarReg(dst, "@tmp");
        cc->vmovaps(tmp.as<Xmm>(), simdMemConst(&ct.i_0F0F0F0F0F0F0F0F, Bcst::kNA, tmp));
        cc->pand(tmp.as<Xmm>(), src2.as<Xmm>());
        cc->pminub(tmp.as<Xmm>(), simdMemConst(&ct.i_1010101010101010, Bcst::kNA, tmp));
        cc->movaps(m_pred, tmp.as<Xmm>());
        cc->mov(m_data.cloneAdjusted(16), 0);

        Gp acc = newGpPtr("@acc");
        Gp idx = newGpPtr("@idx");

        // Process 2 bytes at a time, then use PINSRW to merge them with the destination.
        for (uint32_t i = 0; i < 8; i++) {
          cc->movzx(acc.r32(), m_pred); m_pred.addOffset(1);
          cc->movzx(idx.r32(), m_pred); m_pred.addOffset(1);

          m_data.setIndex(acc);
          cc->movzx(acc, m_data);

          m_data.setIndex(idx);
          cc->mov(acc.r8Hi(), m_data);

          if (i == 0)
            cc->movd(dst.as<Xmm>(), acc.r32());
          else
            cc->pinsrw(dst.as<Xmm>(), acc.r32(), i);
        }

        return;
      }

      default:
        BL_NOT_REACHED();
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
  Vec src1v(src1_.as<Vec>().cloneAs(dst));
  Operand src2(src2_);
  OpcodeVInfo opInfo = opcodeInfo3VI[size_t(op)];

  if (hasAVX()) {
    // AVX Implementation
    // ------------------

    InstId instId = opInfo.avxInstId;

    if (hasAVXExt(AVXExt(opInfo.avxExt))) {
      BL_ASSERT(instId != Inst::kIdNone);

      cc->emit(instId, dst, src1v, src2, imm);
      return;
    }

    switch (op) {
      // Intrin - short-circuit if possible based on the predicate.
      case OpcodeVVVI::kAlignr_U128: {
        if (imm == 0) {
          avxMov(this, dst, src2);
          return;
        }

        if (isSameVec(src1v, src2)) {
          if (imm == 4 || imm == 8 || imm == 12) {
            uint32_t pred = imm ==  4 ? x86::shuffleImm(0, 3, 2, 1) :
                            imm ==  8 ? x86::shuffleImm(1, 0, 3, 2) :
                            imm == 12 ? x86::shuffleImm(2, 1, 0, 3) : 0;
            cc->vpshufd(dst, src1v, pred);
            return;
          }
        }

        cc->emit(Inst::kIdVpalignr, dst, src1v, src2, imm);
        return;
      }

      // Intrin - maps directly to the corresponding instruction, but imm must be converted.
      case OpcodeVVVI::kInterleaveShuffleU32x4:
      case OpcodeVVVI::kInterleaveShuffleF32x4: {
        if (isSameVec(src1v, src2)) {
          OpcodeVVI simplifiedOp = (op == OpcodeVVVI::kInterleaveShuffleU32x4) ? OpcodeVVI::kSwizzleU32x4 : OpcodeVVI::kSwizzleF32x4;
          emit_2vi(simplifiedOp, dst, src1v, imm);
        }
        else {
          uint32_t shufImm = shufImm4FromSwizzle(Swizzle4{imm});
          cc->emit(instId, dst, src1v, src2, shufImm);
        }
        return;
      }

      // Intrin - maps directly to the corresponding instruction, but imm must be converted.
      case OpcodeVVVI::kInterleaveShuffleU64x2:
      case OpcodeVVVI::kInterleaveShuffleF64x2: {
        if (isSameVec(src1v, src2)) {
          OpcodeVVI simplifiedOp = (op == OpcodeVVVI::kInterleaveShuffleU64x2) ? OpcodeVVI::kSwizzleU64x2 : OpcodeVVI::kSwizzleF64x2;
          emit_2vi(simplifiedOp, dst, src1v, imm);
        }
        else {
          uint32_t shufImm = shufImm2FromSwizzleWithWidth(Swizzle2{imm}, VecWidthUtils::vecWidthOf(dst));
          cc->emit(instId, dst, src1v, src2, shufImm);
        }
        return;
      }

      case OpcodeVVVI::kInsertV128_U32:
      case OpcodeVVVI::kInsertV128_F32:
      case OpcodeVVVI::kInsertV128_U64:
      case OpcodeVVVI::kInsertV128_F64: {
        src1v.setSignature(dst.signature());

        if (src2.isMem())
          src2.as<Mem>().setSize(16);
        else
          src2.setSignature(signatureOfXmmYmmZmm[0]);

        if (!hasAVX512()) {
          if (hasAVX2() && (op == OpcodeVVVI::kInsertV128_U32 || op == OpcodeVVVI::kInsertV128_U64))
            instId = Inst::kIdVinserti128;
          else
            instId = Inst::kIdVinsertf128;
        }

        cc->emit(instId, dst, src1v, src2, imm);
        return;
      }

      case OpcodeVVVI::kInsertV256_U32:
      case OpcodeVVVI::kInsertV256_F32:
      case OpcodeVVVI::kInsertV256_U64:
      case OpcodeVVVI::kInsertV256_F64: {
        BL_ASSERT(hasAVX512());
        src1v.setSignature(dst.signature());

        if (src2.isMem())
          src2.as<Mem>().setSize(32);
        else
          src2.setSignature(signatureOfXmmYmmZmm[1]);

        cc->emit(instId, dst, src1v, src2, imm);
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else {
    // SSE Implementation
    // ------------------

    InstId instId = opInfo.sseInstId;

    if (isSameVec(dst, src2) && opInfo.commutative) {
      BLInternal::swap(src1v, src2.as<Vec>());
    }

    // All operations are intrinsics in this case - no direct mapping to instructions without an additional logic.
    BL_ASSERT(!hasSSEExt(SSEExt(opInfo.sseExt)));

    switch (op) {
      // Intrin - short-circuit if possible based on the predicate.
      case OpcodeVVVI::kAlignr_U128: {
        if (imm == 0) {
          sseMov(this, dst, src2);
          return;
        }

        if (isSameVec(src1v, src2)) {
          if (imm == 4 || imm == 8 || imm == 12) {
            uint32_t pred = imm ==  4 ? x86::shuffleImm(0, 3, 2, 1) :
                            imm ==  8 ? x86::shuffleImm(1, 0, 3, 2) :
                            imm == 12 ? x86::shuffleImm(2, 1, 0, 3) : 0;
            cc->emit(Inst::kIdPshufd, dst, src1v, pred);
            return;
          }
        }

        if (hasSSSE3()) {
          if (isSameVec(dst, src2) && !isSameVec(dst, src1v)) {
            Vec tmp = newSimilarReg(dst, "@tmp");
            sseMov(this, tmp, src2);
            src2 = tmp;
          }

          sseMov(this, dst, src1v);
          cc->emit(Inst::kIdPalignr, dst, src2, imm);
          return;
        }

        Vec tmp = newSimilarReg(dst, "@tmp");
        uint32_t src1Shift = (16u - imm) & 15;
        uint32_t src2Shift = imm;

        if (isSameVec(dst, src1v)) {
          sseMov(this, tmp, src2);
          cc->emit(Inst::kIdPsrldq, tmp, src2Shift);
          cc->emit(Inst::kIdPslldq, dst, src1Shift);
        }
        else {
          sseMov(this, tmp, src1v);
          sseMov(this, dst, src2);
          cc->emit(Inst::kIdPslldq, tmp, src1Shift);
          cc->emit(Inst::kIdPsrldq, dst, src2Shift);
        }

        cc->emit(Inst::kIdPor, dst, tmp);
        return;
      }

      // Intrin - maps directly to the corresponding instruction, but imm must be converted.
      case OpcodeVVVI::kInterleaveShuffleU32x4:
      case OpcodeVVVI::kInterleaveShuffleU64x2:
      case OpcodeVVVI::kInterleaveShuffleF32x4:
      case OpcodeVVVI::kInterleaveShuffleF64x2: {
        uint32_t shufImm;
        ElementSize elementSize = ElementSize(opInfo.elementSize);

        if (elementSize == ElementSize::k32)
          shufImm = shufImm4FromSwizzle(Swizzle4{imm});
        else
          shufImm = shufImm2FromSwizzle(Swizzle2{imm});

        if (isSameVec(src1v, src2)) {
          OpcodeVVI vvi_op =  OpcodeVVI(uint32_t(OpcodeVVI::kSwizzleU32x4) + (uint32_t(op) - uint32_t(OpcodeVVVI::kInterleaveShuffleU32x4)));
          emit_2vi(vvi_op, dst, src1v, imm);
          return;
        }

        else if (isSameVec(dst, src1v)) {
          cc->emit(instId, dst, src2, shufImm);
        }
        else if (isSameVec(dst, src2)) {
          // The predicate has to be reversed as we want to swap low/high 64-bit lanes afterwards.
          if (elementSize == ElementSize::k32)
            shufImm = (shufImm >> 4) | ((shufImm & 0xF) << 4);
          else
            shufImm = (shufImm >> 1) | ((shufImm & 0x1) << 1);

          cc->emit(instId, dst, src1v, shufImm);
          cc->emit(Inst::kIdPshufd, dst, dst, x86::shuffleImm(1, 0, 3, 2));
        }
        else {
          sseMov(this, dst, src1v);
          cc->emit(instId, dst, src2, shufImm);
        }
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
        // These are not available in SSE mode (256-bit vectors require AVX)
        BL_NOT_REACHED();

      default:
        BL_NOT_REACHED();
    }
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

  if (hasAVX()) {
    // AVX Implementation
    // ------------------

    InstId instId = opInfo.avxInstId;

    if (isSameVec(dst, src2) && opInfo.commutative) {
      BLInternal::swap(src1, src2.as<Vec>());
    }

    if (hasAVXExt(AVXExt(opInfo.avxExt))) {
      BL_ASSERT(instId != Inst::kIdNone);

      cc->emit(instId, dst, src1, src2, src3);
      return;
    }

    switch (op) {
      case OpcodeVVVV::kBlendV_U8: {
        // Blend(a, b, cond) == (a & ~cond) | (b & cond)
        avxMakeVec(this, src3, dst, "msk");
        cc->emit(opInfo.avxInstId, dst, src1, src2, src3);
        return;
      }

      case OpcodeVVVV::kMAddU16:
      case OpcodeVVVV::kMAddU32: {
        static constexpr uint16_t add_inst_table[2] = {
          Inst::kIdVpaddw,
          Inst::kIdVpaddd
        };

        Vec tmp = dst;
        if (isSameVec(dst, src3)) {
          tmp = newSimilarReg(dst, "@tmp");
        }

        InstId addId = add_inst_table[size_t(op) - size_t(OpcodeVVVV::kMAddU16)];

        cc->emit(instId, tmp, src1, src2);
        cc->emit(addId, dst, tmp, src3);

        return;
      }

      case OpcodeVVVV::kMAddF32S:
      case OpcodeVVVV::kMAddF64S:
      case OpcodeVVVV::kMAddF32:
      case OpcodeVVVV::kMAddF64:
      case OpcodeVVVV::kMSubF32S:
      case OpcodeVVVV::kMSubF64S:
      case OpcodeVVVV::kMSubF32:
      case OpcodeVVVV::kMSubF64:
      case OpcodeVVVV::kNMAddF32S:
      case OpcodeVVVV::kNMAddF64S:
      case OpcodeVVVV::kNMAddF32:
      case OpcodeVVVV::kNMAddF64:
      case OpcodeVVVV::kNMSubF32S:
      case OpcodeVVVV::kNMSubF64S:
      case OpcodeVVVV::kNMSubF32:
      case OpcodeVVVV::kNMSubF64: {
        // 4 operand operation:
        //
        //   madd(dst, a, b, c) -> dst = a * b + c
        //   msub(dst, a, b, c) -> dst = a * b - c
        //   nmadd(dst, a, b, c) -> dst = -a * b + c
        //   nmsub(dst, a, b, c) -> dst = -a * b - c
        //
        // 3 operand operation (FMA):
        //
        //   vfmadd213  a, b, c -> a =  a * b + c
        //   vfmadd132  a, b, c -> a =  a * c + b
        //   vfmadd231  a, b, c -> a =  b * c + a
        //   vfnmadd213 a, b, c -> a = -a * b + c
        //   vfnmadd132 a, b, c -> a = -a * c + b
        //   vfnmadd231 a, b, c -> a = -b * c + a
        //   vfsubd213  a, b, c -> a =  a * b - c
        //   vfsubd132  a, b, c -> a =  a * c - b
        //   vfsubd231  a, b, c -> a =  b * c - a
        //   vfnsubd213 a, b, c -> a = -a * b - c
        //   vfnsubd132 a, b, c -> a = -a * c - b
        //   vfnsubd231 a, b, c -> a = -b * c - a
        size_t fmaId = size_t(op) - size_t(OpcodeVVVV::kMAddF32S);
        FloatMode fm = FloatMode(opInfo.floatMode);

        if (fm == FloatMode::kF32S || fm == FloatMode::kF64S) {
          dst.setSignature(signatureOfXmmYmmZmm[0]);
          src1.setSignature(signatureOfXmmYmmZmm[0]);

          if (src2.isVec())
            src2.setSignature(signatureOfXmmYmmZmm[0]);

          if (src3.isVec())
            src3.setSignature(signatureOfXmmYmmZmm[0]);
        }

        if (hasFMA()) {
          // There is a variation of instructions, which can be used, but each has only 3 operands. Since we
          // allow 4 operands (having a separate desgination) we have to map our 4 operand representation to
          // 3 operand representation as used by FMA.

          static constexpr uint16_t fma_ab_add_c[16] = {
            Inst::kIdVfmadd213ss , Inst::kIdVfmadd213sd , Inst::kIdVfmadd213ps , Inst::kIdVfmadd213pd ,
            Inst::kIdVfmsub213ss , Inst::kIdVfmsub213sd , Inst::kIdVfmsub213ps , Inst::kIdVfmsub213pd ,
            Inst::kIdVfnmadd213ss, Inst::kIdVfnmadd213sd, Inst::kIdVfnmadd213ps, Inst::kIdVfnmadd213pd,
            Inst::kIdVfnmsub213ss, Inst::kIdVfnmsub213sd, Inst::kIdVfnmsub213ps, Inst::kIdVfnmsub213pd
           };

          static constexpr uint16_t fma_ac_add_b[16] = {
            Inst::kIdVfmadd132ss , Inst::kIdVfmadd132sd , Inst::kIdVfmadd132ps , Inst::kIdVfmadd132pd ,
            Inst::kIdVfmsub132ss , Inst::kIdVfmsub132sd , Inst::kIdVfmsub132ps , Inst::kIdVfmsub132pd ,
            Inst::kIdVfnmadd132ss, Inst::kIdVfnmadd132sd, Inst::kIdVfnmadd132ps, Inst::kIdVfnmadd132pd,
            Inst::kIdVfnmsub132ss, Inst::kIdVfnmsub132sd, Inst::kIdVfnmsub132ps, Inst::kIdVfnmsub132pd
          };

          static constexpr uint16_t fma_bc_add_a[16] = {
            Inst::kIdVfmadd231ss , Inst::kIdVfmadd231sd , Inst::kIdVfmadd231ps , Inst::kIdVfmadd231pd ,
            Inst::kIdVfmsub231ss , Inst::kIdVfmsub231sd , Inst::kIdVfmsub231ps , Inst::kIdVfmsub231pd ,
            Inst::kIdVfnmadd231ss, Inst::kIdVfnmadd231sd, Inst::kIdVfnmadd231ps, Inst::kIdVfnmadd231pd,
            Inst::kIdVfnmsub231ss, Inst::kIdVfnmsub231sd, Inst::kIdVfnmsub231ps, Inst::kIdVfnmsub231pd
          };

          if (isSameVec(dst, src1)) {
            if (src2.isReg())
              cc->emit(fma_ab_add_c[fmaId], dst, src2, src3);
            else
              cc->emit(fma_ac_add_b[fmaId], dst, src3, src2);
          }
          else if (isSameVec(dst, src2)) {
            cc->emit(fma_ab_add_c[fmaId], dst, src1, src3);
          }
          else if (isSameVec(dst, src3)) {
            cc->emit(fma_bc_add_a[fmaId], dst, src1, src2);
          }
          else {
            avxMov(this, dst, src1);
            if (!src2.isReg())
              cc->emit(fma_ac_add_b[fmaId], dst, src3, src2);
            else if (!src3.isReg())
              cc->emit(fma_ab_add_c[fmaId], dst, src1, src3);
            else
              cc->emit(fma_ab_add_c[fmaId], dst, src2, src3);
          }
          return;
        }
        else {
          // MAdd/MSub - native FMA not available so we have to do MUL followed by either ADD or SUB.
          const FloatInst& fi = avx_float_inst[size_t(fm)];

          bool mulAdd = (opInfo.imm & 0x01u) == 0u;
          bool negMul = (opInfo.imm & 0x02u) != 0u;
          InstId fi_facc = mulAdd ? fi.fadd : fi.fsub;

          if (!negMul) {
            // MAdd or MSub Operation.
            if (isSameVec(dst, src3)) {
              Vec tmp = newSimilarReg(dst, "@tmp");
              cc->emit(fi.fmul, tmp, src1, src2);
              cc->emit(fi_facc, dst, tmp, src3);
            }
            else {
              cc->emit(fi.fmul, dst, src1, src2);
              cc->emit(fi_facc, dst, dst, src3);
            }
          }
          else {
            // NMAdd or NMSub Operation.
            Vec tmp = newSimilarReg(dst, "@tmp");
            avxFSignFlip(this, tmp, src1, fm);

            cc->emit(fi.fmul, tmp, tmp, src2);
            cc->emit(fi_facc, dst, tmp, src3);
          }
          return;
        }
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else {
    // SSE Implementation
    // ------------------

    switch (op) {
      case OpcodeVVVV::kBlendV_U8: {
        // Blend(a, b, cond) == (a & ~cond) | (b & cond)
        if (hasSSE4_1()) {
          if (isSameVec(dst, src1) || (!isSameVec(dst, src2) && !isSameVec(dst, src3))) {
            sseMakeVec(this, src3, "tmp");
            sseMov(this, dst, src1);
            cc->emit(opInfo.sseInstId, dst, src2, src3);
            return;
          }
        }

        // Blend(a, b, cond) == a ^ ((a ^ b) &  cond)
        //                   == b ^ ((a ^ b) & ~cond)
        if (isSameVec(dst, src1)) {
          Vec tmp = newV128("@tmp");
          v_xor_i32(tmp, dst, src2);
          v_and_i32(tmp, tmp, src3);
          v_xor_i32(dst, dst, tmp);
        }
        else if (isSameVec(dst, src3)) {
          Vec tmp = newV128("@tmp");
          v_xor_i32(tmp, src1, src2);
          v_andn_i32(dst, dst, tmp);
          v_xor_i32(dst, dst, src2);
        }
        else {
          v_xor_i32(dst, src2, src1);
          v_and_i32(dst, dst, src3);
          v_xor_i32(dst, dst, src1);
        }
        return;
      }

      case OpcodeVVVV::kMAddU16:
      case OpcodeVVVV::kMAddU32: {
        Vec tmp = dst;
        if (isSameVec(dst, src3)) {
          tmp = newSimilarReg(dst, "@tmp");
        }

        if (op == OpcodeVVVV::kMAddU16) {
          v_mul_u16(tmp, src1, src2);
          v_add_u16(dst, tmp, src3);
        }
        else {
          v_mul_u32(tmp, src1, src2);
          v_add_u32(dst, tmp, src3);
        }

        return;
      }

      case OpcodeVVVV::kMAddF32S:
      case OpcodeVVVV::kMAddF64S:
      case OpcodeVVVV::kMSubF32S:
      case OpcodeVVVV::kMSubF64S:
      case OpcodeVVVV::kMAddF32:
      case OpcodeVVVV::kMAddF64:
      case OpcodeVVVV::kMSubF32:
      case OpcodeVVVV::kMSubF64:
      case OpcodeVVVV::kNMAddF32S:
      case OpcodeVVVV::kNMAddF64S:
      case OpcodeVVVV::kNMSubF32S:
      case OpcodeVVVV::kNMSubF64S:
      case OpcodeVVVV::kNMAddF32:
      case OpcodeVVVV::kNMAddF64:
      case OpcodeVVVV::kNMSubF32:
      case OpcodeVVVV::kNMSubF64: {
        FloatMode fm = FloatMode(opInfo.floatMode);

        bool mulAdd = (opInfo.imm & 0x01u) == 0u;
        bool negMul = (opInfo.imm & 0x02u) != 0u;

        if (isSameVec(dst, src2)) {
          // Unfortunately, to follow the FMA behavior in scalar case, we have to copy.
          if (fm <= FloatMode::kF64S)
            src2 = sseCopy(this, src2.as<Vec>(), "@copy_src2");
          else
            BLInternal::swap(src1, src2.as<Vec>());
        }

        const FloatInst& fi = sse_float_inst[size_t(fm)];
        InstId fi_facc = mulAdd ? fi.fadd : fi.fsub;

        if (isSameVec(dst, src3)) {
          if (fm <= FloatMode::kF64S || !mulAdd) {
            // Copy if we couldn't avoid the extra move.
            src3 = sseCopy(this, src3.as<Vec>(), "@copy_src3");
          }
          else {
            Vec tmp = cc->newSimilarReg(dst, "@tmp");
            sseMov(this, tmp, src1);
            cc->emit(fi.fmul, tmp, src2);
            cc->emit(negMul ? fi.fsub : fi.fadd, dst, tmp);
            return;
          }
        }

        if (negMul)
          sseFSignFlip(this, dst, src1, fm);
        else
          sseMov(this, dst, src1);

        cc->emit(fi.fmul, dst, src2);
        cc->emit(fi_facc, dst, src3);
        return;
      }

      default:
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

// bl::Pipeline::PipeCompiler - Predicate Helpers
// ==============================================

#if defined(BL_JIT_ARCH_X86)
static KReg PipeCompile_makeMaskPredicate(PipeCompiler* pc, PixelPredicate& predicate, uint32_t lastN, const Gp& adjustedCount) noexcept {
  BL_ASSERT(lastN <= 64);
  BL_ASSERT(IntOps::isPowerOf2(lastN));

  KReg kPred;
  if (!pc->hasAVX512())
    return kPred;

  uint32_t materializedCount = predicate._materializedCount;
  for (uint32_t i = 0; i < materializedCount; i++) {
    const PixelPredicate::MaterializedMask& p = predicate._materializedMasks[i];
    if (p.lastN == lastN && p.elementSize == 0u) {
      // If the record was created it has to provide a mask register, not any other register type.
      BL_ASSERT(p.mask.isKReg());
      return p.mask.as<KReg>();
    }
  }

  if (materializedCount >= PixelPredicate::kMaterializedMaskCapacity)
    return kPred;

  AsmCompiler* cc = pc->cc;
  bool useBZHI = lastN <= 32 || pc->is64Bit();

  if (lastN <= 32)
    kPred = cc->newKd("@kPred");
  else
    kPred = cc->newKq("@kPred");

  PixelPredicate::MaterializedMask& p = predicate._materializedMasks[materializedCount];
  p.lastN = uint8_t(lastN);
  p.elementSize = 0;
  p.mask = kPred;

  Gp gpCount = predicate.count();

  if (adjustedCount.isValid()) {
    gpCount = adjustedCount;
  }
  else if (lastN < predicate.size()) {
    gpCount = pc->newGpPtr("@gpCount");
    pc->and_(gpCount.cloneAs(predicate.count()), predicate.count(), lastN - 1);
  }

  if (useBZHI) {
    Gp gpPred = pc->newGpPtr("@gpPred");

    if (lastN <= 32)
      gpPred = gpPred.r32();

    cc->mov(gpPred, -1);
    cc->bzhi(gpPred, gpPred, gpCount.cloneAs(gpPred));

    if (lastN <= 32)
      cc->kmovd(kPred, gpPred);
    else
      cc->kmovq(kPred, gpPred);
  }
  else {
    x86::Mem mem = pc->_getMemConst(commonTable.k_msk64_data);
    mem.setIndex(cc->gpz(gpCount.id()));
    mem.setShift(3);

    if (lastN <= 8)
      cc->kmovb(kPred, mem);
    else if (lastN <= 16)
      cc->kmovw(kPred, mem);
    else if (lastN <= 32)
      cc->kmovd(kPred, mem);
    else
      cc->kmovq(kPred, mem);
  }

  predicate._materializedCount++;
  return kPred;
}

KReg PipeCompiler::makeMaskPredicate(PixelPredicate& predicate, uint32_t lastN) noexcept {
  Gp noAdjustedCount;
  return PipeCompile_makeMaskPredicate(this, predicate, lastN, noAdjustedCount);
}

KReg PipeCompiler::makeMaskPredicate(PixelPredicate& predicate, uint32_t lastN, const Gp& adjustedCount) noexcept {
  return PipeCompile_makeMaskPredicate(this, predicate, lastN, adjustedCount);
}

Vec PipeCompiler::makeVecPredicate32(PixelPredicate& predicate, uint32_t lastN) noexcept {
  Gp noAdjustedCount;
  return makeVecPredicate32(predicate, lastN, noAdjustedCount);
}

Vec PipeCompiler::makeVecPredicate32(PixelPredicate& predicate, uint32_t lastN, const Gp& adjustedCount) noexcept {
  BL_ASSERT(lastN <= 8);
  BL_ASSERT(IntOps::isPowerOf2(lastN));

  Vec vPred;
  if (!hasAVX())
    return vPred;

  uint32_t materializedCount = predicate._materializedCount;
  for (uint32_t i = 0; i < materializedCount; i++) {
    const PixelPredicate::MaterializedMask& p = predicate._materializedMasks[i];
    if (p.lastN == lastN && p.elementSize == 4u) {
      // If the record was created it has to provide a mask register, not any other register type.
      BL_ASSERT(p.mask.isVec());
      return p.mask.as<Vec>();
    }
  }

  if (materializedCount >= PixelPredicate::kMaterializedMaskCapacity)
    return vPred;

  if (lastN <= 4)
    vPred = newV128("@vPred128");
  else if (lastN <= 8)
    vPred = newV256("@vPred256");
  else
    BL_NOT_REACHED();

  PixelPredicate::MaterializedMask& p = predicate._materializedMasks[materializedCount];
  p.lastN = uint8_t(lastN);
  p.elementSize = uint8_t(4);
  p.mask = vPred;

  Gp gpCount = predicate.count();

  if (adjustedCount.isValid()) {
    gpCount = adjustedCount;
  }
  else if (lastN < predicate.size()) {
    gpCount = newGpPtr("@gpCount");
    and_(gpCount.cloneAs(predicate.count()), predicate.count(), lastN - 1);
  }

  x86::Mem mem = _getMemConst(commonTable.loadstore16_lo8_msk8());
  mem.setIndex(cc->gpz(gpCount.id()));
  mem.setShift(3);
  cc->vpmovsxbd(vPred, mem);

  predicate._materializedCount++;
  return vPred;
}
#endif // BL_JIT_ARCH_X86

} // {JIT}
} // {Pipeline}
} // {bl}

#endif
