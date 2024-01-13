// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if defined(BL_JIT_ARCH_X86)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchgradientpart_p.h"
#include "../../pipeline/jit/fetchpixelptrpart_p.h"
#include "../../pipeline/jit/fetchsolidpart_p.h"
#include "../../pipeline/jit/fetchpatternpart_p.h"
#include "../../pipeline/jit/fillpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../support/intops_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

// bl::Pipeline::PipeCompiler - Constants
// ======================================

static constexpr OperandSignature signatureOfXmmYmmZmm[] = {
  OperandSignature{x86::Xmm::kSignature},
  OperandSignature{x86::Ymm::kSignature},
  OperandSignature{x86::Zmm::kSignature}
};

// bl::Pipeline::PipeCompiler - Construction & Destruction
// =======================================================

PipeCompiler::PipeCompiler(AsmCompiler* cc, const CpuFeatures& features, PipeOptFlags optFlags) noexcept
  : cc(cc),
    ct(commonTable),
    _features(features),
    _optFlags(optFlags),
    _commonTableOff(512 + 128) {}

PipeCompiler::~PipeCompiler() noexcept {}

// bl::Pipeline::PipeCompiler - CPU Features and Optimization Options
// ==================================================================

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

// bl::Pipeline::PipeCompiler - BeginFunction & EndFunction
// ========================================================

void PipeCompiler::beginFunction() noexcept {
  // Function prototype and arguments.
  _funcNode = cc->addFunc(asmjit::FuncSignature::build<void, ContextData*, const void*, const void*>(asmjit::CallConvId::kCDecl));
  _funcInit = cc->cursor();
  _funcEnd = _funcNode->endNode()->prev();

  if (hasAVX()) {
    _funcNode->frame().setAvxEnabled();
    _funcNode->frame().setAvxCleanup();
  }

  if (hasAVX512()) {
    _funcNode->frame().setAvx512Enabled();
  }

  _ctxData = newGpPtr("ctxData");
  _fillData = newGpPtr("fillData");
  _fetchData = newGpPtr("fetchData");

  _funcNode->setArg(0, _ctxData);
  _funcNode->setArg(1, _fillData);
  _funcNode->setArg(2, _fetchData);
}

void PipeCompiler::endFunction() noexcept {
  // Finalize the pipeline function.
  cc->endFunc();
}

// bl::Pipeline::PipeCompiler - Parts
// ==================================

FillPart* PipeCompiler::newFillPart(FillType fillType, FetchPart* dstPart, CompOpPart* compOpPart) noexcept {
  if (fillType == FillType::kBoxA)
    return newPartT<FillBoxAPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  if (fillType == FillType::kMask)
    return newPartT<FillMaskPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  if (fillType == FillType::kAnalytic)
    return newPartT<FillAnalyticPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  return nullptr;
}

FetchPart* PipeCompiler::newFetchPart(FetchType fetchType, FormatExt format) noexcept {
  if (fetchType == FetchType::kSolid)
    return newPartT<FetchSolidPart>(format);

  if (fetchType >= FetchType::kGradientLinearFirst && fetchType <= FetchType::kGradientLinearLast)
    return newPartT<FetchLinearGradientPart>(fetchType, format);

  if (fetchType >= FetchType::kGradientRadialFirst && fetchType <= FetchType::kGradientRadialLast)
    return newPartT<FetchRadialGradientPart>(fetchType, format);

  if (fetchType >= FetchType::kGradientConicFirst && fetchType <= FetchType::kGradientConicLast)
    return newPartT<FetchConicGradientPart>(fetchType, format);

  if (fetchType >= FetchType::kPatternSimpleFirst && fetchType <= FetchType::kPatternSimpleLast)
    return newPartT<FetchSimplePatternPart>(fetchType, format);

  if (fetchType >= FetchType::kPatternAffineFirst && fetchType <= FetchType::kPatternAffineLast)
    return newPartT<FetchAffinePatternPart>(fetchType, format);

  if (fetchType == FetchType::kPixelPtr)
    return newPartT<FetchPixelPtrPart>(fetchType, format);

  return nullptr;
}

CompOpPart* PipeCompiler::newCompOpPart(CompOpExt compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept {
  return newPartT<CompOpPart>(compOp, dstPart, srcPart);
}

// bl::Pipeline::PipeCompiler - Init
// =================================

static RegType simdRegTypeFromWidth(SimdWidth simdWidth) noexcept {
  if (simdWidth == SimdWidth::k512)
    return RegType::kX86_Zmm;
  else if (simdWidth == SimdWidth::k256)
    return RegType::kX86_Ymm;
  else
    return RegType::kX86_Xmm;
}

void PipeCompiler::_initSimdWidth(PipePart* root) noexcept {
  // NOTE: It depends on parts which SIMD width will be used by the pipeline. We set the maximum SIMD width available
  // for this host CPU, but if any part doesn't support such width it will end up lower. For example it's possible
  // that the pipeline would use only 128-bit SIMD width even when the CPU has support for AVX-512.

  SimdWidth simdWidth = SimdWidth::k128;

  // Use 256-bit SIMD width if AVX2 is available.
  if (hasAVX2())
    simdWidth = SimdWidth::k256;

  // Use 512-bit SIMD width if AVX512 is available and the target is 64-bit. We never use 512-bit SIMD in 32-bit mode
  // as it doesn't have enough registers to hold 512-bit constants and we don't store 512-bit constants in memory
  // (they must be broadcasted to full width).
  if (hasAVX512() && is64Bit())
    simdWidth = SimdWidth::k512;

  root->forEachPart([&](PipePart* part) {
    simdWidth = SimdWidth(blMin<uint32_t>(uint32_t(simdWidth), uint32_t(part->maxSimdWidthSupported())));
  });

  _simdWidth = simdWidth;
  _simdRegType = simdRegTypeFromWidth(simdWidth);
  _simdTypeId = asmjit::ArchTraits::byArch(cc->arch()).regTypeToTypeId(_simdRegType);
  _simdMultiplier = 1u << (uint32_t(_simdRegType) - uint32_t(RegType::kX86_Xmm));
}

void PipeCompiler::initPipeline(PipePart* root) noexcept {
  // Initialize SIMD width and everything that relies on it.
  _initSimdWidth(root);

  // Prepare all parts (the flag marks all visited parts).
  root->forEachPartAndMark(PipePartFlags::kPrepareDone, [&](PipePart* part) {
    part->preparePart();
  });
}

// bl::Pipeline::PipeCompiler - Constants
// ======================================

void PipeCompiler::_initCommonTablePtr() noexcept {
  const void* global = &commonTable;

  if (!_commonTablePtr.isValid()) {
    asmjit::BaseNode* prevNode = cc->setCursor(_funcInit);
    _commonTablePtr = newGpPtr("commonTablePtr");

    cc->mov(_commonTablePtr, (int64_t)global + _commonTableOff);
    _funcInit = cc->setCursor(prevNode);
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

Operand PipeCompiler::simdConst(const void* c, Bcst bcstWidth, SimdWidth constWidth) noexcept {
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
  SimdWidth constWidth = SimdWidth(uint32_t(similarTo.type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdConst(c, bcstWidth, constWidth);
}

Operand PipeCompiler::simdConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept {
  BL_ASSERT(!similarTo.empty());

  SimdWidth constWidth = SimdWidth(uint32_t(similarTo[0].type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdConst(c, bcstWidth, constWidth);
}

Vec PipeCompiler::simdVecConst(const void* c, SimdWidth constWidth) noexcept {
  size_t constCount = _vecConsts.size();

  for (size_t i = 0; i < constCount; i++)
    if (_vecConsts[i].ptr == c)
      return Vec(signatureOfXmmYmmZmm[size_t(constWidth)], _vecConsts[i].vRegId);

  return Vec(signatureOfXmmYmmZmm[size_t(constWidth)], _newVecConst(c, false).id());
}

Vec PipeCompiler::simdVecConst(const void* c, const Vec& similarTo) noexcept {
  SimdWidth constWidth = SimdWidth(uint32_t(similarTo.type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdVecConst(c, constWidth);
}

Vec PipeCompiler::simdVecConst(const void* c, const VecArray& similarTo) noexcept {
  BL_ASSERT(!similarTo.empty());

  SimdWidth constWidth = SimdWidth(uint32_t(similarTo[0].type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdVecConst(c, constWidth);
}

x86::Mem PipeCompiler::simdMemConst(const void* c, Bcst bcstWidth, SimdWidth constWidth) noexcept {
  x86::Mem m = _getMemConst(c);
  if (constWidth != SimdWidth::k512)
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
  SimdWidth constWidth = SimdWidth(uint32_t(similarTo.type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdMemConst(c, bcstWidth, constWidth);
}

x86::Mem PipeCompiler::simdMemConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept {
  BL_ASSERT(!similarTo.empty());

  SimdWidth constWidth = SimdWidth(uint32_t(similarTo[0].type()) - uint32_t(asmjit::RegType::kX86_Xmm));
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

  if (c == commonTable.pshufb_dither_rgba64_lo.data)
    specialConstName = "pshufb_dither_rgba64_lo";
  else if (c == commonTable.pshufb_dither_rgba64_hi.data)
    specialConstName = "pshufb_dither_rgba64_hi";

  if (specialConstName) {
    vReg = newVec(simdWidth(), specialConstName);
  }
  else {
    uint64_t u0 = static_cast<const uint64_t*>(c)[0];
    uint64_t u1 = static_cast<const uint64_t*>(c)[1];

    if (u0 != u1)
      vReg = newVec(simdWidth(), "c_0x%016llX%016llX", (unsigned long long)u1, (unsigned long long)u0);
    else if ((u0 >> 32) != (u0 & 0xFFFFFFFFu))
      vReg = newVec(simdWidth(), "c_0x%016llX", (unsigned long long)u0);
    else if (((u0 >> 16) & 0xFFFFu) != (u0 & 0xFFFFu))
      vReg = newVec(simdWidth(), "c_0x%08X", (unsigned)(u0 & 0xFFFFFFFFu));
    else
      vReg = newVec(simdWidth(), "c_0x%04X", (unsigned)(u0 & 0xFFFFu));
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
    x86::Mem m = _getMemConst(c);

    ScopedInjector inject(cc, &_funcInit);
    if (hasAVX512() && !vReg.isXmm() && !isUniqueConst)
      cc->vbroadcasti32x4(vReg, m);
    else if (hasAVX2() && vReg.isYmm() && !isUniqueConst)
      cc->vbroadcasti128(vReg, m);
    else if (hasAVX512())
      cc->vmovdqa32(vReg, m); // EVEX prefix has a compressed displacement, which is smaller.
    else
      v_loada_ivec(vReg, m);
  }
  return vReg;
}

// bl::Pipeline::PipeCompiler - Stack
// ==================================

x86::Mem PipeCompiler::tmpStack(uint32_t size) noexcept {
  BL_ASSERT(IntOps::isPowerOf2(size));
  BL_ASSERT(size <= 32);

  // Only used by asserts.
  blUnused(size);

  if (!_tmpStack.baseId())
    _tmpStack = cc->newStack(32, 16, "tmpStack");
  return _tmpStack;
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

// bl::Pipeline::PipeCompiler - Emit (General Purpose)
// ===================================================

static inline bool isSameReg(const Operand_& a, const Operand_& b) noexcept {
  return a.id() == b.id() && a.id() && b.id();
}

void PipeCompiler::i_emit_2(InstId instId, const Operand_& op1, int imm) noexcept {
  cc->emit(instId, op1, imm);
}

void PipeCompiler::i_emit_2(InstId instId, const Operand_& op1, const Operand_& op2) noexcept {
  cc->emit(instId, op1, op2);
}

void PipeCompiler::i_emit_3(InstId instId, const Operand_& op1, const Operand_& op2, int imm) noexcept {
  cc->emit(instId, op1, op2, imm);
}

void PipeCompiler::emit_mov(const Gp& dst, const Operand_& src) noexcept {
  if (src.isImm() && src.as<Imm>().value() == 0) {
    Gp r(dst);
    if (r.isGpq())
      r = r.r32();
    cc->xor_(r, r);
  }
  else {
    cc->emit(x86::Inst::kIdMov, dst, src);
  }
}

void PipeCompiler::emit_load(const Gp& dst, const Mem& src, uint32_t size) noexcept {
  Gp r(dst);
  Mem m(src);

  InstId instId = x86::Inst::kIdMov;
  if (size <= 4) {
    BL_ASSERT(size == 1 || size == 2 || size == 4);
    r.setTypeAndId(RegType::kGp32, r.id());
    m.setSize(size);
    if (size < 4)
      instId = x86::Inst::kIdMovzx;
  }
  else {
    BL_ASSERT(r.size() == 8);
    BL_ASSERT(size == 8);
    m.setSize(8);
  }

  cc->emit(instId, r, m);
}

void PipeCompiler::emit_store(const Mem& dst, const Gp& src, uint32_t size) noexcept {
  Mem m(dst);
  Gp r;

  m.setSize(size);
  switch (size) {
    case 1: r = src.r8(); break;
    case 2: r = src.r16(); break;
    case 4: r = src.r32(); break;
    case 8: r = src.r64(); break;
    default: BL_NOT_REACHED();
  }

  cc->mov(m, r);
}

static constexpr InstId conditionToInstId[size_t(Condition::Op::kMaxValue) + 1] = {
  x86::Inst::kIdAnd,  // kAssignAnd
  x86::Inst::kIdOr,   // kAssignOr
  x86::Inst::kIdXor,  // kAssignXor
  x86::Inst::kIdAdd,  // kAssignAdd
  x86::Inst::kIdSub,  // kAssignSub
  x86::Inst::kIdShr,  // kAssignSHR
  x86::Inst::kIdTest, // kTest
  x86::Inst::kIdBt,   // kBitTest
  x86::Inst::kIdCmp   // kCompare
};

class ConditionApplier : public Condition {
public:
  BL_INLINE ConditionApplier(const Condition& condition) noexcept : Condition(condition) {
    // The first operand must always be a register.
    BL_ASSERT(a.isReg() && a.as<Reg>().isGp());
  }

  BL_NOINLINE void optimize(PipeCompiler* pc) noexcept {
    switch (op) {
      case Op::kCompare:
        if (b.isImm() && b.as<Imm>().value() == 0 && (cond == CondCode::kEqual || cond == CondCode::kNotEqual)) {
          op = Op::kTest;
          b = a;
          reverse();
        }
        break;

      case Op::kBitTest: {
        if (b.isImm()) {
          uint64_t bitIndex = b.as<Imm>().valueAs<uint64_t>();

          // NOTE: AMD has no performance difference between 'test' and 'bt' instructions, however, Intel can execute less
          // 'bt' instructions per cycle than 'test's, so we prefer 'test' if bitIndex is low. Additionally, we only use
          // test on 64-bit hardware as it's guaranteed that any register index is encodable. On 32-bit hardware only the
          // first 4 registers can be used, which could mean that the register would have to be moved just to be tested,
          // which is something we would like to avoid.
          if (pc->is64Bit() && bitIndex < 8) {
            op = Condition::Op::kTest;
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
    InstId instId = conditionToInstId[size_t(op)];

    if (instId == x86::Inst::kIdTest && cc->is64Bit()) {
      if (b.isImm() && b.as<Imm>().value() <= 255u) {
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

    if (instId == x86::Inst::kIdShr && b.isReg()) {
      cc->emit(instId, a, b.as<Gp>().r8());
      return;
    }


    cc->emit(instId, a, b);
  }
};

void PipeCompiler::emit_cmov(const Gp& dst, const Operand_& sel, const Condition& condition) noexcept {
  ConditionApplier ca(condition);
  ca.optimize(this);
  ca.emit(this);
  cc->emit(x86::Inst::cmovccFromCond(ca.cond), dst, sel);
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
    std::swap(sel1, sel2);
  }

  bool dstIsSel = sel1.isReg() && dst.id() == sel1.id();
  if (sel1 == sel2) {
    if (!dstIsSel)
      cc->emit(x86::Inst::kIdMov, dst, sel1);
    return;
  }

  if (sel1.isImm() && sel1.as<Imm>().value() == 0 && !dstIsA && !dstIsB && !dstIsSel) {
    cc->xor_(dst, dst);
    ca.emit(this);
  }
  else {
    ca.emit(this);
    if (!dstIsSel)
      cc->emit(x86::Inst::kIdMov, sel1);
  }

  if (sel2.isImm()) {
    int64_t value = sel2.as<Imm>().value();
    Mem sel2Mem = cc->newConst(asmjit::ConstPoolScope::kLocal, &value, dst.size());
    sel2 = sel2Mem;
  }

  cc->emit(x86::Inst::cmovccFromCond(x86::negateCond(ca.cond)), dst, sel2);
}

void PipeCompiler::emit_arith2(Arith2Op op, const Gp& dst, const Operand_& src_) noexcept {
  // We need a pointer as we may need to convert the source operand to something else.
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
      case Arith2Op::kCLZ: {
        if (hasLZCNT()) {
          cc->emit(x86::Inst::kIdLzcnt, dst, src);
        }
        else {
          uint32_t msk = (1u << dst.size() * 8u) - 1u;
          cc->emit(x86::Inst::kIdBsr, dst, src);
          cc->xor_(dst, msk);
        }
        return;
      }

      case Arith2Op::kCTZ: {
        cc->emit(hasBMI() ? x86::Inst::kIdTzcnt : x86::Inst::kIdBsf, dst, src);
        return;
      }

      case Arith2Op::kReflect: {
        int nBits = int(dst.size()) * 8 - 1;

        if (src.isReg() && dst.id() == src.as<Reg>().id()) {
          BL_ASSERT(dst.size() == src.as<Reg>().size());
          Gp copy = newSimilarReg(dst, "@copy");

          cc->mov(copy, dst);
          cc->sar(copy, nBits);
          cc->xor_(dst, copy);
        }
        else {
          cc->emit(x86::Inst::kIdMov, dst, src);
          cc->sar(dst, nBits);
          cc->emit(x86::Inst::kIdXor, dst, src);
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
      case Arith2Op::kAbs: {
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

      case Arith2Op::kNeg:
      case Arith2Op::kNot: {
        if (!dstIsSrc)
          cc->mov(dst, srcGp);
        cc->emit(op == Arith2Op::kNeg ? x86::Inst::kIdNeg : x86::Inst::kIdNot, dst);
        return;
      }

      default:
        break;
    }
  }

  // Everything should be handled, so this should never be reached!
  BL_NOT_REACHED();
}

static constexpr uint64_t kArith3OpCommutativeMask =
  (uint64_t(1) << unsigned(PipeCompiler::Arith3Op::kAnd )) |
  (uint64_t(1) << unsigned(PipeCompiler::Arith3Op::kOr  )) |
  (uint64_t(1) << unsigned(PipeCompiler::Arith3Op::kXor )) |
  (uint64_t(1) << unsigned(PipeCompiler::Arith3Op::kAdd )) |
  (uint64_t(1) << unsigned(PipeCompiler::Arith3Op::kMul )) |
  (uint64_t(1) << unsigned(PipeCompiler::Arith3Op::kSMin)) |
  (uint64_t(1) << unsigned(PipeCompiler::Arith3Op::kSMax)) |
  (uint64_t(1) << unsigned(PipeCompiler::Arith3Op::kUMin)) |
  (uint64_t(1) << unsigned(PipeCompiler::Arith3Op::kUMax)) ;

static BL_INLINE_NODEBUG bool isArithOpCommutative(PipeCompiler::Arith3Op op) noexcept {
  return (kArith3OpCommutativeMask & (uint64_t(1) << unsigned(op))) != 0;
}

struct Arith3OpMinMaxCMovInst { InstId a, b; };

void PipeCompiler::emit_arith3(Arith3Op op, const Gp& dst, const Operand_& src1_, const Operand_& src2_) noexcept {
  Operand src1(src1_);
  Operand src2(src2_);

  static constexpr Arith3OpMinMaxCMovInst arithMinMaxCMovInstTable[4] = {
    { x86::Inst::kIdCmovl, x86::Inst::kIdCmovg }, // MinI
    { x86::Inst::kIdCmovg, x86::Inst::kIdCmovl }, // MaxI
    { x86::Inst::kIdCmovb, x86::Inst::kIdCmova }, // MinU
    { x86::Inst::kIdCmova, x86::Inst::kIdCmovb }  // MaxU
  };

  static constexpr InstId legacyShiftInstTable[5] = {
    x86::Inst::kIdShl,  // SHL
    x86::Inst::kIdShr,  // SHR
    x86::Inst::kIdSar,  // SAR
    x86::Inst::kIdRol,  // ROL
    x86::Inst::kIdRor   // ROR
  };

  static constexpr InstId legacyLogicalInstTable[3] = {
    x86::Inst::kIdAnd,  // AND
    x86::Inst::kIdOr,   // OR
    x86::Inst::kIdXor   // XOR
  };

  static constexpr InstId bmi2ShiftInstTable[5] = {
    x86::Inst::kIdShlx, // SHL
    x86::Inst::kIdShrx, // SHR
    x86::Inst::kIdSarx, // SAR
    x86::Inst::kIdNone, // ROL (doesn't exist).
    x86::Inst::kIdRorx  // ROR
  };

  // ArithOp Reg, Mem, Imm
  // ---------------------

  if (src1.isMem() && src2.isImm()) {
    const Mem& a = src1.as<Mem>();
    const Imm& b = src2.as<Imm>();

    switch (op) {
      case Arith3Op::kMul:
        cc->imul(dst, a, b);
        return;

      default:
        break;
    }

    cc->mov(dst, a);
    src1 = dst;
  }

  if (!src1.isReg() && isArithOpCommutative(op)) {
    std::swap(src1, src2);
  }

  // ArithOp Reg, Reg, Imm
  // ---------------------

  if (src1.isReg() && src2.isImm()) {
    const Gp& a = src1.as<Gp>();
    const Imm& b = src2.as<Imm>();

    bool dstIsA = dst.id() == a.id();
    BL_ASSERT(dst.size() == a.size());

    switch (op) {
      case Arith3Op::kAnd:
      case Arith3Op::kOr:
      case Arith3Op::kXor: {
        InstId instId = legacyLogicalInstTable[size_t(op) - size_t(Arith3Op::kAnd)];
        if (!dstIsA)
          cc->mov(dst, a);
        cc->emit(instId, dst, b);
        return;
      }

      case Arith3Op::kAndN: {
        if (!dstIsA)
          cc->mov(dst, a);
        cc->not_(dst);
        cc->and_(dst, b);
        return;
      }

      case Arith3Op::kAdd: {
        if (!dstIsA && b.isInt32()) {
          lea(dst, x86::ptr(a, b.valueAs<int32_t>()));
        }
        else {
          if (!dstIsA)
            cc->mov(dst, a);
          cc->add(dst, b);
        }
        return;
      }

      case Arith3Op::kSub: {
        if (!dstIsA)
          lea(dst, x86::ptr(a, b.valueAs<int32_t>()));
        else
          cc->sub(dst, b);
        return;
      }

      case Arith3Op::kMul: {
        switch (b.value()) {
          case 0:
            cc->xor_(dst, dst);
            return;

          case 1:
            if (!dstIsA)
              cc->mov(dst, a);
            return;

          case 2:
            if (dstIsA)
              cc->shl(dst, 1);
            else
              lea(dst, x86::ptr(a, a));
            return;

          case 3:
            lea(dst, x86::ptr(a, a, 1));
            return;

          case 4:
          case 8: {
            int shift = 2 + (b.value() == 8);
            if (dstIsA)
              cc->shl(dst, shift);
            else
              break;
            return;
          }

          default:
            break;
        }

        cc->imul(dst, a, b);
        return;
      }

      case Arith3Op::kSMin:
      case Arith3Op::kSMax:
      case Arith3Op::kUMin:
      case Arith3Op::kUMax: {
        const Arith3OpMinMaxCMovInst& cmovInst = arithMinMaxCMovInstTable[size_t(op) - size_t(Arith3Op::kSMin)];

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

      case Arith3Op::kSHL:
        // Optimize `dst = dst << 1` to `dst = dst + dst` as it has a higher throughput.
        if (b.value() == 1 && dstIsA) {
          cc->add(dst, dst);
          return;
        }

        BL_FALLTHROUGH

      case Arith3Op::kSHR:
      case Arith3Op::kSAR: {
        InstId legacyInst = legacyShiftInstTable[size_t(op) - size_t(Arith3Op::kSHL)];

        if (!dstIsA)
          cc->mov(dst, a);
        cc->emit(legacyInst, dst, b);
        return;
      }

      case Arith3Op::kROL: {
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

      case Arith3Op::kROR: {
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
      case Arith3Op::kAnd:
      case Arith3Op::kOr:
      case Arith3Op::kXor:
      case Arith3Op::kAdd:
      case Arith3Op::kMul:
      case Arith3Op::kSMin:
      case Arith3Op::kSMax:
      case Arith3Op::kUMin:
      case Arith3Op::kUMax:
        // These are commutative, so this should never happen as these should have been corrected to `Reg, Reg, Mem`.
        BL_NOT_REACHED();

      case Arith3Op::kSub: {
        BL_ASSERT(dst.size() == b.size());

        if (dstIsB) {
          cc->neg(dst);
          cc->add(dst, a);
          return;
        }

        // Bail to `Reg, Reg, Reg` form.
        break;
      }

      case Arith3Op::kSHL:
      case Arith3Op::kSHR:
      case Arith3Op::kSAR: {
        // Prefer BMI2 variants: SHLX, SHRX, SARX, and RORX.
        if (hasBMI2()) {
          InstId bmi2Inst = bmi2ShiftInstTable[size_t(op) - size_t(Arith3Op::kSHL)];
          cc->emit(bmi2Inst, dst, a, b.cloneAs(dst));
          return;
        }

        // Bail to `Reg, Reg, Reg` form if BMI2 is not available.
        break;
      }
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
      case Arith3Op::kAnd:
      case Arith3Op::kOr:
      case Arith3Op::kXor: {
        InstId instId = legacyLogicalInstTable[size_t(op) - size_t(Arith3Op::kAnd)];
        if (!dstIsA)
          cc->mov(dst, a);
        cc->emit(instId, dst, b);
        return;
      }

      case Arith3Op::kAndN: {
        if (!dstIsA)
          cc->mov(dst, a);
        cc->not_(dst);
        cc->and_(dst, b);
        return;
      }

      case Arith3Op::kAdd: {
        if (!dstIsA)
          cc->mov(dst, a);
        cc->add(dst, b);
        return;
      }

      case Arith3Op::kSub: {
        if (!dstIsA)
          cc->mov(dst, a);
        cc->sub(dst, b);
        return;
      }

      case Arith3Op::kMul: {
        if (!dstIsA)
          cc->mov(dst, a);
        cc->imul(dst, b);
        return;
      }

      case Arith3Op::kUDiv: {
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

      case Arith3Op::kUMod: {
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

      case Arith3Op::kSMin:
      case Arith3Op::kSMax:
      case Arith3Op::kUMin:
      case Arith3Op::kUMax: {
        const Arith3OpMinMaxCMovInst& cmovInst = arithMinMaxCMovInstTable[size_t(op) - size_t(Arith3Op::kSMin)];

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
      case Arith3Op::kAnd:
      case Arith3Op::kOr:
      case Arith3Op::kXor: {
        BL_ASSERT(dst.size() == b.size());

        InstId instId = legacyLogicalInstTable[size_t(op) - size_t(Arith3Op::kAnd)];
        if (!dstIsA)
          cc->mov(dst, a);
        cc->emit(instId, dst, b);
        return;
      }

      case Arith3Op::kAndN: {
        BL_ASSERT(dst.size() == b.size());

        if (hasBMI()) {
          cc->andn(dst, a, b);
        }
        else if (dstIsB) {
          Gp tmp = newSimilarReg(dst, "@tmp");
          cc->mov(tmp, a);
          cc->not_(a);
          cc->and_(dst, a);
        }
        else {
          if (!dstIsA)
            cc->mov(dst, a);
          cc->not_(dst);
          cc->and_(dst, b);
        }
        return;
      }

      case Arith3Op::kAdd: {
        BL_ASSERT(dst.size() == b.size());

        if (dstIsA || dstIsB) {
          cc->add(dst, dstIsB ? a : b);
        }
        else if (dst.size() >= 4) {
          lea(dst, x86::ptr(a, b));
        }
        else {
          cc->mov(dst, a);
          cc->add(dst, b);
        }
        return;
      }

      case Arith3Op::kSub: {
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

      case Arith3Op::kMul: {
        BL_ASSERT(dst.size() == b.size());

        if (!dstIsA && !dstIsB)
          cc->mov(dst, a);
        cc->imul(dst, dstIsB ? a : b);
        return;
      }

      case Arith3Op::kUDiv: {
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

      case Arith3Op::kUMod: {
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

      case Arith3Op::kSMin:
      case Arith3Op::kSMax:
      case Arith3Op::kUMin:
      case Arith3Op::kUMax: {
        BL_ASSERT(dst.size() == b.size());
        const Arith3OpMinMaxCMovInst& cmovInst = arithMinMaxCMovInstTable[size_t(op) - size_t(Arith3Op::kSMin)];

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

      case Arith3Op::kSHL:
      case Arith3Op::kSHR:
      case Arith3Op::kSAR:
      case Arith3Op::kROL:
      case Arith3Op::kROR: {
        // Prefer BMI2 variants: SHLX, SHRX, SARX, and RORX.
        if (hasBMI2()) {
          InstId bmi2Inst = bmi2ShiftInstTable[size_t(op) - size_t(Arith3Op::kSHL)];
          if (bmi2Inst != x86::Inst::kIdNone) {
            cc->emit(bmi2Inst, dst, a, b.cloneAs(dst));
            return;
          }
        }

        InstId legacyInst = legacyShiftInstTable[size_t(op) - size_t(Arith3Op::kSHL)];
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
    }
  }

  // Everything should be handled, so this should never be reached!
  BL_NOT_REACHED();
}

void PipeCompiler::emit_jmp(const Operand_& target) noexcept {
  cc->emit(x86::Inst::kIdJmp, target);
}

void PipeCompiler::emit_jmp_if(const Label& target, const Condition& condition) noexcept {
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

void PipeCompiler::lea_bpp(const Gp& dst, const Gp& src_, const Gp& idx_, uint32_t scale, int32_t disp) noexcept {
  Gp src = src_.cloneAs(dst);
  Gp idx = idx_.cloneAs(dst);

  switch (scale) {
    case 1:
      if (dst.id() == src.id() && disp == 0)
        cc->add(dst, idx);
      else
        lea(dst, x86::ptr(src, idx, 0, disp));
      break;

    case 2:
      lea(dst, x86::ptr(src, idx, 1, disp));
      break;

    case 3:
      lea(dst, x86::ptr(src, idx, 1, disp));
      cc->add(dst, idx);
      break;

    case 4:
      lea(dst, x86::ptr(src, idx, 2, disp));
      break;

    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::lea(const Gp& dst, const Mem& src) noexcept {
  Mem m(src);

  if (is64Bit() && dst.size() == 4) {
    if (m.baseType() == asmjit::RegType::kGp32) m.setBaseType(asmjit::RegType::kGp64);
    if (m.indexType() == asmjit::RegType::kGp32) m.setIndexType(asmjit::RegType::kGp64);
  }

  cc->lea(dst, m);
}

// bl::Pipeline::PipeCompiler - Emit (SIMD)
// ========================================

static inline uint32_t shuf32ToShuf64(uint32_t imm) noexcept {
  uint32_t imm0 = uint32_t(imm     ) & 1u;
  uint32_t imm1 = uint32_t(imm >> 1) & 1u;
  return x86::shuffleImm(imm1 * 2u + 1u, imm1 * 2u, imm0 * 2u + 1u, imm0 * 2u);
}

static inline void fixVecSignature(Operand_& op, OperandSignature signature) noexcept {
  if (x86::Reg::isVec(op) && op.signature().bits() > signature.bits())
    op.setSignature(signature);
}

static inline void fixVecWidthToXmm(Operand_& dst) noexcept {
  if (x86::Reg::isVec(dst)) {
    dst.as<x86::Reg>().setSignature(x86::Reg::signatureOfT<RegType::kX86_Xmm>());
  }
}

static inline void fixVecWidthToHalf(Operand_& dst, const Operand_& ref) noexcept {
  if (x86::Reg::isVec(dst) && x86::Reg::isVec(ref)) {
    dst.as<x86::Reg>().setSignature(ref.as<asmjit::BaseReg>().type() == RegType::kX86_Zmm
      ? x86::Reg::signatureOfT<RegType::kX86_Ymm>()
      : x86::Reg::signatureOfT<RegType::kX86_Xmm>());
  }
}

void PipeCompiler::v_emit_xmov(const Operand_& dst, const Operand_& src, uint32_t width) noexcept {
  if (src.isMem() || !isSameReg(dst, src)) {
    InstId instId = x86::Inst::kIdMovaps;

    if (src.isMem()) {
      switch (width) {
        case 4: instId = x86::Inst::kIdMovd; break;
        case 8: instId = x86::Inst::kIdMovq; break;
      }
    }

    cc->emit(instId, dst, src);
  }
}

void PipeCompiler::v_emit_xmov(const OpArray& dst, const Operand_& src, uint32_t width) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  while (dstIndex < dstCount) {
    v_emit_xmov(dst[dstIndex], src, width);
    dstIndex++;
  }
}

void PipeCompiler::v_emit_xmov(const OpArray& dst, const OpArray& src, uint32_t width) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  uint32_t srcIndex = 0;
  uint32_t srcCount = src.size();

  while (dstIndex < dstCount) {
    v_emit_xmov(dst[dstIndex], src[srcIndex], width);

    if (++srcIndex >= srcCount) srcIndex = 0;
    dstIndex++;
  }
}

void PipeCompiler::v_emit_vv_vv(uint32_t packedId, const Operand_& dst_, const Operand_& src_) noexcept {
  Operand dst(dst_);
  Operand src(src_);

  if (PackedInst::width(packedId) < PackedInst::kWidthZ) {
    OperandSignature signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst, signature);
    fixVecSignature(src, signature);
  }

  // Intrinsics support.
  if (PackedInst::isIntrin(packedId)) {
    switch (PackedInst::intrinId(packedId)) {
      case kIntrin2Vloadi128uRO: {
        if (hasSSE3())
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVlddqu, x86::Inst::kIdLddqu);
        else
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVmovdqu, x86::Inst::kIdMovdqu);
        break;
      }

      case kIntrin2Vmovu8u16: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpmovzxbw, x86::Inst::kIdPmovzxbw);
          fixVecWidthToHalf(src, dst);
          break;
        }

        v_emit_xmov(dst, src, 8);
        v_interleave_lo_u8(dst, dst, simdConst(&ct.i_0000000000000000, Bcst::kNA, SimdWidth::k128));
        return;
      }

      case kIntrin2Vmovu8u32: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpmovzxbd, x86::Inst::kIdPmovzxbd);
          fixVecWidthToXmm(src);
          break;
        }

        v_emit_xmov(dst, src, 4);
        v_interleave_lo_u8(dst, dst, simdConst(&ct.i_0000000000000000, Bcst::kNA, SimdWidth::k128));
        v_interleave_lo_u16(dst, dst, simdConst(&ct.i_0000000000000000, Bcst::kNA, SimdWidth::k128));
        return;
      }

      case kIntrin2Vmovu16u32: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpmovzxwd, x86::Inst::kIdPmovzxwd);
          fixVecWidthToHalf(src, dst);
          break;
        }

        v_emit_xmov(dst, src, 8);
        v_interleave_lo_u16(dst, dst, simdConst(&ct.i_0000000000000000, Bcst::kNA, SimdWidth::k128));
        return;
      }

      case kIntrin2Vabsi8: {
        if (hasSSSE3()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpabsb, x86::Inst::kIdPabsb);
          break;
        }

        if (isSameReg(dst, src)) {
          Vec tmp = newSimilarReg(dst.as<Vec>(), "@tmp");
          v_zero_i(tmp);
          v_sub_i8(tmp, tmp, dst);
          v_min_u8(dst, dst, tmp);
        }
        else {
          v_zero_i(dst);
          v_sub_i8(dst, dst, src);
          v_min_u8(dst, dst, src);
        }
        return;
      }

      case kIntrin2Vabsi16: {
        if (hasSSSE3()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpabsw, x86::Inst::kIdPabsw);
          break;
        }

        if (isSameReg(dst, src)) {
          Vec tmp = newSimilarReg(dst.as<Vec>(), "@tmp");
          v_zero_i(tmp);
          v_sub_i16(tmp, tmp, dst);
          v_max_i16(dst, dst, tmp);
        }
        else {
          v_zero_i(dst);
          v_sub_i16(dst, dst, src);
          v_max_i16(dst, dst, src);
        }
        return;
      }

      case kIntrin2Vabsi32: {
        if (hasSSSE3()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpabsd, x86::Inst::kIdPabsd);
          break;
        }

        Vec tmp = newSimilarReg(dst.as<Vec>(), "@tmp");

        v_mov(tmp, src);
        v_sra_i32(tmp, tmp, 31);
        v_xor_i32(dst, src, tmp);
        v_sub_i32(dst, dst, tmp);
        return;
      }

      case kIntrin2Vabsi64: {
        Vec tmp = newSimilarReg(dst.as<Vec>(), "@tmp");

        v_duph_i32(tmp, src);
        v_sra_i32(tmp, tmp, 31);
        v_xor_i32(dst, src, tmp);
        v_sub_i32(dst, dst, tmp);
        return;
      }

      case kIntrin2Vinv255u16: {
        Operand u16_255 = simdConst(&ct.i_00FF00FF00FF00FF, Bcst::k32, dst.as<Vec>());

        if (hasAVX() || isSameReg(dst, src)) {
          v_xor_i32(dst, src, u16_255);
        }
        else {
          v_mov(dst, u16_255);
          v_xor_i32(dst, dst, src);
        }
        return;
      }

      case kIntrin2Vinv256u16: {
        Operand u16_0100 = simdConst(&ct.i_0100010001000100, Bcst::kNA, dst.as<Vec>());

        if (!isSameReg(dst, src)) {
          v_mov(dst, u16_0100);
          v_sub_i16(dst, dst, src);
        }
        else if (hasSSSE3()) {
          v_sub_i16(dst, dst, u16_0100);
          v_abs_i16(dst, dst);
        }
        else {
          v_xor_i32(dst, dst, simdConst(&ct.i_FFFFFFFFFFFFFFFF, Bcst::kNA, dst.as<Vec>()));
          v_add_i16(dst, dst, u16_0100);
        }
        return;
      }

      case kIntrin2Vinv255u32: {
        Operand u32_255 = simdConst(&ct.i_000000FF000000FF, Bcst::kNA, dst.as<Vec>());

        if (hasAVX() || isSameReg(dst, src)) {
          v_xor_i32(dst, src, u32_255);
        }
        else {
          v_mov(dst, u32_255);
          v_xor_i32(dst, dst, src);
        }
        return;
      }

      case kIntrin2Vinv256u32: {
        BL_ASSERT(false);
        // TODO: [PIPEGEN]
        return;
      }

      case kIntrin2Vduplpd: {
        if (hasSSE3())
          vmov_dupl_2xf32_(dst, src);
        else if (hasAVX())
          v_interleave_lo_f64(dst, src, src);
        else if (isSameReg(dst, src))
          v_interleave_lo_f64(dst, dst, src);
        else
          v_dupl_i64(dst, src);
        return;
      }

      case kIntrin2Vduphpd: {
        if (hasAVX())
          v_interleave_hi_f64(dst, src, src);
        if (isSameReg(dst, src))
          v_interleave_hi_f64(dst, dst, src);
        else
          v_duph_i64(dst, src);
        return;
      }

      case kIntrin2VBroadcastU8: {
        BL_ASSERT(src.isReg() || src.isMem());

        if (src.isReg()) {
          Operand x(src);

          // Reg <- BroadcastB(Reg).
          if (src.as<x86::Reg>().isGp()) {
            if (!hasAVX2()) {
              Gp tmp = newGp32("tmp");
              cc->imul(tmp, src.as<Gp>().r32(), 0x01010101u);
              s_mov_i32(dst.as<Vec>(), tmp);
              v_swizzle_u32(dst, dst, x86::shuffleImm(0, 0, 0, 0));
              return;
            }

            if (!hasAVX512()) {
              s_mov_i32(dst.as<Vec>().xmm(), src.as<Gp>().r32());
              x = dst;
            }
            else {
              x = src.as<Gp>().r32();
            }
          }

          if (hasAVX2()) {
            if (x86::Reg::isVec(x))
              cc->emit(x86::Inst::kIdVpbroadcastb, dst, x.as<Vec>().xmm());
            else
              cc->emit(x86::Inst::kIdVpbroadcastb, dst, x);
          }
          else if (hasSSSE3()) {
            v_shuffle_i8(dst, x, simdConst(&ct.i_0000000000000000, Bcst::kNA, dst.as<Vec>()));
          }
          else {
            v_interleave_lo_u8(dst, x, x);
            v_swizzle_lo_u16(dst, dst, x86::shuffleImm(0, 0, 0, 0));
            v_swizzle_u32(dst, dst, x86::shuffleImm(0, 0, 0, 0));
          }
        }
        else {
          // Reg <- BroadcastB(Mem).
          x86::Mem m(src.as<x86::Mem>());

          m.setSize(1);
          if (hasAVX2()) {
            cc->emit(x86::Inst::kIdVpbroadcastb, dst, m);
          }
          else {
            Gp tmp = newGp32("tmp");
            cc->movzx(tmp, m);
            cc->imul(tmp, tmp, 0x01010101u);
            s_mov_i32(dst.as<Vec>(), tmp);
            v_swizzle_u32(dst, dst, x86::shuffleImm(0, 0, 0, 0));
          }
        }

        return;
      }

      case kIntrin2VBroadcastU16: {
        BL_ASSERT(src.isReg() || src.isMem());

        if (src.isReg()) {
          Operand x(src);
          // Reg <- BroadcastW(Reg).
          if (src.as<x86::Reg>().isGp()) {
            if (!hasAVX512()) {
              s_mov_i32(dst.as<Vec>().xmm(), src.as<Gp>().r32());
              x = dst;
            }
            else {
              x = src.as<Gp>().r32();
            }
          }

          if (hasAVX2()) {
            if (x86::Reg::isVec(x))
              cc->emit(x86::Inst::kIdVpbroadcastw, dst, x.as<Vec>().xmm());
            else
              cc->emit(x86::Inst::kIdVpbroadcastw, dst, x);
          }
          else {
            v_swizzle_lo_u16(dst, x, x86::shuffleImm(0, 0, 0, 0));
            v_swizzle_u32(dst, dst, x86::shuffleImm(1, 0, 1, 0));
          }
        }
        else {
          // Reg <- BroadcastW(Mem).
          x86::Mem m(src.as<x86::Mem>());

          if (hasAVX2()) {
            m.setSize(2);
            cc->emit(x86::Inst::kIdVpbroadcastw, dst, m);
          }
          else {
            if (m.size() >= 4) {
              m.setSize(4);
              v_load_i32(dst, m);
            }
            else {
              m.setSize(2);
              v_zero_i(dst);
              v_insert_u16(dst, dst, m, 0);
            }

            v_swizzle_lo_u16(dst, dst, x86::shuffleImm(0, 0, 0, 0));
            v_swizzle_u32(dst, dst, x86::shuffleImm(1, 0, 1, 0));
          }
        }

        return;
      }

      case kIntrin2VBroadcastU32: {
        BL_ASSERT(src.isReg() || src.isMem());

        if (src.isReg()) {
          Operand x(src);
          // VReg <- BroadcastD(Reg).
          if (src.as<x86::Reg>().isGp() && !hasAVX512()) {
            s_mov_i32(dst.as<Vec>().xmm(), src.as<Gp>().r32());
            x = dst;
          }

          if (x.as<x86::Reg>().isVec())
            x = x.as<Vec>().xmm();

          if (hasAVX2())
            cc->emit(x86::Inst::kIdVpbroadcastd, dst, x);
          else
            v_swizzle_u32(dst, dst, x86::shuffleImm(0, 0, 0, 0));
        }
        else {
          // VReg <- BroadcastD(Mem).
          x86::Mem m(src.as<x86::Mem>());
          m.setSize(4);

          if (hasAVX2()) {
            cc->emit(x86::Inst::kIdVpbroadcastd, dst, m);
          }
          else {
            v_load_i32(dst.as<Vec>(), m);
            v_swizzle_u32(dst, dst, x86::shuffleImm(0, 0, 0, 0));
          }
        }

        return;
      }

      case kIntrin2VBroadcastU64: {
        BL_ASSERT(src.isReg() || src.isMem());

        if (src.isReg()) {
          Operand x(src);
          // VReg <- BroadcastQ(Reg).
          if (src.as<x86::Reg>().isGp() && !hasAVX512()) {
            s_mov_i64(dst.as<Vec>().xmm(), src.as<Gp>().r64());
            x = dst;
          }

          if (x.as<x86::Reg>().isVec())
            x = x.as<Vec>().xmm();

          if (hasAVX2())
            cc->emit(x86::Inst::kIdVpbroadcastq, dst, x);
          else
            v_swizzle_u32(dst, dst, x86::shuffleImm(1, 0, 1, 0));
        }
        else {
          // VReg <- BroadcastQ(Mem).
          x86::Mem m(src.as<x86::Mem>());
          m.setSize(8);

          if (hasAVX2()) {
            cc->emit(x86::Inst::kIdVpbroadcastq, dst, m);
          }
          else {
            v_load_i64(dst.as<Vec>(), m);
            v_swizzle_u32(dst, dst, x86::shuffleImm(1, 0, 1, 0));
          }
        }

        return;
      }

      case kIntrin2VBroadcastI32x4:
      case kIntrin2VBroadcastI64x2:
      case kIntrin2VBroadcastF32x4:
      case kIntrin2VBroadcastF64x2: {
        BL_ASSERT(x86::Reg::isVec(dst));

        if (dst.as<Vec>().isXmm()) {
          if (src.isMem())
            v_loadu_i128(dst, src.as<x86::Mem>());
          else
            v_mov(dst, src);
        }
        else if (!hasAVX512()) {
          if (src.isMem()) {
            cc->vbroadcastf128(dst.as<Vec>(), src.as<x86::Mem>());
          }
          else {
            Vec srcAsXmm = src.as<Vec>().xmm();
            cc->vinsertf128(dst.as<Vec>(), srcAsXmm.ymm(), srcAsXmm, 1u);
          }
        }
        else {
          static const asmjit::InstId bcstTable[] = { x86::Inst::kIdVbroadcasti32x4, x86::Inst::kIdVbroadcasti64x2, x86::Inst::kIdVbroadcastf32x4, x86::Inst::kIdVbroadcastf64x2 };
          static const asmjit::InstId shufTable[] = { x86::Inst::kIdVshufi32x4, x86::Inst::kIdVshufi64x2, x86::Inst::kIdVshuff32x4, x86::Inst::kIdVshuff64x2 };
          static const asmjit::InstId insrTable[] = { x86::Inst::kIdVinserti32x4, x86::Inst::kIdVinserti64x2, x86::Inst::kIdVinsertf32x4, x86::Inst::kIdVinsertf64x2 };

          uint32_t tableIndex = PackedInst::intrinId(packedId) - kIntrin2VBroadcastI32x4;
          if (src.isMem()) {
            cc->emit(bcstTable[tableIndex], dst, src);
          }
          else if (dst.as<Vec>().isYmm()) {
            cc->emit(insrTable[tableIndex], dst, src.as<Vec>().cloneAs(dst.as<Vec>()), src.as<Vec>().xmm(), 1u);
          }
          else {
            Operand srcAsDst = src.as<Vec>().cloneAs(dst.as<Vec>());
            cc->emit(shufTable[tableIndex], dst, srcAsDst, srcAsDst, 0u);
          }
        }

        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }

  // Single instruction.
  InstId instId = hasAVX() ? PackedInst::avxId(packedId) : PackedInst::sseId(packedId);
  cc->emit(instId, dst, src);
}

void PipeCompiler::v_emit_vv_vv(uint32_t packedId, const OpArray& dst_, const Operand_& src_) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  while (dstIndex < dstCount) {
    v_emit_vv_vv(packedId, dst_[dstIndex], src_);
    dstIndex++;
  }
}

void PipeCompiler::v_emit_vv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src_) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  uint32_t srcIndex = 0;
  uint32_t srcCount = src_.size();

  while (dstIndex < dstCount) {
    v_emit_vv_vv(packedId, dst_[dstIndex], src_[srcIndex]);

    if (++srcIndex >= srcCount) srcIndex = 0;
    dstIndex++;
  }
}

void PipeCompiler::v_emit_vvi_vi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept {
  // Intrinsics support.
  if (PackedInst::isIntrin(packedId)) {
    switch (PackedInst::intrinId(packedId)) {
      case kIntrin2iVswizps:
        if (isSameReg(dst_, src_) || hasAVX())
          v_shuffle_f32(dst_, src_, src_, imm);
        else
          v_swizzle_u32(dst_, src_, imm);
        return;

      case kIntrin2iVswizpd:
        if (isSameReg(dst_, src_) || hasAVX())
          v_shuffle_f64(dst_, src_, src_, imm);
        else
          v_swizzle_u32(dst_, src_, shuf32ToShuf64(imm));
        return;

      default:
        BL_NOT_REACHED();
    }
  }

  // Instruction support.
  Operand dst(dst_);
  Operand src(src_);

  if (PackedInst::width(packedId) < PackedInst::kWidthZ) {
    OperandSignature signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst, signature);
    fixVecSignature(src, signature);
  }

  if (hasAVX()) {
    InstId instId = PackedInst::avxId(packedId);
    cc->emit(instId, dst, src, imm);
  }
  else {
    InstId instId = PackedInst::sseId(packedId);
    if (!isSameReg(dst, src))
      cc->emit(x86::Inst::kIdMovaps, dst, src);
    cc->emit(instId, dst, imm);
  }
}

void PipeCompiler::v_emit_vvi_vi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  while (dstIndex < dstCount) {
    v_emit_vvi_vi(packedId, dst_[dstIndex], src_, imm);
    dstIndex++;
  }
}

void PipeCompiler::v_emit_vvi_vi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  uint32_t srcIndex = 0;
  uint32_t srcCount = src_.size();

  while (dstIndex < dstCount) {
    v_emit_vvi_vi(packedId, dst_[dstIndex], src_[srcIndex], imm);

    if (++srcIndex >= srcCount) srcIndex = 0;
    dstIndex++;
  }
}

void PipeCompiler::v_emit_vvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept {
  Operand dst(dst_);
  Operand src(src_);

  if (PackedInst::width(packedId) < PackedInst::kWidthZ) {
    OperandSignature signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst, signature);
    fixVecSignature(src, signature);
  }

  InstId instId = hasAVX() ? PackedInst::avxId(packedId)
                           : PackedInst::sseId(packedId);
  cc->emit(instId, dst, src, imm);
}

void PipeCompiler::v_emit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  while (dstIndex < dstCount) {
    v_emit_vvi_vvi(packedId, dst_[dstIndex], src_, imm);
    dstIndex++;
  }
}

void PipeCompiler::v_emit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  uint32_t srcIndex = 0;
  uint32_t srcCount = src_.size();

  while (dstIndex < dstCount) {
    v_emit_vvi_vvi(packedId, dst_[dstIndex], src_[srcIndex], imm);

    if (++srcIndex >= srcCount) srcIndex = 0;
    dstIndex++;
  }
}

void PipeCompiler::v_emit_vvv_vv(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_) noexcept {
  Operand dst(dst_);
  Operand src1(src1_);
  Operand src2(src2_);

  if (PackedInst::width(packedId) < PackedInst::kWidthZ) {
    OperandSignature signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst , signature);
    fixVecSignature(src1, signature);
    fixVecSignature(src2, signature);
  }

  // Intrinsics support.
  if (PackedInst::isIntrin(packedId)) {
    switch (PackedInst::intrinId(packedId)) {
      case kIntrin3Vandi32:
      case kIntrin3Vandi64:
      case kIntrin3Vnandi32:
      case kIntrin3Vnandi64:
      case kIntrin3Vori32:
      case kIntrin3Vori64:
      case kIntrin3Vxori32:
      case kIntrin3Vxori64: {
        static constexpr const uint32_t nonAvx512Table[] = {
          PackedInst::packAvxSse(x86::Inst::kIdVpand, x86::Inst::kIdPand, PackedInst::kWidthZ),
          PackedInst::packAvxSse(x86::Inst::kIdVpand, x86::Inst::kIdPand, PackedInst::kWidthZ),
          PackedInst::packAvxSse(x86::Inst::kIdVpandn, x86::Inst::kIdPandn, PackedInst::kWidthZ),
          PackedInst::packAvxSse(x86::Inst::kIdVpandn, x86::Inst::kIdPandn, PackedInst::kWidthZ),
          PackedInst::packAvxSse(x86::Inst::kIdVpor, x86::Inst::kIdPor, PackedInst::kWidthZ),
          PackedInst::packAvxSse(x86::Inst::kIdVpor, x86::Inst::kIdPor, PackedInst::kWidthZ),
          PackedInst::packAvxSse(x86::Inst::kIdVpxor, x86::Inst::kIdPxor, PackedInst::kWidthZ),
          PackedInst::packAvxSse(x86::Inst::kIdVpxor, x86::Inst::kIdPxor, PackedInst::kWidthZ)
        };

        static constexpr const asmjit::InstId avx512Table[] = {
          x86::Inst::kIdVpandd,
          x86::Inst::kIdVpandq,
          x86::Inst::kIdVpandnd,
          x86::Inst::kIdVpandnq,
          x86::Inst::kIdVpord,
          x86::Inst::kIdVporq,
          x86::Inst::kIdVpxord,
          x86::Inst::kIdVpxorq
        };

        uint32_t tableIndex = PackedInst::intrinId(packedId) - kIntrin3Vandi32;

        if (hasAVX512() && (x86::Reg::isZmm(dst) || (src2.isMem() && src2.as<x86::Mem>().hasBroadcast()))) {
          cc->emit(avx512Table[tableIndex], dst, src1, src2);
          return;
        }

        packedId = nonAvx512Table[tableIndex];
        break;
      }

      case kIntrin3Vcombhli64: {
        // Swap Case:
        //   dst'.u64[0] = src_.u64[1];
        //   dst'.u64[1] = src_.u64[0];
        if (isSameReg(src1_, src2_)) {
          v_swap_u64(dst_, src1_);
          return;
        }

        // Dst is Src2 Case:
        //   dst'.u64[0] = src1.u64[1];
        //   dst'.u64[1] = dst_.u64[0];
        if (isSameReg(dst_, src2_) && !hasAVX()) {
          if (hasSSSE3()) {
            v_alignr_u128_(dst_, dst_, src1_, 8);
          }
          else {
            v_shuffle_f64(dst_, dst_, src1_, x86::shuffleImm(1, 0));
            v_swap_u64(dst_, dst_);
          }
          return;
        }

        // Common Case:
        //   dst'.u64[0] = src1.u64[1];
        //   dst'.u64[1] = src2.u64[0];
        v_shuffle_f64(dst_, src1_, src2_, x86::shuffleImm(0, 1));
        return;
      }

      case kIntrin3Vcombhld64: {
        // Swap Case:
        //   dst'.d64[0] = src_.d64[1];
        //   dst'.d64[1] = src_.d64[0];
        if (isSameReg(src1_, src2_)) {
          v_swap_f64(dst_, src1_);
          return;
        }

        // Dst is Src2 Case:
        //   dst'.d64[0] = src1.d64[1];
        //   dst'.d64[1] = dst_.d64[0];
        if (isSameReg(dst_, src2_) && !hasAVX()) {
          v_shuffle_f64(dst_, dst_, src1_, x86::shuffleImm(1, 0));
          v_swap_f64(dst_, dst_);
          return;
        }

        // Common Case:
        //   dst'.d64[0] = src1.d64[1];
        //   dst'.d64[1] = src2.d64[0];
        v_shuffle_f64(dst_, src1_, src2_, x86::shuffleImm(0, 1));
        return;
      }

      case kIntrin3Vminu16: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpminuw, x86::Inst::kIdPminuw);
          break;
        }

        if (isSameReg(src1, src2)) {
          v_mov(dst, src1);
          return;
        }

        if (isSameReg(dst, src2))
          std::swap(src1, src2);

        x86::Xmm tmp = cc->newXmm("@tmp");
        v_subs_u16(tmp, src1, src2);
        v_sub_i16(dst, src1, tmp);
        return;
      }

      case kIntrin3Vmaxu16: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpmaxuw, x86::Inst::kIdPmaxuw);
          break;
        }

        if (isSameReg(src1, src2)) {
          v_mov(dst, src1);
          return;
        }

        if (isSameReg(dst, src2))
          std::swap(src1, src2);

        v_subs_u16(dst, src1, src2);
        v_add_i16(dst, dst, src2);
        return;
      }

      case kIntrin3Vmulu64x32: {
        if (isSameReg(dst, src1)) {
          Vec tmp = newSimilarReg(dst.as<Vec>(), "@tmp");

          v_swizzle_u32(tmp, dst, x86::shuffleImm(2, 3, 0, 1));
          v_mulx_ll_u32_(dst, dst, src2);
          v_mulx_ll_u32_(tmp, tmp, src2);
          v_sll_i64(tmp, tmp, 32);
          v_add_i64(dst, dst, tmp);
        }
        else if (isSameReg(dst, src2)) {
          Vec tmp = newSimilarReg(dst.as<Vec>(), "@tmp");

          v_swizzle_u32(tmp, src1, x86::shuffleImm(2, 3, 0, 1));
          v_mulx_ll_u32_(tmp, tmp, dst);
          v_mulx_ll_u32_(dst, dst, src1);
          v_sll_i64(tmp, tmp, 32);
          v_add_i64(dst, dst, tmp);
        }
        else {
          v_swizzle_u32(dst, src1, x86::shuffleImm(2, 3, 0, 1));
          v_mulx_ll_u32_(dst, dst, src2);
          v_mulx_ll_u32_(src1, src1, src2);
          v_sll_i64(dst, dst, 32);
          v_add_i64(dst, dst, src1);
        }
        return;
      }

      case kIntrin3Vhaddpd: {
        if (hasSSE3()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVhaddpd, x86::Inst::kIdHaddpd);
          break;
        }

        if (isSameReg(src1, src2)) {
          if (isSameReg(dst, src1)) {
            // dst = haddpd(dst, dst);
            x86::Xmm tmp = cc->newXmmPd("@tmp");
            v_swap_f64(tmp, dst);
            v_add_f64(dst, dst, tmp);
          }
          else {
            // dst = haddpd(src1, src1);
            v_swap_f64(dst, src1);
            v_add_f64(dst, dst, src1);
          }
        }
        else {
          x86::Xmm tmp = cc->newXmmPd("@tmp");
          // dst = haddpd(src1, src2);
          v_interleave_hi_f64(tmp, src1, src2);
          v_interleave_lo_f64(dst, src1, src2);
          v_add_f64(dst, dst, tmp);
        }
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }

  // Single instruction.
  if (hasAVX()) {
    InstId instId = PackedInst::avxId(packedId);
    if (x86::Reg::isZmm(dst)) {
      if (x86::InstDB::infoById(instId).isEvexKRegOnly()) {
        x86::KReg k = cc->newKq("kTmp");
        cc->emit(instId, k, src1, src2);

        switch (instId) {
          case x86::Inst::kIdVpcmpb:
          case x86::Inst::kIdVpcmpub:
          case x86::Inst::kIdVpcmpeqb:
          case x86::Inst::kIdVpcmpgtb:
            cc->vpmovm2b(dst.as<Vec>(), k);
            break;

          case x86::Inst::kIdVpcmpw:
          case x86::Inst::kIdVpcmpuw:
          case x86::Inst::kIdVpcmpeqw:
          case x86::Inst::kIdVpcmpgtw:
          case x86::Inst::kIdVcmpph:
            cc->vpmovm2w(dst.as<Vec>(), k);
            break;

          case x86::Inst::kIdVpcmpd:
          case x86::Inst::kIdVpcmpud:
          case x86::Inst::kIdVpcmpeqd:
          case x86::Inst::kIdVpcmpgtd:
          case x86::Inst::kIdVcmpps:
            cc->vpmovm2d(dst.as<Vec>(), k);
            break;

          case x86::Inst::kIdVpcmpq:
          case x86::Inst::kIdVpcmpuq:
          case x86::Inst::kIdVpcmpeqq:
          case x86::Inst::kIdVpcmpgtq:
          case x86::Inst::kIdVcmppd:
            cc->vpmovm2q(dst.as<Vec>(), k);
            break;

          default:
            BL_NOT_REACHED();
        }

        return;
      }
      else {
        switch (instId) {
          case x86::Inst::kIdVmovdqa: instId = x86::Inst::kIdVmovdqa32; break;
          case x86::Inst::kIdVmovdqu: instId = x86::Inst::kIdVmovdqu32; break;
          case x86::Inst::kIdVpand  : instId = x86::Inst::kIdVpandd   ; break;
          case x86::Inst::kIdVpandn : instId = x86::Inst::kIdVpandnd  ; break;
          case x86::Inst::kIdVpor   : instId = x86::Inst::kIdVpord    ; break;
          case x86::Inst::kIdVpxor  : instId = x86::Inst::kIdVpxord   ; break;
          default: break;
        }
      }
    }

    cc->emit(instId, dst, src1, src2);
  }
  else {
    InstId instId = PackedInst::sseId(packedId);
    BL_ASSERT(instId != x86::Inst::kIdNone);

    if (!isSameReg(dst, src1))
      cc->emit(x86::Inst::kIdMovaps, dst, src1);

    cc->emit(instId, dst, src2);
  }
}

void PipeCompiler::v_emit_vvv_vv(uint32_t packedId, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_) noexcept {
  v_emit_vvv_vv(packedId, dst_, OpArray(src1_), src2_);
}

void PipeCompiler::v_emit_vvv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_) noexcept {
  v_emit_vvv_vv(packedId, dst_, src1_, OpArray(src2_));
}

void PipeCompiler::v_emit_vvv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  uint32_t src1Index = 0;
  uint32_t src1Count = src1_.size();

  uint32_t src2Index = 0;
  uint32_t src2Count = src2_.size();

  while (dstIndex < dstCount) {
    v_emit_vvv_vv(packedId, dst_[dstIndex], src1_[src1Index], src2_[src2Index]);

    if (++src1Index >= src1Count) src1Index = 0;
    if (++src2Index >= src2Count) src2Index = 0;
    dstIndex++;
  }
}

void PipeCompiler::v_emit_vvvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, uint32_t imm) noexcept {
  Operand dst(dst_);
  Operand src1(src1_);
  Operand src2(src2_);

  if (PackedInst::width(packedId) < PackedInst::kWidthZ) {
    OperandSignature signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst , signature);
    fixVecSignature(src1, signature);
    fixVecSignature(src2, signature);
  }

  if (PackedInst::isIntrin(packedId)) {
    switch (PackedInst::intrinId(packedId)) {
      case kIntrin3iVpalignr: {
        if (imm == 0)
          return v_mov(dst, src2);

        if (isSameReg(src1, src2)) {
          if (imm == 4 || imm == 8 || imm == 12) {
            uint32_t pred = imm ==  4 ? x86::shuffleImm(0, 3, 2, 1) :
                            imm ==  8 ? x86::shuffleImm(1, 0, 3, 2) :
                            imm == 12 ? x86::shuffleImm(2, 1, 0, 3) : 0;
            return v_swizzle_u32(dst, src1, pred);
          }
        }

        if (hasSSSE3()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpalignr, x86::Inst::kIdPalignr);
          break;
        }

        uint32_t src1Shift = (16u - imm) % 16u;
        uint32_t src2Shift = imm;

        Vec tmp = cc->newXmm("@tmp");

        if (isSameReg(dst, src1)) {
          v_srlb_u128(tmp, src2, src2Shift);
          v_sllb_u128(dst, src1, src1Shift);
          v_or_i32(dst, dst, tmp);
        }
        else {
          v_sllb_u128(tmp, src1, src1Shift);
          v_srlb_u128(dst, src2, src2Shift);
        }
        v_or_i32(dst, dst, tmp);
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }

  if (hasAVX()) {
    InstId instId = PackedInst::avxId(packedId);
    cc->emit(instId, dst, src1, src2, imm);
  }
  else {
    InstId instId = PackedInst::sseId(packedId);
    BL_ASSERT(instId != x86::Inst::kIdNone);

    if (!isSameReg(dst, src1))
      cc->emit(x86::Inst::kIdMovaps, dst, src1);

    cc->emit(instId, dst, src2, imm);
  }
}

void PipeCompiler::v_emit_vvvi_vvi(uint32_t packedId, const OpArray& dst, const Operand_& src1, const OpArray& src2, uint32_t imm) noexcept {
  v_emit_vvvi_vvi(packedId, dst, OpArray(src1), src2, imm);
}

void PipeCompiler::v_emit_vvvi_vvi(uint32_t packedId, const OpArray& dst, const OpArray& src1, const Operand_& src2, uint32_t imm) noexcept {
  v_emit_vvvi_vvi(packedId, dst, src1, OpArray(src2), imm);
}

void PipeCompiler::v_emit_vvvi_vvi(uint32_t packedId, const OpArray& dst, const OpArray& src1, const OpArray& src2, uint32_t imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  uint32_t src1Index = 0;
  uint32_t src1Count = src1.size();

  uint32_t src2Index = 0;
  uint32_t src2Count = src2.size();

  while (dstIndex < dstCount) {
    v_emit_vvvi_vvi(packedId, dst[dstIndex], src1[src1Index], src2[src2Index], imm);

    if (++src1Index >= src1Count) src1Index = 0;
    if (++src2Index >= src2Count) src2Index = 0;
    dstIndex++;
  }
}

void PipeCompiler::v_emit_vvvv_vvv(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, const Operand_& src3_) noexcept {
  Operand dst(dst_);
  Operand src1(src1_);
  Operand src2(src2_);
  Operand src3(src3_);

  if (PackedInst::width(packedId) < PackedInst::kWidthZ) {
    OperandSignature signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst , signature);
    fixVecSignature(src1, signature);
    fixVecSignature(src2, signature);
    fixVecSignature(src3, signature);
  }

  // Intrinsics support.
  if (PackedInst::isIntrin(packedId)) {
    switch (PackedInst::intrinId(packedId)) {
      case kIntrin4Vpblendvb: {
        // Blend(a, b, cond) == (a & ~cond) | (b & cond)
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpblendvb, x86::Inst::kIdPblendvb);
          break;
        }

        // Blend(a, b, cond) == a ^ ((a ^ b) &  cond)
        //                   == b ^ ((a ^ b) & ~cond)
        if (dst.id() == src1.id()) {
          x86::Xmm tmp = cc->newXmm("@tmp");
          v_xor_i32(tmp, dst, src2);
          v_and_i32(tmp, tmp, src3);
          v_xor_i32(dst, dst, tmp);
        }
        else if (dst.id() == src3.id()) {
          x86::Xmm tmp = cc->newXmm("@tmp");
          v_xor_i32(tmp, src1, src2);
          v_nand_i32(dst, dst, tmp);
          v_xor_i32(dst, dst, src2);
        }
        else {
          v_xor_i32(dst, src2, src1);
          v_and_i32(dst, dst, src3);
          v_xor_i32(dst, dst, src1);
        }
        return;
      }

      case kIntrin4VpblendvbDestructive: {
        // Blend(a, b, cond) == (a & ~cond) | (b & cond)
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpblendvb, x86::Inst::kIdPblendvb);
          break;
        }

        // Blend(a, b, cond) == a ^ ((a ^ b) &  cond)
        //                   == b ^ ((a ^ b) & ~cond)
        if (dst.id() == src3.id()) {
          v_and_i32(src2, src2, src3);
          v_nand_i32(src3, src3, src1);
          v_or_i32(dst, src3, src2);
        }
        else {
          v_and_i32(src2, src2, src3);
          v_nand_i32(src3, src3, src1);
          v_or_i32(dst, src2, src3);
        }
        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }

  if (hasAVX()) {
    InstId instId = PackedInst::avxId(packedId);
    cc->emit(instId, dst, src1, src2, src3);
  }
  else {
    InstId instId = PackedInst::sseId(packedId);
    if (dst.id() != src1.id())
      cc->emit(x86::Inst::kIdMovaps, dst, src1);
    cc->emit(instId, dst, src2, src3);
  }
}

