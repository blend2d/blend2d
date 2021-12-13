// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../../runtime_p.h"
#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchpatternpart_p.h"
#include "../../pipeline/jit/fetchpixelptrpart_p.h"
#include "../../pipeline/jit/fetchsolidpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

#define C_MEM(CONST) pc->constAsMem(&blCommonTable.CONST)

namespace BLPipeline {
namespace JIT {

static uint32_t regCountByRGBA32PixelCount(SimdWidth simdWidth, uint32_t n) noexcept {
  uint32_t shift = uint32_t(simdWidth) + 1;
  uint32_t x = (1 << shift) - 1;
  return (n + x) >> shift;
}

static uint32_t regCountByA8PixelCount(SimdWidth simdWidth, uint32_t n) noexcept {
  uint32_t shift = uint32_t(simdWidth) + 3;
  uint32_t x = (1 << shift) - 1;
  return (n + x) >> shift;
}

// BLPipeline::JIT::CompOpPart - Construction & Destruction
// ========================================================

CompOpPart::CompOpPart(PipeCompiler* pc, uint32_t compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept
  : PipePart(pc, PipePartType::kComposite),
    _compOp(compOp),
    _pixelType(dstPart->hasRGB() ? PixelType::kRGBA : PixelType::kAlpha),
    _isInPartialMode(false),
    _hasDa(dstPart->hasAlpha()),
    _hasSa(srcPart->hasAlpha()),
    _solidPre(_pixelType),
    _partialPixel(_pixelType) {

  _mask->reset();

  // Initialize the children of this part.
  _children[kIndexDstPart] = dstPart;
  _children[kIndexSrcPart] = srcPart;
  _childCount = 2;

  SimdWidth maxSimdWidth = SimdWidth::k128;
  switch (pixelType()) {
    case PixelType::kRGBA:
      switch (compOp) {
        case BL_COMP_OP_SRC_OVER: /* maxSimdWidth = SimdWidth::k256; */ break;
        case BL_COMP_OP_SRC_COPY: /* maxSimdWidth = SimdWidth::k256; */ break;
      }
      break;

    case PixelType::kAlpha:
      break;
  }
  _maxSimdWidthSupported = maxSimdWidth;
}

// BLPipeline::JIT::CompOpPart - Prepare
// =====================================

void CompOpPart::preparePart() noexcept {
  bool isSolid = srcPart()->isSolid();
  uint32_t maxPixels = 0;
  uint32_t pixelLimit = 64;

  // Limit the maximum pixel-step to 4 it the style is not solid and the target
  // is not 64-bit. There's not enough registers to process 8 pixels in parallel
  // in 32-bit mode.
  if (blRuntimeIs32Bit() && !isSolid && _pixelType != PixelType::kAlpha)
    pixelLimit = 4;

  // Decrease the maximum pixels to 4 if the source is complex to fetch.
  // In such case fetching and processing more pixels would result in
  // emitting bloated pipelines that are not faster compared to pipelines
  // working with just 4 pixels at a time.
  if (dstPart()->isComplexFetch() || srcPart()->isComplexFetch())
    pixelLimit = 4;

  switch (pixelType()) {
    case PixelType::kRGBA:
      switch (compOp()) {
        case BL_COMP_OP_SRC_OVER    : maxPixels = 8; break;
        case BL_COMP_OP_SRC_COPY    : maxPixels = 8; break;
        case BL_COMP_OP_SRC_IN      : maxPixels = 8; break;
        case BL_COMP_OP_SRC_OUT     : maxPixels = 8; break;
        case BL_COMP_OP_SRC_ATOP    : maxPixels = 8; break;
        case BL_COMP_OP_DST_OVER    : maxPixels = 8; break;
        case BL_COMP_OP_DST_IN      : maxPixels = 8; break;
        case BL_COMP_OP_DST_OUT     : maxPixels = 8; break;
        case BL_COMP_OP_DST_ATOP    : maxPixels = 8; break;
        case BL_COMP_OP_XOR         : maxPixels = 8; break;
        case BL_COMP_OP_CLEAR       : maxPixels = 8; break;
        case BL_COMP_OP_PLUS        : maxPixels = 8; break;
        case BL_COMP_OP_MINUS       : maxPixels = 4; break;
        case BL_COMP_OP_MODULATE    : maxPixels = 8; break;
        case BL_COMP_OP_MULTIPLY    : maxPixels = 8; break;
        case BL_COMP_OP_SCREEN      : maxPixels = 8; break;
        case BL_COMP_OP_OVERLAY     : maxPixels = 4; break;
        case BL_COMP_OP_DARKEN      : maxPixels = 8; break;
        case BL_COMP_OP_LIGHTEN     : maxPixels = 8; break;
        case BL_COMP_OP_COLOR_DODGE : maxPixels = 1; break;
        case BL_COMP_OP_COLOR_BURN  : maxPixels = 1; break;
        case BL_COMP_OP_LINEAR_BURN : maxPixels = 8; break;
        case BL_COMP_OP_LINEAR_LIGHT: maxPixels = 1; break;
        case BL_COMP_OP_PIN_LIGHT   : maxPixels = 4; break;
        case BL_COMP_OP_HARD_LIGHT  : maxPixels = 4; break;
        case BL_COMP_OP_SOFT_LIGHT  : maxPixels = 1; break;
        case BL_COMP_OP_DIFFERENCE  : maxPixels = 8; break;
        case BL_COMP_OP_EXCLUSION   : maxPixels = 8; break;

        default:
          BL_NOT_REACHED();
      }
      break;

    case PixelType::kAlpha:
      maxPixels = 8;
      break;
  }

  // Descrease to N pixels at a time if the fetch part doesn't support more.
  // This is suboptimal, but can happen if the fetch part is not optimized.
  maxPixels = blMin(maxPixels, pixelLimit, srcPart()->maxPixels());

  if (maxPixels > 1)
    maxPixels *= pc->simdMultiplier();

  if (isRGBAType()) {
    if (maxPixels >= 4)
      _minAlignment = 16;
  }

  setMaxPixels(maxPixels);
}

// BLPipeline::JIT::CompOpPart - Init & Fini
// =========================================

void CompOpPart::init(x86::Gp& x, x86::Gp& y, uint32_t pixelGranularity) noexcept {
  _pixelGranularity = uint8_t(pixelGranularity);

  dstPart()->init(x, y, pixelType(), pixelGranularity);
  srcPart()->init(x, y, pixelType(), pixelGranularity);
}

void CompOpPart::fini() noexcept {
  dstPart()->fini();
  srcPart()->fini();

  _pixelGranularity = 0;
}

// BLPipeline::JIT::CompOpPart - Optimization Opportunities
// ========================================================

bool CompOpPart::shouldOptimizeOpaqueFill() const noexcept {
  // Should be always optimized if the source is not solid.
  if (!srcPart()->isSolid())
    return true;

  // Do not optimize if the CompOp is TypeA. This operator doesn't need any
  // special handling as the source pixel is multiplied with mask before it's
  // passed to the compositor.
  if (blTestFlag(compOpFlags(), BLCompOpFlags::kTypeA))
    return false;

  // Modulate operator just needs to multiply source with mask and add (1 - m)
  // to it.
  if (compOp() == BL_COMP_OP_MODULATE)
    return false;

  // We assume that in all other cases there is a benefit of using optimized
  // `cMask` loop for a fully opaque mask.
  return true;
}

bool CompOpPart::shouldJustCopyOpaqueFill() const noexcept {
  if (compOp() != BL_COMP_OP_SRC_COPY)
    return false;

  if (srcPart()->isSolid())
    return true;

  if (srcPart()->isFetchType(FetchType::kPatternAlignedBlit) &&
      srcPart()->format() == dstPart()->format())
    return true;

  return false;
}

// BLPipeline::JIT::CompOpPart - Advance
// =====================================

void CompOpPart::startAtX(x86::Gp& x) noexcept {
  dstPart()->startAtX(x);
  srcPart()->startAtX(x);
}

void CompOpPart::advanceX(x86::Gp& x, x86::Gp& diff) noexcept {
  dstPart()->advanceX(x, diff);
  srcPart()->advanceX(x, diff);
}

void CompOpPart::advanceY() noexcept {
  dstPart()->advanceY();
  srcPart()->advanceY();
}

// BLPipeline::JIT::CompOpPart - Prefetch & Postfetch
// ==================================================

void CompOpPart::prefetch1() noexcept {
  dstPart()->prefetch1();
  srcPart()->prefetch1();
}

void CompOpPart::enterN() noexcept {
  dstPart()->enterN();
  srcPart()->enterN();
}

void CompOpPart::leaveN() noexcept {
  dstPart()->leaveN();
  srcPart()->leaveN();
}

void CompOpPart::prefetchN() noexcept {
  dstPart()->prefetchN();
  srcPart()->prefetchN();
}

void CompOpPart::postfetchN() noexcept {
  dstPart()->postfetchN();
  srcPart()->postfetchN();
}

// BLPipeline::JIT::CompOpPart - Fetch
// ===================================

void CompOpPart::dstFetch(Pixel& p, PixelFlags flags, uint32_t n) noexcept {
  switch (n) {
    case 1: dstPart()->fetch1(p, flags); break;
    case 4: dstPart()->fetch4(p, flags); break;
    case 8: dstPart()->fetch8(p, flags); break;
    /*
    case 16: dstPart()->fetch16(p, flags); break;
    */
  }
}

void CompOpPart::srcFetch(Pixel& p, PixelFlags flags, uint32_t n) noexcept {
  // Pixels must match as we have already preconfigured the CompOpPart.
  BL_ASSERT(p.type() == pixelType());

  if (!p.count())
    p.setCount(n);

  // Composition with a preprocessed solid color.
  if (isUsingSolidPre()) {
    Pixel& s = _solidPre;

    // INJECT:
    {
      ScopedInjector injector(cc, &_cMaskLoopHook);
      pc->xSatisfySolid(s, flags);
    }

    if (p.isRGBA()) {
      if (blTestFlag(flags, PixelFlags::kImmutable)) {
        if (blTestFlag(flags, PixelFlags::kPC)) p.pc.init(s.pc[0]);
        if (blTestFlag(flags, PixelFlags::kUC)) p.uc.init(s.uc[0]);
        if (blTestFlag(flags, PixelFlags::kUA)) p.ua.init(s.ua[0]);
        if (blTestFlag(flags, PixelFlags::kUIA)) p.uia.init(s.uia[0]);
      }
      else {
        switch (n) {
          case 1:
            if (blTestFlag(flags, PixelFlags::kPC)) { p.pc.init(cc->newXmm("pre.pc")); pc->v_mov(p.pc[0], s.pc[0]); }
            if (blTestFlag(flags, PixelFlags::kUC)) { p.uc.init(cc->newXmm("pre.uc")); pc->v_mov(p.uc[0], s.uc[0]); }
            if (blTestFlag(flags, PixelFlags::kUA)) { p.ua.init(cc->newXmm("pre.ua")); pc->v_mov(p.ua[0], s.ua[0]); }
            if (blTestFlag(flags, PixelFlags::kUIA)) { p.uia.init(cc->newXmm("pre.uia")); pc->v_mov(p.uia[0], s.uia[0]); }
            break;

          case 4:
            if (blTestFlag(flags, PixelFlags::kPC)) {
              pc->newVecArray(p.pc, 1, "pre.pc");
              pc->v_mov(p.pc[0], s.pc[0]);
            }

            if (blTestFlag(flags, PixelFlags::kUC)) {
              pc->newVecArray(p.uc, 2, "pre.uc");
              pc->v_mov(p.uc[0], s.uc[0]);
              pc->v_mov(p.uc[1], s.uc[0]);
            }

            if (blTestFlag(flags, PixelFlags::kUA)) {
              pc->newVecArray(p.ua, 2, "pre.ua");
              pc->v_mov(p.ua[0], s.ua[0]);
              pc->v_mov(p.ua[1], s.ua[0]);
            }

            if (blTestFlag(flags, PixelFlags::kUIA)) {
              pc->newVecArray(p.uia, 2, "pre.uia");
              pc->v_mov(p.uia[0], s.uia[0]);
              pc->v_mov(p.uia[1], s.uia[0]);
            }
            break;

          case 8:
            if (blTestFlag(flags, PixelFlags::kPC)) {
              pc->newVecArray(p.pc, 2, "pre.pc");
              pc->v_mov(p.pc[0], s.pc[0]);
              pc->v_mov(p.pc[1], s.pc[0]);
            }

            if (blTestFlag(flags, PixelFlags::kUC)) {
              pc->newVecArray(p.uc, 4, "pre.uc");
              pc->v_mov(p.uc[0], s.uc[0]);
              pc->v_mov(p.uc[1], s.uc[0]);
              pc->v_mov(p.uc[2], s.uc[0]);
              pc->v_mov(p.uc[3], s.uc[0]);
            }

            if (blTestFlag(flags, PixelFlags::kUA)) {
              pc->newVecArray(p.ua, 4, "pre.ua");
              pc->v_mov(p.ua[0], s.ua[0]);
              pc->v_mov(p.ua[1], s.ua[0]);
              pc->v_mov(p.ua[2], s.ua[0]);
              pc->v_mov(p.ua[3], s.ua[0]);
            }

            if (blTestFlag(flags, PixelFlags::kUIA)) {
              pc->newVecArray(p.uia, 4, "pre.uia");
              pc->v_mov(p.uia[0], s.uia[0]);
              pc->v_mov(p.uia[1], s.uia[0]);
              pc->v_mov(p.uia[2], s.uia[0]);
              pc->v_mov(p.uia[3], s.uia[0]);
            }
            break;
        }
      }
    }
    else if (p.isAlpha()) {
      // TODO: A8 pipepine.
      BL_ASSERT(false);
    }

    return;
  }

  // Partial mode is designed to fetch pixels on the right side of the
  // border one by one, so it's an error if the pipeline requests more
  // than 1 pixel at a time.
  if (isInPartialMode()) {
    BL_ASSERT(n == 1);

    if (p.isRGBA()) {
      if (!blTestFlag(flags, PixelFlags::kImmutable)) {
        if (blTestFlag(flags, PixelFlags::kUC)) {
          pc->newVecArray(p.uc, 1, "uc");
          pc->vmovu8u16(p.uc[0], _partialPixel.pc[0]);
        }
        else {
          pc->newVecArray(p.pc, 1, "pc");
          pc->v_mov(p.pc[0], _partialPixel.pc[0]);
        }
      }
      else {
        p.pc.init(_partialPixel.pc[0]);
      }
    }
    else if (p.isAlpha()) {
      p.sa = cc->newUInt32("sa");
      pc->v_extract_u16(p.sa, _partialPixel.ua[0], 0);
    }

    pc->xSatisfyPixel(p, flags);
    return;
  }

  switch (n) {
    case 1: srcPart()->fetch1(p, flags); break;
    case 4: srcPart()->fetch4(p, flags); break;
    case 8: srcPart()->fetch8(p, flags); break;
  }
}

// BLPipeline::JIT::CompOpPart - PartialFetch
// ==========================================

void CompOpPart::enterPartialMode(PixelFlags partialFlags) noexcept {
  // Doesn't apply to solid fills.
  if (isUsingSolidPre())
    return;

  // TODO: [PIPEGEN] We only support partial fetch of 4 pixels at the moment.
  BL_ASSERT(!isInPartialMode());
  BL_ASSERT(pixelGranularity() == 4);

  switch (pixelType()) {
    case PixelType::kRGBA: {
      srcFetch(_partialPixel, PixelFlags::kPC | partialFlags, pixelGranularity());
      break;
    }

    case PixelType::kAlpha: {
      srcFetch(_partialPixel, PixelFlags::kUA | partialFlags, pixelGranularity());
      break;
    }
  }

  _isInPartialMode = true;
}

void CompOpPart::exitPartialMode() noexcept {
  // Doesn't apply to solid fills.
  if (isUsingSolidPre())
    return;

  BL_ASSERT(isInPartialMode());

  _isInPartialMode = false;
  _partialPixel.resetAllExceptType();
}

void CompOpPart::nextPartialPixel() noexcept {
  if (!isInPartialMode())
    return;

  switch (pixelType()) {
    case PixelType::kRGBA: {
      const x86::Vec& pix = _partialPixel.pc[0];
      pc->v_srlb_i128(pix, pix, 4);
      break;
    }

    case PixelType::kAlpha: {
      const x86::Vec& pix = _partialPixel.ua[0];
      pc->v_srlb_i128(pix, pix, 2);
      break;
    }
  }
}

// BLPipeline::JIT::CompOpPart - CMask - Init & Fini
// =================================================

void CompOpPart::cMaskInit(const x86::Mem& mem) noexcept {
  switch (pixelType()) {
    case PixelType::kRGBA: {
      x86::Vec mVec = cc->newXmm("msk");
      x86::Mem m(mem);

      m.setSize(4);
      pc->v_broadcast_u16(mVec, m);
      cMaskInitRGBA32(mVec);
      break;
    }

    case PixelType::kAlpha: {
      x86::Gp mGp = cc->newUInt32("msk");
      pc->load8(mGp, mem);
      cMaskInitA8(mGp, x86::Vec());
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::cMaskInit(const x86::Gp& sm_, const x86::Vec& vm_) noexcept {
  x86::Gp sm(sm_);
  x86::Vec vm(vm_);

  switch (pixelType()) {
    case PixelType::kRGBA: {
      if (!vm.isValid() && sm.isValid()) {
        vm = cc->newXmm("c.vm");
        pc->v_broadcast_u16(vm, sm);
      }

      cMaskInitRGBA32(vm);
      break;
    }

    case PixelType::kAlpha: {
      cMaskInitA8(sm, vm);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::cMaskInitOpaque() noexcept {
  switch (pixelType()) {
    case PixelType::kRGBA: {
      cMaskInitRGBA32(x86::Vec());
      break;
    }

    case PixelType::kAlpha: {
      cMaskInitA8(x86::Gp(), x86::Vec());
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::cMaskFini() noexcept {
  switch (pixelType()) {
    case PixelType::kAlpha:
      cMaskFiniA8();
      break;

    case PixelType::kRGBA:
      cMaskFiniRGBA32();
      break;

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::_cMaskLoopInit(CMaskLoopType loopType) noexcept {
  // Make sure `_cMaskLoopInit()` and `_cMaskLoopFini()` are used as a pair.
  BL_ASSERT(_cMaskLoopType == CMaskLoopType::kNone);
  BL_ASSERT(_cMaskLoopHook == nullptr);

  _cMaskLoopType = loopType;
  _cMaskLoopHook = cc->cursor();
}

void CompOpPart::_cMaskLoopFini() noexcept {
  // Make sure `_cMaskLoopInit()` and `_cMaskLoopFini()` are used as a pair.
  BL_ASSERT(_cMaskLoopType != CMaskLoopType::kNone);
  BL_ASSERT(_cMaskLoopHook != nullptr);

  _cMaskLoopType = CMaskLoopType::kNone;
  _cMaskLoopHook = nullptr;
}

// BLPipeline::JIT::CompOpPart - CMask - Generic Loop
// ==================================================

void CompOpPart::cMaskGenericLoop(x86::Gp& i) noexcept {
  if (isLoopOpaque() && shouldJustCopyOpaqueFill()) {
    cMaskMemcpyOrMemsetLoop(i);
    return;
  }

  cMaskGenericLoopVec(i);
}

void CompOpPart::cMaskGenericLoopVec(x86::Gp& i) noexcept {
  x86::Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();

  // 1 pixel at a time.
  if (maxPixels() == 1) {
    Label L_Loop = cc->newLabel();

    prefetch1();

    cc->bind(L_Loop);
    cMaskCompositeAndStore(x86::ptr(dPtr), 1, Alignment(1));
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp()), i, 1);
    cc->jnz(L_Loop);

    return;
  }

  BL_ASSERT(minAlignment() >= 1);
  uint32_t alignmentMask = minAlignment().value() - 1;

  // 4+ pixels at a time [no alignment].
  if (maxPixels() == 4 && minAlignment() == 1) {
    Label L_Loop1 = cc->newLabel();
    Label L_Loop4 = cc->newLabel();
    Label L_Skip4 = cc->newLabel();
    Label L_Exit = cc->newLabel();

    cc->sub(i, 4);
    cc->jc(L_Skip4);

    enterN();
    prefetchN();

    cc->bind(L_Loop4);
    cMaskCompositeAndStore(x86::ptr(dPtr), 4);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp() * 4), i, 4);
    cc->jnc(L_Loop4);

    postfetchN();
    leaveN();

    cc->bind(L_Skip4);
    prefetch1();
    cc->add(i, 4);
    cc->jz(L_Exit);

    cc->bind(L_Loop1);
    cMaskCompositeAndStore(x86::ptr(dPtr), 1);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp()), i, 1);
    cc->jnz(L_Loop1);

    cc->bind(L_Exit);
    return;
  }

  // 4+ pixels at a time [with alignment].
  if (maxPixels() == 4 && minAlignment() != 1) {
    Label L_Loop1     = cc->newLabel();
    Label L_Loop4     = cc->newLabel();
    Label L_Aligned   = cc->newLabel();
    Label L_Exit      = cc->newLabel();

    pc->uTest(dPtr, alignmentMask);
    cc->jz(L_Aligned);

    prefetch1();

    cc->bind(L_Loop1);
    cMaskCompositeAndStore(x86::ptr(dPtr), 1);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp()), i, 1);
    cc->jz(L_Exit);

    pc->uTest(dPtr, alignmentMask);
    cc->jnz(L_Loop1);

    cc->bind(L_Aligned);
    cc->cmp(i, 4);
    cc->jb(L_Loop1);

    cc->sub(i, 4);
    dstPart()->as<FetchPixelPtrPart>()->setPtrAlignment(16);

    enterN();
    prefetchN();

    cc->bind(L_Loop4);
    cMaskCompositeAndStore(x86::ptr(dPtr), 4, Alignment(16));
    cc->add(dPtr, int(dstPart()->bpp() * 4));
    cc->sub(i, 4);
    cc->jnc(L_Loop4);

    postfetchN();
    leaveN();
    dstPart()->as<FetchPixelPtrPart>()->setPtrAlignment(0);

    prefetch1();

    cc->add(i, 4);
    cc->jnz(L_Loop1);

    cc->bind(L_Exit);
    return;
  }

  // 8+ pixels at a time [no alignment].
  if (maxPixels() == 8 && minAlignment() == 1) {
    Label L_Loop1 = cc->newLabel();
    Label L_Loop4 = cc->newLabel();
    Label L_Loop8 = cc->newLabel();
    Label L_Skip4 = cc->newLabel();
    Label L_Skip8 = cc->newLabel();
    Label L_Init1 = cc->newLabel();
    Label L_Exit = cc->newLabel();

    cc->sub(i, 4);
    cc->jc(L_Skip4);

    enterN();
    prefetchN();

    cc->sub(i, 4);
    cc->jc(L_Skip8);

    cc->bind(L_Loop8);
    cMaskCompositeAndStore(x86::ptr(dPtr), 8);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp() * 8), i, 8);
    cc->jnc(L_Loop8);

    cc->bind(L_Skip8);
    cc->add(i, 4);
    cc->jnc(L_Init1);

    cc->bind(L_Loop4);
    cMaskCompositeAndStore(x86::ptr(dPtr), 4);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp() * 4), i, 4);
    cc->jnc(L_Loop4);

    cc->bind(L_Init1);
    postfetchN();
    leaveN();

    cc->bind(L_Skip4);
    prefetch1();
    cc->add(i, 4);
    cc->jz(L_Exit);

    cc->bind(L_Loop1);
    cMaskCompositeAndStore(x86::ptr(dPtr), 1);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp()), i, 1);
    cc->jnz(L_Loop1);

    cc->bind(L_Exit);
    return;
  }

  // 8+ pixels at a time [with alignment].
  if (maxPixels() == 8 && minAlignment() != 1) {
    Label L_Loop1   = cc->newLabel();
    Label L_Loop8   = cc->newLabel();
    Label L_Skip8   = cc->newLabel();
    Label L_Skip4   = cc->newLabel();
    Label L_Aligned = cc->newLabel();
    Label L_Exit    = cc->newLabel();

    cc->test(dPtr.r8(), alignmentMask);
    cc->jz(L_Aligned);

    prefetch1();

    cc->bind(L_Loop1);
    cMaskCompositeAndStore(x86::ptr(dPtr), 1);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp()), i, 1);
    cc->jz(L_Exit);

    cc->test(dPtr.r8(), alignmentMask);
    cc->jnz(L_Loop1);

    cc->bind(L_Aligned);
    cc->cmp(i, 4);
    cc->jb(L_Loop1);

    dstPart()->as<FetchPixelPtrPart>()->setPtrAlignment(16);
    enterN();
    prefetchN();

    cc->sub(i, 8);
    cc->jc(L_Skip8);

    cc->bind(L_Loop8);
    cMaskCompositeAndStore(x86::ptr(dPtr), 8, minAlignment());
    cc->add(dPtr, int(dstPart()->bpp() * 8));
    cc->sub(i, 8);
    cc->jnc(L_Loop8);

    cc->bind(L_Skip8);
    cc->add(i, 4);
    cc->jnc(L_Skip4);

    cMaskCompositeAndStore(x86::ptr(dPtr), 4, minAlignment());
    cc->add(dPtr, int(dstPart()->bpp() * 4));
    cc->sub(i, 4);
    cc->bind(L_Skip4);

    postfetchN();
    leaveN();
    dstPart()->as<FetchPixelPtrPart>()->setPtrAlignment(0);

    prefetch1();

    cc->add(i, 4);
    cc->jnz(L_Loop1);

    cc->bind(L_Exit);
    return;
  }

  // 16+ pixels at a time.
  if (maxPixels() == 16) {
    Label L_Loop16 = cc->newLabel();
    Label L_Skip16 = cc->newLabel();
    Label L_Exit   = cc->newLabel();

    enterN();
    prefetchN();

    cc->sub(i, 16);
    cc->jc(L_Skip16);

    cc->bind(L_Loop16);
    cMaskCompositeAndStore(x86::ptr(dPtr), 16, Alignment(1));
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp() * 16), i, 16);
    cc->jnc(L_Loop16);

    cc->bind(L_Skip16);
    cc->add(i, 16);
    cc->jz(L_Exit);

    cMaskCompositeAndStore(x86::ptr(dPtr), 16, Alignment(1));
    // pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp() * 16), i, 16);


    cc->bind(L_Exit);

    postfetchN();
    leaveN();

    return;
  }

  BL_NOT_REACHED();
}

// BLPipeline::JIT::CompOpPart - CMask - Granular Loop
// ===================================================

void CompOpPart::cMaskGranularLoop(x86::Gp& i) noexcept {
  if (isLoopOpaque() && shouldJustCopyOpaqueFill()) {
    cMaskMemcpyOrMemsetLoop(i);
    return;
  }

  cMaskGranularLoopXmm(i);
}

void CompOpPart::cMaskGranularLoopXmm(x86::Gp& i) noexcept {
  BL_ASSERT(pixelGranularity() == 4);

  x86::Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();
  if (pixelGranularity() == 4) {
    // 1 pixel at a time.
    if (maxPixels() == 1) {
      Label L_Loop = cc->newLabel();
      Label L_Step = cc->newLabel();

      cc->bind(L_Loop);
      enterPartialMode();

      cc->bind(L_Step);
      cMaskCompositeAndStore(x86::ptr(dPtr), 1);
      cc->sub(i, 1);
      cc->add(dPtr, int(dstPart()->bpp()));
      nextPartialPixel();

      cc->test(i, 0x3);
      cc->jnz(L_Step);

      exitPartialMode();

      cc->test(i, i);
      cc->jnz(L_Loop);

      return;
    }

    // 4+ pixels at a time.
    if (maxPixels() == 4) {
      Label L_Loop = cc->newLabel();

      cc->bind(L_Loop);
      cMaskCompositeAndStore(x86::ptr(dPtr), 4);
      cc->add(dPtr, int(dstPart()->bpp() * 4));
      cc->sub(i, 4);
      cc->jnz(L_Loop);

      return;
    }

    // 8+ pixels at a time.
    if (maxPixels() == 8) {
      Label L_Loop = cc->newLabel();
      Label L_Skip = cc->newLabel();
      Label L_End = cc->newLabel();

      cc->sub(i, 8);
      cc->jc(L_Skip);

      cc->bind(L_Loop);
      cMaskCompositeAndStore(x86::ptr(dPtr), 8);
      cc->add(dPtr, int(dstPart()->bpp() * 8));
      cc->sub(i, 8);
      cc->jnc(L_Loop);

      cc->bind(L_Skip);
      cc->add(i, 8);
      cc->jz(L_End);

      // 4 remaining pixels.
      cMaskCompositeAndStore(x86::ptr(dPtr), 4);
      cc->add(dPtr, int(dstPart()->bpp() * 4));

      cc->bind(L_End);
      return;
    }
  }

  BL_NOT_REACHED();
}

// BLPipeline::JIT::CompOpPart - CMask - MemCpy & MemSet Loop
// ==========================================================

void CompOpPart::cMaskMemcpyOrMemsetLoop(x86::Gp& i) noexcept {
  BL_ASSERT(shouldJustCopyOpaqueFill());
  x86::Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();

  if (srcPart()->isSolid()) {
    // Optimized solid opaque fill -> MemSet.
    BL_ASSERT(_solidOpt.px.isValid());
    pc->xInlinePixelFillLoop(dPtr, _solidOpt.px, i, 64, dstPart()->bpp(), pixelGranularity());
  }
  else if (srcPart()->isFetchType(FetchType::kPatternAlignedBlit)) {
    // Optimized solid opaque blit -> MemCopy.
    pc->xInlinePixelCopyLoop(dPtr, srcPart()->as<FetchSimplePatternPart>()->f->srcp1, i, 64, dstPart()->bpp(), pixelGranularity(), dstPart()->format());
  }
  else {
    BL_NOT_REACHED();
  }
}

// BLPipeline::JIT::CompOpPart - CMask - Composition Helpers
// =========================================================

void CompOpPart::cMaskCompositeAndStore(const x86::Mem& dPtr_, uint32_t n, Alignment alignment) noexcept {
  PixelPtrLoadStoreMask ptrMask;
  cMaskCompositeAndStore(dPtr_, n, alignment, ptrMask);
}

void CompOpPart::cMaskCompositeAndStore(const x86::Mem& dPtr_, uint32_t n, Alignment alignment, const PixelPtrLoadStoreMask& ptrMask) noexcept {
  blUnused(ptrMask);

  Pixel dPix(pixelType());
  x86::Mem dPtr(dPtr_);

  switch (pixelType()) {
    case PixelType::kRGBA: {
      switch (n) {
        case 1:
          cMaskProcRGBA32Xmm(dPix, 1, PixelFlags::kPC | PixelFlags::kImmutable);
          pc->v_store_i32(dPtr, dPix.pc[0]);
          break;

        case 4:
          cMaskProcRGBA32Xmm(dPix, 4, PixelFlags::kPC | PixelFlags::kImmutable);
          pc->v_storex_i128(dPtr, dPix.pc[0], alignment);
          break;

        case 8:
          cMaskProcRGBA32Xmm(dPix, 8, PixelFlags::kPC | PixelFlags::kImmutable);

          if (dPix.pc[0].isYmm()) {
            pc->v_storex_i256(dPtr, dPix.pc[0], alignment);
          }
          else {
            pc->v_storex_i128(dPtr, dPix.pc[0], alignment);
            dPtr.addOffset(16);
            pc->v_storex_i128(dPtr, dPix.pc[dPix.pc.size() > 1 ? 1 : 0], alignment);
          }
          break;

        case 16:
          cMaskProcRGBA32Xmm(dPix, 16, PixelFlags::kPC | PixelFlags::kImmutable);

          BL_ASSERT(dPix.pc[0].isYmm());
          pc->v_storex_i256(dPtr, dPix.pc[0], alignment);
          dPtr.addOffset(32);
          pc->v_storex_i256(dPtr, dPix.pc[dPix.pc.size() > 1 ? 1 : 0], alignment);
          break;

        default:
          BL_NOT_REACHED();
      }
      break;
    }

    case PixelType::kAlpha: {
      switch (n) {
        case 1:
          cMaskProcA8Gp(dPix, PixelFlags::kSA | PixelFlags::kImmutable);
          pc->store8(dPtr, dPix.sa);
          break;

        case 4:
          cMaskProcA8Xmm(dPix, 4, PixelFlags::kPA | PixelFlags::kImmutable);
          pc->v_store_i32(dPtr, dPix.pa[0]);
          break;

        case 8:
          cMaskProcA8Xmm(dPix, 8, PixelFlags::kPA | PixelFlags::kImmutable);
          pc->v_store_i64(dPtr, dPix.pa[0]);
          break;

        case 16:
          cMaskProcA8Xmm(dPix, 16, PixelFlags::kPA | PixelFlags::kImmutable);
          pc->v_storex_i128(dPtr, dPix.pa[0], alignment);
          break;

        default:
          BL_NOT_REACHED();
      }
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// BLPipeline::JIT::CompOpPart - VMask - Composition Helpers
// =========================================================

void CompOpPart::vMaskProc(Pixel& out, PixelFlags flags, x86::Gp& msk, bool mImmutable) noexcept {
  switch (pixelType()) {
    case PixelType::kRGBA: {
      x86::Vec vm = cc->newXmm("c.vm");
      pc->s_mov_i32(vm, msk);
      pc->v_swizzle_lo_i16(vm, vm, x86::shuffleImm(0, 0, 0, 0));

      VecArray vm_(vm);
      vMaskProcRGBA32Xmm(out, 1, flags, vm_, false);
      break;
    }

    case PixelType::kAlpha: {
      vMaskProcA8Gp(out, flags, msk, mImmutable);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// BLPipeline::JIT::CompOpPart - CMask - Init & Fini - A8
// ======================================================

void CompOpPart::cMaskInitA8(const x86::Gp& sm_, const x86::Vec& vm_) noexcept {
  x86::Gp sm(sm_);
  x86::Vec vm(vm_);

  bool hasMask = sm.isValid() || vm.isValid();
  if (hasMask) {
    // SM must be 32-bit, so make it 32-bit if it's 64-bit for any reason.
    if (sm.isValid())
      sm = sm.r32();

    if (vm.isValid() && !sm.isValid()) {
      sm = cc->newUInt32("sm");
      pc->v_extract_u16(vm, sm, 0);
    }

    _mask->sm = sm;
    _mask->vm = vm;
  }

  if (srcPart()->isSolid()) {
    Pixel& s = srcPart()->as<FetchSolidPart>()->_pixel;
    SolidPixel& o = _solidOpt;
    bool convertToVec = true;

    // CMaskInit - A8 - Solid - SrcCopy
    // --------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (!hasMask) {
        // Xa = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);
        o.sa = s.sa;

        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPA);
          o.px = s.pa[0];
        }

        convertToVec = false;
      }
      else {
        // Xa = (Sa * m) + 0.5 <Rounding>
        // Ya = (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = cc->newUInt32("p.sx");
        o.sy = sm;

        pc->uMul(o.sx, s.sa, o.sy);
        pc->uAdd(o.sx, o.sx, imm(0x80));
        pc->uInv8(o.sy, o.sy);
      }
    }

    // CMaskInit - A8 - Solid - SrcOver
    // --------------------------------

    else if (compOp() == BL_COMP_OP_SRC_OVER) {
      if (!hasMask) {
        // Xa = Sa * 1 + 0.5 <Rounding>
        // Ya = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = cc->newUInt32("p.sx");
        o.sy = sm;

        pc->uMov(o.sx, s.sa);
        cc->shl(o.sx, 8);
        pc->uSub(o.sx, o.sx, s.sa);
        pc->uInv8(o.sy, o.sy);
      }
      else {
        // Xa = Sa * m + 0.5 <Rounding>
        // Ya = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = cc->newUInt32("p.sx");
        o.sy = sm;

        pc->uMul(o.sy, sm, s.sa);
        pc->uDiv255(o.sy, o.sy);

        pc->uShl(o.sx, o.sy, imm(8));
        pc->uSub(o.sx, o.sx, o.sy);
        pc->uAdd(o.sx, o.sx, imm(0x80));
        pc->uInv8(o.sy, o.sy);
      }
    }

    // CMaskInit - A8 - Solid - SrcIn
    // ------------------------------

    else if (compOp() == BL_COMP_OP_SRC_IN) {
      if (!hasMask) {
        // Xa = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = s.sa;
        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUA);
          o.ux = s.ua[0];
        }
      }
      else {
        // Xa = Sa * m + (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = cc->newUInt32("o.sx");
        pc->uMul(o.sx, s.sa, sm);
        pc->uDiv255(o.sx, o.sx);
        pc->uInv8(sm, sm);
        pc->uAdd(o.sx, o.sx, sm);
      }
    }

    // CMaskInit - A8 - Solid - SrcOut
    // -------------------------------

    else if (compOp() == BL_COMP_OP_SRC_OUT) {
      if (!hasMask) {
        // Xa = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = s.sa;
        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUA);
          o.ux = s.ua[0];
        }
      }
      else {
        // Xa = Sa * m
        // Ya = 1  - m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = cc->newUInt32("o.sx");
        o.sy = sm;

        pc->uMul(o.sx, s.sa, o.sy);
        pc->uDiv255(o.sx, o.sx);
        pc->uInv8(o.sy, o.sy);
      }
    }

    // CMaskInit - A8 - Solid - DstOut
    // -------------------------------

    else if (compOp() == BL_COMP_OP_DST_OUT) {
      if (!hasMask) {
        // Xa = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = cc->newUInt32("o.sx");
        pc->uInv8(o.sx, s.sa);

        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUIA);
          o.ux = s.uia[0];
        }
      }
      else {
        // Xa = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = sm;
        pc->uMul(o.sx, sm, s.sa);
        pc->uDiv255(o.sx, o.sx);
        pc->uInv8(o.sx, o.sx);
      }
    }

    // CMaskInit - A8 - Solid - Xor
    // ----------------------------

    else if (compOp() == BL_COMP_OP_XOR) {
      if (!hasMask) {
        // Xa = Sa
        // Ya = 1 - Xa (SIMD only)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);
        o.sx = s.sa;

        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUA | PixelFlags::kUIA);

          o.ux = s.ua[0];
          o.uy = s.uia[0];
        }
      }
      else {
        // Xa = Sa * m
        // Ya = 1 - Xa (SIMD only)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = cc->newUInt32("o.sx");
        pc->uMul(o.sx, sm, s.sa);
        pc->uDiv255(o.sx, o.sx);

        if (maxPixels() > 1) {
          o.ux = cc->newXmm("o.ux");
          o.uy = cc->newXmm("o.uy");
          pc->v_broadcast_u16(o.ux, o.sx);
          pc->v_inv255_u16(o.uy, o.ux);
        }
      }
    }

    // CMaskInit - A8 - Solid - Plus
    // -----------------------------

    else if (compOp() == BL_COMP_OP_PLUS) {
      if (!hasMask) {
        // Xa = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA | PixelFlags::kPA);
        o.sa = s.sa;
        o.px = s.pa[0];
        convertToVec = false;
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);
        o.sx = sm;
        pc->uMul(o.sx, o.sx, s.sa);
        pc->uDiv255(o.sx, o.sx);

        if (maxPixels() > 1) {
          o.px = cc->newXmm("o.px");
          pc->uMul(o.sx, o.sx, 0x01010101);
          pc->v_broadcast_u32(o.px, o.sx);
          pc->uShr(o.sx, o.sx, imm(24));
        }

        convertToVec = false;
      }
    }

    // CMaskInit - A8 - Solid - Extras
    // -------------------------------

    if (convertToVec && maxPixels() > 1) {
      if (o.sx.isValid() && !o.ux.isValid()) {
        o.ux = cc->newXmm("p.ux");
        pc->v_broadcast_u16(o.ux, o.sx);
      }

      if (o.sy.isValid() && !o.uy.isValid()) {
        o.uy = cc->newXmm("p.uy");
        pc->v_broadcast_u16(o.uy, o.sy);
      }
    }
  }
  else {
    if (sm.isValid() && !vm.isValid() && maxPixels() > 1) {
      vm = cc->newXmm("vm");
      pc->v_broadcast_u16(vm, sm);
      _mask->vm = vm;
    }

    /*
    // CMaskInit - A8 - NonSolid - SrcCopy
    // -----------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (hasMask) {
        x86::Xmm vn = cc->newXmm("vn");
        pc->v_inv255_u16(vn, m);
        _mask->vec.vn = vn;
      }
    }
    */
  }

  _cMaskLoopInit(hasMask ? CMaskLoopType::kVariant : CMaskLoopType::kOpaque);
}

void CompOpPart::cMaskFiniA8() noexcept {
  if (srcPart()->isSolid()) {
    _solidOpt.reset();
    _solidPre.reset();
  }
  else {
    // TODO: [PIPEGEN] ???
  }

  _mask->reset();
  _cMaskLoopFini();
}

// BLPipeline::JIT::CompOpPart - CMask - Proc - A8
// ===============================================

void CompOpPart::cMaskProcA8Gp(Pixel& out, PixelFlags flags) noexcept {
  out.setCount(1);

  bool hasMask = isLoopCMask();

  if (srcPart()->isSolid()) {
    Pixel d(pixelType());
    SolidPixel& o = _solidOpt;

    x86::Gp& da = d.sa;
    x86::Gp sx = cc->newUInt32("sx");

    // CMaskProc - A8 - SrcCopy
    // ------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (!hasMask) {
        // Da' = Xa
        out.sa = o.sa;
        out.makeImmutable();
      }
      else {
        // Da' = Xa  + Da .(1 - m)
        dstFetch(d, PixelFlags::kSA, 1);

        pc->uMul(da, da, o.sy),
        pc->uAdd(da, da, o.sx);
        pc->uMul257hu16(da, da);

        out.sa = da;
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - A8 - SrcOver
    // ------------------------

    if (compOp() == BL_COMP_OP_SRC_OVER) {
      // Da' = Xa + Da .Ya
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(da, da, o.sy);
      pc->uAdd(da, da, o.sx);
      pc->uMul257hu16(da, da);

      out.sa = da;

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - A8 - SrcIn & DstOut
    // -------------------------------

    if (compOp() == BL_COMP_OP_SRC_IN || compOp() == BL_COMP_OP_DST_OUT) {
      // Da' = Xa.Da
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(da, da, o.sx);
      pc->uDiv255(da, da);
      out.sa = da;

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - A8 - SrcOut
    // -----------------------

    if (compOp() == BL_COMP_OP_SRC_OUT) {
      if (!hasMask) {
        // Da' = Xa.(1 - Da)
        dstFetch(d, PixelFlags::kSA, 1);

        pc->uInv8(da, da);
        pc->uMul(da, da, o.sx);
        pc->uDiv255(da, da);
        out.sa = da;
      }
      else {
        // Da' = Xa.(1 - Da) + Da.Ya
        dstFetch(d, PixelFlags::kSA, 1);

        pc->uInv8(sx, da);
        pc->uMul(sx, sx, o.sx);
        pc->uMul(da, da, o.sy);
        pc->uAdd(da, da, sx);
        pc->uDiv255(da, da);
        out.sa = da;
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - A8 - Xor
    // --------------------

    if (compOp() == BL_COMP_OP_XOR) {
      // Da' = Xa.(1 - Da) + Da.Ya
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(sx, da, o.sy);
      pc->uInv8(da, da);
      pc->uMul(da, da, o.sx);
      pc->uAdd(da, da, sx);
      pc->uDiv255(da, da);
      out.sa = da;

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - A8 - Plus
    // ---------------------

    if (compOp() == BL_COMP_OP_PLUS) {
      // Da' = Clamp(Da + Xa)
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uAddsU8(da, da, o.sx);
      out.sa = da;

      pc->xSatisfyPixel(out, flags);
      return;
    }
  }

  vMaskProcA8Gp(out, flags, _mask->sm, true);
}

void CompOpPart::cMaskProcA8Xmm(Pixel& out, uint32_t n, PixelFlags flags) noexcept {
  out.setCount(n);

  bool hasMask = isLoopCMask();

  if (srcPart()->isSolid()) {
    Pixel d(pixelType());
    SolidPixel& o = _solidOpt;

    uint32_t kFullN = (n + 7) / 8;

    VecArray& da = d.ua;
    VecArray xa;
    pc->newVecArray(xa, kFullN, "x");

    // CMaskProc - A8 - SrcCopy
    // ------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (!hasMask) {
        // Da' = Xa
        out.pa.init(o.px);
        out.makeImmutable();
      }
      else {
        // Da' = Xa + Da .(1 - m)
        dstFetch(d, PixelFlags::kUA, n);

        pc->v_mul_i16(da, da, o.uy),
        pc->v_add_i16(da, da, o.ux);
        pc->v_mulh257_u16(da, da);

        out.ua.init(da);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - A8 - SrcOver
    // ------------------------

    if (compOp() == BL_COMP_OP_SRC_OVER) {
      // Da' = Xa + Da.Ya
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_mul_i16(da, da, o.uy);
      pc->v_add_i16(da, da, o.ux);
      pc->v_mulh257_u16(da, da);

      out.ua.init(da);

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - A8 - SrcIn & DstOut
    // -------------------------------

    if (compOp() == BL_COMP_OP_SRC_IN || compOp() == BL_COMP_OP_DST_OUT) {
      // Da' = Xa.Da
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_mul_u16(da, da, o.ux);
      pc->v_div255_u16(da);
      out.ua.init(da);

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - A8 - SrcOut
    // -----------------------

    if (compOp() == BL_COMP_OP_SRC_OUT) {
      if (!hasMask) {
        // Da' = Xa.(1 - Da)
        dstFetch(d, PixelFlags::kUA, n);

        pc->v_inv255_u16(da, da);
        pc->v_mul_u16(da, da, o.ux);
        pc->v_div255_u16(da);
        out.ua.init(da);
      }
      else {
        // Da' = Xa.(1 - Da) + Da.Ya
        dstFetch(d, PixelFlags::kUA, n);

        pc->v_inv255_u16(xa, da);
        pc->v_mul_u16(xa, xa, o.ux);
        pc->v_mul_u16(da, da, o.uy);
        pc->v_add_i16(da, da, xa);
        pc->v_div255_u16(da);
        out.ua.init(da);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - A8 - Xor
    // --------------------

    if (compOp() == BL_COMP_OP_XOR) {
      // Da' = Xa.(1 - Da) + Da.Ya
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_mul_u16(xa, da, o.uy);
      pc->v_inv255_u16(da, da);
      pc->v_mul_u16(da, da, o.ux);
      pc->v_add_i16(da, da, xa);
      pc->v_div255_u16(da);
      out.ua.init(da);

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - A8 - Plus
    // ---------------------

    if (compOp() == BL_COMP_OP_PLUS) {
      // Da' = Clamp(Da + Xa)
      dstFetch(d, PixelFlags::kPA, n);

      pc->v_adds_u8(d.pa, d.pa, o.px);
      out.pa.init(d.pa);

      pc->xSatisfyPixel(out, flags);
      return;
    }
  }

  VecArray vm;
  if (_mask->vm.isValid())
    vm.init(_mask->vm);
  vMaskProcA8Xmm(out, n, flags, vm, true);
}

// BLPipeline::JIT::CompOpPart - VMask Proc - A8 (Scalar)
// ======================================================

void CompOpPart::vMaskProcA8Gp(Pixel& out, PixelFlags flags, x86::Gp& msk, bool mImmutable) noexcept {
  bool hasMask = msk.isValid();

  Pixel d(PixelType::kAlpha);
  Pixel s(PixelType::kAlpha);

  x86::Gp x = cc->newUInt32("@x");
  x86::Gp y = cc->newUInt32("@y");

  x86::Gp& da = d.sa;
  x86::Gp& sa = s.sa;

  out.setCount(1);

  // VMask - A8 - SrcCopy
  // --------------------

  if (compOp() == BL_COMP_OP_SRC_COPY) {
    if (!hasMask) {
      // Da' = Sa
      srcFetch(out, flags, 1);
    }
    else {
      // Da' = Sa.m + Da.(1 - m)
      srcFetch(s, PixelFlags::kSA, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(sa, sa, msk);
      pc->uInv8(msk, msk);
      pc->uMul(da, da, msk);

      if (mImmutable)
        pc->uInv8(msk, msk);

      pc->uAdd(da, da, sa);
      pc->uDiv255(da, da);

      out.sa = da;
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - SrcOver
  // --------------------

  if (compOp() == BL_COMP_OP_SRC_OVER) {
    if (!hasMask) {
      // Da' = Sa + Da.(1 - Sa)
      srcFetch(s, PixelFlags::kSA | PixelFlags::kImmutable, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uInv8(x, sa);
      pc->uMul(da, da, x);
      pc->uDiv255(da, da);
      pc->uAdd(da, da, sa);
    }
    else {
      // Da' = Sa.m + Da.(1 - Sa.m)
      srcFetch(s, PixelFlags::kSA, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(sa, sa, msk);
      pc->uDiv255(sa, sa);
      pc->uInv8(x, sa);
      pc->uMul(da, da, x);
      pc->uDiv255(da, da);
      pc->uAdd(da, da, sa);
    }

    out.sa = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - SrcIn
  // ------------------

  if (compOp() == BL_COMP_OP_SRC_IN) {
    if (!hasMask) {
      // Da' = Sa.Da
      srcFetch(s, PixelFlags::kSA | PixelFlags::kImmutable, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(da, da, sa);
      pc->uDiv255(da, da);
    }
    else {
      // Da' = Da.(Sa.m) + Da.(1 - m)
      //     = Da.(Sa.m + 1 - m)
      srcFetch(s, PixelFlags::kSA, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(sa, sa, msk);
      pc->uDiv255(sa, sa);
      pc->uAdd(sa, sa, imm(255));
      pc->uSub(sa, sa, msk);
      pc->uMul(da, da, sa);
      pc->uDiv255(da, da);
    }

    out.sa = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - SrcOut
  // -------------------

  if (compOp() == BL_COMP_OP_SRC_OUT) {
    if (!hasMask) {
      // Da' = Sa.(1 - Da)
      srcFetch(s, PixelFlags::kSA | PixelFlags::kImmutable, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uInv8(da, da);
      pc->uMul(da, da, sa);
      pc->uDiv255(da, da);
    }
    else {
      // Da' = Sa.m.(1 - Da) + Da.(1 - m)
      srcFetch(s, PixelFlags::kSA, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(sa, sa, msk);
      pc->uDiv255(sa, sa);

      pc->uInv8(x, da);
      pc->uInv8(msk, msk);
      pc->uMul(sa, sa, x);
      pc->uMul(da, da, msk);

      if (mImmutable)
        pc->uInv8(msk, msk);

      pc->uAdd(da, da, sa);
      pc->uDiv255(da, da);
    }

    out.sa = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - DstOut
  // -------------------

  if (compOp() == BL_COMP_OP_DST_OUT) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa)
      srcFetch(s, PixelFlags::kSA, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uInv8(sa, sa);
      pc->uMul(da, da, sa);
      pc->uDiv255(da, da);
    }
    else {
      // Da' = Da.(1 - Sa.m)
      srcFetch(s, PixelFlags::kSA, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(sa, sa, msk);
      pc->uDiv255(sa, sa);
      pc->uInv8(sa, sa);
      pc->uMul(da, da, sa);
      pc->uDiv255(da, da);
    }

    out.sa = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - Xor
  // ----------------

  if (compOp() == BL_COMP_OP_XOR) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa) + Sa.(1 - Da)
      srcFetch(s, PixelFlags::kSA, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uInv8(y, sa);
      pc->uInv8(x, da);

      pc->uMul(da, da, y);
      pc->uMul(sa, sa, x);
      pc->uAdd(da, da, sa);
      pc->uDiv255(da, da);
    }
    else {
      // Da' = Da.(1 - Sa.m) + Sa.m.(1 - Da)
      srcFetch(s, PixelFlags::kSA, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(sa, sa, msk);
      pc->uDiv255(sa, sa);

      pc->uInv8(y, sa);
      pc->uInv8(x, da);

      pc->uMul(da, da, y);
      pc->uMul(sa, sa, x);
      pc->uAdd(da, da, sa);
      pc->uDiv255(da, da);
    }

    out.sa = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - Plus
  // -----------------

  if (compOp() == BL_COMP_OP_PLUS) {
    // Da' = Clamp(Da + Sa)
    // Da' = Clamp(Da + Sa.m)
    if (hasMask) {
      srcFetch(s, PixelFlags::kSA, 1);
      dstFetch(d, PixelFlags::kSA, 1);

      pc->uMul(sa, sa, msk);
      pc->uDiv255(sa, sa);
    }
    else {
      srcFetch(s, PixelFlags::kSA | PixelFlags::kImmutable, 1);
      dstFetch(d, PixelFlags::kSA, 1);
    }

    pc->uAddsU8(da, da, sa);

    out.sa = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - Invert
  // -------------------

  if (compOp() == BL_COMP_OP_INTERNAL_ALPHA_INV) {
    // Da' = 1 - Da
    // Da' = Da.(1 - m) + (1 - Da).m
    if (hasMask) {
      dstFetch(d, PixelFlags::kSA, 1);
      pc->uInv8(x, msk);
      pc->uMul(x, x, da);
      pc->uInv8(da, da);
      pc->uMul(da, da, msk);
      pc->uAdd(da, da, x);
      pc->uDiv255(da, da);
    }
    else {
      dstFetch(d, PixelFlags::kSA, 1);
      pc->uInv8(da, da);
    }

    out.sa = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - Invalid
  // --------------------

  BL_NOT_REACHED();
}

// BLPipeline::JIT::CompOpPart - VMask - Proc - A8 (Vec)
// =====================================================

void CompOpPart::vMaskProcA8Xmm(Pixel& out, uint32_t n, PixelFlags flags, VecArray& vm, bool mImmutable) noexcept {
  bool hasMask = !vm.empty();
  uint32_t kFullN = (n + 7) / 8;

  VecArray xv, yv;
  pc->newVecArray(xv, kFullN, "x");
  pc->newVecArray(yv, kFullN, "y");

  Pixel d(PixelType::kAlpha);
  Pixel s(PixelType::kAlpha);

  VecArray& da = d.ua;
  VecArray& sa = s.ua;

  out.setCount(n);

  // VMask - A8 - SrcCopy
  // --------------------

  if (compOp() == BL_COMP_OP_SRC_COPY) {
    if (!hasMask) {
      // Da' = Sa
      srcFetch(out, flags, n);
    }
    else {
      // Da' = Sa.m + Da.(1 - m)
      srcFetch(s, PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_inv255_u16(vm, vm);
      pc->v_mul_u16(da, da, vm);

      if (mImmutable)
        pc->v_inv255_u16(vm, vm);

      pc->v_add_i16(da, da, sa);
      pc->v_div255_u16(da);

      out.ua = da;
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - SrcOver
  // --------------------

  if (compOp() == BL_COMP_OP_SRC_OVER) {
    if (!hasMask) {
      // Da' = Sa + Da.(1 - Sa)
      srcFetch(s, PixelFlags::kUA | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_inv255_u16(xv, sa);
      pc->v_mul_u16(da, da, xv);
      pc->v_div255_u16(da);
      pc->v_add_i16(da, da, sa);
    }
    else {
      // Da' = Sa.m + Da.(1 - Sa.m)
      srcFetch(s, PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_div255_u16(sa);
      pc->v_inv255_u16(xv, sa);
      pc->v_mul_u16(da, da, xv);
      pc->v_div255_u16(da);
      pc->v_add_i16(da, da, sa);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - SrcIn
  // ------------------

  if (compOp() == BL_COMP_OP_SRC_IN) {
    if (!hasMask) {
      // Da' = Sa.Da
      srcFetch(s, PixelFlags::kUA | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_mul_u16(da, da, sa);
      pc->v_div255_u16(da);
    }
    else {
      // Da' = Da.(Sa.m) + Da.(1 - m)
      //     = Da.(Sa.m + 1 - m)
      srcFetch(s, PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_div255_u16(sa);
      pc->v_add_i16(sa, sa, C_MEM(i128_00FF00FF00FF00FF));
      pc->v_sub_i16(sa, sa, vm);
      pc->v_mul_u16(da, da, sa);
      pc->v_div255_u16(da);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - SrcOut
  // -------------------

  if (compOp() == BL_COMP_OP_SRC_OUT) {
    if (!hasMask) {
      // Da' = Sa.(1 - Da)
      srcFetch(s, PixelFlags::kUA | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_inv255_u16(da, da);
      pc->v_mul_u16(da, da, sa);
      pc->v_div255_u16(da);
    }
    else {
      // Da' = Sa.m.(1 - Da) + Da.(1 - m)
      srcFetch(s, PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_div255_u16(sa);

      pc->v_inv255_u16(xv, da);
      pc->v_inv255_u16(vm, vm);
      pc->v_mul_u16(sa, sa, xv);
      pc->v_mul_u16(da, da, vm);

      if (mImmutable)
        pc->v_inv255_u16(vm, vm);

      pc->v_add_i16(da, da, sa);
      pc->v_div255_u16(da);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - DstOut
  // -------------------

  if (compOp() == BL_COMP_OP_DST_OUT) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa)
      srcFetch(s, PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_inv255_u16(sa, sa);
      pc->v_mul_u16(da, da, sa);
      pc->v_div255_u16(da);
    }
    else {
      // Da' = Da.(1 - Sa.m)
      srcFetch(s, PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_div255_u16(sa);
      pc->v_inv255_u16(sa, sa);
      pc->v_mul_u16(da, da, sa);
      pc->v_div255_u16(da);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - Xor
  // ----------------

  if (compOp() == BL_COMP_OP_XOR) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa) + Sa.(1 - Da)
      srcFetch(s, PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_inv255_u16(yv, sa);
      pc->v_inv255_u16(xv, da);

      pc->v_mul_u16(da, da, yv);
      pc->v_mul_u16(sa, sa, xv);
      pc->v_add_i16(da, da, sa);
      pc->v_div255_u16(da);
    }
    else {
      // Da' = Da.(1 - Sa.m) + Sa.m.(1 - Da)
      srcFetch(s, PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kUA, n);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_div255_u16(sa);

      pc->v_inv255_u16(yv, sa);
      pc->v_inv255_u16(xv, da);

      pc->v_mul_u16(da, da, yv);
      pc->v_mul_u16(sa, sa, xv);
      pc->v_add_i16(da, da, sa);
      pc->v_div255_u16(da);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - Plus
  // -----------------

  if (compOp() == BL_COMP_OP_PLUS) {
    if (!hasMask) {
      // Da' = Clamp(Da + Sa)
      srcFetch(s, PixelFlags::kPA | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kPA, n);
    }
    else {
      // Da' = Clamp(Da + Sa.m)
      srcFetch(s, PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kPA, n);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_div255_u16(sa);

      s.pa = sa.even();
      pc->v_packs_i16_u8(s.pa, s.pa, sa.odd());
    }

    pc->v_adds_u8(d.pa, d.pa, s.pa);
    out.pa = d.pa;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - Invert
  // -------------------

  if (compOp() == BL_COMP_OP_INTERNAL_ALPHA_INV) {
    if (!hasMask) {
      // Da' = 1 - Da
      dstFetch(d, PixelFlags::kUA, n);
      pc->v_inv255_u16(da, da);
    }
    else {
      // Da' = Da.(1 - m) + (1 - Da).m
      dstFetch(d, PixelFlags::kUA, n);
      pc->v_inv255_u16(xv, vm);
      pc->v_mul_u16(xv, xv, da);
      pc->v_inv255_u16(da, da);
      pc->v_mul_u16(da, da, vm);
      pc->v_add_i16(da, da, xv);
      pc->v_div255_u16(da);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMask - A8 - Invalid
  // --------------------

  BL_NOT_REACHED();
}

// BLPipeline::JIT::CompOpPart - CMask - Init & Fini - RGBA
// ========================================================

void CompOpPart::cMaskInitRGBA32(const x86::Vec& vm) noexcept {
  bool hasMask = vm.isValid();
  bool useDa = hasDa();

  if (srcPart()->isSolid()) {
    Pixel& s = srcPart()->as<FetchSolidPart>()->_pixel;
    SolidPixel& o = _solidOpt;

    // CMaskInit - RGBA32 - Solid - SrcCopy
    // ------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPC);
        o.px = s.pc[0];
      }
      else {
        // Xca = (Sca * m) + 0.5 <Rounding>
        // Xa  = (Sa  * m) + 0.5 <Rounding>
        // Im  = (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = cc->newXmm("p.ux");
        o.vn = vm;

        pc->v_mul_u16(o.ux, s.uc[0], o.vn);
        pc->v_add_i16(o.ux, o.ux, pc->constAsXmm(&blCommonTable.i128_0080008000800080));
        pc->v_inv255_u16(o.vn, o.vn);
      }
    }

    // CMaskInit - RGBA32 - Solid - SrcOver
    // ------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_OVER) {
      if (!hasMask) {
        // Xca = Sca * 1 + 0.5 <Rounding>
        // Xa  = Sa  * 1 + 0.5 <Rounding>
        // Yca = 1 - Sa
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUIA | PixelFlags::kImmutable);

        o.ux = cc->newXmm("p.ux");
        o.uy = s.uia[0];

        pc->v_sll_i16(o.ux, s.uc[0], 8);
        pc->v_sub_i16(o.ux, o.ux, s.uc[0]);
        pc->v_add_i16(o.ux, o.ux, pc->constAsXmm(&blCommonTable.i128_0080008000800080));
      }
      else {
        // Xca = Sca * m + 0.5 <Rounding>
        // Xa  = Sa  * m + 0.5 <Rounding>
        // Yca = 1 - (Sa * m)
        // Ya  = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kImmutable);

        o.ux = cc->newXmm("p.ux");
        o.uy = cc->newXmm("p.uy");

        pc->v_mul_u16(o.uy, s.uc[0], vm);
        pc->v_div255_u16(o.uy);

        pc->v_sll_i16(o.ux, o.uy, 8);
        pc->v_sub_i16(o.ux, o.ux, o.uy);
        pc->v_add_i16(o.ux, o.ux, pc->constAsXmm(&blCommonTable.i128_0080008000800080));

        pc->v_swizzle_lo_i16(o.uy, o.uy, x86::shuffleImm(3, 3, 3, 3));
        pc->v_swizzle_hi_i16(o.uy, o.uy, x86::shuffleImm(3, 3, 3, 3));
        pc->v_inv255_u16(o.uy, o.uy);
      }
    }

    // CMaskInit - RGBA32 - Solid - SrcIn | SrcOut
    // -------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_IN ||
             compOp() == BL_COMP_OP_SRC_OUT) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = s.uc[0];
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Im  = 1   - m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = cc->newXmm("o.uc0");
        o.vn = vm;

        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_div255_u16(o.ux);
        pc->v_inv255_u16(vm, vm);
      }
    }

    // CMaskInit - RGBA32 - Solid - SrcAtop & Xor & Darken & Lighten
    // -------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_ATOP ||
             compOp() == BL_COMP_OP_XOR ||
             compOp() == BL_COMP_OP_DARKEN ||
             compOp() == BL_COMP_OP_LIGHTEN) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = 1 - Sa
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUIA);

        o.ux = s.uc[0];
        o.uy = s.uia[0];
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = 1 - (Sa * m)
        // Ya  = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = cc->newXmm("o.ux");
        o.uy = vm;

        pc->v_mul_u16(o.ux, s.uc[0], o.uy);
        pc->v_div255_u16(o.ux);

        pc->vExpandAlpha16(o.uy, o.ux, false);
        pc->v_swizzle_i32(o.uy, o.uy, x86::shuffleImm(0, 0, 0, 0));
        pc->v_inv255_u16(o.uy, o.uy);
      }
    }

    // CMaskInit - RGBA32 - Solid - Dst
    // --------------------------------

    else if (compOp() == BL_COMP_OP_DST_COPY) {
      BL_NOT_REACHED();
    }

    // CMaskInit - RGBA32 - Solid - DstOver
    // ------------------------------------

    else if (compOp() == BL_COMP_OP_DST_OVER) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = s.uc[0];
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = cc->newXmm("o.uc0");
        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_div255_u16(o.ux);
      }
    }

    // CMaskInit - RGBA32 - Solid - DstIn
    // ----------------------------------

    else if (compOp() == BL_COMP_OP_DST_IN) {
      if (!hasMask) {
        // Xca = Sa
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUA);

        o.ux = s.ua[0];
      }
      else {
        // Xca = 1 - m.(1 - Sa)
        // Xa  = 1 - m.(1 - Sa)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUA);

        o.ux = cc->newXmm("o.ux");
        pc->v_mov(o.ux, s.ua[0]);

        pc->v_inv255_u16(o.ux, o.ux);
        pc->v_mul_u16(o.ux, o.ux, vm);
        pc->v_div255_u16(o.ux);
        pc->v_inv255_u16(o.ux, o.ux);
      }
    }

    // CMaskInit - RGBA32 - Solid - DstOut
    // -----------------------------------

    else if (compOp() == BL_COMP_OP_DST_OUT) {
      if (!hasMask) {
        // Xca = 1 - Sa
        // Xa  = 1 - Sa
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUIA);

          o.ux = s.uia[0];
        }
        // Xca = 1 - Sa
        // Xa  = 1
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUA);

          o.ux = cc->newXmm("ux");
          pc->v_mov(o.ux, s.ua[0]);
          pc->vNegRgb8W(o.ux, o.ux);
        }
      }
      else {
        // Xca = 1 - (Sa * m)
        // Xa  = 1 - (Sa * m)
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUA);

          o.ux = vm;
          pc->v_mul_u16(o.ux, o.ux, s.ua[0]);
          pc->v_div255_u16(o.ux);
          pc->v_inv255_u16(o.ux, o.ux);
        }
        // Xca = 1 - (Sa * m)
        // Xa  = 1
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUA);

          o.ux = vm;
          pc->v_mul_u16(o.ux, o.ux, s.ua[0]);
          pc->v_div255_u16(o.ux);
          pc->v_inv255_u16(o.ux, o.ux);
          pc->vFillAlpha255W(o.ux, o.ux);
        }
      }
    }

    // CMaskInit - RGBA32 - Solid - DstAtop
    // ------------------------------------

    else if (compOp() == BL_COMP_OP_DST_ATOP) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = Sa
        // Ya  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUA);

        o.ux = s.uc[0];
        o.uy = s.ua[0];
      }
      else {
        // Xca = Sca.m
        // Xa  = Sa .m
        // Yca = Sa .m + (1 - m)
        // Ya  = Sa .m + (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUA);

        o.ux = cc->newXmm("o.ux");
        o.uy = cc->newXmm("o.uy");

        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_inv255_u16(o.uy, vm);
        pc->v_div255_u16(o.ux);
        pc->v_add_i16(o.uy, o.uy, o.ux);
      }
    }

    // CMaskInit - RGBA32 - Solid - Plus
    // ---------------------------------

    else if (compOp() == BL_COMP_OP_PLUS) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPC);

        o.px = s.pc[0];
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);
        o.px = cc->newXmm("px");

        pc->v_mul_u16(o.px, s.uc[0], vm);
        pc->v_div255_u16(o.px);
        pc->v_packs_i16_u8(o.px, o.px, o.px);
      }
    }

    // CMaskInit - RGBA32 - Solid - Minus
    // ----------------------------------

    else if (compOp() == BL_COMP_OP_MINUS) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = 0
        // Yca = Sca
        // Ya  = Sa
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

          o.ux = cc->newXmm("ux");
          o.uy = s.uc[0];

          pc->v_mov(o.ux, o.uy);
          pc->vZeroAlphaW(o.ux, o.ux);
        }
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPC);
          o.px = cc->newXmm("px");
          pc->v_mov(o.px, s.pc[0]);
          pc->vZeroAlphaB(o.px, o.px);
        }
      }
      else {
        // Xca = Sca
        // Xa  = 0
        // Yca = Sca
        // Ya  = Sa
        // M   = m       <Alpha channel is set to 256>
        // Im  = 1 - m   <Alpha channel is set to 0  >
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

          o.ux = cc->newXmm("ux");
          o.uy = cc->newXmm("uy");
          o.vm = vm;
          o.vn = cc->newXmm("vn");

          pc->vZeroAlphaW(o.ux, s.uc[0]);
          pc->v_mov(o.uy, s.uc[0]);

          pc->v_inv255_u16(o.vn, o.vm);
          pc->vZeroAlphaW(o.vm, o.vm);
          pc->vZeroAlphaW(o.vn, o.vn);
          pc->vFillAlpha255W(o.vm, o.vm);
        }
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

          o.ux = cc->newXmm("ux");
          o.vm = vm;
          o.vn = cc->newXmm("vn");

          pc->vZeroAlphaW(o.ux, s.uc[0]);
          pc->v_inv255_u16(o.vn, o.vm);
        }
      }
    }

    // CMaskInit - RGBA32 - Solid - Modulate
    // -------------------------------------

    else if (compOp() == BL_COMP_OP_MODULATE) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = s.uc[0];
      }
      else {
        // Xca = Sca * m + (1 - m)
        // Xa  = Sa  * m + (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = cc->newXmm("o.uc0");

        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_div255_u16(o.ux);
        pc->v_add_i16(o.ux, o.ux, C_MEM(i128_00FF00FF00FF00FF));
        pc->v_sub_i16(o.ux, o.ux, vm);
      }
    }

    // CMaskInit - RGBA32 - Solid - Multiply
    // -------------------------------------

    else if (compOp() == BL_COMP_OP_MULTIPLY) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = Sca + (1 - Sa)
        // Ya  = Sa  + (1 - Sa)
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUIA);

          o.ux = s.uc[0];
          o.uy = cc->newXmm("uy");

          pc->v_mov(o.uy, s.uia[0]);
          pc->v_add_i16(o.uy, o.uy, o.ux);
        }
        // Yca = Sca + (1 - Sa)
        // Ya  = Sa  + (1 - Sa)
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUIA);

          o.uy = cc->newXmm("uy");
          pc->v_mov(o.uy, s.uia[0]);
          pc->v_add_i16(o.uy, o.uy, s.uc[0]);
        }
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = Sca * m + (1 - Sa * m)
        // Ya  = Sa  * m + (1 - Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = cc->newXmm("ux");
        o.uy = cc->newXmm("uy");

        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_div255_u16(o.ux);

        pc->v_swizzle_lo_i16(o.uy, o.ux, x86::shuffleImm(3, 3, 3, 3));
        pc->v_inv255_u16(o.uy, o.uy);
        pc->v_swizzle_i32(o.uy, o.uy, x86::shuffleImm(0, 0, 0, 0));
        pc->v_add_i16(o.uy, o.uy, o.ux);
      }
    }

    // CMaskInit - RGBA32 - Solid - Screen
    // -----------------------------------

    else if (compOp() == BL_COMP_OP_SCREEN) {
      if (!hasMask) {
        // Xca = Sca * 1 + 0.5 <Rounding>
        // Xa  = Sa  * 1 + 0.5 <Rounding>
        // Yca = 1 - Sca
        // Ya  = 1 - Sa

        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = cc->newXmm("p.ux");
        o.uy = cc->newXmm("p.uy");

        pc->v_inv255_u16(o.uy, o.ux);
        pc->v_sll_i16(o.ux, s.uc[0], 8);
        pc->v_sub_i16(o.ux, o.ux, s.uc[0]);
        pc->v_add_i16(o.ux, o.ux, pc->constAsXmm(&blCommonTable.i128_0080008000800080));
      }
      else {
        // Xca = Sca * m + 0.5 <Rounding>
        // Xa  = Sa  * m + 0.5 <Rounding>
        // Yca = 1 - (Sca * m)
        // Ya  = 1 - (Sa  * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = cc->newXmm("p.ux");
        o.uy = cc->newXmm("p.uy");

        pc->v_mul_u16(o.uy, s.uc[0], vm);
        pc->v_div255_u16(o.uy);

        pc->v_sll_i16(o.ux, o.uy, 8);
        pc->v_sub_i16(o.ux, o.ux, o.uy);
        pc->v_add_i16(o.ux, o.ux, pc->constAsXmm(&blCommonTable.i128_0080008000800080));
        pc->v_inv255_u16(o.uy, o.uy);
      }
    }

    // CMaskInit - RGBA32 - Solid - LinearBurn & Difference & Exclusion
    // ----------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_LINEAR_BURN ||
             compOp() == BL_COMP_OP_DIFFERENCE ||
             compOp() == BL_COMP_OP_EXCLUSION) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = Sa
        // Ya  = Sa
        srcPart()->as<FetchSolidPart>()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUA);

        o.ux = s.uc[0];
        o.uy = s.ua[0];
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = Sa  * m
        // Ya  = Sa  * m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = cc->newXmm("ux");
        o.uy = cc->newXmm("uy");

        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_div255_u16(o.ux);

        pc->v_swizzle_lo_i16(o.uy, o.ux, x86::shuffleImm(3, 3, 3, 3));
        pc->v_swizzle_i32(o.uy, o.uy, x86::shuffleImm(0, 0, 0, 0));
      }
    }

    // CMaskInit - RGBA32 - Solid - TypeA (Non-Opaque)
    // -----------------------------------------------

    else if (blTestFlag(compOpFlags(), BLCompOpFlags::kTypeA) && hasMask) {
      // Multiply the source pixel with the mask if `TypeA`.
      srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

      Pixel& pre = _solidPre;
      pre.setCount(1);
      pre.uc.init(cc->newXmm("pre.uc"));

      pc->v_mul_u16(pre.uc[0], s.uc[0], vm);
      pc->v_div255_u16(pre.uc[0]);
    }

    // CMaskInit - RGBA32 - Solid - No Optimizations
    // ---------------------------------------------

    else {
      // No optimization. The compositor will simply use the mask provided.
      _mask->vm = vm;
    }
  }
  else {
    _mask->vm = vm;

    // CMaskInit - RGBA32 - NonSolid - SrcCopy
    // ---------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (hasMask) {
        _mask->vn = cc->newXmm("vn");
        pc->v_inv255_u16(_mask->vn, vm);
      }
    }
  }

  _cMaskLoopInit(hasMask ? CMaskLoopType::kVariant : CMaskLoopType::kOpaque);
}

