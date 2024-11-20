// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fillpart_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchpixelptrpart_p.h"
#include "../../pipeline/jit/fetchutilscoverage_p.h"
#include "../../pipeline/jit/fetchutilsinlineloops_p.h"
#include "../../pipeline/jit/fetchutilspixelaccess_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

// bl::Pipeline::JIT::FillPart - Utilities
// =======================================

static uint32_t calculateCoverageByteCount(PixelCount pixelCount, PixelType pixelType, PixelCoverageFormat coverageFormat) noexcept {
  DataWidth dataWidth = DataWidth::k8;

  switch (coverageFormat) {
    case PixelCoverageFormat::kPacked:
      dataWidth = DataWidth::k8;
      break;

    case PixelCoverageFormat::kUnpacked:
      dataWidth = DataWidth::k16;
      break;

    default:
      BL_NOT_REACHED();
  }

  uint32_t count = pixelCount.value();
  switch (pixelType) {
    case PixelType::kA8:
      break;

    case PixelType::kRGBA32:
      count *= 4u;
      break;

    default:
      BL_NOT_REACHED();
  }

  return (1u << uint32_t(dataWidth)) * count;
}

static void initVecCoverage(
  PipeCompiler* pc,
  VecArray& dst,
  PixelCount maxPixelCount,
  VecWidth accVecWidth,
  VecWidth maxVecWidth,
  PixelType pixelType,
  PixelCoverageFormat coverageFormat) noexcept {

  uint32_t coverageByteCount = calculateCoverageByteCount(maxPixelCount, pixelType, coverageFormat);
  VecWidth vecWidth = VecWidthUtils::vecWidthForByteCount(maxVecWidth, coverageByteCount);
  uint32_t vecCount = VecWidthUtils::vecCountForByteCount(vecWidth, coverageByteCount);

  pc->newVecArray(dst, vecCount, blMax(vecWidth, accVecWidth), "vm");

  // The width of the register must match the accumulator (as otherwise AsmJit could
  // spill and only load a part of it in case the vector width of `dst` is smaller).
  dst.setVecWidth(vecWidth);
}

static void passVecCoverage(
  VecArray& dst,
  const VecArray& src,
  PixelCount pixelCount,
  PixelType pixelType,
  PixelCoverageFormat coverageFormat) noexcept {

  uint32_t coverageByteCount = calculateCoverageByteCount(pixelCount, pixelType, coverageFormat);
  VecWidth vecWidth = VecWidthUtils::vecWidthForByteCount(VecWidthUtils::vecWidthOf(src[0]), coverageByteCount);
  uint32_t vecCount = VecWidthUtils::vecCountForByteCount(vecWidth, coverageByteCount);

  // We can use at most what was given to us, or less in case that the current
  // `pixelCount` is less than `maxPixelCount` passed to `initVecCoverage()`.
  BL_ASSERT(vecCount <= src.size());

  dst._size = vecCount;
  for (uint32_t i = 0; i < vecCount; i++) {
    dst.v[i].reset();
    dst.v[i].as<asmjit::BaseReg>().setSignatureAndId(VecWidthUtils::signatureOf(vecWidth), src.v[i].id());
  }
}

// bl::Pipeline::JIT::FillPart - Construction & Destruction
// ========================================================

FillPart::FillPart(PipeCompiler* pc, FillType fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : PipePart(pc, PipePartType::kFill),
    _fillType(fillType) {

  // Initialize the children of this part.
  _children[kIndexDstPart] = dstPart;
  _children[kIndexCompOpPart] = compOpPart;
  _childCount = 2;
}

// [[pure virtual]]
void FillPart::compile(const PipeFunction& fn) noexcept {
  blUnused(fn);
  BL_NOT_REACHED();
}

// bl::Pipeline::JIT::FillBoxAPart - Construction & Destruction
// ============================================================

FillBoxAPart::FillBoxAPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : FillPart(pc, FillType::kBoxA, dstPart, compOpPart) {

  addPartFlags(PipePartFlags::kRectFill);
  _maxVecWidthSupported = VecWidth::kMaxPlatformWidth;
}

// bl::Pipeline::JIT::FillBoxAPart - Compile
// =========================================

void FillBoxAPart::compile(const PipeFunction& fn) noexcept {
  // Prepare
  // -------

  _initGlobalHook(cc->cursor());

  int dstBpp = int(dstPart()->bpp());
  bool isSrcCopyFill = compOpPart()->isSrcCopy() && compOpPart()->srcPart()->isSolid();

  // Local Registers
  // ---------------

  Gp ctxData = fn.ctxData();                          // Reg/Init.
  Gp fillData = fn.fillData();                        // Reg/Init.

  Gp dstPtr = pc->newGpPtr("dstPtr");                 // Reg.
  Gp dstStride = pc->newGpPtr("dstStride");           // Reg/Mem.

  Gp x = pc->newGp32("x");                            // Reg.
  Gp y = pc->newGp32("y");                            // Reg/Mem.
  Gp w = pc->newGp32("w");                            // Reg/Mem.
  Gp ga_sm = pc->newGp32("ga.sm");                    // Reg/Tmp.

  // Prolog
  // ------

  pc->load(dstStride, mem_ptr(ctxData, BL_OFFSET_OF(ContextData, dst.stride)));
  pc->load_u32(y, mem_ptr(fillData, BL_OFFSET_OF(FillData::BoxA, box.y0)));
  pc->load_u32(w, mem_ptr(fillData, BL_OFFSET_OF(FillData::BoxA, box.x0)));

  pc->mul(dstPtr, dstStride, y.cloneAs(dstPtr));

  dstPart()->initPtr(dstPtr);
  compOpPart()->init(fn, w, y, 1);

  pc->add_ext(dstPtr, dstPtr, w, uint32_t(dstBpp));
  pc->sub(w, mem_ptr(fillData, BL_OFFSET_OF(FillData::BoxA, box.x1)), w);
  pc->sub(y, mem_ptr(fillData, BL_OFFSET_OF(FillData::BoxA, box.y1)), y);
  pc->mul(x, w, dstBpp);
  pc->add(dstPtr, dstPtr, mem_ptr(ctxData, BL_OFFSET_OF(ContextData, dst.pixelData)));

  if (isSrcCopyFill) {
    Label L_NotStride = pc->newLabel();

    pc->j(L_NotStride, cmp_ne(x.cloneAs(dstStride), dstStride));
    pc->mul(w, w, y);
    pc->mov(y, 1);
    pc->bind(L_NotStride);
  }
  else {
    // Only subtract from destination stride if this is not a solid rectangular fill.
    pc->sub(dstStride, dstStride, x.cloneAs(dstStride));
  }

  // Loop
  // ----

  if (compOpPart()->shouldOptimizeOpaqueFill()) {
    Label L_SemiAlphaInit = pc->newLabel();
    Label L_End  = pc->newLabel();

    pc->load_u32(ga_sm, mem_ptr(fillData, BL_OFFSET_OF(FillData::BoxA, alpha)));
    pc->j(L_SemiAlphaInit, cmp_ne(ga_sm, 255));

    // Full Alpha
    // ----------

    if (isSrcCopyFill) {
      // Optimize fill rect if it can be implemented as a memset. The main reason is
      // that if the width is reasonably small we want to only check that condition once.
      compOpPart()->cMaskInitOpaque();
      BL_ASSERT(compOpPart()->_solidOpt.px.isValid());

      FetchUtils::inlineFillRectLoop(pc, dstPtr, dstStride, w, y, compOpPart()->_solidOpt.px, dstPart()->bpp(), L_End);
      compOpPart()->cMaskFini();
    }
    else {
      Label L_AdvanceY = pc->newLabel();
      Label L_ProcessY = pc->newLabel();

      compOpPart()->cMaskInitOpaque();
      pc->j(L_ProcessY);

      pc->bind(L_AdvanceY);
      compOpPart()->advanceY();
      pc->add(dstPtr, dstPtr, dstStride);

      pc->bind(L_ProcessY);
      pc->mov(x, w);
      compOpPart()->startAtX(pc->_gpNone);
      compOpPart()->cMaskGenericLoop(x);
      pc->j(L_AdvanceY, sub_nz(y, 1));

      compOpPart()->cMaskFini();
      pc->j(L_End);
    }

    // Semi Alpha
    // ----------

    {
      Label L_AdvanceY = pc->newLabel();
      Label L_ProcessY = pc->newLabel();

      pc->bind(L_SemiAlphaInit);

      if (isSrcCopyFill) {
        // This was not accounted yet as `inlineFillRectLoop()` expects full stride, so we have to account this now.
        pc->sub(dstStride, dstStride, x.cloneAs(dstStride));
      }

      compOpPart()->cMaskInit(ga_sm, Vec());
      pc->j(L_ProcessY);

      pc->bind(L_AdvanceY);
      compOpPart()->advanceY();
      pc->add(dstPtr, dstPtr, dstStride);

      pc->bind(L_ProcessY);
      pc->mov(x, w);
      compOpPart()->startAtX(pc->_gpNone);
      compOpPart()->cMaskGenericLoop(x);
      pc->j(L_AdvanceY, sub_nz(y, 1));

      compOpPart()->cMaskFini();
      pc->bind(L_End);
    }
  }
  else {
    Label L_AdvanceY = pc->newLabel();
    Label L_ProcessY = pc->newLabel();

    compOpPart()->cMaskInit(mem_ptr(fillData, BL_OFFSET_OF(FillData::BoxA, alpha)));
    pc->j(L_ProcessY);

    pc->bind(L_AdvanceY);
    compOpPart()->advanceY();
    pc->add(dstPtr, dstPtr, dstStride);

    pc->bind(L_ProcessY);
    pc->mov(x, w);
    compOpPart()->startAtX(pc->_gpNone);
    compOpPart()->cMaskGenericLoop(x);
    pc->j(L_AdvanceY, sub_nz(y, 1));

    compOpPart()->cMaskFini();
  }

  // Epilog
  // ------

  compOpPart()->fini();
  _finiGlobalHook();
}

// bl::Pipeline::JIT::FillMaskPart - Construction & Destruction
// ============================================================

FillMaskPart::FillMaskPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : FillPart(pc, FillType::kMask, dstPart, compOpPart) {

  _maxVecWidthSupported = VecWidth::kMaxPlatformWidth;
}

// bl::Pipeline::JIT::FillMaskPart - Compile
// =========================================

void FillMaskPart::compile(const PipeFunction& fn) noexcept {
  // EndOrRepeat is expected to be zero for fast termination of the scanline.
  BL_STATIC_ASSERT(uint32_t(MaskCommandType::kEndOrRepeat) == 0);

  // Prepare
  // -------

  _initGlobalHook(cc->cursor());

  int dstBpp = int(dstPart()->bpp());
  constexpr int kMaskCmdSize = int(sizeof(MaskCommand));

#if defined(BL_JIT_ARCH_X86)
  constexpr int labelAlignment = 8;
#else
  constexpr int labelAlignment = 4;
#endif

  // Local Labels
  // ------------

  Label L_ScanlineInit = pc->newLabel();
  Label L_ScanlineDone = pc->newLabel();
  Label L_ScanlineSkip = pc->newLabel();

  Label L_ProcessNext = pc->newLabel();
  Label L_ProcessCmd = pc->newLabel();
  Label L_CMaskInit = pc->newLabel();
  Label L_VMaskA8WithoutGA = pc->newLabel();
  Label L_End = pc->newLabel();

  // Local Registers
  // ---------------

  Gp ctxData = fn.ctxData();                          // Reg/Init.
  Gp fillData = fn.fillData();                        // Reg/Init.

  Gp dstPtr = pc->newGpPtr("dstPtr");                 // Reg.
  Gp dstStride = pc->newGpPtr("dstStride");           // Reg/Mem.

  Gp i = pc->newGp32("i");                            // Reg.
  Gp x = pc->newGp32("x");                            // Reg.
  Gp y = pc->newGp32("y");                            // Reg/Mem.

  Gp cmdType = pc->newGp32("cmdType");                // Reg/Tmp.
  Gp cmdPtr = pc->newGpPtr("cmdPtr");                 // Reg/Mem.
  Gp cmdBegin = pc->newGpPtr("cmdBegin");             // Mem.
  Gp maskValue = pc->newGpPtr("maskValue");           // Reg.
  Gp maskAdvance = pc->newGpPtr("maskAdvance");       // Reg/Tmp

  GlobalAlpha ga;

  // Prolog
  // ------

  // Initialize the destination.
  pc->load(dstStride, mem_ptr(ctxData, BL_OFFSET_OF(ContextData, dst.stride)));
  pc->load_u32(y, mem_ptr(fillData, BL_OFFSET_OF(FillData, mask.box.y0)));

  pc->mul(dstPtr, dstStride, y.cloneAs(dstPtr));
  pc->add(dstPtr, dstPtr, mem_ptr(ctxData, BL_OFFSET_OF(ContextData, dst.pixelData)));

  // Initialize pipeline parts.
  dstPart()->initPtr(dstPtr);
  compOpPart()->init(fn, pc->_gpNone, y, 1);

  // Initialize mask pointers.
  pc->load(cmdPtr, mem_ptr(fillData, BL_OFFSET_OF(FillData, mask.maskCommandData)));

  // Initialize global alpha.
  ga.initFromMem(pc, mem_ptr(fillData, BL_OFFSET_OF(FillData, mask.alpha)));

  // y = fillData->box.y1 - fillData->box.y0;
  pc->sub(y, mem_ptr(fillData, BL_OFFSET_OF(FillData, mask.box.y1)), y);
  pc->j(L_ScanlineInit);

  // Scanline Done
  // -------------

  Gp repeat = pc->newGp32("repeat");

  pc->align(AlignMode::kCode, labelAlignment);
  pc->bind(L_ScanlineDone);
  deadvanceDstPtr(dstPtr, x, int(dstBpp));

  pc->bind(L_ScanlineSkip);
  pc->load_u32(repeat, mem_ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _x0)));
  pc->j(L_End, sub_z(y, 1));

  pc->sub(repeat, repeat, 1);
  pc->add(dstPtr, dstPtr, dstStride);
  pc->store_u32(mem_ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _x0)), repeat);
  pc->add(cmdPtr, cmdPtr, kMaskCmdSize);
  compOpPart()->advanceY();
  pc->cmov(cmdPtr, cmdBegin, cmp_ne(repeat, 0));

  // Scanline Init
  // -------------

  pc->bind(L_ScanlineInit);
  pc->load_u32(cmdType, mem_ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _x1AndType)));
  pc->mov(cmdBegin, cmdPtr);
  pc->load_u32(x, mem_ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _x0)));
  // This is not really common, but it's possible to skip entire scanlines with `kEndOrRepeat`.
  pc->j(L_ScanlineSkip, test_z(cmdType, MaskCommand::kTypeMask));

  pc->add_scaled(dstPtr, x.cloneAs(dstPtr), dstBpp);
  compOpPart()->startAtX(x);
  pc->j(L_ProcessCmd);

  // Process Command
  // ---------------

  pc->bind(L_ProcessNext);
  pc->load_u32(cmdType, mem_ptr(cmdPtr, kMaskCmdSize + BL_OFFSET_OF(MaskCommand, _x1AndType)));
  pc->load_u32(i, mem_ptr(cmdPtr, kMaskCmdSize + BL_OFFSET_OF(MaskCommand, _x0)));
  pc->add(cmdPtr, cmdPtr, kMaskCmdSize);
  pc->j(L_ScanlineDone, test_z(cmdType, MaskCommand::kTypeMask));

  // Only emit the jump if there is something significant to skip.
  if (compOpPart()->hasPartFlag(PipePartFlags::kAdvanceXIsSimple))
    pc->sub(i, i, x);
  else
    pc->j(L_ProcessCmd, sub_z(i, x));

  pc->add(x, x, i);
  pc->add_scaled(dstPtr, i.cloneAs(dstPtr), dstBpp);
  compOpPart()->advanceX(x, i);

  pc->bind(L_ProcessCmd);

#if defined(BL_JIT_ARCH_X86)
  if (pc->hasBMI2() && pc->is64Bit())
  {
    // This saves one instruction on X86_64 as RORX provides a non-destructive destination.
    pc->ror(i.r64(), cmdType.r64(), MaskCommand::kTypeBits);
  }
  else
#endif // BL_JIT_ARCH_X86
  {
    pc->shr(i, cmdType, MaskCommand::kTypeBits);
  }

  pc->and_(cmdType, cmdType, MaskCommand::kTypeMask);
  pc->sub(i, i, x);
  pc->load(maskValue, mem_ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _value.data)));
  pc->add(x, x, i);

  // We know the command is not kEndOrRepeat, which allows this little trick.
  pc->j(L_CMaskInit, cmp_eq(cmdType, uint32_t(MaskCommandType::kCMask)));

  // VMask Command
  // -------------

  // Increments the advance in the mask command in case it would be repeated.
  pc->load(maskAdvance, mem_ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _maskAdvance)));
  pc->mem_add(mem_ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _value.ptr)), maskAdvance);

  pc->j(L_VMaskA8WithoutGA, cmp_eq(cmdType, uint32_t(MaskCommandType::kVMaskA8WithoutGA)));
  compOpPart()->vMaskGenericLoop(i, dstPtr, maskValue, nullptr, L_ProcessNext);

  pc->bind(L_VMaskA8WithoutGA);
  compOpPart()->vMaskGenericLoop(i, dstPtr, maskValue, &ga, L_ProcessNext);

  // CMask Command
  // -------------

  pc->align(AlignMode::kCode, labelAlignment);
  pc->bind(L_CMaskInit);
  if (compOpPart()->shouldOptimizeOpaqueFill()) {
    Label L_CLoop_Msk = pc->newLabel();
    pc->j(L_CLoop_Msk, cmp_ne(maskValue.r32(), 255));

    compOpPart()->cMaskInitOpaque();
    compOpPart()->cMaskGenericLoop(i);
    compOpPart()->cMaskFini();
    pc->j(L_ProcessNext);

    pc->align(AlignMode::kCode, labelAlignment);
    pc->bind(L_CLoop_Msk);
  }

  compOpPart()->cMaskInit(maskValue.r32(), Vec());
  compOpPart()->cMaskGenericLoop(i);
  compOpPart()->cMaskFini();
  pc->j(L_ProcessNext);

  // Epilog
  // ------

  pc->bind(L_End);
  compOpPart()->fini();
  _finiGlobalHook();
}

