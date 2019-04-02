// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../pipegen/blcompoppart_p.h"
#include "../pipegen/blfillpart_p.h"
#include "../pipegen/blfetchpart_p.h"
#include "../pipegen/blfetchpixelptrpart_p.h"
#include "../pipegen/blpipecompiler_p.h"
#include "../pipegen/blpipedebug_p.h"

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::FillPart - Construction / Destruction]
// ============================================================================

FillPart::FillPart(PipeCompiler* pc, uint32_t fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : PipePart(pc, kTypeFill),
    _fillType(uint8_t(fillType)),
    _isRectFill(false) {

  // Initialize the children of this part.
  _children[kIndexDstPart] = dstPart;
  _children[kIndexCompOpPart] = compOpPart;
  _childrenCount = 2;
}

// ============================================================================
// [BLPipeGen::FillBoxAAPart - Construction / Destruction]
// ============================================================================

FillBoxAAPart::FillBoxAAPart(PipeCompiler* pc, uint32_t fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : FillPart(pc, fillType, dstPart, compOpPart) {

  _maxOptLevelSupported = kOptLevel_X86_AVX;
  _isRectFill = true;

  _persistentRegs[x86::Reg::kGroupGp] = 2;
  _spillableRegs[x86::Reg::kGroupGp] = 3;
}

// ============================================================================
// [BLPipeGen::FillBoxAAPart - Compile]
// ============================================================================

void FillBoxAAPart::compile() noexcept {
  _initGlobalHook(cc->cursor());

  x86::Gp ctxData        = pc->_ctxData;
  x86::Gp fillData       = pc->_fillData;

  x86::Gp dstPtr         = cc->newIntPtr("dstPtr");        // Reg.
  x86::Gp dstStride      = cc->newIntPtr("dstStride");     // Reg/Mem.

  x86::Gp x              = cc->newUInt32("x");             // Reg.
  x86::Gp y              = cc->newUInt32("y");             // Reg/Mem.
  x86::Gp w              = cc->newUInt32("w");             // Reg/Mem.
  x86::Gp m              = cc->newUInt32("m");             // Reg/Tmp.

  int dstBpp = int(dstPart()->bpp());

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  cc->mov(dstPtr, cc->intptr_ptr(ctxData, BL_OFFSET_OF(BLPipeContextData, dst.stride)));
  cc->mov(y, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAA, box.y0)));

  cc->mov(dstStride, dstPtr);
  cc->mov(w, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAA, box.x0)));
  cc->imul(dstPtr, y.cloneAs(dstPtr));

  dstPart()->initPtr(dstPtr);
  compOpPart()->init(w, y, 1);

  cc->neg(y);
  pc->uLeaBpp(dstPtr, dstPtr, w, dstBpp);
  cc->neg(w);

  cc->add(dstPtr, cc->intptr_ptr(ctxData, BL_OFFSET_OF(BLPipeContextData, dst.pixelData)));
  cc->add(w, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAA, box.x1)));

  pc->uMulImm(x, w, dstBpp);
  cc->add(y, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAA, box.y1)));
  cc->sub(dstStride, x.cloneAs(dstStride));

  if (hasLowGpRegs()) {
    cc->spill(w);
    cc->spill(y);
    cc->spill(dstStride);
  }

  // --------------------------------------------------------------------------
  // [Loop]
  // --------------------------------------------------------------------------

  if (compOpPart()->shouldOptimizeOpaqueFill()) {
    Label L_FullAlpha_Loop  = cc->newLabel();
    Label L_SemiAlpha_Init = cc->newLabel();
    Label L_SemiAlpha_Loop = cc->newLabel();

    Label L_End  = cc->newLabel();

    cc->mov(m, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAA, alpha)));
    cc->cmp(m, 255);
    cc->jbe(L_SemiAlpha_Init);

    // Full Alpha
    // ----------

    compOpPart()->cMaskInit(pc->_gpNone);

    cc->bind(L_FullAlpha_Loop);
    cc->mov(x, w);

    compOpPart()->startAtX(pc->_gpNone);
    compOpPart()->cMaskGenericLoop(x);

    cc->add(dstPtr, dstStride);
    compOpPart()->advanceY();

    cc->sub(y, 1);
    cc->jnz(L_FullAlpha_Loop);

    compOpPart()->cMaskFini();
    cc->jmp(L_End);

    // Semi Alpha
    // ----------

    cc->bind(L_SemiAlpha_Init);
    compOpPart()->cMaskInit(m);

    cc->bind(L_SemiAlpha_Loop);
    cc->mov(x, w);

    compOpPart()->startAtX(pc->_gpNone);
    compOpPart()->cMaskGenericLoop(x);

    cc->add(dstPtr, dstStride);
    compOpPart()->advanceY();

    cc->sub(y, 1);
    cc->jnz(L_SemiAlpha_Loop);

    compOpPart()->cMaskFini();
    cc->bind(L_End);
  }
  else {
    Label L_AnyAlphaLoop = cc->newLabel();

    compOpPart()->cMaskInit(x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAA, alpha)));

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

  compOpPart()->fini();
  _finiGlobalHook();
}

// ============================================================================
// [BLPipeGen::FillBoxAUPart - Construction / Destruction]
// ============================================================================

