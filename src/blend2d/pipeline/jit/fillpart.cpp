// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if defined(BL_JIT_ARCH_X86)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fillpart_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchpixelptrpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

// bl::Pipeline::JIT::FillPart - Construction & Destruction
// ========================================================

FillPart::FillPart(PipeCompiler* pc, FillType fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : PipePart(pc, PipePartType::kFill),
    _fillType(fillType),
    _isRectFill(false) {

  // Initialize the children of this part.
  _children[kIndexDstPart] = dstPart;
  _children[kIndexCompOpPart] = compOpPart;
  _childCount = 2;
}

// [[pure virtual]]
void FillPart::compile() noexcept { BL_NOT_REACHED(); }

// bl::Pipeline::JIT::FillBoxAPart - Construction & Destruction
// ============================================================

FillBoxAPart::FillBoxAPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : FillPart(pc, FillType::kBoxA, dstPart, compOpPart) {

  _maxSimdWidthSupported = SimdWidth::k512;
  _isRectFill = true;
}

// bl::Pipeline::JIT::FillBoxAPart - Compile
// =========================================

void FillBoxAPart::compile() noexcept {
  // Prepare
  // -------

  _initGlobalHook(cc->cursor());

  int dstBpp = int(dstPart()->bpp());

  // Local Registers
  // ---------------

  Gp ctxData = pc->_ctxData;                          // Reg/Init.
  Gp fillData = pc->_fillData;                        // Reg/Init.

  Gp dstPtr = pc->newGpPtr("dstPtr");                 // Reg.
  Gp dstStride = pc->newGpPtr("dstStride");           // Reg/Mem.

  Gp x = pc->newGp32("x");                            // Reg.
  Gp y = pc->newGp32("y");                            // Reg/Mem.
  Gp w = pc->newGp32("w");                            // Reg/Mem.
  Gp ga_sm = pc->newGp32("ga.sm");                    // Reg/Tmp.

  // Prolog
  // ------

  pc->load(dstPtr, x86::ptr(ctxData, BL_OFFSET_OF(ContextData, dst.stride)));
  pc->load_u32(y, x86::ptr(fillData, BL_OFFSET_OF(FillData::BoxA, box.y0)));

  pc->mov(dstStride, dstPtr);
  pc->load_u32(w, x86::ptr(fillData, BL_OFFSET_OF(FillData::BoxA, box.x0)));
  pc->mul(dstPtr, dstPtr, y.cloneAs(dstPtr));

  dstPart()->initPtr(dstPtr);
  compOpPart()->init(w, y, 1);

  pc->lea_bpp(dstPtr, dstPtr, w, uint32_t(dstBpp));
  pc->add(dstPtr, dstPtr, x86::ptr(ctxData, BL_OFFSET_OF(ContextData, dst.pixelData)));
  pc->sub(w, x86::ptr(fillData, BL_OFFSET_OF(FillData::BoxA, box.x1)), w);
  pc->mul(x, w, dstBpp);
  pc->sub(y, x86::ptr(fillData, BL_OFFSET_OF(FillData::BoxA, box.y1)), y);
  pc->sub(dstStride, dstStride, x.cloneAs(dstStride));

  // Loop
  // ----

  if (compOpPart()->shouldOptimizeOpaqueFill()) {
    Label L_FullAlphaIter = pc->newLabel();
    Label L_SemiAlphaInit = pc->newLabel();
    Label L_SemiAlphaIter = pc->newLabel();

    Label L_End  = pc->newLabel();

    pc->load_u32(ga_sm, x86::ptr(fillData, BL_OFFSET_OF(FillData::BoxA, alpha)));
    pc->j(L_SemiAlphaInit, cmp_ne(ga_sm, 255));

    // Full Alpha
    // ----------

    compOpPart()->cMaskInitOpaque();

    pc->bind(L_FullAlphaIter);
    pc->mov(x, w);

    compOpPart()->startAtX(pc->_gpNone);
    compOpPart()->cMaskGenericLoop(x);

    pc->add(dstPtr, dstPtr, dstStride);
    compOpPart()->advanceY();
    pc->j(L_FullAlphaIter, sub_nz(y, 1));

    compOpPart()->cMaskFini();
    pc->j(L_End);

    // Semi Alpha
    // ----------

    pc->bind(L_SemiAlphaInit);
    compOpPart()->cMaskInit(ga_sm, Vec());

    pc->bind(L_SemiAlphaIter);
    pc->mov(x, w);

    compOpPart()->startAtX(pc->_gpNone);
    compOpPart()->cMaskGenericLoop(x);

    pc->add(dstPtr, dstPtr, dstStride);
    compOpPart()->advanceY();
    pc->j(L_SemiAlphaIter, sub_nz(y, 1));

    compOpPart()->cMaskFini();
    pc->bind(L_End);
  }
  else {
    Label L_AnyAlphaLoop = pc->newLabel();

    compOpPart()->cMaskInit(x86::ptr_32(fillData, BL_OFFSET_OF(FillData::BoxA, alpha)));

    pc->bind(L_AnyAlphaLoop);
    pc->mov(x, w);

    compOpPart()->startAtX(pc->_gpNone);
    compOpPart()->cMaskGenericLoop(x);

    pc->add(dstPtr, dstPtr, dstStride);
    compOpPart()->advanceY();
    pc->j(L_AnyAlphaLoop, sub_nz(y, 1));

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

  _maxSimdWidthSupported = SimdWidth::k512;
}

// bl::Pipeline::JIT::FillMaskPart - Compile
// =========================================

void FillMaskPart::compile() noexcept {
  // EndOrRepeat is expected to be zero for fast termination of the scanline.
  BL_STATIC_ASSERT(uint32_t(MaskCommandType::kEndOrRepeat) == 0);

  using x86::shuffleImm;

  // Prepare
  // -------

  _initGlobalHook(cc->cursor());

  int dstBpp = int(dstPart()->bpp());
  constexpr int kMaskCmdSize = int(sizeof(MaskCommand));
  constexpr int labelAlignment = 8;

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

  Gp ctxData = pc->_ctxData;                          // Reg/Init.
  Gp fillData = pc->_fillData;                        // Reg/Init.

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

  GlobalAlpha ga;                                     // Reg/Mem.
  GlobalAlpha gaNotApplied;                           // None.

  // Prolog
  // ------

  // Initialize the destination.
  pc->load_u32(y, x86::ptr(fillData, BL_OFFSET_OF(FillData, mask.box.y0)));
  pc->load(dstStride, x86::ptr(ctxData, BL_OFFSET_OF(ContextData, dst.stride)));

  pc->mul(dstPtr, y.cloneAs(dstPtr), dstStride);
  pc->add(dstPtr, dstPtr, x86::ptr(ctxData, BL_OFFSET_OF(ContextData, dst.pixelData)));

  // Initialize pipeline parts.
  dstPart()->initPtr(dstPtr);
  compOpPart()->init(pc->_gpNone, y, 1);

  // Initialize mask pointers.
  pc->load(cmdPtr, x86::ptr(fillData, BL_OFFSET_OF(FillData, mask.maskCommandData)));

  // Initialize global alpha.
  ga.initFromMem(pc, x86::ptr_32(fillData, BL_OFFSET_OF(FillData, mask.alpha)));

  // y = fillData->box.y1 - fillData->box.y0;
  pc->sub(y, x86::ptr_32(fillData, BL_OFFSET_OF(FillData, mask.box.y1)), y);
  pc->j(L_ScanlineInit);

  // Scanline Done
  // -------------

  Gp repeat = pc->newGp32("repeat");

  pc->align(AlignMode::kCode, labelAlignment);
  pc->bind(L_ScanlineDone);
  disadvanceDstPtr(dstPtr, x, int(dstBpp));

  pc->bind(L_ScanlineSkip);
  pc->load_u32(repeat, x86::ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _x0)));
  pc->j(L_End, sub_z(y, 1));

  pc->add(cmdPtr, cmdPtr, kMaskCmdSize);
  pc->add(dstPtr, dstPtr, dstStride);
  compOpPart()->advanceY();
  pc->store_32(x86::ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _x0) - kMaskCmdSize), repeat);
  pc->cmov(cmdPtr, cmdBegin, sub_nz(repeat, 1));

  // Scanline Init
  // -------------

  pc->bind(L_ScanlineInit);
  pc->load_u32(cmdType, x86::ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _x1AndType)));
  pc->mov(cmdBegin, cmdPtr);
  pc->load_u32(x, x86::ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _x0)));
  // This is not really common, but it's possible to skip entire scanlines with `kEndOrRepeat`.
  pc->j(L_ScanlineSkip, test_z(cmdType, MaskCommand::kTypeMask));

  pc->add_scaled(dstPtr, x.cloneAs(dstPtr), dstBpp);
  compOpPart()->startAtX(x);
  pc->j(L_ProcessCmd);

  // Process Command
  // ---------------

  pc->bind(L_ProcessNext);
  pc->load_u32(cmdType, x86::ptr(cmdPtr, kMaskCmdSize + BL_OFFSET_OF(MaskCommand, _x1AndType)));
  pc->load_u32(i, x86::ptr(cmdPtr, kMaskCmdSize + BL_OFFSET_OF(MaskCommand, _x0)));
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
  if (pc->hasBMI2() && pc->is64Bit()) {
    cc->rorx(i.r64(), cmdType.r64(), MaskCommand::kTypeBits);
  }
  else {
    pc->shr(i, cmdType, MaskCommand::kTypeBits);
  }

  pc->and_(cmdType, cmdType, MaskCommand::kTypeMask);
  pc->sub(i, i, x);
  pc->load(maskValue, x86::ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _value.data)));
  pc->add(x, x, i);

  // We know the command is not kEndOrRepeat, which allows this little trick.
  pc->j(L_CMaskInit, cmp_eq(cmdType, uint32_t(MaskCommandType::kCMask)));

  // VMask Command
  // -------------

  // Increments the advance in the mask command in case it would be repeated.
  pc->load(maskAdvance, x86::ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _maskAdvance)));
  cc->add(x86::ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _value.ptr)), maskAdvance);

  pc->j(L_VMaskA8WithoutGA, cmp_eq(cmdType, uint32_t(MaskCommandType::kVMaskA8WithoutGA)));
  compOpPart()->vMaskGenericLoop(i, dstPtr, maskValue, gaNotApplied, L_ProcessNext);

  pc->bind(L_VMaskA8WithoutGA);
  compOpPart()->vMaskGenericLoop(i, dstPtr, maskValue, ga, L_ProcessNext);

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