void CompOpPart::cMaskFiniRGBA32() noexcept {
  if (srcPart()->isSolid()) {
    _solidOpt.reset();
    _solidPre.reset();
  }
  else {
    // TODO: [PIPEGEN]
  }

  _mask->reset();
  _cMaskLoopFini();
}

// BLPipeline::JIT::CompOpPart - CMask - Proc - RGBA
// =================================================

void CompOpPart::cMaskProcRGBA32Xmm(Pixel& out, uint32_t n, PixelFlags flags) noexcept {
  bool hasMask = isLoopCMask();

  uint32_t kFullN = regCountByRGBA32PixelCount(pc->simdWidth(), n);
  uint32_t kUseHi = n > 1;

  out.setCount(n);

  if (srcPart()->isSolid()) {
    Pixel d(pixelType());
    SolidPixel& o = _solidOpt;
    VecArray xv, yv, zv;

    pc->newVecArray(xv, kFullN, "x");
    pc->newVecArray(yv, kFullN, "y");
    pc->newVecArray(zv, kFullN, "z");

    bool useDa = hasDa();

    // CMaskProc - RGBA32 - SrcCopy
    // ----------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      // Dca' = Xca
      // Da'  = Xa
      if (!hasMask) {
        out.pc.init(o.px);
        out.makeImmutable();
      }
      // Dca' = Xca + Dca.(1 - m)
      // Da'  = Xa  + Da .(1 - m)
      else {
        dstFetch(d, PixelFlags::kUC, n);
        VecArray& dv = d.uc;
        pc->v_mul_u16(dv, dv, o.vn);
        pc->v_add_i16(dv, dv, o.ux);
        pc->v_mulh257_u16(dv, dv);
        out.uc.init(dv);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - SrcOver & Screen
    // -------------------------------------

    if (compOp() == BL_COMP_OP_SRC_OVER || compOp() == BL_COMP_OP_SCREEN) {
      // Dca' = Xca + Dca.Yca
      // Da'  = Xa  + Da .Ya
      dstFetch(d, PixelFlags::kUC, n);
      VecArray& dv = d.uc;

      pc->v_mul_u16(dv, dv, o.uy);
      pc->v_add_i16(dv, dv, o.ux);
      pc->v_mulh257_u16(dv, dv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);

      return;
    }

    // CMaskProc - RGBA32 - SrcIn
    // --------------------------

    if (compOp() == BL_COMP_OP_SRC_IN) {
      // Dca' = Xca.Da
      // Da'  = Xa .Da
      if (!hasMask) {
        dstFetch(d, PixelFlags::kUA, n);
        VecArray& dv = d.ua;

        pc->v_mul_u16(dv, dv, o.ux);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Xca.Da + Dca.(1 - m)
      // Da'  = Xa .Da + Da .(1 - m)
      else {
        dstFetch(d, PixelFlags::kUC | PixelFlags::kUA, n);
        VecArray& dv = d.uc;
        VecArray& da = d.ua;

        pc->v_mul_u16(dv, dv, o.vn);
        pc->v_mul_u16(da, da, o.ux);
        pc->v_add_i16(dv, dv, da);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - SrcOut
    // ---------------------------

    if (compOp() == BL_COMP_OP_SRC_OUT) {
      // Dca' = Xca.(1 - Da)
      // Da'  = Xa .(1 - Da)
      if (!hasMask) {
        dstFetch(d, PixelFlags::kUIA, n);
        VecArray& dv = d.uia;

        pc->v_mul_u16(dv, dv, o.ux);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Xca.(1 - Da) + Dca.(1 - m)
      // Da'  = Xa .(1 - Da) + Da .(1 - m)
      else {
        dstFetch(d, PixelFlags::kUC, n);
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->v_inv255_u16(xv, xv);
        pc->v_mul_u16(xv, xv, o.ux);
        pc->v_mul_u16(dv, dv, o.vn);
        pc->v_add_i16(dv, dv, xv);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - SrcAtop
    // ----------------------------

    if (compOp() == BL_COMP_OP_SRC_ATOP) {
      // Dca' = Xca.Da + Dca.Yca
      // Da'  = Xa .Da + Da .Ya
      dstFetch(d, PixelFlags::kUC, n);
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_mul_u16(dv, dv, o.uy);
      pc->v_mul_u16(xv, xv, o.ux);

      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Dst
    // ------------------------

    if (compOp() == BL_COMP_OP_DST_COPY) {
      // Dca' = Dca
      // Da'  = Da
      BL_NOT_REACHED();
    }

    // CMaskProc - RGBA32 - DstOver
    // ----------------------------

    if (compOp() == BL_COMP_OP_DST_OVER) {
      // Dca' = Xca.(1 - Da) + Dca
      // Da'  = Xa .(1 - Da) + Da
      dstFetch(d, PixelFlags::kPC | PixelFlags::kUIA, n);
      VecArray& dv = d.uia;

      pc->v_mul_u16(dv, dv, o.ux);
      pc->v_div255_u16(dv);

      VecArray dh = dv.even();
      pc->v_packs_i16_u8(dh, dh, dv.odd());
      pc->v_add_i32(dh, dh, d.pc);

      out.pc.init(dh);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - DstIn & DstOut
    // -----------------------------------

    if (compOp() == BL_COMP_OP_DST_IN || compOp() == BL_COMP_OP_DST_OUT) {
      // Dca' = Xca.Dca
      // Da'  = Xa .Da
      dstFetch(d, PixelFlags::kUC, n);
      VecArray& dv = d.uc;

      pc->v_mul_u16(dv, dv, o.ux);
      pc->v_div255_u16(dv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - DstAtop | Xor | Multiply
    // ---------------------------------------------

    if (compOp() == BL_COMP_OP_DST_ATOP || compOp() == BL_COMP_OP_XOR || compOp() == BL_COMP_OP_MULTIPLY) {
      if (useDa) {
        // Dca' = Xca.(1 - Da) + Dca.Yca
        // Da'  = Xa .(1 - Da) + Da .Ya
        dstFetch(d, PixelFlags::kUC, n);
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->v_mul_u16(dv, dv, o.uy);
        pc->v_inv255_u16(xv, xv);
        pc->v_mul_u16(xv, xv, o.ux);

        pc->v_add_i16(dv, dv, xv);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      else {
        // Dca' = Dca.Yca
        // Da'  = Da .Ya
        dstFetch(d, PixelFlags::kUC, n);
        VecArray& dv = d.uc;

        pc->v_mul_u16(dv, dv, o.uy);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Plus
    // -------------------------

    if (compOp() == BL_COMP_OP_PLUS) {
      // Dca' = Clamp(Dca + Sca)
      // Da'  = Clamp(Da  + Sa )
      dstFetch(d, PixelFlags::kPC, n);
      VecArray& dv = d.pc;

      pc->v_adds_u8(dv, dv, o.px);
      out.pc.init(dv);

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Minus
    // --------------------------

    if (compOp() == BL_COMP_OP_MINUS) {
      if (!hasMask) {
        if (useDa) {
          // Dca' = Clamp(Dca - Xca) + Yca.(1 - Da)
          // Da'  = Da + Ya.(1 - Da)
          dstFetch(d, PixelFlags::kUC, n);
          VecArray& dv = d.uc;

          pc->vExpandAlpha16(xv, dv, kUseHi);
          pc->v_inv255_u16(xv, xv);
          pc->v_mul_u16(xv, xv, o.uy);
          pc->v_subs_u16(dv, dv, o.ux);
          pc->v_div255_u16(xv);

          pc->v_add_i16(dv, dv, xv);
          out.uc.init(dv);
        }
        else {
          // Dca' = Clamp(Dca - Xca)
          // Da'  = <unchanged>
          dstFetch(d, PixelFlags::kPC, n);
          VecArray& dh = d.pc;

          pc->v_subs_u8(dh, dh, o.px);
          out.pc.init(dh);
        }
      }
      else {
        if (useDa) {
          // Dca' = (Clamp(Dca - Xca) + Yca.(1 - Da)).m + Dca.(1 - m)
          // Da'  = Da + Ya.(1 - Da)
          dstFetch(d, PixelFlags::kUC, n);
          VecArray& dv = d.uc;

          pc->vExpandAlpha16(xv, dv, kUseHi);
          pc->v_inv255_u16(xv, xv);
          pc->v_mul_u16(yv, dv, o.vn);
          pc->v_subs_u16(dv, dv, o.ux);
          pc->v_mul_u16(xv, xv, o.uy);
          pc->v_div255_u16(xv);
          pc->v_add_i16(dv, dv, xv);
          pc->v_mul_u16(dv, dv, o.vm);

          pc->v_add_i16(dv, dv, yv);
          pc->v_div255_u16(dv);
          out.uc.init(dv);
        }
        else {
          // Dca' = Clamp(Dca - Xca).m + Dca.(1 - m)
          // Da'  = <unchanged>
          dstFetch(d, PixelFlags::kUC, n);
          VecArray& dv = d.uc;

          pc->v_mul_u16(yv, dv, o.vn);
          pc->v_subs_u16(dv, dv, o.ux);
          pc->v_mul_u16(dv, dv, o.vm);

          pc->v_add_i16(dv, dv, yv);
          pc->v_div255_u16(dv);
          out.uc.init(dv);
        }
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Modulate
    // -----------------------------

    if (compOp() == BL_COMP_OP_MODULATE) {
      dstFetch(d, PixelFlags::kUC, n);
      VecArray& dv = d.uc;

      // Dca' = Dca.Xca
      // Da'  = Da .Xa
      pc->v_mul_u16(dv, dv, o.ux);
      pc->v_div255_u16(dv);

      if (!useDa)
        pc->vFillAlpha255W(dv, dv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Darken & Lighten
    // -------------------------------------

    if (compOp() == BL_COMP_OP_DARKEN || compOp() == BL_COMP_OP_LIGHTEN) {
      // Dca' = minmax(Dca + Xca.(1 - Da), Xca + Dca.Yca)
      // Da'  = Xa + Da.Ya
      dstFetch(d, PixelFlags::kUC, n);
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_mul_u16(xv, xv, o.ux);
      pc->v_div255_u16(xv);
      pc->v_add_i16(xv, xv, dv);
      pc->v_mul_u16(dv, dv, o.uy);
      pc->v_div255_u16(dv);
      pc->v_add_i16(dv, dv, o.ux);

      if (compOp() == BL_COMP_OP_DARKEN)
        pc->v_min_u8(dv, dv, xv);
      else
        pc->v_max_u8(dv, dv, xv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - LinearBurn
    // -------------------------------

    if (compOp() == BL_COMP_OP_LINEAR_BURN) {
      // Dca' = Dca + Xca - Yca.Da
      // Da'  = Da  + Xa  - Ya .Da
      dstFetch(d, PixelFlags::kUC, n);
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_mul_u16(xv, xv, o.uy);
      pc->v_add_i16(dv, dv, o.ux);
      pc->v_div255_u16(xv);
      pc->v_subs_u16(dv, dv, xv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Difference
    // -------------------------------

    if (compOp() == BL_COMP_OP_DIFFERENCE) {
      // Dca' = Dca + Sca - 2.min(Sca.Da, Dca.Sa)
      // Da'  = Da  + Sa  -   min(Sa .Da, Da .Sa)
      dstFetch(d, PixelFlags::kUC, n);
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_mul_u16(yv, o.uy, dv);
      pc->v_mul_u16(xv, xv, o.ux);
      pc->v_add_i16(dv, dv, o.ux);
      pc->v_min_u16(yv, yv, xv);
      pc->v_div255_u16(yv);
      pc->v_sub_i16(dv, dv, yv);
      pc->vZeroAlphaW(yv, yv);
      pc->v_sub_i16(dv, dv, yv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Exclusion
    // ------------------------------

    if (compOp() == BL_COMP_OP_EXCLUSION) {
      // Dca' = Dca + Xca - 2.Xca.Dca
      // Da'  = Da + Xa - Xa.Da
      dstFetch(d, PixelFlags::kUC, n);
      VecArray& dv = d.uc;

      pc->v_mul_u16(xv, dv, o.ux);
      pc->v_add_i16(dv, dv, o.ux);
      pc->v_div255_u16(xv);
      pc->v_sub_i16(dv, dv, xv);
      pc->vZeroAlphaW(xv, xv);
      pc->v_sub_i16(dv, dv, xv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }
  }

  VecArray vm;
  if (_mask->vm.isValid())
    vm.init(_mask->vm);

  vMaskProcRGBA32Xmm(out, n, flags, vm, true);
}

// BLPipeline::JIT::CompOpPart - VMask - RGBA32 (Vec)
// ==================================================

void CompOpPart::vMaskProcRGBA32Xmm(Pixel& out, uint32_t n, PixelFlags flags, VecArray& vm, bool mImmutable) noexcept {
  bool hasMask = !vm.empty();

  bool useDa = hasDa();
  bool useSa = hasSa() || hasMask || isLoopCMask();

  uint32_t kFullN = regCountByRGBA32PixelCount(pc->simdWidth(), n);
  uint32_t kUseHi = (n > 1);
  uint32_t kSplit = (kFullN == 1) ? 1 : 2;

  VecArray xv, yv, zv;
  pc->newVecArray(xv, kFullN, "x");
  pc->newVecArray(yv, kFullN, "y");
  pc->newVecArray(zv, kFullN, "z");

  Pixel d(PixelType::kRGBA);
  Pixel s(PixelType::kRGBA);

  out.setCount(n);

  // VMaskProc - RGBA32 - SrcCopy
  // ----------------------------

  if (compOp() == BL_COMP_OP_SRC_COPY) {
    // Composition:
    //   Da - Optional.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Sca
      // Da'  = Sa
      srcFetch(out, flags, n);
    }
    else {
      // Dca' = Sca.m + Dca.(1 - m)
      // Da'  = Sa .m + Da .(1 - m)
      srcFetch(s, PixelFlags::kUC, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& vs = s.uc;
      VecArray& vd = d.uc;
      VecArray vn;

      pc->v_mul_u16(vs, vs, vm);
      vMaskProcRGBA32InvertMask(vn, vm);

      pc->v_mul_u16(vd, vd, vn);
      pc->v_add_i16(vd, vd, vs);
      vMaskProcRGBA32InvertDone(vn, mImmutable);

      pc->v_div255_u16(vd);
      out.uc.init(vd);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - SrcOver
  // ----------------------------

  if (compOp() == BL_COMP_OP_SRC_OVER) {
    // Composition:
    //   Da - Optional.
    //   Sa - Required, otherwise SRC_COPY.

    if (!hasMask) {
      // Dca' = Sca + Dca.(1 - Sa)
      // Da'  = Sa  + Da .(1 - Sa)
      srcFetch(s, PixelFlags::kPC | PixelFlags::kUIA | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& uv = s.uia;
      VecArray& dv = d.uc;

      pc->v_mul_u16(dv, dv, uv);
      pc->v_div255_u16(dv);

      VecArray dh = dv.even();
      pc->v_packs_i16_u8(dh, dh, dv.odd());
      pc->v_add_i32(dh, dh, s.pc);

      out.pc.init(dh);
    }
    else {
      // Dca' = Sca.m + Dca.(1 - Sa.m)
      // Da'  = Sa .m + Da .(1 - Sa.m)
      srcFetch(s, PixelFlags::kUC, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_mul_u16(dv, dv, xv);
      pc->v_div255_u16(dv);

      pc->v_add_i16(dv, dv, sv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - SrcIn
  // --------------------------

  if (compOp() == BL_COMP_OP_SRC_IN) {
    // Composition:
    //   Da - Required, otherwise SRC_COPY.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Sca.Da
      // Da'  = Sa .Da
      srcFetch(s, PixelFlags::kUC | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.ua;

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Sca.m.Da + Dca.(1 - m)
      // Da'  = Sa .m.Da + Da .(1 - m)
      srcFetch(s, PixelFlags::kUC | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_mul_u16(xv, xv, sv);
      pc->v_div255_u16(xv);
      pc->v_mul_u16(xv, xv, vm);
      vMaskProcRGBA32InvertMask(vm, vm);

      pc->v_mul_u16(dv, dv, vm);
      vMaskProcRGBA32InvertDone(vm, mImmutable);

      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - SrcOut
  // ---------------------------

  if (compOp() == BL_COMP_OP_SRC_OUT) {
    // Composition:
    //   Da - Required, otherwise CLEAR.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Sca.(1 - Da)
      // Da'  = Sa .(1 - Da)
      srcFetch(s, PixelFlags::kUC | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUIA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uia;

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Sca.(1 - Da).m + Dca.(1 - m)
      // Da'  = Sa .(1 - Da).m + Da .(1 - m)
      srcFetch(s, PixelFlags::kUC | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_inv255_u16(xv, xv);

      pc->v_mul_u16(xv, xv, sv);
      pc->v_div255_u16(xv);
      pc->v_mul_u16(xv, xv, vm);
      vMaskProcRGBA32InvertMask(vm, vm);

      pc->v_mul_u16(dv, dv, vm);
      vMaskProcRGBA32InvertDone(vm, mImmutable);

      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - SrcAtop
  // ----------------------------

  if (compOp() == BL_COMP_OP_SRC_ATOP) {
    // Composition:
    //   Da - Required.
    //   Sa - Required.

    if (!hasMask) {
      // Dca' = Sca.Da + Dca.(1 - Sa)
      // Da'  = Sa .Da + Da .(1 - Sa) = Da
      srcFetch(s, PixelFlags::kUC | PixelFlags::kUIA | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& uv = s.uia;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_mul_u16(dv, dv, uv);
      pc->v_mul_u16(xv, xv, sv);
      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);

      out.uc.init(dv);
    }
    else {
      // Dca' = Sca.Da.m + Dca.(1 - Sa.m)
      // Da'  = Sa .Da.m + Da .(1 - Sa.m) = Da
      srcFetch(s, PixelFlags::kUC, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->vExpandAlpha16(yv, dv, kUseHi);
      pc->v_mul_u16(dv, dv, xv);
      pc->v_mul_u16(yv, yv, sv);
      pc->v_add_i16(dv, dv, yv);
      pc->v_div255_u16(dv);

      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Dst
  // ------------------------

  if (compOp() == BL_COMP_OP_DST_COPY) {
    // Dca' = Dca
    // Da'  = Da
    BL_NOT_REACHED();
  }

  // VMaskProc - RGBA32 - DstOver
  // ----------------------------

  if (compOp() == BL_COMP_OP_DST_OVER) {
    // Composition:
    //   Da - Required, otherwise DST_COPY.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Dca + Sca.(1 - Da)
      // Da'  = Da  + Sa .(1 - Da)
      srcFetch(s, PixelFlags::kUC | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kPC | PixelFlags::kUIA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uia;

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);

      VecArray dh = dv.even();
      pc->v_packs_i16_u8(dh, dh, dv.odd());
      pc->v_add_i32(dh, dh, d.pc);

      out.pc.init(dh);
    }
    else {
      // Dca' = Dca + Sca.m.(1 - Da)
      // Da'  = Da  + Sa .m.(1 - Da)
      srcFetch(s, PixelFlags::kUC, n);
      dstFetch(d, PixelFlags::kPC | PixelFlags::kUIA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uia;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);

      VecArray dh = dv.even();
      pc->v_packs_i16_u8(dh, dh, dv.odd());
      pc->v_add_i32(dh, dh, d.pc);

      out.pc.init(dh);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - DstIn
  // --------------------------

  if (compOp() == BL_COMP_OP_DST_IN) {
    // Composition:
    //   Da - Optional.
    //   Sa - Required, otherwise DST_COPY.

    if (!hasMask) {
      // Dca' = Dca.Sa
      // Da'  = Da .Sa
      srcFetch(s, PixelFlags::kUA | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.ua;
      VecArray& dv = d.uc;

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Dca.(1 - m.(1 - Sa))
      // Da'  = Da .(1 - m.(1 - Sa))
      srcFetch(s, PixelFlags::kUIA, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uia;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
      pc->v_inv255_u16(sv, sv);

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - DstOut
  // ---------------------------

  if (compOp() == BL_COMP_OP_DST_OUT) {
    // Composition:
    //   Da - Optional.
    //   Sa - Required, otherwise CLEAR.

    if (!hasMask) {
      // Dca' = Dca.(1 - Sa)
      // Da'  = Da .(1 - Sa)
      srcFetch(s, PixelFlags::kUIA | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uia;
      VecArray& dv = d.uc;

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Dca.(1 - Sa.m)
      // Da'  = Da .(1 - Sa.m)
      srcFetch(s, PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.ua;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
      pc->v_inv255_u16(sv, sv);

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    if (!useDa) pc->vFillAlpha(out);
    return;
  }

  // VMaskProc - RGBA32 - DstAtop
  // ----------------------------

  if (compOp() == BL_COMP_OP_DST_ATOP) {
    // Composition:
    //   Da - Required.
    //   Sa - Required.

    if (!hasMask) {
      // Dca' = Dca.Sa + Sca.(1 - Da)
      // Da'  = Da .Sa + Sa .(1 - Da)
      srcFetch(s, PixelFlags::kUC | PixelFlags::kUA | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& uv = s.ua;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_mul_u16(dv, dv, uv);
      pc->v_inv255_u16(xv, xv);
      pc->v_mul_u16(xv, xv, sv);

      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Dca.(1 - m.(1 - Sa)) + Sca.m.(1 - Da)
      // Da'  = Da .(1 - m.(1 - Sa)) + Sa .m.(1 - Da)
      srcFetch(s, PixelFlags::kUC | PixelFlags::kUIA, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& uv = s.uia;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_mul_u16(sv, sv, vm);
      pc->v_mul_u16(uv, uv, vm);

      pc->v_div255_u16(sv);
      pc->v_div255_u16(uv);
      pc->v_inv255_u16(xv, xv);
      pc->v_inv255_u16(uv, uv);
      pc->v_mul_u16(xv, xv, sv);
      pc->v_mul_u16(dv, dv, uv);

      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Xor
  // ------------------------

  if (compOp() == BL_COMP_OP_XOR) {
    // Composition:
    //   Da - Required.
    //   Sa - Required.

    if (!hasMask) {
      // Dca' = Dca.(1 - Sa) + Sca.(1 - Da)
      // Da'  = Da .(1 - Sa) + Sa .(1 - Da)
      srcFetch(s, PixelFlags::kUC | PixelFlags::kUIA | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& uv = s.uia;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_mul_u16(dv, dv, uv);
      pc->v_inv255_u16(xv, xv);
      pc->v_mul_u16(xv, xv, sv);

      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Dca.(1 - Sa.m) + Sca.m.(1 - Da)
      // Da'  = Da .(1 - Sa.m) + Sa .m.(1 - Da)
      srcFetch(s, PixelFlags::kUC, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->vExpandAlpha16(yv, dv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_inv255_u16(yv, yv);
      pc->v_mul_u16(dv, dv, xv);
      pc->v_mul_u16(sv, sv, yv);

      pc->v_add_i16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Plus
  // -------------------------

  if (compOp() == BL_COMP_OP_PLUS) {
    if (!hasMask) {
      // Dca' = Clamp(Dca + Sca)
      // Da'  = Clamp(Da  + Sa )
      srcFetch(s, PixelFlags::kPC | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kPC, n);

      VecArray& sh = s.pc;
      VecArray& dh = d.pc;

      pc->v_adds_u8(dh, dh, sh);
      out.pc.init(dh);
    }
    else {
      // Dca' = Clamp(Dca + Sca.m)
      // Da'  = Clamp(Da  + Sa .m)
      srcFetch(s, PixelFlags::kUC, n);
      dstFetch(d, PixelFlags::kPC, n);

      VecArray& sv = s.uc;
      VecArray& dh = d.pc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      VecArray sh = sv.even();
      pc->v_packs_i16_u8(sh, sh, sv.odd());
      pc->v_adds_u8(dh, dh, sh);

      out.pc.init(dh);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Minus
  // --------------------------

  if (compOp() == BL_COMP_OP_MINUS) {
    if (!hasMask) {
      // Dca' = Clamp(Dca - Sca) + Sca.(1 - Da)
      // Da'  = Da + Sa.(1 - Da)
      if (useDa) {
        srcFetch(s, PixelFlags::kUC, n);
        dstFetch(d, PixelFlags::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->v_inv255_u16(xv, xv);
        pc->v_mul_u16(xv, xv, sv);
        pc->vZeroAlphaW(sv, sv);
        pc->v_div255_u16(xv);

        pc->v_subs_u16(dv, dv, sv);
        pc->v_add_i16(dv, dv, xv);
        out.uc.init(dv);
      }
      // Dca' = Clamp(Dca - Sca)
      // Da'  = <unchanged>
      else {
        srcFetch(s, PixelFlags::kPC, n);
        dstFetch(d, PixelFlags::kPC, n);

        VecArray& sh = s.pc;
        VecArray& dh = d.pc;

        pc->vZeroAlphaB(sh, sh);
        pc->v_subs_u8(dh, dh, sh);

        out.pc.init(dh);
      }
    }
    else {
      // Dca' = (Clamp(Dca - Sca) + Sca.(1 - Da)).m + Dca.(1 - m)
      // Da'  = Da + Sa.m(1 - Da)
      if (useDa) {
        srcFetch(s, PixelFlags::kUC, n);
        dstFetch(d, PixelFlags::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->v_mov(yv, dv);
        pc->v_inv255_u16(xv, xv);
        pc->v_subs_u16(dv, dv, sv);
        pc->v_mul_u16(sv, sv, xv);

        pc->vZeroAlphaW(dv, dv);
        pc->v_div255_u16(sv);
        pc->v_add_i16(dv, dv, sv);
        pc->v_mul_u16(dv, dv, vm);

        pc->vZeroAlphaW(vm, vm);
        pc->v_inv255_u16(vm, vm);

        pc->v_mul_u16(yv, yv, vm);

        if (mImmutable) {
          pc->v_inv255_u16(vm[0], vm[0]);
          pc->v_swizzle_i32(vm[0], vm[0], x86::shuffleImm(2, 2, 0, 0));
        }

        pc->v_add_i16(dv, dv, yv);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Clamp(Dca - Sca).m + Dca.(1 - m)
      // Da'  = <unchanged>
      else {
        srcFetch(s, PixelFlags::kUC, n);
        dstFetch(d, PixelFlags::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->v_inv255_u16(xv, vm);
        pc->vZeroAlphaW(sv, sv);

        pc->v_mul_u16(xv, xv, dv);
        pc->v_subs_u16(dv, dv, sv);
        pc->v_mul_u16(dv, dv, vm);

        pc->v_add_i16(dv, dv, xv);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Modulate
  // -----------------------------

  if (compOp() == BL_COMP_OP_MODULATE) {
    VecArray& dv = d.uc;
    VecArray& sv = s.uc;

    if (!hasMask) {
      // Dca' = Dca.Sca
      // Da'  = Da .Sa
      srcFetch(s, PixelFlags::kUC | PixelFlags::kImmutable, n);
      dstFetch(d, PixelFlags::kUC, n);

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
    }
    else {
      // Dca' = Dca.(Sca.m + 1 - m)
      // Da'  = Da .(Sa .m + 1 - m)
      srcFetch(s, PixelFlags::kUC, n);
      dstFetch(d, PixelFlags::kUC, n);

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
      pc->v_add_i16(sv, sv, C_MEM(i128_00FF00FF00FF00FF));
      pc->v_sub_i16(sv, sv, vm);
      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);

      out.uc.init(dv);
    }

    if (!useDa)
      pc->vFillAlpha255W(dv, dv);

    out.uc.init(dv);
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Multiply
  // -----------------------------

  if (compOp() == BL_COMP_OP_MULTIPLY) {
    if (!hasMask) {
      if (useDa && useSa) {
        // Dca' = Dca.(Sca + 1 - Sa) + Sca.(1 - Da)
        // Da'  = Da .(Sa  + 1 - Sa) + Sa .(1 - Da)
        srcFetch(s, PixelFlags::kUC | PixelFlags::kImmutable, n);
        dstFetch(d, PixelFlags::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        // SPLIT.
        for (unsigned int i = 0; i < kSplit; i++) {
          VecArray sh = sv.even_odd(i);
          VecArray dh = dv.even_odd(i);
          VecArray xh = xv.even_odd(i);
          VecArray yh = yv.even_odd(i);

          pc->vExpandAlpha16(yh, sh, kUseHi);
          pc->vExpandAlpha16(xh, dh, kUseHi);
          pc->v_inv255_u16(yh, yh);
          pc->v_add_i16(yh, yh, sh);
          pc->v_inv255_u16(xh, xh);
          pc->v_mul_u16(dh, dh, yh);
          pc->v_mul_u16(xh, xh, sh);
          pc->v_add_i16(dh, dh, xh);
        }

        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      else if (useDa) {
        // Dca' = Sc.(Dca + 1 - Da)
        // Da'  = 1 .(Da  + 1 - Da) = 1
        srcFetch(s, PixelFlags::kUC | PixelFlags::kImmutable, n);
        dstFetch(d, PixelFlags::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->v_inv255_u16(xv, xv);
        pc->v_add_i16(dv, dv, xv);
        pc->v_mul_u16(dv, dv, sv);

        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      else if (hasSa()) {
        // Dc'  = Dc.(Sca + 1 - Sa)
        // Da'  = Da.(Sa  + 1 - Sa)
        srcFetch(s, PixelFlags::kUC | PixelFlags::kImmutable, n);
        dstFetch(d, PixelFlags::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, sv, kUseHi);
        pc->v_inv255_u16(xv, xv);
        pc->v_add_i16(xv, xv, sv);
        pc->v_mul_u16(dv, dv, xv);

        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      else {
        // Dc' = Dc.Sc
        srcFetch(s, PixelFlags::kUC | PixelFlags::kImmutable, n);
        dstFetch(d, PixelFlags::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->v_mul_u16(dv, dv, sv);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
    }
    else {
      if (useDa) {
        // Dca' = Dca.(Sca.m + 1 - Sa.m) + Sca.m(1 - Da)
        // Da'  = Da .(Sa .m + 1 - Sa.m) + Sa .m(1 - Da)
        srcFetch(s, PixelFlags::kUC, n);
        dstFetch(d, PixelFlags::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->v_mul_u16(sv, sv, vm);
        pc->v_div255_u16(sv);

        // SPLIT.
        for (unsigned int i = 0; i < kSplit; i++) {
          VecArray sh = sv.even_odd(i);
          VecArray dh = dv.even_odd(i);
          VecArray xh = xv.even_odd(i);
          VecArray yh = yv.even_odd(i);

          pc->vExpandAlpha16(yh, sh, kUseHi);
          pc->vExpandAlpha16(xh, dh, kUseHi);
          pc->v_inv255_u16(yh, yh);
          pc->v_add_i16(yh, yh, sh);
          pc->v_inv255_u16(xh, xh);
          pc->v_mul_u16(dh, dh, yh);
          pc->v_mul_u16(xh, xh, sh);
          pc->v_add_i16(dh, dh, xh);
        }

        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      else {
        srcFetch(s, PixelFlags::kUC, n);
        dstFetch(d, PixelFlags::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->v_mul_u16(sv, sv, vm);
        pc->v_div255_u16(sv);

        pc->vExpandAlpha16(xv, sv, kUseHi);
        pc->v_inv255_u16(xv, xv);
        pc->v_add_i16(xv, xv, sv);
        pc->v_mul_u16(dv, dv, xv);

        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Overlay
  // ----------------------------

  if (compOp() == BL_COMP_OP_OVERLAY) {
    srcFetch(s, PixelFlags::kUC, n);
    dstFetch(d, PixelFlags::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
      useSa = true;
    }

    if (useSa) {
      // if (2.Dca < Da)
      //   Dca' = Dca + Sca - (Dca.Sa + Sca.Da - 2.Sca.Dca)
      //   Da'  = Da  + Sa  - (Da .Sa + Sa .Da - 2.Sa .Da ) - Sa.Da
      //   Da'  = Da  + Sa  - Sa.Da
      // else
      //   Dca' = Dca + Sca + (Dca.Sa + Sca.Da - 2.Sca.Dca) - Sa.Da
      //   Da'  = Da  + Sa  + (Da .Sa + Sa .Da - 2.Sa .Da ) - Sa.Da
      //   Da'  = Da  + Sa  - Sa.Da

      for (unsigned int i = 0; i < kSplit; i++) {
        VecArray sh = sv.even_odd(i);
        VecArray dh = dv.even_odd(i);

        VecArray xh = xv.even_odd(i);
        VecArray yh = yv.even_odd(i);
        VecArray zh = zv.even_odd(i);

        if (!useDa)
          pc->vFillAlpha255W(dh, dh);

        pc->vExpandAlpha16(xh, dh, kUseHi);
        pc->vExpandAlpha16(yh, sh, kUseHi);

        pc->v_mul_u16(xh, xh, sh);                                 // Sca.Da
        pc->v_mul_u16(yh, yh, dh);                                 // Dca.Sa
        pc->v_mul_u16(zh, dh, sh);                                 // Dca.Sca

        pc->v_add_i16(sh, sh, dh);                                 // Dca + Sca
        pc->v_sub_i16(xh, xh, zh);                                 // Sca.Da - Dca.Sca
        pc->vZeroAlphaW(zh, zh);
        pc->v_add_i16(xh, xh, yh);                                 // Dca.Sa + Sca.Da - Dca.Sca
        pc->vExpandAlpha16(yh, dh, kUseHi);                        // Da
        pc->v_sub_i16(xh, xh, zh);                                 // [C=Dca.Sa + Sca.Da - 2.Dca.Sca] [A=Sa.Da]

        pc->v_sll_i16(dh, dh, 1);                                  // 2.Dca
        pc->v_cmp_gt_i16(yh, yh, dh);                              // 2.Dca < Da
        pc->v_div255_u16(xh);
        pc->v_or(yh, yh, C_MEM(i128_FFFF000000000000));

        pc->vExpandAlpha16(zh, xh, kUseHi);
        // if (2.Dca < Da)
        //   X = [C = -(Dca.Sa + Sca.Da - 2.Sca.Dca)] [A = -Sa.Da]
        // else
        //   X = [C =  (Dca.Sa + Sca.Da - 2.Sca.Dca)] [A = -Sa.Da]
        pc->v_xor(xh, xh, yh);
        pc->v_sub_i16(xh, xh, yh);

        // if (2.Dca < Da)
        //   Y = [C = 0] [A = 0]
        // else
        //   Y = [C = Sa.Da] [A = 0]
        pc->v_nand(yh, yh, zh);

        pc->v_add_i16(sh, sh, xh);
        pc->v_sub_i16(sh, sh, yh);
      }

      out.uc.init(sv);
    }
    else if (useDa) {
      // if (2.Dca < Da)
      //   Dca' = Sc.(1 + 2.Dca - Da)
      //   Da'  = 1
      // else
      //   Dca' = 2.Dca - Da + Sc.(1 - (2.Dca - Da))
      //   Da'  = 1

      pc->vExpandAlpha16(xv, dv, kUseHi);                          // Da
      pc->v_sll_i16(dv, dv, 1);                                    // 2.Dca

      pc->v_cmp_gt_i16(yv, xv, dv);                                //  (2.Dca < Da) ? -1 : 0
      pc->v_sub_i16(xv, xv, dv);                                   // -(2.Dca - Da)

      pc->v_xor(xv, xv, yv);
      pc->v_sub_i16(xv, xv, yv);                                   // 2.Dca < Da ? 2.Dca - Da : -(2.Dca - Da)
      pc->v_nand(yv, yv, xv);                                      // 2.Dca < Da ? 0          : -(2.Dca - Da)
      pc->v_add_i16(xv, xv, C_MEM(i128_00FF00FF00FF00FF));

      pc->v_mul_u16(xv, xv, sv);
      pc->v_div255_u16(xv);
      pc->v_sub_i16(xv, xv, yv);

      out.uc.init(xv);
    }
    else {
      // if (2.Dc < 1)
      //   Dc'  = 2.Dc.Sc
      // else
      //   Dc'  = 2.Dc + 2.Sc - 1 - 2.Dc.Sc

      pc->v_mul_u16(xv, dv, sv);                                   // Dc.Sc
      pc->v_cmp_gt_i16(yv, dv, C_MEM(i128_007F007F007F007F));      // !(2.Dc < 1)
      pc->v_add_i16(dv, dv, sv);                                   // Dc + Sc
      pc->v_div255_u16(xv);

      pc->v_sll_i16(dv, dv, 1);                                    // 2.Dc + 2.Sc
      pc->v_sll_i16(xv, xv, 1);                                    // 2.Dc.Sc
      pc->v_sub_i16(dv, dv, C_MEM(i128_00FF00FF00FF00FF));         // 2.Dc + 2.Sc - 1

      pc->v_xor(xv, xv, yv);
      pc->v_and(dv, dv, yv);                                       // 2.Dc < 1 ? 0 : 2.Dc + 2.Sc - 1
      pc->v_sub_i16(xv, xv, yv);                                   // 2.Dc < 1 ? 2.Dc.Sc : -2.Dc.Sc
      pc->v_add_i16(dv, dv, xv);                                   // 2.Dc < 1 ? 2.Dc.Sc : 2.Dc + 2.Sc - 1 - 2.Dc.Sc

      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Screen
  // ---------------------------

  if (compOp() == BL_COMP_OP_SCREEN) {
    // Dca' = Sca + Dca.(1 - Sca)
    // Da'  = Sa  + Da .(1 - Sa)
    srcFetch(s, PixelFlags::kUC | (hasMask ? PixelFlags::kNone : PixelFlags::kImmutable), n);
    dstFetch(d, PixelFlags::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
    }

    pc->v_inv255_u16(xv, sv);
    pc->v_mul_u16(dv, dv, xv);
    pc->v_div255_u16(dv);
    pc->v_add_i16(dv, dv, sv);

    out.uc.init(dv);
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Darken & Lighten
  // -------------------------------------

  if (compOp() == BL_COMP_OP_DARKEN || compOp() == BL_COMP_OP_LIGHTEN) {
    srcFetch(s, PixelFlags::kUC, n);
    dstFetch(d, PixelFlags::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    bool minMaxPredicate = compOp() == BL_COMP_OP_DARKEN;

    if (hasMask) {
      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
      useSa = true;
    }

    if (useSa && useDa) {
      // Dca' = minmax(Dca + Sca.(1 - Da), Sca + Dca.(1 - Sa))
      // Da'  = Sa + Da.(1 - Sa)
      for (unsigned int i = 0; i < kSplit; i++) {
        VecArray sh = sv.even_odd(i);
        VecArray dh = dv.even_odd(i);
        VecArray xh = xv.even_odd(i);
        VecArray yh = yv.even_odd(i);

        pc->vExpandAlpha16(xh, dh, kUseHi);
        pc->vExpandAlpha16(yh, sh, kUseHi);

        pc->v_inv255_u16(xh, xh);
        pc->v_inv255_u16(yh, yh);

        pc->v_mul_u16(xh, xh, sh);
        pc->v_mul_u16(yh, yh, dh);
        pc->v_div255_u16_2x(xh, yh);

        pc->v_add_i16(dh, dh, xh);
        pc->v_add_i16(sh, sh, yh);

        pc->vminmaxu8(dh, dh, sh, minMaxPredicate);
      }

      out.uc.init(dv);
    }
    else if (useDa) {
      // Dca' = minmax(Dca + Sc.(1 - Da), Sc)
      // Da'  = 1
      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_mul_u16(xv, xv, sv);
      pc->v_div255_u16(xv);
      pc->v_add_i16(dv, dv, xv);
      pc->vminmaxu8(dv, dv, sv, minMaxPredicate);

      out.uc.init(dv);
    }
    else if (useSa) {
      // Dc' = minmax(Dc, Sca + Dc.(1 - Sa))
      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_mul_u16(xv, xv, dv);
      pc->v_div255_u16(xv);
      pc->v_add_i16(xv, xv, sv);
      pc->vminmaxu8(dv, dv, xv, minMaxPredicate);

      out.uc.init(dv);
    }
    else {
      // Dc' = minmax(Dc, Sc)
      pc->vminmaxu8(dv, dv, sv, minMaxPredicate);

      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - ColorDodge (SCALAR)
  // ----------------------------------------

  if (compOp() == BL_COMP_OP_COLOR_DODGE && n == 1) {
    // Dca' = min(Dca.Sa.Sa / max(Sa - Sca, 0.001), Sa.Da) + Sca.(1 - Da) + Dca.(1 - Sa);
    // Da'  = min(Da .Sa.Sa / max(Sa - Sa , 0.001), Sa.Da) + Sa .(1 - Da) + Da .(1 - Sa);

    srcFetch(s, PixelFlags::kUC, n);
    dstFetch(d, PixelFlags::kPC, n);

    x86::Vec& s0 = s.uc[0];
    x86::Vec& d0 = d.pc[0];
    x86::Vec& x0 = xv[0];
    x86::Vec& y0 = yv[0];
    x86::Vec& z0 = zv[0];

    if (hasMask) {
      pc->v_mul_u16(s0, s0, vm[0]);
      pc->v_div255_u16(s0);
    }

    pc->vmovu8u32(d0, d0);
    pc->vmovu16u32(s0, s0);

    pc->v_cvt_i32_f32(y0, s0);
    pc->v_cvt_i32_f32(z0, d0);
    pc->v_packs_i32_i16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);
    pc->v_xor_f32(y0, y0, C_MEM(f128_sgn));
    pc->v_mul_f32(z0, z0, x0);
    pc->v_and_f32(y0, y0, C_MEM(i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0));
    pc->v_add_f32(y0, y0, x0);

    pc->v_max_f32(y0, y0, C_MEM(f128_1e_m3));
    pc->v_div_f32(z0, z0, y0);

    pc->v_swizzle_i32(s0, d0, x86::shuffleImm(1, 1, 3, 3));
    pc->vExpandAlphaHi16(s0, s0);
    pc->vExpandAlphaLo16(s0, s0);
    pc->v_inv255_u16(s0, s0);
    pc->v_mul_u16(d0, d0, s0);
    pc->v_swizzle_i32(s0, d0, x86::shuffleImm(1, 0, 3, 2));
    pc->v_add_i16(d0, d0, s0);

    pc->v_mul_f32(z0, z0, x0);
    pc->vExpandAlphaPS(x0, z0);
    pc->v_min_f32(z0, z0, x0);

    pc->v_cvtt_f32_i32(z0, z0);
    pc->xPackU32ToU16Lo(z0, z0);
    pc->v_add_i16(d0, d0, z0);

    pc->v_div255_u16(d0);
    out.uc.init(d0);

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - ColorBurn (SCALAR)
  // ---------------------------------------

  if (compOp() == BL_COMP_OP_COLOR_BURN && n == 1) {
    // Dca' = Sa.Da - min(Sa.Da, (Da - Dca).Sa.Sa / max(Sca, 0.001)) + Sca.(1 - Da) + Dca.(1 - Sa)
    // Da'  = Sa.Da - min(Sa.Da, (Da - Da ).Sa.Sa / max(Sa , 0.001)) + Sa .(1 - Da) + Da .(1 - Sa)
    srcFetch(s, PixelFlags::kUC, n);
    dstFetch(d, PixelFlags::kPC, n);

    x86::Vec& s0 = s.uc[0];
    x86::Vec& d0 = d.pc[0];
    x86::Vec& x0 = xv[0];
    x86::Vec& y0 = yv[0];
    x86::Vec& z0 = zv[0];

    if (hasMask) {
      pc->v_mul_u16(s0, s0, vm[0]);
      pc->v_div255_u16(s0);
    }

    pc->vmovu8u32(d0, d0);
    pc->vmovu16u32(s0, s0);

    pc->v_cvt_i32_f32(y0, s0);
    pc->v_cvt_i32_f32(z0, d0);
    pc->v_packs_i32_i16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);
    pc->v_max_f32(y0, y0, C_MEM(f128_1e_m3));
    pc->v_mul_f32(z0, z0, x0);                                     // Dca.Sa

    pc->vExpandAlphaPS(x0, z0);                                    // Sa.Da
    pc->v_xor_f32(z0, z0, C_MEM(f128_sgn));

    pc->v_and_f32(z0, z0, C_MEM(i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0));
    pc->v_add_f32(z0, z0, x0);                                     // (Da - Dxa).Sa
    pc->v_div_f32(z0, z0, y0);

    pc->v_swizzle_i32(s0, d0, x86::shuffleImm(1, 1, 3, 3));
    pc->vExpandAlphaHi16(s0, s0);
    pc->vExpandAlphaLo16(s0, s0);
    pc->v_inv255_u16(s0, s0);
    pc->v_mul_u16(d0, d0, s0);
    pc->v_swizzle_i32(s0, d0, x86::shuffleImm(1, 0, 3, 2));
    pc->v_add_i16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);                                    // Sa
    pc->v_mul_f32(z0, z0, x0);
    pc->vExpandAlphaPS(x0, z0);                                    // Sa.Da
    pc->v_min_f32(z0, z0, x0);
    pc->v_and_f32(z0, z0, C_MEM(i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0));
    pc->v_sub_f32(x0, x0, z0);

    pc->v_cvtt_f32_i32(x0, x0);
    pc->xPackU32ToU16Lo(x0, x0);
    pc->v_add_i16(d0, d0, x0);

    pc->v_div255_u16(d0);
    out.uc.init(d0);

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - LinearBurn
  // -------------------------------

  if (compOp() == BL_COMP_OP_LINEAR_BURN) {
    srcFetch(s, PixelFlags::kUC | (hasMask ? PixelFlags::kNone : PixelFlags::kImmutable), n);
    dstFetch(d, PixelFlags::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
    }

    if (useDa && useSa) {
      // Dca' = Dca + Sca - Sa.Da
      // Da'  = Da  + Sa  - Sa.Da
      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->vExpandAlpha16(yv, dv, kUseHi);
      pc->v_mul_u16(xv, xv, yv);
      pc->v_div255_u16(xv);
      pc->v_add_i16(dv, dv, sv);
      pc->v_subs_u16(dv, dv, xv);
    }
    else if (useDa || useSa) {
      pc->vExpandAlpha16(xv, useDa ? dv : sv, kUseHi);
      pc->v_add_i16(dv, dv, sv);
      pc->v_subs_u16(dv, dv, xv);
    }
    else {
      // Dca' = Dc + Sc - 1
      pc->v_add_i16(dv, dv, sv);
      pc->v_subs_u16(dv, dv, C_MEM(i128_000000FF00FF00FF));
    }

    out.uc.init(dv);
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - LinearLight
  // --------------------------------

  if (compOp() == BL_COMP_OP_LINEAR_LIGHT && n == 1) {
    srcFetch(s, PixelFlags::kUC, 1);
    dstFetch(d, PixelFlags::kUC, 1);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
      useSa = 1;
    }

    if (useSa || useDa) {
      // Dca' = min(max((Dca.Sa + 2.Sca.Da - Sa.Da), 0), Sa.Da) + Sca.(1 - Da) + Dca.(1 - Sa)
      // Da'  = min(max((Da .Sa + 2.Sa .Da - Sa.Da), 0), Sa.Da) + Sa .(1 - Da) + Da .(1 - Sa)

      x86::Vec& d0 = dv[0];
      x86::Vec& s0 = sv[0];
      x86::Vec& x0 = xv[0];
      x86::Vec& y0 = yv[0];

      pc->vExpandAlphaLo16(y0, d0);
      pc->vExpandAlphaLo16(x0, s0);

      pc->v_interleave_lo_i64(d0, d0, s0);
      pc->v_interleave_lo_i64(x0, x0, y0);

      pc->v_mov(s0, d0);
      pc->v_mul_u16(d0, d0, x0);
      pc->v_inv255_u16(x0, x0);
      pc->v_div255_u16(d0);

      pc->v_mul_u16(s0, s0, x0);
      pc->v_swap_i64(x0, s0);
      pc->v_swap_i64(y0, d0);
      pc->v_add_i16(s0, s0, x0);
      pc->v_add_i16(d0, d0, y0);
      pc->vExpandAlphaLo16(x0, y0);
      pc->v_add_i16(d0, d0, y0);
      pc->v_div255_u16(s0);

      pc->v_subs_u16(d0, d0, x0);
      pc->v_min_i16(d0, d0, x0);

      pc->v_add_i16(d0, d0, s0);
      out.uc.init(d0);
    }
    else {
      // Dc' = min(max((Dc + 2.Sc - 1), 0), 1)
      pc->v_sll_i16(sv, sv, 1);
      pc->v_add_i16(dv, dv, sv);
      pc->v_subs_u16(dv, dv, C_MEM(i128_000000FF00FF00FF));
      pc->v_min_i16(dv, dv, C_MEM(i128_00FF00FF00FF00FF));

      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - PinLight
  // -----------------------------

  if (compOp() == BL_COMP_OP_PIN_LIGHT) {
    srcFetch(s, PixelFlags::kUC, n);
    dstFetch(d, PixelFlags::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      useSa = true;
    }

    if (useSa && useDa) {
      // if 2.Sca <= Sa
      //   Dca' = min(Dca + Sca - Sca.Da, Dca + Sca + Sca.Da - Dca.Sa)
      //   Da'  = min(Da  + Sa  - Sa .Da, Da  + Sa  + Sa .Da - Da .Sa) = Da + Sa.(1 - Da)
      // else
      //   Dca' = max(Dca + Sca - Sca.Da, Dca + Sca + Sca.Da - Dca.Sa - Da.Sa)
      //   Da'  = max(Da  + Sa  - Sa .Da, Da  + Sa  + Sa .Da - Da .Sa - Da.Sa) = Da + Sa.(1 - Da)

      pc->vExpandAlpha16(yv, sv, kUseHi);                          // Sa
      pc->vExpandAlpha16(xv, dv, kUseHi);                          // Da

      pc->v_mul_u16(yv, yv, dv);                                   // Dca.Sa
      pc->v_mul_u16(xv, xv, sv);                                   // Sca.Da
      pc->v_add_i16(dv, dv, sv);                                   // Dca + Sca
      pc->v_div255_u16_2x(yv, xv);

      pc->v_sub_i16(yv, yv, dv);                                   // Dca.Sa - Dca - Sca
      pc->v_sub_i16(dv, dv, xv);                                   // Dca + Sca - Sca.Da
      pc->v_sub_i16(xv, xv, yv);                                   // Dca + Sca + Sca.Da - Dca.Sa

      pc->vExpandAlpha16(yv, sv, kUseHi);                          // Sa
      pc->v_sll_i16(sv, sv, 1);                                    // 2.Sca
      pc->v_cmp_gt_i16(sv, sv, yv);                                // !(2.Sca <= Sa)

      pc->v_sub_i16(zv, dv, xv);
      pc->vExpandAlpha16(zv, zv, kUseHi);                          // -Da.Sa
      pc->v_and(zv, zv, sv);                                       // 2.Sca <= Sa ? 0 : -Da.Sa
      pc->v_add_i16(xv, xv, zv);                                   // 2.Sca <= Sa ? Dca + Sca + Sca.Da - Dca.Sa : Dca + Sca + Sca.Da - Dca.Sa - Da.Sa

      // if 2.Sca <= Sa:
      //   min(dv, xv)
      // else
      //   max(dv, xv) <- ~min(~dv, ~xv)
      pc->v_xor(dv, dv, sv);
      pc->v_xor(xv, xv, sv);
      pc->v_min_i16(dv, dv, xv);
      pc->v_xor(dv, dv, sv);

      out.uc.init(dv);
    }
    else if (useDa) {
      // if 2.Sc <= 1
      //   Dca' = min(Dca + Sc - Sc.Da, Sc + Sc.Da)
      //   Da'  = min(Da  + 1  - 1 .Da, 1  + 1 .Da) = 1
      // else
      //   Dca' = max(Dca + Sc - Sc.Da, Sc + Sc.Da - Da)
      //   Da'  = max(Da  + 1  - 1 .Da, 1  + 1 .Da - Da) = 1

      pc->vExpandAlpha16(xv, dv, kUseHi);                          // Da
      pc->v_mul_u16(xv, xv, sv);                                   // Sc.Da
      pc->v_add_i16(dv, dv, sv);                                   // Dca + Sc
      pc->v_div255_u16(xv);

      pc->v_cmp_gt_i16(yv, sv, C_MEM(i128_007F007F007F007F));      // !(2.Sc <= 1)
      pc->v_add_i16(sv, sv, xv);                                   // Sc + Sc.Da
      pc->v_sub_i16(dv, dv, xv);                                   // Dca + Sc - Sc.Da
      pc->vExpandAlpha16(xv, xv);                                  // Da
      pc->v_and(xv, xv, yv);                                       // 2.Sc <= 1 ? 0 : Da
      pc->v_sub_i16(sv, sv, xv);                                   // 2.Sc <= 1 ? Sc + Sc.Da : Sc + Sc.Da - Da

      // if 2.Sc <= 1:
      //   min(dv, sv)
      // else
      //   max(dv, sv) <- ~min(~dv, ~sv)
      pc->v_xor(dv, dv, yv);
      pc->v_xor(sv, sv, yv);
      pc->v_min_i16(dv, dv, sv);
      pc->v_xor(dv, dv, yv);

      out.uc.init(dv);
    }
    else if (useSa) {
      // if 2.Sca <= Sa
      //   Dc' = min(Dc, Dc + 2.Sca - Dc.Sa)
      // else
      //   Dc' = max(Dc, Dc + 2.Sca - Dc.Sa - Sa)

      pc->vExpandAlpha16(xv, sv, kUseHi);                          // Sa
      pc->v_sll_i16(sv, sv, 1);                                    // 2.Sca
      pc->v_cmp_gt_i16(yv, sv, xv);                                // !(2.Sca <= Sa)
      pc->v_and(yv, yv, xv);                                       // 2.Sca <= Sa ? 0 : Sa
      pc->v_mul_u16(xv, xv, dv);                                   // Dc.Sa
      pc->v_add_i16(sv, sv, dv);                                   // Dc + 2.Sca
      pc->v_div255_u16(xv);
      pc->v_sub_i16(sv, sv, yv);                                   // 2.Sca <= Sa ? Dc + 2.Sca : Dc + 2.Sca - Sa
      pc->v_cmp_eq_i16(yv, yv, C_MEM(i128_zero));      // 2.Sc <= 1
      pc->v_sub_i16(sv, sv, xv);                                   // 2.Sca <= Sa ? Dc + 2.Sca - Dc.Sa : Dc + 2.Sca - Dc.Sa - Sa

      // if 2.Sc <= 1:
      //   min(dv, sv)
      // else
      //   max(dv, sv) <- ~min(~dv, ~sv)
      pc->v_xor(dv, dv, yv);
      pc->v_xor(sv, sv, yv);
      pc->v_max_i16(dv, dv, sv);
      pc->v_xor(dv, dv, yv);

      out.uc.init(dv);
    }
    else {
      // if 2.Sc <= 1
      //   Dc' = min(Dc, 2.Sc)
      // else
      //   Dc' = max(Dc, 2.Sc - 1)

      pc->v_sll_i16(sv, sv, 1);                                    // 2.Sc
      pc->v_min_i16(xv, sv, dv);                                   // min(Dc, 2.Sc)

      pc->v_cmp_gt_i16(yv, sv, C_MEM(i128_00FF00FF00FF00FF));      // !(2.Sc <= 1)
      pc->v_sub_i16(sv, sv, C_MEM(i128_00FF00FF00FF00FF));         // 2.Sc - 1
      pc->v_max_i16(dv, dv, sv);                                   // max(Dc, 2.Sc - 1)

      pc->v_blendv_u8_destructive(xv, xv, dv, yv);                 // 2.Sc <= 1 ? min(Dc, 2.Sc) : max(Dc, 2.Sc - 1)
      out.uc.init(xv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - HardLight
  // ------------------------------

  if (compOp() == BL_COMP_OP_HARD_LIGHT) {
    // if (2.Sca < Sa)
    //   Dca' = Dca + Sca - (Dca.Sa + Sca.Da - 2.Sca.Dca)
    //   Da'  = Da  + Sa  - Sa.Da
    // else
    //   Dca' = Dca + Sca + (Dca.Sa + Sca.Da - 2.Sca.Dca) - Sa.Da
    //   Da'  = Da  + Sa  - Sa.Da
    srcFetch(s, PixelFlags::kUC, n);
    dstFetch(d, PixelFlags::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
    }

    // SPLIT.
    for (unsigned int i = 0; i < kSplit; i++) {
      VecArray sh = sv.even_odd(i);
      VecArray dh = dv.even_odd(i);
      VecArray xh = xv.even_odd(i);
      VecArray yh = yv.even_odd(i);
      VecArray zh = zv.even_odd(i);

      pc->vExpandAlpha16(xh, dh, kUseHi);
      pc->vExpandAlpha16(yh, sh, kUseHi);

      pc->v_mul_u16(xh, xh, sh);                                   // Sca.Da
      pc->v_mul_u16(yh, yh, dh);                                   // Dca.Sa
      pc->v_mul_u16(zh, dh, sh);                                   // Dca.Sca

      pc->v_add_i16(dh, dh, sh);
      pc->v_sub_i16(xh, xh, zh);
      pc->v_add_i16(xh, xh, yh);
      pc->v_sub_i16(xh, xh, zh);

      pc->vExpandAlpha16(yh, yh, kUseHi);
      pc->vExpandAlpha16(zh, sh, kUseHi);
      pc->v_div255_u16_2x(xh, yh);

      pc->v_sll_i16(sh, sh, 1);
      pc->v_cmp_gt_i16(zh, zh, sh);

      pc->v_xor(xh, xh, zh);
      pc->v_sub_i16(xh, xh, zh);
      pc->vZeroAlphaW(zh, zh);
      pc->v_nand(zh, zh, yh);
      pc->v_add_i16(dh, dh, xh);
      pc->v_sub_i16(dh, dh, zh);
    }

    out.uc.init(dv);
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - SoftLight (SCALAR)
  // ---------------------------------------

  if (compOp() == BL_COMP_OP_SOFT_LIGHT && n == 1) {
    // Dc = Dca/Da
    //
    // Dca' =
    //   if 2.Sca - Sa <= 0
    //     Dca + Sca.(1 - Da) + (2.Sca - Sa).Da.[[              Dc.(1 - Dc)           ]]
    //   else if 2.Sca - Sa > 0 and 4.Dc <= 1
    //     Dca + Sca.(1 - Da) + (2.Sca - Sa).Da.[[ 4.Dc.(4.Dc.Dc + Dc - 4.Dc + 1) - Dc]]
    //   else
    //     Dca + Sca.(1 - Da) + (2.Sca - Sa).Da.[[             sqrt(Dc) - Dc          ]]
    // Da'  = Da + Sa - Sa.Da
    srcFetch(s, PixelFlags::kUC, n);
    dstFetch(d, PixelFlags::kPC, n);

    x86::Vec& s0 = s.uc[0];
    x86::Vec& d0 = d.pc[0];

    x86::Vec  a0 = cc->newXmm("a0");
    x86::Vec  b0 = cc->newXmm("b0");
    x86::Vec& x0 = xv[0];
    x86::Vec& y0 = yv[0];
    x86::Vec& z0 = zv[0];

    if (hasMask) {
      pc->v_mul_u16(s0, s0, vm[0]);
      pc->v_div255_u16(s0);
    }

    pc->vmovu8u32(d0, d0);
    pc->vmovu16u32(s0, s0);
    pc->v_loada_f128(x0, C_MEM(f128_1div255));

    pc->v_cvt_i32_f32(s0, s0);
    pc->v_cvt_i32_f32(d0, d0);

    pc->v_mul_f32(s0, s0, x0);                                     // Sca (0..1)
    pc->v_mul_f32(d0, d0, x0);                                     // Dca (0..1)

    pc->vExpandAlphaPS(b0, d0);                                    // Da
    pc->v_mul_f32(x0, s0, b0);                                     // Sca.Da
    pc->v_max_f32(b0, b0, C_MEM(f128_1e_m3));                      // max(Da, 0.001)

    pc->v_div_f32(a0, d0, b0);                                     // Dc <- Dca/Da
    pc->v_add_f32(d0, d0, s0);                                     // Dca + Sca

    pc->vExpandAlphaPS(y0, s0);                                    // Sa
    pc->v_loada_f128(z0, C_MEM(f128_4));                           // 4

    pc->v_sub_f32(d0, d0, x0);                                     // Dca + Sca.(1 - Da)
    pc->v_add_f32(s0, s0, s0);                                     // 2.Sca
    pc->v_mul_f32(z0, z0, a0);                                     // 4.Dc

    pc->v_sqrt_f32(x0, a0);                                        // sqrt(Dc)
    pc->v_sub_f32(s0, s0, y0);                                     // 2.Sca - Sa

    pc->vmovaps(y0, z0);                                           // 4.Dc
    pc->v_mul_f32(z0, z0, a0);                                     // 4.Dc.Dc

    pc->v_add_f32(z0, z0, a0);                                     // 4.Dc.Dc + Dc
    pc->v_mul_f32(s0, s0, b0);                                     // (2.Sca - Sa).Da

    pc->v_sub_f32(z0, z0, y0);                                     // 4.Dc.Dc + Dc - 4.Dc
    pc->v_loada_f128(b0, C_MEM(f128_1));                           // 1

    pc->v_add_f32(z0, z0, b0);                                     // 4.Dc.Dc + Dc - 4.Dc + 1
    pc->v_mul_f32(z0, z0, y0);                                     // 4.Dc(4.Dc.Dc + Dc - 4.Dc + 1)
    pc->v_cmp_f32(y0, y0, b0, x86::VCmpImm::kLE_OS);               // 4.Dc <= 1

    pc->v_and_f32(z0, z0, y0);
    pc->v_nand_f32(y0, y0, x0);

    pc->v_zero_f(x0);
    pc->v_or_f32(z0, z0, y0);                                      // (4.Dc(4.Dc.Dc + Dc - 4.Dc + 1)) or sqrt(Dc)

    pc->v_cmp_f32(x0, x0, s0, x86::VCmpImm::kLT_OS);               // 2.Sca - Sa > 0
    pc->v_sub_f32(z0, z0, a0);                                     // [[4.Dc(4.Dc.Dc + Dc - 4.Dc + 1) or sqrt(Dc)]] - Dc

    pc->v_sub_f32(b0, b0, a0);                                     // 1 - Dc
    pc->v_and_f32(z0, z0, x0);

    pc->v_mul_f32(b0, b0, a0);                                     // Dc.(1 - Dc)
    pc->v_nand_f32(x0, x0, b0);
    pc->v_and_f32(s0, s0, C_MEM(i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0)); // Zero alpha.

    pc->v_or_f32(z0, z0, x0);
    pc->v_mul_f32(s0, s0, z0);

    pc->v_add_f32(d0, d0, s0);
    pc->v_mul_f32(d0, d0, C_MEM(f128_255));

    pc->v_cvt_f32_i32(d0, d0);
    pc->v_packs_i32_i16(d0, d0, d0);
    pc->v_packs_i16_u8(d0, d0, d0);
    out.pc.init(d0);

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Difference
  // -------------------------------

  if (compOp() == BL_COMP_OP_DIFFERENCE) {
    // Dca' = Dca + Sca - 2.min(Sca.Da, Dca.Sa)
    // Da'  = Da  + Sa  -   min(Sa .Da, Da .Sa)
    if (!hasMask) {
      srcFetch(s, PixelFlags::kUC | PixelFlags::kUA, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& uv = s.ua;
      VecArray& dv = d.uc;

      // SPLIT.
      for (unsigned int i = 0; i < kSplit; i++) {
        VecArray sh = sv.even_odd(i);
        VecArray uh = uv.even_odd(i);
        VecArray dh = dv.even_odd(i);
        VecArray xh = xv.even_odd(i);

        pc->vExpandAlpha16(xh, dh, kUseHi);
        pc->v_mul_u16(uh, uh, dh);
        pc->v_mul_u16(xh, xh, sh);
        pc->v_add_i16(dh, dh, sh);
        pc->v_min_u16(uh, uh, xh);
      }

      pc->v_div255_u16(uv);
      pc->v_sub_i16(dv, dv, uv);

      pc->vZeroAlphaW(uv, uv);
      pc->v_sub_i16(dv, dv, uv);
      out.uc.init(dv);
    }
    // Dca' = Dca + Sca.m - 2.min(Sca.Da, Dca.Sa).m
    // Da'  = Da  + Sa .m -   min(Sa .Da, Da .Sa).m
    else {
      srcFetch(s, PixelFlags::kUC, n);
      dstFetch(d, PixelFlags::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      // SPLIT.
      for (unsigned int i = 0; i < kSplit; i++) {
        VecArray sh = sv.even_odd(i);
        VecArray dh = dv.even_odd(i);
        VecArray xh = xv.even_odd(i);
        VecArray yh = yv.even_odd(i);

        pc->vExpandAlpha16(yh, sh, kUseHi);
        pc->vExpandAlpha16(xh, dh, kUseHi);
        pc->v_mul_u16(yh, yh, dh);
        pc->v_mul_u16(xh, xh, sh);
        pc->v_add_i16(dh, dh, sh);
        pc->v_min_u16(yh, yh, xh);
      }

      pc->v_div255_u16(yv);
      pc->v_sub_i16(dv, dv, yv);

      pc->vZeroAlphaW(yv, yv);
      pc->v_sub_i16(dv, dv, yv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Exclusion
  // ------------------------------

  if (compOp() == BL_COMP_OP_EXCLUSION) {
    // Dca' = Dca + Sca - 2.Sca.Dca
    // Da'  = Da + Sa - Sa.Da
    srcFetch(s, PixelFlags::kUC | (hasMask ? PixelFlags::kNone : PixelFlags::kImmutable), n);
    dstFetch(d, PixelFlags::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
    }

    pc->v_mul_u16(xv, dv, sv);
    pc->v_add_i16(dv, dv, sv);
    pc->v_div255_u16(xv);
    pc->v_sub_i16(dv, dv, xv);

    pc->vZeroAlphaW(xv, xv);
    pc->v_sub_i16(dv, dv, xv);

    out.uc.init(dv);
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Invalid
  // ----------------------------

  BL_NOT_REACHED();
}

void CompOpPart::vMaskProcRGBA32InvertMask(VecArray& vn, VecArray& vm) noexcept {
  uint32_t i;
  uint32_t size = vm.size();

  if (cMaskLoopType() == CMaskLoopType::kVariant) {
    if (_mask->vn.isValid()) {
      bool ok = true;

      // TODO: [PIPEGEN] A leftover from a template-based code, I don't understand
      // it anymore and it seems it's unnecessary so verify this and all places
      // that hit `ok == false`.
      for (i = 0; i < blMin(vn.size(), size); i++)
        if (vn[i].id() != vm[i].id())
          ok = false;

      if (ok) {
        vn.init(_mask->vn);
        return;
      }
    }
  }

  if (vn.empty())
    pc->newVecArray(vn, size, "vn");

  pc->v_inv255_u16(vn, vm);
}

void CompOpPart::vMaskProcRGBA32InvertDone(VecArray& vn, bool mImmutable) noexcept {
  blUnused(mImmutable);

  if (cMaskLoopType() == CMaskLoopType::kVariant) {
    if (vn[0].id() == _mask->vm.id())
      pc->v_inv255_u16(vn, vn);
  }
}

} // {JIT}
} // {BLPipeline}

#endif
