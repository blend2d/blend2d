// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fillpart_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchpixelptrpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipedebug_p.h"

namespace BLPipeline {
namespace JIT {

// BLPipeline::JIT::FillPart - Construction & Destruction
// ======================================================

FillPart::FillPart(PipeCompiler* pc, FillType fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : PipePart(pc, PipePartType::kFill),
    _fillType(fillType),
    _isRectFill(false) {

  // Initialize the children of this part.
  _children[kIndexDstPart] = dstPart;
  _children[kIndexCompOpPart] = compOpPart;
  _childCount = 2;
}

// BLPipeline::JIT::FillBoxAPart - Construction & Destruction
// ==========================================================

FillBoxAPart::FillBoxAPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : FillPart(pc, FillType::kBoxA, dstPart, compOpPart) {

  _maxSimdWidthSupported = SimdWidth::k512;
  _isRectFill = true;
}

// BLPipeline::JIT::FillBoxAPart - Compile
// =======================================

void FillBoxAPart::compile() noexcept {
  // Prepare
  // -------

  _initGlobalHook(cc->cursor());

  int dstBpp = int(dstPart()->bpp());

  // Local Registers
  // ---------------

  x86::Gp ctxData = pc->_ctxData;                          // Reg/Init.
  x86::Gp fillData = pc->_fillData;                        // Reg/Init.

  x86::Gp dstPtr = cc->newIntPtr("dstPtr");                // Reg.
  x86::Gp dstStride = cc->newIntPtr("dstStride");          // Reg/Mem.

  x86::Gp x = cc->newUInt32("x");                          // Reg.
  x86::Gp y = cc->newUInt32("y");                          // Reg/Mem.
  x86::Gp w = cc->newUInt32("w");                          // Reg/Mem.
  x86::Gp ga_sm = cc->newUInt32("ga.sm");                  // Reg/Tmp.

  // Prolog
  // ------

  cc->mov(dstPtr, cc->intptr_ptr(ctxData, BL_OFFSET_OF(ContextData, dst.stride)));
  cc->mov(y, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::BoxA, box.y0)));

  cc->mov(dstStride, dstPtr);
  cc->mov(w, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::BoxA, box.x0)));
  cc->imul(dstPtr, y.cloneAs(dstPtr));

  dstPart()->initPtr(dstPtr);
  compOpPart()->init(w, y, 1);

  cc->neg(y);
  pc->i_lea_bpp(dstPtr, dstPtr, w, uint32_t(dstBpp));
  cc->neg(w);

  cc->add(dstPtr, cc->intptr_ptr(ctxData, BL_OFFSET_OF(ContextData, dst.pixelData)));
  cc->add(w, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::BoxA, box.x1)));

  pc->i_mul(x, w, dstBpp);
  cc->add(y, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::BoxA, box.y1)));
  cc->sub(dstStride, x.cloneAs(dstStride));

  // Loop
  // ----

  if (compOpPart()->shouldOptimizeOpaqueFill()) {
    Label L_FullAlphaIter = cc->newLabel();
    Label L_SemiAlphaInit = cc->newLabel();
    Label L_SemiAlphaIter = cc->newLabel();

    Label L_End  = cc->newLabel();

    cc->mov(ga_sm, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::BoxA, alpha)));
    pc->i_jump_if_not_opaque_mask(ga_sm, L_SemiAlphaInit);

    // Full Alpha
    // ----------

    compOpPart()->cMaskInitOpaque();

    cc->bind(L_FullAlphaIter);
    cc->mov(x, w);

    compOpPart()->startAtX(pc->_gpNone);
    compOpPart()->cMaskGenericLoop(x);

    cc->add(dstPtr, dstStride);
    compOpPart()->advanceY();

    cc->sub(y, 1);
    cc->jnz(L_FullAlphaIter);

    compOpPart()->cMaskFini();
    cc->jmp(L_End);

    // Semi Alpha
    // ----------

    cc->bind(L_SemiAlphaInit);
    compOpPart()->cMaskInit(ga_sm, x86::Vec());

    cc->bind(L_SemiAlphaIter);
    cc->mov(x, w);

    compOpPart()->startAtX(pc->_gpNone);
    compOpPart()->cMaskGenericLoop(x);

    cc->add(dstPtr, dstStride);
    compOpPart()->advanceY();

    cc->sub(y, 1);
    cc->jnz(L_SemiAlphaIter);

    compOpPart()->cMaskFini();
    cc->bind(L_End);
  }
  else {
    Label L_AnyAlphaLoop = cc->newLabel();

    compOpPart()->cMaskInit(x86::ptr_32(fillData, BL_OFFSET_OF(FillData::BoxA, alpha)));

    cc->bind(L_AnyAlphaLoop);
    cc->mov(x, w);

    compOpPart()->startAtX(pc->_gpNone);
    compOpPart()->cMaskGenericLoop(x);

    cc->add(dstPtr, dstStride);
    compOpPart()->advanceY();

    cc->sub(y, 1);
    cc->jnz(L_AnyAlphaLoop);

    compOpPart()->cMaskFini();
  }

  // Epilog
  // ------

  compOpPart()->fini();
  _finiGlobalHook();
}

// BLPipeline::JIT::FillMaskPart - Construction & Destruction
// ==========================================================

FillMaskPart::FillMaskPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : FillPart(pc, FillType::kMask, dstPart, compOpPart) {

  _maxSimdWidthSupported = SimdWidth::k512;
}

// BLPipeline::JIT::FillMaskPart - Compile
// =======================================

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

  Label L_ScanlineInit = cc->newLabel();
  Label L_ScanlineDone = cc->newLabel();
  Label L_ScanlineSkip = cc->newLabel();

  Label L_ProcessNext = cc->newLabel();
  Label L_ProcessCmd = cc->newLabel();
  Label L_CMaskInit = cc->newLabel();
  Label L_VMaskA8WithoutGA = cc->newLabel();
  Label L_End = cc->newLabel();

  // Local Registers
  // ---------------

  x86::Gp ctxData = pc->_ctxData;                          // Reg/Init.
  x86::Gp fillData = pc->_fillData;                        // Reg/Init.

  x86::Gp dstPtr = cc->newIntPtr("dstPtr");                // Reg.
  x86::Gp dstStride = cc->newIntPtr("dstStride");          // Reg/Mem.

  x86::Gp i = cc->newUInt32("i");                          // Reg.
  x86::Gp x = cc->newUInt32("x");                          // Reg.
  x86::Gp y = cc->newUInt32("y");                          // Reg/Mem.

  x86::Gp cmdType = cc->newUInt32("cmdType");              // Reg/Tmp.
  x86::Gp cmdPtr = cc->newUIntPtr("cmdPtr");               // Reg/Mem.
  x86::Gp cmdBegin = cc->newUIntPtr("cmdBegin");           // Mem.
  x86::Gp maskValue = cc->newUIntPtr("maskValue");         // Reg.
  x86::Gp maskAdvance = cc->newUIntPtr("maskAdvance");     // Reg/Tmp

  GlobalAlpha ga;                                          // Reg/Mem.
  GlobalAlpha gaNotApplied;                                // None.

  // Prolog
  // ------

  // Initialize the destination.
  cc->mov(y, x86::ptr_32(fillData, BL_OFFSET_OF(FillData, mask.box.y0)));
  cc->mov(dstStride, x86::ptr(ctxData, BL_OFFSET_OF(ContextData, dst.stride)));

  cc->mov(dstPtr.r32(), y);
  cc->imul(dstPtr, dstStride);
  cc->add(dstPtr, x86::ptr(ctxData, BL_OFFSET_OF(ContextData, dst.pixelData)));

  // Initialize pipeline parts.
  dstPart()->initPtr(dstPtr);
  compOpPart()->init(pc->_gpNone, y, 1);

  // Initialize mask pointers.
  cc->mov(cmdPtr, x86::ptr(fillData, BL_OFFSET_OF(FillData, mask.maskCommandData)));

  // Initialize global alpha.
  ga.initFromMem(pc, x86::ptr_32(fillData, BL_OFFSET_OF(FillData, mask.alpha)));

  // y = fillData->box.y1 - fillData->box.y0;
  cc->neg(y);
  cc->add(y, x86::ptr_32(fillData, BL_OFFSET_OF(FillData, mask.box.y1)));
  cc->jmp(L_ScanlineInit);

  // Scanline Done
  // -------------

  x86::Gp repeat = cc->newUInt32("repeat");

  cc->align(AlignMode::kCode, labelAlignment);
  cc->bind(L_ScanlineDone);
  disadvanceDstPtr(dstPtr, x, int(dstBpp));

  cc->bind(L_ScanlineSkip);
  cc->mov(repeat, x86::ptr_32(cmdPtr, BL_OFFSET_OF(MaskCommand, _x0)));
  cc->sub(y, 1);
  cc->jz(L_End);

  cc->add(cmdPtr, kMaskCmdSize);
  cc->add(dstPtr, dstStride);
  compOpPart()->advanceY();
  cc->sub(repeat, 1);
  cc->mov(x86::ptr_32(cmdPtr, BL_OFFSET_OF(MaskCommand, _x0) - kMaskCmdSize), repeat);
  cc->cmovnz(cmdPtr, cmdBegin);

  // Scanline Init
  // -------------

  cc->bind(L_ScanlineInit);
  cc->mov(cmdType, x86::ptr_32(cmdPtr, BL_OFFSET_OF(MaskCommand, _x1AndType)));
  cc->mov(cmdBegin, cmdPtr);
  cc->mov(x, x86::ptr_32(cmdPtr, BL_OFFSET_OF(MaskCommand, _x0)));

  // This is not really common, but it's possible to skip entire scanlines with kEndOrRepeat.
  pc->i_test(cmdType, MaskCommand::kTypeMask);
  cc->jz(L_ScanlineSkip);

  pc->i_add_scaled(dstPtr, x.cloneAs(dstPtr), dstBpp);
  compOpPart()->startAtX(x);
  cc->jmp(L_ProcessCmd);

  // Process Command
  // ---------------

  cc->bind(L_ProcessNext);
  cc->mov(cmdType, x86::ptr_32(cmdPtr, kMaskCmdSize + BL_OFFSET_OF(MaskCommand, _x1AndType)));
  cc->mov(i, x86::ptr_32(cmdPtr, kMaskCmdSize + BL_OFFSET_OF(MaskCommand, _x0)));
  cc->add(cmdPtr, kMaskCmdSize);
  pc->i_test(cmdType, MaskCommand::kTypeMask);
  cc->jz(L_ScanlineDone);

  // Only emit the jump if there is something significant to skip.
  cc->sub(i, x);
  if (!compOpPart()->hasPartFlag(PipePartFlags::kAdvanceXIsSimple))
    cc->jz(L_ProcessCmd);

  cc->add(x, i);
  pc->i_add_scaled(dstPtr, i.cloneAs(dstPtr), dstBpp);
  compOpPart()->advanceX(x, i);

  cc->bind(L_ProcessCmd);
  if (pc->hasBMI2() && cc->is64Bit()) {
    cc->rorx(i.r64(), cmdType.r64(), MaskCommand::kTypeBits);
  }
  else {
    cc->mov(i, cmdType);
    cc->shr(i, MaskCommand::kTypeBits);
  }

  cc->and_(cmdType, MaskCommand::kTypeMask);
  cc->sub(i, x);
  cc->mov(maskValue, cc->intptr_ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _value.data)));
  cc->add(x, i);

  // We know the command is not kEndOrRepeat, which allows this little trick.
  cc->cmp(cmdType, uint32_t(MaskCommandType::kCMask));
  cc->jz(L_CMaskInit);

  // VMask Command
  // -------------

  // Increments the advance in the mask command in case it would be repeated.
  cc->mov(maskAdvance, cc->intptr_ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _maskAdvance)));
  cc->add(cc->intptr_ptr(cmdPtr, BL_OFFSET_OF(MaskCommand, _value.ptr)), maskAdvance);

  cc->cmp(cmdType, uint32_t(MaskCommandType::kVMaskA8WithoutGA));
  cc->jz(L_VMaskA8WithoutGA);
  compOpPart()->vMaskGenericLoop(i, dstPtr, maskValue, gaNotApplied, L_ProcessNext);

  cc->bind(L_VMaskA8WithoutGA);
  compOpPart()->vMaskGenericLoop(i, dstPtr, maskValue, ga, L_ProcessNext);

  // CMask Command
  // -------------

  cc->align(AlignMode::kCode, labelAlignment);
  cc->bind(L_CMaskInit);
  if (compOpPart()->shouldOptimizeOpaqueFill()) {
    Label L_CLoop_Msk = cc->newLabel();
    pc->i_jump_if_not_opaque_mask(maskValue.r32(), L_CLoop_Msk);

    compOpPart()->cMaskInitOpaque();
    compOpPart()->cMaskGenericLoop(i);
    compOpPart()->cMaskFini();
    cc->jmp(L_ProcessNext);

    cc->align(AlignMode::kCode, labelAlignment);
    cc->bind(L_CLoop_Msk);
  }

  compOpPart()->cMaskInit(maskValue.r32(), x86::Vec());
  compOpPart()->cMaskGenericLoop(i);
  compOpPart()->cMaskFini();
  cc->jmp(L_ProcessNext);

  // Epilog
  // ------

  cc->bind(L_End);
  compOpPart()->fini();
  _finiGlobalHook();
}