void FillMaskPart::disadvanceDstPtr(const Gp& dstPtr, const Gp& x, int dstBpp) noexcept {
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

  _maxSimdWidthSupported = SimdWidth::k256;
}

// bl::Pipeline::JIT::FillAnalyticPart - Compile
// =============================================

void FillAnalyticPart::compile() noexcept {
  using x86::shuffleImm;

  // Prepare
  // -------

  _initGlobalHook(cc->cursor());

  PixelType pixelType = compOpPart()->pixelType();
  uint32_t dstBpp = dstPart()->bpp();
  uint32_t maxPixels = compOpPart()->maxPixels();

  // vProc SIMD width describes SIMD width used to accumulate coverages and then to calculate alpha masks. In
  // general if we only calculate 4 coverages at once we only need 128-bit SIMD. However, 8 and more coverages
  // need 256-bit SIMD or higher, if available. At the moment we use always a single register for this purpose,
  // so SIMD width determines how many pixels we can process in a vMask loop at a time.
  uint32_t vProcPixelCount = 0;
  SimdWidth vProcWidth = pc->simdWidth();

  if (pc->simdWidth() >= SimdWidth::k256 && maxPixels >= 8) {
    vProcPixelCount = 8;
    vProcWidth = SimdWidth::k256;
  }
  else {
    vProcPixelCount = blMin<uint32_t>(maxPixels, 4);;
    vProcWidth = SimdWidth::k128;
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

  Label L_VTail_Init = pc->newLabel(); // Only used if maxPixels >= 4.
  Label L_CLoop_Init = pc->newLabel();

  Label L_Scanline_Done0 = pc->newLabel();
  Label L_Scanline_Done1 = pc->newLabel();
  Label L_Scanline_AdvY = pc->newLabel();
  Label L_Scanline_Iter = pc->newLabel();
  Label L_Scanline_Init = pc->newLabel();

  Label L_End = pc->newLabel();

  // Local Registers
  // ---------------

  Gp ctxData = pc->_ctxData;                                 // Init.
  Gp fillData = pc->_fillData;                               // Init.

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

  Vec cov = pc->newVec(vProcWidth, "cov");                   // Reg.
  Vec globalAlpha = pc->newVec(vProcWidth, "globalAlpha");   // Mem.
  Vec fillRuleMask = pc->newVec(vProcWidth, "fillRuleMask"); // Mem.
  Xmm vecZero = cc->newXmm("vec_zero");                      // Reg/Tmp.

  Pixel dPix("d", pixelType);                                // Reg.

  VecArray m;                                                // Reg.
  pc->newVecArray(m, 2, pc->simdWidth(), "m");

  // Prolog
  // ------

  // Initialize the destination.
  pc->load_u32(y, x86::ptr(fillData, BL_OFFSET_OF(FillData::Analytic, box.y0)));
  pc->load(dstStride, x86::ptr(ctxData, BL_OFFSET_OF(ContextData, dst.stride)));

  pc->mul(dstPtr, y.cloneAs(dstPtr), dstStride);
  pc->add(dstPtr, dstPtr, x86::ptr(ctxData, BL_OFFSET_OF(ContextData, dst.pixelData)));

  // Initialize cell pointers.
  pc->load(bitPtrSkipLen, x86::ptr(fillData, BL_OFFSET_OF(FillData::Analytic, bitStride)));
  pc->load(cellStride, x86::ptr(fillData, BL_OFFSET_OF(FillData::Analytic, cellStride)));

  pc->load(bitPtr, x86::ptr(fillData, BL_OFFSET_OF(FillData::Analytic, bitTopPtr)));
  pc->load(cellPtr, x86::ptr(fillData, BL_OFFSET_OF(FillData::Analytic, cellTopPtr)));

  // Initialize pipeline parts.
  dstPart()->initPtr(dstPtr);
  compOpPart()->init(pc->_gpNone, y, uint32_t(pixelGranularity));

  // y = fillData->box.y1 - fillData->box.y0;
  pc->sub(y, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::Analytic, box.y1)), y);

  // Decompose the original `bitStride` to bitPtrRunLen + bitPtrSkipLen, where:
  //   - `bitPtrRunLen` - Number of BitWords (in byte units) active in this band.
  //   - `bitPtrRunSkip` - Number of BitWords (in byte units) to skip for this band.
  pc->shr(xStart, x86::ptr(fillData, BL_OFFSET_OF(FillData::Analytic, box.x0)), pixelsPerBitWordShift);
  pc->load_u32(xEnd, x86::ptr(fillData, BL_OFFSET_OF(FillData::Analytic, box.x1)));
  pc->shr(bitPtrRunLen.r32(), xEnd, pixelsPerBitWordShift);

  pc->sub(bitPtrRunLen.r32(), bitPtrRunLen.r32(), xStart);
  pc->inc(bitPtrRunLen.r32());
  pc->shl(bitPtrRunLen, bitPtrRunLen, IntOps::ctz(bwSize));
  pc->sub(bitPtrSkipLen, bitPtrSkipLen, bitPtrRunLen);

  // Make `xStart` to become the X offset of the first active BitWord.
  pc->lea(bitPtr, x86::ptr(bitPtr, xStart.cloneAs(bitPtr), IntOps::ctz(bwSize)));
  pc->shl(xStart, xStart, pixelsPerBitWordShift);

  pc->v_broadcast_u16(globalAlpha, x86::ptr(fillData, BL_OFFSET_OF(FillData::Analytic, alpha)));
  // We shift left by 7 bits so we can use `pmulhuw` in `calcMasksFromCells()`.
  pc->v_sll_i16(globalAlpha, globalAlpha, 7);

  // Initialize fill-rule.
  pc->v_broadcast_u32(fillRuleMask, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::Analytic, fillRuleMask)));
  pc->j(L_Scanline_Init);

  // BitScan
  // -------

  // Called by Scanline iterator on the first non-zero BitWord it matches. The responsibility of BitScan is to find
  // the first bit in the passed BitWord followed by matching the bit that ends this match. This would essentially
  // produce the first [x0, x1) span that has to be composited as 'VMask' loop.

  pc->bind(L_BitScan_Init);                                  // L_BitScan_Init:
  pc->ctz(x0.cloneAs(bitWord), bitWord);                     //   x0 = ctz(bitWord);
  cc->mov(x86::ptr(bitPtr, -bwSize, uint32_t(bwSize)), 0);   //   bitPtr[-1] = 0;
  pc->mov(bitWordTmp, -1);                                   //   bitWordTmp = -1; (all ones).
  pc->shl(bitWordTmp, bitWordTmp, x0);                       //   bitWordTmp <<= x0;

  // Convert bit offset `x0` into a pixel offset. We must consider `xOff` as it's only zero for the very first
  // BitWord (all others are multiplies of `pixelsPerBitWord`).

  pc->shl(x0, x0, pixelsPerOneBitShift);                     //   x0 <<= pixelsPerOneBitShift;
  pc->add(x0, x0, xOff);                                     //   x0 += xOff;

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

  pc->v_load_i32(cov, pc->_getMemConst(&ct.i_0002000000020000));

  // If `bitWord ^ bitWordTmp` results in non-zero value it means that the current span ends within the same BitWord,
  // otherwise the span crosses multiple BitWords.

  pc->j(L_BitScan_Match, xor_nz(bitWord, bitWordTmp));       //   if ((bitWord ^= bitWordTmp) != 0) goto L_BitScan_Match;

  // Okay, so the span crosses multiple BitWords. Firstly we have to make sure this was not the last one. If that's
  // the case we must terminate the scanning immediately.

  pc->mov(i, bwSizeInBits);                                  //   i = bwSizeInBits;
  pc->j(L_BitScan_End, cmp_eq(bitPtr, bitPtrEnd));           //   if (bitPtr == bitPtrEnd) goto L_BitScan_End;

  // A BitScan loop - iterates over all consecutive BitWords and finds those that don't have all bits set to 1.

  pc->bind(L_BitScan_Iter);                                  // L_BitScan_Iter:
  pc->load(bitWord, x86::ptr(bitPtr, 0, uint32_t(bwSize)));  //   bitWord = bitPtr[0];
  cc->mov(x86::ptr(bitPtr, 0, uint32_t(bwSize)), 0);         //   bitPtr[0] = 0;
  pc->add(xOff, xOff, pixelsPerBitWord);                     //   xOff += pixelsPerBitWord;
  pc->lea(bitPtr, x86::ptr(bitPtr, bwSize));                 //   bitPtr += bwSize;
  pc->j(L_BitScan_Match, xor_nz(bitWord, -1));               //   if ((bitWord ^= -1) != 0) goto L_BitScan_Match;
  pc->j(L_BitScan_End, cmp_eq(bitPtr, bitPtrEnd));           //   if (bitPtr == bitPtrEnd) goto L_BitScan_End;
  pc->j(L_BitScan_Iter);                                     //   goto L_BitScan_Iter;

  pc->bind(L_BitScan_Match);                                 // L_BitScan_Match:
  pc->ctz(i.cloneAs(bitWord), bitWord);                      //   i = ctz(bitWord);

  pc->bind(L_BitScan_End);                                   // L_BitScan_End:
  if (vProcPixelCount == 8)
    pc->v_add_i32(cov.ymm(), cov.ymm(), x86::ptr(cellPtr));  //   acc[7:0] += cellPtr[7:0];
  else
    pc->v_add_i32(cov.xmm(), cov.xmm(), x86::ptr(cellPtr));  //   acc[3:0] += cellPtr[3:0];
  pc->mov(bitWordTmp, -1);                                   //   bitWordTmp = -1; (all ones).
  pc->shl(bitWordTmp, bitWordTmp, i);                        //   bitWordTmp <<= i;
  pc->shl(i, i, pixelsPerOneBitShift);                       //   i <<= pixelsPerOneBitShift;

  pc->xor_(bitWord, bitWord, bitWordTmp);                    //   bitWord ^= bitWordTmp;
  pc->add(i, i, xOff);                                       //   i += xOff;

  // In cases where the raster width is not a multiply of `pixelsPerOneBit` we must make sure we won't overflow it.

  pc->umin(i, i, xEnd);                                      //   i = min(i, xEnd);
  pc->v_zero_i(vecZero);                                     //   vecZero = 0;
  pc->v_storea_i128(x86::ptr(cellPtr), vecZero);             //   cellPtr[3:0] = 0;

  // `i` is now the number of pixels (and cells) to composite by using `vMask`.

  pc->sub(i, i, x0);                                         //   i -= x0;
  pc->add(x0, x0, i);                                        //   x0 += i;
  pc->j(L_VLoop_Init);                                       //   goto L_VLoop_Init;

  // VLoop - Main VMask Loop - 8 Pixels (256-bit SIMD)
  // -------------------------------------------------

  if (vProcPixelCount == 8u) {
    Label L_VLoop_Iter8 = pc->newLabel();
    Label L_VLoop_End = pc->newLabel();

    pc->bind(L_VLoop_Iter8);                                 // L_VLoop_Iter8:
    cc->vextracti128(cov.xmm(), cov, 1);

    if (pixelType == PixelType::kRGBA32) {
      VecArray vm(m);
      pc->v_interleave_lo_u32(m[1], m[1], m[1]);             //   m1 = [M7 M7 M7 M7 M6 M6 M6 M6|M5 M5 M5 M5 M4 M4 M4 M4]
      compOpPart()->vMaskProcStoreAdvance(dstPtr, PixelCount(8), vm, false);
    }
    else if (pixelType == PixelType::kA8) {
      VecArray vm(m[0].xmm());
      compOpPart()->vMaskProcStoreAdvance(dstPtr, PixelCount(8), vm, false);
    }

    pc->add(cellPtr, cellPtr, 8 * 4);                        //   cellPtr += 8 * sizeof(uint32_t);
    pc->v_add_i32(cov, cov, x86::ptr(cellPtr));              //   cov[7:0] += cellPtr[7:0]
    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
    pc->v_storeu_i256(x86::ptr(cellPtr, -16), vecZero.ymm());//   cellPtr[3:-4] = 0;

    pc->bind(L_VLoop_Init);                                  // L_VLoop_Init:
    accumulateCoverages(cov);
    calcMasksFromCells(m[0], cov, fillRuleMask, globalAlpha);
    normalizeCoverages(cov);

    if (pixelType == PixelType::kRGBA32) {
      pc->v_interleave_lo_u16(m[0], m[0], m[0]);             //   m0 = [M7 M7 M6 M6 M5 M5 M4 M4|M3 M3 M2 M2 M1 M1 M0 M0]
      pc->v_perm_i64(m[1], m[0], shuffleImm(3, 3, 2, 2));    //   m1 = [M7 M7 M6 M6 M7 M7 M6 M6|M5 M5 M4 M4 M5 M5 M4 M4]
      pc->v_perm_i64(m[0], m[0], shuffleImm(1, 1, 0, 0));    //   m0 = [M3 M3 M2 M2 M3 M3 M2 M2|M1 M1 M0 M0 M1 M1 M0 M0]
      pc->v_interleave_lo_u32(m[0], m[0], m[0]);             //   m0 = [M3 M3 M3 M3 M2 M2 M2 M2|M1 M1 M1 M1 M0 M0 M0 M0]
    }
    else if (pixelType == PixelType::kA8) {
      pc->v_perm_i64(m[0], m[0], shuffleImm(2, 0, 2, 0));    //   m0 = [M7 M6 M5 M4 M3 M2 M1 M0|M7 M6 M5 M4 M3 M2 M1 M0]
    }

    pc->j(L_VLoop_Iter8, sub_nc(i, 8));                      //   if ((i -= 8) >= 0) goto L_VLoop_Iter8;
    pc->j(L_VLoop_End, add_z(i, 8));                         //   if ((i += 8) == 0) goto L_VLoop_End;
    pc->j(L_VTail_Init, ucmp_lt(i, 4));                      //   if (i < 4) goto L_VTail_Init;

    pc->add(cellPtr, cellPtr, 4 * 4);                        //   cellPtr += 4 * sizeof(uint32_t);
    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
    pc->v_storea_i128(x86::ptr(cellPtr), vecZero.xmm());     //   cellPtr[3:0] = 0;

    if (pixelType == PixelType::kRGBA32) {
      VecArray vm(m[0]);
      compOpPart()->vMaskProcStoreAdvance(dstPtr, PixelCount(4), vm, false);
      pc->v_interleave_lo_u32(m[0], m[1], m[1]);             //   m0 = [M7 M7 M7 M7 M6 M6 M6 M6|M5 M5 M5 M5 M4 M4 M4 M4]
    }
    else if (pixelType == PixelType::kA8) {
      VecArray vm(m[0].xmm());
      compOpPart()->vMaskProcStoreAdvance(dstPtr, PixelCount(4), vm, true);
      pc->v_swizzle_u32(m[0], m[0], shuffleImm(3, 2, 3, 2));
    }

    cc->vextracti128(cov.xmm(), cov, 1);
    pc->j(L_VTail_Init, sub_nz(i, 4));                       //   if ((i -= 4) > 0) goto L_VTail_Init;

    pc->bind(L_VLoop_End);                                   // L_VLoop_End:
    cc->vextracti128(cov.xmm(), cov, 0);
    pc->j(L_Scanline_Done1, ucmp_ge(x0, xEnd));              //   if (x0 >= xEnd) goto L_Scanline_Done1;
  }

  // VLoop - Main VMask Loop - 4 Pixels
  // ----------------------------------

  else if (vProcPixelCount == 4u) {
    Label L_VLoop_Cont = pc->newLabel();

    pc->bind(L_VLoop_Cont);                                  // L_VLoop_Cont:

    if (pixelType == PixelType::kRGBA32) {
      VecArray vm;
      if (m[0].isYmm()) {
        vm.init(m[0]);
      }
      else {
        vm.init(m[0], m[1]);
      }

      compOpPart()->vMaskProcStoreAdvance(dstPtr, PixelCount(4), vm, false);
    }
    else if (pixelType == PixelType::kA8) {
      VecArray vm(m[0]);
      compOpPart()->vMaskProcStoreAdvance(dstPtr, PixelCount(4), vm, false);
    }

    pc->add(cellPtr, cellPtr, 4 * 4);                        //   cellPtr += 4 * sizeof(uint32_t);
    pc->v_add_i32(cov, cov, x86::ptr(cellPtr));              //   cov[3:0] += cellPtr[3:0];
    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
    pc->v_storea_i128(x86::ptr(cellPtr), vecZero);           //   cellPtr[3:0] = 0;
    dPix.resetAllExceptTypeAndName();

    pc->bind(L_VLoop_Init);                                  // L_VLoop_Init:
    accumulateCoverages(cov);
    calcMasksFromCells(m[0], cov, fillRuleMask, globalAlpha);
    normalizeCoverages(cov);

    if (pixelType == PixelType::kRGBA32) {
      Xmm m0Xmm = m[0].xmm();
      pc->v_interleave_lo_u16(m0Xmm, m0Xmm, m0Xmm);          //   m0 = [M3 M3 M2 M2 M1 M1 M0 M0]
      if (m[0].isYmm()) {
        pc->v_perm_i64(m[0], m[0], shuffleImm(1, 1, 0, 0));    // m0 = [M3 M3 M2 M2 M3 M3 M2 M2|M1 M1 M0 M0 M1 M1 M0 M0]
        pc->v_swizzle_u32(m[0], m[0], shuffleImm(1, 1, 0, 0)); // m0 = [M3 M3 M2 M2 M1 M1 M0 M0|M1 M1 M1 M1 M0 M0 M0 M0]
      }
      else {
        pc->v_swizzle_u32(m[1], m[0], shuffleImm(3, 3, 2, 2)); // m1 = [M3 M3 M3 M3 M2 M2 M2 M2]
        pc->v_swizzle_u32(m[0], m[0], shuffleImm(1, 1, 0, 0)); // m0 = [M1 M1 M1 M1 M0 M0 M0 M0]
      }
    }

    pc->j(L_VLoop_Cont, sub_nc(i, 4));                       //   if ((i -= 4) >= 0) goto L_VLoop_Cont;
    pc->j(L_VTail_Init, add_nz(i, 4));                       //   if ((i += 4) != 0) goto L_VTail_Init;
    pc->j(L_Scanline_Done1, ucmp_ge(x0, xEnd));              //   if (x0 >= xEnd) goto L_Scanline_Done1;
  }

  // VLoop - Main VMask Loop - 1 Pixel
  // ---------------------------------

  else {
    Label L_VLoop_Iter = pc->newLabel();
    Label L_VLoop_Step = pc->newLabel();

    pc->bind(L_VLoop_Iter);                                  // L_VLoop_Iter:

    if (pixelGranularity >= 4)
      compOpPart()->enterPartialMode();                      //   <CompOpPart::enterPartialMode>

    if (pixelType == PixelType::kRGBA32) {
      pc->v_sllb_u128(m[0], m[0], 6);                        //   m0[7:0] = [__ M3 M2 M1 M0 __ __ __]

      pc->bind(L_VLoop_Step);                                // L_VLoop_Step:
      pc->v_swizzle_lo_u16(m[0], m[0], shuffleImm(3, 3, 3, 3));// m0[7:0] = [__ M3 M2 M1 M0 M0 M0 M0]

      compOpPart()->vMaskProcRGBA32Vec(dPix, PixelCount(1), PixelFlags::kPC | PixelFlags::kImmutable, m, true, pc->emptyPredicate());

      pc->xStorePixel(dstPtr, dPix.pc[0], 1, dstBpp, Alignment(1));
      dPix.resetAllExceptTypeAndName();

      pc->dec(i);                                            //   i--;
      pc->add(dstPtr, dstPtr, dstBpp);                       //   dstPtr += dstBpp;
      pc->add(cellPtr, cellPtr, 4);                          //   cellPtr += 4;
      pc->v_srlb_u128(m[0], m[0], 2);                        //   m0[7:0] = [0, m[7:1]]
    }
    else if (pixelType == PixelType::kA8) {
      pc->bind(L_VLoop_Step);                                // L_VLoop_Step:

      Gp msk = pc->newGp32("@msk");
      pc->v_extract_u16(msk, m[0], 0);

      compOpPart()->vMaskProcA8Gp(dPix, PixelFlags::kSA | PixelFlags::kImmutable, msk, false);

      pc->store_8(x86::ptr(dstPtr), dPix.sa);
      dPix.resetAllExceptTypeAndName();

      pc->dec(i);                                            //   i--;
      pc->add(dstPtr, dstPtr, dstBpp);                       //   dstPtr += dstBpp;
      pc->add(cellPtr, cellPtr, 4);                          //   cellPtr += 4;
      pc->v_srlb_u128(m[0], m[0], 2);                        //   m0[7:0] = [0, m[7:1]]
    }

    if (pixelGranularity >= 4)                               //   if (pixelGranularity >= 4)
      compOpPart()->nextPartialPixel();                      //     <CompOpPart::nextPartialPixel>

    pc->j(L_VLoop_Step, test_nz(i, 0x3));                    //   if (i % 4 != 0) goto L_VLoop_Step;

    if (pixelGranularity >= 4)                               //   if (pixelGranularity >= 4)
      compOpPart()->exitPartialMode();                       //     <CompOpPart::exitPartialMode>

    // We must use unaligned loads here as we don't know whether we are at the end of the scanline. In that case
    // `cellPtr` could already be misaligned if the image width is not divisible by 4.

    if (!pc->hasAVX()) {
      Xmm covTmp = cc->newXmm("covTmp");
      pc->v_loadu_i128(covTmp, x86::ptr(cellPtr));           //   covTmp[3:0] = cellPtr[3:0];
      pc->v_add_i32(cov, cov, covTmp);                       //   cov[3:0] += covTmp
    }
    else {
      pc->v_add_i32(cov, cov, x86::ptr(cellPtr));            //   cov[3:0] += cellPtr[3:0]
    }

    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
    pc->v_storeu_i128(x86::ptr(cellPtr), vecZero);           //   cellPtr[3:0] = 0;

    pc->bind(L_VLoop_Init);                                  // L_VLoop_Init:

    accumulateCoverages(cov);
    calcMasksFromCells(m[0], cov, fillRuleMask, globalAlpha);
    normalizeCoverages(cov);

    pc->j(L_VLoop_Iter, test_nz(i));                         //   if (i != 0) goto L_VLoop_Iter;
    pc->j(L_Scanline_Done1, ucmp_ge(x0, xEnd));              //   if (x0 >= xEnd) goto L_Scanline_Done1;
  }

  // BitGap
  // ------

  // If we are here we are at the end of `vMask` loop. There are two possibilities:
  //
  //   1. There is a gap between bits in a single or multiple BitWords. This means that there is a possibility
  //      for a `cMask` loop which could be solid, masked, or have zero-mask (a real gap).
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

  pc->load(bitWord, x86::ptr(bitPtr));                       //   bitWord = bitPtr[0];
  pc->lea(bitPtr, x86::ptr(bitPtr, bwSize));                 //   bitPtr += bwSize;
  pc->j(L_BitGap_Match, test_nz(bitWord));                   //   if (bitWord != 0) goto L_BitGap_Match;

  pc->add(xOff, xOff, pixelsPerBitWord);                     //   xOff += pixelsPerBitWord;
  pc->j(L_Scanline_Done1, cmp_eq(bitPtr, bitPtrEnd));        //   if (bitPtr == bitPtrEnd) goto L_Scanline_Done1;

  pc->load(bitWord, x86::ptr(bitPtr));                       //   bitWord = bitPtr[0];
  pc->lea(bitPtr, x86::ptr(bitPtr, bwSize));                 //   bitPtr += bwSize;
  pc->j(L_BitGap_Cont, test_z(bitWord));                     //   if (bitWord == 0) goto L_BitGap_Cont;

  pc->bind(L_BitGap_Match);                                  // L_BitGap_Match:
  cc->mov(x86::ptr(bitPtr, -bwSize, uint32_t(bwSize)), 0);   //   bitPtr[-1] = 0;
  pc->ctz(i.cloneAs(bitWord), bitWord);                      //   i = ctz(bitWord);
  pc->mov(bitWordTmp, -1);                                   //   bitWordTmp = -1; (all ones)
  pc->v_extract_u16(cMaskAlpha, m[0].xmm(), 0);              //   cMaskAlpha = extracti16(m0, 0);

  pc->shl(bitWordTmp, bitWordTmp, i);                        //   bitWordTmp <<= i;
  pc->shl(i, i, imm(pixelsPerOneBitShift));                  //   i <<= pixelsPerOneBitShift;

  pc->xor_(bitWord, bitWord, bitWordTmp);                    //   bitWord ^= bitWordTmp;
  pc->add(i, i, xOff);                                       //   i += xOff;
  pc->sub(i, i, x0);                                         //   i -= x0;
  pc->add(x0, x0, i);                                        //   x0 += i;
  pc->add_scaled(cellPtr, i.cloneAs(cellPtr), 4);            //   cellPtr += i * sizeof(uint32_t);
  pc->j(L_CLoop_Init, test_nz(cMaskAlpha));                  //   if (cMaskAlpha != 0) goto L_CLoop_Init;

  // Fully-Transparent span where `cMaskAlpha == 0`.

  pc->add_scaled(dstPtr, i.cloneAs(dstPtr), int(dstBpp));    //   dstPtr += i * dstBpp;

  compOpPart()->postfetchN();
  compOpPart()->advanceX(x0, i);
  compOpPart()->prefetchN();

  pc->j(L_BitScan_Match, test_nz(bitWord));                  //   if (bitWord != 0) goto L_BitScan_Match;
  pc->j(L_BitScan_Iter);                                     //   goto L_BitScan_Iter;

  // CLoop
  // -----

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

  if (pixelType == PixelType::kA8 && maxPixels > 4u) {
    pc->v_swizzle_u32(m[0], m[0], shuffleImm(0, 0, 0, 0));   //   m0 = [M0 M0 M0 M0 M0 M0 M0 M0]
  }
  else {
    pc->v_swizzle_u32(m[0], m[0], shuffleImm(0, 0, 0, 0));   //   m0 = [M0 M0 M0 M0 M0 M0 M0 M0]
  }

  compOpPart()->cMaskInit(cMaskAlpha, m[0]);
  if (pixelGranularity >= 4)
    compOpPart()->cMaskGranularLoop(i);
  else
    compOpPart()->cMaskGenericLoop(i);
  compOpPart()->cMaskFini();

  pc->j(L_BitScan_Match, test_nz(bitWord));                  //   if (bitWord != 0) goto L_BitScan_Match;
  pc->j(L_BitScan_Iter);                                     //   goto L_BitScan_Iter;

  // VTail - Tail `vMask` loop for pixels near the end of the scanline
  // -----------------------------------------------------------------

  if (maxPixels >= 4u) {
    Label L_VTail_Cont = pc->newLabel();

    Vec mXmm = m[0].xmm();
    VecArray msk(mXmm);

    // Tail loop can handle up to `pixelsPerOneBit - 1`.
    if (pixelType == PixelType::kRGBA32) {
      bool hasYmmMask = m[0].isYmm();

      pc->bind(L_VTail_Init);                                // L_VTail_Init:
      pc->add_scaled(cellPtr, i, 4);                         //   cellPtr += i * sizeof(uint32_t);

      if (!hasYmmMask) {
        pc->v_swap_u64(m[1], m[1]);
      }
      compOpPart()->enterPartialMode();                      //   <CompOpPart::enterPartialMode>

      pc->bind(L_VTail_Cont);                                // L_VTail_Cont:
      compOpPart()->vMaskProcRGBA32Vec(dPix, PixelCount(1), PixelFlags::kPC | PixelFlags::kImmutable, msk, true, pc->emptyPredicate());

      pc->xStorePixel(dstPtr, dPix.pc[0], 1, dstBpp, Alignment(1));
      pc->add(dstPtr, dstPtr, dstBpp);                       //   dstPtr += dstBpp;

      if (!hasYmmMask) {
        pc->v_interleave_hi_u64(m[0], m[0], m[1]);
      }
      else {
        // All 4 expanded masks for ARGB channels are in a single register, so just permute.
        pc->v_perm_i64(m[0], m[0], shuffleImm(0, 3, 2, 1));
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
      pc->v_extract_u16(mScalar, mXmm, 0);
      compOpPart()->vMaskProcA8Gp(dPix, PixelFlags::kSA | PixelFlags::kImmutable, mScalar, false);

      pc->store_8(x86::ptr(dstPtr), dPix.sa);
      pc->add(dstPtr, dstPtr, dstBpp);                       //   dstPtr += dstBpp;
      pc->v_srlb_u128(mXmm, mXmm, 2);                        //   m0[7:0] = [0, m[7:1]]
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
  pc->v_zero_i(vecZero);                                     //   vecZero = 0;
  pc->v_storeu_i128(x86::ptr(cellPtr), vecZero);             //   cellPtr[3:0] = 0;

  pc->bind(L_Scanline_Done1);                                // L_Scanline_Done1:
  disadvanceDstPtrAndCellPtr(dstPtr,                         //   dstPtr -= x0 * dstBpp;
                             cellPtr, x0, dstBpp);           //   cellPtr -= x0 * sizeof(uint32_t);
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
  pc->load(bitWord, x86::ptr(bitPtr));                       //   bitWord = bitPtr[0];
  pc->lea(bitPtr, x86::ptr(bitPtr, bwSize));                 //   bitPtr += bwSize;
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

void FillAnalyticPart::accumulateCoverages(const Vec& cov) noexcept {
  Vec tmp = pc->newSimilarReg<Vec>(cov, "vCovTmp");

  pc->v_sllb_u128(tmp, cov, 4);                              //   tmp[7:0]  = [  c6    c5    c4    0  |  c2    c1    c0    0  ];
  pc->v_add_i32(cov, cov, tmp);                              //   cov[7:0]  = [c7:c6 c6:c5 c5:c4   c4 |c3:c2 c2:c1 c1:c0   c0 ];
  pc->v_sllb_u128(tmp, cov, 8);                              //   tmp[7:0]  = [c5:c4   c4    0     0  |c1:c0   c0    0     0  ];
  pc->v_add_i32(cov, cov, tmp);                              //   cov[7:0]  = [c7:c4 c6:c4 c5:c4   c4 |c3:c0 c2:c0 c1:c0   c0 ];

  if (cov.isYmm()) {
    pc->v_swizzle_u32(tmp.xmm(), cov.xmm(), x86::shuffleImm(3, 3, 3, 3));
    cc->vperm2i128(tmp, tmp, tmp, perm2x128Imm(Perm2x128::kALo, Perm2x128::kZero));
    pc->v_add_i32(cov, cov, tmp);                            //   cov[7:0]  = [c7:c0 c6:c0 c5:c0 c4:c0|c3:c0 c2:c0 c1:c0   c0 ];
  }
}

void FillAnalyticPart::normalizeCoverages(const Vec& cov) noexcept {
  pc->v_srlb_u128(cov, cov, 12);                             //   cov[3:0]  = [  0     0     0     c0 ];
}

// Calculate masks from cell and store them to a vector of the following layout:
//
//   [__ __ __ __ M7 M6 M5 M4|__ __ __ __ M3 M2 M1 M0]
//
// NOTE: Depending on the vector size the output mask is for either 4 or 8 pixels.
void FillAnalyticPart::calcMasksFromCells(const Vec& dst_, const Vec& cov, const Vec& fillRuleMask, const Vec& globalAlpha) noexcept {
  Vec dst = dst_.cloneAs(cov);

  // This implementation is a bit tricky. In the original AGG and FreeType `A8_SHIFT + 1` is used. However, we don't do
  // that and mask out the last bit through `fillRuleMask`. The reason we do this is that our `globalAlpha` is already
  // pre-shifted by `7` bits left and we only need to shift the final mask by one bit left after it's been calculated.
  // So instead of shifting it left later we clear the LSB bit now and that's it, we saved one instruction.
  pc->v_sra_i32(dst, cov, A8Info::kShift);
  pc->v_and_i32(dst, dst, fillRuleMask);

  // We have to make sure that that cleared LSB bit stays zero. Since we only use SUB with even value and abs we are
  // fine. However, that packing would not be safe if there was no "VMINI16", which makes sure we are always safe.
  pc->v_sub_i32(dst, dst, pc->simdConst(&ct.i_0000020000000200, Bcst::k32, dst));
  pc->v_abs_i32(dst, dst);

  pc->v_packs_i32_i16(dst, dst, dst);
  pc->v_min_i16(dst, dst, pc->simdConst(&ct.i_0200020002000200, Bcst::kNA, dst));

  // Multiply masks by global alpha, this would output masks in [0, 255] range.
  pc->v_mulh_u16(dst, dst, globalAlpha);
}

void FillAnalyticPart::disadvanceDstPtrAndCellPtr(const Gp& dstPtr, const Gp& cellPtr, const Gp& x, uint32_t dstBpp) noexcept {
  Gp xAdv = x.cloneAs(dstPtr);

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
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif
