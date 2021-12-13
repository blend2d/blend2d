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

// BLPipeline::PipeCompiler - Construction & Destruction
// =====================================================

PipeCompiler::PipeCompiler(x86::Compiler* cc, const CpuFeatures& features) noexcept
  : cc(cc),
    _features(features),
    _commonTableOff(128) {}

PipeCompiler::~PipeCompiler() noexcept {}

// BLPipeline::PipeCompiler - BeginFunction & EndFunction
// ======================================================

void PipeCompiler::beginFunction() noexcept {
  // Function prototype and arguments.
  _funcNode = cc->addFunc(asmjit::FuncSignatureT<void, ContextData*, const void*, const void*>(asmjit::CallConvId::kCDecl));
  _funcInit = cc->cursor();
  _funcEnd = _funcNode->endNode()->prev();

  if (_features.x86().hasAVX())
    _funcNode->frame().setAvxEnabled();

  if (_features.x86().hasAVX512_F())
    _funcNode->frame().setAvx512Enabled();

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

  if (fillType == FillType::kBoxU)
    return newPartT<FillBoxUPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  if (fillType == FillType::kMask)
    return newPartT<FillMaskPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  if (fillType == FillType::kAnalytic)
    return newPartT<FillAnalyticPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  return nullptr;
}

FetchPart* PipeCompiler::newFetchPart(FetchType fetchType, uint32_t format) noexcept {
  if (fetchType == FetchType::kSolid)
    return newPartT<FetchSolidPart>(format);

  if (fetchType >= FetchType::kGradientLinearFirst && fetchType <= FetchType::kGradientLinearLast)
    return newPartT<FetchLinearGradientPart>(fetchType, format);

  if (fetchType >= FetchType::kGradientRadialFirst && fetchType <= FetchType::kGradientRadialLast)
    return newPartT<FetchRadialGradientPart>(fetchType, format);

  if (fetchType >= FetchType::kGradientConicalFirst && fetchType <= FetchType::kGradientConicalLast)
    return newPartT<FetchConicalGradientPart>(fetchType, format);

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

  SimdWidth simdWidth = _features.x86().hasAVX512_BW() ? SimdWidth::k512 :
                        _features.x86().hasAVX2()      ? SimdWidth::k256 : SimdWidth::k128;

  root->forEachPart([&](PipePart* part) {
    simdWidth = SimdWidth(blMin<uint32_t>(uint32_t(simdWidth), uint32_t(part->maxSimdWidthSupported())));
  });

  _simdWidth = simdWidth;
  _simdRegType = simdRegTypeFromWidth(simdWidth);
  _simdTypeId = asmjit::ArchTraits::byArch(cc->arch()).regTypeToTypeId(_simdRegType);
  _simdMultiplier = 1 << (uint32_t(_simdRegType) - uint32_t(RegType::kX86_Xmm));
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

x86::Mem PipeCompiler::constAsMem(const void* p) noexcept {
  // Make sure we are addressing a constant from the `blCommonTable` constant pool.
  const void* global = &blCommonTable;
  BL_ASSERT((uintptr_t)p >= (uintptr_t)global &&
            (uintptr_t)p <  (uintptr_t)global + sizeof(BLCommonTable));

  if (cc->is32Bit()) {
    // 32-bit mode - These constants will never move in memory so the absolute
    // addressing is a win/win as we can save one GP register that can be used
    // for something else.
    return x86::ptr((uint64_t)p);
  }
  else {
    // 64-bit mode - One GP register is sacrificed to hold the pointer to the
    // `blCommonTable`. This is probably the safest approach as relying on absolute
    // addressing or anything else could lead to problems or performance issues.
    _initCommonTablePtr();

    int32_t disp = int32_t((intptr_t)p - (intptr_t)global);
    return x86::ptr(_commonTablePtr, disp - _commonTableOff);
  }
}

x86::Xmm PipeCompiler::constAsXmm(const void* p) noexcept {
  static const char xmmNames[4][16] = {
    "xmm.zero",
    "xmm.u16_128",
    "xmm.u16_257",
    "xmm.alpha"
  };

  int constIndex = -1;

  if      (p == &blCommonTable.i128_zero) constIndex = 0; // Required if the CPU doesn't have SSE4.1.
  else if (p == &blCommonTable.i128_0080008000800080) constIndex = 1; // Required by `xDiv255()` and friends.
  else if (p == &blCommonTable.i128_0101010101010101) constIndex = 2; // Required by `xDiv255()` and friends.
  else if (p == &blCommonTable.i128_FF000000FF000000) constIndex = 3; // Required by fetching XRGB32 pixels as PRGB32 pixels.

  if (constIndex == -1) {
    // TODO: [PIPEGEN] This works, but it's really nasty!
    x86::Mem m = constAsMem(p);
    return reinterpret_cast<x86::Xmm&>(m);
  }

  x86::Xmm& xmm = _constantsXmm[constIndex];
  if (!xmm.isValid()) {
    xmm = cc->newXmm(xmmNames[constIndex]);

    if (constIndex == 0) {
      asmjit::BaseNode* prevNode = cc->setCursor(_funcInit);
      v_zero_f(xmm);
      _funcInit = cc->setCursor(prevNode);
    }
    else {
      // `constAsMem()` may call `_initCommonTablePtr()` for the very first time.
      // We cannot inject any code before `constAsMem()` returns.
      x86::Mem m = constAsMem(p);

      asmjit::BaseNode* prevNode = cc->setCursor(_funcInit);
      v_loada_f128(xmm, m);
      _funcInit = cc->setCursor(prevNode);
    }
  }

  return xmm;
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

// BLPipeline::PipeCompiler - Emit
// ===============================

static constexpr OperandSignature signatureOfXmmYmmZmm[] = {
  OperandSignature{x86::Xmm::kSignature},
  OperandSignature{x86::Ymm::kSignature},
  OperandSignature{x86::Zmm::kSignature}
};

static inline uint32_t shuf32ToShuf64(uint32_t imm) noexcept {
  uint32_t imm0 = uint32_t(imm     ) & 1u;
  uint32_t imm1 = uint32_t(imm >> 1) & 1u;
  return x86::shuffleImm(imm1 * 2u + 1u, imm1 * 2u, imm0 * 2u + 1u, imm0 * 2u);
}

static inline void fixVecSignature(Operand_& op, OperandSignature signature) noexcept {
  if (x86::Reg::isVec(op) && op.signature().bits() > signature.bits())
    op.setSignature(signature);
}

static inline bool isSameReg(const Operand_& a, const Operand_& b) noexcept {
  return a.id() == b.id() && a.id() && b.id();
}

void PipeCompiler::iemit2(InstId instId, const Operand_& op1, int imm) noexcept {
  cc->emit(instId, op1, imm);
}

void PipeCompiler::iemit2(InstId instId, const Operand_& op1, const Operand_& op2) noexcept {
  cc->emit(instId, op1, op2);
}

void PipeCompiler::iemit3(InstId instId, const Operand_& op1, const Operand_& op2, int imm) noexcept {
  cc->emit(instId, op1, op2, imm);
}

void PipeCompiler::vemit_xmov(const Operand_& dst, const Operand_& src, uint32_t width) noexcept {
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

void PipeCompiler::vemit_xmov(const OpArray& dst, const Operand_& src, uint32_t width) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  while (dstIndex < dstCount) {
    vemit_xmov(dst[dstIndex], src, width);
    dstIndex++;
  }
}

void PipeCompiler::vemit_xmov(const OpArray& dst, const OpArray& src, uint32_t width) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  uint32_t srcIndex = 0;
  uint32_t srcCount = src.size();

  while (dstIndex < dstCount) {
    vemit_xmov(dst[dstIndex], src[srcIndex], width);

    if (++srcIndex >= srcCount) srcIndex = 0;
    dstIndex++;
  }
}

void PipeCompiler::vemit_vv_vv(uint32_t packedId, const Operand_& dst_, const Operand_& src_) noexcept {
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
          break;
        }

        vemit_xmov(dst, src, 8);
        v_interleave_lo_i8(dst, dst, constAsXmm(&blCommonTable.i128_zero));
        return;
      }

      case kIntrin2Vmovu8u32: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpmovzxbd, x86::Inst::kIdPmovzxbd);
          break;
        }

        vemit_xmov(dst, src, 4);
        v_interleave_lo_i8(dst, dst, constAsXmm(&blCommonTable.i128_zero));
        v_interleave_lo_i16(dst, dst, constAsXmm(&blCommonTable.i128_zero));
        return;
      }

      case kIntrin2Vmovu16u32: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpmovzxwd, x86::Inst::kIdPmovzxwd);
          break;
        }

        vemit_xmov(dst, src, 8);
        v_interleave_lo_i16(dst, dst, constAsXmm(&blCommonTable.i128_zero));
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
        v_xor(dst, src, tmp);
        v_sub_i32(dst, dst, tmp);
        return;
      }

      case kIntrin2Vabsi64: {
        x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");

        v_duph_i32(tmp, src);
        v_sra_i32(tmp, tmp, 31);
        v_xor(dst, src, tmp);
        v_sub_i32(dst, dst, tmp);
        return;
      }

      case kIntrin2Vinv255u16: {
        Operand u16_255 = constAsXmm(&blCommonTable.i128_00FF00FF00FF00FF);

        if (hasAVX() || isSameReg(dst, src)) {
          v_xor(dst, src, u16_255);
        }
        else {
          v_mov(dst, u16_255);
          v_xor(dst, dst, src);
        }
        return;
      }

      case kIntrin2Vinv256u16: {
        x86::Vec u16_0100 = constAsXmm(&blCommonTable.i128_0100010001000100);

        if (!isSameReg(dst, src)) {
          v_mov(dst, u16_0100);
          v_sub_i16(dst, dst, src);
        }
        else if (hasSSSE3()) {
          v_sub_i16(dst, dst, u16_0100);
          v_abs_i16(dst, dst);
        }
        else {
          v_xor(dst, dst, constAsXmm(&blCommonTable.i128_FFFFFFFFFFFFFFFF));
          v_add_i16(dst, dst, u16_0100);
        }
        return;
      }

      case kIntrin2Vinv255u32: {
        Operand u32_255 = constAsXmm(&blCommonTable.i128_000000FF000000FF);

        if (hasAVX() || isSameReg(dst, src)) {
          v_xor(dst, src, u32_255);
        }
        else {
          v_mov(dst, u32_255);
          v_xor(dst, dst, src);
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

      case kIntrin2VBroadcastU16: {
        BL_ASSERT(src.isReg() || src.isMem());

        if (src.isReg()) {
          Operand x(src);
          // Reg <- BroadcastW(Reg).
          if (src.as<x86::Reg>().isGp()) {
            if (!hasAVX512_F()) {
              s_mov_i32(dst.as<x86::Vec>().xmm(), src.as<x86::Gp>().r32());
              x = dst;
            }
            else {
              src = src.as<x86::Gp>().r32();
            }
          }

          if (hasAVX2()) {
            cc->emit(x86::Inst::kIdVpbroadcastw, dst, x);
          }
          else {
            v_swizzle_lo_i16(dst, x, x86::shuffleImm(0, 0, 0, 0));
            v_swizzle_i32(dst, dst, x86::shuffleImm(1, 0, 1, 0));
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

            v_swizzle_lo_i16(dst, dst, x86::shuffleImm(0, 0, 0, 0));
            v_swizzle_i32(dst, dst, x86::shuffleImm(1, 0, 1, 0));
          }
        }

        return;
      }

      case kIntrin2VBroadcastU32: {
        BL_ASSERT(src.isReg() || src.isMem());

        if (src.isReg()) {
          Operand x(src);
          // Reg <- BroadcastD(Reg).
          if (src.as<x86::Reg>().isGp() && !hasAVX512_F()) {
            s_mov_i32(dst.as<x86::Vec>().xmm(), src.as<x86::Gp>().r32());
            x = dst;
          }

          if (x.as<x86::Reg>().isVec())
            x = x.as<x86::Vec>().xmm();

          if (hasAVX2())
            cc->emit(x86::Inst::kIdVpbroadcastd, dst, x);
          else
            v_swizzle_i32(dst, dst, x86::shuffleImm(0, 0, 0, 0));
        }
        else {
          // Reg <- BroadcastD(Mem).
          x86::Mem m(src.as<x86::Mem>());
          m.setSize(4);

          if (hasAVX2()) {
            cc->emit(x86::Inst::kIdVpbroadcastd, dst, m);
          }
          else {
            v_load_i32(dst.as<x86::Vec>(), m);
            v_swizzle_i32(dst, dst, x86::shuffleImm(0, 0, 0, 0));
          }
        }

        return;
      }

      case kIntrin2VBroadcastU64: {
        BL_ASSERT(src.isReg() || src.isMem());

        if (src.isReg()) {
          Operand x(src);
          // Reg <- BroadcastQ(Reg).
          if (src.as<x86::Reg>().isGp() && !hasAVX512_F()) {
            s_mov_i64(dst.as<x86::Vec>().xmm(), src.as<x86::Gp>().r64());
            x = dst;
          }

          if (x.as<x86::Reg>().isVec())
            x = x.as<x86::Vec>().xmm();

          if (hasAVX2())
            cc->emit(x86::Inst::kIdVpbroadcastq, dst, x);
          else
            v_swizzle_i32(dst, dst, x86::shuffleImm(1, 0, 1, 0));
        }
        else {
          // Reg <- BroadcastQ(Mem).
          x86::Mem m(src.as<x86::Mem>());
          m.setSize(8);

          if (hasAVX2()) {
            cc->emit(x86::Inst::kIdVpbroadcastq, dst, m);
          }
          else {
            v_load_i64(dst.as<x86::Vec>(), m);
            v_swizzle_i32(dst, dst, x86::shuffleImm(1, 0, 1, 0));
          }
        }

        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }

  // Single instruction.
  InstId instId = hasAVX() ? PackedInst::avxId(packedId)
                           : PackedInst::sseId(packedId);
  cc->emit(instId, dst, src);
}

void PipeCompiler::vemit_vv_vv(uint32_t packedId, const OpArray& dst_, const Operand_& src_) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  while (dstIndex < dstCount) {
    vemit_vv_vv(packedId, dst_[dstIndex], src_);
    dstIndex++;
  }
}

void PipeCompiler::vemit_vv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src_) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  uint32_t srcIndex = 0;
  uint32_t srcCount = src_.size();

  while (dstIndex < dstCount) {
    vemit_vv_vv(packedId, dst_[dstIndex], src_[srcIndex]);

    if (++srcIndex >= srcCount) srcIndex = 0;
    dstIndex++;
  }
}

void PipeCompiler::vemit_vvi_vi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept {
  // Intrinsics support.
  if (PackedInst::isIntrin(packedId)) {
    switch (PackedInst::intrinId(packedId)) {
      case kIntrin2iVswizps:
        if (isSameReg(dst_, src_) || hasAVX())
          v_shuffle_f32(dst_, src_, src_, imm);
        else
          v_swizzle_i32(dst_, src_, imm);
        return;

      case kIntrin2iVswizpd:
        if (isSameReg(dst_, src_) || hasAVX())
          v_shuffle_f64(dst_, src_, src_, imm);
        else
          v_swizzle_i32(dst_, src_, shuf32ToShuf64(imm));
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

void PipeCompiler::vemit_vvi_vi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  while (dstIndex < dstCount) {
    vemit_vvi_vi(packedId, dst_[dstIndex], src_, imm);
    dstIndex++;
  }
}

void PipeCompiler::vemit_vvi_vi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  uint32_t srcIndex = 0;
  uint32_t srcCount = src_.size();

  while (dstIndex < dstCount) {
    vemit_vvi_vi(packedId, dst_[dstIndex], src_[srcIndex], imm);

    if (++srcIndex >= srcCount) srcIndex = 0;
    dstIndex++;
  }
}

void PipeCompiler::vemit_vvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept {
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

void PipeCompiler::vemit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  while (dstIndex < dstCount) {
    vemit_vvi_vvi(packedId, dst_[dstIndex], src_, imm);
    dstIndex++;
  }
}

void PipeCompiler::vemit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  uint32_t srcIndex = 0;
  uint32_t srcCount = src_.size();

  while (dstIndex < dstCount) {
    vemit_vvi_vvi(packedId, dst_[dstIndex], src_[srcIndex], imm);

    if (++srcIndex >= srcCount) srcIndex = 0;
    dstIndex++;
  }
}

void PipeCompiler::vemit_vvv_vv(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_) noexcept {
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
      case kIntrin3Vcombhli64: {
        // Swap Case:
        //   dst'.u64[0] = src_.u64[1];
        //   dst'.u64[1] = src_.u64[0];
        if (isSameReg(src1_, src2_)) {
          v_swap_i64(dst_, src1_);
          return;
        }

        // Dst is Src2 Case:
        //   dst'.u64[0] = src1.u64[1];
        //   dst'.u64[1] = dst_.u64[0];
        if (isSameReg(dst_, src2_) && !hasAVX()) {
          if (hasSSSE3()) {
            v_alignr_u8_(dst_, dst_, src1_, 8);
          }
          else {
            v_shuffle_f64(dst_, dst_, src1_, x86::shuffleImm(1, 0));
            v_swap_i64(dst_, dst_);
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

          v_swizzle_i32(tmp, dst, x86::shuffleImm(2, 3, 0, 1));
          v_mulx_ll_u32_(dst, dst, src2);
          v_mulx_ll_u32_(tmp, tmp, src2);
          v_sll_i64(tmp, tmp, 32);
          v_add_i64(dst, dst, tmp);
        }
        else if (isSameReg(dst, src2)) {
          x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");

          v_swizzle_i32(tmp, src1, x86::shuffleImm(2, 3, 0, 1));
          v_mulx_ll_u32_(tmp, tmp, dst);
          v_mulx_ll_u32_(dst, dst, src1);
          v_sll_i64(tmp, tmp, 32);
          v_add_i64(dst, dst, tmp);
        }
        else {
          v_swizzle_i32(dst, src1, x86::shuffleImm(2, 3, 0, 1));
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
    cc->emit(instId, dst, src1, src2);
  }
  else {
    InstId instId = PackedInst::sseId(packedId);
    if (!isSameReg(dst, src1))
      cc->emit(x86::Inst::kIdMovaps, dst, src1);
    cc->emit(instId, dst, src2);
  }
}

void PipeCompiler::vemit_vvv_vv(uint32_t packedId, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_) noexcept {
  vemit_vvv_vv(packedId, dst_, OpArray(src1_), src2_);
}

void PipeCompiler::vemit_vvv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_) noexcept {
  vemit_vvv_vv(packedId, dst_, src1_, OpArray(src2_));
}

void PipeCompiler::vemit_vvv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  uint32_t src1Index = 0;
  uint32_t src1Count = src1_.size();

  uint32_t src2Index = 0;
  uint32_t src2Count = src2_.size();

  while (dstIndex < dstCount) {
    vemit_vvv_vv(packedId, dst_[dstIndex], src1_[src1Index], src2_[src2Index]);

    if (++src1Index >= src1Count) src1Index = 0;
    if (++src2Index >= src2Count) src2Index = 0;
    dstIndex++;
  }
}

void PipeCompiler::vemit_vvvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, uint32_t imm) noexcept {
  Operand dst(dst_);
  Operand src1(src1_);
  Operand src2(src2_);

  if (PackedInst::width(packedId) < PackedInst::kWidthZ) {
    OperandSignature signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst , signature);
    fixVecSignature(src1, signature);
    fixVecSignature(src2, signature);
  }

  if (hasAVX()) {
    InstId instId = PackedInst::avxId(packedId);
    cc->emit(instId, dst, src1, src2, imm);
  }
  else {
    InstId instId = PackedInst::sseId(packedId);
    if (!isSameReg(dst, src1))
      cc->emit(x86::Inst::kIdMovaps, dst, src1);
    cc->emit(instId, dst, src2, imm);
  }
}

void PipeCompiler::vemit_vvvi_vvi(uint32_t packedId, const OpArray& dst, const Operand_& src1, const OpArray& src2, uint32_t imm) noexcept {
  vemit_vvvi_vvi(packedId, dst, OpArray(src1), src2, imm);
}

void PipeCompiler::vemit_vvvi_vvi(uint32_t packedId, const OpArray& dst, const OpArray& src1, const Operand_& src2, uint32_t imm) noexcept {
  vemit_vvvi_vvi(packedId, dst, src1, OpArray(src2), imm);
}

void PipeCompiler::vemit_vvvi_vvi(uint32_t packedId, const OpArray& dst, const OpArray& src1, const OpArray& src2, uint32_t imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  uint32_t src1Index = 0;
  uint32_t src1Count = src1.size();

  uint32_t src2Index = 0;
  uint32_t src2Count = src2.size();

  while (dstIndex < dstCount) {
    vemit_vvvi_vvi(packedId, dst[dstIndex], src1[src1Index], src2[src2Index], imm);

    if (++src1Index >= src1Count) src1Index = 0;
    if (++src2Index >= src2Count) src2Index = 0;
    dstIndex++;
  }
}

void PipeCompiler::vemit_vvvv_vvv(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, const Operand_& src3_) noexcept {
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
          v_xor(tmp, dst, src2);
          v_and(tmp, tmp, src3);
          v_xor(dst, dst, tmp);
        }
        else if (dst.id() == src3.id()) {
          x86::Xmm tmp = cc->newXmm("@tmp");
          v_xor(tmp, src1, src2);
          v_nand(dst, dst, tmp);
          v_xor(dst, dst, src2);
        }
        else {
          v_xor(dst, src2, src1);
          v_and(dst, dst, src3);
          v_xor(dst, dst, src1);
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
          v_and(src2, src2, src3);
          v_nand(src3, src3, src1);
          v_or(dst, src3, src2);
        }
        else {
          v_and(src2, src2, src3);
          v_nand(src3, src3, src1);
          v_or(dst, src2, src3);
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

void PipeCompiler::vemit_vvvv_vvv(uint32_t packedId, const OpArray& dst, const OpArray& src1, const OpArray& src2, const Operand_& src3) noexcept {
  vemit_vvvv_vvv(packedId, dst, src1, src2, OpArray(src3));
}

void PipeCompiler::vemit_vvvv_vvv(uint32_t packedId, const OpArray& dst, const OpArray& src1, const OpArray& src2, const OpArray& src3) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst.size();

  uint32_t src1Index = 0;
  uint32_t src1Count = src1.size();

  uint32_t src2Index = 0;
  uint32_t src2Count = src2.size();

  uint32_t src3Index = 0;
  uint32_t src3Count = src3.size();

  while (dstIndex < dstCount) {
    vemit_vvvv_vvv(packedId, dst[dstIndex], src1[src1Index], src2[src2Index], src3[src3Index]);

    if (++src1Index >= src1Count) src1Index = 0;
    if (++src2Index >= src2Count) src2Index = 0;
    if (++src3Index >= src3Count) src3Index = 0;
    dstIndex++;
  }
}

// BLPipeline::PipeCompiler - Fetch Helpers
// ========================================

void PipeCompiler::xFetchPixel_1x(Pixel& p, PixelFlags flags, uint32_t sFormat, const x86::Mem& sMem, uint32_t sAlignment) noexcept {
  BL_ASSERT(p.type() != PixelType::kNone);
  blUnused(sAlignment);

  p.setCount(1);
  x86::Mem sAdj(sMem);

  if (p.isRGBA()) {
    switch (sFormat) {
      case BL_FORMAT_PRGB32: {
        if (blTestFlag(flags, PixelFlags::kAny)) {
          newVecArray(p.pc, 1, "c");
          v_load_i32(p.pc[0].xmm(), sAdj);
        }
        break;
      }

      case BL_FORMAT_XRGB32: {
        if (blTestFlag(flags, PixelFlags::kAny)) {
          newVecArray(p.pc, 1, "c");
          v_load_i32(p.pc[0].xmm(), sAdj);
          vFillAlpha255B(p.pc[0].xmm(), p.pc[0]);
        }
        break;
      }

      case BL_FORMAT_A8: {
        if (blTestFlag(flags, PixelFlags::kAny)) {
          if (hasAVX2()) {
            newVecArray(p.pc, 1, "c");
            cc->vpbroadcastb(p.pc[0].xmm(), sAdj);
          }
          else if (hasSSE4_1()) {
            newVecArray(p.uc, 1, "c");
            v_zero_i(p.uc[0]);
            v_insert_u8_(p.uc[0], p.uc[0], sAdj, 0);
            v_swizzle_lo_i16(p.uc[0], p.uc[0], x86::shuffleImm(0, 0, 0, 0));
          }
          else {
            newVecArray(p.uc, 1, "c");
            x86::Gp scalar = cc->newUInt32();
            load8(scalar, sAdj);
            s_mov_i32(p.uc[0], scalar);
            v_swizzle_lo_i16(p.uc[0], p.uc[0], x86::shuffleImm(0, 0, 0, 0));
          }
        }

        break;
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else if (p.isAlpha()) {
    p.sa = cc->newUInt32("a");

    switch (sFormat) {
      case BL_FORMAT_PRGB32: {
        sAdj.addOffset(3);
        load8(p.sa, sAdj);
        break;
      }

      case BL_FORMAT_XRGB32: {
        cc->mov(p.sa, 255);
        break;
      }

      case BL_FORMAT_A8: {
        load8(p.sa, sAdj);
        break;
      }

      default:
        BL_NOT_REACHED();
    }
  }

  xSatisfyPixel(p, flags);
}

void PipeCompiler::xFetchPixel_4x(Pixel& p, PixelFlags flags, uint32_t sFormat, const x86::Mem& sMem, uint32_t sAlignment) noexcept {
  BL_ASSERT(p.type() != PixelType::kNone);

  p.setCount(4);
  x86::Mem sAdj(sMem);

  if (p.isRGBA()) {
    switch (sFormat) {
      case BL_FORMAT_PRGB32: {
        if (blTestFlag(flags, PixelFlags::kPC)) {
          newXmmArray(p.pc, 1, "c");

          sAdj.setSize(16);
          if (sAlignment == 16)
            v_loada_i128(p.pc[0], sAdj);
          else
            v_loadu_i128(p.pc[0], sAdj);
        }
        else {
          newXmmArray(p.uc, 2, "c");

          sAdj.setSize(8);
          vmovu8u16(p.uc[0], sAdj); sAdj.addOffsetLo32(8);
          vmovu8u16(p.uc[1], sAdj);
        }
        break;
      }

      case BL_FORMAT_XRGB32: {
        if (blTestFlag(flags, PixelFlags::kAny)) {
          newXmmArray(p.pc, 1, "c");
          sAdj.setSize(16);

          if (sAlignment == 16)
            v_loada_i128(p.pc[0], sAdj);
          else
            v_loadu_i128(p.pc[0], sAdj);

          vFillAlpha255B(p.pc[0], p.pc[0]);
        }
        break;
      }

      case BL_FORMAT_A8: {
        sAdj.setSize(4);
        if (blTestFlag(flags, PixelFlags::kPC)) {
          newXmmArray(p.pc, 1, "c");

          v_load_i32(p.pc[0], sAdj);
          v_interleave_lo_i8(p.pc[0], p.pc[0], p.pc[0]);
          v_interleave_lo_i16(p.pc[0], p.pc[0], p.pc[0]);
        }
        else {
          newXmmArray(p.uc, 2, "c");

          v_load_i32(p.uc[0], sAdj);
          v_interleave_lo_i8(p.uc[0], p.uc[0], p.uc[0]);
          vmovu8u16(p.uc[0], p.uc[0]);

          v_swizzle_i32(p.uc[1], p.uc[0], x86::shuffleImm(3, 3, 2, 2));
          v_swizzle_i32(p.uc[0], p.uc[0], x86::shuffleImm(1, 1, 0, 0));
        }
        break;
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else if (p.isAlpha()) {
    // Cannot use scalar pixel in SIMD mode.
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kSA));

    switch (sFormat) {
      case BL_FORMAT_PRGB32: {
        x86::Xmm a = cc->newXmm("a");
        sAdj.setSize(16);

        if (sAlignment == 16)
          v_loada_i128(a, sAdj);
        else
          v_loadu_i128(a, sAdj);

        v_srl_i32(a, a, 24);
        v_packs_i32_i16(a, a, a);

        if (blTestFlag(flags, PixelFlags::kPA)) {
          v_packs_i16_u8(a, a, a);
          p.pa.init(a);
        }
        else {
          p.ua.init(a);
        }
        break;
      }

      case BL_FORMAT_A8: {
        x86::Xmm a = cc->newXmm("a");
        sAdj.setSize(4);

        v_load_i32(a, sAdj);

        if (blTestFlag(flags, PixelFlags::kPC)) {
          p.pa.init(a);
        }
        else {
          vmovu8u16(a, a);
          p.ua.init(a);
        }
        break;
      }

      default:
        BL_NOT_REACHED();
    }
  }

  xSatisfyPixel(p, flags);
}

void PipeCompiler::xFetchPixel_8x(Pixel& p, PixelFlags flags, uint32_t sFormat, const x86::Mem& sMem, uint32_t sAlignment) noexcept {
  BL_ASSERT(p.type() != PixelType::kNone);

  p.setCount(8);
  x86::Mem sAdj(sMem);

  if (p.isRGBA()) {
    switch (sFormat) {
      case BL_FORMAT_PRGB32: {
        if (blTestFlag(flags, PixelFlags::kPC) || !hasSSE4_1()) {
          newXmmArray(p.pc, 2, "c");
          sAdj.setSize(16);

          if (sAlignment == 16) {
            v_loada_i128(p.pc[0], sAdj); sAdj.addOffsetLo32(16);
            v_loada_i128(p.pc[1], sAdj);
          }
          else {
            v_loadu_i128(p.pc[0], sAdj); sAdj.addOffsetLo32(16);
            v_loadu_i128(p.pc[1], sAdj);
          }
        }
        else {
          newXmmArray(p.uc, 4, "c");
          sAdj.setSize(8);

          vmovu8u16(p.uc[0], sAdj); sAdj.addOffsetLo32(8);
          vmovu8u16(p.uc[1], sAdj); sAdj.addOffsetLo32(8);
          vmovu8u16(p.uc[2], sAdj); sAdj.addOffsetLo32(8);
          vmovu8u16(p.uc[3], sAdj);
        }
        break;
      }

      case BL_FORMAT_XRGB32: {
        if (blTestFlag(flags, PixelFlags::kAny)) {
          newXmmArray(p.pc, 2, "c");
          sAdj.setSize(16);

          if (sAlignment == 16) {
            v_loada_i128(p.pc[0], sAdj); sAdj.addOffsetLo32(16);
            v_loada_i128(p.pc[1], sAdj);
          }
          else {
            v_loadu_i128(p.pc[0], sAdj); sAdj.addOffsetLo32(16);
            v_loadu_i128(p.pc[1], sAdj);
          }

          vFillAlpha255B(p.pc[0], p.pc[0]);
          vFillAlpha255B(p.pc[1], p.pc[1]);
        }
        break;
      }

      case BL_FORMAT_A8: {
        sAdj.setSize(4);
        if (blTestFlag(flags, PixelFlags::kPC)) {
          newXmmArray(p.pc, 2, "c");

          v_load_i32(p.pc[0], sAdj); sAdj.addOffsetLo32(4);
          v_load_i32(p.pc[1], sAdj);

          v_interleave_lo_i8(p.pc[0], p.pc[0], p.pc[0]);
          v_interleave_lo_i8(p.pc[1], p.pc[1], p.pc[1]);

          v_interleave_lo_i16(p.pc[0], p.pc[0], p.pc[0]);
          v_interleave_lo_i16(p.pc[1], p.pc[1], p.pc[1]);
        }
        else {
          newXmmArray(p.uc, 4, "c");

          v_load_i32(p.uc[0], sAdj); sAdj.addOffsetLo32(4);
          v_load_i32(p.uc[2], sAdj);

          v_interleave_lo_i8(p.uc[0], p.uc[0], p.uc[0]);
          v_interleave_lo_i8(p.uc[2], p.uc[2], p.uc[2]);

          vmovu8u16(p.uc[0], p.uc[0]);
          vmovu8u16(p.uc[2], p.uc[2]);

          v_swizzle_i32(p.uc[1], p.uc[0], x86::shuffleImm(3, 3, 2, 2));
          v_swizzle_i32(p.uc[3], p.uc[2], x86::shuffleImm(3, 3, 2, 2));
          v_swizzle_i32(p.uc[0], p.uc[0], x86::shuffleImm(1, 1, 0, 0));
          v_swizzle_i32(p.uc[2], p.uc[2], x86::shuffleImm(1, 1, 0, 0));
        }
        break;
      }
    }
  }
  else if (p.isAlpha()) {
    // Cannot use scalar pixel in SIMD mode.
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kSA));

    switch (sFormat) {
      case BL_FORMAT_PRGB32: {
        x86::Xmm a0 = cc->newXmm("a");
        x86::Xmm a1 = cc->newXmm("aHi");
        sAdj.setSize(16);

        if (sAlignment >= 16) {
          v_loada_i128(a0, sAdj);
          sAdj.addOffset(16);
          v_loada_i128(a1, sAdj);
        }
        else {
          v_loadu_i128(a0, sAdj);
          sAdj.addOffset(16);
          v_loadu_i128(a1, sAdj);
        }

        v_srl_i32(a0, a0, 24);
        v_srl_i32(a1, a1, 24);
        v_packs_i32_i16(a0, a0, a1);

        if (blTestFlag(flags, PixelFlags::kPA)) {
          v_packs_i16_u8(a0, a0, a0);
          p.pa.init(a0);
        }
        else {
          p.ua.init(a0);
        }
        break;
      }

      case BL_FORMAT_A8: {
        x86::Xmm a = cc->newXmm("a");
        sAdj.setSize(8);

        if (blTestFlag(flags, PixelFlags::kPA)) {
          v_load_i64(a, sAdj);
          p.pa.init(a);
        }
        else {
          if (hasSSE4_1()) {
            v_load_i64_u8u16_(a, sAdj);
          }
          else {
            v_load_i64(a, sAdj);
            vmovu8u16(a, a);
          }
          p.ua.init(a);
        }
        break;
      }

      default:
        BL_NOT_REACHED();
    }
  }

  xSatisfyPixel(p, flags);
}

void PipeCompiler::xSatisfyPixel(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() != PixelType::kNone);
  BL_ASSERT(p.count() != 0);

  switch (p.type()) {
    case PixelType::kRGBA:
      _xSatisfyPixelRGBA(p, flags);
      break;
    case PixelType::kAlpha:
      _xSatisfyPixelAlpha(p, flags);
      break;
    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::_xSatisfyPixelRGBA(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kRGBA);
  BL_ASSERT(p.count() != 0);

  uint32_t i;

  // Quick reject if all flags were satisfied already or no flags were given.
  if ((!blTestFlag(flags, PixelFlags::kPC ) || !p.pc .empty()) &&
      (!blTestFlag(flags, PixelFlags::kUC ) || !p.uc .empty()) &&
      (!blTestFlag(flags, PixelFlags::kUA ) || !p.ua .empty()) &&
      (!blTestFlag(flags, PixelFlags::kUIA) || !p.uia.empty()))
    return;

  // Only fetch if we have already unpacked pixels. Wait otherwise as fetch flags may contain `PixelFlags::kUC`, which
  // is handled below. This is an optimization for cases where the caller wants packed RGBA and unpacked alpha.
  if (blTestFlag(flags, PixelFlags::kUA | PixelFlags::kUIA) && p.ua.empty() && !p.uc.empty()) {
    // Emit pshuflw/pshufhw sequence for every unpacked pixel.
    newXmmArray(p.ua, p.uc.size(), "a");

    v_swizzle_lo_i16(p.ua, p.uc, x86::shuffleImm(3, 3, 3, 3));
    v_swizzle_hi_i16(p.ua, p.ua, x86::shuffleImm(3, 3, 3, 3));
  }

  if (blTestFlag(flags, PixelFlags::kPC) && p.pc.empty()) {
    // Either PC or UC, but never both.
    BL_ASSERT(!p.uc.empty());
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kUC));

    // Emit pack sequence.
    p.pc.init(p.uc.even());
    rename(p.pc, "pc");
    v_packs_i16_u8(p.pc, p.uc.even(), p.uc.odd());
    p.uc.reset();
  }
  else if (blTestFlag(flags, PixelFlags::kUC) && p.uc.empty()) {
    // Emit unpack sequence.
    if (p.count() == 1) {
      cc->rename(p.pc[0], "c0");
      vmovu8u16(p.pc[0], p.pc[0]);

      p.uc.init(p.pc[0]);
      p.pc.reset();
    }
    else {
      p.uc._size = p.pc.size() * 2;
      for (i = 0; i < p.pc.size(); i++) {
        cc->rename(p.pc[i], "c%u", i * 2);

        p.uc[i * 2 + 0] = p.pc[i];
        p.uc[i * 2 + 1] = cc->newXmm("c%u", i * 2 + 1);

        xMovzxBW_LoHi(p.uc[i * 2 + 0], p.uc[i * 2 + 1], p.uc[i * 2 + 0]);
      }
      p.pc.reset();
    }
  }

  if (blTestFlag(flags, PixelFlags::kUA | PixelFlags::kUIA) && p.ua.empty()) {
    // This time we have to really fetch A8/IA8, if we haven't before.
    if (!p.uc.empty()) {
      newXmmArray(p.ua, p.uc.size(), "ua");
      v_swizzle_lo_i16(p.ua, p.uc, x86::shuffleImm(3, 3, 3, 3));
      if (p.count() > 1)
        v_swizzle_hi_i16(p.ua, p.ua, x86::shuffleImm(3, 3, 3, 3));
    }
    else {
      BL_ASSERT(!p.pc.empty());
      if (p.count() <= 2) {
        newXmmArray(p.ua, 1, "ua");
        v_swizzle_lo_i16(p.ua[0], p.pc[0], x86::shuffleImm(1, 1, 1, 1));
        v_srl_i16(p.ua[0], p.ua[0], 8);
      }
      else {
        newXmmArray(p.ua, p.pc.size() * 2, "ua");
        for (i = 0; i < p.pc.size(); i++)
          xExtractUnpackedAFromPackedARGB32_4(p.ua[i * 2], p.ua[i * 2 + 1], p.pc[i]);
      }
    }
  }

  if (blTestFlag(flags, PixelFlags::kUIA) && p.uia.empty()) {
    p.uia.init(p.ua);
    p.ua.reset();

    rename(p.uia, "uia");
    v_inv255_u16(p.uia, p.uia);
  }
}

void PipeCompiler::_xSatisfyPixelAlpha(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kAlpha);
  BL_ASSERT(p.count() != 0);

  // Scalar mode uses only SA.
  if (p.count() == 1) {
    BL_ASSERT( blTestFlag(flags, PixelFlags::kSA));
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kPA | PixelFlags::kUA));
    return;
  }

  if (blTestFlag(flags, PixelFlags::kPA) && p.pa.empty()) {
    // Either PA or UA, but never both.
    BL_ASSERT(!p.ua.empty());
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kUA));

    // Emit pack sequence.
    p.pa.init(p.ua.even());
    rename(p.pa, "pa");
    v_packs_i16_u8(p.pa, p.ua.even(), p.ua.odd());
    p.ua.reset();
  }
  else if (blTestFlag(flags, PixelFlags::kUA) && p.ua.empty()) {
    // Either PA or UA, but never both.
    BL_ASSERT(!p.pa.empty());
    BL_ASSERT(!blTestFlag(flags, PixelFlags::kPA));

    // Emit unpack sequence.
    if (p.count() <= 8) {
      cc->rename(p.pa[0], "a0");
      vmovu8u16(p.pa[0], p.pa[0]);

      p.ua.init(p.pa[0]);
      p.pa.reset();
    }
    else {
      p.uc._size = p.pa.size() * 2;
      for (uint32_t i = 0; i < p.pa.size(); i++) {
        cc->rename(p.pa[i], "c%u", i * 2);

        p.ua[i * 2 + 0] = p.pa[i];
        p.ua[i * 2 + 1] = cc->newXmm("a%u", i * 2 + 1);

        xMovzxBW_LoHi(p.ua[i * 2 + 0], p.ua[i * 2 + 1], p.ua[i * 2 + 0]);
      }
      p.pc.reset();
    }
  }

  if (blTestFlag(flags, PixelFlags::kUA | PixelFlags::kUIA)) {
    if (p.ua.empty()) {
      // TODO: A8 pipeline - finalize satisfy-pixel.
      BL_ASSERT(!true);
    }
  }
}

void PipeCompiler::xSatisfySolid(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() != PixelType::kNone);
  BL_ASSERT(p.count() != 0);

  switch (p.type()) {
    case PixelType::kRGBA:
      _xSatisfySolidRGBA(p, flags);
      break;
    case PixelType::kAlpha:
      _xSatisfySolidAlpha(p, flags);
      break;
    default:
      BL_NOT_REACHED();
  }
}

void PipeCompiler::_xSatisfySolidRGBA(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kRGBA);
  BL_ASSERT(p.count() != 0);

  if (blTestFlag(flags, PixelFlags::kPC) && p.pc.empty()) {
    BL_ASSERT(!p.uc.empty());
    newXmmArray(p.pc, 1, "pixel.pc");

    v_mov(p.pc[0], p.uc[0]);
    v_packs_i16_u8(p.pc[0], p.pc[0], p.pc[0]);
  }

  if (blTestFlag(flags, PixelFlags::kUC) && p.uc.empty()) {
    BL_ASSERT(!p.pc.empty());
    newXmmArray(p.uc, 1, "pixel.uc");

    vmovu8u16(p.uc[0], p.pc[0]);
  }

  if (blTestFlag(flags, PixelFlags::kUA) && p.ua.empty()) {
    newXmmArray(p.ua, 1, "pixel.ua");

    if (!p.uc.empty()) {
      v_swizzle_lo_i16(p.ua[0], p.uc[0], x86::shuffleImm(3, 3, 3, 3));
      v_swizzle_i32(p.ua[0], p.ua[0], x86::shuffleImm(1, 0, 1, 0));
    }
    else {
      v_swizzle_lo_i16(p.ua[0], p.pc[0], x86::shuffleImm(1, 1, 1, 1));
      v_swizzle_i32(p.ua[0], p.ua[0], x86::shuffleImm(1, 0, 1, 0));
      v_srl_i16(p.ua[0], p.ua[0], 8);
    }
  }

  if (blTestFlag(flags, PixelFlags::kUIA) && p.uia.empty()) {
    newXmmArray(p.uia, 1, "pixel.uia");

    if (!p.ua.empty()) {
      v_mov(p.uia[0], p.ua[0]);
    }
    else if (!p.uc.empty()) {
      v_swizzle_lo_i16(p.uia[0], p.uc[0], x86::shuffleImm(3, 3, 3, 3));
      v_swizzle_i32(p.uia[0], p.uia[0], x86::shuffleImm(1, 0, 1, 0));
    }
    else {
      v_swizzle_lo_i16(p.uia[0], p.pc[0], x86::shuffleImm(1, 1, 1, 1));
      v_swizzle_i32(p.uia[0], p.uia[0], x86::shuffleImm(1, 0, 1, 0));
      v_srl_i16(p.uia[0], p.uia[0], 8);
    }
    v_inv255_u16(p.uia[0], p.uia[0]);
  }
}

void PipeCompiler::_xSatisfySolidAlpha(Pixel& p, PixelFlags flags) noexcept {
  BL_ASSERT(p.type() == PixelType::kAlpha);
  BL_ASSERT(p.count() != 0);

  if (blTestFlag(flags, PixelFlags::kPA) && p.pa.empty()) {
    BL_ASSERT(!p.ua.empty());
    newXmmArray(p.pa, 1, "pixel.pa");
    v_packs_i16_u8(p.pa[0], p.ua[0], p.ua[0]);
  }

  // TODO: A8 pipeline - finalize solid-alpha.
}

void PipeCompiler::xFetchUnpackedA8_2x(const x86::Xmm& dst, uint32_t format, const x86::Mem& src1, const x86::Mem& src0) noexcept {
  x86::Mem m0 = src0;
  x86::Mem m1 = src1;

  m0.setSize(1);
  m1.setSize(1);

  if (format == BL_FORMAT_PRGB32) {
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

void PipeCompiler::xAssignUnpackedAlphaValues(Pixel& p, PixelFlags flags, x86::Xmm& vec) noexcept {
  BL_ASSERT(p.type() != PixelType::kNone);
  BL_ASSERT(p.count() != 0);

  x86::Xmm v0 = vec;

  if (p.isRGBA()) {
    switch (p.count()) {
      case 1: {
        v_swizzle_lo_i16(v0, v0, x86::shuffleImm(0, 0, 0, 0));
        p.uc.init(v0);
        break;
      }

      case 2: {
        v_interleave_lo_i16(v0, v0, v0);
        v_swizzle_i32(v0, v0, x86::shuffleImm(1, 1, 0, 0));
        p.uc.init(v0);
        break;
      }

      case 4: {
        x86::Xmm v1 = cc->newXmm();

        v_interleave_lo_i16(v0, v0, v0);
        v_swizzle_i32(v1, v0, x86::shuffleImm(3, 3, 2, 2));
        v_swizzle_i32(v0, v0, x86::shuffleImm(1, 1, 0, 0));
        p.uc.init(v0, v1);
        break;
      }

      case 8: {
        x86::Xmm v1 = cc->newXmm();
        x86::Xmm v2 = cc->newXmm();
        x86::Xmm v3 = cc->newXmm();

        v_interleave_hi_i16(v2, v0, v0);
        v_interleave_lo_i16(v0, v0, v0);

        v_swizzle_i32(v1, v0, x86::shuffleImm(3, 3, 2, 2));
        v_swizzle_i32(v0, v0, x86::shuffleImm(1, 1, 0, 0));
        v_swizzle_i32(v3, v2, x86::shuffleImm(3, 3, 2, 2));
        v_swizzle_i32(v2, v2, x86::shuffleImm(1, 1, 0, 0));

        p.uc.init(v0, v1, v2, v3);
        break;
      }

      default:
        BL_NOT_REACHED();
    }

    rename(p.uc, "uc");
  }
  else {
    switch (p.count()) {
      case 1: {
        BL_ASSERT(blTestFlag(flags, PixelFlags::kSA));
        x86::Gp sa = cc->newUInt32("sa");
        v_extract_u16(sa, vec, 0);
        p.sa = sa;
        break;
      }

      default: {
        p.ua.init(vec);
        rename(p.ua, "ua");
        break;
      }
    }
  }
}

void PipeCompiler::vFillAlpha(Pixel& p) noexcept {
  BL_ASSERT(p.type() != PixelType::kNone);

  if (!p.pc.empty()) vFillAlpha255B(p.pc, p.pc);
  if (!p.uc.empty()) vFillAlpha255W(p.uc, p.uc);
}

// BLPipeline::PipeCompiler - PixelFill
// ====================================

void PipeCompiler::xInlinePixelFillLoop(x86::Gp& dst, x86::Vec& src, x86::Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity) noexcept {
  BL_ASSERT(BLIntOps::isPowerOf2(itemSize));
  BL_ASSERT(itemSize <= 16u);

  uint32_t granularityInBytes = itemSize * itemGranularity;
  uint32_t mainStepInItems = mainLoopSize / itemSize;

  BL_ASSERT(BLIntOps::isPowerOf2(granularityInBytes));
  BL_ASSERT(mainStepInItems * itemSize == mainLoopSize);

  BL_ASSERT(mainLoopSize >= 16u);
  BL_ASSERT(mainLoopSize >= granularityInBytes);

  uint32_t j;

  // Granularity >= 16 Bytes
  // -----------------------

  if (granularityInBytes >= 16u) {
    Label L_End = cc->newLabel();

    // MainLoop
    // --------

    {
      Label L_MainLoop = cc->newLabel();
      Label L_MainSkip = cc->newLabel();

      cc->sub(i, mainStepInItems);
      cc->jc(L_MainSkip);

      cc->bind(L_MainLoop);
      cc->add(dst, mainLoopSize);
      cc->sub(i, mainStepInItems);
      for (j = 0; j < mainLoopSize; j += 16u)
        v_storeu_i128(x86::ptr(dst, int(j) - int(mainLoopSize)), src);
      cc->jnc(L_MainLoop);

      cc->bind(L_MainSkip);
      cc->add(i, mainStepInItems);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize * 2 > granularityInBytes) {
      Label L_TailLoop = cc->newLabel();
      cc->bind(L_TailLoop);
      for (j = 0; j < granularityInBytes; j += 16u)
        v_storeu_i128(x86::ptr(dst, int(j)), src);
      cc->add(dst, granularityInBytes);
      cc->sub(i, itemGranularity);
      cc->jnz(L_TailLoop);
    }
    else if (mainLoopSize * 2 == granularityInBytes) {
      for (j = 0; j < granularityInBytes; j += 16u)
        v_storeu_i128(x86::ptr(dst, int(j)), src);
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
      if (sizeShift)
        cc->shl(iptr, sizeShift);
      cc->add(iptr, dst);

      v_storeu_i128(x86::ptr(dst), src);
      cc->add(dst, 16);
      cc->and_(dst, -1 ^ int(alignPattern));

      cc->sub(iptr, dst);
      if (sizeShift)
        cc->shr(iptr, sizeShift);
      cc->jz(L_End);
    }

    // MainLoop
    // --------

    {
      Label L_MainLoop = cc->newLabel();
      Label L_MainSkip = cc->newLabel();

      cc->sub(i, mainStepInItems);
      cc->jc(L_MainSkip);

      cc->bind(L_MainLoop);
      cc->add(dst, mainLoopSize);
      cc->sub(i, mainStepInItems);
      for (j = 0; j < mainLoopSize; j += 16u)
        v_storea_i128(x86::ptr(dst, int(j) - int(mainLoopSize)), src);
      cc->jnc(L_MainLoop);

      cc->bind(L_MainSkip);
      cc->add(i, mainStepInItems);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailLoop = cc->newLabel();
      Label L_TailSkip = cc->newLabel();

      cc->sub(i, tailStepInItems);
      cc->jc(L_TailSkip);

      cc->bind(L_TailLoop);
      cc->add(dst, 16);
      cc->sub(i, tailStepInItems);
      v_storea_i128(x86::ptr(dst, -16), src);
      cc->jnc(L_TailLoop);

      cc->bind(L_TailSkip);
      cc->add(i, tailStepInItems);
      cc->jz(L_End);
    }
    else if (mainLoopSize >= 32) {
      cc->cmp(i, tailStepInItems);
      cc->jb(L_Finalize);

      v_storea_i128(x86::ptr(dst), src);
      cc->add(dst, 16);
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
      Label L_MainLoop = cc->newLabel();
      Label L_MainSkip = cc->newLabel();

      cc->sub(i, mainLoopSize);
      cc->jc(L_MainSkip);

      cc->bind(L_MainLoop);
      cc->add(dst, mainLoopSize);
      cc->sub(i, mainLoopSize);
      for (j = 0; j < mainLoopSize; j += 16u)
        v_storea_i128(x86::ptr(dst, int(j) - int(mainLoopSize)), src);
      cc->jnc(L_MainLoop);

      cc->bind(L_MainSkip);
      cc->add(i, mainLoopSize);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailLoop = cc->newLabel();
      Label L_TailSkip = cc->newLabel();

      cc->sub(i, 16);
      cc->jc(L_TailSkip);

      cc->bind(L_TailLoop);
      cc->add(dst, 16);
      cc->sub(i, 16);
      v_storea_i128(x86::ptr(dst, -16), src);
      cc->jnc(L_TailLoop);

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

void PipeCompiler::xInlinePixelCopyLoop(x86::Gp& dst, x86::Gp& src, x86::Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity, uint32_t format) noexcept {
  BL_ASSERT(BLIntOps::isPowerOf2(itemSize));
  BL_ASSERT(itemSize <= 16u);

  uint32_t granularityInBytes = itemSize * itemGranularity;
  uint32_t mainStepInItems = mainLoopSize / itemSize;

  BL_ASSERT(BLIntOps::isPowerOf2(granularityInBytes));
  BL_ASSERT(mainStepInItems * itemSize == mainLoopSize);

  BL_ASSERT(mainLoopSize >= 16u);
  BL_ASSERT(mainLoopSize >= granularityInBytes);

  x86::Xmm t0 = cc->newXmm("t0");
  x86::Xmm fillMask;

  if (format == BL_FORMAT_XRGB32)
    fillMask = constAsXmm(&blCommonTable.i128_FF000000FF000000);

  // Granularity >= 16 Bytes
  // -----------------------

  if (granularityInBytes >= 16u) {
    Label L_End = cc->newLabel();

    // MainLoop
    // --------

    {
      Label L_MainLoop = cc->newLabel();
      Label L_MainSkip = cc->newLabel();
      int ptrOffset = -int(mainLoopSize);

      cc->sub(i, mainStepInItems);
      cc->jc(L_MainSkip);

      cc->bind(L_MainLoop);
      cc->add(dst, mainLoopSize);
      cc->add(src, mainLoopSize);
      cc->sub(i, mainStepInItems);
      _xInlineMemCopySequenceXmm(x86::ptr(dst, ptrOffset), false, x86::ptr(src, ptrOffset), false, mainLoopSize, fillMask);
      cc->jnc(L_MainLoop);

      cc->bind(L_MainSkip);
      cc->add(i, mainStepInItems);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize * 2 > granularityInBytes) {
      Label L_TailLoop = cc->newLabel();
      cc->bind(L_TailLoop);
      _xInlineMemCopySequenceXmm(x86::ptr(dst), false, x86::ptr(src), false, granularityInBytes, fillMask);
      cc->add(dst, granularityInBytes);
      cc->add(src, granularityInBytes);
      cc->sub(i, itemGranularity);
      cc->jnz(L_TailLoop);
    }
    else if (mainLoopSize * 2 == granularityInBytes) {
      _xInlineMemCopySequenceXmm(x86::ptr(dst), false, x86::ptr(src), false, granularityInBytes, fillMask);
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
      Label L_MainLoop = cc->newLabel();
      Label L_MainSkip = cc->newLabel();

      cc->sub(i, mainStepInItems);
      cc->jc(L_MainSkip);

      cc->bind(L_MainLoop);
      cc->add(dst, mainLoopSize);
      cc->add(src, mainLoopSize);
      cc->sub(i, mainStepInItems);
      int ptrOffset = - int(mainLoopSize);
      _xInlineMemCopySequenceXmm(x86::ptr(dst, ptrOffset), true, x86::ptr(src, ptrOffset), false, mainLoopSize, fillMask);
      cc->jnc(L_MainLoop);

      cc->bind(L_MainSkip);
      cc->add(i, mainStepInItems);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailLoop = cc->newLabel();
      Label L_TailSkip = cc->newLabel();

      cc->sub(i, tailStepInItems);
      cc->jc(L_TailSkip);

      cc->bind(L_TailLoop);
      cc->add(dst, 16);
      cc->add(src, 16);
      cc->sub(i, tailStepInItems);
      _xInlineMemCopySequenceXmm(x86::ptr(dst, -16), true, x86::ptr(src, -16), false, 16, fillMask);
      cc->jnc(L_TailLoop);

      cc->bind(L_TailSkip);
      cc->add(i, tailStepInItems);
      cc->jz(L_End);
    }
    else if (mainLoopSize >= 32) {
      cc->cmp(i, tailStepInItems);
      cc->jb(L_Finalize);

      _xInlineMemCopySequenceXmm(x86::ptr(dst), true, x86::ptr(src), false, 16, fillMask);
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
      Label L_MainLoop = cc->newLabel();
      Label L_MainSkip = cc->newLabel();

      cc->sub(i, mainLoopSize);
      cc->jc(L_MainSkip);

      cc->bind(L_MainLoop);
      _xInlineMemCopySequenceXmm(x86::ptr(dst), true, x86::ptr(src), false, mainLoopSize, fillMask);
      cc->add(dst, mainLoopSize);
      cc->add(src, mainLoopSize);
      cc->sub(i, mainLoopSize);
      cc->jnc(L_MainLoop);

      cc->bind(L_MainSkip);
      cc->add(i, mainLoopSize);
      cc->jz(L_End);
    }

    // TailLoop / TailSequence
    // -----------------------

    if (mainLoopSize > 32) {
      Label L_TailLoop = cc->newLabel();
      Label L_TailSkip = cc->newLabel();

      cc->sub(i, 16);
      cc->jc(L_TailSkip);

      cc->bind(L_TailLoop);
      _xInlineMemCopySequenceXmm(x86::ptr(dst), true, x86::ptr(src), false, 16, fillMask);
      cc->add(dst, 16);
      cc->add(src, 16);
      cc->sub(i, 16);
      cc->jnc(L_TailLoop);

      cc->bind(L_TailSkip);
      cc->add(i, 16);
      cc->jz(L_End);
    }
    else if (mainLoopSize >= 32) {
      cc->cmp(i, 16);
      cc->jb(L_Finalize);

      _xInlineMemCopySequenceXmm(x86::ptr(dst), true, x86::ptr(src), false, 16, fillMask);
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
      _xInlineMemCopySequenceXmm(x86::ptr(dst, -16), false, x86::ptr(src, -16), false, 16, fillMask);
    }

    cc->bind(L_End);
    return;
  }
}

void PipeCompiler::_xInlineMemCopySequenceXmm(
  const x86::Mem& dPtr, bool dstAligned,
  const x86::Mem& sPtr, bool srcAligned, uint32_t numBytes, const x86::Vec& fillMask) noexcept {

  x86::Mem dAdj(dPtr);
  x86::Mem sAdj(sPtr);
  VecArray t;

  uint32_t fetchInst = hasAVX() ? x86::Inst::kIdVmovaps : x86::Inst::kIdMovaps;
  uint32_t storeInst = hasAVX() ? x86::Inst::kIdVmovaps : x86::Inst::kIdMovaps;

  if (!srcAligned) fetchInst = hasAVX()  ? x86::Inst::kIdVlddqu  :
                               hasSSE3() ? x86::Inst::kIdLddqu   : x86::Inst::kIdMovups;
  if (!dstAligned) storeInst = hasAVX()  ? x86::Inst::kIdVmovups : x86::Inst::kIdMovups;

  uint32_t n = numBytes / 16;
  uint32_t limit = 2;
  newXmmArray(t, blMin(n, limit), "t");

  do {
    uint32_t a, b = blMin<uint32_t>(n, limit);

    if (hasAVX() && fillMask.isValid()) {
      // Shortest code for this use case. AVX allows to read from unaligned
      // memory, so if we use VEC instructions we are generally safe here.
      for (a = 0; a < b; a++) {
        v_or(t[a], fillMask, sAdj);
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
          v_or(t[a], t[a], fillMask);

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