void FillMaskPart::disadvanceDstPtr(const x86::Gp& dstPtr, const x86::Gp& x, int dstBpp) noexcept {
  x86::Gp xAdv = x.cloneAs(dstPtr);

  if (BLIntOps::isPowerOf2(dstBpp)) {
    if (dstBpp > 1)
      cc->shl(xAdv, BLIntOps::ctz(dstBpp));
    cc->sub(dstPtr, xAdv);
  }
  else {
    x86::Gp dstAdv = cc->newIntPtr("dstAdv");
    pc->i_mul(dstAdv, xAdv, dstBpp);
    cc->sub(dstPtr, dstAdv);
  }
}

// BLPipeline::JIT::FillAnalyticPart - Construction & Destruction
// ==============================================================

FillAnalyticPart::FillAnalyticPart(PipeCompiler* pc, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : FillPart(pc, FillType::kAnalytic, dstPart, compOpPart) {

  _maxSimdWidthSupported = SimdWidth::k256;
}

// BLPipeline::JIT::FillAnalyticPart - Compile
// ===========================================

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
  int pixelsPerOneBitShift = int(BLIntOps::ctz(pixelsPerOneBit));

  int pixelGranularity = pixelsPerOneBit;
  int pixelsPerBitWord = pixelsPerOneBit * bwSizeInBits;
  int pixelsPerBitWordShift = int(BLIntOps::ctz(pixelsPerBitWord));

  if (compOpPart()->maxPixelsOfChildren() < 4)
    pixelGranularity = 1;

  // Local Labels
  // ------------

  Label L_BitScan_Init = cc->newLabel();
  Label L_BitScan_Iter = cc->newLabel();
  Label L_BitScan_Match = cc->newLabel();
  Label L_BitScan_End = cc->newLabel();

  Label L_VLoop_Init = cc->newLabel();

  Label L_VTail_Init = cc->newLabel(); // Only used if maxPixels >= 4.
  Label L_CLoop_Init = cc->newLabel();

  Label L_Scanline_Done0 = cc->newLabel();
  Label L_Scanline_Done1 = cc->newLabel();
  Label L_Scanline_AdvY = cc->newLabel();
  Label L_Scanline_Iter = cc->newLabel();
  Label L_Scanline_Init = cc->newLabel();

  Label L_End = cc->newLabel();

  // Local Registers
  // ---------------

  x86::Gp ctxData = pc->_ctxData;                                 // Init.
  x86::Gp fillData = pc->_fillData;                               // Init.

  x86::Gp dstPtr = cc->newIntPtr("dstPtr");                       // Reg.
  x86::Gp dstStride = cc->newIntPtr("dstStride");                 // Mem.

  x86::Gp bitPtr = cc->newIntPtr("bitPtr");                       // Reg.
  x86::Gp bitPtrEnd = cc->newIntPtr("bitPtrEnd");                 // Reg/Mem.

  x86::Gp bitPtrRunLen = cc->newIntPtr("bitPtrRunLen");           // Mem.
  x86::Gp bitPtrSkipLen = cc->newIntPtr("bitPtrSkipLen");         // Mem.

  x86::Gp cellPtr = cc->newIntPtr("cellPtr");                     // Reg.
  x86::Gp cellStride = cc->newIntPtr("cellStride");               // Mem.

  x86::Gp x0 = cc->newUInt32("x0");                               // Reg
  x86::Gp xOff = cc->newUInt32("xOff");                           // Reg/Mem.
  x86::Gp xEnd = cc->newUInt32("xEnd");                           // Mem.
  x86::Gp xStart = cc->newUInt32("xStart");                       // Mem.

  x86::Gp y = cc->newUInt32("y");                                 // Reg/Mem.
  x86::Gp i = cc->newUInt32("i");                                 // Reg.
  x86::Gp cMaskAlpha = cc->newUInt32("cMaskAlpha");               // Reg/Tmp.

  x86::Gp bitWord = cc->newUIntPtr("bitWord");                    // Reg/Mem.
  x86::Gp bitWordTmp = cc->newUIntPtr("bitWordTmp");              // Reg/Tmp.

  x86::Vec cov = pc->newVec(vProcWidth, "cov");                   // Reg.
  x86::Vec globalAlpha = pc->newVec(vProcWidth, "globalAlpha");   // Mem.
  x86::Vec fillRuleMask = pc->newVec(vProcWidth, "fillRuleMask"); // Mem.
  x86::Xmm vecZero = cc->newXmm("vec_zero");                      // Reg/Tmp.

  Pixel dPix("d", pixelType);                                     // Reg.

  VecArray m;                                                     // Reg.
  pc->newVecArray(m, 2, pc->simdWidth(), "m");

  // Prolog
  // ------

  // Initialize the destination.
  cc->mov(y, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::Analytic, box.y0)));
  cc->mov(dstStride, cc->intptr_ptr(ctxData, BL_OFFSET_OF(ContextData, dst.stride)));

  cc->mov(dstPtr.r32(), y);
  cc->imul(dstPtr, dstStride);
  cc->add(dstPtr, cc->intptr_ptr(ctxData, BL_OFFSET_OF(ContextData, dst.pixelData)));

  // Initialize cell pointers.
  cc->mov(bitPtrSkipLen, cc->intptr_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, bitStride)));
  cc->mov(cellStride, cc->intptr_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, cellStride)));

  cc->mov(bitPtr, cc->intptr_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, bitTopPtr)));
  cc->mov(cellPtr, cc->intptr_ptr(fillData, BL_OFFSET_OF(FillData::Analytic, cellTopPtr)));

  // Initialize pipeline parts.
  dstPart()->initPtr(dstPtr);
  compOpPart()->init(pc->_gpNone, y, uint32_t(pixelGranularity));

  // y = fillData->box.y1 - fillData->box.y0;
  cc->neg(y);
  cc->add(y, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::Analytic, box.y1)));

  // Decompose the original `bitStride` to bitPtrRunLen + bitPtrSkipLen, where:
  //   - `bitPtrRunLen` - Number of BitWords (in byte units) active in this band.
  //   - `bitPtrRunSkip` - Number of BitWords (in byte units) to skip for this band.
  cc->mov(xStart, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::Analytic, box.x0)));
  cc->shr(xStart, pixelsPerBitWordShift);

  cc->mov(xEnd, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::Analytic, box.x1)));
  cc->mov(bitPtrRunLen.r32(), xEnd);
  cc->shr(bitPtrRunLen.r32(), pixelsPerBitWordShift);

  cc->sub(bitPtrRunLen.r32(), xStart);
  cc->inc(bitPtrRunLen.r32());
  cc->shl(bitPtrRunLen, BLIntOps::ctz(bwSize));
  cc->sub(bitPtrSkipLen, bitPtrRunLen);

  // Make `xStart` to become the X offset of the first active BitWord.
  cc->lea(bitPtr, x86::ptr(bitPtr, xStart.cloneAs(bitPtr), BLIntOps::ctz(bwSize)));
  cc->shl(xStart, pixelsPerBitWordShift);

  pc->v_broadcast_u16(globalAlpha, x86::ptr(fillData, BL_OFFSET_OF(FillData::Analytic, alpha)));
  // We shift left by 7 bits so we can use `pmulhuw` in `calcMasksFromCells()`.
  pc->v_sll_i16(globalAlpha, globalAlpha, 7);

  // Initialize fill-rule.
  pc->v_broadcast_u32(fillRuleMask, x86::ptr_32(fillData, BL_OFFSET_OF(FillData::Analytic, fillRuleMask)));

  cc->jmp(L_Scanline_Init);

  // BitScan
  // -------

  // Called by Scanline iterator on the first non-zero BitWord it matches. The responsibility of BitScan is to find
  // the first bit in the passed BitWord followed by matching the bit that ends this match. This would essentially
  // produce the first [x0, x1) span that has to be composited as 'VMask' loop.

  cc->bind(L_BitScan_Init);                                  // L_BitScan_Init:
  pc->i_ctz(x0.cloneAs(bitWord), bitWord);                   //   x0 = ctz(bitWord);

  cc->mov(x86::ptr(bitPtr, -bwSize, uint32_t(bwSize)), 0);   //   bitPtr[-1] = 0;
  cc->or_(bitWordTmp, -1);                                   //   bitWordTmp = -1; (all ones).
  pc->i_shl(bitWordTmp, bitWordTmp, x0);                     //   bitWordTmp <<= x0;

  // Convert bit offset `x0` into a pixel offset. We must consider `xOff` as it's only zero for the very first
  // BitWord (all others are multiplies of `pixelsPerBitWord`).

  cc->shl(x0, pixelsPerOneBitShift);                         //   x0 <<= pixelsPerOneBitShift;
  cc->add(x0, xOff);                                         //   x0 += xOff;

  // Load the given cells to `m0` and clear the BitWord and all cells it represents in memory. This is important as
  // the compositor has to clear the memory during composition. If this is a rare case where `x0` points at the end
  // of the raster there is still one cell that is non-zero. This makes sure it's cleared.

  pc->i_add_scaled(dstPtr, x0.cloneAs(dstPtr), int(dstBpp)); //   dstPtr += x0 * dstBpp;
  pc->i_add_scaled(cellPtr, x0.cloneAs(cellPtr), 4);         //   cellPtr += x0 * sizeof(uint32_t);

  // Rare case - line rasterized at the end of the raster boundary. In 99% cases this is a clipped line that was
  // rasterized as vertical-only line at the end of the render box. This is a completely valid case that produces
  // nothing.

  cc->cmp(x0, xEnd);                                         //   if (x0 >= xEnd)
  cc->jae(L_Scanline_Done0);                                 //     goto L_Scanline_Done0;

  // Setup compositor and source/destination parts. This is required as the fetcher needs to know where to start.
  // And since `startAtX()` can only be called once per scanline we must do it here.

  compOpPart()->startAtX(x0);                                //   <CompOpPart::StartAtX>

  if (maxPixels > 1)
    compOpPart()->prefetchN();                                 //   <CompOpPart::PrefetchN>

  pc->v_load_i32(cov, pc->_getMemConst(&ct.i_0002000000020000));

  // If `bitWord ^ bitWordTmp` results in non-zero value it means that the current span ends within the same BitWord,
  // otherwise the span crosses multiple BitWords.

  cc->xor_(bitWord, bitWordTmp);                             //   if ((bitWord ^= bitWordTmp) != 0)
  cc->jnz(L_BitScan_Match);                                  //     goto L_BitScan_Match;

  // Okay, so the span crosses multiple BitWords. Firstly we have to make sure this was not the last one. If that's
  // the case we must terminate the scanning immediately.

  cc->mov(i, bwSizeInBits);                                  //   i = bwSizeInBits;
  cc->cmp(bitPtr, bitPtrEnd);                                //   if (bitPtr == bitPtrEnd)
  cc->jz(L_BitScan_End);                                     //     goto L_BitScan_End;

  // A BitScan loop - iterates over all consecutive BitWords and finds those that don't have all bits set to 1.

  cc->bind(L_BitScan_Iter);                                  // L_BitScan_Iter:
  cc->or_(bitWord, -1);                                      //   bitWord = -1; (all ones);
  cc->add(xOff, pixelsPerBitWord);                           //   xOff += pixelsPerBitWord;
  cc->xor_(bitWord, x86::ptr(bitPtr, 0, uint32_t(bwSize)));  //   bitWord ^= bitPtr[0];
  cc->mov(x86::ptr(bitPtr, 0, uint32_t(bwSize)), 0);         //   bitPtr[0] = 0;
  cc->lea(bitPtr, x86::ptr(bitPtr, bwSize));                 //   bitPtr += bwSize;
  cc->jnz(L_BitScan_Match);                                  //   if (bitWord != 0) goto L_BitScan_Match;

  cc->cmp(bitPtr, bitPtrEnd);                                //   if (bitPtr == bitPtrEnd)
  cc->jz(L_BitScan_End);                                     //     goto L_BitScan_End;
  cc->jmp(L_BitScan_Iter);                                   //   goto L_BitScan_Iter;

  cc->bind(L_BitScan_Match);                                 // L_BitScan_Match:
  pc->i_ctz(i.cloneAs(bitWord), bitWord);                    //   i = ctz(bitWord);

  cc->bind(L_BitScan_End);                                   // L_BitScan_End:
  if (vProcPixelCount == 8)
    pc->v_add_i32(cov.ymm(), cov.ymm(), x86::ptr(cellPtr));  //   acc[7:0] += cellPtr[7:0];
  else
    pc->v_add_i32(cov.xmm(), cov.xmm(), x86::ptr(cellPtr));  //   acc[3:0] += cellPtr[3:0];
  cc->or_(bitWordTmp, -1);                                   //   bitWordTmp = -1; (all ones).
  pc->i_shl(bitWordTmp, bitWordTmp, i);                      //   bitWordTmp <<= i;
  cc->shl(i, pixelsPerOneBitShift);                          //   i <<= pixelsPerOneBitShift;

  cc->xor_(bitWord, bitWordTmp);                             //   bitWord ^= bitWordTmp;
  cc->add(i, xOff);                                          //   i += xOff;

  // In cases where the raster width is not a multiply of `pixelsPerOneBit` we must make sure we won't overflow it.

  pc->i_umin(i, i, xEnd);                                    //   i = min(i, xEnd);
  pc->v_zero_i(vecZero);                                     //   vecZero = 0;
  pc->v_storea_i128(x86::ptr(cellPtr), vecZero);             //   cellPtr[3:0] = 0;

  // `i` is now the number of pixels (and cells) to composite by using `vMask`.

  cc->sub(i, x0);                                            //   i -= x0;
  cc->add(x0, i);                                            //   x0 += i;
  cc->jmp(L_VLoop_Init);                                     //   goto L_VLoop_Init;

  // VLoop - Main VMask Loop - 8 Pixels (256-bit SIMD)
  // -------------------------------------------------

  if (vProcPixelCount == 8u) {
    Label L_VLoop_Iter8 = cc->newLabel();
    Label L_VLoop_End = cc->newLabel();

    cc->bind(L_VLoop_Iter8);                                 // L_VLoop_Iter8:
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

    cc->add(cellPtr, 8 * 4);                                 //   cellPtr += 8 * sizeof(uint32_t);
    pc->v_add_i32(cov, cov, x86::ptr(cellPtr));              //   cov[7:0] += cellPtr[7:0]
    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
    pc->v_storeu_i256(x86::ptr(cellPtr, -16), vecZero.ymm());//   cellPtr[3:-4] = 0;

    cc->bind(L_VLoop_Init);                                  // L_VLoop_Init:
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

    cc->sub(i, 8);                                           //   if ((i -= 8) >= 0)
    cc->jnc(L_VLoop_Iter8);                                  //     goto L_VLoop_Iter8;

    cc->add(i, 8);                                           //   if ((i += 8) == 0)
    cc->jz(L_VLoop_End);                                     //     goto L_VLoop_End;

    cc->cmp(i, 4);                                           //   if (i < 4)
    cc->jb(L_VTail_Init);                                    //     goto L_VTail_Init;

    cc->add(cellPtr, 4 * 4);                                 //   cellPtr += 4 * sizeof(uint32_t);
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
    cc->sub(i, 4);                                           //   if ((i -= 4) > 0)
    cc->jnz(L_VTail_Init);                                   //     goto L_VTail_Init;

    cc->bind(L_VLoop_End);                                   // L_VLoop_End:
    cc->vextracti128(cov.xmm(), cov, 0);
    cc->cmp(x0, xEnd);                                       //   if (x0 >= xEnd)
    cc->jae(L_Scanline_Done1);                               //     goto L_Scanline_Done1;
  }

  // VLoop - Main VMask Loop - 4 Pixels
  // ----------------------------------

  else if (vProcPixelCount == 4u) {
    Label L_VLoop_Cont = cc->newLabel();

    cc->bind(L_VLoop_Cont);                                  // L_VLoop_Cont:

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

    cc->add(cellPtr, 16);                                    //   cellPtr += 4 * sizeof(uint32_t);
    pc->v_add_i32(cov, cov, x86::ptr(cellPtr));              //   cov[3:0] += cellPtr[3:0];
    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
    pc->v_storea_i128(x86::ptr(cellPtr), vecZero);           //   cellPtr[3:0] = 0;
    dPix.resetAllExceptTypeAndName();

    cc->bind(L_VLoop_Init);                                  // L_VLoop_Init:
    accumulateCoverages(cov);
    calcMasksFromCells(m[0], cov, fillRuleMask, globalAlpha);
    normalizeCoverages(cov);

    if (pixelType == PixelType::kRGBA32) {
      x86::Xmm m0Xmm = m[0].xmm();
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

    cc->sub(i, 4);                                           //   if ((i -= 4) >= 0)
    cc->jnc(L_VLoop_Cont);                                   //     goto L_VLoop_Cont;

    cc->add(i, 4);                                           //   if ((i += 4) != 0)
    cc->jnz(L_VTail_Init);                                   //     goto L_VTail_Init;

    cc->cmp(x0, xEnd);                                       //   if (x0 >= xEnd)
    cc->jae(L_Scanline_Done1);                               //     goto L_Scanline_Done1;
  }

  // VLoop - Main VMask Loop - 1 Pixel
  // ---------------------------------

  else {
    Label L_VLoop_Iter = cc->newLabel();
    Label L_VLoop_Step = cc->newLabel();

    cc->bind(L_VLoop_Iter);                                  // L_VLoop_Iter:

    if (pixelGranularity >= 4)
      compOpPart()->enterPartialMode();                      //   <CompOpPart::enterPartialMode>

    if (pixelType == PixelType::kRGBA32) {
      pc->v_sllb_u128(m[0], m[0], 6);                        //   m0[7:0] = [__ M3 M2 M1 M0 __ __ __]

      cc->bind(L_VLoop_Step);                                // L_VLoop_Step:
      pc->v_swizzle_lo_u16(m[0], m[0], shuffleImm(3, 3, 3, 3));// m0[7:0] = [__ M3 M2 M1 M0 M0 M0 M0]

      compOpPart()->vMaskProcRGBA32Vec(dPix, PixelCount(1), PixelFlags::kPC | PixelFlags::kImmutable, m, true, pc->emptyPredicate());

      pc->xStorePixel(dstPtr, dPix.pc[0], 1, dstBpp, Alignment(1));
      dPix.resetAllExceptTypeAndName();

      cc->sub(i, 1);                                         //   i--;
      cc->add(dstPtr, dstBpp);                               //   dstPtr += dstBpp;
      cc->add(cellPtr, 4);                                   //   cellPtr += 4;
      pc->v_srlb_u128(m[0], m[0], 2);                        //   m0[7:0] = [0, m[7:1]]
    }
    else if (pixelType == PixelType::kA8) {
      cc->bind(L_VLoop_Step);                                // L_VLoop_Step:

      x86::Gp msk = cc->newUInt32();
      pc->v_extract_u16(msk, m[0], 0);

      compOpPart()->vMaskProcA8Gp(dPix, PixelFlags::kSA | PixelFlags::kImmutable, msk, false);

      pc->i_store_u8(x86::ptr(dstPtr), dPix.sa);
      dPix.resetAllExceptTypeAndName();

      cc->sub(i, 1);                                         //   i--;
      cc->add(dstPtr, dstBpp);                               //   dstPtr += dstBpp;
      cc->add(cellPtr, 4);                                   //   cellPtr += 4;
      pc->v_srlb_u128(m[0], m[0], 2);                        //   m0[7:0] = [0, m[7:1]]
    }

    if (pixelGranularity >= 4)
      compOpPart()->nextPartialPixel();                      //   <CompOpPart::nextPartialPixel>

    cc->test(i, 0x3);                                        //   if (i % 4 != 0)
    cc->jnz(L_VLoop_Step);                                   //     goto L_VLoop_Step;

    if (pixelGranularity >= 4)
      compOpPart()->exitPartialMode();                       //   <CompOpPart::exitPartialMode>

    // We must use unaligned loads here as we don't know whether we are at the end of the scanline. In that case
    // `cellPtr` could already be misaligned if the image width is not divisible by 4.

    if (!pc->hasAVX()) {
      x86::Xmm covTmp = cc->newXmm("covTmp");
      pc->v_loadu_i128(covTmp, x86::ptr(cellPtr));           //   covTmp[3:0] = cellPtr[3:0];
      pc->v_add_i32(cov, cov, covTmp);                       //   cov[3:0] += covTmp
    }
    else {
      pc->v_add_i32(cov, cov, x86::ptr(cellPtr));            //   cov[3:0] += cellPtr[3:0]
    }

    pc->v_zero_i(vecZero);                                   //   vecZero = 0;
    pc->v_storeu_i128(x86::ptr(cellPtr), vecZero);           //   cellPtr[3:0] = 0;

    cc->bind(L_VLoop_Init);                                  // L_VLoop_Init:

    accumulateCoverages(cov);
    calcMasksFromCells(m[0], cov, fillRuleMask, globalAlpha);
    normalizeCoverages(cov);

    cc->test(i, i);                                          //   if (i != 0)
    cc->jnz(L_VLoop_Iter);                                   //     goto L_VLoop_Iter;

    cc->cmp(x0, xEnd);                                       //   if (x0 >= xEnd)
    cc->jae(L_Scanline_Done1);                               //     goto L_Scanline_Done1;
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

  Label L_BitGap_Match = cc->newLabel();
  Label L_BitGap_Cont = cc->newLabel();

  cc->test(bitWord, bitWord);                                //   if (bitWord != 0)
  cc->jnz(L_BitGap_Match);                                   //     goto L_BitGap_Match;

  // Loop unrolled 2x as we could be inside a larger span.

  cc->bind(L_BitGap_Cont);                                   // L_BitGap_Cont:
  cc->add(xOff, pixelsPerBitWord);                           //   xOff += pixelsPerBitWord;
  cc->cmp(bitPtr, bitPtrEnd);                                //   if (bitPtr == bitPtrEnd)
  cc->jz(L_Scanline_Done1);                                  //     goto L_Scanline_Done1;

  cc->or_(bitWord, x86::ptr(bitPtr));                        //   bitWord |= bitPtr[0];
  cc->lea(bitPtr, x86::ptr(bitPtr, bwSize));                 //   bitPtr += bwSize;
  cc->jnz(L_BitGap_Match);                                   //   if (bitWord != 0) goto L_BitGap_Match;

  cc->add(xOff, pixelsPerBitWord);                           //   xOff += pixelsPerBitWord;
  cc->cmp(bitPtr, bitPtrEnd);                                //   if (bitPtr == bitPtrEnd)
  cc->jz(L_Scanline_Done1);                                  //     goto L_Scanline_Done1;

  cc->or_(bitWord, x86::ptr(bitPtr));                        //   bitWord |= bitPtr[0];
  cc->lea(bitPtr, x86::ptr(bitPtr, bwSize));                 //   bitPtr += bwSize;
  cc->jz(L_BitGap_Cont);                                     //   if (bitWord == 0) goto L_BitGap_Cont;

  cc->bind(L_BitGap_Match);                                  // L_BitGap_Match:
  cc->mov(x86::ptr(bitPtr, -bwSize, uint32_t(bwSize)), 0);   //   bitPtr[-1] = 0;
  pc->i_ctz(i.cloneAs(bitWord), bitWord);                    //   i = ctz(bitWord);
  cc->mov(bitWordTmp, -1);                                   //   bitWordTmp = -1; (all ones)
  pc->v_extract_u16(cMaskAlpha, m[0].xmm(), 0);              //   cMaskAlpha = extracti16(m0, 0);

  pc->i_shl(bitWordTmp, bitWordTmp, i);                      //   bitWordTmp <<= i;
  pc->i_shl(i, i, imm(pixelsPerOneBitShift));                //   i <<= pixelsPerOneBitShift;

  cc->xor_(bitWord, bitWordTmp);                             //   bitWord ^= bitWordTmp;
  cc->add(i, xOff);                                          //   i += xOff;
  cc->sub(i, x0);                                            //   i -= x0;
  cc->add(x0, i);                                            //   x0 += i;
  pc->i_add_scaled(cellPtr, i.cloneAs(cellPtr), 4);          //   cellPtr += i * sizeof(uint32_t);

  cc->test(cMaskAlpha, cMaskAlpha);                          //   if (cMaskAlpha != 0)
  cc->jnz(L_CLoop_Init);                                     //     goto L_CLoop_Init;

  // Fully-Transparent span where `cMaskAlpha == 0`.

  pc->i_add_scaled(dstPtr, i.cloneAs(dstPtr), int(dstBpp));  //   dstPtr += i * dstBpp;

  compOpPart()->postfetchN();
  compOpPart()->advanceX(x0, i);
  compOpPart()->prefetchN();

  cc->test(bitWord, bitWord);                                //   if (bitWord != 0)
  cc->jnz(L_BitScan_Match);                                  //     goto L_BitScan_Match;
  cc->jmp(L_BitScan_Iter);                                   //   goto L_BitScan_Iter;

  // CLoop
  // -----

  cc->bind(L_CLoop_Init);                                    // L_CLoop_Init:
  if (compOpPart()->shouldOptimizeOpaqueFill()) {
    Label L_CLoop_Msk = cc->newLabel();
    pc->i_jump_if_not_opaque_mask(cMaskAlpha, L_CLoop_Msk);  //   if (cMaskAlpha != 255) goto L_CLoop_Msk

    compOpPart()->cMaskInitOpaque();
    if (pixelGranularity >= 4)
      compOpPart()->cMaskGranularLoop(i);
    else
      compOpPart()->cMaskGenericLoop(i);
    compOpPart()->cMaskFini();

    cc->test(bitWord, bitWord);                              //   if (bitWord != 0)
    cc->jnz(L_BitScan_Match);                                //     goto L_BitScan_Match;
    cc->jmp(L_BitScan_Iter);                                 //   goto L_BitScan_Iter;

    cc->bind(L_CLoop_Msk);                                   // L_CLoop_Msk:
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

  cc->test(bitWord, bitWord);                                //   if (bitWord != 0)
  cc->jnz(L_BitScan_Match);                                  //     goto L_BitScan_Match;
  cc->jmp(L_BitScan_Iter);                                   //   goto L_BitScan_Iter;

  // VTail - Tail `vMask` loop for pixels near the end of the scanline
  // -----------------------------------------------------------------

  if (maxPixels >= 4u) {
    Label L_VTail_Cont = cc->newLabel();

    x86::Vec mXmm = m[0].xmm();
    VecArray msk(mXmm);

    // Tail loop can handle up to `pixelsPerOneBit - 1`.
    if (pixelType == PixelType::kRGBA32) {
      bool hasYmmMask = m[0].isYmm();

      cc->bind(L_VTail_Init);                                // L_VTail_Init:
      pc->i_add_scaled(cellPtr, i, 4);                       //   cellPtr += i * sizeof(uint32_t);

      if (!hasYmmMask) {
        pc->v_swap_u64(m[1], m[1]);
      }
      compOpPart()->enterPartialMode();                      //   <CompOpPart::enterPartialMode>

      cc->bind(L_VTail_Cont);                                // L_VTail_Cont:
      compOpPart()->vMaskProcRGBA32Vec(dPix, PixelCount(1), PixelFlags::kPC | PixelFlags::kImmutable, msk, true, pc->emptyPredicate());

      pc->xStorePixel(dstPtr, dPix.pc[0], 1, dstBpp, Alignment(1));
      cc->add(dstPtr, dstBpp);                               //   dstPtr += dstBpp;

      if (!hasYmmMask) {
        pc->v_interleave_hi_u64(m[0], m[0], m[1]);
      }
      else {
        // All 4 expanded masks for ARGB channels are in a single register, so just permute.
        pc->v_perm_i64(m[0], m[0], shuffleImm(0, 3, 2, 1));
      }

      compOpPart()->nextPartialPixel();                      //   <CompOpPart::nextPartialPixel>
      dPix.resetAllExceptTypeAndName();

      cc->sub(i, 1);                                         //   if (--i)
      cc->jnz(L_VTail_Cont);                                 //     goto L_VTail_Cont;

      compOpPart()->exitPartialMode();                       //   <CompOpPart::exitPartialMode>
    }
    else if (pixelType == PixelType::kA8) {
      x86::Gp mScalar = cc->newUInt32("mScalar");

      cc->bind(L_VTail_Init);                                // L_VTail_Init:
      pc->i_add_scaled(cellPtr, i, 4);                       //   cellPtr += i * sizeof(uint32_t);
      compOpPart()->enterPartialMode();                      //   <CompOpPart::enterPartialMode>

      cc->bind(L_VTail_Cont);                                // L_VTail_Cont:
      pc->v_extract_u16(mScalar, mXmm, 0);
      compOpPart()->vMaskProcA8Gp(dPix, PixelFlags::kSA | PixelFlags::kImmutable, mScalar, false);

      pc->i_store_u8(x86::ptr(dstPtr), dPix.sa);
      cc->add(dstPtr, dstBpp);                               //   dstPtr += dstBpp;
      pc->v_srlb_u128(mXmm, mXmm, 2);                        //   m0[7:0] = [0, m[7:1]]
      compOpPart()->nextPartialPixel();                      //   <CompOpPart::nextPartialPixel>
      dPix.resetAllExceptTypeAndName();

      cc->sub(i, 1);                                         //   if (--i)
      cc->jnz(L_VTail_Cont);                                 //     goto L_VTail_Cont;

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

  cc->bind(L_Scanline_Done0);                                // L_Scanline_Done0:
  pc->v_zero_i(vecZero);                                     //   vecZero = 0;
  pc->v_storeu_i128(x86::ptr(cellPtr), vecZero);             //   cellPtr[3:0] = 0;

  cc->bind(L_Scanline_Done1);                                // L_Scanline_Done1:
  disadvanceDstPtrAndCellPtr(dstPtr,                         //   dstPtr -= x0 * dstBpp;
                             cellPtr, x0, dstBpp);           //   cellPtr -= x0 * sizeof(uint32_t);
  cc->sub(y, 1);                                             //   if (--y == 0)
  cc->jz(L_End);                                             //     goto L_End;
  cc->mov(bitPtr, bitPtrEnd);                                //   bitPtr = bitPtrEnd;

  cc->bind(L_Scanline_AdvY);                                 // L_Scanline_AdvY:
  cc->add(dstPtr, dstStride);                                //   dstPtr += dstStride;
  cc->add(bitPtr, bitPtrSkipLen);                            //   bitPtr += bitPtrSkipLen;
  cc->add(cellPtr, cellStride);                              //   cellPtr += cellStride;
  compOpPart()->advanceY();                                  //   <CompOpPart::AdvanceY>

  cc->bind(L_Scanline_Init);                                 // L_Scanline_Init:
  cc->mov(xOff, xStart);                                     //   xOff = xStart;
  cc->mov(bitPtrEnd, bitPtr);                                //   bitPtrEnd = bitPtr;
  cc->add(bitPtrEnd, bitPtrRunLen);                          //   bitPtrEnd += bitPtrRunLen;
  cc->xor_(bitWord, bitWord);                                //   bitWord = 0;

  cc->bind(L_Scanline_Iter);                                 // L_Scanline_Iter:
  cc->or_(bitWord, x86::ptr(bitPtr));                        //   bitWord |= bitPtr[0];
  cc->lea(bitPtr, x86::ptr(bitPtr, bwSize));                 //   bitPtr += bwSize;
  cc->jnz(L_BitScan_Init);                                   //   if (bitWord) goto L_BitScan_Init;

  cc->add(xOff, pixelsPerBitWord);                           //   xOff += pixelsPerBitWord;
  cc->cmp(bitPtr, bitPtrEnd);                                //   if (bitPtr != bitPtrEnd)
  cc->jnz(L_Scanline_Iter);                                  //     goto L_Scanline_Iter;

  cc->dec(y);                                                //   if (--y)
  cc->jnz(L_Scanline_AdvY);                                  //     goto L_Scanline_AdvY;

  // Epilog
  // ------

  cc->bind(L_End);
  compOpPart()->fini();
  _finiGlobalHook();
}

void FillAnalyticPart::accumulateCoverages(const x86::Vec& cov) noexcept {
  x86::Vec tmp = cc->newSimilarReg<x86::Vec>(cov, "vCovTmp");

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

void FillAnalyticPart::normalizeCoverages(const x86::Vec& cov) noexcept {
  pc->v_srlb_u128(cov, cov, 12);                             //   cov[3:0]  = [  0     0     0     c0 ];
}

// Calculate masks from cell and store them to a vector of the following layout:
//
//   [__ __ __ __ M7 M6 M5 M4|__ __ __ __ M3 M2 M1 M0]
//
// NOTE: Depending on the vector size the output mask is for either 4 or 8 pixels.
void FillAnalyticPart::calcMasksFromCells(const x86::Vec& dst_, const x86::Vec& cov, const x86::Vec& fillRuleMask, const x86::Vec& globalAlpha) noexcept {
  x86::Vec dst = dst_.cloneAs(cov);

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

void FillAnalyticPart::disadvanceDstPtrAndCellPtr(const x86::Gp& dstPtr, const x86::Gp& cellPtr, const x86::Gp& x, uint32_t dstBpp) noexcept {
  x86::Gp xAdv = x.cloneAs(dstPtr);

  if (dstBpp == 1) {
    cc->sub(dstPtr, xAdv);
    cc->shl(xAdv, 2);
    cc->sub(cellPtr, xAdv);
  }
  else if (dstBpp == 2) {
    cc->shl(xAdv, 1);
    cc->sub(dstPtr, xAdv);
    cc->shl(xAdv, 1);
    cc->sub(cellPtr, xAdv);
  }
  else if (dstBpp == 4) {
    cc->shl(xAdv, 2);
    cc->sub(dstPtr, xAdv);
    cc->sub(cellPtr, xAdv);
  }
  else {
    x86::Gp dstAdv = cc->newIntPtr("dstAdv");
    pc->i_mul(dstAdv, xAdv, dstBpp);
    cc->shl(xAdv, 2);
    cc->sub(dstPtr, dstAdv);
    cc->sub(cellPtr, xAdv);
  }
}

} // {JIT}
} // {BLPipeline}

#endif
