// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchgradientpart_p.h"
#include "../../pipeline/jit/fetchpixelptrpart_p.h"
#include "../../pipeline/jit/fetchsolidpart_p.h"
#include "../../pipeline/jit/fetchpatternpart_p.h"
#include "../../pipeline/jit/fillpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../support/intops_p.h"

namespace BLPipeline {
namespace JIT {

// BLPipeline::PipeCompiler - Constants
// ====================================

static constexpr OperandSignature signatureOfXmmYmmZmm[] = {
  OperandSignature{x86::Xmm::kSignature},
  OperandSignature{x86::Ymm::kSignature},
  OperandSignature{x86::Zmm::kSignature}
};

// BLPipeline::PipeCompiler - Construction & Destruction
// =====================================================

PipeCompiler::PipeCompiler(x86::Compiler* cc, const CpuFeatures& features, PipeOptFlags optFlags) noexcept
  : cc(cc),
    ct(blCommonTable),
    _features(features),
    _optFlags(optFlags),
    _commonTableOff(512 + 128) {}

PipeCompiler::~PipeCompiler() noexcept {}

// BLPipeline::PipeCompiler - CPU Features and Optimization Options
// ================================================================

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

// BLPipeline::PipeCompiler - BeginFunction & EndFunction
// ======================================================

void PipeCompiler::beginFunction() noexcept {
  // Function prototype and arguments.
  _funcNode = cc->addFunc(asmjit::FuncSignatureT<void, ContextData*, const void*, const void*>(asmjit::CallConvId::kCDecl));
  _funcInit = cc->cursor();
  _funcEnd = _funcNode->endNode()->prev();

  if (hasAVX()) {
    _funcNode->frame().setAvxEnabled();
    _funcNode->frame().setAvxCleanup();
  }

  if (hasAVX512()) {
    _funcNode->frame().setAvx512Enabled();
  }

  _ctxData = cc->newIntPtr("ctxData");
  _fillData = cc->newIntPtr("fillData");
  _fetchData = cc->newIntPtr("fetchData");

  _funcNode->setArg(0, _ctxData);
  _funcNode->setArg(1, _fillData);
  _funcNode->setArg(2, _fetchData);
}

void PipeCompiler::endFunction() noexcept {
  // Finalize the pipeline function.
  cc->endFunc();
}

// BLPipeline::PipeCompiler - Parts
// ================================

FillPart* PipeCompiler::newFillPart(FillType fillType, FetchPart* dstPart, CompOpPart* compOpPart) noexcept {
  if (fillType == FillType::kBoxA)
    return newPartT<FillBoxAPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  if (fillType == FillType::kMask)
    return newPartT<FillMaskPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  if (fillType == FillType::kAnalytic)
    return newPartT<FillAnalyticPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  return nullptr;
}

FetchPart* PipeCompiler::newFetchPart(FetchType fetchType, BLInternalFormat format) noexcept {
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

CompOpPart* PipeCompiler::newCompOpPart(uint32_t compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept {
  return newPartT<CompOpPart>(compOp, dstPart, srcPart);
}

// BLPipeline::PipeCompiler - Init
// ===============================

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
  if (hasAVX512() && cc->is64Bit())
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

// BLPipeline::PipeCompiler - Constants
// ====================================

void PipeCompiler::_initCommonTablePtr() noexcept {
  const void* global = &blCommonTable;

  if (!_commonTablePtr.isValid()) {
    asmjit::BaseNode* prevNode = cc->setCursor(_funcInit);
    _commonTablePtr = cc->newIntPtr("commonTablePtr");

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
  x86::Gp tmp;
  x86::KReg kReg;

  if (slot < kMaxKRegConstCount) {
    prevNode = cc->setCursor(_funcInit);
  }

  if (value & 0xFFFFFFFF00000000u) {
    tmp = cc->newUInt64("kTmp");
    kReg = cc->newKq("k0x%016llX", (unsigned long long)value);
    cc->mov(tmp, value);
    cc->kmovq(kReg, tmp);
  }
  else {
    tmp = cc->newUInt32("kTmp");
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
      return x86::Vec(signatureOfXmmYmmZmm[size_t(constWidth)], _vecConsts[i].vRegId);

  // We don't use memory constants when compiling for AVX-512, because we don't store 64-byte constants and AVX-512
  // has enough registers to hold all the constants that we need. However, in SSE/AVX2 case, we don't want so many
  // constants in registers as that could limit registers that we need during fetching and composition.
  if (!hasAVX512()) {
    bool useVReg = c == &blCommonTable.i_0000000000000000 || // Required if the CPU doesn't have SSE4.1.
                   c == &blCommonTable.i_0080008000800080 || // Required by `div255()` and friends.
                   c == &blCommonTable.i_0101010101010101 || // Required by `div255()` and friends.
                   c == &blCommonTable.i_FF000000FF000000 ;  // Required by fetching XRGB32 pixels as PRGB32 pixels.

    if (!useVReg)
      return simdMemConst(c, bcstWidth, constWidth);
  }

  return x86::Vec(signatureOfXmmYmmZmm[size_t(constWidth)], _newVecConst(c, bcstWidth == Bcst::kNA_Unique).id());
}

Operand PipeCompiler::simdConst(const void* c, Bcst bcstWidth, const x86::Vec& similarTo) noexcept {
  SimdWidth constWidth = SimdWidth(uint32_t(similarTo.type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdConst(c, bcstWidth, constWidth);
}

Operand PipeCompiler::simdConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept {
  BL_ASSERT(!similarTo.empty());

  SimdWidth constWidth = SimdWidth(uint32_t(similarTo[0].type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdConst(c, bcstWidth, constWidth);
}

x86::Vec PipeCompiler::simdVecConst(const void* c, SimdWidth constWidth) noexcept {
  size_t constCount = _vecConsts.size();

  for (size_t i = 0; i < constCount; i++)
    if (_vecConsts[i].ptr == c)
      return x86::Vec(signatureOfXmmYmmZmm[size_t(constWidth)], _vecConsts[i].vRegId);

  return x86::Vec(signatureOfXmmYmmZmm[size_t(constWidth)], _newVecConst(c, false).id());
}

x86::Vec PipeCompiler::simdVecConst(const void* c, const x86::Vec& similarTo) noexcept {
  SimdWidth constWidth = SimdWidth(uint32_t(similarTo.type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdVecConst(c, constWidth);
}

x86::Vec PipeCompiler::simdVecConst(const void* c, const VecArray& similarTo) noexcept {
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

x86::Mem PipeCompiler::simdMemConst(const void* c, Bcst bcstWidth, const x86::Vec& similarTo) noexcept {
  SimdWidth constWidth = SimdWidth(uint32_t(similarTo.type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdMemConst(c, bcstWidth, constWidth);
}

x86::Mem PipeCompiler::simdMemConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept {
  BL_ASSERT(!similarTo.empty());

  SimdWidth constWidth = SimdWidth(uint32_t(similarTo[0].type()) - uint32_t(asmjit::RegType::kX86_Xmm));
  return simdMemConst(c, bcstWidth, constWidth);
}

x86::Mem PipeCompiler::_getMemConst(const void* c) noexcept {
  // Make sure we are addressing a constant from the `blCommonTable` constant pool.
  const void* global = &blCommonTable;
  BL_ASSERT((uintptr_t)c >= (uintptr_t)global &&
            (uintptr_t)c <  (uintptr_t)global + sizeof(BLCommonTable));

  if (cc->is32Bit()) {
    // 32-bit mode - These constants will never move in memory so the absolute addressing is a win/win as we can save
    // one GP register that can be used for something else.
    return x86::ptr((uint64_t)c);
  }
  else {
    // 64-bit mode - One GP register is sacrificed to hold the pointer to the `blCommonTable`. This is probably the
    // safest approach as relying on absolute addressing or anything else could lead to problems or performance issues.
    _initCommonTablePtr();

    int32_t disp = int32_t((intptr_t)c - (intptr_t)global);
    return x86::ptr(_commonTablePtr, disp - _commonTableOff);
  }
}

x86::Vec PipeCompiler::_newVecConst(const void* c, bool isUniqueConst) noexcept {
  x86::Vec vReg;
  const char* specialConstName = nullptr;

  if (c == blCommonTable.pshufb_dither_rgba64_lo.data)
    specialConstName = "pshufb_dither_rgba64_lo";
  else if (c == blCommonTable.pshufb_dither_rgba64_hi.data)
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

  if (c == &blCommonTable.i_0000000000000000) {
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

// BLPipeline::PipeCompiler - Stack
// ================================

x86::Mem PipeCompiler::tmpStack(uint32_t size) noexcept {
  BL_ASSERT(BLIntOps::isPowerOf2(size));
  BL_ASSERT(size <= 32);

  // Only used by asserts.
  blUnused(size);

  if (!_tmpStack.baseId())
    _tmpStack = cc->newStack(32, 16, "tmpStack");
  return _tmpStack;
}

// BLPipeline::PipeCompiler - Utilities
// ====================================

void PipeCompiler::embedJumpTable(const Label* jumpTable, size_t jumpTableSize, const Label& jumpTableBase, uint32_t entrySize) noexcept {
  static const uint8_t zeros[8] {};

  for (size_t i = 0; i < jumpTableSize; i++) {
    if (jumpTable[i].isValid())
      cc->embedLabelDelta(jumpTable[i], jumpTableBase, entrySize);
    else
      cc->embed(zeros, entrySize);
  }
}

// BLPipeline::PipeCompiler - Emit
// ===============================

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
          x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");
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
          x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");
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

        x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");

        v_mov(tmp, src);
        v_sra_i32(tmp, tmp, 31);
        v_xor_i32(dst, src, tmp);
        v_sub_i32(dst, dst, tmp);
        return;
      }

      case kIntrin2Vabsi64: {
        x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");

        v_duph_i32(tmp, src);
        v_sra_i32(tmp, tmp, 31);
        v_xor_i32(dst, src, tmp);
        v_sub_i32(dst, dst, tmp);
        return;
      }

      case kIntrin2Vinv255u16: {
        Operand u16_255 = simdConst(&ct.i_00FF00FF00FF00FF, Bcst::k32, dst.as<x86::Vec>());

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
        Operand u16_0100 = simdConst(&ct.i_0100010001000100, Bcst::kNA, dst.as<x86::Vec>());

        if (!isSameReg(dst, src)) {
          v_mov(dst, u16_0100);
          v_sub_i16(dst, dst, src);
        }
        else if (hasSSSE3()) {
          v_sub_i16(dst, dst, u16_0100);
          v_abs_i16(dst, dst);
        }
        else {
          v_xor_i32(dst, dst, simdConst(&ct.i_FFFFFFFFFFFFFFFF, Bcst::kNA, dst.as<x86::Vec>()));
          v_add_i16(dst, dst, u16_0100);
        }
        return;
      }

      case kIntrin2Vinv255u32: {
        Operand u32_255 = simdConst(&ct.i_000000FF000000FF, Bcst::kNA, dst.as<x86::Vec>());

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
              x86::Gp tmp = cc->newUInt32("tmp");
              cc->imul(tmp, src.as<x86::Gp>().r32(), 0x01010101u);
              s_mov_i32(dst.as<x86::Vec>(), tmp);
              v_swizzle_u32(dst, dst, x86::shuffleImm(0, 0, 0, 0));
              return;
            }

            if (!hasAVX512()) {
              s_mov_i32(dst.as<x86::Vec>().xmm(), src.as<x86::Gp>().r32());
              x = dst;
            }
            else {
              x = src.as<x86::Gp>().r32();
            }
          }

          if (hasAVX2()) {
            if (x86::Reg::isVec(x))
              cc->emit(x86::Inst::kIdVpbroadcastb, dst, x.as<x86::Vec>().xmm());
            else
              cc->emit(x86::Inst::kIdVpbroadcastb, dst, x);
          }
          else if (hasSSSE3()) {
            v_shuffle_i8(dst, x, simdConst(&ct.i_0000000000000000, Bcst::kNA, dst.as<x86::Vec>()));
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
            x86::Gp tmp = cc->newUInt32("tmp");
            cc->movzx(tmp, m);
            cc->imul(tmp, tmp, 0x01010101u);
            s_mov_i32(dst.as<x86::Vec>(), tmp);
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
              s_mov_i32(dst.as<x86::Vec>().xmm(), src.as<x86::Gp>().r32());
              x = dst;
            }
            else {
              x = src.as<x86::Gp>().r32();
            }
          }

          if (hasAVX2()) {
            if (x86::Reg::isVec(x))
              cc->emit(x86::Inst::kIdVpbroadcastw, dst, x.as<x86::Vec>().xmm());
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
            s_mov_i32(dst.as<x86::Vec>().xmm(), src.as<x86::Gp>().r32());
            x = dst;
          }

          if (x.as<x86::Reg>().isVec())
            x = x.as<x86::Vec>().xmm();

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
            v_load_i32(dst.as<x86::Vec>(), m);
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
            s_mov_i64(dst.as<x86::Vec>().xmm(), src.as<x86::Gp>().r64());
            x = dst;
          }

          if (x.as<x86::Reg>().isVec())
            x = x.as<x86::Vec>().xmm();

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
            v_load_i64(dst.as<x86::Vec>(), m);
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

        if (dst.as<x86::Vec>().isXmm()) {
          if (src.isMem())
            v_loadu_i128(dst, src.as<x86::Mem>());
          else
            v_mov(dst, src);
        }
        else if (!hasAVX512()) {
          if (src.isMem()) {
            cc->vbroadcastf128(dst.as<x86::Vec>(), src.as<x86::Mem>());
          }
          else {
            x86::Vec srcAsXmm = src.as<x86::Vec>().xmm();
            cc->vinsertf128(dst.as<x86::Vec>(), srcAsXmm.ymm(), srcAsXmm, 1u);
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
          else if (dst.as<x86::Vec>().isYmm()) {
            cc->emit(insrTable[tableIndex], dst, src.as<x86::Vec>().cloneAs(dst.as<x86::Vec>()), src.as<x86::Vec>().xmm(), 1u);
          }
          else {
            Operand srcAsDst = src.as<x86::Vec>().cloneAs(dst.as<x86::Vec>());
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
          x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");

          v_swizzle_u32(tmp, dst, x86::shuffleImm(2, 3, 0, 1));
          v_mulx_ll_u32_(dst, dst, src2);
          v_mulx_ll_u32_(tmp, tmp, src2);
          v_sll_i64(tmp, tmp, 32);
          v_add_i64(dst, dst, tmp);
        }
        else if (isSameReg(dst, src2)) {
          x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");

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
            cc->vpmovm2b(dst.as<x86::Vec>(), k);
            break;

          case x86::Inst::kIdVpcmpw:
          case x86::Inst::kIdVpcmpuw:
          case x86::Inst::kIdVpcmpeqw:
          case x86::Inst::kIdVpcmpgtw:
          case x86::Inst::kIdVcmpph:
            cc->vpmovm2w(dst.as<x86::Vec>(), k);
            break;

          case x86::Inst::kIdVpcmpd:
          case x86::Inst::kIdVpcmpud:
          case x86::Inst::kIdVpcmpeqd:
          case x86::Inst::kIdVpcmpgtd:
          case x86::Inst::kIdVcmpps:
            cc->vpmovm2d(dst.as<x86::Vec>(), k);
            break;

          case x86::Inst::kIdVpcmpq:
          case x86::Inst::kIdVpcmpuq:
          case x86::Inst::kIdVpcmpeqq:
          case x86::Inst::kIdVpcmpgtq:
          case x86::Inst::kIdVcmppd:
            cc->vpmovm2q(dst.as<x86::Vec>(), k);
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

        x86::Vec tmp = cc->newXmm("@tmp");

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

// BLPipeline::PipeCompiler - Predicate Helpers
// ============================================

void PipeCompiler::x_make_predicate_v32(const x86::Vec& vmask, const x86::Gp& count) noexcept {
  x86::Mem maskPtr = _getMemConst(blCommonTable.loadstore16_lo8_msk8());
  maskPtr._setIndex(cc->_gpSignature.regType(), count.id());
  maskPtr.setShift(3);
  cc->vpmovsxbd(vmask, maskPtr);
}

void PipeCompiler::x_ensure_predicate_8(PixelPredicate& predicate, uint32_t maxWidth) noexcept {
  BL_ASSERT(!predicate.empty());

  blUnused(maxWidth);

  if (hasAVX512()) {
    if (!predicate.k.isValid()) {
      x86::Mem mem = _getMemConst(blCommonTable.k_msk16_data);
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
      x86::Mem mem = _getMemConst(blCommonTable.k_msk16_data);
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

// BLPipeline::PipeCompiler - Fetch Helpers
// ========================================

void PipeCompiler::x_fetch_mask_a8_advance(VecArray& vm, PixelCount n, PixelType pixelType, const x86::Gp& mPtr, const x86::Vec& globalAlpha) noexcept {
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

      i_add(mPtr, mPtr, n.value());

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
            i_add(mPtr, mPtr, n.value());
            v_mov_u8_u16(vm[0], vm[0]);
          }
          else {
            v_load_i8(vm[0], m);
            i_add(mPtr, mPtr, n.value());
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
            i_add(mPtr, mPtr, n.value());
            v_shuffle_i8(vm[0], vm[0], simdConst(&ct.pshufb_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));
          }
          else {
            v_load_i8(vm[0], m);
            i_add(mPtr, mPtr, n.value());
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
            i_add(mPtr, mPtr, n.value());
            v_shuffle_i8(vm[0], vm[0], simdConst(&ct.pshufb_xxxxxxx1xxxxxxx0_to_z1z1z1z1z0z0z0z0, Bcst::kNA, vm[0]));

            if (globalAlpha.isValid()) {
              v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
              v_div255_u16(vm[0]);
            }
          }
          else {
            v_load_i32(vm[0], m);
            i_add(mPtr, mPtr, n.value());
            v_mov_u8_u16(vm[0], vm[0]);

            if (globalAlpha.isValid()) {
              v_mul_i16(vm[0], vm[0], globalAlpha.cloneAs(vm[0]));
              v_div255_u16(vm[0]);
            }

            if (simdWidth == SimdWidth::k128) {
              v_interleave_lo_u16(vm[0], vm[0], vm[0]);                 // vm[0] = [M3 M3 M2 M2 M1 M1 M0 M0]
              v_swizzle_u32(vm[1], vm[0], x86::shuffleImm(3, 3, 2, 2)); // vm[1] = [M3 M3 M3 M3 M2 M2 M2 M2]
              v_swizzle_u32(vm[0], vm[0], x86::shuffleImm(1, 1, 0, 0)); // vm[0] = [M1 M1 M1 M1 M0 M0 M0 M0]
            }
          }
          break;
        }

        default: {
          if (simdWidth >= SimdWidth::k256) {
            for (uint32_t i = 0; i < regCount; i++) {
              v_mov_u8_u64_(vm[i], m);
              m.addOffsetLo32(vm[i].size() / 8u);
            }

            i_add(mPtr, mPtr, n.value());

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

            i_add(mPtr, mPtr, n.value());

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

void PipeCompiler::x_fetch_pixel(Pixel& p, PixelCount n, PixelFlags flags, BLInternalFormat format, const x86::Mem& src_, Alignment alignment) noexcept {
  PixelPredicate noPredicate;
  x_fetch_pixel(p, n, flags, format, src_, alignment, noPredicate);
}

void PipeCompiler::x_fetch_pixel(Pixel& p, PixelCount n, PixelFlags flags, BLInternalFormat format, const x86::Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept  {
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

void PipeCompiler::_x_fetch_pixel_a8(Pixel& p, PixelCount n, PixelFlags flags, BLInternalFormat format, const x86::Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept {
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
    case BLInternalFormat::kPRGB32: {
      SimdWidth p32Width = simdWidthOf(DataWidth::k32, n);
      uint32_t p32RegCount = SimdWidthUtils::regCountOf(p32Width, DataWidth::k32, n);

      x86::Vec predicatedPixel;
      if (!predicate.empty()) {
        // TODO: [JIT] Do we want to support masked loading of more that 1 register?
        BL_ASSERT(n.value() > 1);
        BL_ASSERT(regCountOf(DataWidth::k32, n) == 1);

        predicatedPixel = newVec(p32Width, p.name(), "pred");
        x_ensure_predicate_32(predicate, n.value());
        v_load_predicated_v32(predicatedPixel, predicate, src);
      }

      auto fetch4Shifted = [](PipeCompiler* pc, const x86::Vec& dst, const x86::Mem& src, Alignment alignment, const x86::Vec& predicatedPixel) noexcept {
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
          p.sa = cc->newUInt32("a");
          src.addOffset(3);
          i_load_u8(p.sa, src);
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

            for (const x86::Vec& v : p32) {
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

    case BLInternalFormat::kXRGB32: {
      BL_ASSERT(predicate.empty());

      switch (n.value()) {
        case 1: {
          p.sa = cc->newUInt32("a");
          cc->mov(p.sa, 255);
          break;
        }

        default:
          BL_NOT_REACHED();
      }

      break;
    }

    case BLInternalFormat::kA8: {
      x86::Vec predicatedPixel;
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
          p.sa = cc->newUInt32("a");
          i_load_u8(p.sa, src);

          break;
        }

        case 4: {
          x86::Vec a;

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
            x86::Vec a = predicatedPixel;

            if (blTestFlag(flags, PixelFlags::kPA)) {
              p.pa.init(a);
            }
            else {
              v_mov_u8_u16_(a, a);
              p.ua.init(a);
            }
          }
          else {
            x86::Vec a = cc->newXmm("a");
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

void PipeCompiler::_x_fetch_pixel_rgba32(Pixel& p, PixelCount n, PixelFlags flags, BLInternalFormat format, const x86::Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept  {
  BL_ASSERT(p.isRGBA32());

  x86::Mem src(src_);
  p.setCount(n);

  switch (format) {
    // RGBA32 <- PRGB32 | XRGB32.
    case BLInternalFormat::kPRGB32:
    case BLInternalFormat::kXRGB32: {
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

      if (format == BLInternalFormat::kXRGB32)
        x_fill_pixel_alpha(p);

      break;
    }

    // RGBA32 <- A8.
    case BLInternalFormat::kA8: {
      BL_ASSERT(predicate.empty());

      switch (n.value()) {
        case 1: {
          if (blTestFlag(flags, PixelFlags::kPC)) {
            newXmmArray(p.pc, 1, p.name(), "pc");

            if (hasAVX2()) {
              cc->vpbroadcastb(p.pc[0].xmm(), src);
            }
            else {
              x86::Gp tmp = cc->newUInt32("tmp");
              i_load_u8(tmp, src);
              i_mul(tmp, tmp, 0x01010101u);
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
              x86::Gp tmp = cc->newUInt32("tmp");
              i_load_u8(tmp, src);
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
              x86::Gp tmp = cc->newUInt32("tmp");
              i_load_u16(tmp, src);
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
          v_shuffle_i8(ux[i], px[i / 2u], simdConst(&blCommonTable.pshufb_76543210xxxxxxxx_to_z7z6z5z4z3z2z1z0, Bcst::kNA, ux[i]));
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

void PipeCompiler::x_fetch_unpacked_a8_2x(const x86::Xmm& dst, BLInternalFormat format, const x86::Mem& src1, const x86::Mem& src0) noexcept {
  x86::Mem m0 = src0;
  x86::Mem m1 = src1;

  m0.setSize(1);
  m1.setSize(1);

  if (format == BLInternalFormat::kPRGB32) {
    m0.addOffset(3);
    m1.addOffset(3);
  }

  if (hasSSE4_1()) {
    v_zero_i(dst);
    v_insert_u8_(dst, dst, m0, 0);
    v_insert_u8_(dst, dst, m1, 2);
  }
  else {
    x86::Gp aGp = cc->newUInt32("aGp");
    cc->movzx(aGp, m1);
    cc->shl(aGp, 16);
    cc->mov(aGp.r8(), m0);
    s_mov_i32(dst, aGp);
  }
}

void PipeCompiler::x_assign_unpacked_alpha_values(Pixel& p, PixelFlags flags, x86::Xmm& vec) noexcept {
  blUnused(flags);

  BL_ASSERT(p.type() != PixelType::kNone);
  BL_ASSERT(p.count() != 0);

  x86::Xmm v0 = vec;

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
        x86::Xmm v1 = cc->newXmm();
        x86::Xmm v2 = cc->newXmm();
        x86::Xmm v3 = cc->newXmm();

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

        x86::Gp sa = cc->newUInt32("sa");
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

void PipeCompiler::x_store_pixel_advance(const x86::Gp& dPtr, Pixel& p, PixelCount n, uint32_t bpp, Alignment alignment, PixelPredicate& predicate) noexcept {
  x86::Mem dMem = x86::ptr(dPtr);

  switch (bpp) {
    case 1: {
      if (!predicate.empty()) {
        // Predicated pixel count must be greater than 1!
        BL_ASSERT(n != 1);

        x_satisfy_pixel(p, PixelFlags::kPA | PixelFlags::kImmutable);

        x_ensure_predicate_8(predicate, n.value());
        v_store_predicated_v8(dMem, predicate, p.pa[0]);
        i_add(dPtr, dPtr, predicate.count.cloneAs(dPtr));
      }
      else {
        if (n == 1) {
          x_satisfy_pixel(p, PixelFlags::kSA | PixelFlags::kImmutable);
          i_store_u8(dMem, p.sa);
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

        i_add(dPtr, dPtr, n.value());
      }

      break;
    }

    case 4: {
      if (!predicate.empty()) {
        x_satisfy_pixel(p, PixelFlags::kPC | PixelFlags::kImmutable);

        if (hasOptFlag(PipeOptFlags::kFastStoreWithMask)) {
          x_ensure_predicate_32(predicate, n.value());
          v_store_predicated_v32(dMem, predicate, p.pc[0]);
          i_add_scaled(dPtr, predicate.count.cloneAs(dPtr), bpp);
        }
        else {
          Label L_StoreSkip1 = cc->newLabel();

          const x86::Gp& count = predicate.count;
          const x86::Vec& pc0 = p.pc[0];

          if (n > 8) {
            Label L_StoreSkip8 = cc->newLabel();
            x86::Ymm pc0YmmHigh = cc->newYmm("pc0.ymmHigh");

            v_extract_i256(pc0YmmHigh, pc0.zmm(), 1);
            i_jmp_if_bit_zero(count, 3, L_StoreSkip8);
            v_storeu_i256(dMem, pc0.ymm());
            v_mov(pc0.ymm(), pc0YmmHigh);
            i_add(dPtr, dPtr, 8u * 4u);
            bind(L_StoreSkip8);
          }

          if (n > 4) {
            Label L_StoreSkip4 = cc->newLabel();
            x86::Xmm pc0XmmHigh = cc->newXmm("pc0.xmmHigh");

            v_extract_i128(pc0XmmHigh, pc0.ymm(), 1);
            i_jmp_if_bit_zero(count, 2, L_StoreSkip4);
            v_storeu_i128(dMem, pc0.xmm());
            v_mov(pc0.xmm(), pc0XmmHigh);
            i_add(dPtr, dPtr, 4u * 4u);
            bind(L_StoreSkip4);
          }

          if (n > 2) {
            Label L_StoreSkip2 = cc->newLabel();

            i_jmp_if_bit_zero(count, 1, L_StoreSkip2);
            v_store_i64(dMem, pc0.xmm());
            v_srlb_u128(pc0.xmm(), pc0.xmm(), 8);
            i_add(dPtr, dPtr, 2u * 4u);
            bind(L_StoreSkip2);
          }

          i_jmp_if_bit_zero(count, 0, L_StoreSkip1);
          v_store_i32(dMem, pc0.xmm());
          i_add(dPtr, dPtr, 1u * 4u);
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

// BLPipeline::PipeCompiler - PixelFill
// ====================================

void PipeCompiler::x_inline_pixel_fill_loop(x86::Gp& dst, x86::Vec& src, x86::Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity) noexcept {
  BL_ASSERT(BLIntOps::isPowerOf2(itemSize));
  BL_ASSERT(itemSize <= 16u);

  uint32_t granularityInBytes = itemSize * itemGranularity;
  uint32_t mainStepInItems = mainLoopSize / itemSize;

  BL_ASSERT(BLIntOps::isPowerOf2(granularityInBytes));
  BL_ASSERT(mainStepInItems * itemSize == mainLoopSize);

  BL_ASSERT(mainLoopSize >= 16u);
  BL_ASSERT(mainLoopSize >= granularityInBytes);

  uint32_t j;
  uint32_t vecSize = src.size();

  // Granularity >= 16 Bytes
  // -----------------------

  if (granularityInBytes >= 16u) {
    Label L_End = cc->newLabel();

    // MainLoop
    // --------

    {
      Label L_MainIter = cc->newLabel();
      Label L_MainSkip = cc->newLabel();

      cc->sub(i, mainStepInItems);
      cc->jc(L_MainSkip);

      cc->bind(L_MainIter);
      cc->add(dst, mainLoopSize);
      cc->sub(i, mainStepInItems);

      x_storeu_fill(x86::ptr(dst, -int(mainLoopSize)), src, mainLoopSize);
      cc->jnc(L_MainIter);

      cc->bind(L_MainSkip);
      cc->add(i, mainStepInItems);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize * 2 > granularityInBytes) {
      Label L_TailIter = cc->newLabel();

      cc->bind(L_TailIter);
      x_storeu_fill(x86::ptr(dst), src, granularityInBytes);
      cc->add(dst, granularityInBytes);
      cc->sub(i, itemGranularity);
      cc->jnz(L_TailIter);
    }
    else if (mainLoopSize * 2 == granularityInBytes) {
      x_storeu_fill(x86::ptr(dst), src, granularityInBytes);
      cc->add(dst, granularityInBytes);
    }

    cc->bind(L_End);
    return;
  }

  // Granularity == 4 Bytes
  // ----------------------

  if (granularityInBytes == 4u) {
    BL_ASSERT(itemSize <= 4u);

    uint32_t sizeShift = BLIntOps::ctz(itemSize);
    uint32_t alignPattern = ((vecSize - 1u) * itemSize) & (vecSize - 1u);

    uint32_t oneStepInItems = 4u >> sizeShift;
    uint32_t tailStepInItems = 16u >> sizeShift;

    if (vecSize >= 32u) {
      // Make `i` contain the number of 32-bit units to fill.
      x86::Gp iNative = i.cloneAs(dst);
      if (itemSize != 4u)
        cc->shr(i, 2u - sizeShift);

      if (hasMaskedAccessOf(4) && hasOptFlag(PipeOptFlags::kFastStoreWithMask)) {
        Label L_MainIter = cc->newLabel();
        Label L_MainSkip = cc->newLabel();
        Label L_TailIter = cc->newLabel();
        Label L_TailSkip = cc->newLabel();
        Label L_End = cc->newLabel();

        cc->sub(iNative, vecSize);
        cc->jc(L_MainSkip);

        cc->bind(L_MainIter);
        x_storeu_fill(x86::ptr(dst), src, vecSize * 4u);
        cc->add(dst, vecSize * 4u);
        cc->sub(iNative, vecSize);
        cc->jnc(L_MainIter);

        cc->bind(L_MainSkip);
        cc->add(iNative, vecSize - vecSize / 4u);
        cc->js(L_TailSkip);

        cc->bind(L_TailIter);
        x_storeu_fill(x86::ptr(dst), src, vecSize);
        cc->add(dst, vecSize);
        cc->sub(iNative, vecSize / 4u);
        cc->jnc(L_TailIter);

        cc->bind(L_TailSkip);
        cc->add(iNative, vecSize / 4u);
        cc->jz(L_End);

        PixelPredicate predicate(vecSize / 4u, PredicateFlags::kNeverEmptyOrFull, i);
        x_ensure_predicate_32(predicate, vecSize / 4u);
        v_store_predicated_v32(x86::ptr(dst), predicate, src);

        cc->lea(dst, x86::ptr(dst, iNative, 2));
        cc->bind(L_End);
      }
      else {
        Label L_LargeIter = cc->newLabel();
        Label L_SmallIter = cc->newLabel();
        Label L_SmallCheck = cc->newLabel();
        Label L_TinyCase16 = cc->newLabel();
        Label L_TinyCase8 = cc->newLabel();
        Label L_TinyCase4 = cc->newLabel();
        Label L_TinyCase2 = cc->newLabel();
        Label L_End = cc->newLabel();

        cc->sub(iNative, vecSize / 4u);
        cc->jc(vecSize == 64 ? L_TinyCase16 : L_TinyCase8);

        cc->cmp(iNative, vecSize);
        cc->jc(L_SmallIter);

        // Align to a vecSize, but keep two LSB bits in case the alignment is unfixable.
        v_storeu_ivec(x86::ptr(dst), src);
        cc->add(dst, vecSize);
        cc->lea(iNative, x86::ptr(dst, iNative, 2));
        cc->and_(dst, -int(vecSize) | 0x3);
        cc->sub(iNative, dst);
        cc->sar(iNative, 2);
        cc->sub(iNative, vecSize);

        cc->bind(L_LargeIter);
        x_storeu_fill(x86::ptr(dst), src, vecSize * 4);
        cc->add(dst, vecSize * 4);
        cc->sub(iNative, vecSize);
        cc->ja(L_LargeIter);

        cc->add(iNative, vecSize);
        cc->jmp(L_SmallCheck);

        cc->bind(L_SmallIter);
        v_storeu_ivec(x86::ptr(dst), src);
        cc->add(dst, vecSize);
        cc->bind(L_SmallCheck);
        cc->sub(iNative, vecSize / 4u);
        cc->ja(L_SmallIter);

        cc->lea(dst, x86::ptr(dst, iNative, 2, vecSize));
        v_storeu_ivec(x86::ptr(dst, -int(vecSize)), src);
        cc->jmp(L_End);

        if (vecSize == 64) {
          bind(L_TinyCase16);
          i_jmp_if_bit_zero(i, 3, L_TinyCase8);
          v_storeu_i256(x86::ptr(dst), src);
          i_add(dst, dst, 32);
        }

        bind(L_TinyCase8);
        i_jmp_if_bit_zero(i, 2, L_TinyCase4);
        v_storeu_i128(x86::ptr(dst), src);
        i_add(dst, dst, 16);

        bind(L_TinyCase4);
        i_jmp_if_bit_zero(i, 1, L_TinyCase2);
        v_store_i64(x86::ptr(dst), src);
        i_add(dst, dst, 8);

        bind(L_TinyCase2);
        cc->and_(i, 0x1);
        cc->shl(i, 2);
        i_add(dst, dst, iNative);
        v_store_i32(x86::ptr(dst, -4), src);

        bind(L_End);
      }
    }
    else {
      Label L_Finalize = cc->newLabel();
      Label L_End = cc->newLabel();

      // Preparation / Alignment
      // -----------------------

      {
        cc->cmp(i, oneStepInItems * (vecSize / 4u));
        cc->jb(L_Finalize);

        x86::Gp iptr = i.cloneAs(dst);
        if (sizeShift)
          cc->shl(iptr, sizeShift);
        cc->add(iptr, dst);

        v_storeu_ivec(x86::ptr(dst), src);

        cc->add(dst, src.size());
        cc->and_(dst, -1 ^ int(alignPattern));

        cc->sub(iptr, dst);
        if (sizeShift)
          cc->shr(iptr, sizeShift);
        cc->jz(L_End);
      }

      // MainLoop
      // --------

      {
        Label L_MainIter = cc->newLabel();
        Label L_MainSkip = cc->newLabel();

        cc->sub(i, mainStepInItems);
        cc->jc(L_MainSkip);

        cc->bind(L_MainIter);
        cc->add(dst, mainLoopSize);
        cc->sub(i, mainStepInItems);
        x_storea_fill(x86::ptr(dst, -int(mainLoopSize)), src.xmm(), mainLoopSize);
        cc->jnc(L_MainIter);

        cc->bind(L_MainSkip);
        cc->add(i, mainStepInItems);
        cc->jz(L_End);
      }

      // TailLoop / TailSequence
      // -----------------------

      if (mainLoopSize > vecSize * 2u) {
        Label L_TailIter = cc->newLabel();
        Label L_TailSkip = cc->newLabel();

        cc->sub(i, tailStepInItems);
        cc->jc(L_TailSkip);

        cc->bind(L_TailIter);
        cc->add(dst, vecSize);
        cc->sub(i, tailStepInItems);
        v_storea_ivec(x86::ptr(dst, -int(vecSize)), src);
        cc->jnc(L_TailIter);

        cc->bind(L_TailSkip);
        cc->add(i, tailStepInItems);
        cc->jz(L_End);
      }
      else if (mainLoopSize >= vecSize * 2u) {
        cc->cmp(i, tailStepInItems);
        cc->jb(L_Finalize);

        v_storea_ivec(x86::ptr(dst), src);
        cc->add(dst, vecSize);
        cc->sub(i, tailStepInItems);
        cc->jz(L_End);
      }

      // Finalize
      // --------

      {
        Label L_Store1 = cc->newLabel();

        cc->bind(L_Finalize);
        cc->cmp(i, 8u / itemSize);
        cc->jb(L_Store1);

        v_store_i64(x86::ptr(dst), src);
        cc->add(dst, 8);
        cc->sub(i, 8u / itemSize);
        cc->jz(L_End);

        cc->bind(L_Store1);
        v_store_i32(x86::ptr(dst), src);
        cc->add(dst, 4);
      }

      cc->bind(L_End);
    }

    return;
  }

  // Granularity == 1 Byte
  // ---------------------

  if (granularityInBytes == 1) {
    BL_ASSERT(itemSize == 1u);

    Label L_Finalize = cc->newLabel();
    Label L_End      = cc->newLabel();

    // Preparation / Alignment
    // -----------------------

    {
      Label L_Small = cc->newLabel();
      Label L_Large = cc->newLabel();

      cc->cmp(i, 15);
      cc->ja(L_Large);

      x86::Gp srcGp = cc->newInt32("srcGp");
      s_mov_i32(srcGp, src);

      cc->bind(L_Small);
      cc->mov(ptr_8(dst), srcGp.r8());
      cc->inc(dst);
      cc->dec(i);
      cc->jnz(L_Small);

      cc->jmp(L_End);

      cc->bind(L_Large);
      x86::Gp iptr = i.cloneAs(dst);
      cc->add(iptr, dst);

      v_storeu_i128(x86::ptr(dst), src);
      cc->add(dst, 16);
      cc->and_(dst, -16);

      cc->sub(iptr, dst);
      cc->jz(L_End);
    }

    // MainLoop
    // --------

    {
      Label L_MainIter = cc->newLabel();
      Label L_MainSkip = cc->newLabel();

      cc->sub(i, mainLoopSize);
      cc->jc(L_MainSkip);

      cc->bind(L_MainIter);
      cc->add(dst, mainLoopSize);
      cc->sub(i, mainLoopSize);
      for (j = 0; j < mainLoopSize; j += 16u)
        v_storea_i128(x86::ptr(dst, int(j) - int(mainLoopSize)), src);
      cc->jnc(L_MainIter);

      cc->bind(L_MainSkip);
      cc->add(i, mainLoopSize);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailIter = cc->newLabel();
      Label L_TailSkip = cc->newLabel();

      cc->sub(i, 16);
      cc->jc(L_TailSkip);

      cc->bind(L_TailIter);
      cc->add(dst, 16);
      cc->sub(i, 16);
      v_storea_i128(x86::ptr(dst, -16), src);
      cc->jnc(L_TailIter);

      cc->bind(L_TailSkip);
      cc->add(i, 16);
      cc->jz(L_End);
    }
    else if (mainLoopSize >= 32) {
      cc->cmp(i, 16);
      cc->jb(L_Finalize);

      v_storea_i128(x86::ptr(dst, int(j)), src);
      cc->add(dst, 16);
      cc->sub(i, 16);
      cc->jz(L_End);
    }

    // Finalize
    // --------

    {
      cc->add(dst, i.cloneAs(dst));
      v_storeu_i128(x86::ptr(dst, -16), src);
    }

    cc->bind(L_End);
    return;
  }

  BL_NOT_REACHED();
}

// BLPipeline::PipeCompiler - PixelCopy
// ====================================

void PipeCompiler::x_inline_pixel_copy_loop(x86::Gp& dst, x86::Gp& src, x86::Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity, BLInternalFormat format) noexcept {
  BL_ASSERT(BLIntOps::isPowerOf2(itemSize));
  BL_ASSERT(itemSize <= 16u);

  uint32_t granularityInBytes = itemSize * itemGranularity;
  uint32_t mainStepInItems = mainLoopSize / itemSize;

  BL_ASSERT(BLIntOps::isPowerOf2(granularityInBytes));
  BL_ASSERT(mainStepInItems * itemSize == mainLoopSize);

  BL_ASSERT(mainLoopSize >= 16u);
  BL_ASSERT(mainLoopSize >= granularityInBytes);

  x86::Vec t0 = cc->newXmm("t0");
  x86::Vec fillMask;

  if (format == BLInternalFormat::kXRGB32)
    fillMask = simdVecConst(&blCommonTable.i_FF000000FF000000, t0);

  // Granularity >= 16 Bytes
  // -----------------------

  if (granularityInBytes >= 16u) {
    Label L_End = cc->newLabel();

    // MainLoop
    // --------

    {
      Label L_MainIter = cc->newLabel();
      Label L_MainSkip = cc->newLabel();
      int ptrOffset = -int(mainLoopSize);

      cc->sub(i, mainStepInItems);
      cc->jc(L_MainSkip);

      cc->bind(L_MainIter);
      cc->add(dst, mainLoopSize);
      cc->add(src, mainLoopSize);
      cc->sub(i, mainStepInItems);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst, ptrOffset), false, x86::ptr(src, ptrOffset), false, mainLoopSize, fillMask);
      cc->jnc(L_MainIter);

      cc->bind(L_MainSkip);
      cc->add(i, mainStepInItems);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize * 2 > granularityInBytes) {
      Label L_TailIter = cc->newLabel();
      cc->bind(L_TailIter);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), false, x86::ptr(src), false, granularityInBytes, fillMask);
      cc->add(dst, granularityInBytes);
      cc->add(src, granularityInBytes);
      cc->sub(i, itemGranularity);
      cc->jnz(L_TailIter);
    }
    else if (mainLoopSize * 2 == granularityInBytes) {
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), false, x86::ptr(src), false, granularityInBytes, fillMask);
      cc->add(dst, granularityInBytes);
      cc->add(src, granularityInBytes);
    }

    cc->bind(L_End);
    return;
  }

  // Granularity == 4 Bytes
  // ----------------------

  if (granularityInBytes == 4u) {
    BL_ASSERT(itemSize <= 4u);
    uint32_t sizeShift = BLIntOps::ctz(itemSize);
    uint32_t alignPattern = (15u * itemSize) & 15u;

    uint32_t oneStepInItems = 4u >> sizeShift;
    uint32_t tailStepInItems = 16u >> sizeShift;

    Label L_Finalize = cc->newLabel();
    Label L_End      = cc->newLabel();

    // Preparation / Alignment
    // -----------------------

    {
      cc->cmp(i, oneStepInItems * 4u);
      cc->jb(L_Finalize);

      x86::Gp iptr = i.cloneAs(dst);
      v_loadu_i128(t0, x86::ptr(src));
      if (sizeShift)
        cc->shl(iptr, sizeShift);

      cc->add(iptr, dst);
      cc->sub(src, dst);
      v_storeu_i128(x86::ptr(dst), t0);
      cc->add(dst, 16);
      cc->and_(dst, -1 ^ int(alignPattern));

      cc->add(src, dst);
      cc->sub(iptr, dst);
      if (sizeShift)
        cc->shr(iptr, sizeShift);
      cc->jz(L_End);
    }

    // MainLoop
    // --------

    {
      Label L_MainIter = cc->newLabel();
      Label L_MainSkip = cc->newLabel();

      cc->sub(i, mainStepInItems);
      cc->jc(L_MainSkip);

      cc->bind(L_MainIter);
      cc->add(dst, mainLoopSize);
      cc->add(src, mainLoopSize);
      cc->sub(i, mainStepInItems);
      int ptrOffset = - int(mainLoopSize);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst, ptrOffset), true, x86::ptr(src, ptrOffset), false, mainLoopSize, fillMask);
      cc->jnc(L_MainIter);

      cc->bind(L_MainSkip);
      cc->add(i, mainStepInItems);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailIter = cc->newLabel();
      Label L_TailSkip = cc->newLabel();

      cc->sub(i, tailStepInItems);
      cc->jc(L_TailSkip);

      cc->bind(L_TailIter);
      cc->add(dst, 16);
      cc->add(src, 16);
      cc->sub(i, tailStepInItems);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst, -16), true, x86::ptr(src, -16), false, 16, fillMask);
      cc->jnc(L_TailIter);

      cc->bind(L_TailSkip);
      cc->add(i, tailStepInItems);
      cc->jz(L_End);
    }
    else if (mainLoopSize >= 32) {
      cc->cmp(i, tailStepInItems);
      cc->jb(L_Finalize);

      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), true, x86::ptr(src), false, 16, fillMask);
      cc->add(dst, 16);
      cc->add(src, 16);
      cc->sub(i, tailStepInItems);
      cc->jz(L_End);
    }

    // Finalize
    // --------

    {
      Label L_Store1 = cc->newLabel();

      cc->bind(L_Finalize);
      cc->cmp(i, 8u / itemSize);
      cc->jb(L_Store1);

      v_load_i64(t0, x86::ptr(src));
      cc->add(src, 8);
      v_store_i64(x86::ptr(dst), t0);
      cc->add(dst, 8);
      cc->sub(i, 8u / itemSize);
      cc->jz(L_End);

      cc->bind(L_Store1);
      v_load_i32(t0, x86::ptr(src));
      cc->add(src, 4);
      v_store_i32(x86::ptr(dst), t0);
      cc->add(dst, 4);
    }

    cc->bind(L_End);
    return;
  }

  // Granularity == 1 Byte
  // ---------------------

  if (granularityInBytes == 1) {
    BL_ASSERT(itemSize == 1u);

    Label L_Finalize = cc->newLabel();
    Label L_End      = cc->newLabel();

    // Preparation / Alignment
    // -----------------------

    {
      Label L_Small = cc->newLabel();
      Label L_Large = cc->newLabel();

      x86::Gp iptr = i.cloneAs(dst);
      x86::Gp byte_val = cc->newInt32("byte_val");

      cc->cmp(i, 15);
      cc->ja(L_Large);

      cc->bind(L_Small);
      cc->movzx(byte_val, ptr_8(src));
      cc->inc(src);
      cc->mov(ptr_8(dst), byte_val.r8());
      cc->inc(dst);
      cc->dec(i);
      cc->jnz(L_Small);
      cc->jmp(L_End);

      cc->bind(L_Large);
      v_loadu_i128(t0, x86::ptr(src));
      cc->add(iptr, dst);
      cc->sub(src, dst);

      v_storeu_i128(x86::ptr(dst), t0);
      cc->add(dst, 16);
      cc->and_(dst, -16);

      cc->add(src, dst);
      cc->sub(iptr, dst);
      cc->jz(L_End);
    }

    // MainLoop
    // --------

    {
      Label L_MainIter = cc->newLabel();
      Label L_MainSkip = cc->newLabel();

      cc->sub(i, mainLoopSize);
      cc->jc(L_MainSkip);

      cc->bind(L_MainIter);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), true, x86::ptr(src), false, mainLoopSize, fillMask);
      cc->add(dst, mainLoopSize);
      cc->add(src, mainLoopSize);
      cc->sub(i, mainLoopSize);
      cc->jnc(L_MainIter);

      cc->bind(L_MainSkip);
      cc->add(i, mainLoopSize);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailIter = cc->newLabel();
      Label L_TailSkip = cc->newLabel();

      cc->sub(i, 16);
      cc->jc(L_TailSkip);

      cc->bind(L_TailIter);
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), true, x86::ptr(src), false, 16, fillMask);
      cc->add(dst, 16);
      cc->add(src, 16);
      cc->sub(i, 16);
      cc->jnc(L_TailIter);

      cc->bind(L_TailSkip);
      cc->add(i, 16);
      cc->jz(L_End);
    }
    else if (mainLoopSize >= 32) {
      cc->cmp(i, 16);
      cc->jb(L_Finalize);

      _x_inline_memcpy_sequence_xmm(x86::ptr(dst), true, x86::ptr(src), false, 16, fillMask);
      cc->add(dst, 16);
      cc->add(src, 16);
      cc->sub(i, 16);
      cc->jz(L_End);
    }

    // Finalize
    // --------

    {
      cc->add(dst, i.cloneAs(dst));
      cc->add(src, i.cloneAs(src));
      _x_inline_memcpy_sequence_xmm(x86::ptr(dst, -16), false, x86::ptr(src, -16), false, 16, fillMask);
    }

    cc->bind(L_End);
    return;
  }
}

void PipeCompiler::_x_inline_memcpy_sequence_xmm(
  const x86::Mem& dPtr, bool dstAligned,
  const x86::Mem& sPtr, bool srcAligned, uint32_t numBytes, const x86::Vec& fillMask) noexcept {

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
} // {BLPipeline}

#endif