FillBoxAUPart::FillBoxAUPart(PipeCompiler* pc, uint32_t fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : FillPart(pc, fillType, dstPart, compOpPart) {

  _maxOptLevelSupported = kOptLevel_X86_AVX;
  _isRectFill = true;

  _persistentRegs[x86::Reg::kGroupGp] = 5;
  _spillableRegs[x86::Reg::kGroupGp] = 1;
}

// ============================================================================
// [BLPipeGen::FillBoxAUPart - Compile]
// ============================================================================

void FillBoxAUPart::compile() noexcept {
  _initGlobalHook(cc->cursor());

  Label L_VertLoop       = cc->newLabel();
  Label L_VMaskInit      = cc->newLabel();
  Label L_VMaskLoop      = cc->newLabel();
  Label L_CMask          = cc->newLabel();
  Label L_End            = cc->newLabel();

  x86::Gp ctxData        = pc->_ctxData;
  x86::Gp fillData       = pc->_fillData;

  x86::Gp dstPtr         = cc->newIntPtr("dstPtr");        // Reg.
  x86::Gp dstStride      = cc->newIntPtr("dstStride");     // Reg/Mem.

  x86::Gp x              = cc->newUInt32("x");             // Reg.
  x86::Gp y              = cc->newUInt32("y");             // Reg.
  x86::Gp h              = cc->newUInt32("h");             // Reg/Mem.

  x86::Gp startWidth     = cc->newUInt32("startWidth");    // Reg/Mem.
  x86::Gp innerWidth     = cc->newUInt32("innerWidth");    // Reg/Mem.

  x86::Gp pMasks         = cc->newIntPtr("pMasks");        // Reg.
  x86::Gp masks          = cc->newUInt32("masks");         // Reg.
  x86::Gp m              = cc->newUInt32("m");             // Reg/Tmp.

  int dstBpp = int(dstPart()->bpp());

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  cc->mov(dstPtr, cc->intptr_ptr(ctxData, BL_OFFSET_OF(BLPipeContextData, dst.stride)));
  cc->mov(h, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAU, box.y0)));

  cc->mov(dstStride, dstPtr);
  cc->mov(innerWidth, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAU, box.x0)));
  cc->imul(dstPtr, h.cloneAs(dstPtr));

  dstPart()->initPtr(dstPtr);
  compOpPart()->init(innerWidth, h, 1);

  cc->neg(h);
  pc->uLeaBpp(dstPtr, dstPtr, innerWidth, dstBpp);
  cc->neg(innerWidth);

  cc->add(dstPtr, cc->intptr_ptr(ctxData, BL_OFFSET_OF(BLPipeContextData, dst.pixelData)));
  cc->add(innerWidth, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAU, box.x1)));

  pc->uMulImm(x, innerWidth, dstBpp);
  cc->add(h, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAU, box.y1)));
  cc->sub(dstStride, x.cloneAs(dstStride));

  cc->mov(startWidth, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAU, startWidth)));
  cc->mov(innerWidth, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAU, innerWidth)));

  cc->spill(startWidth);
  cc->spill(innerWidth);

  cc->sub(h, 1);
  cc->mov(y, 1);

  cc->spill(h);
  cc->spill(dstStride);

  cc->lea(pMasks, x86::ptr(fillData, BL_OFFSET_OF(BLPipeFillData::BoxAU, masks)));

  // --------------------------------------------------------------------------
  // [Loop - VMask]
  // --------------------------------------------------------------------------

  cc->bind(L_VertLoop);
  compOpPart()->startAtX(pc->_gpNone);
  cc->mov(masks, x86::ptr(pMasks));
  cc->mov(x, startWidth);

  cc->bind(L_VMaskInit);
  compOpPart()->prefetch1();

  cc->bind(L_VMaskLoop);
  cc->mov(m, masks);
  cc->and_(m, 0x1FF);
  cc->shr(masks, 9);

  PixelARGB p;
  compOpPart()->vMaskProc(p, PixelARGB::kPC | PixelARGB::kImmutable, m);
  pc->xStore32_ARGB(dstPtr, p.pc[0]);

  cc->add(dstPtr, dstBpp);
  cc->sub(x, 1);
  cc->jnz(L_VMaskLoop);

  cc->test(masks, masks);
  cc->jnz(L_CMask);

  // Advance-Y.
  cc->add(dstPtr, dstStride);
  compOpPart()->advanceY();

  cc->sub(y, 1);
  cc->jnz(L_VertLoop);

  cc->add(y, h);
  cc->lea(pMasks, x86::ptr(pMasks, 4));
  cc->jz(L_End);

  cc->cmp(y, 1);
  cc->mov(h, 0);
  cc->je(L_VertLoop);

  cc->sub(y, 1);
  cc->mov(h, 1);
  cc->jmp(L_VertLoop);

  // --------------------------------------------------------------------------
  // [Loop - CMask]
  // --------------------------------------------------------------------------

  cc->bind(L_CMask);
  cc->mov(m, masks);
  cc->and_(m, 0x1FF);
  cc->mov(x, innerWidth);

  if (compOpPart()->shouldOptimizeOpaqueFill()) {
    Label L_Skip = cc->newLabel();

    cc->cmp(m, 255);
    cc->jbe(L_Skip);

    if (hasLowGpRegs())
      cc->spill(masks);

    compOpPart()->cMaskInit(pc->_gpNone);
    compOpPart()->cMaskGenericLoop(x);
    compOpPart()->cMaskFini();

    if (hasLowGpRegs())
      cc->alloc(masks);

    cc->shr(masks, 9);
    cc->inc(x);
    cc->jmp(L_VMaskInit);

    cc->bind(L_Skip);
  }

  compOpPart()->cMaskInit(m);
  compOpPart()->cMaskGenericLoop(x);
  compOpPart()->cMaskFini();

  cc->shr(masks, 9);
  cc->inc(x);
  cc->jmp(L_VMaskInit);

  cc->bind(L_End);
  compOpPart()->fini();

  _finiGlobalHook();
}


// ============================================================================
// [BLPipeGen::FillAnalyticPart - Construction / Destruction]
// ============================================================================

FillAnalyticPart::FillAnalyticPart(PipeCompiler* pc, uint32_t fillType, FetchPixelPtrPart* dstPart, CompOpPart* compOpPart) noexcept
  : FillPart(pc, fillType, dstPart, compOpPart) {

  _maxOptLevelSupported = kOptLevel_X86_AVX;

  _persistentRegs[x86::Reg::kGroupGp]  = 5;
  _persistentRegs[x86::Reg::kGroupVec] = 1;

  _spillableRegs[x86::Reg::kGroupGp]   = 4;
  _spillableRegs[x86::Reg::kGroupVec]  = 2;

  _temporaryRegs[x86::Reg::kGroupGp]   = 2;
}

// ============================================================================
// [BLPipeGen::FillAnalyticPart - Compile]
// ============================================================================

void FillAnalyticPart::compile() noexcept {
  using x86::Predicate::shuf;

  _initGlobalHook(cc->cursor());

  Label L_BitScan_Init   = cc->newLabel();
  Label L_BitScan_Next   = cc->newLabel();
  Label L_BitScan_Match  = cc->newLabel();
  Label L_BitScan_End    = cc->newLabel();

  Label L_VLoop_Init     = cc->newLabel();
  Label L_VLoop_Cont     = cc->newLabel();

  Label L_VTail_Init     = cc->newLabel(); // Only used if maxPixels >= 4.
  Label L_VTail_Cont     = cc->newLabel(); // Only used if maxPixels >= 4.

  Label L_CLoop_Init     = cc->newLabel();

  Label L_Scanline_Done0 = cc->newLabel();
  Label L_Scanline_Done1 = cc->newLabel();
  Label L_Scanline_AdvY  = cc->newLabel();
  Label L_Scanline_Init  = cc->newLabel();
  Label L_Scanline_Cont  = cc->newLabel();

  Label L_End            = cc->newLabel();

  x86::Gp ctxData        = pc->_ctxData;
  x86::Gp fillData       = pc->_fillData;

  x86::Gp dstPtr         = cc->newIntPtr("dstPtr");        // Reg.
  x86::Gp dstStride      = cc->newIntPtr("dstStride");     // Mem.

  x86::Gp bitPtr         = cc->newIntPtr("bitPtr");        // Reg.
  x86::Gp bitPtrEnd      = cc->newIntPtr("bitPtrEnd");     // Reg/Mem.
  x86::Gp bitStride      = cc->newIntPtr("bitStride");     // Mem.

  x86::Gp cellPtr        = cc->newIntPtr("cellPtr");       // Reg.
  x86::Gp cellStride     = cc->newIntPtr("cellStride");    // Mem.

  x86::Gp x0             = cc->newUInt32("x0");            // Reg
  x86::Gp xOff           = cc->newUInt32("xOff");          // Reg/Mem.
  x86::Gp xEnd           = cc->newUInt32("xEnd");          // Mem.

  x86::Gp y              = cc->newUInt32("y");             // Reg/Mem.
  x86::Gp i              = cc->newUInt32("i");             // Reg.
  x86::Gp cMaskAlpha     = cc->newUInt32("cMaskAlpha");    // Reg/Tmp.

  x86::Gp bitWord        = cc->newUIntPtr("bitWord");      // Reg/Mem.
  x86::Gp bitWordTmp     = cc->newUIntPtr("bitWordTmp");   // Reg/Tmp.

  x86::Xmm globalAlpha   = cc->newXmm("globalAlpha");      // Mem.
  x86::Xmm cov           = cc->newXmm("cov");              // Reg.
  x86::Xmm fillRuleMask  = cc->newXmm("fillRuleMask");     // Mem.

  VecArray m;
  pc->newXmmArray(m, 2, "m");

  PixelARGB dPix;

  int dstBpp = int(dstPart()->bpp());
  int bwSize = int(sizeof(BLBitWord));
  int bwSizeInBits = bwSize * 8;

  int pixelsPerOneBit = 4;
  int pixelsPerOneBitShift = blBitCtz(pixelsPerOneBit);

  int pixelsPerBitWord = pixelsPerOneBit * bwSizeInBits;
  int pixelGranularity = pixelsPerOneBit;

  if (compOpPart()->maxPixelsOfChildren() < 4)
    pixelGranularity = 1;

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  // Initialize the destination.
  cc->mov(y, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::Analytic, box.y0)));
  cc->mov(dstStride, cc->intptr_ptr(ctxData, BL_OFFSET_OF(BLPipeContextData, dst.stride)));

  cc->mov(dstPtr.r32(), y);
  cc->imul(dstPtr, dstStride);
  cc->add(dstPtr, cc->intptr_ptr(ctxData, BL_OFFSET_OF(BLPipeContextData, dst.pixelData)));
  cc->spill(dstStride);

  // Initialize cell pointers.
  cc->mov(bitStride , cc->intptr_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::Analytic, bitStride)));
  cc->mov(cellStride, cc->intptr_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::Analytic, cellStride)));

  cc->spill(bitStride);
  cc->spill(cellStride);

  cc->mov(bitPtr, cc->intptr_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::Analytic, bitTopPtr)));
  cc->mov(cellPtr, cc->intptr_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::Analytic, cellTopPtr)));

  // Initialize pipeline parts.
  dstPart()->initPtr(dstPtr);
  compOpPart()->init(pc-> _gpNone, y, pixelGranularity);

  // y = fillData->box.y1 - fillData->box.y0;
  cc->neg(y);
  cc->add(y, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::Analytic, box.y1)));

  // xEnd = fillData->box.x1;
  cc->mov(xEnd, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::Analytic, box.x1)));
  cc->spill(xEnd);

  pc->vloadi32(globalAlpha, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::Analytic, alpha)));
  pc->vswizli16(globalAlpha, globalAlpha, shuf(0, 0, 0, 0));
  pc->vswizi32(globalAlpha, globalAlpha, shuf(0, 0, 0, 0));
  // We shift left by 7 bits so we can use `pmulhuw` in `calcMasksFromCells()`.
  pc->vslli16(globalAlpha, globalAlpha, 7);
  cc->spill(globalAlpha);

  // Initialize fill-rule.
  pc->vloadi32(fillRuleMask, x86::dword_ptr(fillData, BL_OFFSET_OF(BLPipeFillData::Analytic, fillRuleMask)));
  pc->vswizi32(fillRuleMask, fillRuleMask, shuf(0, 0, 0, 0));
  cc->spill(fillRuleMask);

  if (hasLowGpRegs()) {
    cc->spill(y);
  }

  cc->jmp(L_Scanline_Init);

  // --------------------------------------------------------------------------
  // [BitScan]
  // --------------------------------------------------------------------------

  // Called by Scanline iterator on the first non-zero BitWord it matches. The
  // responsibility of BitMatch is to find the first bit in the passed BitWord
  // followed by matching the bit that ends this match. This would essentially
  // produce the first [x0, x1) span that has to be composited as 'VMask' loop.

  cc->bind(L_BitScan_Init);                                // L_BitScan_Init:
  pc->uCTZ(x0.cloneAs(bitWord), bitWord);                  //   x0 = ctz(bitWord);

  cc->mov(x86::ptr(bitPtr, -bwSize, bwSize), 0);           //   bitPtr[-1] = 0;
  cc->or_(bitWordTmp, -1);                                 //   bitWordTmp = -1; (all ones).
  pc->uShl(bitWordTmp, x0);                                //   bitWordTmp <<= x0;

  // Convert bit offset `x0` into a pixel offset. We must consider `xOff` as it's
  // only zero for the very first BitWord (all others are multiplies of `pixelsPerBitWord`).

  cc->shl(x0, pixelsPerOneBitShift);                       //   x0 <<= pixelsPerOneBitShift;
  cc->add(x0, xOff);                                       //   x0 += xOff;

  // Load the given cells to `m0` and clear the BitWord and all cells it represents
  // in memory. This is important as the compositor has to clear the memory during
  // compositing. If this is a rare case where `x0` points at the end of the raster
  // there is still one cell that is non-zero. This makes sure it's cleared.

  pc->uAddMulImm(dstPtr, x0.cloneAs(dstPtr), dstBpp);      //   dstPtr += x0 * dstBpp;
  pc->uAddMulImm(cellPtr, x0.cloneAs(cellPtr), 4);         //   cellPtr += x0 * 4;

  // Rare case - line rasterized at the end of the raster boundary. In 99% cases
  // this is a clipped line that was rasterized as vertical-only line at the end
  // of the render box. This is a completely valid case that produces nothing.

  cc->cmp(x0, xEnd);                                       //   if (x0 >= xEnd)
  cc->jae(L_Scanline_Done0);                               //     goto L_Scanline_Done0;

  // Setup compositor and source/destination parts. This is required as the fetcher
  // needs to know where to start. And since `startAtX()` can only be called once per
  // scanline we must do it here.

  compOpPart()->startAtX(x0);                              //   <CompOpPart::StartAtX>
  compOpPart()->prefetchN();                               //   <CompOpPart::PrefetchN>

  pc->vloadi128a(cov, pc->constAsMem(blCommonTable.i128_0002000000020000)); // cov[3:0] = 256 << 9;

  // If `xor(bitWord, bitWordTmp)` results in non-zero value it means that the current
  // span ends within the same BitWord, otherwise the span crosses multiple BitWords.

  cc->xor_(bitWord, bitWordTmp);                           //   if ((bitWord ^= bitWordTmp) != 0)
  cc->jnz(L_BitScan_Match);                                //     goto L_BitScan_Match;

  // Okay, so the span crosses multiple BitWords. Firstly we have to make sure this was
  // not the last one. If that's the case we must terminate the scannling immediately.

  cc->mov(i, bwSizeInBits);                                //   i = bwSizeInBits;
  cc->cmp(bitPtr, bitPtrEnd);                              //   if (bitPtr == bitPtrEnd)
  cc->jz(L_BitScan_End);                                   //     goto L_BitScan_End;

  // A BitScan loop - iterates over all consecutive BitWords and finds those that don't
  // have all bits set to 1.

  cc->bind(L_BitScan_Next);                                // L_BitScan_Next:
  cc->or_(bitWord, -1);                                    //   bitWord = -1; (all ones);
  cc->add(xOff, pixelsPerBitWord);                         //   xOff += pixelsPerBitWord;
  cc->xor_(bitWord, x86::ptr(bitPtr, 0, bwSize));          //   bitWord ^= bitPtr[0];
  cc->mov(x86::ptr(bitPtr, 0, bwSize), 0);                 //   bitPtr[0] = 0;
  cc->lea(bitPtr, x86::ptr(bitPtr, bwSize));               //   bitPtr += bwSize;
  cc->jnz(L_BitScan_Match);                                //   if (bitWord != 0) goto L_BitScan_Match;

  cc->cmp(bitPtr, bitPtrEnd);                              //   if (bitPtr != bitPtrEnd)
  cc->jz(L_BitScan_End);                                   //     goto L_BitScan_End;
  cc->jmp(L_BitScan_Next);                                 //   goto L_BitScan_Next;

  cc->bind(L_BitScan_Match);                               // L_BitScan_Match:
  pc->uCTZ(i.cloneAs(bitWord), bitWord);                   //   i = ctz(bitWord);

  cc->bind(L_BitScan_End);                                 // L_BitScan_End:
  pc->vloadi128a(m[0], x86::ptr(cellPtr));                 //   m0[3:0] = cellPtr[3:0];
  cc->or_(bitWordTmp, -1);                                 //   bitWordTmp = -1; (all ones).
  pc->uShl(bitWordTmp, i);                                 //   bitWordTmp <<= i;
  cc->shl(i, pixelsPerOneBitShift);                        //   i <<= pixelsPerOneBitShift;

  cc->xor_(bitWord, bitWordTmp);                           //   bitWord ^= bitWordTmp;
  cc->add(i, xOff);                                        //   i += xOff;
  pc->vzeropi(m[1]);                                       //   m1[3:0] = 0;

  // In cases where the raster width is not a multiply of `pixelsPerOneBit` we
  // must make sure we won't overflow it.

  cc->cmp(i, xEnd);                                        //   if (i > xEnd)
  cc->cmova(i, xEnd);                                      //     i = xEnd;
  pc->vstorei128a(x86::ptr(cellPtr), m[1]);                //   cellPtr[3:0] = 0;

  // `i` is now the number of pixels (and cells) to composite by using `vMask`.

  cc->sub(i, x0);                                          //   i -= x0;
  cc->add(x0, i);                                          //   x0 += i;

  cc->jmp(L_VLoop_Init);                                   //   goto L_VLoop_Init;

  // --------------------------------------------------------------------------
  // [VLoop - Main `vMask` loop [1 PIXEL]
  // --------------------------------------------------------------------------

  if (compOpPart()->maxPixels() < 4) {
    Label L_VLoop_Step = cc->newLabel();

    cc->bind(L_VLoop_Cont);                                // L_VLoop_Cont:
    if (pixelGranularity >= 4)
      compOpPart()->enterPartialMode();                    //   <CompOpPart::enterPartialMode>
    pc->vslli128b(m[0], m[0], 6);                          //   m0[7:0] = [__, M3, M2, M1, M0, __, __, __]

    cc->bind(L_VLoop_Step);                                // L_VLoop_Step:
    pc->vswizli16(m[0], m[0], shuf(3, 3, 3, 3));           //   m0[7:0] = [__, M3, M2, M1, M0, M0, M0, M0]

    compOpPart()->vMaskProc32Xmm1(dPix, PixelARGB::kPC | PixelARGB::kImmutable, m, true);

    pc->xStore32_ARGB(dstPtr, dPix.pc[0]);                 //   dstPtr[0] = dPix;
    dPix.reset();

    cc->sub(i, 1);                                         //   i--;
    cc->add(dstPtr, dstBpp);                               //   dstPtr += dstBpp;
    cc->add(cellPtr, 4);                                   //   cellPtr += 4;
    pc->vsrli128b(m[0], m[0], 2);                          //   m0[7:0] = [0, m[7:1]]

    if (pixelGranularity >= 4)
      compOpPart()->nextPartialPixel();                    //   <CompOpPart::nextPartialPixel>

    cc->test(i, 0x3);                                      //   if (i % 4 != 0)
    cc->jnz(L_VLoop_Step);                                 //     goto L_VLoop_Step;

    if (pixelGranularity >= 4)
      compOpPart()->exitPartialMode();                     //   <CompOpPart::exitPartialMode>

    // We must use unaligned loads here as we don't know whether we are at the
    // end of the scanline. In that case `cellPtr` might already be misaligned
    // if the image width is not divisible by 4.

    // TODO {AFTER BETA} It would be nice to know why unaligned read/writ is used here...
    pc->vzeropi(m[1]);                                     //   m1[3:0] = 0;
    pc->vloadi128u(m[0], x86::ptr(cellPtr));               //   m0[3:0] = cellPtr[3:0];
    pc->vstorei128u(x86::ptr(cellPtr), m[1]);              //   cellPtr[3:0] = 0;

    cc->bind(L_VLoop_Init);                                // L_VLoop_Init:

    accumulateCells(cov, m[0]);
    calcMasksFromCells(m[0], m[0], fillRuleMask, globalAlpha, false);

    cc->test(i, i);                                        //   if (i != 0)
    cc->jnz(L_VLoop_Cont);                                 //     goto L_VLoop_Cont;

    cc->cmp(x0, xEnd);                                     //   if (x0 >= xEnd)
    cc->jae(L_Scanline_Done1);                             //     goto L_Scanline_Done1;
  }

  // --------------------------------------------------------------------------
  // [VLoop - Main `vMask` loop [4 PIXELS]
  // --------------------------------------------------------------------------

  if (compOpPart()->maxPixels() >= 4) {
    cc->bind(L_VLoop_Cont);                                // L_VLoop_Cont:

    pc->vunpackli16(m[0], m[0], m[0]);                     //   m0 = [M3, M3, M2, M2, M1, M1, M0, M0]
    pc->vswizi32(m[1], m[0], shuf(3, 3, 2, 2));            //   m1 = [M3, M3, M3, M3, M2, M2, M2, M2]
    pc->vswizi32(m[0], m[0], shuf(1, 1, 0, 0));            //   m0 = [M1, M1, M1, M1, M0, M0, M0, M0]

    compOpPart()->vMaskProc32Xmm4(dPix, PixelARGB::kPC | PixelARGB::kImmutable, m, false);

    cc->add(cellPtr, 16);                                  //   cellPtr += 4 * 4;

    pc->vzeropi(m[1]);                                     //   m1[3:0] = 0;
    pc->vstorei128u(x86::ptr(dstPtr), dPix.pc[0]);
    pc->vloadi128a(m[0], x86::ptr(cellPtr));               //   m0[3:0] = cellPtr[3:0];
    cc->add(dstPtr, dstBpp * 4);                           //   dstPtr += dstBpp * 4;
    pc->vstorei128a(x86::ptr(cellPtr), m[1]);              //   cellPtr[3:0] = 0;

    dPix.reset();

    cc->bind(L_VLoop_Init);                                // L_VLoop_Init:

    accumulateCells(cov, m[0]);
    calcMasksFromCells(m[0], m[0], fillRuleMask, globalAlpha, false);

    cc->sub(i, 4);                                         // if ((i -= 4) >= 0)
    cc->jnc(L_VLoop_Cont);                                 //   goto L_VLoop_Cont;

    cc->add(i, 4);                                         // if ((i += 4) != 0)
    cc->jnz(L_VTail_Init);                                 //   goto L_VTail_Init;

    cc->cmp(x0, xEnd);                                     // if (x0 >= xEnd)
    cc->jae(L_Scanline_Done1);                             //   goto L_Scanline_Done1;
  }

  // --------------------------------------------------------------------------
  // [BitGap]
  // --------------------------------------------------------------------------

  // If we are here we are at the end of `vMask` loop. There are two possibilities:
  //
  //   1. There is a gap between bits in a single or multiple BitWords. This
  //      means that there is a possibility for a `cMask` loop which could be
  //      solid, masked, or have zero-mask (a real gap).
  //
  //   2. This was the last span and there are no more bits in consecutive BitWords.
  //      We will not consider this as a special case and just process the remaining
  //      BitWords in a normal way (scanning until the end of the current scanline).

  Label L_BitGap_Match = cc->newLabel();
  Label L_BitGap_Cont = cc->newLabel();

  cc->test(bitWord, bitWord);                              // if (bitWord != 0)
  cc->jnz(L_BitGap_Match);                                 //   goto L_BitGap_Match;

  // Loop unrolled 2x as we could be inside a larger span.
  cc->bind(L_BitGap_Cont);                                 // L_BitGap_Cont:
  cc->add(xOff, pixelsPerBitWord);                         //   xOff += pixelsPerBitWord;
  cc->cmp(bitPtr, bitPtrEnd);                              //   if (bitPtr == bitPtrEnd)
  cc->jz(L_Scanline_Done1);                                //     goto L_Scanline_Done1;

  cc->or_(bitWord, x86::ptr(bitPtr));                      //   bitWord |= bitPtr[0];
  cc->lea(bitPtr, x86::ptr(bitPtr, bwSize));               //   bitPtr += bwSize;
  cc->jnz(L_BitGap_Match);                                 //   if (bitWord != 0) goto L_BitGap_Match;

  cc->add(xOff, pixelsPerBitWord);                         //   xOff += pixelsPerBitWord;
  cc->cmp(bitPtr, bitPtrEnd);                              //   if (bitPtr == bitPtrEnd)
  cc->jz(L_Scanline_Done1);                                //     goto L_Scanline_Done1;

  cc->or_(bitWord, x86::ptr(bitPtr));                      //   bitWord |= bitPtr[0];
  cc->lea(bitPtr, x86::ptr(bitPtr, bwSize));               //   bitPtr += bwSize;
  cc->jz(L_BitGap_Cont);                                   //   if (bitWord == 0) goto L_BitGap_Cont;

  cc->bind(L_BitGap_Match);                                // L_BitGap_Match:
  cc->mov(x86::ptr(bitPtr, -bwSize, bwSize), 0);           //   bitPtr[-1] = 0;
  pc->uCTZ(i.cloneAs(bitWord), bitWord);                   //   i = ctz(bitWord);
  cc->mov(bitWordTmp, -1);                                 //   bitWordTmp = -1; (all ones)
  pc->vextractu16(cMaskAlpha, m[0], 0);                    //   cMaskAlpha = extracti16(m0, 0);

  pc->uShl(bitWordTmp, i);                                 //   bitWordTmp <<= i;
  cc->shl(i, pixelsPerOneBitShift);                        //   i <<= pixelsPerOneBitShift;

  cc->xor_(bitWord, bitWordTmp);                           //   bitWord ^= bitWordTmp;
  cc->add(i, xOff);                                        //   i += xOff;
  cc->sub(i, x0);                                          //   i -= x0;
  cc->add(x0, i);                                          //   x0 += i;
  pc->uAddMulImm(cellPtr, i.cloneAs(cellPtr), 4);          //   cellPtr += i * 4;

  cc->test(cMaskAlpha, cMaskAlpha);                        //   if (cMaskAlpha != 0)
  cc->jnz(L_CLoop_Init);                                   //     goto L_CLoop_Init;

  // Fully-Transparent span where `cMaskAlpha == 0`.
  pc->uAddMulImm(dstPtr, i.cloneAs(dstPtr), dstBpp);       //   dstPtr += i * dstBpp;

  compOpPart()->postfetchN();
  compOpPart()->advanceX(x0, i);
  compOpPart()->prefetchN();

  cc->test(bitWord, bitWord);                              //   if (bitWord != 0)
  cc->jnz(L_BitScan_Match);                                //     goto L_BitScan_Match;
  cc->jmp(L_BitScan_Next);                                 //   goto L_BitScan_Next;

  // --------------------------------------------------------------------------
  // [CLoop]
  // --------------------------------------------------------------------------

  cc->bind(L_CLoop_Init);                                  // L_CLoop_Init:
  if (compOpPart()->shouldOptimizeOpaqueFill()) {
    Label L_CLoop_Msk = cc->newLabel();

    cc->cmp(cMaskAlpha, 256);                              //   if (cMaskAlpha != 256)
    cc->jnz(L_CLoop_Msk);                                  //     goto L_CLoop_Msk

    compOpPart()->cMaskInit(pc->_gpNone);
    if (pixelGranularity >= 4)
      compOpPart()->cMaskGranularLoop(i);
    else
      compOpPart()->cMaskGenericLoop(i);
    compOpPart()->cMaskFini();

    cc->test(bitWord, bitWord);                            //   if (bitWord != 0)
    cc->jnz(L_BitScan_Match);                              //     goto L_BitScan_Match;
    cc->jmp(L_BitScan_Next);                               //   goto L_BitScan_Next;

    cc->bind(L_CLoop_Msk);                                 // L_CLoop_Msk:
  }

  compOpPart()->cMaskInitXmm(m[0].as<x86::Xmm>());
  if (pixelGranularity >= 4)
    compOpPart()->cMaskGranularLoop(i);
  else
    compOpPart()->cMaskGenericLoop(i);
  compOpPart()->cMaskFini();

  cc->test(bitWord, bitWord);                              //   if (bitWord != 0)
  cc->jnz(L_BitScan_Match);                                //     goto L_BitScan_Match;
  cc->jmp(L_BitScan_Next);                                 //   goto L_BitScan_Next;

  // --------------------------------------------------------------------------
  // [VTail - Tail `vMask` loop for pixels near the end of the scanline]
  // --------------------------------------------------------------------------

  if (compOpPart()->maxPixels() >= 4) {
    // Tail loop can handle up to `pixelsPerOneBit` pixels in theory, in practice
    // maximum `pixelsPerOneBit - 1` pixels are processed, because we know that
    // the number of pixels is less than `pixelsPerOneBit`.

    cc->bind(L_VTail_Init);                                // L_VTail_Init:
    pc->uAddMulImm(cellPtr, i, 4);                         //   cellPtr += i * 4;
    pc->vslli128b(m[0], m[0], 6);                          //   m0[7:0] = [__, M3, M2, M1, M0, __, __, __]
    compOpPart()->enterPartialMode();                      //   <CompOpPart::enterPartialMode>

    cc->bind(L_VTail_Cont);                                // L_VTail_Cont:
    pc->vswizli16(m[0], m[0], shuf(3, 3, 3, 3));           //   m0[7:0] = [__, M3, M2, M1, M0, M0, M0, M0]

    compOpPart()->vMaskProc32Xmm1(dPix, PixelARGB::kPC | PixelARGB::kImmutable, m, true);

    pc->xStore32_ARGB(dstPtr, dPix.pc[0]);
    cc->add(dstPtr, dstBpp);                               //   dstPtr += dstBpp;
    pc->vsrli128b(m[0], m[0], 2);                          //   m0[7:0] = [0, m[7:1]]
    compOpPart()->nextPartialPixel();                      //   <CompOpPart::nextPartialPixel>

    dPix.reset();

    cc->sub(i, 1);                                         //   if (--i)
    cc->jnz(L_VTail_Cont);                                 //     goto L_VTail_Cont;

    compOpPart()->exitPartialMode();                       //   <CompOpPart::exitPartialMode>

    // Since this was a tail loop we know that there is nothing to be processed afterwards,
    // because tail loop is only possible at the end of the scanline boundary / clip region.
  }

  // --------------------------------------------------------------------------
  // [Scanline Iterator]
  // --------------------------------------------------------------------------

  // This loop is used to quickly test bitWords in `bitPtr`. In some cases the
  // whole scanline could be empty, so this loop makes sure we won't enter more
  // complicated loops if this happens. It's also used to quickly find the first
  // bit, which is non-zero - in that case it jumps directly to BitMatch section.

  cc->bind(L_Scanline_Done0);                              // L_Scanline_Done0:
  pc->vzeropi(m[1]);                                       //   m1[3:0] = 0;
  pc->vstorei128u(x86::ptr(cellPtr), m[1]);                //   cellPtr[3:0] = 0;

  cc->bind(L_Scanline_Done1);                              // L_Scanline_Done1:
  disadvanceDstPtrAndCellPtr(dstPtr,                       //   dstPtr -= x0 * dstBpp;
                             cellPtr, x0, dstBpp);         //   cellPtr -= x0 * 4;
  cc->sub(y, 1);                                           //   if (--y == 0)
  cc->jz(L_End);                                           //     goto L_End;

  // TODO: [PIPEGEN] Verify whether it's necessary.
  cc->mov(bitPtr, bitPtrEnd);                              //   bitPtr = bitPtrEnd;

  cc->bind(L_Scanline_AdvY);                               // L_Scanline_AdvY:
  cc->add(dstPtr, dstStride);                              //   dstPtr += dstStride;
  cc->add(cellPtr, cellStride);                            //   cellPtr += cellStride;
  compOpPart()->advanceY();                                //   <CompOpPart::AdvanceY>

  cc->bind(L_Scanline_Init);                               // L_Scanline_Init:
  cc->xor_(xOff, xOff);                                    //   xOff = 0;
  cc->mov(bitPtrEnd, bitPtr);                              //   bitPtrEnd = bitPtr;
  cc->xor_(bitWord, bitWord);                              //   bitWord = 0;
  cc->add(bitPtrEnd, bitStride);                           //   bitPtrEnd += bitStride;

  cc->bind(L_Scanline_Cont);                               // L_Scanline_Cont:
  cc->or_(bitWord, x86::ptr(bitPtr));                      //   bitWord |= bitPtr[0];
  cc->lea(bitPtr, x86::ptr(bitPtr, bwSize));               //   bitPtr += bwSize;
  cc->jnz(L_BitScan_Init);                                 //   if (bitWord) goto L_BitScan_Init;

  cc->add(xOff, pixelsPerBitWord);                         //   xOff += pixelsPerBitWord;
  cc->cmp(bitPtr, bitPtrEnd);                              //   if (bitPtr == bitPtrEnd)
  cc->jnz(L_Scanline_Cont);                                //     goto L_Scanline_Cont;

  cc->dec(y);                                              //   if (--y)
  cc->jnz(L_Scanline_AdvY);                                //     goto L_Scanline_AdvY;

  // --------------------------------------------------------------------------
  // [End]
  // --------------------------------------------------------------------------

  cc->bind(L_End);
  compOpPart()->fini();
  _finiGlobalHook();
}

void FillAnalyticPart::accumulateCells(const x86::Vec& acc, const x86::Vec& val) noexcept {
  x86::Vec tmp = cc->newSimilarReg<x86::Vec>(val, "vAccTmp");

  pc->vslli128b(tmp, val, 4);                              //   tmp[3:0]  = [  c2 |  c1 |  c0 |  0  ];
  pc->vaddi32(val, val, tmp);                              //   val[3:0]  = [c3:c2|c2:c1|c1:c0|  c0 ];
  pc->vaddi32(acc, acc, val);                              //   acc[3:0] += val[3:0];

  pc->vslli128b(val, val, 8);                              //   val[3:0]  = [c1:c0|  c0 |  0  |  0  ];
  pc->vaddi32(val, val, acc);                              //   val[3:0] += acc[3:0];
  pc->vswizi32(acc, val, x86::Predicate::shuf(3, 3, 3, 3));
}

void FillAnalyticPart::calcMasksFromCells(const x86::Vec& dst, const x86::Vec& src, const x86::Vec& fillRuleMask, const x86::Vec& globalAlpha, bool unpack) noexcept {
  // This implementation is a bit tricky. In the original AGG and FreeType
  // `A8_SHIFT + 1` is used. However, we don't do that and mask out the last
  // bit through `fillRuleMask`. The reason we do this is that our `globalAlpha`
  // is already preshifted by `7` bits left and we only need to shift the final
  // mask by one bit left after it's been calculated. So instead of shifting it
  // left later we clear the LSB bit now and that's it, we saved one instruction.
  pc->vsrai32(dst, src, BL_PIPE_A8_SHIFT);
  pc->vand(dst, dst, fillRuleMask);

  // We have to make sure that that cleared LSB bit stays zero. Since we only
  // use SUB with even value and abs we are fine. However, that packing would not
  // be safe if there was no "VMINI16", which makes sure we are always safe.
  pc->vsubi32(dst, dst, pc->constAsMem(blCommonTable.i128_0000020000000200));
  pc->vabsi32(dst, dst);

  pc->vpacki32i16(dst, dst, dst);
  pc->vmini16(dst, dst, pc->constAsMem(blCommonTable.i128_0200020002000200));

  // Now we have a vector of 16-bit masks:
  //
  //   [__, __, __, __, M3, M2, M1, M0]
  //
  // After unpacking (if enabled) we would shuffle it into:
  //
  //   [M3, M3, M2, M2, M1, M1, M0, M0]
  if (unpack)
    pc->vunpackli16(dst, dst, dst);

  // Multiply masks by global alpha, this would output masks in [0, 256] range.
  pc->vmulhu16(dst, dst, globalAlpha);
}

void FillAnalyticPart::disadvanceDstPtrAndCellPtr(const x86::Gp& dstPtr, const x86::Gp& cellPtr, const x86::Gp& x, int dstBpp) noexcept {
  x86::Gp xAdv = x.cloneAs(dstPtr);

  if (dstBpp == 1) {
    cc->sub(dstPtr, xAdv);
    cc->shl(xAdv, 2);
    cc->sub(cellPtr, xAdv);
  }
  else if (dstBpp == 4) {
    cc->shl(xAdv, 2);
    cc->sub(dstPtr, xAdv);
    cc->sub(cellPtr, xAdv);
  }
  else {
    x86::Gp dstAdv = cc->newIntPtr("dstAdv");
    pc->uMulImm(dstAdv, xAdv, dstBpp);
    cc->shl(xAdv, 2);
    cc->sub(dstPtr, dstAdv);
    cc->sub(cellPtr, xAdv);
  }
}

} // {BLPipeGen}