void PipeCompiler::v_emit_vvvv_vvv(uint32_t packedId, const OpArray& dst, const OpArray& src1, const OpArray& src2, const Operand_& src3) noexcept {
  v_emit_vvvv_vvv(packedId, dst, src1, src2, OpArray(src3));
}

void PipeCompiler::v_emit_vvvv_vvv(uint32_t packedId, const OpArray& dst, const OpArray& src1, const OpArray& src2, const OpArray& src3) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  uint32_t src1Index = 0;
  uint32_t src1Count = src1.size();

  uint32_t src2Index = 0;
  uint32_t src2Count = src2.size();

  uint32_t src3Index = 0;
  uint32_t src3Count = src3.size();

  while (dstIndex < dstCount) {
    v_emit_vvvv_vvv(packedId, dst[dstIndex], src1[src1Index], src2[src2Index], src3[src3Index]);

    if (++src1Index >= src1Count) src1Index = 0;
    if (++src2Index >= src2Count) src2Index = 0;
    if (++src3Index >= src3Count) src3Index = 0;
    dstIndex++;
  }
}

void PipeCompiler::v_emit_k_vv(InstId instId, const x86::KReg& mask, const Operand_& dst, const Operand_& src) noexcept {
  cc->k(mask).emit(instId, dst, src);
}

void PipeCompiler::v_emit_k_vv(InstId instId, const x86::KReg& mask, OpArray& dst, const Operand_& src) noexcept {
  v_emit_k_vv(instId, mask, dst, OpArray(src));
}

void PipeCompiler::v_emit_k_vv(InstId instId, const x86::KReg& mask, OpArray& dst, const OpArray& src) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  uint32_t srcIndex = 0;
  uint32_t srcCount = src.size();

  while (dstIndex < dstCount) {
    cc->k(mask).emit(instId, dst[dstIndex], src[srcIndex]);

    if (++srcIndex >= srcCount) srcIndex = 0;
    dstIndex++;
  }
}

void PipeCompiler::v_emit_k_vvi(InstId instId, const x86::KReg& mask, const Operand_& dst, const Operand_& src, uint32_t imm8) noexcept {
  cc->k(mask).emit(instId, dst, src, imm8);
}

void PipeCompiler::v_emit_k_vvi(InstId instId, const x86::KReg& mask, OpArray& dst, const Operand_& src, uint32_t imm8) noexcept {
  v_emit_k_vvi(instId, mask, dst, OpArray(src), imm8);
}

void PipeCompiler::v_emit_k_vvi(InstId instId, const x86::KReg& mask, OpArray& dst, const OpArray& src, uint32_t imm8) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  uint32_t srcIndex = 0;
  uint32_t srcCount = src.size();

  Imm immOp(imm8);

  while (dstIndex < dstCount) {
    cc->k(mask).emit(instId, dst[dstIndex], src[srcIndex], immOp);

    if (++srcIndex >= srcCount) srcIndex = 0;
    dstIndex++;
  }
}

void PipeCompiler::v_emit_k_vvv(InstId instId, const x86::KReg& mask, const Operand_& dst, const Operand_& src1, const Operand_& src2) noexcept {
  cc->k(mask).emit(instId, dst, src1, src2);
}

void PipeCompiler::v_emit_k_vvv(InstId instId, const x86::KReg& mask, const OpArray& dst, const Operand_& src1, const OpArray& src2) noexcept {
  v_emit_k_vvv(instId, mask, dst, OpArray(src1), src2);
}

void PipeCompiler::v_emit_k_vvv(InstId instId, const x86::KReg& mask, const OpArray& dst, const OpArray& src1, const Operand_& src2) noexcept {
  v_emit_k_vvv(instId, mask, dst, src1, OpArray(src2));
}

void PipeCompiler::v_emit_k_vvv(InstId instId, const x86::KReg& mask, const OpArray& dst, const OpArray& src1, const OpArray& src2) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  uint32_t src1Index = 0;
  uint32_t src1Count = src1.size();

  uint32_t src2Index = 0;
  uint32_t src2Count = src2.size();

  while (dstIndex < dstCount) {
    cc->k(mask).emit(instId, dst[dstIndex], src1[src1Index], src2[src2Index]);

    if (++src1Index >= src1Count) src1Index = 0;
    if (++src2Index >= src2Count) src2Index = 0;
    dstIndex++;
  }
}

void PipeCompiler::v_emit_k_vvvi(InstId instId, const x86::KReg& mask, const Operand_& dst, const Operand_& src1, const Operand_& src2, uint32_t imm8) noexcept {
  cc->k(mask).emit(instId, dst, src1, src2, imm8);
}

void PipeCompiler::v_emit_k_vvvi(InstId instId, const x86::KReg& mask, const OpArray& dst, const Operand_& src1, const OpArray& src2, uint32_t imm8) noexcept {
  v_emit_k_vvvi(instId, mask, dst, OpArray(src1), src2, imm8);
}

void PipeCompiler::v_emit_k_vvvi(InstId instId, const x86::KReg& mask, const OpArray& dst, const OpArray& src1, const Operand_& src2, uint32_t imm8) noexcept {
  v_emit_k_vvvi(instId, mask, dst, src1, OpArray(src2), imm8);
}

void PipeCompiler::v_emit_k_vvvi(InstId instId, const x86::KReg& mask, const OpArray& dst, const OpArray& src1, const OpArray& src2, uint32_t imm8) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  uint32_t src1Index = 0;
  uint32_t src1Count = src1.size();

  uint32_t src2Index = 0;
  uint32_t src2Count = src2.size();

  Imm immOp(imm8);

  while (dstIndex < dstCount) {
    cc->k(mask).emit(instId, dst[dstIndex], src1[src1Index], src2[src2Index], immOp);

    if (++src1Index >= src1Count) src1Index = 0;
    if (++src2Index >= src2Count) src2Index = 0;
    dstIndex++;
  }
}

// bl::Pipeline::PipeCompiler - Predicate Helpers
// ==============================================

void PipeCompiler::x_make_predicate_v32(const Vec& vmask, const Gp& count) noexcept {
  x86::Mem maskPtr = _getMemConst(commonTable.loadstore16_lo8_msk8());
  maskPtr._setIndex(cc->_gpSignature.regType(), count.id());
  maskPtr.setShift(3);
  cc->vpmovsxbd(vmask, maskPtr);
}

void PipeCompiler::x_ensure_predicate_8(PixelPredicate& predicate, uint32_t maxWidth) noexcept {
  BL_ASSERT(!predicate.empty());

  blUnused(maxWidth);

  if (hasAVX512()) {
    if (!predicate.k.isValid()) {
      x86::Mem mem = _getMemConst(commonTable.k_msk16_data);
      predicate.k = cc->newKq("mask_k");
      mem._setIndex(cc->_gpSignature.regType(), predicate.count.id());
      mem.setShift(1);
      cc->kmovw(predicate.k, mem);
    }
  }
  else {
    BL_NOT_REACHED();
  }
}

void PipeCompiler::x_ensure_predicate_32(PixelPredicate& predicate, uint32_t maxWidth) noexcept {
  BL_ASSERT(!predicate.empty());

  if (hasAVX512()) {
    if (!predicate.k.isValid()) {
      x86::Mem mem = _getMemConst(commonTable.k_msk16_data);
      predicate.k = cc->newKq("mask_k");
      mem._setIndex(cc->_gpSignature.regType(), predicate.count.id());
      mem.setShift(1);
      cc->kmovw(predicate.k, mem);
    }
  }
  else {
    if (!predicate.v32.isValid()) {
      if (maxWidth <= 4)
        predicate.v32 = newXmm("mask_v32");
      else
        predicate.v32 = newYmm("mask_v32");
      x_make_predicate_v32(predicate.v32, predicate.count);
    }
  }
}

// bl::Pipeline::PipeCompiler - Fetch Helpers
// ==========================================

void PipeCompiler::x_fetch_mask_a8_advance(VecArray& vm, PixelCount n, PixelType pixelType, const Gp& mPtr, const Vec& globalAlpha) noexcept {
  x86::Mem m = x86::ptr(mPtr);

  switch (pixelType) {
    case PixelType::kA8: {
      BL_ASSERT(n != 1u);

      SimdWidth simdWidth = simdWidthOf(DataWidth::k16, n);
      uint32_t regCount = regCountOf(DataWidth::k16, n);

      newVecArray(vm, regCount, simdWidth, "vm");

      switch (n.value()) {
        case 2:
          if (hasAVX2())
            v_broadcast_u16(vm[0], m);
          else
            v_load_i16(vm[0], m);
          v_mov_u8_u16(vm[0], vm[0]);
          break;

        case 4:
          v_load_i32(vm[0], m);
          v_mov_u8_u16(vm[0], vm[0]);
          break;

        case 8:
          v_mov_u8_u16(vm[0], m);
          break;

        default: {
          for (uint32_t i = 0; i < regCount; i++) {
            v_mov_u8_u16(vm[i], m);
            m.addOffsetLo32(vm[i].size() / 2u);
          }
          break;
        }
      }

      add(mPtr, mPtr, n.value());

      if (globalAlpha.isValid()) {
        v_mul_i16(vm, vm, globalAlpha.cloneAs(vm[0]));
        v_div255_u16(vm);
      }

      break;
    }

    case PixelType::kRGBA32: {
      SimdWidth simdWidth = simdWidthOf(DataWidth::k64, n);
      uint32_t regCount = regCountOf(DataWidth::k64, n);

      newVecArray(vm, regCount, simdWidth, "vm");

      switch (n.value()) {
        case 1: {
          BL_ASSERT(regCount == 1);

          if (hasAVX2()) {
            v_broadcast_u8(vm[0], m);
            add(mPtr, mPtr, n.value());
            v_mov_u8_u16(vm[0], vm[0]);
          }
          else {
            v_load_i8(vm[0], m);
            add(mPtr, mPtr, n.value());
            v_swizzle_lo_u16(vm[0], vm[0], x86::shuffleImm(0, 0, 0, 0));
          }

          if (globalAlpha.isValid()) {
            v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
            v_div255_u16(vm[0]);
          }
          break;
        }

        case 2: {
          BL_ASSERT(regCount == 1);

          if (hasAVX2()) {
            v_mov_u8_u64_(vm[0], m);
            add(mPtr, mPtr, n.value());
            v_shuffle_i8(vm[0], vm[0], simdConst(&ct.pshufb_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));
          }
          else {
            v_load_i8(vm[0], m);
            add(mPtr, mPtr, n.value());
            v_swizzle_lo_u16(vm[0], vm[0], x86::shuffleImm(0, 0, 0, 0));
          }

          if (globalAlpha.isValid()) {
            v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
            v_div255_u16(vm[0]);
          }
          break;
        }

        case 4: {
          if (simdWidth >= SimdWidth::k256) {
            v_mov_u8_u64_(vm[0], m);
            add(mPtr, mPtr, n.value());
            v_shuffle_i8(vm[0], vm[0], simdConst(&ct.pshufb_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));

            if (globalAlpha.isValid()) {
              v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
              v_div255_u16(vm[0]);
            }
          }
          else {
            v_load_i32(vm[0], m);
            add(mPtr, mPtr, n.value());
            v_mov_u8_u16(vm[0], vm[0]);

            if (globalAlpha.isValid()) {
              v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
              v_div255_u16(vm[0]);
            }

            v_interleave_lo_u16(vm[0], vm[0], vm[0]);                 // vm[0] = [M3 M3 M2 M2 M1 M1 M0 M0]
            v_swizzle_u32(vm[1], vm[0], x86::shuffleImm(3, 3, 2, 2)); // vm[1] = [M3 M3 M3 M3 M2 M2 M2 M2]
            v_swizzle_u32(vm[0], vm[0], x86::shuffleImm(1, 1, 0, 0)); // vm[0] = [M1 M1 M1 M1 M0 M0 M0 M0]
          }
          break;
        }

        default: {
          if (simdWidth >= SimdWidth::k256) {
            for (uint32_t i = 0; i < regCount; i++) {
              v_mov_u8_u64_(vm[i], m);
              m.addOffsetLo32(vm[i].size() / 8u);
            }

            add(mPtr, mPtr, n.value());

            if (globalAlpha.isValid()) {
              if (hasOptFlag(PipeOptFlags::kFastVpmulld)) {
                v_mul_i32_(vm, vm, globalAlpha.cloneAs(vm[0]));
                v_div255_u16(vm);
                v_swizzle_u32(vm, vm, x86::shuffleImm(2, 2, 0, 0));
              }
              else {
                v_mul_i16(vm, vm, globalAlpha.cloneAs(vm[0]));
                v_div255_u16(vm);
                v_shuffle_i8(vm, vm, simdConst(&ct.pshufb_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));
              }
            }
            else {
              v_shuffle_i8(vm, vm, simdConst(&ct.pshufb_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));
            }
          }
          else {
            // Maximum pixels for 128-bit SIMD is 8 - there are no registers for more...
            BL_ASSERT(n == 8);

            v_mov_u8_u16(vm[0], m);

            if (globalAlpha.isValid()) {
              v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
              v_div255_u16(vm[0]);
            }

            add(mPtr, mPtr, n.value());

            v_interleave_hi_u16(vm[2], vm[0], vm[0]);                 // vm[2] = [M7 M7 M6 M6 M5 M5 M4 M4]
            v_interleave_lo_u16(vm[0], vm[0], vm[0]);                 // vm[0] = [M3 M3 M2 M2 M1 M1 M0 M0]
            v_swizzle_u32(vm[3], vm[2], x86::shuffleImm(3, 3, 2, 2)); // vm[3] = [M7 M7 M7 M7 M6 M6 M6 M6]
            v_swizzle_u32(vm[1], vm[0], x86::shuffleImm(3, 3, 2, 2)); // vm[1] = [M3 M3 M3 M3 M2 M2 M2 M2]
            v_swizzle_u32(vm[0], vm[0], x86::shuffleImm(1, 1, 0, 0)); // vm[0] = [M1 M1 M1 M1 M0 M0 M0 M0]
            v_swizzle_u32(vm[2], vm[2], x86::shuffleImm(1, 1, 0, 0)); // vm[2] = [M5 M5 M5 M5 M4 M4 M4 M4]
          }
          break;
        }
      }

      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::x_fetch_pixel(Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const x86::Mem& src_, Alignment alignment) noexcept {
  PixelPredicate noPredicate;
  x_fetch_pixel(p, n, flags, format, src_, alignment, noPredicate);
}

void PipeCompiler::x_fetch_pixel(Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const x86::Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept  {
  switch (p.type()) {
    case PixelType::kA8:
      _x_fetch_pixel_a8(p, n, flags, format, src_, alignment, predicate);
      break;

    case PixelType::kRGBA32:
      _x_fetch_pixel_rgba32(p, n, flags, format, src_, alignment, predicate);
      break;

    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::_x_fetch_pixel_a8(Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const x86::Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept {
  BL_ASSERT(p.isA8());

  blUnused(predicate);

  x86::Mem src(src_);

  p.setCount(n);

  SimdWidth paWidth = simdWidthOf(DataWidth::k8, n);
  SimdWidth uaWidth = simdWidthOf(DataWidth::k16, n);

  // It's forbidden to use PA in single-pixel case (scalar mode) and SA in multiple-pixel case (vector mode).
  BL_ASSERT(uint32_t(n.value() != 1) ^ uint32_t(blTestFlag(flags, PixelFlags::kSA)));

  // It's forbidden to request both - PA and UA.
  BL_ASSERT((flags & (PixelFlags::kPA | PixelFlags::kUA)) != (PixelFlags::kPA | PixelFlags::kUA));

  switch (format) {
    case FormatExt::kPRGB32: {
      SimdWidth p32Width = simdWidthOf(DataWidth::k32, n);
      uint32_t p32RegCount = SimdWidthUtils::regCountOf(p32Width, DataWidth::k32, n);

      Vec predicatedPixel;
      if (!predicate.empty()) {
        // TODO: [JIT] Do we want to support masked loading of more that 1 register?
        BL_ASSERT(n.value() > 1);
        BL_ASSERT(regCountOf(DataWidth::k32, n) == 1);

        predicatedPixel = newVec(p32Width, p.name(), "pred");
        x_ensure_predicate_32(predicate, n.value());
        v_load_predicated_v32(predicatedPixel, predicate, src);
      }

      auto fetch4Shifted = [](PipeCompiler* pc, const Vec& dst, const x86::Mem& src, Alignment alignment, const Vec& predicatedPixel) noexcept {
        if (predicatedPixel.isValid()) {
          pc->v_srl_i32(dst, predicatedPixel, 24);
        }
        else if (pc->hasAVX512()) {
          pc->v_srl_i32(dst, src, 24);
        }
        else {
          pc->v_load_i128(dst, src, alignment);
          pc->v_srl_i32(dst, dst, 24);
        }
      };

      switch (n.value()) {
        case 1: {
          p.sa = newGp32("a");
          src.addOffset(3);
          load_u8(p.sa, src);
          break;
        }

        case 4: {
          if (blTestFlag(flags, PixelFlags::kPA)) {
            newVecArray(p.pa, 1, SimdWidth::k128, p.name(), "pa");
            x86::Xmm a = p.pa[0].as<x86::Xmm>();

            fetch4Shifted(this, a, src, alignment, predicatedPixel);
            if (hasAVX512()) {
              cc->vpmovdb(a, a);
            }
            else {
              v_packs_i32_i16(a, a, a);
              v_packs_i16_u8(a, a, a);
            }

            p.pa.init(a);
          }
          else {
            newVecArray(p.ua, 1, SimdWidth::k128, p.name(), "ua");
            x86::Xmm a = p.ua[0].as<x86::Xmm>();

            fetch4Shifted(this, a, src, alignment, predicatedPixel);
            v_packs_i32_i16(a, a, a);

            p.ua.init(a);
          }

          break;
        }

        case 8: {
          x86::Xmm a0 = cc->newXmm("pa");
          if (hasAVX512()) {
            x86::Ymm aTmp = cc->newYmm("a.tmp");
            v_srl_i32(aTmp, src, 24);

            if (blTestFlag(flags, PixelFlags::kPA)) {
              cc->vpmovdb(a0, aTmp);
              p.pa.init(a0);
              rename(p.pa, p.name(), "pa");
            }
            else {
              cc->vpmovdw(a0, aTmp);
              p.ua.init(a0);
              rename(p.ua, p.name(), "ua");
            }
          }
          else {
            x86::Xmm a1 = cc->newXmm("paHi");

            fetch4Shifted(this, a0, src, alignment, predicatedPixel);
            src.addOffsetLo32(16);
            fetch4Shifted(this, a1, src, alignment, predicatedPixel);
            v_packs_i32_i16(a0, a0, a1);

            if (blTestFlag(flags, PixelFlags::kPA)) {
              v_packs_i16_u8(a0, a0, a0);
              p.pa.init(a0);
              rename(p.pa, p.name(), "pa");
            }
            else {
              p.ua.init(a0);
              rename(p.ua, p.name(), "ua");
            }
          }
          break;
        }

        case 16:
        case 32:
        case 64: {
          if (hasAVX512()) {
            VecArray p32;
            newVecArray(p32, p32RegCount, p32Width, p.name(), "p32");

            auto multiVecUnpack = [](PipeCompiler* pc, VecArray& dst, VecArray src, uint32_t srcWidth) noexcept {
              uint32_t dstVecSize = dst[0].size();

              // Number of bytes in dst registers after this is done.
              uint32_t dstWidth = blMin<uint32_t>(dst.size() * dstVecSize, src.size() * srcWidth) / dst.size();

              for (;;) {
                VecArray out;
                BL_ASSERT(srcWidth < dstWidth);

                bool isLastStep = (srcWidth * 2u == dstWidth);
                uint32_t outRegCount = blMax<uint32_t>(src.size() / 2u, 1u);

                switch (srcWidth) {
                  case 4:
                    if (isLastStep)
                      out = dst.xmm();
                    else
                      pc->newXmmArray(out, outRegCount, "tmp");
                    pc->v_interleave_lo_u32(out, src.even(), src.odd());
                    break;

                  case 8:
                    if (isLastStep)
                      out = dst.xmm();
                    else
                      pc->newXmmArray(out, outRegCount, "tmp");
                    pc->v_interleave_lo_u64(out, src.even(), src.odd());
                    break;

                  case 16:
                    if (isLastStep)
                      out = dst.ymm();
                    else
                      pc->newYmmArray(out, outRegCount, "tmp");
                    pc->v_insert_i128(out.ymm(), src.even().ymm(), src.odd().xmm(), 1);
                    break;

                  case 32:
                    BL_ASSERT(isLastStep);
                    out = dst.zmm();
                    pc->v_insert_i256(out.zmm(), src.even().zmm(), src.odd().ymm(), 1);
                    break;
                }

                srcWidth *= 2u;
                if (isLastStep)
                  break;

                src = out;
                srcWidth *= 2u;
              }
            };

            for (const Vec& v : p32) {
              if (predicatedPixel.isValid())
                v_srl_i32(v, predicatedPixel, 24);
              else
                v_srl_i32(v, src, 24);

              src.addOffset(v.size());
              if (blTestFlag(flags, PixelFlags::kPA))
                cc->vpmovdb(v.xmm(), v);
              else
                cc->vpmovdw(v.half(), v);
            }

            if (blTestFlag(flags, PixelFlags::kPA)) {
              uint32_t paRegCount = SimdWidthUtils::regCountOf(paWidth, DataWidth::k8, n);
              BL_ASSERT(paRegCount <= OpArray::kMaxSize);

              if (p32RegCount == 1) {
                p.pa.init(p32[0]);
                rename(p.pa, p.name(), "pa");
              }
              else {
                newVecArray(p.pa, paRegCount, paWidth, p.name(), "pa");
                multiVecUnpack(this, p.pa, p32, p32[0].size() / 4u);
              }
            }
            else {
              uint32_t uaRegCount = SimdWidthUtils::regCountOf(paWidth, DataWidth::k16, n);
              BL_ASSERT(uaRegCount <= OpArray::kMaxSize);

              if (p32RegCount == 1) {
                p.ua.init(p32[0]);
                rename(p.ua, p.name(), "ua");
              }
              else {
                newVecArray(p.ua, uaRegCount, uaWidth, p.name(), "ua");
                multiVecUnpack(this, p.ua, p32, p32[0].size() / 2u);
              }
            }
          }
          else {
            // TODO:
            BL_ASSERT(false);
          }

          break;
        }

        default:
          BL_NOT_REACHED();
      }

      break;
    }

    case FormatExt::kXRGB32: {
      BL_ASSERT(predicate.empty());

      switch (n.value()) {
        case 1: {
          p.sa = newGp32("a");
          cc->mov(p.sa, 255);
          break;
        }

        default:
          BL_NOT_REACHED();
      }

      break;
    }

    case FormatExt::kA8: {
      Vec predicatedPixel;
      if (!predicate.empty()) {
        // TODO: [JIT] Do we want to support masked loading of more that 1 register?
        BL_ASSERT(n.value() > 1);
        BL_ASSERT(regCountOf(DataWidth::k8, n) == 1);

        predicatedPixel = newVec(paWidth, p.name(), "pred");
        x_ensure_predicate_8(predicate, n.value());
        v_load_predicated_v8(predicatedPixel, predicate, src);
      }

      switch (n.value()) {
        case 1: {
          p.sa = newGp32("a");
          load_u8(p.sa, src);

          break;
        }

        case 4: {
          Vec a;

          if (predicatedPixel.isValid()) {
            a = predicatedPixel;
          }
          else {
            a = cc->newXmm("a");
            src.setSize(4);
            v_load_i32(a, src);
          }

          if (blTestFlag(flags, PixelFlags::kPC)) {
            p.pa.init(a);
          }
          else {
            v_mov_u8_u16(a, a);
            p.ua.init(a);
          }

          break;
        }

        case 8: {
          if (predicatedPixel.isValid()) {
            Vec a = predicatedPixel;

            if (blTestFlag(flags, PixelFlags::kPA)) {
              p.pa.init(a);
            }
            else {
              v_mov_u8_u16_(a, a);
              p.ua.init(a);
            }
          }
          else {
            Vec a = cc->newXmm("a");
            src.setSize(8);

            if (blTestFlag(flags, PixelFlags::kPA)) {
              v_load_i64(a, src);
              p.pa.init(a);
            }
            else {
              if (hasSSE4_1()) {
                v_load_i64_u8u16_(a, src);
              }
              else {
                v_load_i64(a, src);
                v_mov_u8_u16(a, a);
              }
              p.ua.init(a);
            }
          }

          break;
        }

        case 16:
        case 32:
        case 64: {
          BL_ASSERT(!predicatedPixel.isValid());

          if (simdWidth() >= SimdWidth::k256) {
            if (blTestFlag(flags, PixelFlags::kPA)) {
              uint32_t paRegCount = SimdWidthUtils::regCountOf(paWidth, DataWidth::k8, n);
              BL_ASSERT(paRegCount <= OpArray::kMaxSize);

              newVecArray(p.pa, paRegCount, paWidth, p.name(), "pa");
              src.setSize(16u << uint32_t(paWidth));

              for (uint32_t i = 0; i < paRegCount; i++) {
                v_load_ivec(p.pa[i], src, alignment);
                src.addOffsetLo32(p.pa[i].size());
              }
            }
            else {
              uint32_t uaRegCount = SimdWidthUtils::regCountOf(uaWidth, DataWidth::k16, n);
              BL_ASSERT(uaRegCount <= OpArray::kMaxSize);

              newVecArray(p.ua, uaRegCount, uaWidth, p.name(), "ua");
              src.setSize(p.ua[0].size() / 2u);

              for (uint32_t i = 0; i < uaRegCount; i++) {
                v_mov_u8_u16(p.ua[i], src);
                src.addOffsetLo32(p.ua[i].size() / 2u);
              }
            }
          }
          else {
            if (blTestFlag(flags, PixelFlags::kPA) || !hasSSE4_1()) {
              uint32_t paRegCount = regCountOf(DataWidth::k8, n);
              BL_ASSERT(paRegCount <= OpArray::kMaxSize);

              newXmmArray(p.pc, paRegCount, p.name(), "pc");
              src.setSize(16);

              for (uint32_t i = 0; i < paRegCount; i++) {
                v_load_i128(p.pc[i], src, alignment);
                src.addOffsetLo32(16);
              }
            }
            else {
              uint32_t uaRegCount = regCountOf(DataWidth::k16, n);
              BL_ASSERT(uaRegCount <= OpArray::kMaxSize);

              newXmmArray(p.ua, uaRegCount, p.name(), "ua");
              src.setSize(8);

              for (uint32_t i = 0; i < uaRegCount; i++) {
                v_mov_u8_u16(p.ua[i], src);
                src.addOffsetLo32(8);
              }
            }
          }

          break;
        }

        default:
          BL_NOT_REACHED();
      }

      break;
    }

    default:
      BL_NOT_REACHED();
  }

  _x_satisfy_pixel_a8(p, flags);
}

void PipeCompiler::_x_fetch_pixel_rgba32(Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const x86::Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept  {
  BL_ASSERT(p.isRGBA32());

  x86::Mem src(src_);
  p.setCount(n);

  switch (format) {
    // RGBA32 <- PRGB32 | XRGB32.
    case FormatExt::kPRGB32:
    case FormatExt::kXRGB32: {
      SimdWidth pcWidth = simdWidthOf(DataWidth::k32, n);
      SimdWidth ucWidth = simdWidthOf(DataWidth::k64, n);

      if (!predicate.empty()) {
        // TODO: [JIT] Do we want to support masking with more than 1 packed register?
        BL_ASSERT(regCountOf(DataWidth::k32, n) == 1);
        newVecArray(p.pc, 1, pcWidth, p.name(), "pc");

        x_ensure_predicate_32(predicate, n.value());
        v_load_predicated_v32(p.pc[0], predicate, src);
      }
      else {
        switch (n.value()) {
          case 1: {
            newXmmArray(p.pc, 1, p.name(), "pc");
            v_load_i32(p.pc[0].xmm(), src);

            break;
          }

          case 2: {
            if (blTestFlag(flags, PixelFlags::kUC) && hasSSE4_1()) {
              newXmmArray(p.uc, 1, p.name(), "uc");

              src.setSize(8);
              v_mov_u8_u16(p.pc[0].xmm(), src);
            }
            else {
              newXmmArray(p.pc, 1, p.name(), "pc");
              v_load_i64(p.pc[0].xmm(), src);
            }

            break;
          }

          case 4: {
            if (!blTestFlag(flags, PixelFlags::kPC) && use256BitSimd()) {
              newYmmArray(p.uc, 1, p.name(), "uc");

              src.setSize(16);
              v_mov_u8_u16(p.uc[0].ymm(), src);
            }
            else if (!blTestFlag(flags, PixelFlags::kPC) && hasSSE4_1()) {
              newXmmArray(p.uc, 2, p.name(), "uc");

              src.setSize(8);
              v_mov_u8_u16(p.uc[0].xmm(), src);

              src.addOffsetLo32(8);
              v_mov_u8_u16(p.uc[1].xmm(), src);
            }
            else {
              newXmmArray(p.pc, 1, p.name(), "pc");
              v_load_i128(p.pc[0], src, alignment);
            }

            break;
          }

          case 8:
          case 16:
          case 32: {
            if (simdWidth() >= SimdWidth::k256) {
              if (blTestFlag(flags, PixelFlags::kPC)) {
                uint32_t pcRegCount = SimdWidthUtils::regCountOf(pcWidth, DataWidth::k32, n);
                BL_ASSERT(pcRegCount <= OpArray::kMaxSize);

                newVecArray(p.pc, pcRegCount, pcWidth, p.name(), "pc");
                src.setSize(16u << uint32_t(pcWidth));

                for (uint32_t i = 0; i < pcRegCount; i++) {
                  v_load_ivec(p.pc[i], src, alignment);
                  src.addOffsetLo32(p.pc[i].size());
                }
              }
              else {
                uint32_t ucRegCount = SimdWidthUtils::regCountOf(ucWidth, DataWidth::k64, n);
                BL_ASSERT(ucRegCount <= OpArray::kMaxSize);

                newVecArray(p.uc, ucRegCount, ucWidth, p.name(), "uc");
                src.setSize(p.uc[0].size() / 2u);

                for (uint32_t i = 0; i < ucRegCount; i++) {
                  v_mov_u8_u16(p.uc[i], src);
                  src.addOffsetLo32(p.uc[i].size() / 2u);
                }
              }
            }
            else {
              if (blTestFlag(flags, PixelFlags::kPC) || !hasSSE4_1()) {
                uint32_t regCount = regCountOf(DataWidth::k32, n);
                BL_ASSERT(regCount <= OpArray::kMaxSize);

                newXmmArray(p.pc, regCount, p.name(), "pc");
                src.setSize(16);

                for (uint32_t i = 0; i < regCount; i++) {
                  v_load_i128(p.pc[i], src, alignment);
                  src.addOffsetLo32(16);
                }
              }
              else {
                uint32_t regCount = regCountOf(DataWidth::k64, n);
                BL_ASSERT(regCount <= OpArray::kMaxSize);

                newXmmArray(p.uc, regCount, p.name(), "uc");
                src.setSize(8);

                for (uint32_t i = 0; i < regCount; i++) {
                  v_mov_u8_u16(p.uc[i], src);
                  src.addOffsetLo32(8);
                }
              }
            }

            break;
          }

          default:
            BL_NOT_REACHED();
        }
      }

      if (format == FormatExt::kXRGB32)
        x_fill_pixel_alpha(p);

      break;
    }

    // RGBA32 <- A8.
    case FormatExt::kA8: {
      BL_ASSERT(predicate.empty());

      switch (n.value()) {
        case 1: {
          if (blTestFlag(flags, PixelFlags::kPC)) {
            newXmmArray(p.pc, 1, p.name(), "pc");

            if (hasAVX2()) {
              cc->vpbroadcastb(p.pc[0].xmm(), src);
            }
            else {
              Gp tmp = newGp32("tmp");
              load_u8(tmp, src);
              mul(tmp, tmp, 0x01010101u);
              s_mov_i32(p.pc[0], tmp);
            }
          }
          else {
            newXmmArray(p.uc, 1, p.name(), "uc");
            if (hasSSE4_1()) {
              v_zero_i(p.uc[0]);
              v_insert_u8_(p.uc[0], p.uc[0], src, 0);
              v_swizzle_lo_u16(p.uc[0], p.uc[0], x86::shuffleImm(0, 0, 0, 0));
            }
            else {
              Gp tmp = newGp32("tmp");
              load_u8(tmp, src);
              s_mov_i32(p.uc[0], tmp);
              v_swizzle_lo_u16(p.uc[0], p.uc[0], x86::shuffleImm(0, 0, 0, 0));
            }
          }

          break;
        }

        case 2: {
          if (blTestFlag(flags, PixelFlags::kPC)) {
            newXmmArray(p.pc, 1, p.name(), "pc");
            src.setSize(2);

            if (hasAVX2()) {
              cc->vpbroadcastw(p.pc[0].xmm(), src);
              v_shuffle_i8(p.pc[0], p.pc[0], simdConst(&ct.pshufb_xxxxxxxxxxxx3210_to_3333222211110000, Bcst::kNA, p.pc[0]));
            }
            else if (hasSSE4_1()) {
              v_mov_u8_u64_(p.pc[0], src);
              v_shuffle_i8(p.pc[0], p.pc[0], simdConst(&ct.pshufb_xxxxxxx1xxxxxxx0_to_zzzzzzzz11110000, Bcst::kNA, p.pc[0]));
            }
            else {
              Gp tmp = newGp32("tmp");
              load_u16(tmp, src);
              s_mov_i32(p.pc[0], tmp);
              v_interleave_lo_u8(p.pc[0], p.pc[0], p.pc[0]);
              v_interleave_lo_u16(p.pc[0], p.pc[0], p.pc[0]);
            }
          }
          else {
            // TODO: [JIT] Unfinished code.
          }

          break;
        }

        case 4: {
          src.setSize(4);
          if (blTestFlag(flags, PixelFlags::kPC)) {
            newXmmArray(p.pc, 1, p.name(), "pc");

            v_load_i32(p.pc[0], src);
            if (hasSSSE3()) {
              v_shuffle_i8(p.pc[0], p.pc[0], simdConst(&ct.pshufb_xxxxxxxxxxxx3210_to_3333222211110000, Bcst::kNA, p.pc[0]));
            }
            else {
              v_interleave_lo_u8(p.pc[0], p.pc[0], p.pc[0]);
              v_interleave_lo_u16(p.pc[0], p.pc[0], p.pc[0]);
            }
          }
          else if (use256BitSimd()) {
            newYmmArray(p.uc, 1, p.name(), "uc");

            src.setSize(4);
            v_mov_u8_u64_(p.uc, src);
            v_shuffle_i8(p.pc[0], p.pc[0], simdConst(&ct.pshufb_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, p.pc[0]));
          }
          else {
            newXmmArray(p.uc, 2, p.name(), "uc");

            v_load_i32(p.uc[0], src);
            v_interleave_lo_u8(p.uc[0], p.uc[0], p.uc[0]);
            v_mov_u8_u16(p.uc[0], p.uc[0]);

            v_swizzle_u32(p.uc[1], p.uc[0], x86::shuffleImm(3, 3, 2, 2));
            v_swizzle_u32(p.uc[0], p.uc[0], x86::shuffleImm(1, 1, 0, 0));
          }

          break;
        }

        case 8:
        case 16: {
          if (use256BitSimd()) {
            if (blTestFlag(flags, PixelFlags::kPC)) {
              uint32_t pcCount = regCountOf(DataWidth::k32, n);
              BL_ASSERT(pcCount <= OpArray::kMaxSize);

              newYmmArray(p.pc, pcCount, p.name(), "pc");
              src.setSize(8);

              for (uint32_t i = 0; i < pcCount; i++) {
                v_mov_u8_u32_(p.pc[i], src);
                src.addOffsetLo32(8);
              }

              v_shuffle_i8(p.pc, p.pc, simdConst(&ct.pshufb_xxx3xxx2xxx1xxx0_to_3333222211110000, Bcst::kNA, p.pc));
            }
            else {
              uint32_t ucCount = regCountOf(DataWidth::k64, n);
              BL_ASSERT(ucCount <= OpArray::kMaxSize);

              newYmmArray(p.uc, ucCount, p.name(), "uc");
              src.setSize(4);

              for (uint32_t i = 0; i < ucCount; i++) {
                v_mov_u8_u64_(p.uc[i], src);
                src.addOffsetLo32(4);
              }

              v_shuffle_i8(p.uc, p.uc, simdConst(&ct.pshufb_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, p.uc));
            }
          }
          else {
            src.setSize(4);
            if (blTestFlag(flags, PixelFlags::kPC)) {
              uint32_t pcCount = regCountOf(DataWidth::k32, n);
              BL_ASSERT(pcCount <= OpArray::kMaxSize);

              newXmmArray(p.pc, pcCount, p.name(), "pc");
              src.setSize(4);

              for (uint32_t i = 0; i < pcCount; i++) {
                v_load_i32(p.pc[i], src);
                src.addOffsetLo32(4);
              }

              if (hasSSSE3()) {
                v_shuffle_i8(p.uc, p.uc, simdConst(&ct.pshufb_xxx3xxx2xxx1xxx0_to_3333222211110000, Bcst::kNA, p.uc));
              }
              else {
                v_interleave_lo_u8(p.pc, p.pc, p.pc);
                v_interleave_lo_u16(p.pc, p.pc, p.pc);
              }
            }
            else {
              uint32_t ucCount = regCountOf(DataWidth::k64, n);
              BL_ASSERT(ucCount == 4);

              newXmmArray(p.uc, ucCount, p.name(), "uc");

              v_load_i32(p.uc[0], src);
              src.addOffsetLo32(4);
              v_load_i32(p.uc[2], src);

              v_interleave_lo_u8(p.uc[0], p.uc[0], p.uc[0]);
              v_interleave_lo_u8(p.uc[2], p.uc[2], p.uc[2]);

              v_mov_u8_u16(p.uc[0], p.uc[0]);
              v_mov_u8_u16(p.uc[2], p.uc[2]);

              v_swizzle_u32(p.uc[1], p.uc[0], x86::shuffleImm(3, 3, 2, 2));
              v_swizzle_u32(p.uc[3], p.uc[2], x86::shuffleImm(3, 3, 2, 2));
              v_swizzle_u32(p.uc[0], p.uc[0], x86::shuffleImm(1, 1, 0, 0));
              v_swizzle_u32(p.uc[2], p.uc[2], x86::shuffleImm(1, 1, 0, 0));
            }
          }

          break;
        }

        default:
          BL_NOT_REACHED();
      }

      break;
    }

    // RGBA32 <- Unknown?
    default:
      BL_NOT_REACHED();
  }

  _x_satisfy_pixel_rgba32(p, flags);
}

void PipeCompiler::x_satisfy_pixel(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.count() != 0);

  switch (p.type()) {
    case PixelType::kA8:
      _x_satisfy_pixel_a8(p, flags);
      break;

    case PixelType::kRGBA32:
      _x_satisfy_pixel_rgba32(p, flags);
      break;

    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::_x_satisfy_pixel_a8(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kA8);
  BL_ASSERT(p.count() != 0);

  // Scalar mode uses only SA.
  if (p.count() == 1) {
    BL_ASSERT( blTestFlag(flags, PixelFlags::kSA));
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kPA | PixelFlags::kUA));

    return;
  }

  if (blTestFlag(flags, PixelFlags::kPA) && p.pa.empty()) {
    // Either PA or UA, but never both.
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kUA));

    _x_pack_pixel(p.pa, p.ua, p.count().value(), p.name(), "pa");
  }
  else if (blTestFlag(flags, PixelFlags::kUA) && p.ua.empty()) {
    // Either PA or UA, but never both.
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kPA));

    _x_unpack_pixel(p.ua, p.pa, p.count().value(), p.name(), "ua");
  }

  if (blTestFlag(flags, PixelFlags::kUA | PixelFlags::kUI)) {
    if (p.ua.empty()) {
      // TODO: A8 pipeline - finalize satisfy-pixel.
      BL_ASSERT(false);
    }
  }
}

void PipeCompiler::_x_satisfy_pixel_rgba32(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kRGBA32);
  BL_ASSERT(p.count() != 0);

  // Quick reject if all flags were satisfied already or no flags were given.
  if ((!blTestFlag(flags, PixelFlags::kPC) || !p.pc.empty()) &&
      (!blTestFlag(flags, PixelFlags::kUC) || !p.uc.empty()) &&
      (!blTestFlag(flags, PixelFlags::kUA) || !p.ua.empty()) &&
      (!blTestFlag(flags, PixelFlags::kUI) || !p.ui.empty()))
    return;

  // Only fetch unpacked alpha if we already have unpacked pixels. Wait otherwise as fetch flags may contain
  // `PixelFlags::kUC`, which is handled below. This is an optimization for cases in which the caller wants
  // packed RGBA and unpacked alpha.
  if (blTestFlag(flags, PixelFlags::kUA | PixelFlags::kUI) && p.ua.empty() && !p.uc.empty()) {
    // Emit pshuflw/pshufhw sequence for every unpacked pixel.
    newVecArray(p.ua, p.uc.size(), p.uc[0], p.name(), "ua");

    if (hasAVX()) {
      v_shuffle_i8(p.ua, p.uc, simdConst(&ct.pshufb_32xxxxxx10xxxxxx_to_3232323210101010, Bcst::kNA, p.ua));
    }
    else {
      v_expand_alpha_16(p.ua, p.uc, true);
    }
  }

  // Pack or unpack sequence.
  //
  // The following code handles packing or unpacking pixels. Typically, depending on a fetcher, either
  // packed or unpacked pixels are assigned to a `Pixel`. Then, the consumer of that pixel decides which
  // format to use. So, if there is a mismatch, we have to emit a pack/unpack sequence. Unpacked pixels
  // are needed for almost everything except some special cases like SRC_COPY and PLUS without a mask.

  // Either PC or UC, but never both.
  BL_ASSERT((flags & (PixelFlags::kPC | PixelFlags::kUC)) != (PixelFlags::kPC | PixelFlags::kUC));

  if (blTestFlag(flags, PixelFlags::kPC) && p.pc.empty()) {
    _x_pack_pixel(p.pc, p.uc, p.count().value() * 4u, p.name(), "pc");
  }
  else if (blTestFlag(flags, PixelFlags::kUC) && p.uc.empty()) {
    _x_unpack_pixel(p.uc, p.pc, p.count().value() * 4, p.name(), "uc");
  }

  // Unpack alpha from either packed or unpacked pixels.
  if (blTestFlag(flags, PixelFlags::kUA | PixelFlags::kUI) && p.ua.empty()) {
    // This time we have to really fetch A8/IA8, if we haven't before.
    BL_ASSERT(!p.pc.empty() || !p.uc.empty());

    uint32_t uaCount = regCountOf(DataWidth::k64, p.count());
    BL_ASSERT(uaCount <= OpArray::kMaxSize);

    if (!p.uc.empty()) {
      newVecArray(p.ua, uaCount, p.uc[0], p.name(), "ua");
      if (hasAVX()) {
        v_shuffle_i8(p.ua, p.uc, simdConst(&ct.pshufb_32xxxxxx10xxxxxx_to_3232323210101010, Bcst::kNA, p.ua));
      }
      else {
        v_expand_alpha_16(p.ua, p.uc, p.count() > 1);
      }
    }
    else {
      if (p.count() <= 2) {
        newXmmArray(p.ua, uaCount, p.name(), "ua");
        if (hasAVX() || p.count() == 2u) {
          v_shuffle_i8(p.ua[0], p.pc[0], simdConst(&ct.pshufb_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, p.ua[0]));
        }
        else {
          v_swizzle_lo_u16(p.ua[0], p.pc[0], x86::shuffleImm(1, 1, 1, 1));
          v_srl_i16(p.ua[0], p.ua[0], 8);
        }
      }
      else {
        SimdWidth ucWidth = simdWidthOf(DataWidth::k64, p.count());
        newVecArray(p.ua, uaCount, ucWidth, p.name(), "ua");

        if (ucWidth == SimdWidth::k512) {
          if (uaCount == 1) {
            v_mov_u8_u16_(p.ua[0], p.pc[0].ymm());
          }
          else {
            v_extract_i256(p.ua.odd().ymm(), p.pc.zmm(), 1);
            v_mov_u8_u16_(p.ua.even(), p.pc.ymm());
            v_mov_u8_u16_(p.ua.odd(), p.ua.odd().ymm());
          }

          v_shuffle_i8(p.ua, p.ua, simdConst(&ct.pshufb_32xxxxxx10xxxxxx_to_3232323210101010, Bcst::kNA, p.ua));
        }
        else if (ucWidth == SimdWidth::k256) {
          if (uaCount == 1) {
            v_mov_u8_u16_(p.ua[0], p.pc[0].xmm());
          }
          else {
            v_extract_i128(p.ua.odd().xmm(), p.pc.ymm(), 1);
            v_mov_u8_u16_(p.ua.even(), p.pc.xmm());
            v_mov_u8_u16_(p.ua.odd(), p.ua.odd().xmm());
          }

          v_shuffle_i8(p.ua, p.ua, simdConst(&ct.pshufb_32xxxxxx10xxxxxx_to_3232323210101010, Bcst::kNA, p.ua));
        }
        else {
          for (uint32_t i = 0; i < p.pc.size(); i++)
            xExtractUnpackedAFromPackedARGB32_4(p.ua[i * 2], p.ua[i * 2 + 1], p.pc[i]);
        }
      }
    }
  }

  if (blTestFlag(flags, PixelFlags::kUI) && p.ui.empty()) {
    if (hasAVX() || blTestFlag(flags, PixelFlags::kUA)) {
      newVecArray(p.ui, p.ua.size(), p.ua[0], p.name(), "ui");
      v_inv255_u16(p.ui, p.ua);
    }
    else {
      p.ui.init(p.ua);
      v_inv255_u16(p.ui, p.ua);

      p.ua.reset();
      rename(p.ui, p.name(), "ui");
    }
  }
}

void PipeCompiler::x_satisfy_solid(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.count() != 0);

  switch (p.type()) {
    case PixelType::kA8:
      _x_satisfy_solid_a8(p, flags);
      break;

    case PixelType::kRGBA32:
      _x_satisfy_solid_rgba32(p, flags);
      break;

    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::_x_satisfy_solid_a8(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kA8);
  BL_ASSERT(p.count() != 0);

  if (blTestFlag(flags, PixelFlags::kPA) && p.pa.empty()) {
    BL_ASSERT(!p.ua.empty());
    newVecArray(p.pa, 1, p.name(), "pa");
    v_packs_i16_u8(p.pa[0], p.ua[0], p.ua[0]);
  }

  // TODO: A8 pipeline - finalize solid-alpha.
}

void PipeCompiler::_x_satisfy_solid_rgba32(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kRGBA32);
  BL_ASSERT(p.count() != 0);

  if (blTestFlag(flags, PixelFlags::kPC) && p.pc.empty()) {
    BL_ASSERT(!p.uc.empty());

    newVecArray(p.pc, 1, p.name(), "pc");
    v_mov(p.pc[0], p.uc[0]);
    v_packs_i16_u8(p.pc[0], p.pc[0], p.pc[0]);
  }

  if (blTestFlag(flags, PixelFlags::kUC) && p.uc.empty()) {
    BL_ASSERT(!p.pc.empty());

    newVecArray(p.uc, 1, p.name(), "uc");
    v_mov_u8_u16(p.uc[0], p.pc[0]);
  }

  if (blTestFlag(flags, PixelFlags::kUA) && p.ua.empty()) {
    newVecArray(p.ua, 1, p.name(), "ua");

    if (!p.uc.empty()) {
      v_swizzle_lo_u16(p.ua[0], p.uc[0], x86::shuffleImm(3, 3, 3, 3));
      v_swizzle_u32(p.ua[0], p.ua[0], x86::shuffleImm(1, 0, 1, 0));
    }
    else {
      v_swizzle_lo_u16(p.ua[0], p.pc[0], x86::shuffleImm(1, 1, 1, 1));
      v_swizzle_u32(p.ua[0], p.ua[0], x86::shuffleImm(1, 0, 1, 0));
      v_srl_i16(p.ua[0], p.ua[0], 8);
    }
  }

  if (blTestFlag(flags, PixelFlags::kUI) && p.ui.empty()) {
    newVecArray(p.ui, 1, p.name(), "ui");

    if (!p.ua.empty()) {
      v_mov(p.ui[0], p.ua[0]);
    }
    else if (!p.uc.empty()) {
      v_swizzle_lo_u16(p.ui[0], p.uc[0], x86::shuffleImm(3, 3, 3, 3));
      v_swizzle_u32(p.ui[0], p.ui[0], x86::shuffleImm(1, 0, 1, 0));
    }
    else {
      v_swizzle_lo_u16(p.ui[0], p.pc[0], x86::shuffleImm(1, 1, 1, 1));
      v_swizzle_u32(p.ui[0], p.ui[0], x86::shuffleImm(1, 0, 1, 0));
      v_srl_i16(p.ui[0], p.ui[0], 8);
    }

    v_inv255_u16(p.ui[0], p.ui[0]);
  }
}

// Emits a pixel packing sequence.
void PipeCompiler::_x_pack_pixel(VecArray& px, VecArray& ux, uint32_t n, const char* prefix, const char* pxName) noexcept {
  BL_ASSERT( px.empty());
  BL_ASSERT(!ux.empty());

  if (hasAVX512() && ux[0].type() >= asmjit::RegType::kX86_Ymm) {
    SimdWidth pxWidth = simdWidthOf(DataWidth::k8, n);
    uint32_t pxCount = regCountOf(DataWidth::k8, n);
    BL_ASSERT(pxCount <= OpArray::kMaxSize);

    newVecArray(px, pxCount, pxWidth, prefix, pxName);

    if (ux.size() == 1) {
      // Pack ZMM->YMM or YMM->XMM.
      BL_ASSERT(pxCount == 1);
      cc->vpmovwb(px[0], ux[0]);
      ux.reset();
      return;
    }
    else if (ux[0].type() >= asmjit::RegType::kX86_Zmm) {
      // Pack ZMM to ZMM.
      VecArray pxTmp;
      newYmmArray(pxTmp, ux.size(), prefix, "pxTmp");

      for (uint32_t i = 0; i < ux.size(); i++)
        cc->vpmovwb(pxTmp[i].ymm(), ux[i]);

      for (uint32_t i = 0; i < ux.size(); i += 2)
        cc->vinserti32x8(px[i / 2u].zmm(), pxTmp[i].zmm(), pxTmp[i + 1u].ymm(), 1);

      ux.reset();
      return;
    }
  }

  if (hasAVX()) {
    uint32_t pxCount = regCountOf(DataWidth::k8, n);
    BL_ASSERT(pxCount <= OpArray::kMaxSize);

    if (ux[0].type() >= asmjit::RegType::kX86_Ymm) {
      if (ux.size() == 1) {
        // Pack YMM to XMM.
        BL_ASSERT(pxCount == 1);

        x86::Ymm pTmp = cc->newYmm("pTmp");
        newXmmArray(px, pxCount, prefix, pxName);

        v_packs_i16_u8(pTmp, ux[0], ux[0]);
        v_perm_i64(px[0].ymm(), pTmp, x86::shuffleImm(3, 1, 2, 0));
      }
      else {
        newYmmArray(px, pxCount, prefix, pxName);
        v_packs_i16_u8(px, ux.even(), ux.odd());
        v_perm_i64(px, px, x86::shuffleImm(3, 1, 2, 0));
      }
    }
    else {
      newXmmArray(px, pxCount, prefix, pxName);
      v_packs_i16_u8(px, ux.even(), ux.odd());
    }
    ux.reset();
  }
  else {
    // NOTE: This is only used by a non-AVX pipeline. Renaming makes no sense when in AVX mode. Additionally,
    // we may need to pack to XMM register from two YMM registers, so the register types don't have to match
    // if the pipeline is using 256-bit SIMD or higher.
    px.init(ux.even());
    rename(px, prefix, pxName);

    v_packs_i16_u8(px, ux.even(), ux.odd());
    ux.reset();
  }
}

// Emits a pixel unpacking sequence.
void PipeCompiler::_x_unpack_pixel(VecArray& ux, VecArray& px, uint32_t n, const char* prefix, const char* uxName) noexcept {
  BL_ASSERT( ux.empty());
  BL_ASSERT(!px.empty());

  SimdWidth uxWidth = simdWidthOf(DataWidth::k16, n);
  uint32_t uxCount = regCountOf(DataWidth::k16, n);
  BL_ASSERT(uxCount <= OpArray::kMaxSize);

  if (hasAVX()) {
    newVecArray(ux, uxCount, uxWidth, prefix, uxName);

    if (uxWidth == SimdWidth::k512) {
      if (uxCount == 1) {
        v_mov_u8_u16_(ux[0], px[0].ymm());
      }
      else {
        v_extract_i256(ux.odd().ymm(), px, 1);
        v_mov_u8_u16_(ux.even(), px.ymm());
        v_mov_u8_u16_(ux.odd(), ux.odd().ymm());
      }
    }
    else if (uxWidth == SimdWidth::k256 && n >= 16) {
      if (uxCount == 1) {
        v_mov_u8_u16_(ux[0], px[0].xmm());
      }
      else {
        v_extract_i128(ux.odd().xmm(), px, 1);
        v_mov_u8_u16_(ux.even(), px.xmm());
        v_mov_u8_u16_(ux.odd(), ux.odd().xmm());
      }
    }
    else {
      for (uint32_t i = 0; i < uxCount; i++) {
        if (i & 1)
          v_shuffle_i8(ux[i], px[i / 2u], simdConst(&commonTable.pshufb_76543210xxxxxxxx_to_z7z6z5z4z3z2z1z0, Bcst::kNA, ux[i]));
        else
          v_mov_u8_u16_(ux[i], px[i / 2u]);
      }
    }
  }
  else {
    if (n <= 8) {
      ux.init(px[0]);
      v_mov_u8_u16(ux[0], ux[0]);
    }
    else {
      ux._size = px.size() * 2;
      for (uint32_t i = 0; i < px.size(); i++) {
        ux[i * 2 + 0] = px[i];
        ux[i * 2 + 1] = cc->newXmm();
        xMovzxBW_LoHi(ux[i * 2 + 0], ux[i * 2 + 1], ux[i * 2 + 0]);
      }
    }

    px.reset();
    rename(ux, prefix, uxName);
  }
}

void PipeCompiler::x_fetch_unpacked_a8_2x(const Vec& dst, FormatExt format, const x86::Mem& src1, const x86::Mem& src0) noexcept {
  x86::Mem m0 = src0;
  x86::Mem m1 = src1;

  m0.setSize(1);
  m1.setSize(1);

  if (format == FormatExt::kPRGB32) {
    m0.addOffset(3);
    m1.addOffset(3);
  }

  if (hasSSE4_1()) {
    v_zero_i(dst);
    v_insert_u8_(dst, dst, m0, 0);
    v_insert_u8_(dst, dst, m1, 2);
  }
  else {
    Gp aGp = newGp32("aGp");
    cc->movzx(aGp, m1);
    cc->shl(aGp, 16);
    cc->mov(aGp.r8(), m0);
    s_mov_i32(dst, aGp);
  }
}

void PipeCompiler::x_assign_unpacked_alpha_values(Pixel& p, PixelFlags flags, Vec& vec) noexcept {
  blUnused(flags);

  BL_ASSERT(p.type() != PixelType::kNone);
  BL_ASSERT(p.count() != 0);

  Vec v0 = vec;

  if (p.isRGBA32()) {
    switch (p.count().value()) {
      case 1: {
        v_swizzle_lo_u16(v0, v0, x86::shuffleImm(0, 0, 0, 0));

        p.uc.init(v0);
        break;
      }

      case 2: {
        v_interleave_lo_u16(v0, v0, v0);
        v_swizzle_u32(v0, v0, x86::shuffleImm(1, 1, 0, 0));

        p.uc.init(v0);
        break;
      }

      case 4: {
        x86::Xmm v1 = cc->newXmm();

        v_interleave_lo_u16(v0, v0, v0);
        v_swizzle_u32(v1, v0, x86::shuffleImm(3, 3, 2, 2));
        v_swizzle_u32(v0, v0, x86::shuffleImm(1, 1, 0, 0));

        p.uc.init(v0, v1);
        break;
      }

      case 8: {
        Vec v1 = cc->newXmm();
        Vec v2 = cc->newXmm();
        Vec v3 = cc->newXmm();

        v_interleave_hi_u16(v2, v0, v0);
        v_interleave_lo_u16(v0, v0, v0);

        v_swizzle_u32(v1, v0, x86::shuffleImm(3, 3, 2, 2));
        v_swizzle_u32(v0, v0, x86::shuffleImm(1, 1, 0, 0));
        v_swizzle_u32(v3, v2, x86::shuffleImm(3, 3, 2, 2));
        v_swizzle_u32(v2, v2, x86::shuffleImm(1, 1, 0, 0));

        p.uc.init(v0, v1, v2, v3);
        break;
      }

      default:
        BL_NOT_REACHED();
    }

    rename(p.uc, "uc");
  }
  else {
    switch (p.count().value()) {
      case 1: {
        BL_ASSERT(blTestFlag(flags, PixelFlags::kSA));

        Gp sa = newGp32("sa");
        v_extract_u16(sa, vec, 0);

        p.sa = sa;
        break;
      }

      default: {
        p.ua.init(vec);
        rename(p.ua, p.name(), "ua");
        break;
      }
    }
  }
}

