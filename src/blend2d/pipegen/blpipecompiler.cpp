// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../pipegen/blcompoppart_p.h"
#include "../pipegen/blfetchpart_p.h"
#include "../pipegen/blfetchgradientpart_p.h"
#include "../pipegen/blfetchpixelptrpart_p.h"
#include "../pipegen/blfetchsolidpart_p.h"
#include "../pipegen/blfetchpatternpart_p.h"
#include "../pipegen/blfillpart_p.h"
#include "../pipegen/blpipecompiler_p.h"

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::PipeCompiler - Construction / Destruction]
// ============================================================================

PipeCompiler::PipeCompiler(x86::Compiler* cc, const asmjit::x86::Features& features) noexcept
  : cc(cc),
    _features(features) { reset(); }
PipeCompiler::~PipeCompiler() noexcept {}

// ============================================================================
// [BLPipeGen::PipeCompiler - Reset]
// ============================================================================

void PipeCompiler::reset() noexcept {
  _availableRegs.reset();
  _persistentRegs.reset();
  _temporaryRegs.reset();

  _funcNode = nullptr;
  _funcInit = nullptr;
  _funcEnd = nullptr;

  _commonTableOff = 128;
  _commonTablePtr.reset();
  JitUtils::resetVarStruct(_constantsXmm);

  // These are always overwritten by `compileFunc()`, reset for safety.
  _ctxData.reset();
  _fillData.reset();
  _fetchData.reset();

  _ctxDataOffset = 0;
  _fillDataOffset = 0;
  _fetchDataOffset = 0;

  updateOptLevel();
}

// ============================================================================
// [BLPipeGen::PipeCompiler - Optimization Level]
// ============================================================================

void PipeCompiler::updateOptLevel() noexcept {
  uint32_t optLevel = kOptLevel_X86_SSE2;

  // TODO: [PIPEGEN] No AVX2 use at the moment.
  if (_features.hasSSE3()) optLevel = kOptLevel_X86_SSE3;
  if (_features.hasSSSE3()) optLevel = kOptLevel_X86_SSSE3;
  if (_features.hasSSE4_1()) optLevel = kOptLevel_X86_SSE4_1;
  if (_features.hasSSE4_2()) optLevel = kOptLevel_X86_SSE4_2;
  if (_features.hasAVX()) optLevel = kOptLevel_X86_AVX;

  _optLevel = optLevel;
}

// ============================================================================
// [BLPipeGen::PipeCompiler - BeginFunction / EndFunction]
// ============================================================================

void PipeCompiler::beginFunction() noexcept {
  // Setup constants first.
  _availableRegs[x86::Reg::kGroupGp  ] = cc->gpCount() - kReservedGpRegs;
  _availableRegs[x86::Reg::kGroupMm  ] = 8             - kReservedMmRegs;
  _availableRegs[x86::Reg::kGroupVec ] = cc->gpCount() - kReservedVecRegs;
  _availableRegs[x86::Reg::kGroupKReg] = 8;

  // Function prototype and arguments.
  _funcNode = cc->addFunc(asmjit::FuncSignatureT<uint32_t, void*, void*, void*>(asmjit::CallConv::kIdHostCDecl));
  _funcInit = cc->cursor();
  _funcEnd = _funcNode->endNode()->prev();

  if (optLevel() >= kOptLevel_X86_AVX)
    _funcNode->frame().setAvxEnabled();

  _ctxData = cc->newIntPtr("ctxData");
  _fillData = cc->newIntPtr("fillData");
  _fetchData = cc->newIntPtr("fetchData");

  cc->setArg(0, _ctxData);
  cc->setArg(1, _fillData);
  cc->setArg(2, _fetchData);
}

void PipeCompiler::endFunction() noexcept {
  // All pipelines return zero, which means `BL_SUCCESS`.
  x86::Gp ret = cc->newU32("ret");
  cc->xor_(ret, ret);
  cc->ret(ret);

  // Finalize the pipeline function.
  cc->endFunc();
}

// ============================================================================
// [BLPipeGen::PipeCompiler - Parts]
// ============================================================================

FillPart* PipeCompiler::newFillPart(uint32_t fillType, FetchPart* dstPart, CompOpPart* compOpPart) noexcept {
  if (fillType == BL_PIPE_FILL_TYPE_BOX_AA)
    return newPartT<FillBoxAAPart>(fillType, dstPart->as<FetchPixelPtrPart>(), compOpPart);

  if (fillType == BL_PIPE_FILL_TYPE_BOX_AU)
    return newPartT<FillBoxAUPart>(fillType, dstPart->as<FetchPixelPtrPart>(), compOpPart);

  if (fillType == BL_PIPE_FILL_TYPE_ANALYTIC)
    return newPartT<FillAnalyticPart>(fillType, dstPart->as<FetchPixelPtrPart>(), compOpPart);

  return nullptr;
}

FetchPart* PipeCompiler::newFetchPart(uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept {
  if (fetchType == BL_PIPE_FETCH_TYPE_SOLID)
    return newPartT<FetchSolidPart>(fetchType, fetchPayload, format);

  if (fetchType >= BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_FIRST && fetchType <= BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_LAST)
    return newPartT<FetchLinearGradientPart>(fetchType, fetchPayload, format);

  if (fetchType >= BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_FIRST && fetchType <= BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_LAST)
    return newPartT<FetchRadialGradientPart>(fetchType, fetchPayload, format);

  if (fetchType >= BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL_FIRST && fetchType <= BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL_LAST)
    return newPartT<FetchConicalGradientPart>(fetchType, fetchPayload, format);

  if (fetchType >= BL_PIPE_FETCH_TYPE_PATTERN_SIMPLE_FIRST && fetchType <= BL_PIPE_FETCH_TYPE_PATTERN_SIMPLE_LAST)
    return newPartT<FetchSimplePatternPart>(fetchType, fetchPayload, format);

  if (fetchType >= BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_FIRST && fetchType <= BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_LAST)
    return newPartT<FetchAffinePatternPart>(fetchType, fetchPayload, format);

  if (fetchType == BL_PIPE_FETCH_TYPE_PIXEL_PTR)
    return newPartT<FetchPixelPtrPart>(fetchType, fetchPayload, format);

  return nullptr;
}

CompOpPart* PipeCompiler::newCompOpPart(uint32_t compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept {
  return newPartT<CompOpPart>(compOp, dstPart, srcPart);
}

// ============================================================================
// [BLPipeGen::PipeCompiler - Init]
// ============================================================================

void PipeCompiler::initPipeline(PipePart* root) noexcept {
  if (_ctxDataOffset != 0) cc->add(_ctxData, _ctxDataOffset);
  if (_fillDataOffset != 0) cc->add(_fillData, _fillDataOffset);
  if (_fetchDataOffset != 0) cc->add(_fetchData, _fetchDataOffset);

  root->preparePart();
  onPreInitPart(root);
  onPostInitPart(root);
}

void PipeCompiler::onPreInitPart(PipePart* part) noexcept {
  PipePart** children = part->children();
  uint32_t count = part->childrenCount();

  // Mark so `onPreInitPart()` is called only once for this `part`.
  part->_flags |= PipePart::kFlagPreInitDone;

  // Collect the register usage of the part.
  _persistentRegs.add(part->_persistentRegs);
  _persistentRegs.add(part->_spillableRegs);
  _temporaryRegs.max(part->_temporaryRegs);

  for (uint32_t i = 0; i < count; i++) {
    PipePart* child = children[i];
    if (!(child->flags() & PipePart::kFlagPreInitDone))
      onPreInitPart(child);
  }
}

void PipeCompiler::onPostInitPart(PipePart* part) noexcept {
  PipePart** children = part->children();
  uint32_t count = part->childrenCount();

  // Mark so `onPostInitPart()` is called only once for this `part`.
  part->_flags |= PipePart::kFlagPostInitDone;

  // Mark `hasLow` registers in case that the register usage is greater than
  // the total number of registers available. This is per-part only, not global.
  for (uint32_t i = 0; i < kNumVirtGroups; i++) {
    if (_persistentRegs[i] > _availableRegs[i]) {
      part->_hasLowRegs[i] = true;
      _persistentRegs[i] -= part->_spillableRegs[i];
    }
  }

  for (uint32_t i = 0; i < count; i++) {
    PipePart* child = children[i];
    if (!(child->flags() & PipePart::kFlagPostInitDone))
      onPostInitPart(child);
  }
}

// ============================================================================
// [BLPipeGen::PipeCompiler - Constants]
// ============================================================================

void PipeCompiler::_initCommonTablePtr() noexcept {
  const void* global = &blCommonTable;

  if (!_commonTablePtr.isValid()) {
    asmjit::BaseNode* prevNode = cc->setCursor(_funcInit);
    _commonTablePtr = cc->newIntPtr("commonTablePtr");

    cc->alloc(_commonTablePtr);
    cc->mov(_commonTablePtr, (int64_t)global + _commonTableOff);

    _funcInit = cc->setCursor(prevNode);
  }
}

x86::Mem PipeCompiler::constAsMem(const void* p) noexcept {
  // Make sure we are addressing a constant from the `blCommonTable` constant pool.
  const void* global = &blCommonTable;
  BL_ASSERT((uintptr_t)p >= (uintptr_t)global &&
            (uintptr_t)p <  (uintptr_t)global + sizeof(BLCommonTable));

  if (asmjit::ArchInfo::kIdHost == asmjit::ArchInfo::kIdX86) {
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
    ""
  };

  int constIndex = -1;

  if      (p == blCommonTable.i128_0000000000000000   ) constIndex = 0; // Required if the CPU doesn't have SSE4.1.
  else if (p == blCommonTable.i128_0080008000800080) constIndex = 1; // Required by `xDiv255()` and friends.
  else if (p == blCommonTable.i128_0101010101010101) constIndex = 2; // Required by `xDiv255()` and friends.

  if (constIndex == -1 || _persistentRegs[x86::Reg::kGroupVec] + _temporaryRegs[x86::Reg::kGroupVec] > _availableRegs[x86::Reg::kGroupVec]) {
    // TODO: [PIPEGEN] This works, but it's really nasty!
    x86::Mem m = constAsMem(p);
    return reinterpret_cast<x86::Xmm&>(m);
  }

  x86::Xmm& xmm = _constantsXmm[constIndex];
  if (!xmm.isValid()) {
    xmm = cc->newXmm(xmmNames[constIndex]);

    if (constIndex == 0) {
      asmjit::BaseNode* prevNode = cc->setCursor(_funcInit);
      vzerops(xmm);
      _funcInit = cc->setCursor(prevNode);
    }
    else {
      // `constAsMem()` may call `_initCommonTablePtr()` for the very first time.
      // We cannot inject any code before `constAsMem()` returns.
      x86::Mem m = constAsMem(p);

      asmjit::BaseNode* prevNode = cc->setCursor(_funcInit);
      vloadps_128a(xmm, m);
      _funcInit = cc->setCursor(prevNode);
    }

    _persistentRegs[x86::Reg::kGroupVec]++;
  }

  return xmm;
}

// ============================================================================
// [BLPipeGen::PipeCompiler - Stack]
// ============================================================================

x86::Mem PipeCompiler::tmpStack(uint32_t size) noexcept {
  BL_ASSERT(blIsPowerOf2(size));

  // TODO: [PIPEGEN] We don't use greater right now.
  BL_ASSERT(size <= 16);

  if (!_tmpStack.baseId())
    _tmpStack = cc->newStack(size, size, "tmpStack");
  return _tmpStack;
}

// ============================================================================
// [BLPipeGen::PipeCompiler - Emit]
// ============================================================================

static constexpr uint32_t signatureOfXmmYmmZmm[] = {
  x86::Xmm::kSignature,
  x86::Ymm::kSignature,
  x86::Zmm::kSignature
};

static inline int shuf32ToShuf64(int imm) noexcept {
  int imm0 = (imm     ) & 1;
  int imm1 = (imm >> 1) & 1;
  return int(x86::Predicate::shuf(imm1 * 2, imm1 * 2 + 1, imm0 * 2, imm0 * 2 + 1));
}

static inline void fixVecSignature(Operand_& op, uint32_t signature) noexcept {
  if (x86::Reg::isVec(op) && op.signature() > signature)
    op.setSignature(signature);
}

static inline bool isSameReg(const Operand_& a, const Operand_& b) noexcept {
  return a.id() == b.id() && a.id() && b.id();
}

void PipeCompiler::iemit2(uint32_t instId, const Operand_& op1, int imm) noexcept {
  cc->emit(instId, op1, imm);
}

void PipeCompiler::iemit2(uint32_t instId, const Operand_& op1, const Operand_& op2) noexcept {
  cc->emit(instId, op1, op2);
}

void PipeCompiler::iemit3(uint32_t instId, const Operand_& op1, const Operand_& op2, int imm) noexcept {
  cc->emit(instId, op1, op2, imm);
}

void PipeCompiler::vemit_xmov(const Operand_& dst, const Operand_& src, uint32_t width) noexcept {
  if (src.isMem() || !isSameReg(dst, src)) {
    uint32_t instId = x86::Inst::kIdMovaps;

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
    uint32_t signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
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
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVmovdqa, x86::Inst::kIdMovdqa);
        break;
      }

      case kIntrin2Vmovu8u16: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpmovzxbw, x86::Inst::kIdPmovzxbw);
          break;
        }

        vemit_xmov(dst, src, 8);
        vunpackli8(dst, dst, constAsXmm(blCommonTable.i128_0000000000000000));
        return;
      }

      case kIntrin2Vmovu8u32: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpmovzxbd, x86::Inst::kIdPmovzxbd);
          break;
        }

        vemit_xmov(dst, src, 4);
        vunpackli8(dst, dst, constAsXmm(blCommonTable.i128_0000000000000000));
        vunpackli16(dst, dst, constAsXmm(blCommonTable.i128_0000000000000000));
        return;
      }

      case kIntrin2Vmovu16u32: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpmovzxwd, x86::Inst::kIdPmovzxwd);
          break;
        }

        vemit_xmov(dst, src, 8);
        vunpackli16(dst, dst, constAsXmm(blCommonTable.i128_0000000000000000));
        return;
      }

      case kIntrin2Vabsi8: {
        if (hasSSSE3()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpabsb, x86::Inst::kIdPabsb);
          break;
        }

        if (isSameReg(dst, src)) {
          x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");
          vzeropi(tmp);
          vsubi8(tmp, tmp, dst);
          vminu8(dst, dst, tmp);
        }
        else {
          vzeropi(dst);
          vsubi8(dst, dst, src);
          vminu8(dst, dst, src);
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
          vzeropi(tmp);
          vsubi16(tmp, tmp, dst);
          vmaxi16(dst, dst, tmp);
        }
        else {
          vzeropi(dst);
          vsubi16(dst, dst, src);
          vmaxi16(dst, dst, src);
        }
        return;
      }

      case kIntrin2Vabsi32: {
        if (hasSSSE3()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpabsd, x86::Inst::kIdPabsd);
          break;
        }

        x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");

        vmov(tmp, src);
        vsrai32(tmp, tmp, 31);
        vxor(dst, src, tmp);
        vsubi32(dst, dst, tmp);
        return;
      }

      case kIntrin2Vabsi64: {
        x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");

        vduphi32(tmp, src);
        vsrai32(tmp, tmp, 31);
        vxor(dst, src, tmp);
        vsubi32(dst, dst, tmp);
        return;
      }

      case kIntrin2Vinv255u16: {
        Operand u16_255 = constAsXmm(blCommonTable.i128_00FF00FF00FF00FF);

        if (hasAVX() || isSameReg(dst, src)) {
          vxor(dst, src, u16_255);
        }
        else {
          vmov(dst, u16_255);
          vxor(dst, dst, src);
        }
        return;
      }

      case kIntrin2Vinv256u16: {
        x86::Vec u16_0100 = constAsXmm(blCommonTable.i128_0100010001000100);

        if (!isSameReg(dst, src)) {
          vmov(dst, u16_0100);
          vsubi16(dst, dst, src);
        }
        else if (hasSSSE3()) {
          vsubi16(dst, dst, u16_0100);
          vabsi16(dst, dst);
        }
        else {
          vxor(dst, dst, constAsXmm(blCommonTable.i128_FFFFFFFFFFFFFFFF));
          vaddi16(dst, dst, u16_0100);
        }
        return;
      }

      case kIntrin2Vinv255u32: {
        Operand u32_255 = constAsXmm(blCommonTable.i128_000000FF000000FF);

        if (hasAVX() || isSameReg(dst, src)) {
          vxor(dst, src, u32_255);
        }
        else {
          vmov(dst, u32_255);
          vxor(dst, dst, src);
        }
        return;
      }

      case kIntrin2Vinv256u32: {
        BL_ASSERT(!"Implemented");
        // TODO: [PIPEGEN]
        return;
      }

      case kIntrin2Vduplpd: {
        if (hasSSE3())
          vmovduplpd_(dst, src);
        else if (hasAVX())
          vunpacklpd(dst, src, src);
        else if (isSameReg(dst, src))
          vunpacklpd(dst, dst, src);
        else
          vdupli64(dst, src);
        return;
      }

      case kIntrin2Vduphpd: {
        if (hasAVX())
          vunpackhpd(dst, src, src);
        if (isSameReg(dst, src))
          vunpackhpd(dst, dst, src);
        else
          vduphi64(dst, src);
        return;
      }

      default:
        BL_ASSERT(!"Invalid intrinsic at vemit_vv_vv()");
    }
  }

  // Single instruction.
  uint32_t instId = hasAVX() ? PackedInst::avxId(packedId)
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

void PipeCompiler::vemit_vvi_vi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, int imm) noexcept {
  // Intrinsics support.
  if (PackedInst::isIntrin(packedId)) {
    switch (PackedInst::intrinId(packedId)) {
      case kIntrin2iVswizps:
        if (isSameReg(dst_, src_) || hasAVX())
          vshufps(dst_, src_, src_, imm);
        else
          vswizi32(dst_, src_, imm);
        return;

      case kIntrin2iVswizpd:
        if (isSameReg(dst_, src_) || hasAVX())
          vshufpd(dst_, src_, src_, imm);
        else
          vswizi32(dst_, src_, shuf32ToShuf64(imm));
        return;

      default:
        BL_ASSERT(!"Invalid intrinsic at vemit_vvi_vi()");
    }
  }

  // Instruction support.
  Operand dst(dst_);
  Operand src(src_);

  if (PackedInst::width(packedId) < PackedInst::kWidthZ) {
    uint32_t signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst, signature);
    fixVecSignature(src, signature);
  }

  if (hasAVX()) {
    uint32_t instId = PackedInst::avxId(packedId);
    cc->emit(instId, dst, src, imm);
  }
  else {
    uint32_t instId = PackedInst::sseId(packedId);
    if (!isSameReg(dst, src))
      cc->emit(x86::Inst::kIdMovaps, dst, src);
    cc->emit(instId, dst, imm);
  }
}

void PipeCompiler::vemit_vvi_vi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, int imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  while (dstIndex < dstCount) {
    vemit_vvi_vi(packedId, dst_[dstIndex], src_, imm);
    dstIndex++;
  }
}

void PipeCompiler::vemit_vvi_vi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, int imm) noexcept {
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

void PipeCompiler::vemit_vvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, int imm) noexcept {
  Operand dst(dst_);
  Operand src(src_);

  if (PackedInst::width(packedId) < PackedInst::kWidthZ) {
    uint32_t signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst, signature);
    fixVecSignature(src, signature);
  }

  uint32_t instId = hasAVX() ? PackedInst::avxId(packedId)
                             : PackedInst::sseId(packedId);
  cc->emit(instId, dst, src, imm);
}

void PipeCompiler::vemit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, int imm) noexcept {
  uint32_t dstIndex = 0;
  uint32_t dstCount = dst_.size();

  while (dstIndex < dstCount) {
    vemit_vvi_vvi(packedId, dst_[dstIndex], src_, imm);
    dstIndex++;
  }
}

void PipeCompiler::vemit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, int imm) noexcept {
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
    uint32_t signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
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
          vswapi64(dst_, src1_);
          return;
        }

        // Dst is Src2 Case:
        //   dst'.u64[0] = src1.u64[1];
        //   dst'.u64[1] = dst_.u64[0];
        if (isSameReg(dst_, src2_) && !hasAVX()) {
          if (hasSSSE3()) {
            valignr8_(dst_, dst_, src1_, 8);
          }
          else {
            vshufpd(dst_, dst_, src1_, x86::Predicate::shuf(1, 0));
            vswapi64(dst_, dst_);
          }
          return;
        }

        // Common Case:
        //   dst'.u64[0] = src1.u64[1];
        //   dst'.u64[1] = src2.u64[0];
        vshufpd(dst_, src1_, src2_, x86::Predicate::shuf(0, 1));
        return;
      }

      case kIntrin3Vcombhld64: {
        // Swap Case:
        //   dst'.d64[0] = src_.d64[1];
        //   dst'.d64[1] = src_.d64[0];
        if (isSameReg(src1_, src2_)) {
          vswappd(dst_, src1_);
          return;
        }

        // Dst is Src2 Case:
        //   dst'.d64[0] = src1.d64[1];
        //   dst'.d64[1] = dst_.d64[0];
        if (isSameReg(dst_, src2_) && !hasAVX()) {
          vshufpd(dst_, dst_, src1_, x86::Predicate::shuf(1, 0));
          vswappd(dst_, dst_);
          return;
        }

        // Common Case:
        //   dst'.d64[0] = src1.d64[1];
        //   dst'.d64[1] = src2.d64[0];
        vshufpd(dst_, src1_, src2_, x86::Predicate::shuf(0, 1));
        return;
      }

      case kIntrin3Vminu16: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpminuw, x86::Inst::kIdPminuw);
          break;
        }

        if (isSameReg(src1, src2)) {
          vmov(dst, src1);
          return;
        }

        if (isSameReg(dst, src2))
          std::swap(src1, src2);

        x86::Xmm tmp = cc->newXmm("@tmp");
        vsubsu16(tmp, src1, src2);
        vsubi16(dst, src1, tmp);
        return;
      }

      case kIntrin3Vmaxu16: {
        if (hasSSE4_1()) {
          packedId = PackedInst::packAvxSse(x86::Inst::kIdVpmaxuw, x86::Inst::kIdPmaxuw);
          break;
        }

        if (isSameReg(src1, src2)) {
          vmov(dst, src1);
          return;
        }

        if (isSameReg(dst, src2))
          std::swap(src1, src2);

        vsubsu16(dst, src1, src2);
        vaddi16(dst, dst, src2);
        return;
      }

      case kIntrin3Vmulu64x32: {
        if (isSameReg(dst, src1)) {
          x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");

          vswizi32(tmp, dst, x86::Predicate::shuf(2, 3, 0, 1));
          vmulxllu32(dst, dst, src2);
          vmulxllu32(tmp, tmp, src2);
          vslli64(tmp, tmp, 32);
          vaddi64(dst, dst, tmp);
        }
        else if (isSameReg(dst, src2)) {
          x86::Vec tmp = cc->newSimilarReg(dst.as<x86::Vec>(), "@tmp");

          vswizi32(tmp, src1, x86::Predicate::shuf(2, 3, 0, 1));
          vmulxllu32(tmp, tmp, dst);
          vmulxllu32(dst, dst, src1);
          vslli64(tmp, tmp, 32);
          vaddi64(dst, dst, tmp);
        }
        else {
          vswizi32(dst, src1, x86::Predicate::shuf(2, 3, 0, 1));
          vmulxllu32(dst, dst, src2);
          vmulxllu32(src1, src1, src2);
          vslli64(dst, dst, 32);
          vaddi64(dst, dst, src1);
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
            vswappd(tmp, dst);
            vaddpd(dst, dst, tmp);
          }
          else {
            // dst = haddpd(src1, src1);
            vswappd(dst, src1);
            vaddpd(dst, dst, src1);
          }
        }
        else {
          x86::Xmm tmp = cc->newXmmPd("@tmp");
          // dst = haddpd(src1, src2);
          vunpackhpd(tmp, src1, src2);
          vunpacklpd(dst, src1, src2);
          vaddpd(dst, dst, tmp);
        }
        return;
      }

      default:
        BL_ASSERT(!"Invalid intrinsic at vemit_vvv_vv()");
    }
  }

  // Single instruction.
  if (hasAVX()) {
    uint32_t instId = PackedInst::avxId(packedId);
    cc->emit(instId, dst, src1, src2);
  }
  else {
    uint32_t instId = PackedInst::sseId(packedId);
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

void PipeCompiler::vemit_vvvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, int imm) noexcept {
  Operand dst(dst_);
  Operand src1(src1_);
  Operand src2(src2_);

  if (PackedInst::width(packedId) < PackedInst::kWidthZ) {
    uint32_t signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst , signature);
    fixVecSignature(src1, signature);
    fixVecSignature(src2, signature);
  }

  if (hasAVX()) {
    uint32_t instId = PackedInst::avxId(packedId);
    cc->emit(instId, dst, src1, src2, imm);
  }
  else {
    uint32_t instId = PackedInst::sseId(packedId);
    if (!isSameReg(dst, src1))
      cc->emit(x86::Inst::kIdMovaps, dst, src1);
    cc->emit(instId, dst, src2, imm);
  }
}

void PipeCompiler::vemit_vvvi_vvi(uint32_t packedId, const OpArray& dst, const Operand_& src1, const OpArray& src2, int imm) noexcept {
  vemit_vvvi_vvi(packedId, dst, OpArray(src1), src2, imm);
}

void PipeCompiler::vemit_vvvi_vvi(uint32_t packedId, const OpArray& dst, const OpArray& src1, const Operand_& src2, int imm) noexcept {
  vemit_vvvi_vvi(packedId, dst, src1, OpArray(src2), imm);
}

void PipeCompiler::vemit_vvvi_vvi(uint32_t packedId, const OpArray& dst, const OpArray& src1, const OpArray& src2, int imm) noexcept {
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
    uint32_t signature = signatureOfXmmYmmZmm[PackedInst::width(packedId)];
    fixVecSignature(dst , signature);
    fixVecSignature(src1, signature);
    fixVecSignature(src2, signature);
    fixVecSignature(src3, signature);
  }

  if (hasAVX()) {
    uint32_t instId = PackedInst::avxId(packedId);
    cc->emit(instId, dst, src1, src2, src3);
  }
  else {
    uint32_t instId = PackedInst::sseId(packedId);
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

// ============================================================================
// [BLPipeGen::PipeCompiler - Emit - 'Q' Vector Instructions (64-bit MMX)]
// ============================================================================

void PipeCompiler::qmulu64u32(const x86::Mm& dst, const x86::Mm& src1, const x86::Mm& src2) noexcept {
  if (isSameReg(dst, src1)) {
    x86::Mm t = cc->newMm("@t");

    qswapi32(t, dst);
    qmulxllu32(t, src2);
    qslli64(t, 32);
    qmulxllu32(dst, src2);
    qaddi64(dst, t);
  }
  else if (isSameReg(dst, src2)) {
    x86::Mm t = cc->newMm("@t");

    qswapi32(t, src1);
    qmulxllu32(t, dst);
    qslli64(t, 32);
    qmulxllu32(dst, src1);
    qaddi64(dst, t);
  }
  else {
    qswapi32(dst, src1);
    qmulxllu32(dst, src2);
    qmulxllu32(src1, src2);
    qslli64(dst, 32);
    qaddi64(dst, src1);
  }
}

// ============================================================================
// [BLPipeGen::PipeCompiler - Fetch Helpers]
// ============================================================================

void PipeCompiler::xFetchARGB32_1x(PixelARGB& p, uint32_t flags, const x86::Mem& sMem, uint32_t sAlignment) noexcept {
  if (flags & PixelARGB::kAny) {
    newXmmArray(p.pc, 1, "c");
    vloadi32(p.pc[0], sMem);
  }

  xSatisfyARGB32_1x(p, flags);
}

void PipeCompiler::xFetchARGB32_4x(PixelARGB& p, uint32_t flags, const x86::Mem& sMem, uint32_t sAlignment) noexcept {
  x86::Mem sAdj(sMem);

  if (flags & PixelARGB::kPC) {
    newXmmArray(p.pc, 1, "c");

    sAdj.setSize(16);
    if (sAlignment == 16)
      vloadi128a(p.pc[0], sAdj);
    else
      vloadi128u(p.pc[0], sAdj);
  }
  else {
    newXmmArray(p.uc, 2, "c");

    sAdj.setSize(8);
    vmovu8u16(p.uc[0], sAdj); sAdj.addOffsetLo32(8);
    vmovu8u16(p.uc[1], sAdj);
  }

  xSatisfyARGB32_Nx(p, flags);
}

void PipeCompiler::xFetchARGB32_8x(PixelARGB& p, uint32_t flags, const x86::Mem& sMem, uint32_t sAlignment) noexcept {
  x86::Mem sAdj(sMem);

  if (flags & PixelARGB::kPC) {
    newXmmArray(p.pc, 2, "c");
    sAdj.setSize(16);

    if (sAlignment == 16) {
      vloadi128a(p.pc[0], sAdj); sAdj.addOffsetLo32(16);
      vloadi128a(p.pc[1], sAdj);
    }
    else {
      vloadi128u(p.pc[0], sAdj); sAdj.addOffsetLo32(16);
      vloadi128u(p.pc[1], sAdj);
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

  xSatisfyARGB32_Nx(p, flags);
}

void PipeCompiler::xSatisfyARGB32_1x(PixelARGB& p, uint32_t flags) noexcept {
  // Quick reject if all flags were satisfied already or no flags were given.
  if ((!(flags & PixelARGB::kPC ) || !p.pc .empty()) &&
      (!(flags & PixelARGB::kUC ) || !p.uc .empty()) &&
      (!(flags & PixelARGB::kUA ) || !p.ua .empty()) &&
      (!(flags & PixelARGB::kUIA) || !p.uia.empty()) )
    return;

  // Only fetch if we have already unpacked pixels. Wait otherwise as fetch
  // flags may contain `PixelARGB::kUC`, which is handled below.
  if ((flags & (PixelARGB::kUA | PixelARGB::kUIA)) && p.ua.empty() && !p.uc.empty()) {
    newXmmArray(p.ua, 1, "a");

    vswizli16(p.ua[0], p.uc[0], x86::Predicate::shuf(3, 3, 3, 3));
    vswizi32(p.ua[0], p.ua[0], x86::Predicate::shuf(1, 0, 1, 0));
  }

  if ((flags & PixelARGB::kPC) && p.pc.empty()) {
    BL_ASSERT(!p.uc.empty());

    cc->rename(p.uc[0], "c0");
    vpacki16u8(p.uc[0], p.uc[0], p.uc[0]);

    p.pc.init(p.uc[0]);
    p.uc.reset();
  }
  else if ((flags & PixelARGB::kUC) && p.uc.empty()) {
    cc->rename(p.pc[0], "c0");
    vmovu8u16(p.pc[0], p.pc[0]);

    p.uc.init(p.pc[0]);
    p.pc.reset();
  }

  if ((flags & (PixelARGB::kUA | PixelARGB::kUIA)) && p.ua.empty()) {
    // This time we have to really fetch A8/IA8 if we didn't do before.
    newXmmArray(p.ua, 1, "ua");

    if (!p.uc.empty()) {
      vswizli16(p.ua[0], p.uc[0], x86::Predicate::shuf(3, 3, 3, 3));
    }
    else {
      BL_ASSERT(!p.pc.empty());
      vswizli16(p.ua[0], p.pc[0], x86::Predicate::shuf(1, 1, 1, 1));
      vsrli16(p.ua[0], p.ua[0], 8);
    }
  }

  if ((flags & PixelARGB::kUIA) && p.uia.empty()) {
    p.uia.init(p.ua);
    p.ua.reset();

    cc->rename(p.uia[0], "uia0");
    vinv255u16(p.uia[0], p.uia[0]);
  }
}

void PipeCompiler::xSatisfyARGB32_Nx(PixelARGB& p, uint32_t flags) noexcept {
  uint32_t i;

  // Quick reject if all flags were satisfied already or no flags were given.
  if ((!(flags & PixelARGB::kPC ) || !p.pc .empty()) &&
      (!(flags & PixelARGB::kUC ) || !p.uc .empty()) &&
      (!(flags & PixelARGB::kUA ) || !p.ua .empty()) &&
      (!(flags & PixelARGB::kUIA) || !p.uia.empty()) )
    return;

  // Only fetch if we have already unpacked pixels. Wait otherwise as fetch
  // flags may contain `PixelARGB::kUC`, which is handled below. This is an
  // optimization for cases where user wants packed ARGB and unpacked Alpha.
  if ((flags & (PixelARGB::kUA | PixelARGB::kUIA)) && p.ua.empty() && !p.uc.empty()) {
    // Emit pshuflw/pshufhw sequence for every unpacked pixel.
    newXmmArray(p.ua, p.uc.size(), "a");

    vswizli16(p.ua, p.uc, x86::Predicate::shuf(3, 3, 3, 3));
    vswizhi16(p.ua, p.ua, x86::Predicate::shuf(3, 3, 3, 3));
  }

  if ((flags & PixelARGB::kPC) && p.pc.empty()) {
    BL_ASSERT(!p.uc.empty());

    // Emit pack sequence.
    p.pc._size = p.uc.size() / 2;
    for (i = 0; i < p.uc.size(); i += 2) {
      BL_ASSERT(i + 1 < p.uc.size());

      cc->rename(p.uc[i], "c%u", i);
      vpacki16u8(p.uc[i], p.uc[i], p.uc[i + 1]);

      p.pc[i / 2] = p.uc[i];
    }
    p.uc.reset();
  }
  else if ((flags & PixelARGB::kUC) && p.uc.empty()) {
    // Emit unpack sequence.
    p.uc._size = p.pc.size() * 2;
    for (i = 0; i < p.pc.size(); i++) {
      cc->rename(p.pc[i], "c%u", i * 2);

      p.uc[i * 2 + 0] = p.pc[i];
      p.uc[i * 2 + 1] = cc->newXmm("c%u", i * 2 + 1);

      xMovzxBW_LoHi(p.uc[i * 2 + 0], p.uc[i * 2 + 1], p.uc[i * 2 + 0]);
    }
    p.pc.reset();
  }

  if ((flags & (PixelARGB::kUA | PixelARGB::kUIA)) && p.ua.empty()) {
    // This time we have to really fetch A8/IA8, if we didn't before.
    if (!p.uc.empty()) {
      newXmmArray(p.ua, p.uc.size(), "a");

      vswizli16(p.ua, p.uc, x86::Predicate::shuf(3, 3, 3, 3));
      vswizhi16(p.ua, p.ua, x86::Predicate::shuf(3, 3, 3, 3));
    }
    else if (!p.pc.empty()) {
      newXmmArray(p.ua, p.pc.size() * 2, "ua");
      for (i = 0; i < p.pc.size(); i++)
        xExtractUnpackedAFromPackedARGB32_4(p.ua[i * 2], p.ua[i * 2 + 1], p.pc[i]);
    }
    else {
      BL_NOT_REACHED();
    }
  }

  if ((flags & PixelARGB::kUIA) && p.uia.empty()) {
    p.uia._size = p.ua.size();
    for (i = 0; i < p.ua.size(); i++) {
      p.uia[i] = p.ua[i];
      cc->rename(p.uia[i], "ia%u", i);
      vinv255u16(p.uia[i], p.uia[i]);
    }
    p.ua.reset();
  }
}

void PipeCompiler::xSatisfySolid(PixelARGB& p, uint32_t flags) noexcept {
  if ((flags & PixelARGB::kPC) && p.pc.empty()) {
    BL_ASSERT(!p.uc.empty());
    newXmmArray(p.pc, 1, "pixel.pc");

    vmov(p.pc[0], p.uc[0]);
    vpacki16u8(p.pc[0], p.pc[0], p.pc[0]);
  }

  if ((flags & PixelARGB::kUC) && p.uc.empty()) {
    BL_ASSERT(!p.pc.empty());
    newXmmArray(p.uc, 1, "pixel.uc");

    vmovu8u16(p.uc[0], p.pc[0]);
  }

  if ((flags & PixelARGB::kUA) && p.ua.empty()) {
    newXmmArray(p.ua, 1, "pixel.ua");

    if (!p.uc.empty()) {
      vswizli16(p.ua[0], p.uc[0], x86::Predicate::shuf(3, 3, 3, 3));
      vswizi32(p.ua[0], p.ua[0], x86::Predicate::shuf(1, 0, 1, 0));
    }
    else {
      vswizli16(p.ua[0], p.pc[0], x86::Predicate::shuf(1, 1, 1, 1));
      vswizi32(p.ua[0], p.ua[0], x86::Predicate::shuf(1, 0, 1, 0));
      vsrli16(p.ua[0], p.ua[0], 8);
    }
  }

  if ((flags & PixelARGB::kUIA) && p.uia.empty()) {
    newXmmArray(p.uia, 1, "pixel.uia");

    if (!p.ua.empty()) {
      vmov(p.uia[0], p.ua[0]);
    }
    else if (!p.uc.empty()) {
      vswizli16(p.uia[0], p.uc[0], x86::Predicate::shuf(3, 3, 3, 3));
      vswizi32(p.uia[0], p.uia[0], x86::Predicate::shuf(1, 0, 1, 0));
    }
    else {
      vswizli16(p.uia[0], p.pc[0], x86::Predicate::shuf(1, 1, 1, 1));
      vswizi32(p.uia[0], p.uia[0], x86::Predicate::shuf(1, 0, 1, 0));
      vsrli16(p.uia[0], p.uia[0], 8);
    }
    vinv255u16(p.uia[0], p.uia[0]);
  }
}

void PipeCompiler::vFillAlpha(PixelARGB& p) noexcept {
  if (!p.pc.empty()) vFillAlpha255B(p.pc, p.pc);
  if (!p.uc.empty()) vFillAlpha255W(p.uc, p.uc);
}

// ============================================================================
// [BLPipeGen::PipeCompiler - Commons]
// ============================================================================

void PipeCompiler::xLoopMemset32(x86::Gp& dst, x86::Vec& src, x86::Gp& i, uint32_t n, uint32_t granularity) noexcept {
  BL_ASSERT(n >= 16);
  BL_ASSERT((n % 16) == 0);

  int nInBytes = int(n * 4);

  if (granularity == 4) {
    // Memset loop expecting `i % 4 == 0`, which means that we can process 4
    // elements at a time without having to check whether we are at the end.
    Label L_MainLoop = cc->newLabel();
    Label L_MainSkip = cc->newLabel();
    Label L_TailLoop = cc->newLabel();
    Label L_End      = cc->newLabel();

    cc->alloc(dst);
    cc->alloc(src);

    cc->sub(i, n);
    cc->jc(L_MainSkip);

    cc->bind(L_MainLoop);
    cc->add(dst, nInBytes);
    cc->sub(i, n);
    for (uint32_t unrollIndex = 0; unrollIndex < n; unrollIndex += 4)
      vstorei128u(x86::ptr(dst, int((unrollIndex - n) * 4)), src);
    cc->jnc(L_MainLoop);

    cc->bind(L_MainSkip);
    cc->add(i, n);
    cc->jz(L_End);

    cc->bind(L_TailLoop);
    vstorei128u(x86::ptr(dst), src);
    cc->add(dst, 16);
    cc->sub(i, 4);
    cc->jnz(L_TailLoop);

    cc->bind(L_End);
  }
  else {
    Label L_MainInit = cc->newLabel();
    Label L_MainLoop = cc->newLabel();
    Label L_MainDone = cc->newLabel();

    Label L_TailLoop = cc->newLabel();
    Label L_TailDone = cc->newLabel();

    Label L_Repeat = cc->newLabel();
    Label L_End = cc->newLabel();

    cc->alloc(dst);
    cc->alloc(src);

    cc->test(dst.r8(), 0xF);
    cc->short_().jz(L_MainInit);

    // If jumped here it will repeat the lead sequence for at most 3 values. The
    // `test(dst, 0xF)` condition will never be taken in such case as the first
    // iteration, which is here, basically misaligns an already aligned `dst`.
    cc->bind(L_Repeat);

    cc->add(dst, 4);
    cc->dec(i);
    vstorei32(x86::ptr(dst, -4), src);
    cc->jz(L_End);

    cc->test(dst.r8(), 0xF);
    cc->short_().jz(L_MainInit);

    cc->add(dst, 4);
    cc->dec(i);
    vstorei32(x86::ptr(dst, -4), src);
    cc->jz(L_End);

    cc->test(dst.r8(), 0xF);
    cc->short_().jz(L_MainInit);

    cc->add(dst, 4);
    cc->dec(i);
    vstorei32(x86::ptr(dst, -4), src);
    cc->jz(L_End);

    // Main loop.
    cc->bind(L_MainInit);
    cc->sub(i, int(n));
    cc->short_().jc(L_MainDone);

    cc->bind(L_MainLoop);
    cc->add(dst, int(n * 4));
    cc->sub(i, int(n));
    for (uint32_t unrollIndex = 0; unrollIndex < n; unrollIndex += 4)
      vstorei128a(x86::ptr(dst, int((unrollIndex - n) * 4)), src);
    cc->jnc(L_MainLoop);

    cc->bind(L_MainDone);
    cc->add(i, n);
    cc->short_().jz(L_End);

    // Tail loop.
    cc->sub(i, 4);
    cc->short_().jc(L_TailDone);

    cc->bind(L_TailLoop);
    vstorei128a(x86::ptr(dst), src);
    cc->add(dst, 16);
    cc->sub(i, 4);
    cc->short_().jnc(L_TailLoop);

    cc->bind(L_TailDone);
    cc->add(i, 4);
    cc->jnz(L_Repeat);

    cc->bind(L_End);
  }
}

void PipeCompiler::xLoopMemcpy32(x86::Gp& dst, x86::Gp& src, x86::Gp& i, uint32_t n, uint32_t granularity) noexcept {
  BL_ASSERT(n >= 16);
  BL_ASSERT((n % 16) == 0);

  int nInBytes = int(n * 4);
  x86::Xmm t0 = cc->newXmm("t0");

  if (granularity == 4) {
    // Memcpy loop expecting `i % 4 == 0`, which means that we can process 4
    // elements at a time without having to check whether we are at the end.
    Label L_MainLoop = cc->newLabel();
    Label L_MainSkip = cc->newLabel();
    Label L_TailLoop = cc->newLabel();
    Label L_End      = cc->newLabel();

    cc->alloc(dst);
    cc->alloc(src);

    cc->sub(i, n);
    cc->jc(L_MainSkip);

    cc->bind(L_MainLoop);
    cc->add(dst, nInBytes);
    cc->add(src, nInBytes);
    cc->sub(i, n);
    xInlineMemcpyXmm(x86::ptr(dst, -nInBytes), false, x86::ptr(src, -nInBytes), false, nInBytes);
    cc->jnc(L_MainLoop);

    cc->bind(L_MainSkip);
    cc->add(i, n);
    cc->jz(L_End);

    cc->bind(L_TailLoop);
    vloadps_128u(t0, x86::ptr(src));
    vstoreps_128u(x86::ptr(dst), t0);
    cc->add(dst, 16);
    cc->add(src, 16);
    cc->sub(i, 4);
    cc->jnz(L_TailLoop);

    cc->bind(L_End);
  }
  else {
    Label L_MainInit = cc->newLabel();
    Label L_MainLoop = cc->newLabel();
    Label L_MainDone = cc->newLabel();

    Label L_TailLoop = cc->newLabel();
    Label L_TailDone = cc->newLabel();

    Label L_Repeat = cc->newLabel();
    Label L_End = cc->newLabel();

    cc->alloc(dst);
    cc->alloc(src);

    cc->test(dst.r8(), 0xF);
    cc->short_().jz(L_MainInit);

    // If jumped here it will repeat the lead sequence for at most 3 pixels. The
    // `test(dst, 0xF)` condition will never be taken in such case as the first
    // iteration, which is here, basically misaligns an already aligned `dst`.
    cc->bind(L_Repeat);

    vloadi32(t0, x86::ptr(src));
    cc->add(dst, 4);
    cc->add(src, 4);
    cc->dec(i);
    vstorei32(x86::ptr(dst, -4), t0);
    cc->jz(L_End);

    cc->test(dst.r8(), 0xF);
    cc->short_().jz(L_MainInit);

    vloadi32(t0, x86::ptr(src));
    cc->add(dst, 4);
    cc->add(src, 4);
    cc->dec(i);
    vstorei32(x86::ptr(dst, -4), t0);
    cc->jz(L_End);

    cc->test(dst.r8(), 0xF);
    cc->short_().jz(L_MainInit);

    vloadi32(t0, x86::ptr(src));
    cc->add(dst, 4);
    cc->add(src, 4);
    cc->dec(i);
    vstorei32(x86::ptr(dst, -4), t0);
    cc->jz(L_End);

    // Main loop.
    cc->bind(L_MainInit);
    cc->sub(i, int(n));
    cc->jc(L_MainDone);

    cc->bind(L_MainLoop);
    cc->add(src, nInBytes);
    cc->add(dst, nInBytes);
    cc->sub(i, int(n));
    xInlineMemcpyXmm(x86::ptr(dst, -nInBytes), true,
                     x86::ptr(src, -nInBytes), false, nInBytes);
    cc->jnc(L_MainLoop);

    cc->bind(L_MainDone);
    cc->add(i, int(n));
    cc->short_().jz(L_End);

    // Tail loop.
    cc->sub(i, 4);
    cc->short_().jc(L_TailDone);

    cc->bind(L_TailLoop);
    vloadps_128u(t0, x86::ptr(src));
    cc->add(dst, 16);
    cc->add(src, 16);
    cc->sub(i, 4);
    vstoreps_128a(x86::ptr(dst, -16), t0);
    cc->jnc(L_TailLoop);

    cc->bind(L_TailDone);
    cc->add(i, 4);
    cc->jnz(L_Repeat);

    cc->bind(L_End);
  }
}

void PipeCompiler::xInlineMemcpyXmm(
  const x86::Mem& dPtr, bool dstAligned,
  const x86::Mem& sPtr, bool srcAligned, int numBytes) noexcept {

  x86::Mem dAdj(dPtr);
  x86::Mem sAdj(sPtr);
  x86::Xmm t[4];

  // TODO: [PIPEGEN] Don't create registers we don't need, there should be loop
  // that only creates enough registers for the memcpy.
  t[0] = cc->newXmm("t0");
  t[1] = cc->newXmm("t1");
  t[2] = cc->newXmm("t2");
  t[3] = cc->newXmm("t3");

  uint32_t fetchInst = hasAVX() ? x86::Inst::kIdVmovaps : x86::Inst::kIdMovaps;
  uint32_t storeInst = hasAVX() ? x86::Inst::kIdVmovaps : x86::Inst::kIdMovaps;

  if (!srcAligned) fetchInst = hasAVX()  ? x86::Inst::kIdVlddqu  :
                               hasSSE3() ? x86::Inst::kIdLddqu   : x86::Inst::kIdMovups;
  if (!dstAligned) storeInst = hasAVX()  ? x86::Inst::kIdVmovups : x86::Inst::kIdMovups;

  int n = numBytes / 16;
  do {
    int a, b = blMin<int>(numBytes, int(ASMJIT_ARRAY_SIZE(t)));
    for (a = 0; a < b; a++) { cc->emit(fetchInst, t[a], sAdj); sAdj.addOffsetLo32(16); }
    for (a = 0; a < b; a++) { cc->emit(storeInst, dAdj, t[a]); dAdj.addOffsetLo32(16); }
    n -= b;
  } while (n > 0);
}

} // {BLPipeGen}