void FillMaskPart::deadvanceDstPtr(const Gp& dstPtr, const Gp& x, int dstBpp) noexcept {
  Gp xAdv = x.cloneAs(dstPtr);

  if (IntOps::isPowerOf2(dstBpp)) {
    if (dstBpp > 1)
      pc->shl(xAdv, xAdv, IntOps::ctz(dstBpp));
    pc->sub(dstPtr, dstPtr, xAdv);
  }
  else {
    Gp dstAdv = pc->newGpPtr("dstAdv");
    pc->mul(dstAdv, xAdv, dstBpp);
    pc->sub(dstPtr, dstPtr, dstAdv);
  }
}

// bl::Pipeline::JIT::FillAnalyticPart - Construction & Destruction
// ================================================================

FillAnalyticPart::FillAnalyticPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : FillPart(pc, FillType::kAnalytic, dstPart, compOpPart) {

  _maxVecWidthSupported = VecWidth::kMaxPlatformWidth;
}

// bl::Pipeline::JIT::FillAnalyticPart - Compile
// =============================================

void FillAnalyticPart::compile(const PipeFunction& fn) noexcept {
  // Prepare
  // -------

  _initGlobalHook(cc->cursor());

  PixelType pixelType = compOpPart()->pixelType();
  PixelCoverageFormat coverageFormat = compOpPart()->coverageFormat();

  uint32_t dstBpp = dstPart()->bpp();
  uint32_t maxPixels = compOpPart()->maxPixels();

  // vProc SIMD width describes SIMD width used to accumulate coverages and then to calculate alpha masks. In
  // general if we only calculate 4 coverages at once we only need 128-bit SIMD. However, 8 and more coverages
  // need 256-bit SIMD or higher, if available. At the moment we use always a single register for this purpose,
  // so SIMD width determines how many pixels we can process in a vMask loop at a time.
  uint32_t vProcPixelCount = 0;
  VecWidth vProcWidth = pc->vecWidth();

  if (pc->vecWidth() >= VecWidth::k256 && maxPixels >= 8) {
    vProcPixelCount = 8;
    vProcWidth = VecWidth::k256;
  }
  else {
    vProcPixelCount = blMin<uint32_t>(maxPixels, 4);
    vProcWidth = VecWidth::k128;
  }

  int bwSize = int(sizeof(BLBitWord));
  int bwSizeInBits = bwSize * 8;

  int pixelsPerOneBit = 4;
  int pixelsPerOneBitShift = int(IntOps::ctz(pixelsPerOneBit));

  int pixelGranularity = pixelsPerOneBit;
  int pixelsPerBitWord = pixelsPerOneBit * bwSizeInBits;
  int pixelsPerBitWordShift = int(IntOps::ctz(pixelsPerBitWord));

  if (compOpPart()->maxPixelsOfChildren() < 4)
    pixelGranularity = 1;

  // Local Labels
  // ------------

  Label L_BitScan_Init = pc->newLabel();
  Label L_BitScan_Iter = pc->newLabel();
  Label L_BitScan_Match = pc->newLabel();
  Label L_BitScan_End = pc->newLabel();

  Label L_VLoop_Init = pc->newLabel();
  Label L_CLoop_Init = pc->newLabel();

  Label L_VTail_Init;

  if (maxPixels >= 4) {
    L_VTail_Init = pc->newLabel();
  }

  Label L_Scanline_Done0 = pc->newLabel();
  Label L_Scanline_Done1 = pc->newLabel();
  Label L_Scanline_AdvY = pc->newLabel();
  Label L_Scanline_Iter = pc->newLabel();
  Label L_Scanline_Init = pc->newLabel();

  Label L_End = pc->newLabel();

  // Local Registers
  // ---------------

  Gp ctxData = fn.ctxData();                                 // Init.
  Gp fillData = fn.fillData();                               // Init.

  Gp dstPtr = pc->newGpPtr("dstPtr");                        // Reg.
  Gp dstStride = pc->newGpPtr("dstStride");                  // Mem.

  Gp bitPtr = pc->newGpPtr("bitPtr");                        // Reg.
  Gp bitPtrEnd = pc->newGpPtr("bitPtrEnd");                  // Reg/Mem.

  Gp bitPtrRunLen = pc->newGpPtr("bitPtrRunLen");            // Mem.
  Gp bitPtrSkipLen = pc->newGpPtr("bitPtrSkipLen");          // Mem.

  Gp cellPtr = pc->newGpPtr("cellPtr");                      // Reg.
  Gp cellStride = pc->newGpPtr("cellStride");                // Mem.

  Gp x0 = pc->newGp32("x0");                                 // Reg
  Gp xOff = pc->newGp32("xOff");                             // Reg/Mem.
  Gp xEnd = pc->newGp32("xEnd");                             // Mem.
  Gp xStart = pc->newGp32("xStart");                         // Mem.

  Gp y = pc->newGp32("y");                                   // Reg/Mem.
  Gp i = pc->newGp32("i");                                   // Reg.
  Gp cMaskAlpha = pc->newGp32("cMaskAlpha");                 // Reg/Tmp.

  Gp bitWord = pc->newGpPtr("bitWord");                      // Reg/Mem.
  Gp bitWordTmp = pc->newGpPtr("bitWordTmp");                // Reg/Tmp.

  Vec acc = pc->newVec(vProcWidth, "acc");                   // Reg.
  Vec globalAlpha = pc->newVec(vProcWidth, "globalAlpha");   // Mem.
  Vec fillRuleMask = pc->newVec(vProcWidth, "fillRuleMask"); // Mem.
  Vec vecZero;                                               // Reg/Tmp.

  Pixel dPix("d", pixelType);                                // Reg.

  VecArray m;                                                // Reg.
  VecArray compCov;                                          // Tmp (only for passing coverages to the compositor).
  initVecCoverage(pc, m, PixelCount(maxPixels), VecWidthUtils::vecWidthOf(acc), pc->vecWidth(), pixelType, coverageFormat);

  // Prolog
  // ------

  // Initialize the destination.
  pc->load_u32(y, mem_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, box.y0)));
  pc->load(dstStride, mem_ptr(ctxData, BL_OFFSET_OF(ContextData, dst.stride)));

  pc->mul(dstPtr, y.cloneAs(dstPtr), dstStride);
  pc->add(dstPtr, dstPtr, mem_ptr(ctxData, BL_OFFSET_OF(ContextData, dst.pixelData)));

  // Initialize cell pointers.
  pc->load(bitPtrSkipLen, mem_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, bitStride)));
  pc->load(cellStride, mem_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, cellStride)));

  pc->load(bitPtr, mem_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, bitTopPtr)));
  pc->load(cellPtr, mem_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, cellTopPtr)));

  // Initialize pipeline parts.
  dstPart()->initPtr(dstPtr);
  compOpPart()->init(fn, pc->_gpNone, y, uint32_t(pixelGranularity));

  // y = fillData->box.y1 - fillData->box.y0;
  pc->sub(y, mem_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, box.y1)), y);

  // Decompose the original `bitStride` to bitPtrRunLen + bitPtrSkipLen, where:
  //   - `bitPtrRunLen` - Number of BitWords (in byte units) active in this band.
  //   - `bitPtrRunSkip` - Number of BitWords (in byte units) to skip for this band.
  pc->shr(xStart, mem_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, box.x0)), pixelsPerBitWordShift);
  pc->load_u32(xEnd, mem_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, box.x1)));
  pc->shr(bitPtrRunLen.r32(), xEnd, pixelsPerBitWordShift);

  pc->sub(bitPtrRunLen.r32(), bitPtrRunLen.r32(), xStart);
  pc->inc(bitPtrRunLen.r32());
  pc->shl(bitPtrRunLen, bitPtrRunLen, IntOps::ctz(bwSize));
  pc->sub(bitPtrSkipLen, bitPtrSkipLen, bitPtrRunLen);

  // Make `xStart` to become the X offset of the first active BitWord.
  pc->lea(bitPtr, mem_ptr(bitPtr, xStart.cloneAs(bitPtr), IntOps::ctz(bwSize)));
  pc->shl(xStart, xStart, pixelsPerBitWordShift);

  // Initialize global alpha and fill-rule.
  pc->v_broadcast_u16(globalAlpha, mem_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, alpha)));
  pc->v_broadcast_u32(fillRuleMask, mem_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, fillRuleMask)));

#if defined(BL_JIT_ARCH_X86)
  vecZero = pc->newV128("vec_zero");
  // We shift left by 7 bits so we can use [V]PMULHUW in `calcMasksFromCells()` on X86 ISA. In order to make that
  // work, we have to also shift `fillRuleMask` left by 1, so the total shift left is 8, which is what we want for
  // [V]PMULHUW.
  pc->v_slli_i16(globalAlpha, globalAlpha, 7);
  pc->v_slli_i16(fillRuleMask, fillRuleMask, 1);
#else
  // In non-x86 case we want to keep zero in `vecZero` - no need to clear it every time we want to clear memory.
  vecZero = pc->simdVecZero(acc);
#endif

  pc->j(L_Scanline_Init);

  // BitScan
  // -------

  // Called by Scanline iterator on the first non-zero BitWord it matches. The responsibility of BitScan is to find
  // the first bit in the passed BitWord followed by matching the bit that ends this match. This would essentially
  // produce the first [x0, x1) span that has to be composited as 'VMask' loop.

  pc->bind(L_BitScan_Init);                                  // L_BitScan_Init:

  countZeros(x0.cloneAs(bitWord), bitWord);                  //   x0 = ctz(bitWord) or clz(bitWord);
  pc->store_zero_reg(mem_ptr(bitPtr, -bwSize));              //   bitPtr[-1] = 0;
  pc->mov(bitWordTmp, -1);                                   //   bitWordTmp = -1; (all ones).
  shiftMask(bitWordTmp, bitWordTmp, x0);                     //   bitWordTmp = bitWordTmp << x0 or bitWordTmp >> x0

  // Convert bit offset `x0` into a pixel offset. We must consider `xOff` as it's only zero for the very first
  // BitWord (all others are multiplies of `pixelsPerBitWord`).
  pc->add_ext(x0, xOff, x0, 1 << pixelsPerOneBitShift);      //   x0 = xOff + (x0 << pixelsPerOneBitShift);

  // Load the given cells to `m0` and clear the BitWord and all cells it represents in memory. This is important as
  // the compositor has to clear the memory during composition. If this is a rare case where `x0` points at the end
  // of the raster there is still one cell that is non-zero. This makes sure it's cleared.

  pc->add_scaled(dstPtr, x0.cloneAs(dstPtr), int(dstBpp));   //   dstPtr += x0 * dstBpp;
  pc->add_scaled(cellPtr, x0.cloneAs(cellPtr), 4);           //   cellPtr += x0 * sizeof(uint32_t);

  // Rare case - line rasterized at the end of the raster boundary. In 99% cases this is a clipped line that was
  // rasterized as vertical-only line at the end of the render box. This is a completely valid case that produces
  // nothing.

  pc->j(L_Scanline_Done0, ucmp_ge(x0, xEnd));                //   if (x0 >= xEnd) goto L_Scanline_Done0;

  // Setup compositor and source/destination parts. This is required as the fetcher needs to know where to start.
  // And since `startAtX()` can only be called once per scanline we must do it here.

  compOpPart()->startAtX(x0);                                //   <CompOpPart::StartAtX>

  if (maxPixels > 1)
    compOpPart()->prefetchN();                               //   <CompOpPart::PrefetchN>
  else if (pixelGranularity > 1)
    compOpPart()->srcPart()->prefetchN();

  pc->v_loada32(acc, pc->_getMemConst(&ct.i_0002000000020000));

  // If `bitWord ^ bitWordTmp` results in non-zero value it means that the current span ends within the same BitWord,
  // otherwise the span crosses multiple BitWords.

  pc->j(L_BitScan_Match, xor_nz(bitWord, bitWordTmp));       //   if ((bitWord ^= bitWordTmp) != 0) goto L_BitScan_Match;

  // Okay, so the span crosses multiple BitWords. Firstly we have to make sure this was not the last one. If that's
  // the case we must terminate the scanning immediately.

  pc->mov(i, bwSizeInBits);                                  //   i = bwSizeInBits;
  pc->j(L_BitScan_End, cmp_eq(bitPtr, bitPtrEnd));           //   if (bitPtr == bitPtrEnd) goto L_BitScan_End;

  // A BitScan loop - iterates over all consecutive BitWords and finds those that don't have all bits set to 1.

  pc->bind(L_BitScan_Iter);                                  // L_BitScan_Iter:
  pc->load(bitWord, mem_ptr(bitPtr));                        //   bitWord = bitPtr[0];
  pc->store_zero_reg(mem_ptr(bitPtr));                       //   bitPtr[0] = 0;
  pc->add(xOff, xOff, pixelsPerBitWord);                     //   xOff += pixelsPerBitWord;
  pc->add(bitPtr, bitPtr, bwSize);                           //   bitPtr += bwSize;
  pc->j(L_BitScan_Match, xor_nz(bitWord, -1));               //   if ((bitWord ^= -1) != 0) goto L_BitScan_Match;
  pc->j(L_BitScan_End, cmp_eq(bitPtr, bitPtrEnd));           //   if (bitPtr == bitPtrEnd) goto L_BitScan_End;
  pc->j(L_BitScan_Iter);                                     //   goto L_BitScan_Iter;

  pc->bind(L_BitScan_Match);                                 // L_BitScan_Match:
  countZeros(i.cloneAs(bitWord), bitWord);                   //   i = ctz(bitWord) or clz(bitWord);

  pc->bind(L_BitScan_End);                                   // L_BitScan_End:

#if defined(BL_JIT_ARCH_X86)
  if (vProcPixelCount == 8) {
    pc->v_add_i32(acc.v256(), acc.v256(), mem_ptr(cellPtr)); //   acc[7:0] += cellPtr[7:0];
  }
  else
#endif // BL_JIT_ARCH_X86
  {
    pc->v_add_i32(acc.v128(), acc.v128(), mem_ptr(cellPtr)); //   acc[3:0] += cellPtr[3:0];
  }

  pc->mov(bitWordTmp, -1);                                   //   bitWordTmp = -1; (all ones).
  shiftMask(bitWordTmp, bitWordTmp, i);                      //   bitWordTmp = bitWordTmp << i or bitWordTmp >> i;
  pc->shl(i, i, pixelsPerOneBitShift);                       //   i <<= pixelsPerOneBitShift;

  pc->xor_(bitWord, bitWord, bitWordTmp);                    //   bitWord ^= bitWordTmp;
  pc->add(i, i, xOff);                                       //   i += xOff;

  // In cases where the raster width is not a multiply of `pixelsPerOneBit` we must make sure we won't overflow it.

  pc->umin(i, i, xEnd);                                      //   i = min(i, xEnd);
#if defined(BL_JIT_ARCH_X86)
  pc->v_zero_i(vecZero);                                     //   vecZero = 0;
#endif // BL_JIT_ARCH_X86
  pc->v_storea128(mem_ptr(cellPtr), vecZero);                //   cellPtr[3:0] = 0;

  // `i` is now the number of pixels (and cells) to composite by using `vMask`.

  pc->sub(i, i, x0);                                         //   i -= x0;
  pc->add(x0, x0, i);                                        //   x0 += i;
  pc->j(L_VLoop_Init);                                       //   goto L_VLoop_Init;

  // VMaskLoop - Main VMask Loop - 8 Pixels (256-bit SIMD)
  // -----------------------------------------------------

#if defined(BL_JIT_ARCH_X86)
  if (vProcPixelCount == 8u) {
    Label L_VLoop_Iter8 = pc->newLabel();
    Label L_VLoop_End = pc->newLabel();

    pc->bind(L_VLoop_Iter8);                                 // L_VLoop_Iter8:
    pc->v_extract_v128(acc, acc, 1);

    passVecCoverage(compCov, m, PixelCount(8), pixelType, coverageFormat);
    compOpPart()->vMaskProcStoreAdvance(dstPtr, PixelCount(8), compCov, PixelCoverageFlags::kNone);

    pc->add(cellPtr, cellPtr, 8 * 4);                        //   cellPtr += 8 * sizeof(uint32_t);
    pc->v_add_i32(acc, acc, mem_ptr(cellPtr));               //   acc[7:0] += cellPtr[7:0]
    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
    pc->v_storeu256(mem_ptr(cellPtr, -16), vecZero.v256());  //   cellPtr[3:-4] = 0;

    pc->bind(L_VLoop_Init);                                  // L_VLoop_Init:
    accumulateCoverages(acc);
    calcMasksFromCells(m[0], acc, fillRuleMask, globalAlpha);
    normalizeCoverages(acc);
    expandMask(m, PixelCount(8));

    pc->j(L_VLoop_Iter8, sub_nc(i, 8));                      //   if ((i -= 8) >= 0) goto L_VLoop_Iter8;
    pc->j(L_VLoop_End, add_z(i, 8));                         //   if ((i += 8) == 0) goto L_VLoop_End;
    pc->j(L_VTail_Init, ucmp_lt(i, 4));                      //   if (i < 4) goto L_VTail_Init;

    pc->add(cellPtr, cellPtr, 4 * 4);                        //   cellPtr += 4 * sizeof(uint32_t);
    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
    pc->v_storea128(mem_ptr(cellPtr), vecZero.v128());       //   cellPtr[3:0] = 0;

    passVecCoverage(compCov, m, PixelCount(4), pixelType, coverageFormat);
    compOpPart()->vMaskProcStoreAdvance(dstPtr, PixelCount(4), compCov, PixelCoverageFlags::kImmutable);
    if (pixelType == PixelType::kRGBA32) {
      if (m[0].isZmm())
        pc->cc->vshufi32x4(m[0], m[0], m[0], x86::shuffleImm(3, 2, 3, 2)); //   m[0] = [a7 a7 a7 a7 a6 a6 a6 a6|a5 a5 a5 a5 a4 a4 a4 a4]
      else
        pc->v_mov(m[0], m[1]);                               //   m[0] = [a7 a7 a7 a7 a6 a6 a6 a6|a5 a5 a5 a5 a4 a4 a4 a4]
    }
    else if (pixelType == PixelType::kA8) {
      pc->v_swizzle_u32x4(m[0], m[0], swizzle(3, 2, 3, 2));  //   m[0] = [?? ?? ?? ?? ?? ?? ?? ??|a7 a6 a5 a4 a7 a6 a5 a4]
    }
    else {
      BL_NOT_REACHED();
    }

    pc->v_extract_v128(acc, acc, 1);
    pc->j(L_VTail_Init, sub_nz(i, 4));                       //   if ((i -= 4) > 0) goto L_VTail_Init;

    pc->bind(L_VLoop_End);                                   // L_VLoop_End:
    pc->v_extract_v128(acc, acc, 0);
    pc->j(L_Scanline_Done1, ucmp_ge(x0, xEnd));              //   if (x0 >= xEnd) goto L_Scanline_Done1;
  }
  else
#endif

  // VMask Loop - Main VMask Loop - 4 Pixels
  // ---------------------------------------

  if (vProcPixelCount == 4u) {
    Label L_VLoop_Cont = pc->newLabel();

    pc->bind(L_VLoop_Cont);                                  // L_VLoop_Cont:

    passVecCoverage(compCov, m, PixelCount(4), pixelType, coverageFormat);
    compOpPart()->vMaskProcStoreAdvance(dstPtr, PixelCount(4), compCov, PixelCoverageFlags::kNone);

    pc->add(cellPtr, cellPtr, 4 * 4);                        //   cellPtr += 4 * sizeof(uint32_t);
    pc->v_add_i32(acc, acc, mem_ptr(cellPtr));               //   acc[3:0] += cellPtr[3:0];
#if defined(BL_JIT_ARCH_X86)
    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
#endif // BL_JIT_ARCH_X86
    pc->v_storea128(mem_ptr(cellPtr), vecZero);              //   cellPtr[3:0] = 0;
    dPix.resetAllExceptTypeAndName();

    pc->bind(L_VLoop_Init);                                  // L_VLoop_Init:
    accumulateCoverages(acc);
    calcMasksFromCells(m[0], acc, fillRuleMask, globalAlpha);
    normalizeCoverages(acc);
    expandMask(m, PixelCount(4));

    pc->j(L_VLoop_Cont, sub_nc(i, 4));                       //   if ((i -= 4) >= 0) goto L_VLoop_Cont;
    pc->j(L_VTail_Init, add_nz(i, 4));                       //   if ((i += 4) != 0) goto L_VTail_Init;
    pc->j(L_Scanline_Done1, ucmp_ge(x0, xEnd));              //   if (x0 >= xEnd) goto L_Scanline_Done1;
  }

  // VMask Loop - Main VMask Loop - 1 Pixel
  // --------------------------------------

  else {
    Label L_VLoop_Iter = pc->newLabel();
    Label L_VLoop_Step = pc->newLabel();

    Gp n = pc->newGp32("n");

    pc->bind(L_VLoop_Iter);                                  // L_VLoop_Iter:
    pc->umin(n, i, 4);                                       //   n = umin(i, 4);
    pc->sub(i, i, n);                                        //   i -= n;
    pc->add_scaled(cellPtr, n, 4);                           //   cellPtr += n * 4;

    if (pixelGranularity >= 4)
      compOpPart()->enterPartialMode();                      //   <CompOpPart::enterPartialMode>

    if (pixelType == PixelType::kRGBA32) {
      constexpr PixelFlags kPC_Immutable = PixelFlags::kPC | PixelFlags::kImmutable;

#if defined(BL_JIT_ARCH_X86)
      if (!pc->hasAVX2()) {
        // Broadcasts were introduced by AVX2, so we generally don't want to use code that relies on them as they
        // would expand to more than a single instruction. So instead of a broadcast, we pre-shift the input in a
        // way so we can use a single [V]PSHUFLW to shuffle the components to places where the compositor needs them.
        pc->v_sllb_u128(m[0], m[0], 6);                      //   m0[7:0] = [__ a3 a2 a1 a0 __ __ __]

        pc->bind(L_VLoop_Step);                              // L_VLoop_Step:
        pc->v_swizzle_lo_u16x4(m[0], m[0], swizzle(3, 3, 3, 3)); // m0[7:0] = [__ a3 a2 a1 a0 a0 a0 a0]

        compCov.init(m[0].v128());
        compOpPart()->vMaskProcRGBA32Vec(dPix, PixelCount(1), kPC_Immutable, compCov, PixelCoverageFlags::kImmutable, pc->emptyPredicate());
      }
      else
#endif
      {
        Vec vmTmp = pc->newV128("@vmTmp");
        pc->bind(L_VLoop_Step);                              // L_VLoop_Step:

        if (coverageFormat == PixelCoverageFormat::kPacked)
          pc->v_broadcast_u8(vmTmp, m[0].v128());            //   vmTmp[15:0] = [a0 a0 a0 a0 a0 a0 a0 a0|a0 a0 a0 a0 a0 a0 a0 a0]
        else
          pc->v_broadcast_u16(vmTmp, m[0].v128());           //   vmTmp[15:0] = [_0 a0 _0 a0 _0 a0 _0 a0|_0 a0 _0 a0 _0 a0 _0 a0]

        compCov.init(vmTmp);
        compOpPart()->vMaskProcRGBA32Vec(dPix, PixelCount(1), kPC_Immutable, compCov, PixelCoverageFlags::kNone, pc->emptyPredicate());
      }

      pc->xStorePixel(dstPtr, dPix.pc[0], 1, dstBpp, Alignment(1));
      dPix.resetAllExceptTypeAndName();
    }
    else if (pixelType == PixelType::kA8) {
      pc->bind(L_VLoop_Step);                                // L_VLoop_Step:

      Gp msk = pc->newGp32("@msk");
      pc->s_extract_u16(msk, m[0], 0);

      compOpPart()->vMaskProcA8Gp(dPix, PixelFlags::kSA | PixelFlags::kImmutable, msk, PixelCoverageFlags::kNone);

      pc->store_u8(mem_ptr(dstPtr), dPix.sa);
      dPix.resetAllExceptTypeAndName();
    }

    pc->add(dstPtr, dstPtr, dstBpp);                         //   dstPtr += dstBpp;
    pc->shiftOrRotateRight(m[0], m[0], 2);                   //   m0[15:0] = [??, m[15:2]]

    if (pixelGranularity >= 4)                               //   if (pixelGranularity >= 4)
      compOpPart()->nextPartialPixel();                      //     <CompOpPart::nextPartialPixel>

    pc->j(L_VLoop_Step, sub_nz(n, 1));                       //   if (--n != 0) goto L_VLoop_Step;

    if (pixelGranularity >= 4)                               //   if (pixelGranularity >= 4)
      compOpPart()->exitPartialMode();                       //     <CompOpPart::exitPartialMode>

#if defined(BL_JIT_ARCH_X86)
    if (!pc->hasAVX()) {
      // We must use unaligned loads here as we don't know whether we are at the end of the scanline.
      // In that case `cellPtr` could already be misaligned if the image width is not divisible by 4.
      Vec covTmp = pc->newV128("@covTmp");
      pc->v_loadu128(covTmp, mem_ptr(cellPtr));              //   covTmp[3:0] = cellPtr[3:0];
      pc->v_add_i32(acc, acc, covTmp);                       //   acc[3:0] += covTmp
    }
    else
#endif // BL_JIT_ARCH_X86
    {
      pc->v_add_i32(acc, acc, mem_ptr(cellPtr));             //   acc[3:0] += cellPtr[3:0]
    }

#if defined(BL_JIT_ARCH_X86)
    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
#endif
    pc->v_storeu128(mem_ptr(cellPtr), vecZero);              //   cellPtr[3:0] = 0;

    pc->bind(L_VLoop_Init);                                  // L_VLoop_Init:

    accumulateCoverages(acc);
    calcMasksFromCells(m[0], acc, fillRuleMask, globalAlpha);
    normalizeCoverages(acc);

    pc->j(L_VLoop_Iter, test_nz(i));                         //   if (i != 0) goto L_VLoop_Iter;
    pc->j(L_Scanline_Done1, ucmp_ge(x0, xEnd));              //   if (x0 >= xEnd) goto L_Scanline_Done1;
  }

  // BitGap
  // ------

  // If we are here we are at the end of `vMask` loop. There are two possibilities:
  //
  //   1. There is a gap between bits in a single or multiple BitWords. This means that there is a possibility
  //      for a `cMask` loop which could be fully opaque, semi-transparent, or fully transparent (a real gap).
  //
  //   2. This was the last span and there are no more bits in consecutive BitWords. We will not consider this as
  //      a special case and just process the remaining BitWords in a normal way (scanning until the end of the
  //      current scanline).

  Label L_BitGap_Match = pc->newLabel();
  Label L_BitGap_Cont = pc->newLabel();

  pc->j(L_BitGap_Match, test_nz(bitWord));                   //   if (bitWord != 0) goto L_BitGap_Match;

  // Loop unrolled 2x as we could be inside a larger span.

  pc->bind(L_BitGap_Cont);                                   // L_BitGap_Cont:
  pc->add(xOff, xOff, pixelsPerBitWord);                     //   xOff += pixelsPerBitWord;
  pc->j(L_Scanline_Done1, cmp_eq(bitPtr, bitPtrEnd));        //   if (bitPtr == bitPtrEnd) goto L_Scanline_Done1;

  pc->load(bitWord, mem_ptr(bitPtr));                        //   bitWord = bitPtr[0];
  pc->add(bitPtr, bitPtr, bwSize);                           //   bitPtr += bwSize;
  pc->j(L_BitGap_Match, test_nz(bitWord));                   //   if (bitWord != 0) goto L_BitGap_Match;

  pc->add(xOff, xOff, pixelsPerBitWord);                     //   xOff += pixelsPerBitWord;
  pc->j(L_Scanline_Done1, cmp_eq(bitPtr, bitPtrEnd));        //   if (bitPtr == bitPtrEnd) goto L_Scanline_Done1;

  pc->load(bitWord, mem_ptr(bitPtr));                        //   bitWord = bitPtr[0];
  pc->add(bitPtr, bitPtr, bwSize);                           //   bitPtr += bwSize;
  pc->j(L_BitGap_Cont, test_z(bitWord));                     //   if (bitWord == 0) goto L_BitGap_Cont;

  pc->bind(L_BitGap_Match);                                  // L_BitGap_Match:
  pc->store_zero_reg(mem_ptr(bitPtr, -bwSize));              //   bitPtr[-1] = 0;
  countZeros(i.cloneAs(bitWord), bitWord);                   //   i = ctz(bitWord) or clz(bitWord);
  pc->mov(bitWordTmp, -1);                                   //   bitWordTmp = -1; (all ones)

  if (coverageFormat == PixelCoverageFormat::kPacked)
    pc->s_extract_u8(cMaskAlpha, m[0], 0);                   //   cMaskAlpha = s_extract_u8(m0, 0);
  else
    pc->s_extract_u16(cMaskAlpha, m[0], 0);                  //   cMaskAlpha = s_extract_u16(m0, 0);

  shiftMask(bitWordTmp, bitWordTmp, i);                      //   bitWordTmp = bitWordTmp << i or bitWordTmp >> i;
  pc->shl(i, i, imm(pixelsPerOneBitShift));                  //   i <<= pixelsPerOneBitShift;

  pc->xor_(bitWord, bitWord, bitWordTmp);                    //   bitWord ^= bitWordTmp;
  pc->add(i, i, xOff);                                       //   i += xOff;
  pc->sub(i, i, x0);                                         //   i -= x0;
  pc->add(x0, x0, i);                                        //   x0 += i;
  pc->add_scaled(cellPtr, i.cloneAs(cellPtr), 4);            //   cellPtr += i * sizeof(uint32_t);
  pc->j(L_CLoop_Init, test_nz(cMaskAlpha));                  //   if (cMaskAlpha != 0) goto L_CLoop_Init;

  // Fully-Transparent span where `cMaskAlpha == 0`.

  pc->add_scaled(dstPtr, i.cloneAs(dstPtr), int(dstBpp));    //   dstPtr += i * dstBpp;

  if (vProcPixelCount >= 4)
    compOpPart()->postfetchN();

  compOpPart()->advanceX(x0, i);

  if (vProcPixelCount >= 4)
    compOpPart()->prefetchN();

  pc->j(L_BitScan_Match, test_nz(bitWord));                  //   if (bitWord != 0) goto L_BitScan_Match;
  pc->j(L_BitScan_Iter);                                     //   goto L_BitScan_Iter;

  // CMask - Loop
  // ------------

  pc->bind(L_CLoop_Init);                                    // L_CLoop_Init:
  if (compOpPart()->shouldOptimizeOpaqueFill()) {
    Label L_CLoop_Msk = pc->newLabel();
    pc->j(L_CLoop_Msk, cmp_ne(cMaskAlpha, 255));             //   if (cMaskAlpha != 255) goto L_CLoop_Msk

    compOpPart()->cMaskInitOpaque();
    if (pixelGranularity >= 4)
      compOpPart()->cMaskGranularLoop(i);
    else
      compOpPart()->cMaskGenericLoop(i);
    compOpPart()->cMaskFini();

    pc->j(L_BitScan_Match, test_nz(bitWord));                //   if (bitWord != 0) goto L_BitScan_Match;
    pc->j(L_BitScan_Iter);                                   //   goto L_BitScan_Iter;

    pc->bind(L_CLoop_Msk);                                   // L_CLoop_Msk:
  }

  if (coverageFormat == PixelCoverageFormat::kPacked) {
    pc->v_broadcast_u8(m[0], m[0]);                          //   m0 = [a0 a0 a0 a0 a0 a0 a0 a0|a0 a0 a0 a0 a0 a0 a0 a0]
  }
#if defined(BL_JIT_ARCH_X86)
  else if (!pc->hasAVX2()) {
    pc->v_swizzle_u32x4(m[0], m[0], swizzle(0, 0, 0, 0));    //   m0 = [_0 a0 _0 a0 _0 a0 _0 a0|_0 a0 _0 a0 _0 a0 _0 a0]
  }
#endif
  else {
    pc->v_broadcast_u16(m[0], m[0]);                         //   m0 = [_0 a0 _0 a0 _0 a0 _0 a0|_0 a0 _0 a0 _0 a0 _0 a0]
  }

  compOpPart()->cMaskInit(cMaskAlpha, m[0]);
  if (pixelGranularity >= 4)
    compOpPart()->cMaskGranularLoop(i);
  else
    compOpPart()->cMaskGenericLoop(i);
  compOpPart()->cMaskFini();

  pc->j(L_BitScan_Match, test_nz(bitWord));                  //   if (bitWord != 0) goto L_BitScan_Match;
  pc->j(L_BitScan_Iter);                                     //   goto L_BitScan_Iter;

  // VMask - Tail - Tail `vMask` loop for pixels near the end of the scanline
  // ------------------------------------------------------------------------

  if (maxPixels >= 4u) {
    Label L_VTail_Cont = pc->newLabel();

    Vec m128 = m[0].v128();
    VecArray msk(m128);

    // Tail loop can handle up to `pixelsPerOneBit - 1`.
    if (pixelType == PixelType::kRGBA32) {
      bool hasV256Mask = m[0].size() >= 32u;

      pc->bind(L_VTail_Init);                                // L_VTail_Init:
      pc->add_scaled(cellPtr, i, 4);                         //   cellPtr += i * sizeof(uint32_t);

      if (coverageFormat == PixelCoverageFormat::kUnpacked && !hasV256Mask) {
        pc->v_swap_u64(m[1], m[1]);
      }
      compOpPart()->enterPartialMode();                      //   <CompOpPart::enterPartialMode>

      pc->bind(L_VTail_Cont);                                // L_VTail_Cont:
      compOpPart()->vMaskProcRGBA32Vec(dPix, PixelCount(1), PixelFlags::kPC | PixelFlags::kImmutable, msk, PixelCoverageFlags::kImmutable, pc->emptyPredicate());

      pc->xStorePixel(dstPtr, dPix.pc[0], 1, dstBpp, Alignment(1));
      pc->add(dstPtr, dstPtr, dstBpp);                       //   dstPtr += dstBpp;

      if (coverageFormat == PixelCoverageFormat::kPacked) {
        pc->shiftOrRotateRight(m[0], m[0], 4);               //   m0[15:0] = [????, m[15:4]]
      }
      else {
#if defined(BL_JIT_ARCH_X86)
        if (hasV256Mask) {
          // All 4 expanded masks for ARGB channels are in a single register, so just permute.
          pc->v_swizzle_u64x4(m[0], m[0], swizzle(0, 3, 2, 1));
        }
        else
#endif
        {
          pc->v_interleave_hi_u64(m[0], m[0], m[1]);
        }
      }

      compOpPart()->nextPartialPixel();                      //   <CompOpPart::nextPartialPixel>
      dPix.resetAllExceptTypeAndName();
      pc->j(L_VTail_Cont, sub_nz(i, 1));                     //   if (--i) goto L_VTail_Cont;

      compOpPart()->exitPartialMode();                       //   <CompOpPart::exitPartialMode>
    }
    else if (pixelType == PixelType::kA8) {
      Gp mScalar = pc->newGp32("mScalar");

      pc->bind(L_VTail_Init);                                // L_VTail_Init:
      pc->add_scaled(cellPtr, i, 4);                         //   cellPtr += i * sizeof(uint32_t);
      compOpPart()->enterPartialMode();                      //   <CompOpPart::enterPartialMode>

      pc->bind(L_VTail_Cont);                                // L_VTail_Cont:
      if (coverageFormat == PixelCoverageFormat::kPacked)
        pc->s_extract_u8(mScalar, m128, 0);
      else
        pc->s_extract_u16(mScalar, m128, 0);
      compOpPart()->vMaskProcA8Gp(dPix, PixelFlags::kSA | PixelFlags::kImmutable, mScalar, PixelCoverageFlags::kNone);

      pc->store_u8(mem_ptr(dstPtr), dPix.sa);
      pc->add(dstPtr, dstPtr, dstBpp);                       //   dstPtr += dstBpp;
      if (coverageFormat == PixelCoverageFormat::kPacked)
        pc->shiftOrRotateRight(m128, m128, 1);               //   m0[15:0] = [?, m[15:1]]
      else
        pc->shiftOrRotateRight(m128, m128, 2);               //   m0[15:0] = [??, m[15:2]]
      compOpPart()->nextPartialPixel();                      //   <CompOpPart::nextPartialPixel>
      dPix.resetAllExceptTypeAndName();
      pc->j(L_VTail_Cont, sub_nz(i, 1));                     //   if (--i) goto L_VTail_Cont;

      compOpPart()->exitPartialMode();                       //   <CompOpPart::exitPartialMode>
    }

    // Since this was a tail loop we know that there is nothing to be processed afterwards, because tail loop is only
    // possible at the end of the scanline boundary / clip region.
  }

  // Scanline Iterator
  // -----------------

  // This loop is used to quickly test bitWords in `bitPtr`. In some cases the whole scanline could be empty, so this
  // loop makes sure we won't enter more complicated loops if this happens. It's also used to quickly find the first
  // bit, which is non-zero - in that case it jumps directly to BitScan section.
  //
  // NOTE: Storing zeros to `cellPtr` must be unaligned here as we may be at the end of the scanline.

  pc->bind(L_Scanline_Done0);                                // L_Scanline_Done0:
#if defined(BL_JIT_ARCH_X86)
  pc->v_zero_i(vecZero);                                     //   vecZero = 0;
#endif // BL_JIT_ARCH_X86
  pc->v_storeu128(mem_ptr(cellPtr), vecZero);                //   cellPtr[3:0] = 0;

  pc->bind(L_Scanline_Done1);                                // L_Scanline_Done1:
  deadvanceDstPtrAndCellPtr(dstPtr,                          //   dstPtr -= x0 * dstBpp;
                            cellPtr, x0, dstBpp);            //   cellPtr -= x0 * sizeof(uint32_t);
  pc->j(L_End, sub_z(y, 1));                                 //   if (--y == 0) goto L_End;
  pc->mov(bitPtr, bitPtrEnd);                                //   bitPtr = bitPtrEnd;

  pc->bind(L_Scanline_AdvY);                                 // L_Scanline_AdvY:
  pc->add(dstPtr, dstPtr, dstStride);                        //   dstPtr += dstStride;
  pc->add(bitPtr, bitPtr, bitPtrSkipLen);                    //   bitPtr += bitPtrSkipLen;
  pc->add(cellPtr, cellPtr, cellStride);                     //   cellPtr += cellStride;
  compOpPart()->advanceY();                                  //   <CompOpPart::AdvanceY>

  pc->bind(L_Scanline_Init);                                 // L_Scanline_Init:
  pc->mov(xOff, xStart);                                     //   xOff = xStart;
  pc->add(bitPtrEnd, bitPtr, bitPtrRunLen);                  //   bitPtrEnd = bitPtr + bitPtrRunLen;

  pc->bind(L_Scanline_Iter);                                 // L_Scanline_Iter:
  pc->load(bitWord, mem_ptr(bitPtr));                        //   bitWord = bitPtr[0];
  pc->add(bitPtr, bitPtr, bwSize);                           //   bitPtr += bwSize;
  pc->j(L_BitScan_Init, test_nz(bitWord));                   //   if (bitWord != 0) goto L_BitScan_Init;

  pc->add(xOff, xOff, pixelsPerBitWord);                     //   xOff += pixelsPerBitWord;
  pc->j(L_Scanline_Iter, cmp_ne(bitPtr, bitPtrEnd));         //   if (bitPtr != bitPtrEnd) goto L_Scanline_Iter;
  pc->j(L_Scanline_AdvY, sub_nz(y, 1));                      //   if (--y) goto L_Scanline_AdvY;

  // Epilog
  // ------

  pc->bind(L_End);
  compOpPart()->fini();
  _finiGlobalHook();
}

void FillAnalyticPart::accumulateCoverages(const Vec& acc) noexcept {
  Vec tmp = pc->newSimilarReg<Vec>(acc, "vCovTmp");

  pc->v_sllb_u128(tmp, acc, 4);                              //   tmp[7:0]  = [  c6    c5    c4    0  |  c2    c1    c0    0  ];
  pc->v_add_i32(acc, acc, tmp);                              //   acc[7:0]  = [c7:c6 c6:c5 c5:c4   c4 |c3:c2 c2:c1 c1:c0   c0 ];
  pc->v_sllb_u128(tmp, acc, 8);                              //   tmp[7:0]  = [c5:c4   c4    0     0  |c1:c0   c0    0     0  ];
  pc->v_add_i32(acc, acc, tmp);                              //   acc[7:0]  = [c7:c4 c6:c4 c5:c4   c4 |c3:c0 c2:c0 c1:c0   c0 ];

#if defined(BL_JIT_ARCH_X86)
  if (acc.isVec256()) {
    pc->v_swizzle_u32x4(tmp.v128(), acc.v128(), swizzle(3, 3, 3, 3));
    cc->vperm2i128(tmp, tmp, tmp, perm2x128Imm(Perm2x128::kALo, Perm2x128::kZero));
    pc->v_add_i32(acc, acc, tmp);                            //   acc[7:0]  = [c7:c0 c6:c0 c5:c0 c4:c0|c3:c0 c2:c0 c1:c0   c0 ];
  }
#endif // BL_JIT_ARCH_X86
}

void FillAnalyticPart::normalizeCoverages(const Vec& acc) noexcept {
  pc->v_srlb_u128(acc, acc, 12);                             //   acc[3:0]  = [  0     0     0     c0 ];
}

// Calculate masks from cell and store them to a vector of the following layout:
//
//   [__ __ __ __ a7 a6 a5 a4|__ __ __ __ a3 a2 a1 a0]
//
// NOTE: Depending on the vector size the output mask is for either 4 or 8 pixels.
void FillAnalyticPart::calcMasksFromCells(const Vec& msk_, const Vec& acc, const Vec& fillRuleMask, const Vec& globalAlpha) noexcept {
  Vec msk = msk_.cloneAs(acc);

#if defined(BL_JIT_ARCH_X86)
  // This implementation is a bit tricky. In the original AGG and FreeType `A8_SHIFT + 1` is used. However, we don't do
  // that and mask out the last bit through `fillRuleMask`. The reason we do this is that our `globalAlpha` is already
  // pre-shifted by `7` bits left and we only need to shift the final mask by one bit left after it's been calculated.
  // So instead of shifting it left later we clear the LSB bit now and that's it, we saved one instruction.
  pc->v_srai_i32(msk, acc, A8Info::kShift);
  pc->v_and_i32(msk, msk, fillRuleMask);

  // We have to make sure that the cleared LSB bit stays zero. Since we only use SUB with even value and abs we are
  // fine. However, that packing would not be safe if there was no "v_min_i16", which makes sure we are always safe.
  Operand i_0x00000200 = pc->simdConst(&ct.i_0000020000000200, Bcst::k32, msk);
  pc->v_sub_i32(msk, msk, i_0x00000200);
  pc->v_abs_i32(msk, msk);

  if (pc->hasSSE4_1()) {
    // This is not really faster, but it uses the same constant as one of the previous operations, potentially saving
    // us a register.
    pc->v_min_u32(msk, msk, i_0x00000200);
    pc->v_packs_i32_i16(msk, msk, msk);
  }
  else {
    pc->v_packs_i32_i16(msk, msk, msk);
    pc->v_min_i16(msk, msk, pc->simdConst(&ct.i_0200020002000200, Bcst::kNA, msk));
  }

  // Multiply masks by global alpha, this would output masks in [0, 255] range.
  pc->v_mulh_u16(msk, msk, globalAlpha);
#else
  // This implementation doesn't need any tricks as a lot of SIMD primitives are just provided natively.
  pc->v_srai_i32(msk, acc, A8Info::kShift + 1);
  pc->v_and_i32(msk, msk, fillRuleMask);

  pc->v_sub_i32(msk, msk, pc->simdConst(&ct.i_0000010000000100, Bcst::k32, msk));
  pc->v_abs_i32(msk, msk);
  pc->v_min_u32(msk, msk, pc->simdConst(&ct.i_0000010000000100, Bcst::kNA, msk));

  pc->v_mul_u16(msk, msk, globalAlpha);
  pc->cc->shrn(msk.h4(), msk.s4(), 8);
#endif
}

void FillAnalyticPart::expandMask(const VecArray& msk, PixelCount pixelCount) noexcept {
  PixelType pixelType = compOpPart()->pixelType();
  PixelCoverageFormat coverageFormat = compOpPart()->coverageFormat();

  if (pixelType == PixelType::kRGBA32) {
    switch (coverageFormat) {
#if defined(BL_JIT_ARCH_A64)
      case PixelCoverageFormat::kPacked: {
        uint32_t nRegs = (pixelCount.value() + 3u) / 4u;
        for (uint32_t i = 0; i < nRegs; i++) {
          Vec v = msk[i].v128();
          pc->v_swizzlev_u8(v, v, pc->simdConst(&pc->ct.swizu8_xxxxxxxxx3x2x1x0_to_3333222211110000, Bcst::kNA, v));
        }
        return;
      }
#endif // BL_JIT_ARCH_A64

      case PixelCoverageFormat::kUnpacked: {
        if (pixelCount == 4) {
          Vec cov0_128 = msk[0].v128();

          pc->v_interleave_lo_u16(cov0_128, cov0_128, cov0_128);        //   msk[0] = [a3 a3 a2 a2 a1 a1 a0 a0]
#if defined(BL_JIT_ARCH_X86)
          if (msk[0].isVec256()) {
            pc->v_swizzle_u64x4(msk[0], msk[0], swizzle(1, 1, 0, 0));   //   msk[0] = [a3 a3 a2 a2 a3 a3 a2 a2|a1 a1 a0 a0 a1 a1 a0 a0]
            pc->v_swizzle_u32x4(msk[0], msk[0], swizzle(1, 1, 0, 0));   //   msk[0] = [a3 a3 a3 a3 a2 a2 a2 a2|a1 a1 a1 a1 a0 a0 a0 a0]
          }
          else
#endif // BL_JIT_ARCH_X86
          {
            pc->v_swizzle_u32x4(msk[1], msk[0], swizzle(3, 3, 2, 2));   //   msk[0] = [a3 a3 a3 a3 a2 a2 a2 a2]
            pc->v_swizzle_u32x4(msk[0], msk[0], swizzle(1, 1, 0, 0));   //   msk[0] = [a1 a1 a1 a1 a0 a0 a0 a0]
          }

          return;
        }

#if defined(BL_JIT_ARCH_X86)
        if (pixelCount == 8) {
          if (msk[0].isVec512()) {
            if (pc->hasAVX512_VBMI()) {
              Vec pred = pc->simdVecConst(&pc->ct.permu8_4xa8_lo_to_rgba32_uc, Bcst::kNA_Unique, msk[0]);
              pc->v_permute_u8(msk[0], pred, msk[0]);                   //   msk[0] = [4x00a7 4x00a6 4x00a5 4x00a4|4x00a3 4x00a2 4x00a1 4x00a0]
            }
            else {
              Vec msk_256 = msk[0].v256();
              Operand pred = pc->simdConst(&pc->ct.swizu8_xxxxxxxxx3x2x1x0_to_3333222211110000, Bcst::kNA, msk_256);
              pc->v_swizzlev_u8(msk_256, msk_256, pred);                //   msk[0] = [2xa7a7 3xa6a6 3xa5a5 2xa4a4|2xa3a3 2xa2a2 2xa1a1 2xa0a0]
              pc->v_cvt_u8_lo_to_u16(msk[0], msk_256);                  //   msk[0] = [4x00a7 4x00a6 4x00a5 4x00a4|4x00a3 4x00a2 4x00a1 4x00a0]
            }
          }
          else {
            //                                                               msk[0] = [__ __ __ __ a7 a6 a5 a4|__ __ __ __ a3 a2 a1 a0]
            pc->v_interleave_lo_u16(msk[0], msk[0], msk[0]);            //   msk[0] = [a7 a7 a6 a6 a5 a5 a4 a4|a3 a3 a2 a2 a1 a1 a0 a0]
            pc->v_swizzle_u64x4(msk[1], msk[0], swizzle(3, 3, 2, 2));   //   msk[1] = [a7 a7 a6 a6 a7 a7 a6 a6|a5 a5 a4 a4 a5 a5 a4 a4]
            pc->v_swizzle_u64x4(msk[0], msk[0], swizzle(1, 1, 0, 0));   //   msk[0] = [a3 a3 a2 a2 a3 a3 a2 a2|a1 a1 a0 a0 a1 a1 a0 a0]
            pc->v_interleave_lo_u32(msk[0], msk[0], msk[0]);            //   msk[0] = [a3 a3 a3 a3 a2 a2 a2 a2|a1 a1 a1 a1 a0 a0 a0 a0]
            pc->v_interleave_lo_u32(msk[1], msk[1], msk[1]);            //   msk[1] = [a7 a7 a7 a7 a6 a6 a6 a6|a5 a5 a5 a5 a4 a4 a4 a4]
          }
          return;
        }
#endif // BL_JIT_ARCH_X86

        break;
      }

      default:
        BL_NOT_REACHED();
    }
  }
  else if (pixelType == PixelType::kA8) {
    switch (coverageFormat) {
      case PixelCoverageFormat::kPacked: {
        if (pixelCount <= 8u) {
          Vec v = msk[0].v128();
          pc->v_packs_i16_u8(v, v, v);
          return;
        }

        break;
      }

      case PixelCoverageFormat::kUnpacked: {
        if (pixelCount <= 4)
          return;

#if defined(BL_JIT_ARCH_X86)
        // We have to convert from:
        //   msk = [?? ?? ?? ?? a7 a6 a5 a4|?? ?? ?? ?? a3 a2 a1 a0]
        // To:
        //   msk = [a7 a6 a5 a4 a3 a2 a1 a0|a7 a6 a5 a4 a3 a2 a1 a0]
        pc->v_swizzle_u64x4(msk[0].ymm(), msk[0].ymm(), swizzle(2, 0, 2, 0));
#endif // BL_JIT_ARCH_X86

        return;
      }

      default:
        BL_NOT_REACHED();
    }
  }

  BL_NOT_REACHED();
}

void FillAnalyticPart::deadvanceDstPtrAndCellPtr(const Gp& dstPtr, const Gp& cellPtr, const Gp& x, uint32_t dstBpp) noexcept {
  Gp xAdv = x.cloneAs(dstPtr);

#if defined(BL_JIT_ARCH_A64)
  pc->cc->sub(cellPtr, cellPtr, xAdv, a64::lsl(2));
  if (asmjit::Support::isPowerOf2(dstBpp)) {
    uint32_t shift = asmjit::Support::ctz(dstBpp);
    pc->cc->sub(dstPtr, dstPtr, xAdv, a64::lsl(shift));
  }
  else {
    pc->mul(xAdv, xAdv, dstBpp);
    pc->sub(dstPtr, dstPtr, xAdv);
  }
#else
  if (dstBpp == 1) {
    pc->sub(dstPtr, dstPtr, xAdv);
    pc->shl(xAdv, xAdv, 2);
    pc->sub(cellPtr, cellPtr, xAdv);
  }
  else if (dstBpp == 2) {
    pc->shl(xAdv, xAdv, 1);
    pc->sub(dstPtr, dstPtr, xAdv);
    pc->shl(xAdv, xAdv, 1);
    pc->sub(cellPtr, cellPtr, xAdv);
  }
  else if (dstBpp == 4) {
    pc->shl(xAdv, xAdv, 2);
    pc->sub(dstPtr, dstPtr, xAdv);
    pc->sub(cellPtr, cellPtr, xAdv);
  }
  else {
    Gp dstAdv = pc->newGpPtr("dstAdv");
    pc->mul(dstAdv, xAdv, dstBpp);
    pc->shl(xAdv, xAdv, 2);
    pc->sub(dstPtr, dstPtr, dstAdv);
    pc->sub(cellPtr, cellPtr, xAdv);
  }
#endif
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