void PipeCompiler::x_fill_pixel_alpha(Pixel& p) noexcept {
  switch (p.type()) {
    case PixelType::kRGBA32:
      if (!p.pc.empty()) vFillAlpha255B(p.pc, p.pc);
      if (!p.uc.empty()) vFillAlpha255W(p.uc, p.uc);
      break;

    case PixelType::kA8:
      break;

    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::x_store_pixel_advance(const Gp& dPtr, Pixel& p, PixelCount n, uint32_t bpp, Alignment alignment, PixelPredicate& predicate) noexcept {
  x86::Mem dMem = x86::ptr(dPtr);

  switch (bpp) {
    case 1: {
      if (!predicate.empty()) {
        // Predicated pixel count must be greater than 1!
        BL_ASSERT(n != 1);

        x_satisfy_pixel(p, PixelFlags::kPA | PixelFlags::kImmutable);

        x_ensure_predicate_8(predicate, n.value());
        v_store_predicated_v8(dMem, predicate, p.pa[0]);
        add(dPtr, dPtr, predicate.count.cloneAs(dPtr));
      }
      else {
        if (n == 1) {
          x_satisfy_pixel(p, PixelFlags::kSA | PixelFlags::kImmutable);
          store_8(dMem, p.sa);
        }
        else {
          x_satisfy_pixel(p, PixelFlags::kPA | PixelFlags::kImmutable);

          if (n <= 16) {
            v_store_iany(dMem, p.pa[0], n.value(), alignment);
          }
          else {
            x_satisfy_pixel(p, PixelFlags::kPA | PixelFlags::kImmutable);

            uint32_t pcIndex = 0;
            uint32_t vecSize = p.pa[0].size();
            uint32_t pixelsPerReg = vecSize;

            for (uint32_t i = 0; i < n.value(); i += pixelsPerReg) {
              v_store_ivec(dMem, p.pa[pcIndex], alignment);
              if (++pcIndex >= p.pa.size())
                pcIndex = 0;
              dMem.addOffset(vecSize);
            }
          }
        }

        add(dPtr, dPtr, n.value());
      }

      break;
    }

    case 4: {
      if (!predicate.empty()) {
        x_satisfy_pixel(p, PixelFlags::kPC | PixelFlags::kImmutable);

        if (hasOptFlag(PipeOptFlags::kFastStoreWithMask)) {
          x_ensure_predicate_32(predicate, n.value());
          v_store_predicated_v32(dMem, predicate, p.pc[0]);
          add_scaled(dPtr, predicate.count.cloneAs(dPtr), bpp);
        }
        else {
          Label L_StoreSkip1 = newLabel();

          const Gp& count = predicate.count;
          const Vec& pc0 = p.pc[0];

          if (n > 8) {
            Label L_StoreSkip8 = newLabel();
            x86::Ymm pc0YmmHigh = cc->newYmm("pc0.ymmHigh");

            v_extract_i256(pc0YmmHigh, pc0.zmm(), 1);
            j(L_StoreSkip8, bt_z(count, 3));
            v_storeu_i256(dMem, pc0.ymm());
            v_mov(pc0.ymm(), pc0YmmHigh);
            add(dPtr, dPtr, 8u * 4u);
            bind(L_StoreSkip8);
          }

          if (n > 4) {
            Label L_StoreSkip4 = newLabel();
            x86::Xmm pc0XmmHigh = cc->newXmm("pc0.xmmHigh");

            v_extract_i128(pc0XmmHigh, pc0.ymm(), 1);
            j(L_StoreSkip4, bt_z(count, 2));
            v_storeu_i128(dMem, pc0.xmm());
            v_mov(pc0.xmm(), pc0XmmHigh);
            add(dPtr, dPtr, 4u * 4u);
            bind(L_StoreSkip4);
          }

          if (n > 2) {
            Label L_StoreSkip2 = newLabel();

            j(L_StoreSkip2, bt_z(count, 1));
            v_store_i64(dMem, pc0.xmm());
            v_srlb_u128(pc0.xmm(), pc0.xmm(), 8);
            add(dPtr, dPtr, 2u * 4u);
            bind(L_StoreSkip2);
          }

          j(L_StoreSkip1, bt_z(count, 0));
          v_store_i32(dMem, pc0.xmm());
          add(dPtr, dPtr, 1u * 4u);
          bind(L_StoreSkip1);
        }
      }
      else {
        if (hasAVX512() && n >= 2 && !p.uc.empty()) {
          uint32_t ucIndex = 0;
          uint32_t vecSize = p.uc[0].size();
          uint32_t pixelsPerReg = vecSize / 8u;

          for (uint32_t i = 0; i < n.value(); i += pixelsPerReg) {
            cc->vpmovwb(dMem, p.uc[ucIndex]);
            if (++ucIndex >= p.uc.size())
              ucIndex = 0;
            dMem.addOffset(vecSize / 2u);
          }
        }
        else {
          x_satisfy_pixel(p, PixelFlags::kPC | PixelFlags::kImmutable);

          if (n <= 4) {
            v_store_iany(dMem, p.pc[0], n.value() * 4u, alignment);
          }
          else {
            uint32_t pcIndex = 0;
            uint32_t vecSize = p.pc[0].size();
            uint32_t pixelsPerReg = vecSize / 4u;

            for (uint32_t i = 0; i < n.value(); i += pixelsPerReg) {
              v_store_ivec(dMem, p.pc[pcIndex], alignment);
              if (++pcIndex >= p.pc.size())
                pcIndex = 0;
              dMem.addOffset(vecSize);
            }
          }
        }
        cc->add(dPtr, n.value() * 4);
      }

      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::PipeCompiler - PixelFill
// ======================================

void PipeCompiler::x_inline_pixel_fill_loop(Gp& dst, Vec& src, Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity) noexcept {
  BL_ASSERT(IntOps::isPowerOf2(itemSize));
  BL_ASSERT(itemSize <= 16u);

  uint32_t granularityInBytes = itemSize * itemGranularity;
  uint32_t mainStepInItems = mainLoopSize / itemSize;

  BL_ASSERT(IntOps::isPowerOf2(granularityInBytes));
  BL_ASSERT(mainStepInItems * itemSize == mainLoopSize);

  BL_ASSERT(mainLoopSize >= 16u);
  BL_ASSERT(mainLoopSize >= granularityInBytes);

  uint32_t k;
  uint32_t vecSize = src.size();

  // Granularity >= 16 Bytes
  // -----------------------

  if (granularityInBytes >= 16u) {
    Label L_End = newLabel();

    // MainLoop
    // --------

    {
      Label L_MainIter = newLabel();
      Label L_MainSkip = newLabel();

      j(L_MainSkip, sub_c(i, mainStepInItems));
      bind(L_MainIter);
      add(dst, dst, mainLoopSize);
      x_storeu_fill(x86::ptr(dst, -int(mainLoopSize)), src, mainLoopSize);
      j(L_MainIter, sub_nc(i, mainStepInItems));

      bind(L_MainSkip);
      j(L_End, add_z(i, mainStepInItems));
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize * 2 > granularityInBytes) {
      Label L_TailIter = newLabel();

      bind(L_TailIter);
      x_storeu_fill(x86::ptr(dst), src, granularityInBytes);
      add(dst, dst, granularityInBytes);
      j(L_TailIter, sub_nz(i, itemGranularity));
    }
    else if (mainLoopSize * 2 == granularityInBytes) {
      x_storeu_fill(x86::ptr(dst), src, granularityInBytes);
      add(dst, dst, granularityInBytes);
    }

    bind(L_End);
    return;
  }

  // Granularity == 4 Bytes
  // ----------------------

  if (granularityInBytes == 4u) {
    BL_ASSERT(itemSize <= 4u);

    uint32_t sizeShift = IntOps::ctz(itemSize);
    uint32_t alignPattern = ((vecSize - 1u) * itemSize) & (vecSize - 1u);

    uint32_t oneStepInItems = 4u >> sizeShift;
    uint32_t tailStepInItems = 16u >> sizeShift;

    if (vecSize >= 32u) {
      // Make `i` contain the number of 32-bit units to fill.
      Gp i_ptr = i.cloneAs(dst);
      if (itemSize != 4u)
        shr(i, i, 2u - sizeShift);

      if (hasMaskedAccessOf(4) && hasOptFlag(PipeOptFlags::kFastStoreWithMask)) {
        Label L_MainIter = newLabel();
        Label L_MainSkip = newLabel();
        Label L_TailIter = newLabel();
        Label L_TailSkip = newLabel();
        Label L_End = newLabel();

        j(L_MainSkip, sub_c(i_ptr, vecSize));

        bind(L_MainIter);
        x_storeu_fill(x86::ptr(dst), src, vecSize * 4u);
        add(dst, dst, vecSize * 4u);
        j(L_MainIter, sub_nc(i_ptr, vecSize));

        bind(L_MainSkip);
        j(L_TailSkip, add_s(i_ptr, vecSize - vecSize / 4u));

        bind(L_TailIter);
        x_storeu_fill(x86::ptr(dst), src, vecSize);
        add(dst, dst, vecSize);
        j(L_TailIter, sub_nc(i_ptr, vecSize / 4u));

        bind(L_TailSkip);
        j(L_End, add_z(i_ptr, vecSize / 4u));

        PixelPredicate predicate(vecSize / 4u, PredicateFlags::kNeverEmptyOrFull, i);
        x_ensure_predicate_32(predicate, vecSize / 4u);
        v_store_predicated_v32(x86::ptr(dst), predicate, src);

        lea(dst, x86::ptr(dst, i_ptr, 2));
        bind(L_End);
      }
      else {
        Label L_LargeIter = newLabel();
        Label L_SmallIter = newLabel();
        Label L_SmallCheck = newLabel();
        Label L_TinyCase16 = newLabel();
        Label L_TinyCase8 = newLabel();
        Label L_TinyCase4 = newLabel();
        Label L_TinyCase2 = newLabel();
        Label L_End = newLabel();

        j(vecSize == 64 ? L_TinyCase16 : L_TinyCase8, sub_c(i_ptr, vecSize / 4u));
        j(L_SmallIter, ucmp_lt(i_ptr, vecSize));

        // Align to a vecSize, but keep two LSB bits in case the alignment is unfixable.
        v_storeu_ivec(x86::ptr(dst), src);
        add(dst, dst, vecSize);
        lea(i_ptr, x86::ptr(dst, i_ptr, 2));
        and_(dst, dst, -int(vecSize) | 0x3);
        sub(i_ptr, i_ptr, dst);
        sar(i_ptr, i_ptr, 2);
        sub(i_ptr, i_ptr, vecSize);

        bind(L_LargeIter);
        x_storeu_fill(x86::ptr(dst), src, vecSize * 4);
        add(dst, dst, vecSize * 4);
        cc->sub(i_ptr, vecSize);
        cc->ja(L_LargeIter);

        add(i_ptr, i_ptr, vecSize);
        j(L_SmallCheck);

        bind(L_SmallIter);
        v_storeu_ivec(x86::ptr(dst), src);
        add(dst, dst, vecSize);
        bind(L_SmallCheck);
        cc->sub(i_ptr, vecSize / 4u);
        cc->ja(L_SmallIter);

        lea(dst, x86::ptr(dst, i_ptr, 2, vecSize));
        v_storeu_ivec(x86::ptr(dst, -int(vecSize)), src);
        j(L_End);

        if (vecSize == 64) {
          bind(L_TinyCase16);
          j(L_TinyCase8, bt_z(i, 3));
          v_storeu_i256(x86::ptr(dst), src);
          add(dst, dst, 32);
        }

        bind(L_TinyCase8);
        j(L_TinyCase4, bt_z(i, 2));
        v_storeu_i128(x86::ptr(dst), src);
        add(dst, dst, 16);

        bind(L_TinyCase4);
        j(L_TinyCase2, bt_z(i, 1));
        v_store_i64(x86::ptr(dst), src);
        add(dst, dst, 8);

        bind(L_TinyCase2);
        and_(i, i, 0x1);
        shl(i, i, 2);
        add(dst, dst, i_ptr);
        v_store_i32(x86::ptr(dst, -4), src);

        bind(L_End);
      }
    }
    else {
      Label L_Finalize = newLabel();
      Label L_End = newLabel();

      // Preparation / Alignment
      // -----------------------

      {
        j(L_Finalize, ucmp_lt(i, oneStepInItems * (vecSize / 4u)));

        Gp i_ptr = i.cloneAs(dst);
        if (sizeShift)
          cc->shl(i_ptr, sizeShift);
        add(i_ptr, i_ptr, dst);

        v_storeu_ivec(x86::ptr(dst), src);

        add(dst, dst, src.size());
        and_(dst, dst, -1 ^ int(alignPattern));

        if (sizeShift == 0) {
          j(L_End, sub_z(i_ptr, dst));
        }
        else {
          sub(i_ptr, i_ptr, dst);
          j(L_End, shr_z(i_ptr, sizeShift));
        }
      }

      // MainLoop
      // --------

      {
        Label L_MainIter = newLabel();
        Label L_MainSkip = newLabel();

        j(L_MainSkip, sub_c(i, mainStepInItems));

        bind(L_MainIter);
        add(dst, dst, mainLoopSize);
        x_storea_fill(x86::ptr(dst, -int(mainLoopSize)), src.xmm(), mainLoopSize);
        j(L_MainIter, sub_nc(i, mainStepInItems));

        bind(L_MainSkip);
        j(L_End, add_z(i, mainStepInItems));
      }

      // TailLoop / TailSequence
      // -----------------------

      if (mainLoopSize > vecSize * 2u) {
        Label L_TailIter = newLabel();
        Label L_TailSkip = newLabel();

        j(L_TailSkip, sub_c(i, tailStepInItems));

        bind(L_TailIter);
        add(dst, dst, vecSize);
        v_storea_ivec(x86::ptr(dst, -int(vecSize)), src);
        j(L_TailIter, sub_nc(i, tailStepInItems));

        bind(L_TailSkip);
        j(L_End, add_z(i, tailStepInItems));
      }
      else if (mainLoopSize >= vecSize * 2u) {
        j(L_Finalize, ucmp_lt(i, tailStepInItems));

        v_storea_ivec(x86::ptr(dst), src);
        add(dst, dst, vecSize);
        j(L_End, sub_z(i, tailStepInItems));
      }

      // Finalize
      // --------

      {
        Label L_Store1 = newLabel();

        bind(L_Finalize);
        j(L_Store1, ucmp_lt(i, 8u / itemSize));

        v_store_i64(x86::ptr(dst), src);
        add(dst, dst, 8);
        j(L_End, sub_z(i, 8u / itemSize));

        bind(L_Store1);
        v_store_i32(x86::ptr(dst), src);
        add(dst, dst, 4);
      }

      bind(L_End);
    }

    return;
  }

  // Granularity == 1 Byte
  // ---------------------

  if (granularityInBytes == 1) {
    BL_ASSERT(itemSize == 1u);

    Label L_Finalize = newLabel();
    Label L_End      = newLabel();

    // Preparation / Alignment
    // -----------------------

    {
      Label L_Small = newLabel();
      Label L_Large = newLabel();
      Gp srcGp = newGp32("srcGp");

      j(L_Large, ucmp_gt(i, 15));
      s_mov_i32(srcGp, src);

      bind(L_Small);
      store_8(ptr(dst), srcGp);
      inc(dst);
      j(L_Small, sub_nz(i, 1));
      j(L_End);

      bind(L_Large);
      Gp i_ptr = i.cloneAs(dst);
      add(i_ptr, i_ptr, dst);

      v_storeu_i128(x86::ptr(dst), src);
      add(dst, dst, 16);
      and_(dst, dst, -16);

      j(L_End, sub_z(i_ptr, dst));
    }

    // MainLoop
    // --------

    {
      Label L_MainIter = newLabel();
      Label L_MainSkip = newLabel();

      j(L_MainSkip, sub_c(i, mainLoopSize));

      bind(L_MainIter);
      add(dst, dst, mainLoopSize);
      for (k = 0; k < mainLoopSize; k += 16u)
        v_storea_i128(x86::ptr(dst, int(k) - int(mainLoopSize)), src);
      j(L_MainIter, sub_nc(i, mainLoopSize));

      bind(L_MainSkip);
      j(L_End, add_z(i, mainLoopSize));
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailIter = newLabel();
      Label L_TailSkip = newLabel();

      j(L_TailSkip, sub_c(i, 16));

      bind(L_TailIter);
      add(dst, dst, 16);
      v_storea_i128(x86::ptr(dst, -16), src);
      j(L_TailIter, sub_nc(i, 16));

      bind(L_TailSkip);
      j(L_End, add_z(i, 16));
    }
    else if (mainLoopSize >= 32) {
      j(L_Finalize, scmp_lt(i, 16));
      v_storea_i128(x86::ptr(dst, int(k)), src);
      add(dst, dst, 16);
      j(L_End, sub_z(i, 16));
    }

    // Finalize
    // --------

    {
      add(dst, dst, i.cloneAs(dst));
      v_storeu_i128(x86::ptr(dst, -16), src);
    }

    bind(L_End);
    return;
  }

  BL_NOT_REACHED();
}

// bl::Pipeline::PipeCompiler - PixelCopy
// ======================================

void PipeCompiler::x_inline_pixel_copy_loop(Gp& dst, Gp& src, Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity, FormatExt format) noexcept {
  BL_ASSERT(IntOps::isPowerOf2(itemSize));
  BL_ASSERT(itemSize <= 16u);

  uint32_t granularityInBytes = itemSize * itemGranularity;
  uint32_t mainStepInItems = mainLoopSize / itemSize;

  BL_ASSERT(IntOps::isPowerOf2(granularityInBytes));
  BL_ASSERT(mainStepInItems * itemSize == mainLoopSize);

  BL_ASSERT(mainLoopSize >= 16u);
  BL_ASSERT(mainLoopSize >= granularityInBytes);

  Vec t0 = cc->newXmm("t0");
  Vec fillMask;

  if (format == FormatExt::kXRGB32)
    fillMask = simdVecConst(&commonTable.i_FF000000FF000000, t0);

  // Granularity >= 16 Bytes
  // -----------------------

  if (granularityInBytes >= 16u) {
    Label L_End = newLabel();

    // MainLoop
    // --------

    {
      Label L_MainIter = newLabel();
      Label L_MainSkip = newLabel();
      int ptrOffset = -int(mainLoopSize);

      j(L_MainSkip, sub_c(i, mainStepInItems));

      bind(L_MainIter);
      add(dst, dst, mainLoopSize);
      add(src, src, mainLoopSize);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst, ptrOffset), false, x86::ptr(src, ptrOffset), false, mainLoopSize, fillMask);
      j(L_MainIter, sub_nc(i, mainStepInItems));

      bind(L_MainSkip);
      j(L_End, add_z(i, mainStepInItems));
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize * 2 > granularityInBytes) {
      Label L_TailIter = newLabel();

      bind(L_TailIter);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), false, x86::ptr(src), false, granularityInBytes, fillMask);
      add(dst, dst, granularityInBytes);
      add(src, src, granularityInBytes);
      j(L_TailIter, sub_nz(i, itemGranularity));
    }
    else if (mainLoopSize * 2 == granularityInBytes) {
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), false, x86::ptr(src), false, granularityInBytes, fillMask);
      add(dst, dst, granularityInBytes);
      add(src, src, granularityInBytes);
    }

    bind(L_End);
    return;
  }

  // Granularity == 4 Bytes
  // ----------------------

  if (granularityInBytes == 4u) {
    BL_ASSERT(itemSize <= 4u);
    uint32_t sizeShift = IntOps::ctz(itemSize);
    uint32_t alignPattern = (15u * itemSize) & 15u;

    uint32_t oneStepInItems = 4u >> sizeShift;
    uint32_t tailStepInItems = 16u >> sizeShift;

    Label L_Finalize = newLabel();
    Label L_End      = newLabel();

    // Preparation / Alignment
    // -----------------------

    {
      j(L_Finalize, ucmp_lt(i, oneStepInItems * 4u));

      Gp i_ptr = i.cloneAs(dst);
      v_loadu_i128(t0, x86::ptr(src));
      if (sizeShift)
        shl(i_ptr, i_ptr, sizeShift);

      add(i_ptr, i_ptr, dst);
      sub(src, src, dst);
      v_storeu_i128(x86::ptr(dst), t0);
      add(dst, dst, 16);
      and_(dst, dst, -1 ^ int(alignPattern));
      add(src, src, dst);

      if (sizeShift == 0) {
        j(L_End, sub_z(i_ptr, dst));
      }
      else {
        sub(i_ptr, i_ptr, dst);
        j(L_End, shr_z(i_ptr, sizeShift));
      }
    }

    // MainLoop
    // --------

    {
      Label L_MainIter = newLabel();
      Label L_MainSkip = newLabel();

      j(L_MainSkip, sub_c(i, mainStepInItems));

      bind(L_MainIter);
      add(dst, dst, mainLoopSize);
      add(src, src, mainLoopSize);

      int ptrOffset = - int(mainLoopSize);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst, ptrOffset), true, x86::ptr(src, ptrOffset), false, mainLoopSize, fillMask);
      j(L_MainIter, sub_nc(i, mainStepInItems));

      bind(L_MainSkip);
      j(L_End, add_z(i, mainStepInItems));
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailIter = newLabel();
      Label L_TailSkip = newLabel();

      j(L_TailSkip, sub_c(i, tailStepInItems));

      bind(L_TailIter);
      add(dst, dst, 16);
      add(src, src, 16);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst, -16), true, x86::ptr(src, -16), false, 16, fillMask);
      j(L_TailIter, sub_nc(i, tailStepInItems));

      bind(L_TailSkip);
      j(L_End, add_z(i, tailStepInItems));
    }
    else if (mainLoopSize >= 32) {
      j(L_Finalize, ucmp_lt(i, tailStepInItems));

      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), true, x86::ptr(src), false, 16, fillMask);
      add(dst, dst, 16);
      add(src, src, 16);
      j(L_End, sub_z(i, tailStepInItems));
    }

    // Finalize
    // --------

    {
      Label L_Store1 = newLabel();

      bind(L_Finalize);
      j(L_Store1, ucmp_lt(i, 8u / itemSize));

      v_load_i64(t0, x86::ptr(src));
      add(src, src, 8);
      v_store_i64(x86::ptr(dst), t0);
      add(dst, dst, 8);
      j(L_End, sub_z(i, 8u / itemSize));

      bind(L_Store1);
      v_load_i32(t0, x86::ptr(src));
      add(src, src, 4);
      v_store_i32(x86::ptr(dst), t0);
      add(dst, dst, 4);
    }

    bind(L_End);
    return;
  }

  // Granularity == 1 Byte
  // ---------------------

  if (granularityInBytes == 1) {
    BL_ASSERT(itemSize == 1u);

    Label L_Finalize = newLabel();
    Label L_End      = newLabel();

    // Preparation / Alignment
    // -----------------------

    {
      Label L_Small = newLabel();
      Label L_Large = newLabel();

      Gp i_ptr = i.cloneAs(dst);
      Gp byte_val = newGp32("@byte_val");

      j(L_Large, ucmp_gt(i, 15));

      bind(L_Small);
      load_u8(byte_val, ptr(src));
      inc(src);
      store_8(ptr(dst), byte_val);
      inc(dst);
      j(L_Small, sub_nz(i, 1));
      j(L_End);

      bind(L_Large);
      v_loadu_i128(t0, x86::ptr(src));
      add(i_ptr, i_ptr, dst);
      sub(src, src, dst);

      v_storeu_i128(x86::ptr(dst), t0);
      add(dst, dst, 16);
      and_(dst, dst, -16);

      add(src, src, dst);
      j(L_End, sub_z(i_ptr, dst));
    }

    // MainLoop
    // --------

    {
      Label L_MainIter = newLabel();
      Label L_MainSkip = newLabel();

      j(L_MainSkip, sub_c(i, mainLoopSize));

      bind(L_MainIter);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), true, x86::ptr(src), false, mainLoopSize, fillMask);
      add(dst, dst, mainLoopSize);
      add(src, src, mainLoopSize);
      j(L_MainIter, sub_nc(i, mainLoopSize));

      bind(L_MainSkip);
      j(L_End, add_z(i, mainLoopSize));
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailIter = newLabel();
      Label L_TailSkip = newLabel();

      j(L_TailSkip, sub_c(i, 16));

      bind(L_TailIter);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), true, x86::ptr(src), false, 16, fillMask);
      add(dst, dst, 16);
      add(src, src, 16);
      j(L_TailIter, sub_nc(i, 16));

      bind(L_TailSkip);
      j(L_End, add_z(i, 16));
    }
    else if (mainLoopSize >= 32) {
      j(L_Finalize, ucmp_lt(i, 16));

      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), true, x86::ptr(src), false, 16, fillMask);
      add(dst, dst, 16);
      add(src, src, 16);
      j(L_End, sub_z(i, 16));
    }

    // Finalize
    // --------

    {
      add(dst, dst, i.cloneAs(dst));
      add(src, src, i.cloneAs(src));
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst, -16), false, x86::ptr(src, -16), false, 16, fillMask);
    }

    bind(L_End);
    return;
  }
}

void PipeCompiler::_x_inline_memcpy_sequence_xmm(
  const x86::Mem& dPtr, bool dstAligned,
  const x86::Mem& sPtr, bool srcAligned, uint32_t numBytes, const Vec& fillMask) noexcept {

  x86::Mem dAdj(dPtr);
  x86::Mem sAdj(sPtr);
  VecArray t;

  uint32_t fetchInst = hasAVX() ? x86::Inst::kIdVmovdqa : x86::Inst::kIdMovaps;
  uint32_t storeInst = hasAVX() ? x86::Inst::kIdVmovdqa : x86::Inst::kIdMovaps;

  if (!srcAligned) fetchInst = hasAVX512() ? x86::Inst::kIdVmovdqu :
                               hasAVX()    ? x86::Inst::kIdVlddqu  :
                               hasSSE3()   ? x86::Inst::kIdLddqu   : x86::Inst::kIdMovups;
  if (!dstAligned) storeInst = hasAVX()    ? x86::Inst::kIdVmovdqu : x86::Inst::kIdMovups;

  uint32_t n = numBytes / 16;
  uint32_t limit = 2;
  newXmmArray(t, blMin(n, limit), "t");

  do {
    uint32_t a, b = blMin<uint32_t>(n, limit);

    if (hasAVX() && fillMask.isValid()) {
      // Shortest code for this use case. AVX allows to read from unaligned
      // memory, so if we use VEC instructions we are generally safe here.
      for (a = 0; a < b; a++) {
        v_or_i32(t[a], fillMask, sAdj);
        sAdj.addOffsetLo32(16);
      }

      for (a = 0; a < b; a++) {
        cc->emit(storeInst, dAdj, t[a]);
        dAdj.addOffsetLo32(16);
      }
    }
    else {
      for (a = 0; a < b; a++) {
        cc->emit(fetchInst, t[a], sAdj);
        sAdj.addOffsetLo32(16);
      }

      for (a = 0; a < b; a++)
        if (fillMask.isValid())
          v_or_i32(t[a], t[a], fillMask);

      for (a = 0; a < b; a++) {
        cc->emit(storeInst, dAdj, t[a]);
        dAdj.addOffsetLo32(16);
      }
    }

    n -= b;
  } while (n > 0);
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif
