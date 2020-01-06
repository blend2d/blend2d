// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../pipegen/compoppart_p.h"
#include "../pipegen/fetchpart_p.h"
#include "../pipegen/fetchpatternpart_p.h"
#include "../pipegen/fetchpixelptrpart_p.h"
#include "../pipegen/fetchsolidpart_p.h"
#include "../pipegen/pipecompiler_p.h"

#define C_MEM(CONST) pc->constAsMem(blCommonTable.CONST)

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::CompOpPart - Construction / Destruction]
// ============================================================================

CompOpPart::CompOpPart(PipeCompiler* pc, uint32_t compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept
  : PipePart(pc, kTypeComposite),
    _compOp(compOp),
    _pixelType(dstPart->hasRGB() ? Pixel::kTypeRGBA : Pixel::kTypeAlpha),
    _cMaskLoopType(kCMaskLoopTypeNone),
    _maxPixels(1),
    _pixelGranularity(0),
    _minAlignment(1),
    _isInPartialMode(false),
    _hasDa(dstPart->hasAlpha()),
    _hasSa(srcPart->hasAlpha()),
    _cMaskLoopHook(nullptr),
    _solidPre(_pixelType),
    _partialPixel(_pixelType) {

  // Initialize the children of this part.
  _children[kIndexDstPart] = dstPart;
  _children[kIndexSrcPart] = srcPart;
  _childrenCount = 2;

  _maxSimdWidthSupported = 16;

  bool isSolid = srcPart->isSolid();
  uint32_t maxPixels = 0;
  uint32_t pixelLimit = 64;

  // Limit the maximum pixel-step to 4 it the style is not solid and the target
  // is not 64-bit. There's not enough registers to process 8 pixels in parallel
  // in 32-bit mode.
  if (BL_TARGET_ARCH_BITS < 64 && !isSolid && _pixelType != Pixel::kTypeAlpha)
    pixelLimit = 4;

  // Decrease the maximum pixels to 4 if the source is complex to fetch.
  // In such case fetching and processing more pixels would result in
  // emitting bloated pipelines that are not faster compared to pipelines
  // working with just 4 pixels at a time.
  if (dstPart->isComplexFetch() || srcPart->isComplexFetch())
    pixelLimit = 4;

  switch (pixelType()) {
    case Pixel::kTypeRGBA:
      switch (compOp) {
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

    case Pixel::kTypeAlpha:
      maxPixels = 8;
      break;
  }

  // Descrease to N pixels at a time if the fetch part doesn't support more.
  // This is suboptimal, but can happen if the fetch part is not optimized.
  maxPixels = blMin(maxPixels, pixelLimit, srcPart->maxPixels());

  if (isRGBAType()) {
    if (maxPixels >= 4)
      _minAlignment = 16;
  }

  _maxPixels = uint8_t(maxPixels);
  _mask->reset();
}

// ============================================================================
// [BLPipeGen::CompOpPart - Init / Fini]
// ============================================================================

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

// ============================================================================
// [BLPipeGen::CompOpPart - Optimization Opportunities]
// ============================================================================

bool CompOpPart::shouldOptimizeOpaqueFill() const noexcept {
  // Should be always optimized if the source is not solid.
  if (!srcPart()->isSolid())
    return true;

  // Do not optimize if the CompOp is TypeA. This operator doesn't need any
  // special handling as the source pixel is multiplied with mask before it's
  // passed to the compositor.
  if (compOpFlags() & BL_COMP_OP_FLAG_TYPE_A)
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

  if (srcPart()->isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT) &&
      srcPart()->format() == dstPart()->format())
    return true;

  return false;
}

// ============================================================================
// [BLPipeGen::CompOpPart - Advance]
// ============================================================================

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

// ============================================================================
// [BLPipeGen::CompOpPart - Prefetch / Postfetch]
// ============================================================================

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

// ============================================================================
// [BLPipeGen::CompOpPart - Fetch]
// ============================================================================

void CompOpPart::dstFetch(Pixel& p, uint32_t flags, uint32_t n) noexcept {
  switch (n) {
    case 1: dstPart()->fetch1(p, flags); break;
    case 4: dstPart()->fetch4(p, flags); break;
    case 8: dstPart()->fetch8(p, flags); break;
  }
}

void CompOpPart::srcFetch(Pixel& p, uint32_t flags, uint32_t n) noexcept {
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
      if (flags & Pixel::kImmutable) {
        if (flags & Pixel::kPC) p.pc.init(s.pc[0]);
        if (flags & Pixel::kUC) p.uc.init(s.uc[0]);
        if (flags & Pixel::kUA) p.ua.init(s.ua[0]);
        if (flags & Pixel::kUIA) p.uia.init(s.uia[0]);
      }
      else {
        switch (n) {
          case 1:
            if (flags & Pixel::kPC) { p.pc.init(cc->newXmm("pre.pc")); pc->vmov(p.pc[0], s.pc[0]); }
            if (flags & Pixel::kUC) { p.uc.init(cc->newXmm("pre.uc")); pc->vmov(p.uc[0], s.uc[0]); }
            if (flags & Pixel::kUA) { p.ua.init(cc->newXmm("pre.ua")); pc->vmov(p.ua[0], s.ua[0]); }
            if (flags & Pixel::kUIA) { p.uia.init(cc->newXmm("pre.uia")); pc->vmov(p.uia[0], s.uia[0]); }
            break;

          case 4:
            if (flags & Pixel::kPC) {
              pc->newXmmArray(p.pc, 1, "pre.pc");
              pc->vmov(p.pc[0], s.pc[0]);
            }

            if (flags & Pixel::kUC) {
              pc->newXmmArray(p.uc, 2, "pre.uc");
              pc->vmov(p.uc[0], s.uc[0]);
              pc->vmov(p.uc[1], s.uc[0]);
            }

            if (flags & Pixel::kUA) {
              pc->newXmmArray(p.ua, 2, "pre.ua");
              pc->vmov(p.ua[0], s.ua[0]);
              pc->vmov(p.ua[1], s.ua[0]);
            }

            if (flags & Pixel::kUIA) {
              pc->newXmmArray(p.uia, 2, "pre.uia");
              pc->vmov(p.uia[0], s.uia[0]);
              pc->vmov(p.uia[1], s.uia[0]);
            }
            break;

          case 8:
            if (flags & Pixel::kPC) {
              pc->newXmmArray(p.pc, 2, "pre.pc");
              pc->vmov(p.pc[0], s.pc[0]);
              pc->vmov(p.pc[1], s.pc[0]);
            }

            if (flags & Pixel::kUC) {
              pc->newXmmArray(p.uc, 4, "pre.uc");
              pc->vmov(p.uc[0], s.uc[0]);
              pc->vmov(p.uc[1], s.uc[0]);
              pc->vmov(p.uc[2], s.uc[0]);
              pc->vmov(p.uc[3], s.uc[0]);
            }

            if (flags & Pixel::kUA) {
              pc->newXmmArray(p.ua, 4, "pre.ua");
              pc->vmov(p.ua[0], s.ua[0]);
              pc->vmov(p.ua[1], s.ua[0]);
              pc->vmov(p.ua[2], s.ua[0]);
              pc->vmov(p.ua[3], s.ua[0]);
            }

            if (flags & Pixel::kUIA) {
              pc->newXmmArray(p.uia, 4, "pre.uia");
              pc->vmov(p.uia[0], s.uia[0]);
              pc->vmov(p.uia[1], s.uia[0]);
              pc->vmov(p.uia[2], s.uia[0]);
              pc->vmov(p.uia[3], s.uia[0]);
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
      if (!(flags & Pixel::kImmutable)) {
        if (flags & Pixel::kUC) {
          pc->newXmmArray(p.uc, 1, "uc");
          pc->vmovu8u16(p.uc[0], _partialPixel.pc[0]);
        }
        else {
          pc->newXmmArray(p.pc, 1, "pc");
          pc->vmov(p.pc[0], _partialPixel.pc[0]);
        }
      }
      else {
        p.pc.init(_partialPixel.pc[0]);
      }
    }
    else if (p.isAlpha()) {
      p.sa = cc->newUInt32("sa");
      pc->vextractu16(p.sa, _partialPixel.ua[0], 0);
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

// ============================================================================
// [BLPipeGen::CompOpPart - PartialFetch]
// ============================================================================

void CompOpPart::enterPartialMode(uint32_t partialFlags) noexcept {
  // Doesn't apply to solid fills.
  if (isUsingSolidPre())
    return;

  // TODO: [PIPEGEN] We only support partial fetch of 4 pixels at the moment.
  BL_ASSERT(!isInPartialMode());
  BL_ASSERT(pixelGranularity() == 4);

  switch (pixelType()) {
    case Pixel::kTypeRGBA: {
      srcFetch(_partialPixel, Pixel::kPC | partialFlags, pixelGranularity());
      break;
    }

    case Pixel::kTypeAlpha: {
      srcFetch(_partialPixel, Pixel::kUA | partialFlags, pixelGranularity());
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
    case Pixel::kTypeRGBA: {
      const x86::Vec& pix = _partialPixel.pc[0];
      pc->vsrli128b(pix, pix, 4);
      break;
    }

    case Pixel::kTypeAlpha: {
      const x86::Vec& pix = _partialPixel.ua[0];
      pc->vsrli128b(pix, pix, 2);
      break;
    }
  }
}

// ============================================================================
// [BLPipeGen::CompOpPart - CMask - Init / Fini]
// ============================================================================

void CompOpPart::cMaskInit(const x86::Mem& mem) noexcept {
  switch (pixelType()) {
    case Pixel::kTypeRGBA: {
      x86::Vec mVec = cc->newXmm("msk");
      x86::Mem m(mem);

      m.setSize(4);
      pc->vbroadcast_u16(mVec, m);
      cMaskInitRGBA32(mVec);
      break;
    }

    case Pixel::kTypeAlpha: {
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
    case Pixel::kTypeRGBA: {
      if (!vm.isValid() && sm.isValid()) {
        vm = cc->newXmm("c.vm");
        pc->vbroadcast_u16(vm, sm);
      }

      cMaskInitRGBA32(vm);
      break;
    }

    case Pixel::kTypeAlpha: {
      cMaskInitA8(sm, vm);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::cMaskInitOpaque() noexcept {
  switch (pixelType()) {
    case Pixel::kTypeRGBA: {
      cMaskInitRGBA32(x86::Vec());
      break;
    }

    case Pixel::kTypeAlpha: {
      cMaskInitA8(x86::Gp(), x86::Vec());
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::cMaskFini() noexcept {
  switch (pixelType()) {
    case Pixel::kTypeAlpha:
      cMaskFiniA8();
      break;

    case Pixel::kTypeRGBA:
      cMaskFiniRGBA32();
      break;

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::_cMaskLoopInit(uint32_t loopType) noexcept {
  // Make sure `_cMaskLoopInit()` and `_cMaskLoopFini()` are used as a pair.
  BL_ASSERT(_cMaskLoopType == kCMaskLoopTypeNone);
  BL_ASSERT(_cMaskLoopHook == nullptr);

  _cMaskLoopType = uint8_t(loopType);
  _cMaskLoopHook = cc->cursor();
}

void CompOpPart::_cMaskLoopFini() noexcept {
  // Make sure `_cMaskLoopInit()` and `_cMaskLoopFini()` are used as a pair.
  BL_ASSERT(_cMaskLoopType != kCMaskLoopTypeNone);
  BL_ASSERT(_cMaskLoopHook != nullptr);

  _cMaskLoopType = kCMaskLoopTypeNone;
  _cMaskLoopHook = nullptr;
}

// ============================================================================
// [BLPipeGen::CompOpPart - CMask - Generic Loop]
// ============================================================================

void CompOpPart::cMaskGenericLoop(x86::Gp& i) noexcept {
  if (isLoopOpaque() && shouldJustCopyOpaqueFill()) {
    cMaskMemcpyOrMemsetLoop(i);
    return;
  }

  cMaskGenericLoopXmm(i);
}

void CompOpPart::cMaskGenericLoopXmm(x86::Gp& i) noexcept {
  x86::Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();

  // 1 pixel at a time.
  if (maxPixels() == 1) {
    Label L_Loop = cc->newLabel();

    prefetch1();

    cc->bind(L_Loop);
    cMaskCompositeAndStore(x86::ptr(dPtr), 1);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp()), i, 1);
    cc->jnz(L_Loop);

    return;
  }

  BL_ASSERT(minAlignment() >= 1);
  int alignmentMask = int(minAlignment()) - 1;

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
    cMaskCompositeAndStore(x86::ptr(dPtr), 4, 16);
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

  BL_NOT_REACHED();
}

// ============================================================================
// [BLPipeGen::CompOpPart - CMask - Granular Loop]
// ============================================================================

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

// ============================================================================
// [BLPipeGen::CompOpPart - CMask - MemCpy / MemSet Loop]
// ============================================================================

void CompOpPart::cMaskMemcpyOrMemsetLoop(x86::Gp& i) noexcept {
  BL_ASSERT(shouldJustCopyOpaqueFill());
  x86::Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();

  if (srcPart()->isSolid()) {
    // Optimized solid opaque fill -> MemSet.
    BL_ASSERT(_solidOpt.px.isValid());
    pc->xInlinePixelFillLoop(dPtr, _solidOpt.px, i, 64, dstPart()->bpp(), pixelGranularity());
  }
  else if (srcPart()->isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT)) {
    // Optimized solid opaque blit -> MemCopy.
    pc->xInlinePixelCopyLoop(dPtr, srcPart()->as<FetchSimplePatternPart>()->f->srcp1, i, 64, dstPart()->bpp(), pixelGranularity(), dstPart()->format());
  }
  else {
    BL_NOT_REACHED();
  }
}

// ============================================================================
// [BLPipeGen::CompOpPart - CMask - Composition Helpers]
// ============================================================================

void CompOpPart::cMaskCompositeAndStore(const x86::Mem& dPtr_, uint32_t n, uint32_t alignment) noexcept {
  Pixel dPix(pixelType());
  x86::Mem dPtr(dPtr_);

  switch (pixelType()) {
    case Pixel::kTypeRGBA: {
      switch (n) {
        case 1:
          cMaskProcRGBA32Xmm(dPix, 1, Pixel::kPC | Pixel::kImmutable);
          pc->vstorei32(dPtr, dPix.pc[0]);
          break;

        case 4:
          cMaskProcRGBA32Xmm(dPix, 4, Pixel::kPC | Pixel::kImmutable);
          pc->vstorei128x(dPtr, dPix.pc[0], alignment);
          break;

        case 8:
          cMaskProcRGBA32Xmm(dPix, 8, Pixel::kPC | Pixel::kImmutable);
          pc->vstorei128x(dPtr, dPix.pc[0], alignment);
          dPtr.addOffset(16);
          pc->vstorei128x(dPtr, dPix.pc[dPix.pc.size() > 1 ? 1 : 0], alignment);
          break;

        default:
          BL_NOT_REACHED();
      }
      break;
    }

    case Pixel::kTypeAlpha: {
      switch (n) {
        case 1:
          cMaskProcA8Gp(dPix, Pixel::kSA | Pixel::kImmutable);
          pc->store8(dPtr, dPix.sa);
          break;

        case 4:
          cMaskProcA8Xmm(dPix, 4, Pixel::kPA | Pixel::kImmutable);
          pc->vstorei32(dPtr, dPix.pa[0]);
          break;

        case 8:
          cMaskProcA8Xmm(dPix, 8, Pixel::kPA | Pixel::kImmutable);
          pc->vstorei64(dPtr, dPix.pa[0]);
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

// ============================================================================
// [BLPipeGen::CompOpPart - VMask - Composition Helpers]
// ============================================================================

void CompOpPart::vMaskProc(Pixel& out, uint32_t flags, x86::Gp& msk, bool mImmutable) noexcept {
  switch (pixelType()) {
    case Pixel::kTypeRGBA: {
      x86::Vec vm = cc->newXmm("c.vm");
      pc->vmovsi32(vm, msk);
      pc->vswizli16(vm, vm, x86::Predicate::shuf(0, 0, 0, 0));

      VecArray vm_(vm);
      vMaskProcRGBA32Xmm(out, 1, flags, vm_, false);
      break;
    }

    case Pixel::kTypeAlpha: {
      vMaskProcA8Gp(out, flags, msk, mImmutable);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// ============================================================================
// [BLPipeGen::CompOpPart - CMask - Init / Fini - A8]
// ============================================================================

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
      pc->vextractu16(vm, sm, 0);
    }

    _mask->sm = sm;
    _mask->vm = vm;
  }

  if (srcPart()->isSolid()) {
    Pixel& s = srcPart()->as<FetchSolidPart>()->_pixel;
    SolidPixel& o = _solidOpt;
    bool convertToVec = true;

    // ------------------------------------------------------------------------
    // [CMaskInit - A8 - Solid - SrcCopy]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (!hasMask) {
        // Xa = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);
        o.sa = s.sa;

        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kPA);
          o.px = s.pa[0];
        }

        convertToVec = false;
      }
      else {
        // Xa = (Sa * m) + 0.5 <Rounding>
        // Ya = (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);

        o.sx = cc->newUInt32("p.sx");
        o.sy = sm;

        pc->uMul(o.sx, s.sa, o.sy);
        pc->uAdd(o.sx, o.sx, imm(0x80));
        pc->uInv8(o.sy, o.sy);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - A8 - Solid - SrcOver]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_OVER) {
      if (!hasMask) {
        // Xa = Sa * 1 + 0.5 <Rounding>
        // Ya = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);

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
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);

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

    // ------------------------------------------------------------------------
    // [CMaskInit - A8 - Solid - SrcIn]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_IN) {
      if (!hasMask) {
        // Xa = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);

        o.sx = s.sa;
        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUA);
          o.ux = s.ua[0];
        }
      }
      else {
        // Xa = Sa * m + (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);

        o.sx = cc->newUInt32("o.sx");
        pc->uMul(o.sx, s.sa, sm);
        pc->uDiv255(o.sx, o.sx);
        pc->uInv8(sm, sm);
        pc->uAdd(o.sx, o.sx, sm);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - A8 - Solid - SrcOut]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_OUT) {
      if (!hasMask) {
        // Xa = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);

        o.sx = s.sa;
        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUA);
          o.ux = s.ua[0];
        }
      }
      else {
        // Xa = Sa * m
        // Ya = 1  - m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);

        o.sx = cc->newUInt32("o.sx");
        o.sy = sm;

        pc->uMul(o.sx, s.sa, o.sy);
        pc->uDiv255(o.sx, o.sx);
        pc->uInv8(o.sy, o.sy);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - A8 - Solid - DstOut]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_OUT) {
      if (!hasMask) {
        // Xa = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);

        o.sx = cc->newUInt32("o.sx");
        pc->uInv8(o.sx, s.sa);

        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUIA);
          o.ux = s.uia[0];
        }
      }
      else {
        // Xa = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);

        o.sx = sm;
        pc->uMul(o.sx, sm, s.sa);
        pc->uDiv255(o.sx, o.sx);
        pc->uInv8(o.sx, o.sx);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - A8 - Solid - Xor]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_XOR) {
      if (!hasMask) {
        // Xa = Sa
        // Ya = 1 - Xa (SIMD only)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);
        o.sx = s.sa;

        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUA | Pixel::kUIA);

          o.ux = s.ua[0];
          o.uy = s.uia[0];
        }
      }
      else {
        // Xa = Sa * m
        // Ya = 1 - Xa (SIMD only)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);

        o.sx = cc->newUInt32("o.sx");
        pc->uMul(o.sx, sm, s.sa);
        pc->uDiv255(o.sx, o.sx);

        if (maxPixels() > 1) {
          o.ux = cc->newXmm("o.ux");
          o.uy = cc->newXmm("o.uy");
          pc->vbroadcast_u16(o.ux, o.sx);
          pc->vinv255u16(o.uy, o.ux);
        }
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - A8 - Solid - Plus]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_PLUS) {
      if (!hasMask) {
        // Xa = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA | Pixel::kPA);
        o.sa = s.sa;
        o.px = s.pa[0];
        convertToVec = false;
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kSA);
        o.sx = sm;
        pc->uMul(o.sx, o.sx, s.sa);
        pc->uDiv255(o.sx, o.sx);

        if (maxPixels() > 1) {
          o.px = cc->newXmm("o.px");
          pc->uMul(o.sx, o.sx, 0x01010101);
          pc->vbroadcast_u32(o.px, o.sx);
          pc->uShr(o.sx, o.sx, imm(24));
        }

        convertToVec = false;
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - A8 - Solid - Extras]
    // ------------------------------------------------------------------------

    if (convertToVec && maxPixels() > 1) {
      if (o.sx.isValid() && !o.ux.isValid()) {
        o.ux = cc->newXmm("p.ux");
        pc->vbroadcast_u16(o.ux, o.sx);
      }

      if (o.sy.isValid() && !o.uy.isValid()) {
        o.uy = cc->newXmm("p.uy");
        pc->vbroadcast_u16(o.uy, o.sy);
      }
    }
  }
  else {
    if (sm.isValid() && !vm.isValid() && maxPixels() > 1) {
      vm = cc->newXmm("vm");
      pc->vbroadcast_u16(vm, sm);
      _mask->vm = vm;
    }

    /*
    // ------------------------------------------------------------------------
    // [CMaskInit - A8 - NonSolid - SrcCopy]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (hasMask) {
        x86::Xmm vn = cc->newXmm("vn");
        pc->vinv255u16(vn, m);
        _mask->vec.vn = vn;
      }
    }
    */
  }

  _cMaskLoopInit(hasMask ? kCMaskLoopTypeMask : kCMaskLoopTypeOpaque);
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

// ============================================================================
// [BLPipeGen::CompOpPart - CMask - Proc - A8]
// ============================================================================

void CompOpPart::cMaskProcA8Gp(Pixel& out, uint32_t flags) noexcept {
  out.setCount(1);

  bool hasMask = isLoopCMask();

  if (srcPart()->isSolid()) {
    Pixel d(pixelType());
    SolidPixel& o = _solidOpt;

    x86::Gp& da = d.sa;
    x86::Gp sx = cc->newUInt32("sx");

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - SrcCopy]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (!hasMask) {
        // Da' = Xa
        out.sa = o.sa;
        out.makeImmutable();
      }
      else {
        // Da' = Xa  + Da .(1 - m)
        dstFetch(d, Pixel::kSA, 1);

        pc->uMul(da, da, o.sy),
        pc->uAdd(da, da, o.sx);
        pc->uMul257hu16(da, da);

        out.sa = da;
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - SrcOver]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_OVER) {
      // Da' = Xa + Da .Ya
      dstFetch(d, Pixel::kSA, 1);

      pc->uMul(da, da, o.sy);
      pc->uAdd(da, da, o.sx);
      pc->uMul257hu16(da, da);

      out.sa = da;

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - SrcIn / DstOut]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_IN || compOp() == BL_COMP_OP_DST_OUT) {
      // Da' = Xa.Da
      dstFetch(d, Pixel::kSA, 1);

      pc->uMul(da, da, o.sx);
      pc->uDiv255(da, da);
      out.sa = da;

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - SrcOut]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_OUT) {
      if (!hasMask) {
        // Da' = Xa.(1 - Da)
        dstFetch(d, Pixel::kSA, 1);

        pc->uInv8(da, da);
        pc->uMul(da, da, o.sx);
        pc->uDiv255(da, da);
        out.sa = da;
      }
      else {
        // Da' = Xa.(1 - Da) + Da.Ya
        dstFetch(d, Pixel::kSA, 1);

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

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - Xor]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_XOR) {
      // Da' = Xa.(1 - Da) + Da.Ya
      dstFetch(d, Pixel::kSA, 1);

      pc->uMul(sx, da, o.sy);
      pc->uInv8(da, da);
      pc->uMul(da, da, o.sx);
      pc->uAdd(da, da, sx);
      pc->uDiv255(da, da);
      out.sa = da;

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - Plus]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_PLUS) {
      // Da' = Clamp(Da + Xa)
      dstFetch(d, Pixel::kSA, 1);

      pc->uAddsU8(da, da, o.sx);
      out.sa = da;

      pc->xSatisfyPixel(out, flags);
      return;
    }
  }

  vMaskProcA8Gp(out, flags, _mask->sm, true);
}

void CompOpPart::cMaskProcA8Xmm(Pixel& out, uint32_t n, uint32_t flags) noexcept {
  out.setCount(n);

  bool hasMask = isLoopCMask();

  if (srcPart()->isSolid()) {
    Pixel d(pixelType());
    SolidPixel& o = _solidOpt;

    uint32_t kFullN = (n + 7) / 8;

    VecArray& da = d.ua;
    VecArray xa;
    pc->newXmmArray(xa, kFullN, "x");

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - SrcCopy]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (!hasMask) {
        // Da' = Xa
        out.pa.init(o.px);
        out.makeImmutable();
      }
      else {
        // Da' = Xa + Da .(1 - m)
        dstFetch(d, Pixel::kUA, n);

        pc->vmuli16(da, da, o.uy),
        pc->vaddi16(da, da, o.ux);
        pc->vmul257hu16(da, da);

        out.ua.init(da);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - SrcOver]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_OVER) {
      // Da' = Xa + Da.Ya
      dstFetch(d, Pixel::kUA, n);

      pc->vmuli16(da, da, o.uy);
      pc->vaddi16(da, da, o.ux);
      pc->vmul257hu16(da, da);

      out.ua.init(da);

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - SrcIn / DstOut]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_IN || compOp() == BL_COMP_OP_DST_OUT) {
      // Da' = Xa.Da
      dstFetch(d, Pixel::kUA, n);

      pc->vmulu16(da, da, o.ux);
      pc->vdiv255u16(da);
      out.ua.init(da);

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - SrcOut]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_OUT) {
      if (!hasMask) {
        // Da' = Xa.(1 - Da)
        dstFetch(d, Pixel::kUA, n);

        pc->vinv255u16(da, da);
        pc->vmulu16(da, da, o.ux);
        pc->vdiv255u16(da);
        out.ua.init(da);
      }
      else {
        // Da' = Xa.(1 - Da) + Da.Ya
        dstFetch(d, Pixel::kUA, n);

        pc->vinv255u16(xa, da);
        pc->vmulu16(xa, xa, o.ux);
        pc->vmulu16(da, da, o.uy);
        pc->vaddi16(da, da, xa);
        pc->vdiv255u16(da);
        out.ua.init(da);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - Xor]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_XOR) {
      // Da' = Xa.(1 - Da) + Da.Ya
      dstFetch(d, Pixel::kUA, n);

      pc->vmulu16(xa, da, o.uy);
      pc->vinv255u16(da, da);
      pc->vmulu16(da, da, o.ux);
      pc->vaddi16(da, da, xa);
      pc->vdiv255u16(da);
      out.ua.init(da);

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - A8 - Plus]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_PLUS) {
      // Da' = Clamp(Da + Xa)
      dstFetch(d, Pixel::kPA, n);

      pc->vaddsu8(d.pa, d.pa, o.px);
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

// ============================================================================
// [BLPipeGen::CompOpPart - VMask Proc - A8 (Scalar)]
// ============================================================================

void CompOpPart::vMaskProcA8Gp(Pixel& out, uint32_t flags, x86::Gp& msk, bool mImmutable) noexcept {
  bool hasMask = msk.isValid();

  Pixel d(Pixel::kTypeAlpha);
  Pixel s(Pixel::kTypeAlpha);

  x86::Gp x = cc->newUInt32("@x");
  x86::Gp y = cc->newUInt32("@y");

  x86::Gp& da = d.sa;
  x86::Gp& sa = s.sa;

  out.setCount(1);

  // --------------------------------------------------------------------------
  // [VMask - A8 - SrcCopy]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_COPY) {
    if (!hasMask) {
      // Da' = Sa
      srcFetch(out, flags, 1);
    }
    else {
      // Da' = Sa.m + Da.(1 - m)
      srcFetch(s, Pixel::kSA, 1);
      dstFetch(d, Pixel::kSA, 1);

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

  // --------------------------------------------------------------------------
  // [VMask - A8 - SrcOver]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_OVER) {
    if (!hasMask) {
      // Da' = Sa + Da.(1 - Sa)
      srcFetch(s, Pixel::kSA | Pixel::kImmutable, 1);
      dstFetch(d, Pixel::kSA, 1);

      pc->uInv8(x, sa);
      pc->uMul(da, da, x);
      pc->uDiv255(da, da);
      pc->uAdd(da, da, sa);
    }
    else {
      // Da' = Sa.m + Da.(1 - Sa.m)
      srcFetch(s, Pixel::kSA, 1);
      dstFetch(d, Pixel::kSA, 1);

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

  // --------------------------------------------------------------------------
  // [VMask - A8 - SrcIn]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_IN) {
    if (!hasMask) {
      // Da' = Sa.Da
      srcFetch(s, Pixel::kSA | Pixel::kImmutable, 1);
      dstFetch(d, Pixel::kSA, 1);

      pc->uMul(da, da, sa);
      pc->uDiv255(da, da);
    }
    else {
      // Da' = Da.(Sa.m) + Da.(1 - m)
      //     = Da.(Sa.m + 1 - m)
      srcFetch(s, Pixel::kSA, 1);
      dstFetch(d, Pixel::kSA, 1);

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

  // --------------------------------------------------------------------------
  // [VMask - A8 - SrcOut]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_OUT) {
    if (!hasMask) {
      // Da' = Sa.(1 - Da)
      srcFetch(s, Pixel::kSA | Pixel::kImmutable, 1);
      dstFetch(d, Pixel::kSA, 1);

      pc->uInv8(da, da);
      pc->uMul(da, da, sa);
      pc->uDiv255(da, da);
    }
    else {
      // Da' = Sa.m.(1 - Da) + Da.(1 - m)
      srcFetch(s, Pixel::kSA, 1);
      dstFetch(d, Pixel::kSA, 1);

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

  // --------------------------------------------------------------------------
  // [VMask - A8 - DstOut]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_OUT) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa)
      srcFetch(s, Pixel::kSA, 1);
      dstFetch(d, Pixel::kSA, 1);

      pc->uInv8(sa, sa);
      pc->uMul(da, da, sa);
      pc->uDiv255(da, da);
    }
    else {
      // Da' = Da.(1 - Sa.m)
      srcFetch(s, Pixel::kSA, 1);
      dstFetch(d, Pixel::kSA, 1);

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

  // --------------------------------------------------------------------------
  // [VMask - A8 - Xor]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_XOR) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa) + Sa.(1 - Da)
      srcFetch(s, Pixel::kSA, 1);
      dstFetch(d, Pixel::kSA, 1);

      pc->uInv8(y, sa);
      pc->uInv8(x, da);

      pc->uMul(da, da, y);
      pc->uMul(sa, sa, x);
      pc->uAdd(da, da, sa);
      pc->uDiv255(da, da);
    }
    else {
      // Da' = Da.(1 - Sa.m) + Sa.m.(1 - Da)
      srcFetch(s, Pixel::kSA, 1);
      dstFetch(d, Pixel::kSA, 1);

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

  // --------------------------------------------------------------------------
  // [VMask - A8 - Plus]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_PLUS) {
    // Da' = Clamp(Da + Sa)
    // Da' = Clamp(Da + Sa.m)
    if (hasMask) {
      srcFetch(s, Pixel::kSA, 1);
      dstFetch(d, Pixel::kSA, 1);

      pc->uMul(sa, sa, msk);
      pc->uDiv255(sa, sa);
    }
    else {
      srcFetch(s, Pixel::kSA | Pixel::kImmutable, 1);
      dstFetch(d, Pixel::kSA, 1);
    }

    pc->uAddsU8(da, da, sa);

    out.sa = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMask - A8 - Invert]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_INTERNAL_ALPHA_INV) {
    // Da' = 1 - Da
    // Da' = Da.(1 - m) + (1 - Da).m
    if (hasMask) {
      dstFetch(d, Pixel::kSA, 1);
      pc->uInv8(x, msk);
      pc->uMul(x, x, da);
      pc->uInv8(da, da);
      pc->uMul(da, da, msk);
      pc->uAdd(da, da, x);
      pc->uDiv255(da, da);
    }
    else {
      dstFetch(d, Pixel::kSA, 1);
      pc->uInv8(da, da);
    }

    out.sa = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMask - A8 - Invalid]
  // --------------------------------------------------------------------------

  BL_NOT_REACHED();
}

// ============================================================================
// [BLPipeGen::CompOpPart - VMask - Proc - A8 (XMM)]
// ============================================================================

void CompOpPart::vMaskProcA8Xmm(Pixel& out, uint32_t n, uint32_t flags, VecArray& vm, bool mImmutable) noexcept {
  bool hasMask = !vm.empty();
  uint32_t kFullN = (n + 7) / 8;

  VecArray xv, yv;
  pc->newXmmArray(xv, kFullN, "x");
  pc->newXmmArray(yv, kFullN, "y");

  Pixel d(Pixel::kTypeAlpha);
  Pixel s(Pixel::kTypeAlpha);

  VecArray& da = d.ua;
  VecArray& sa = s.ua;

  out.setCount(n);

  // --------------------------------------------------------------------------
  // [VMask - A8 - SrcCopy]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_COPY) {
    if (!hasMask) {
      // Da' = Sa
      srcFetch(out, flags, n);
    }
    else {
      // Da' = Sa.m + Da.(1 - m)
      srcFetch(s, Pixel::kUA, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vmulu16(sa, sa, vm);
      pc->vinv255u16(vm, vm);
      pc->vmulu16(da, da, vm);

      if (mImmutable)
        pc->vinv255u16(vm, vm);

      pc->vaddi16(da, da, sa);
      pc->vdiv255u16(da);

      out.ua = da;
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMask - A8 - SrcOver]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_OVER) {
    if (!hasMask) {
      // Da' = Sa + Da.(1 - Sa)
      srcFetch(s, Pixel::kUA | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vinv255u16(xv, sa);
      pc->vmulu16(da, da, xv);
      pc->vdiv255u16(da);
      pc->vaddi16(da, da, sa);
    }
    else {
      // Da' = Sa.m + Da.(1 - Sa.m)
      srcFetch(s, Pixel::kUA, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vmulu16(sa, sa, vm);
      pc->vdiv255u16(sa);
      pc->vinv255u16(xv, sa);
      pc->vmulu16(da, da, xv);
      pc->vdiv255u16(da);
      pc->vaddi16(da, da, sa);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMask - A8 - SrcIn]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_IN) {
    if (!hasMask) {
      // Da' = Sa.Da
      srcFetch(s, Pixel::kUA | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vmulu16(da, da, sa);
      pc->vdiv255u16(da);
    }
    else {
      // Da' = Da.(Sa.m) + Da.(1 - m)
      //     = Da.(Sa.m + 1 - m)
      srcFetch(s, Pixel::kUA, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vmulu16(sa, sa, vm);
      pc->vdiv255u16(sa);
      pc->vaddi16(sa, sa, C_MEM(i128_00FF00FF00FF00FF));
      pc->vsubi16(sa, sa, vm);
      pc->vmulu16(da, da, sa);
      pc->vdiv255u16(da);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMask - A8 - SrcOut]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_OUT) {
    if (!hasMask) {
      // Da' = Sa.(1 - Da)
      srcFetch(s, Pixel::kUA | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vinv255u16(da, da);
      pc->vmulu16(da, da, sa);
      pc->vdiv255u16(da);
    }
    else {
      // Da' = Sa.m.(1 - Da) + Da.(1 - m)
      srcFetch(s, Pixel::kUA, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vmulu16(sa, sa, vm);
      pc->vdiv255u16(sa);

      pc->vinv255u16(xv, da);
      pc->vinv255u16(vm, vm);
      pc->vmulu16(sa, sa, xv);
      pc->vmulu16(da, da, vm);

      if (mImmutable)
        pc->vinv255u16(vm, vm);

      pc->vaddi16(da, da, sa);
      pc->vdiv255u16(da);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMask - A8 - DstOut]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_OUT) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa)
      srcFetch(s, Pixel::kUA, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vinv255u16(sa, sa);
      pc->vmulu16(da, da, sa);
      pc->vdiv255u16(da);
    }
    else {
      // Da' = Da.(1 - Sa.m)
      srcFetch(s, Pixel::kUA, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vmulu16(sa, sa, vm);
      pc->vdiv255u16(sa);
      pc->vinv255u16(sa, sa);
      pc->vmulu16(da, da, sa);
      pc->vdiv255u16(da);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMask - A8 - Xor]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_XOR) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa) + Sa.(1 - Da)
      srcFetch(s, Pixel::kUA, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vinv255u16(yv, sa);
      pc->vinv255u16(xv, da);

      pc->vmulu16(da, da, yv);
      pc->vmulu16(sa, sa, xv);
      pc->vaddi16(da, da, sa);
      pc->vdiv255u16(da);
    }
    else {
      // Da' = Da.(1 - Sa.m) + Sa.m.(1 - Da)
      srcFetch(s, Pixel::kUA, n);
      dstFetch(d, Pixel::kUA, n);

      pc->vmulu16(sa, sa, vm);
      pc->vdiv255u16(sa);

      pc->vinv255u16(yv, sa);
      pc->vinv255u16(xv, da);

      pc->vmulu16(da, da, yv);
      pc->vmulu16(sa, sa, xv);
      pc->vaddi16(da, da, sa);
      pc->vdiv255u16(da);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMask - A8 - Plus]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_PLUS) {
    // Da' = Clamp(Da + Sa)
    // Da' = Clamp(Da + Sa.m)
    if (hasMask) {
      srcFetch(s, Pixel::kUA, n);
      dstFetch(d, Pixel::kPA, n);

      pc->vmulu16(sa, sa, vm);
      pc->vdiv255u16(sa);

      s.pa = sa.even();
      pc->vpacki16u8(s.pa, s.pa, sa.odd());
    }
    else {
      srcFetch(s, Pixel::kPA | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kPA, n);
    }

    pc->vaddsu8(d.pa, d.pa, s.pa);
    out.pa = d.pa;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMask - A8 - Invert]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_INTERNAL_ALPHA_INV) {
    // Da' = 1 - Da
    // Da' = Da.(1 - m) + (1 - Da).m
    if (hasMask) {
      dstFetch(d, Pixel::kUA, n);
      pc->vinv255u16(xv, vm);
      pc->vmulu16(xv, xv, da);
      pc->vinv255u16(da, da);
      pc->vmulu16(da, da, vm);
      pc->vaddi16(da, da, xv);
      pc->vdiv255u16(da);
    }
    else {
      dstFetch(d, Pixel::kUA, n);
      pc->vinv255u16(da, da);
    }

    out.ua = da;
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMask - A8 - Invalid]
  // --------------------------------------------------------------------------

  BL_NOT_REACHED();
}

// ============================================================================
// [BLPipeGen::CompOpPart - CMask - Init / Fini - RGBA]
// ============================================================================

void CompOpPart::cMaskInitRGBA32(const x86::Vec& vm) noexcept {
  bool hasMask = vm.isValid();
  bool useDa = hasDa();

  if (srcPart()->isSolid()) {
    Pixel& s = srcPart()->as<FetchSolidPart>()->_pixel;
    SolidPixel& o = _solidOpt;

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - SrcCopy]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kPC);
        o.px = s.pc[0];
      }
      else {
        // Xca = (Sca * m) + 0.5 <Rounding>
        // Xa  = (Sa  * m) + 0.5 <Rounding>
        // Im  = (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

        o.ux = cc->newXmm("p.ux");
        o.vn = vm;

        pc->vmulu16(o.ux, s.uc[0], o.vn);
        pc->vaddi16(o.ux, o.ux, pc->constAsXmm(blCommonTable.i128_0080008000800080));
        pc->vinv255u16(o.vn, o.vn);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - SrcOver]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_OVER) {
      if (!hasMask) {
        // Xca = Sca * 1 + 0.5 <Rounding>
        // Xa  = Sa  * 1 + 0.5 <Rounding>
        // Yca = 1 - Sa
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC | Pixel::kUIA | Pixel::kImmutable);

        o.ux = cc->newXmm("p.ux");
        o.uy = s.uia[0];

        pc->vslli16(o.ux, s.uc[0], 8);
        pc->vsubi16(o.ux, o.ux, s.uc[0]);
        pc->vaddi16(o.ux, o.ux, pc->constAsXmm(blCommonTable.i128_0080008000800080));

        cc->alloc(o.uy);
      }
      else {
        // Xca = Sca * m + 0.5 <Rounding>
        // Xa  = Sa  * m + 0.5 <Rounding>
        // Yca = 1 - (Sa * m)
        // Ya  = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC | Pixel::kImmutable);

        o.ux = cc->newXmm("p.ux");
        o.uy = cc->newXmm("p.uy");

        pc->vmulu16(o.uy, s.uc[0], vm);
        pc->vdiv255u16(o.uy);

        pc->vslli16(o.ux, o.uy, 8);
        pc->vsubi16(o.ux, o.ux, o.uy);
        pc->vaddi16(o.ux, o.ux, pc->constAsXmm(blCommonTable.i128_0080008000800080));

        pc->vswizli16(o.uy, o.uy, x86::Predicate::shuf(3, 3, 3, 3));
        pc->vswizhi16(o.uy, o.uy, x86::Predicate::shuf(3, 3, 3, 3));
        pc->vinv255u16(o.uy, o.uy);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - SrcIn / SrcOut]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_IN || compOp() == BL_COMP_OP_SRC_OUT) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

        o.ux = s.uc[0];
        cc->alloc(o.ux);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Im  = 1   - m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

        o.ux = cc->newXmm("o.uc0");
        o.vn = vm;

        pc->vmulu16(o.ux, s.uc[0], vm);
        pc->vdiv255u16(o.ux);
        pc->vinv255u16(vm, vm);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - SrcAtop / Xor / Darken / Lighten]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_ATOP || compOp() == BL_COMP_OP_XOR || compOp() == BL_COMP_OP_DARKEN || compOp() == BL_COMP_OP_LIGHTEN) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = 1 - Sa
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC | Pixel::kUIA);

        o.ux = s.uc[0];
        o.uy = s.uia[0];

        cc->alloc(o.ux);
        cc->alloc(o.uy);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = 1 - (Sa * m)
        // Ya  = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

        o.ux = cc->newXmm("o.ux");
        o.uy = vm;

        pc->vmulu16(o.ux, s.uc[0], o.uy);
        pc->vdiv255u16(o.ux);

        pc->vswizli16(o.uy, o.ux, x86::Predicate::shuf(3, 3, 3, 3));
        pc->vswizi32(o.uy, o.uy, x86::Predicate::shuf(0, 0, 0, 0));
        pc->vinv255u16(o.uy, o.uy);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - Dst]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_COPY) {
      BL_NOT_REACHED();
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - DstOver]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_OVER) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

        o.ux = s.uc[0];
        cc->alloc(o.ux);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

        o.ux = cc->newXmm("o.uc0");
        pc->vmulu16(o.ux, s.uc[0], vm);
        pc->vdiv255u16(o.ux);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - DstIn]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_IN) {
      if (!hasMask) {
        // Xca = Sa
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUA);

        o.ux = s.ua[0];
        cc->alloc(o.ux);
      }
      else {
        // Xca = 1 - m.(1 - Sa)
        // Xa  = 1 - m.(1 - Sa)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUA);

        o.ux = cc->newXmm("o.ux");
        pc->vmov(o.ux, s.ua[0]);

        pc->vinv255u16(o.ux, o.ux);
        pc->vmulu16(o.ux, o.ux, vm);
        pc->vdiv255u16(o.ux);
        pc->vinv255u16(o.ux, o.ux);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - DstOut]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_OUT) {
      if (!hasMask) {
        // Xca = 1 - Sa
        // Xa  = 1 - Sa
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUIA);

          o.ux = s.uia[0];
          cc->alloc(o.ux);
        }
        // Xca = 1 - Sa
        // Xa  = 1
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUA);

          o.ux = cc->newXmm("ux");
          pc->vmov(o.ux, s.ua[0]);
          pc->vNegRgb8W(o.ux, o.ux);
        }
      }
      else {
        // Xca = 1 - (Sa * m)
        // Xa  = 1 - (Sa * m)
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUA);

          o.ux = vm;
          pc->vmulu16(o.ux, o.ux, s.ua[0]);
          pc->vdiv255u16(o.ux);
          pc->vinv255u16(o.ux, o.ux);
        }
        // Xca = 1 - (Sa * m)
        // Xa  = 1
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUA);

          o.ux = vm;
          pc->vmulu16(o.ux, o.ux, s.ua[0]);
          pc->vdiv255u16(o.ux);
          pc->vinv255u16(o.ux, o.ux);
          pc->vFillAlpha255W(o.ux, o.ux);
        }
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - DstAtop]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_ATOP) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = Sa
        // Ya  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC | Pixel::kUA);

        o.ux = s.uc[0];
        o.uy = s.ua[0];

        cc->alloc(o.ux);
        cc->alloc(o.uy);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = 1 - m.(1 - Sa)
        // Ya  = 1 - m.(1 - Sa)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC | Pixel::kUA);

        o.ux = cc->newXmm("o.ux");
        o.uy = cc->newXmm("o.uy");

        pc->vmov(o.uy, s.ua[0]);
        pc->vinv255u16(o.uy, o.uy);

        pc->vmulu16(o.ux, s.uc[0], vm);
        pc->vmulu16(o.uy, o.uy, vm);

        pc->vdiv255u16_2x(o.ux, o.uy);
        pc->vinv255u16(o.uy, o.uy);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - Plus]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_PLUS) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kPC);

        o.px = s.pc[0];
        cc->alloc(o.px);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);
        o.px = cc->newXmm("px");

        pc->vmulu16(o.px, s.uc[0], vm);
        pc->vdiv255u16(o.px);
        pc->vpacki16u8(o.px, o.px, o.px);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - Minus]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_MINUS) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = 0
        // Yca = Sca
        // Ya  = Sa
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

          o.ux = cc->newXmm("ux");
          o.uy = s.uc[0];

          cc->alloc(o.uy);
          pc->vmov(o.ux, o.uy);
          pc->vZeroAlphaW(o.ux, o.ux);
        }
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kPC);
          o.px = cc->newXmm("px");
          pc->vmov(o.px, s.pc[0]);
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
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

          o.ux = cc->newXmm("ux");
          o.uy = cc->newXmm("uy");
          o.vm = vm;
          o.vn = cc->newXmm("vn");

          pc->vZeroAlphaW(o.ux, s.uc[0]);
          pc->vmov(o.uy, s.uc[0]);

          pc->vinv255u16(o.vn, o.vm);
          pc->vZeroAlphaW(o.vm, o.vm);
          pc->vZeroAlphaW(o.vn, o.vn);
          pc->vFillAlpha255W(o.vm, o.vm);
        }
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

          o.ux = cc->newXmm("ux");
          o.vm = vm;
          o.vn = cc->newXmm("vn");

          pc->vZeroAlphaW(o.ux, s.uc[0]);
          pc->vinv255u16(o.vn, o.vm);
        }
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - Multiply]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_MULTIPLY) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = Sca + (1 - Sa)
        // Ya  = Sa  + (1 - Sa)
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC | Pixel::kUIA);

          o.ux = s.uc[0];
          o.uy = cc->newXmm("uy");

          cc->alloc(o.ux);
          pc->vmov(o.uy, s.uia[0]);
          pc->vaddi16(o.uy, o.uy, o.ux);
        }
        // Yca = Sca + (1 - Sa)
        // Ya  = Sa  + (1 - Sa)
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC | Pixel::kUIA);

          o.uy = cc->newXmm("uy");
          pc->vmov(o.uy, s.uia[0]);
          pc->vaddi16(o.uy, o.uy, s.uc[0]);
        }
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = Sca * m + (1 - Sa * m)
        // Ya  = Sa  * m + (1 - Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

        o.ux = cc->newXmm("ux");
        o.uy = cc->newXmm("uy");

        pc->vmulu16(o.ux, s.uc[0], vm);
        pc->vdiv255u16(o.ux);

        pc->vswizli16(o.uy, o.ux, x86::Predicate::shuf(3, 3, 3, 3));
        pc->vinv255u16(o.uy, o.uy);
        pc->vswizi32(o.uy, o.uy, x86::Predicate::shuf(0, 0, 0, 0));
        pc->vaddi16(o.uy, o.uy, o.ux);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - Screen]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SCREEN) {
      if (!hasMask) {
        // Xca = Sca * 1 + 0.5 <Rounding>
        // Xa  = Sa  * 1 + 0.5 <Rounding>
        // Yca = 1 - Sca
        // Ya  = 1 - Sa

        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

        o.ux = cc->newXmm("p.ux");
        o.uy = cc->newXmm("p.uy");

        pc->vinv255u16(o.uy, o.ux);
        pc->vslli16(o.ux, s.uc[0], 8);
        pc->vsubi16(o.ux, o.ux, s.uc[0]);
        pc->vaddi16(o.ux, o.ux, pc->constAsXmm(blCommonTable.i128_0080008000800080));

        cc->alloc(o.uy);
      }
      else {
        // Xca = Sca * m + 0.5 <Rounding>
        // Xa  = Sa  * m + 0.5 <Rounding>
        // Yca = 1 - (Sca * m)
        // Ya  = 1 - (Sa  * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

        o.ux = cc->newXmm("p.ux");
        o.uy = cc->newXmm("p.uy");

        pc->vmulu16(o.uy, s.uc[0], vm);
        pc->vdiv255u16(o.uy);

        pc->vslli16(o.ux, o.uy, 8);
        pc->vsubi16(o.ux, o.ux, o.uy);
        pc->vaddi16(o.ux, o.ux, pc->constAsXmm(blCommonTable.i128_0080008000800080));
        pc->vinv255u16(o.uy, o.uy);
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - LinearBurn / Difference / Exclusion]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_LINEAR_BURN || compOp() == BL_COMP_OP_DIFFERENCE || compOp() == BL_COMP_OP_EXCLUSION) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = Sa
        // Ya  = Sa
        srcPart()->as<FetchSolidPart>()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC | Pixel::kUA);

        o.ux = s.uc[0];
        o.uy = s.ua[0];

        cc->alloc(o.ux);
        cc->alloc(o.uy);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = Sa  * m
        // Ya  = Sa  * m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

        o.ux = cc->newXmm("ux");
        o.uy = cc->newXmm("uy");

        pc->vmulu16(o.ux, s.uc[0], vm);
        pc->vdiv255u16(o.ux);

        pc->vswizli16(o.uy, o.ux, x86::Predicate::shuf(3, 3, 3, 3));
        pc->vswizi32(o.uy, o.uy, x86::Predicate::shuf(0, 0, 0, 0));
      }
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - TypeA (Non-Opaque)]
    // ------------------------------------------------------------------------

    else if ((compOpFlags() & BL_COMP_OP_FLAG_TYPE_A) && hasMask) {
      // Multiply the source pixel with the mask if `TypeA`.
      srcPart()->as<FetchSolidPart>()->initSolidFlags(Pixel::kUC);

      Pixel& pre = _solidPre;
      pre.setCount(1);
      pre.uc.init(cc->newXmm("pre.uc"));

      pc->vmulu16(pre.uc[0], s.uc[0], vm);
      pc->vdiv255u16(pre.uc[0]);
    }

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - Solid - No Optimizations]
    // ------------------------------------------------------------------------

    else {
      // No optimization. The compositor will simply use the mask provided.
      _mask->vm = vm;
    }
  }
  else {
    _mask->vm = vm;

    // ------------------------------------------------------------------------
    // [CMaskInit - RGBA32 - NonSolid - SrcCopy]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (hasMask) {
        _mask->vn = cc->newXmm("vn");
        pc->vinv255u16(_mask->vn, vm);
      }
    }
  }

  _cMaskLoopInit(hasMask ? kCMaskLoopTypeMask : kCMaskLoopTypeOpaque);
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

// ============================================================================
// [BLPipeGen::CompOpPart - CMask - Proc - RGBA]
// ============================================================================

void CompOpPart::cMaskProcRGBA32Xmm(Pixel& out, uint32_t n, uint32_t flags) noexcept {
  bool hasMask = isLoopCMask();

  uint32_t kFullN = (n + 1) / 2;
  uint32_t kUseHi = n > 1;

  out.setCount(n);

  if (srcPart()->isSolid()) {
    Pixel d(pixelType());
    SolidPixel& o = _solidOpt;
    VecArray xv, yv, zv;

    pc->newXmmArray(xv, kFullN, "x");
    pc->newXmmArray(yv, kFullN, "y");
    pc->newXmmArray(zv, kFullN, "z");

    bool useDa = hasDa();

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - SrcCopy]
    // ------------------------------------------------------------------------

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
        dstFetch(d, Pixel::kUC, n);
        VecArray& dv = d.uc;
        pc->vmulu16(dv, dv, o.vn);
        pc->vaddi16(dv, dv, o.ux);
        pc->vmul257hu16(dv, dv);
        out.uc.init(dv);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - SrcOver / Screen]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_OVER || compOp() == BL_COMP_OP_SCREEN) {
      // Dca' = Xca + Dca.Yca
      // Da'  = Xa  + Da .Ya
      dstFetch(d, Pixel::kUC, n);
      VecArray& dv = d.uc;

      pc->vmulu16(dv, dv, o.uy);
      pc->vaddi16(dv, dv, o.ux);
      pc->vmul257hu16(dv, dv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);

      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - SrcIn]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_IN) {
      // Dca' = Xca.Da
      // Da'  = Xa .Da
      if (!hasMask) {
        dstFetch(d, Pixel::kUA, n);
        VecArray& dv = d.ua;

        pc->vmulu16(dv, dv, o.ux);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Xca.Da + Dca.(1 - m)
      // Da'  = Xa .Da + Da .(1 - m)
      else {
        dstFetch(d, Pixel::kUC | Pixel::kUA, n);
        VecArray& dv = d.uc;
        VecArray& da = d.ua;

        pc->vmulu16(dv, dv, o.vn);
        pc->vmulu16(da, da, o.ux);
        pc->vaddi16(dv, dv, da);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - SrcOut]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_OUT) {
      // Dca' = Xca.(1 - Da)
      // Da'  = Xa .(1 - Da)
      if (!hasMask) {
        dstFetch(d, Pixel::kUIA, n);
        VecArray& dv = d.uia;

        pc->vmulu16(dv, dv, o.ux);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Xca.(1 - Da) + Dca.(1 - m)
      // Da'  = Xa .(1 - Da) + Da .(1 - m)
      else {
        dstFetch(d, Pixel::kUC, n);
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->vinv255u16(dv, dv);
        pc->vmulu16(xv, xv, o.ux);
        pc->vmulu16(dv, dv, o.vn);
        pc->vaddi16(dv, dv, xv);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - SrcAtop]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_ATOP) {
      // Dca' = Xca.Da + Dca.Yca
      // Da'  = Xa .Da + Da .Ya
      dstFetch(d, Pixel::kUC, n);
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(dv, dv, o.uy);
      pc->vmulu16(xv, xv, o.ux);

      pc->vaddi16(dv, dv, xv);
      pc->vdiv255u16(dv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - Dst]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DST_COPY) {
      // Dca' = Dca
      // Da'  = Da
      BL_NOT_REACHED();
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - DstOver]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DST_OVER) {
      // Dca' = Xca.(1 - Da) + Dca
      // Da'  = Xa .(1 - Da) + Da
      dstFetch(d, Pixel::kPC | Pixel::kUIA, n);
      VecArray& dv = d.uia;

      pc->vmulu16(dv, dv, o.ux);
      pc->vdiv255u16(dv);

      VecArray dh = dv.even();
      pc->vpacki16u8(dh, dh, dv.odd());
      pc->vaddi32(dh, dh, d.pc);

      out.pc.init(dh);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - DstIn / DstOut]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DST_IN || compOp() == BL_COMP_OP_DST_OUT) {
      // Dca' = Xca.Dca
      // Da'  = Xa .Da
      dstFetch(d, Pixel::kUC, n);
      VecArray& dv = d.uc;

      pc->vmulu16(dv, dv, o.ux);
      pc->vdiv255u16(dv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - DstAtop / Xor / Multiply]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DST_ATOP || compOp() == BL_COMP_OP_XOR || compOp() == BL_COMP_OP_MULTIPLY) {
      // Dca' = Xca.(1 - Da) + Dca.Yca
      // Da'  = Xa .(1 - Da) + Da .Ya
      if (useDa) {
        dstFetch(d, Pixel::kUC, n);
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->vmulu16(dv, dv, o.uy);
        pc->vinv255u16(xv, xv);
        pc->vmulu16(xv, xv, o.ux);

        pc->vaddi16(dv, dv, xv);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Dca.Yca
      // Da'  = Da .Ya
      else {
        dstFetch(d, Pixel::kUC, n);
        VecArray& dv = d.uc;

        pc->vmulu16(dv, dv, o.uy);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - Plus]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_PLUS) {
      // Dca' = Clamp(Dca + Sca)
      // Da'  = Clamp(Da  + Sa )
      dstFetch(d, Pixel::kPC, n);
      VecArray& dv = d.pc;

      pc->vaddsu8(dv, dv, o.px);
      out.pc.init(dv);

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - Minus]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_MINUS) {
      if (!hasMask) {
        // Dca' = Clamp(Dca - Xca) + Yca.(1 - Da)
        // Da'  = Da + Ya.(1 - Da)
        if (useDa) {
          dstFetch(d, Pixel::kUC, n);
          VecArray& dv = d.uc;

          pc->vExpandAlpha16(xv, dv, kUseHi);
          pc->vinv255u16(xv, xv);
          pc->vmulu16(xv, xv, o.uy);
          pc->vsubsu16(dv, dv, o.ux);
          pc->vdiv255u16(xv);

          pc->vaddi16(dv, dv, xv);
          out.uc.init(dv);
        }
        // Dca' = Clamp(Dca - Xca)
        // Da'  = <unchanged>
        else {
          dstFetch(d, Pixel::kPC, n);
          VecArray& dh = d.pc;

          pc->vsubsu8(dh, dh, o.px);
          out.pc.init(dh);
        }
      }
      else {
        // Dca' = (Clamp(Dca - Xca) + Yca.(1 - Da)).m + Dca.(1 - m)
        // Da'  = Da + Ya.(1 - Da)
        if (useDa) {
          dstFetch(d, Pixel::kUC, n);
          VecArray& dv = d.uc;

          pc->vExpandAlpha16(xv, dv, kUseHi);
          pc->vinv255u16(xv, xv);
          pc->vmulu16(yv, dv, o.vn);
          pc->vsubsu16(dv, dv, o.ux);
          pc->vmulu16(xv, xv, o.uy);
          pc->vdiv255u16(xv);
          pc->vaddi16(dv, dv, xv);
          pc->vmulu16(dv, dv, o.vm);

          pc->vaddi16(dv, dv, yv);
          pc->vdiv255u16(dv);
          out.uc.init(dv);
        }
        // Dca' = Clamp(Dca - Xca).m + Dca.(1 - m)
        // Da'  = <unchanged>
        else {
          dstFetch(d, Pixel::kUC, n);
          VecArray& dv = d.uc;

          pc->vmulu16(yv, dv, o.vn);
          pc->vsubsu16(dv, dv, o.ux);
          pc->vmulu16(dv, dv, o.vm);

          pc->vaddi16(dv, dv, yv);
          pc->vdiv255u16(dv);
          out.uc.init(dv);
        }
      }

      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - Darken / Lighten]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DARKEN || compOp() == BL_COMP_OP_LIGHTEN) {
      // Dca' = minmax(Dca + Xca.(1 - Da), Xca + Dca.Yca)
      // Da'  = Xa + Da.Ya
      dstFetch(d, Pixel::kUC, n);
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vinv255u16(xv, xv);
      pc->vmulu16(xv, xv, o.ux);
      pc->vdiv255u16(xv);
      pc->vaddi16(xv, xv, dv);
      pc->vmulu16(dv, dv, o.uy);
      pc->vdiv255u16(dv);
      pc->vaddi16(dv, dv, o.ux);

      if (compOp() == BL_COMP_OP_DARKEN)
        pc->vminu8(dv, dv, xv);
      else
        pc->vmaxu8(dv, dv, xv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - LinearBurn]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_LINEAR_BURN) {
      // Dca' = Dca + Xca - Yca.Da
      // Da'  = Da  + Xa  - Ya .Da
      dstFetch(d, Pixel::kUC, n);
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(xv, xv, o.uy);
      pc->vaddi16(dv, dv, o.ux);
      pc->vdiv255u16(xv);
      pc->vsubsu16(dv, dv, xv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - Difference]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DIFFERENCE) {
      // Dca' = Dca + Sca - 2.min(Sca.Da, Dca.Sa)
      // Da'  = Da  + Sa  -   min(Sa .Da, Da .Sa)
      dstFetch(d, Pixel::kUC, n);
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(yv, o.uy, dv);
      pc->vmulu16(xv, xv, o.ux);
      pc->vaddi16(dv, dv, o.ux);
      pc->vminu16(yv, yv, xv);
      pc->vdiv255u16(yv);
      pc->vsubi16(dv, dv, yv);
      pc->vZeroAlphaW(yv, yv);
      pc->vsubi16(dv, dv, yv);

      out.uc.init(dv);
      pc->xSatisfyPixel(out, flags);
      return;
    }

    // ------------------------------------------------------------------------
    // [CMaskProc - RGBA32 - Exclusion]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_EXCLUSION) {
      // Dca' = Dca + Xca - 2.Xca.Dca
      // Da'  = Da + Xa - Xa.Da
      dstFetch(d, Pixel::kUC, n);
      VecArray& dv = d.uc;

      pc->vmulu16(xv, dv, o.ux);
      pc->vaddi16(dv, dv, o.ux);
      pc->vdiv255u16(xv);
      pc->vsubi16(dv, dv, xv);
      pc->vZeroAlphaW(xv, xv);
      pc->vsubi16(dv, dv, xv);

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

// ============================================================================
// [BLPipeGen::CompOpPart - VMask - RGBA32 (XMM)]
// ============================================================================

void CompOpPart::vMaskProcRGBA32Xmm(Pixel& out, uint32_t n, uint32_t flags, VecArray& vm, bool mImmutable) noexcept {
  bool hasMask = !vm.empty();

  bool useDa = hasDa();
  bool useSa = hasSa() || hasMask || isLoopCMask();

  uint32_t kFullN = (n + 1) / 2;
  uint32_t kUseHi = (n > 1);
  uint32_t kSplit = (kFullN == 1) ? 1 : 2;

  VecArray xv, yv, zv;
  pc->newXmmArray(xv, kFullN, "x");
  pc->newXmmArray(yv, kFullN, "y");
  pc->newXmmArray(zv, kFullN, "z");

  Pixel d(Pixel::kTypeRGBA);
  Pixel s(Pixel::kTypeRGBA);

  out.setCount(n);

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - SrcCopy]
  // --------------------------------------------------------------------------

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
      srcFetch(s, Pixel::kUC, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& vs = s.uc;
      VecArray& vd = d.uc;
      VecArray vn;

      pc->vmulu16(vs, vs, vm);
      vMaskProcRGBA32InvertMask(vn, vm);

      pc->vmulu16(vd, vd, vn);
      pc->vaddi16(vd, vd, vs);
      vMaskProcRGBA32InvertDone(vn, mImmutable);

      pc->vdiv255u16(vd);
      out.uc.init(vd);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - SrcOver]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_OVER) {
    // Composition:
    //   Da - Optional.
    //   Sa - Required, otherwise SRC_COPY.

    if (!hasMask) {
      // Dca' = Sca + Dca.(1 - Sa)
      // Da'  = Sa  + Da .(1 - Sa)
      srcFetch(s, Pixel::kPC | Pixel::kUIA | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& uv = s.uia;
      VecArray& dv = d.uc;

      pc->vmulu16(dv, dv, uv);
      pc->vdiv255u16(dv);

      VecArray dh = dv.even();
      pc->vpacki16u8(dh, dh, dv.odd());
      pc->vaddi32(dh, dh, s.pc);

      out.pc.init(dh);
    }
    else {
      // Dca' = Sca.m + Dca.(1 - Sa.m)
      // Da'  = Sa .m + Da .(1 - Sa.m)
      srcFetch(s, Pixel::kUC, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);

      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->vinv255u16(xv, xv);
      pc->vmulu16(dv, dv, xv);
      pc->vdiv255u16(dv);

      pc->vaddi16(dv, dv, sv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - SrcIn]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_IN) {
    // Composition:
    //   Da - Required, otherwise SRC_COPY.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Sca.Da
      // Da'  = Sa .Da
      srcFetch(s, Pixel::kUC | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.ua;

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Sca.m.Da + Dca.(1 - m)
      // Da'  = Sa .m.Da + Da .(1 - m)
      srcFetch(s, Pixel::kUC | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(xv, xv, sv);
      pc->vdiv255u16(xv);
      pc->vmulu16(xv, xv, vm);
      vMaskProcRGBA32InvertMask(vm, vm);

      pc->vmulu16(dv, dv, vm);
      vMaskProcRGBA32InvertDone(vm, mImmutable);

      pc->vaddi16(dv, dv, xv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - SrcOut]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_OUT) {
    // Composition:
    //   Da - Required, otherwise CLEAR.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Sca.(1 - Da)
      // Da'  = Sa .(1 - Da)
      srcFetch(s, Pixel::kUC | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUIA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uia;

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Sca.m.(1 - Da) + Dca.(1 - m)
      // Da'  = Sa .m.(1 - Da) + Da .(1 - m)
      srcFetch(s, Pixel::kUC | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vinv255u16(xv, xv);

      pc->vmulu16(xv, xv, sv);
      pc->vdiv255u16(xv);
      pc->vmulu16(xv, xv, vm);
      vMaskProcRGBA32InvertMask(vm, vm);

      pc->vmulu16(dv, dv, vm);
      vMaskProcRGBA32InvertDone(vm, mImmutable);

      pc->vaddi16(dv, dv, xv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - SrcAtop]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_ATOP) {
    // Composition:
    //   Da - Required.
    //   Sa - Required.

    if (!hasMask) {
      // Dca' = Sca.Da + Dca.(1 - Sa)
      // Da'  = Sa .Da + Da .(1 - Sa) = Da
      srcFetch(s, Pixel::kUC | Pixel::kUIA | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uc;
      VecArray& uv = s.uia;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(dv, dv, uv);
      pc->vmulu16(xv, xv, sv);
      pc->vaddi16(dv, dv, xv);
      pc->vdiv255u16(dv);

      out.uc.init(dv);
    }
    else {
      // Dca' = Sca.Da.m + Dca.(1 - Sa.m)
      // Da'  = Sa .Da.m + Da .(1 - Sa.m) = Da
      srcFetch(s, Pixel::kUC, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);

      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->vinv255u16(xv, xv);
      pc->vExpandAlpha16(yv, dv, kUseHi);
      pc->vmulu16(dv, dv, xv);
      pc->vmulu16(yv, yv, sv);
      pc->vaddi16(dv, dv, yv);
      pc->vdiv255u16(dv);

      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Dst]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_COPY) {
    // Dca' = Dca
    // Da'  = Da
    BL_NOT_REACHED();
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - DstOver]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_OVER) {
    // Composition:
    //   Da - Required, otherwise DST_COPY.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Dca + Sca.(1 - Da)
      // Da'  = Da  + Sa .(1 - Da)
      srcFetch(s, Pixel::kUC | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kPC | Pixel::kUIA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uia;

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);

      VecArray dh = dv.even();
      pc->vpacki16u8(dh, dh, dv.odd());
      pc->vaddi32(dh, dh, d.pc);

      out.pc.init(dh);
    }
    else {
      // Dca' = Dca + Sca.m.(1 - Da)
      // Da'  = Da  + Sa .m.(1 - Da)
      srcFetch(s, Pixel::kUC, n);
      dstFetch(d, Pixel::kPC | Pixel::kUIA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uia;

      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);

      VecArray dh = dv.even();
      pc->vpacki16u8(dh, dh, dv.odd());
      pc->vaddi32(dh, dh, d.pc);

      out.pc.init(dh);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - DstIn]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_IN) {
    // Composition:
    //   Da - Optional.
    //   Sa - Required, otherwise DST_COPY.

    if (!hasMask) {
      // Dca' = Dca.Sa
      // Da'  = Da .Sa
      srcFetch(s, Pixel::kUA | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.ua;
      VecArray& dv = d.uc;

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Dca.(1 - m.(1 - Sa))
      // Da'  = Da .(1 - m.(1 - Sa))
      srcFetch(s, Pixel::kUIA, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uia;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);
      pc->vinv255u16(sv, sv);

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - DstOut]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_OUT) {
    // Composition:
    //   Da - Optional.
    //   Sa - Required, otherwise CLEAR.

    if (!hasMask) {
      // Dca' = Dca.(1 - Sa)
      // Da'  = Da .(1 - Sa)
      srcFetch(s, Pixel::kUIA | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uia;
      VecArray& dv = d.uc;

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Dca.(1 - Sa.m)
      // Da'  = Da .(1 - Sa.m)
      srcFetch(s, Pixel::kUA, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.ua;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);
      pc->vinv255u16(sv, sv);

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    if (!useDa) pc->vFillAlpha(out);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - DstAtop]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_ATOP) {
    // Composition:
    //   Da - Required.
    //   Sa - Required.

    if (!hasMask) {
      // Dca' = Dca.Sa + Sca.(1 - Da)
      // Da'  = Da .Sa + Sa .(1 - Da)
      srcFetch(s, Pixel::kUC | Pixel::kUA | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uc;
      VecArray& uv = s.ua;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(dv, dv, uv);
      pc->vinv255u16(xv, xv);
      pc->vmulu16(xv, xv, sv);

      pc->vaddi16(dv, dv, xv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Dca.(1 - m.(1 - Sa)) + Sca.m.(1 - Da)
      // Da'  = Da .(1 - m.(1 - Sa)) + Sa .m.(1 - Da)
      srcFetch(s, Pixel::kUC | Pixel::kUIA, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uc;
      VecArray& uv = s.uia;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(sv, sv, vm);
      pc->vmulu16(uv, uv, vm);

      pc->vdiv255u16(sv);
      pc->vdiv255u16(uv);
      pc->vinv255u16(xv, xv);
      pc->vinv255u16(uv, uv);
      pc->vmulu16(xv, xv, sv);
      pc->vmulu16(dv, dv, uv);

      pc->vaddi16(dv, dv, xv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Xor]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_XOR) {
    // Composition:
    //   Da - Required.
    //   Sa - Required.

    if (!hasMask) {
      // Dca' = Dca.(1 - Sa) + Sca.(1 - Da)
      // Da'  = Da .(1 - Sa) + Sa .(1 - Da)
      srcFetch(s, Pixel::kUC | Pixel::kUIA | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uc;
      VecArray& uv = s.uia;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(dv, dv, uv);
      pc->vinv255u16(xv, xv);
      pc->vmulu16(xv, xv, sv);

      pc->vaddi16(dv, dv, xv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Dca.(1 - Sa.m) + Sca.m.(1 - Da)
      // Da'  = Da .(1 - Sa.m) + Sa .m.(1 - Da)
      srcFetch(s, Pixel::kUC, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);

      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->vExpandAlpha16(yv, dv, kUseHi);
      pc->vinv255u16(xv, xv);
      pc->vinv255u16(yv, yv);
      pc->vmulu16(dv, dv, xv);
      pc->vmulu16(sv, sv, yv);

      pc->vaddi16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Plus]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_PLUS) {
    if (!hasMask) {
      // Dca' = Clamp(Dca + Sca)
      // Da'  = Clamp(Da  + Sa )
      srcFetch(s, Pixel::kPC | Pixel::kImmutable, n);
      dstFetch(d, Pixel::kPC, n);

      VecArray& sh = s.pc;
      VecArray& dh = d.pc;

      pc->vaddsu8(dh, dh, sh);
      out.pc.init(dh);
    }
    else {
      // Dca' = Clamp(Dca + Sca.m)
      // Da'  = Clamp(Da  + Sa .m)
      srcFetch(s, Pixel::kUC, n);
      dstFetch(d, Pixel::kPC, n);

      VecArray& sv = s.uc;
      VecArray& dh = d.pc;

      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);

      VecArray sh = sv.even();
      pc->vpacki16u8(sh, sh, sv.odd());
      pc->vaddsu8(dh, dh, sh);

      out.pc.init(dh);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Minus]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_MINUS) {
    if (!hasMask) {
      // Dca' = Clamp(Dca - Sca) + Sca.(1 - Da)
      // Da'  = Da + Sa.(1 - Da)
      if (useDa) {
        srcFetch(s, Pixel::kUC, n);
        dstFetch(d, Pixel::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->vinv255u16(xv, xv);
        pc->vmulu16(xv, xv, sv);
        pc->vZeroAlphaW(sv, sv);
        pc->vdiv255u16(xv);

        pc->vsubsu16(dv, dv, sv);
        pc->vaddi16(dv, dv, xv);
        out.uc.init(dv);
      }
      // Dca' = Clamp(Dca - Sca)
      // Da'  = <unchanged>
      else {
        srcFetch(s, Pixel::kPC, n);
        dstFetch(d, Pixel::kPC, n);

        VecArray& sh = s.pc;
        VecArray& dh = d.pc;

        pc->vZeroAlphaB(sh, sh);
        pc->vsubsu8(dh, dh, sh);

        out.pc.init(dh);
      }
    }
    else {
      // Dca' = (Clamp(Dca - Sca) + Sca.(1 - Da)).m + Dca.(1 - m)
      // Da'  = Da + Sa.m(1 - Da)
      if (useDa) {
        srcFetch(s, Pixel::kUC, n);
        dstFetch(d, Pixel::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->vmov(yv, dv);
        pc->vinv255u16(xv, xv);
        pc->vsubsu16(dv, dv, sv);
        pc->vmulu16(sv, sv, xv);

        pc->vZeroAlphaW(dv, dv);
        pc->vdiv255u16(sv);
        pc->vaddi16(dv, dv, sv);
        pc->vmulu16(dv, dv, vm);

        pc->vZeroAlphaW(vm, vm);
        pc->vinv255u16(vm, vm);

        pc->vmulu16(yv, yv, vm);

        if (mImmutable) {
          pc->vinv255u16(vm[0], vm[0]);
          pc->vswizi32(vm[0], vm[0], x86::Predicate::shuf(2, 2, 0, 0));
        }

        pc->vaddi16(dv, dv, yv);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Clamp(Dca - Sca).m + Dca.(1 - m)
      // Da'  = <unchanged>
      else {
        srcFetch(s, Pixel::kUC, n);
        dstFetch(d, Pixel::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vinv255u16(xv, vm);
        pc->vZeroAlphaW(sv, sv);

        pc->vmulu16(xv, xv, dv);
        pc->vsubsu16(dv, dv, sv);
        pc->vmulu16(dv, dv, vm);

        pc->vaddi16(dv, dv, xv);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Multiply]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_MULTIPLY) {
    if (!hasMask) {
      // Dca' = Dca.(Sca + 1 - Sa) + Sca.(1 - Da)
      // Da'  = Da .(Sa  + 1 - Sa) + Sa .(1 - Da)
      if (useDa && useSa) {
        srcFetch(s, Pixel::kUC | Pixel::kImmutable, n);
        dstFetch(d, Pixel::kUC, n);

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
          pc->vinv255u16(yh, yh);
          pc->vaddi16(yh, yh, sh);
          pc->vinv255u16(xh, xh);
          pc->vmulu16(dh, dh, yh);
          pc->vmulu16(xh, xh, sh);
          pc->vaddi16(dh, dh, xh);
        }

        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Sc.(Dca + 1 - Da)
      // Da'  = 1 .(Da  + 1 - Da) = 1
      else if (useDa) {
        srcFetch(s, Pixel::kUC | Pixel::kImmutable, n);
        dstFetch(d, Pixel::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->vinv255u16(xv, xv);
        pc->vaddi16(dv, dv, xv);
        pc->vmulu16(dv, dv, sv);

        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
      // Dc'  = Dc.(Sca + 1 - Sa)
      // Da'  = Da.(Sa  + 1 - Sa)
      else if (hasSa()) {
        srcFetch(s, Pixel::kUC | Pixel::kImmutable, n);
        dstFetch(d, Pixel::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, sv, kUseHi);
        pc->vinv255u16(xv, xv);
        pc->vaddi16(xv, xv, sv);
        pc->vmulu16(dv, dv, xv);

        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
      // Dc'  = Dc.Sc
      // Da'  = Da.Sa
      else {
        srcFetch(s, Pixel::kUC | Pixel::kImmutable, n);
        dstFetch(d, Pixel::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vmulu16(dv, dv, sv);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
    }
    else {
      // Dca' = Dca.(Sca.m + 1 - Sa.m) + Sca.m(1 - Da)
      // Da'  = Da .(Sa .m + 1 - Sa.m) + Sa .m(1 - Da)
      if (useDa) {
        srcFetch(s, Pixel::kUC, n);
        dstFetch(d, Pixel::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vmulu16(sv, sv, vm);
        pc->vdiv255u16(sv);

        // SPLIT.
        for (unsigned int i = 0; i < kSplit; i++) {
          VecArray sh = sv.even_odd(i);
          VecArray dh = dv.even_odd(i);
          VecArray xh = xv.even_odd(i);
          VecArray yh = yv.even_odd(i);

          pc->vExpandAlpha16(yh, sh, kUseHi);
          pc->vExpandAlpha16(xh, dh, kUseHi);
          pc->vinv255u16(yh, yh);
          pc->vaddi16(yh, yh, sh);
          pc->vinv255u16(xh, xh);
          pc->vmulu16(dh, dh, yh);
          pc->vmulu16(xh, xh, sh);
          pc->vaddi16(dh, dh, xh);
        }

        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
      else {
        srcFetch(s, Pixel::kUC, n);
        dstFetch(d, Pixel::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vmulu16(sv, sv, vm);
        pc->vdiv255u16(sv);

        pc->vExpandAlpha16(xv, sv, kUseHi);
        pc->vinv255u16(xv, xv);
        pc->vaddi16(xv, xv, sv);
        pc->vmulu16(dv, dv, xv);

        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Overlay]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_OVERLAY) {
    srcFetch(s, Pixel::kUC, n);
    dstFetch(d, Pixel::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);
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

        pc->vmulu16(xh, xh, sh);                                 // Sca.Da
        pc->vmulu16(yh, yh, dh);                                 // Dca.Sa
        pc->vmulu16(zh, dh, sh);                                 // Dca.Sca

        pc->vaddi16(sh, sh, dh);                                 // Dca + Sca
        pc->vsubi16(xh, xh, zh);                                 // Sca.Da - Dca.Sca
        pc->vZeroAlphaW(zh, zh);
        pc->vaddi16(xh, xh, yh);                                 // Dca.Sa + Sca.Da - Dca.Sca
        pc->vExpandAlpha16(yh, dh, kUseHi);                      // Da
        pc->vsubi16(xh, xh, zh);                                 // [C=Dca.Sa + Sca.Da - 2.Dca.Sca] [A=Sa.Da]

        pc->vslli16(dh, dh, 1);                                  // 2.Dca
        pc->vcmpgti16(yh, yh, dh);                               // 2.Dca < Da
        pc->vdiv255u16(xh);
        pc->vor(yh, yh, C_MEM(i128_FFFF000000000000));

        pc->vExpandAlpha16(zh, xh, kUseHi);
        // if (2.Dca < Da)
        //   X = [C = -(Dca.Sa + Sca.Da - 2.Sca.Dca)] [A = -Sa.Da]
        // else
        //   X = [C =  (Dca.Sa + Sca.Da - 2.Sca.Dca)] [A = -Sa.Da]
        pc->vxor(xh, xh, yh);
        pc->vsubi16(xh, xh, yh);

        // if (2.Dca < Da)
        //   Y = [C = 0] [A = 0]
        // else
        //   Y = [C = Sa.Da] [A = 0]
        pc->vandnot_a(yh, yh, zh);

        pc->vaddi16(sh, sh, xh);
        pc->vsubi16(sh, sh, yh);
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

      pc->vExpandAlpha16(xv, dv, kUseHi);                        // Da
      pc->vslli16(dv, dv, 1);                                    // 2.Dca

      pc->vcmpgti16(yv, xv, dv);                                 //  (2.Dca < Da) ? -1 : 0
      pc->vsubi16(xv, xv, dv);                                   // -(2.Dca - Da)

      pc->vxor(xv, xv, yv);
      pc->vsubi16(xv, xv, yv);                                   // 2.Dca < Da ? 2.Dca - Da : -(2.Dca - Da)
      pc->vandnot_a(yv, yv, xv);                                 // 2.Dca < Da ? 0          : -(2.Dca - Da)
      pc->vaddi16(xv, xv, C_MEM(i128_00FF00FF00FF00FF));

      pc->vmulu16(xv, xv, sv);
      pc->vdiv255u16(xv);
      pc->vsubi16(xv, xv, yv);

      out.uc.init(xv);
    }
    else {
      // if (2.Dc < 1)
      //   Dc'  = 2.Dc.Sc
      // else
      //   Dc'  = 2.Dc + 2.Sc - 1 - 2.Dc.Sc

      pc->vmulu16(xv, dv, sv);                                   // Dc.Sc
      pc->vcmpgti16(yv, dv, C_MEM(i128_007F007F007F007F));       // !(2.Dc < 1)
      pc->vaddi16(dv, dv, sv);                                   // Dc + Sc
      pc->vdiv255u16(xv);

      pc->vslli16(dv, dv, 1);                                    // 2.Dc + 2.Sc
      pc->vslli16(xv, xv, 1);                                    // 2.Dc.Sc
      pc->vsubi16(dv, dv, C_MEM(i128_00FF00FF00FF00FF));         // 2.Dc + 2.Sc - 1

      pc->vxor(xv, xv, yv);
      pc->vand(dv, dv, yv);                                      // 2.Dc < 1 ? 0 : 2.Dc + 2.Sc - 1
      pc->vsubi16(xv, xv, yv);                                   // 2.Dc < 1 ? 2.Dc.Sc : -2.Dc.Sc
      pc->vaddi16(dv, dv, xv);                                   // 2.Dc < 1 ? 2.Dc.Sc : 2.Dc + 2.Sc - 1 - 2.Dc.Sc

      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Screen]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SCREEN) {
    // Dca' = Sca + Dca.(1 - Sca)
    // Da'  = Sa  + Da .(1 - Sa)
    srcFetch(s, Pixel::kUC | (hasMask ? uint32_t(0) : Pixel::kImmutable), n);
    dstFetch(d, Pixel::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);
    }

    pc->vinv255u16(xv, sv);
    pc->vmulu16(dv, dv, xv);
    pc->vdiv255u16(dv);
    pc->vaddi16(dv, dv, sv);

    out.uc.init(dv);
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Darken / Lighten]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DARKEN || compOp() == BL_COMP_OP_LIGHTEN) {
    srcFetch(s, Pixel::kUC, n);
    dstFetch(d, Pixel::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    bool minMaxPredicate = compOp() == BL_COMP_OP_DARKEN;

    if (hasMask) {
      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);
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

        pc->vinv255u16(xh, xh);
        pc->vinv255u16(yh, yh);

        pc->vmulu16(xh, xh, sh);
        pc->vmulu16(yh, yh, dh);
        pc->vdiv255u16_2x(xh, yh);

        pc->vaddi16(dh, dh, xh);
        pc->vaddi16(sh, sh, yh);

        pc->vminmaxu8(dh, dh, sh, minMaxPredicate);
      }

      out.uc.init(dv);
    }
    else if (useDa) {
      // Dca' = minmax(Dca + Sc.(1 - Da), Sc)
      // Da'  = 1
      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vinv255u16(xv, xv);
      pc->vmulu16(xv, xv, sv);
      pc->vdiv255u16(xv);
      pc->vaddi16(dv, dv, xv);
      pc->vminmaxu8(dv, dv, sv, minMaxPredicate);

      out.uc.init(dv);
    }
    else if (useSa) {
      // Dc' = minmax(Dc, Sca + Dc.(1 - Sa))
      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->vinv255u16(xv, xv);
      pc->vmulu16(xv, xv, dv);
      pc->vdiv255u16(xv);
      pc->vaddi16(xv, xv, sv);
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

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - ColorDodge (SCALAR)]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_COLOR_DODGE && n == 1) {
    // Dca' = min(Dca.Sa.Sa / max(Sa - Sca, 0.001), Sa.Da) + Sca.(1 - Da) + Dca.(1 - Sa);
    // Da'  = min(Da .Sa.Sa / max(Sa - Sa , 0.001), Sa.Da) + Sa .(1 - Da) + Da .(1 - Sa);

    srcFetch(s, Pixel::kUC, n);
    dstFetch(d, Pixel::kPC, n);

    x86::Vec& s0 = s.uc[0];
    x86::Vec& d0 = d.pc[0];
    x86::Vec& x0 = xv[0];
    x86::Vec& y0 = yv[0];
    x86::Vec& z0 = zv[0];

    if (hasMask) {
      pc->vmulu16(s0, s0, vm[0]);
      pc->vdiv255u16(s0);
    }

    pc->vmovu8u32(d0, d0);
    pc->vmovu16u32(s0, s0);

    pc->vcvti32ps(y0, s0);
    pc->vcvti32ps(z0, d0);
    pc->vpacki32i16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);
    pc->vxorps(y0, y0, C_MEM(f128_sgn));
    pc->vmulps(z0, z0, x0);
    pc->vandps(y0, y0, C_MEM(i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0));
    pc->vaddps(y0, y0, x0);

    pc->vmaxps(y0, y0, C_MEM(f128_1e_m3));
    pc->vdivps(z0, z0, y0);

    pc->vswizi32(s0, d0, x86::Predicate::shuf(1, 1, 3, 3));
    pc->vExpandAlphaHi16(s0, s0);
    pc->vExpandAlphaLo16(s0, s0);
    pc->vinv255u16(s0, s0);
    pc->vmulu16(d0, d0, s0);
    pc->vswizi32(s0, d0, x86::Predicate::shuf(1, 0, 3, 2));
    pc->vaddi16(d0, d0, s0);

    pc->vmulps(z0, z0, x0);
    pc->vExpandAlphaPS(x0, z0);
    pc->vminps(z0, z0, x0);

    pc->vcvttpsi32(z0, z0);
    pc->xPackU32ToU16Lo(z0, z0);
    pc->vaddi16(d0, d0, z0);

    pc->vdiv255u16(d0);
    out.uc.init(d0);

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - ColorBurn (SCALAR)]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_COLOR_BURN && n == 1) {
    // Dca' = Sa.Da - min(Sa.Da, (Da - Dca).Sa.Sa / max(Sca, 0.001)) + Sca.(1 - Da) + Dca.(1 - Sa)
    // Da'  = Sa.Da - min(Sa.Da, (Da - Da ).Sa.Sa / max(Sa , 0.001)) + Sa .(1 - Da) + Da .(1 - Sa)
    srcFetch(s, Pixel::kUC, n);
    dstFetch(d, Pixel::kPC, n);

    x86::Vec& s0 = s.uc[0];
    x86::Vec& d0 = d.pc[0];
    x86::Vec& x0 = xv[0];
    x86::Vec& y0 = yv[0];
    x86::Vec& z0 = zv[0];

    if (hasMask) {
      pc->vmulu16(s0, s0, vm[0]);
      pc->vdiv255u16(s0);
    }

    pc->vmovu8u32(d0, d0);
    pc->vmovu16u32(s0, s0);

    pc->vcvti32ps(y0, s0);
    pc->vcvti32ps(z0, d0);
    pc->vpacki32i16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);
    pc->vmaxps(y0, y0, C_MEM(f128_1e_m3));
    pc->vmulps(z0, z0, x0);                                      // Dca.Sa

    pc->vExpandAlphaPS(x0, z0);                                  // Sa.Da
    pc->vxorps(z0, z0, C_MEM(f128_sgn));

    pc->vandps(z0, z0, C_MEM(i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0));
    pc->vaddps(z0, z0, x0);                                      // (Da - Dxa).Sa
    pc->vdivps(z0, z0, y0);

    pc->vswizi32(s0, d0, x86::Predicate::shuf(1, 1, 3, 3));
    pc->vExpandAlphaHi16(s0, s0);
    pc->vExpandAlphaLo16(s0, s0);
    pc->vinv255u16(s0, s0);
    pc->vmulu16(d0, d0, s0);
    pc->vswizi32(s0, d0, x86::Predicate::shuf(1, 0, 3, 2));
    pc->vaddi16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);                                  // Sa
    pc->vmulps(z0, z0, x0);
    pc->vExpandAlphaPS(x0, z0);                                  // Sa.Da
    pc->vminps(z0, z0, x0);
    pc->vandps(z0, z0, C_MEM(i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0));
    pc->vsubps(x0, x0, z0);

    pc->vcvttpsi32(x0, x0);
    pc->xPackU32ToU16Lo(x0, x0);
    pc->vaddi16(d0, d0, x0);

    pc->vdiv255u16(d0);
    out.uc.init(d0);

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - LinearBurn]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_LINEAR_BURN) {
    srcFetch(s, Pixel::kUC | (hasMask ? uint32_t(0) : Pixel::kImmutable), n);
    dstFetch(d, Pixel::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);
    }

    if (useDa && useSa) {
      // Dca' = Dca + Sca - Sa.Da
      // Da'  = Da  + Sa  - Sa.Da
      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->vExpandAlpha16(yv, dv, kUseHi);
      pc->vmulu16(xv, xv, yv);
      pc->vdiv255u16(xv);
      pc->vaddi16(dv, dv, sv);
      pc->vsubsu16(dv, dv, xv);
    }
    else if (useDa || useSa) {
      pc->vExpandAlpha16(xv, useDa ? dv : sv, kUseHi);
      pc->vaddi16(dv, dv, sv);
      pc->vsubsu16(dv, dv, xv);
    }
    else {
      // Dca' = Dc + Sc - 1
      pc->vaddi16(dv, dv, sv);
      pc->vsubsu16(dv, dv, C_MEM(i128_000000FF00FF00FF));
    }

    out.uc.init(dv);
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - LinearLight]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_LINEAR_LIGHT && n == 1) {
    srcFetch(s, Pixel::kUC, 1);
    dstFetch(d, Pixel::kUC, 1);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);
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

      pc->vunpackli64(d0, d0, s0);
      pc->vunpackli64(x0, x0, y0);

      pc->vmov(s0, d0);
      pc->vmulu16(d0, d0, x0);
      pc->vinv255u16(x0, x0);
      pc->vdiv255u16(d0);

      pc->vmulu16(s0, s0, x0);
      pc->vswapi64(x0, s0);
      pc->vswapi64(y0, d0);
      pc->vaddi16(s0, s0, x0);
      pc->vaddi16(d0, d0, y0);
      pc->vExpandAlphaLo16(x0, y0);
      pc->vaddi16(d0, d0, y0);
      pc->vdiv255u16(s0);

      pc->vsubsu16(d0, d0, x0);
      pc->vmini16(d0, d0, x0);

      pc->vaddi16(d0, d0, s0);
      out.uc.init(d0);
    }
    else {
      // Dc' = min(max((Dc + 2.Sc - 1), 0), 1)
      pc->vslli16(sv, sv, 1);
      pc->vaddi16(dv, dv, sv);
      pc->vsubsu16(dv, dv, C_MEM(i128_000000FF00FF00FF));
      pc->vmini16(dv, dv, C_MEM(i128_00FF00FF00FF00FF));

      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - PinLight]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_PIN_LIGHT) {
    srcFetch(s, Pixel::kUC, n);
    dstFetch(d, Pixel::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);

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

      pc->vmulu16(yv, yv, dv);                                     // Dca.Sa
      pc->vmulu16(xv, xv, sv);                                     // Sca.Da
      pc->vaddi16(dv, dv, sv);                                     // Dca + Sca
      pc->vdiv255u16_2x(yv, xv);

      pc->vsubi16(yv, yv, dv);                                     // Dca.Sa - Dca - Sca
      pc->vsubi16(dv, dv, xv);                                     // Dca + Sca - Sca.Da
      pc->vsubi16(xv, xv, yv);                                     // Dca + Sca + Sca.Da - Dca.Sa

      pc->vExpandAlpha16(yv, sv, kUseHi);                          // Sa
      pc->vslli16(sv, sv, 1);                                      // 2.Sca
      pc->vcmpgti16(sv, sv, yv);                                   // !(2.Sca <= Sa)

      pc->vsubi16(zv, dv, xv);
      pc->vExpandAlpha16(zv, zv, kUseHi);                          // -Da.Sa
      pc->vand(zv, zv, sv);                                        // 2.Sca <= Sa ? 0 : -Da.Sa
      pc->vaddi16(xv, xv, zv);                                     // 2.Sca <= Sa ? Dca + Sca + Sca.Da - Dca.Sa : Dca + Sca + Sca.Da - Dca.Sa - Da.Sa

      // if 2.Sca <= Sa:
      //   min(dv, xv)
      // else
      //   max(dv, xv) <- ~min(~dv, ~xv)
      pc->vxor(dv, dv, sv);
      pc->vxor(xv, xv, sv);
      pc->vmini16(dv, dv, xv);
      pc->vxor(dv, dv, sv);

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
      pc->vmulu16(xv, xv, sv);                                     // Sc.Da
      pc->vaddi16(dv, dv, sv);                                     // Dca + Sc
      pc->vdiv255u16(xv);

      pc->vcmpgti16(yv, sv, C_MEM(i128_007F007F007F007F));         // !(2.Sc <= 1)
      pc->vaddi16(sv, sv, xv);                                     // Sc + Sc.Da
      pc->vsubi16(dv, dv, xv);                                     // Dca + Sc - Sc.Da
      pc->vExpandAlpha16(xv, xv);                                  // Da
      pc->vand(xv, xv, yv);                                        // 2.Sc <= 1 ? 0 : Da
      pc->vsubi16(sv, sv, xv);                                     // 2.Sc <= 1 ? Sc + Sc.Da : Sc + Sc.Da - Da

      // if 2.Sc <= 1:
      //   min(dv, sv)
      // else
      //   max(dv, sv) <- ~min(~dv, ~sv)
      pc->vxor(dv, dv, yv);
      pc->vxor(sv, sv, yv);
      pc->vmini16(dv, dv, sv);
      pc->vxor(dv, dv, yv);

      out.uc.init(dv);
    }
    else if (useSa) {
      // if 2.Sca <= Sa
      //   Dc' = min(Dc, Dc + 2.Sca - Dc.Sa)
      // else
      //   Dc' = max(Dc, Dc + 2.Sca - Dc.Sa - Sa)

      pc->vExpandAlpha16(xv, sv, kUseHi);                          // Sa
      pc->vslli16(sv, sv, 1);                                      // 2.Sca
      pc->vcmpgti16(yv, sv, xv);                                   // !(2.Sca <= Sa)
      pc->vand(yv, yv, xv);                                        // 2.Sca <= Sa ? 0 : Sa
      pc->vmulu16(xv, xv, dv);                                     // Dc.Sa
      pc->vaddi16(sv, sv, dv);                                     // Dc + 2.Sca
      pc->vdiv255u16(xv);
      pc->vsubi16(sv, sv, yv);                                     // 2.Sca <= Sa ? Dc + 2.Sca : Dc + 2.Sca - Sa
      pc->vcmpeqi16(yv, yv, C_MEM(i128_0000000000000000));         // 2.Sc <= 1
      pc->vsubi16(sv, sv, xv);                                     // 2.Sca <= Sa ? Dc + 2.Sca - Dc.Sa : Dc + 2.Sca - Dc.Sa - Sa

      // if 2.Sc <= 1:
      //   min(dv, sv)
      // else
      //   max(dv, sv) <- ~min(~dv, ~sv)
      pc->vxor(dv, dv, yv);
      pc->vxor(sv, sv, yv);
      pc->vmaxi16(dv, dv, sv);
      pc->vxor(dv, dv, yv);

      out.uc.init(dv);
    }
    else {
      // if 2.Sc <= 1
      //   Dc' = min(Dc, 2.Sc)
      // else
      //   Dc' = max(Dc, 2.Sc - 1)

      pc->vslli16(sv, sv, 1);                                      // 2.Sc
      pc->vmini16(xv, sv, dv);                                     // min(Dc, 2.Sc)

      pc->vcmpgti16(yv, sv, C_MEM(i128_00FF00FF00FF00FF));         // !(2.Sc <= 1)
      pc->vsubi16(sv, sv, C_MEM(i128_00FF00FF00FF00FF));           // 2.Sc - 1
      pc->vmaxi16(dv, dv, sv);                                     // max(Dc, 2.Sc - 1)

      pc->vblendv8_destructive(xv, xv, dv, yv);                    // 2.Sc <= 1 ? min(Dc, 2.Sc) : max(Dc, 2.Sc - 1)
      out.uc.init(xv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - HardLight]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_HARD_LIGHT) {
    // if (2.Sca < Sa)
    //   Dca' = Dca + Sca - (Dca.Sa + Sca.Da - 2.Sca.Dca)
    //   Da'  = Da  + Sa  - Sa.Da
    // else
    //   Dca' = Dca + Sca + (Dca.Sa + Sca.Da - 2.Sca.Dca) - Sa.Da
    //   Da'  = Da  + Sa  - Sa.Da
    srcFetch(s, Pixel::kUC, n);
    dstFetch(d, Pixel::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);
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

      pc->vmulu16(xh, xh, sh);                                   // Sca.Da
      pc->vmulu16(yh, yh, dh);                                   // Dca.Sa
      pc->vmulu16(zh, dh, sh);                                   // Dca.Sca

      pc->vaddi16(dh, dh, sh);
      pc->vsubi16(xh, xh, zh);
      pc->vaddi16(xh, xh, yh);
      pc->vsubi16(xh, xh, zh);

      pc->vExpandAlpha16(yh, yh, kUseHi);
      pc->vExpandAlpha16(zh, sh, kUseHi);
      pc->vdiv255u16_2x(xh, yh);

      pc->vslli16(sh, sh, 1);
      pc->vcmpgti16(zh, zh, sh);

      pc->vxor(xh, xh, zh);
      pc->vsubi16(xh, xh, zh);
      pc->vZeroAlphaW(zh, zh);
      pc->vandnot_a(zh, zh, yh);
      pc->vaddi16(dh, dh, xh);
      pc->vsubi16(dh, dh, zh);
    }

    out.uc.init(dv);
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - SoftLight (SCALAR)]
  // --------------------------------------------------------------------------

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
    srcFetch(s, Pixel::kUC, n);
    dstFetch(d, Pixel::kPC, n);

    x86::Vec& s0 = s.uc[0];
    x86::Vec& d0 = d.pc[0];

    x86::Vec  a0 = cc->newXmm("a0");
    x86::Vec  b0 = cc->newXmm("b0");
    x86::Vec& x0 = xv[0];
    x86::Vec& y0 = yv[0];
    x86::Vec& z0 = zv[0];

    if (hasMask) {
      pc->vmulu16(s0, s0, vm[0]);
      pc->vdiv255u16(s0);
    }

    pc->vmovu8u32(d0, d0);
    pc->vmovu16u32(s0, s0);
    pc->vloadps_128a(x0, C_MEM(f128_1div255));

    pc->vcvti32ps(s0, s0);
    pc->vcvti32ps(d0, d0);

    pc->vmulps(s0, s0, x0);                                      // Sca (0..1)
    pc->vmulps(d0, d0, x0);                                      // Dca (0..1)

    pc->vExpandAlphaPS(b0, d0);                                  // Da
    pc->vmulps(x0, s0, b0);                                      // Sca.Da
    pc->vmaxps(b0, b0, C_MEM(f128_1e_m3));                       // max(Da, 0.001)

    pc->vdivps(a0, d0, b0);                                      // Dc <- Dca/Da
    pc->vaddps(d0, d0, s0);                                      // Dca + Sca

    pc->vExpandAlphaPS(y0, s0);                                  // Sa
    pc->vloadps_128a(z0, C_MEM(f128_4));                         // 4

    pc->vsubps(d0, d0, x0);                                      // Dca + Sca.(1 - Da)
    pc->vaddps(s0, s0, s0);                                      // 2.Sca
    pc->vmulps(z0, z0, a0);                                      // 4.Dc

    pc->vsqrtps(x0, a0);                                         // sqrt(Dc)
    pc->vsubps(s0, s0, y0);                                      // 2.Sca - Sa

    pc->vmovaps(y0, z0);                                         // 4.Dc
    pc->vmulps(z0, z0, a0);                                      // 4.Dc.Dc

    pc->vaddps(z0, z0, a0);                                      // 4.Dc.Dc + Dc
    pc->vmulps(s0, s0, b0);                                      // (2.Sca - Sa).Da

    pc->vsubps(z0, z0, y0);                                      // 4.Dc.Dc + Dc - 4.Dc
    pc->vloadps_128a(b0, C_MEM(f128_1));                         // 1

    pc->vaddps(z0, z0, b0);                                      // 4.Dc.Dc + Dc - 4.Dc + 1
    pc->vmulps(z0, z0, y0);                                      // 4.Dc(4.Dc.Dc + Dc - 4.Dc + 1)
    pc->vcmpps(y0, y0, b0, x86::Predicate::kCmpLE);              // 4.Dc <= 1

    pc->vandps(z0, z0, y0);
    pc->vandnot_aps(y0, y0, x0);

    pc->vzerops(x0);
    pc->vorps(z0, z0, y0);                                       // (4.Dc(4.Dc.Dc + Dc - 4.Dc + 1)) or sqrt(Dc)

    pc->vcmpps(x0, x0, s0, x86::Predicate::kCmpLT);              // 2.Sca - Sa > 0
    pc->vsubps(z0, z0, a0);                                      // [[4.Dc(4.Dc.Dc + Dc - 4.Dc + 1) or sqrt(Dc)]] - Dc

    pc->vsubps(b0, b0, a0);                                      // 1 - Dc
    pc->vandps(z0, z0, x0);

    pc->vmulps(b0, b0, a0);                                      // Dc.(1 - Dc)
    pc->vandnot_aps(x0, x0, b0);
    pc->vandps(s0, s0, C_MEM(i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0)); // Zero alpha.

    pc->vorps(z0, z0, x0);
    pc->vmulps(s0, s0, z0);

    pc->vaddps(d0, d0, s0);
    pc->vmulps(d0, d0, C_MEM(f128_255));

    pc->vcvtpsi32(d0, d0);
    pc->vpacki32i16(d0, d0, d0);
    pc->vpacki16u8(d0, d0, d0);
    out.pc.init(d0);

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Difference]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DIFFERENCE) {
    // Dca' = Dca + Sca - 2.min(Sca.Da, Dca.Sa)
    // Da'  = Da  + Sa  -   min(Sa .Da, Da .Sa)
    if (!hasMask) {
      srcFetch(s, Pixel::kUC | Pixel::kUA, n);
      dstFetch(d, Pixel::kUC, n);

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
        pc->vmulu16(uh, uh, dh);
        pc->vmulu16(xh, xh, sh);
        pc->vaddi16(dh, dh, sh);
        pc->vminu16(uh, uh, xh);
      }

      pc->vdiv255u16(uv);
      pc->vsubi16(dv, dv, uv);

      pc->vZeroAlphaW(uv, uv);
      pc->vsubi16(dv, dv, uv);
      out.uc.init(dv);
    }
    // Dca' = Dca + Sca.m - 2.min(Sca.Da, Dca.Sa).m
    // Da'  = Da  + Sa .m -   min(Sa .Da, Da .Sa).m
    else {
      srcFetch(s, Pixel::kUC, n);
      dstFetch(d, Pixel::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);

      // SPLIT.
      for (unsigned int i = 0; i < kSplit; i++) {
        VecArray sh = sv.even_odd(i);
        VecArray dh = dv.even_odd(i);
        VecArray xh = xv.even_odd(i);
        VecArray yh = yv.even_odd(i);

        pc->vExpandAlpha16(yh, sh, kUseHi);
        pc->vExpandAlpha16(xh, dh, kUseHi);
        pc->vmulu16(yh, yh, dh);
        pc->vmulu16(xh, xh, sh);
        pc->vaddi16(dh, dh, sh);
        pc->vminu16(yh, yh, xh);
      }

      pc->vdiv255u16(yv);
      pc->vsubi16(dv, dv, yv);

      pc->vZeroAlphaW(yv, yv);
      pc->vsubi16(dv, dv, yv);
      out.uc.init(dv);
    }

    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Exclusion]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_EXCLUSION) {
    // Dca' = Dca + Sca - 2.Sca.Dca
    // Da'  = Da + Sa - Sa.Da
    srcFetch(s, Pixel::kUC | (hasMask ? uint32_t(0) : Pixel::kImmutable), n);
    dstFetch(d, Pixel::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, vm);
      pc->vdiv255u16(sv);
    }

    pc->vmulu16(xv, dv, sv);
    pc->vaddi16(dv, dv, sv);
    pc->vdiv255u16(xv);
    pc->vsubi16(dv, dv, xv);

    pc->vZeroAlphaW(xv, xv);
    pc->vsubi16(dv, dv, xv);

    out.uc.init(dv);
    pc->xSatisfyPixel(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VMaskProc - RGBA32 - Invalid]
  // --------------------------------------------------------------------------

  BL_NOT_REACHED();
}

void CompOpPart::vMaskProcRGBA32InvertMask(VecArray& vn, VecArray& vm) noexcept {
  uint32_t i;
  uint32_t size = vm.size();

  if (cMaskLoopType() == kCMaskLoopTypeMask) {
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
    pc->newXmmArray(vn, size, "vn");

  if (vm.isScalar()) {
    // TODO: Seems wrong as well, the `vmov` code-path would never execute.
    pc->vinv255u16(vn[0], vm[0]);
    for (i = 1; i < size; i++)
      pc->vmov(vn[i], vn[0]);
  }
  else {
    pc->vinv255u16(vn, vm);
  }
}

void CompOpPart::vMaskProcRGBA32InvertDone(VecArray& vn, bool mImmutable) noexcept {
  BL_UNUSED(mImmutable);

  if (cMaskLoopType() == kCMaskLoopTypeMask) {
    if (vn[0].id() == _mask->vm.id())
      pc->vinv255u16(vn, vn);
  }
}

} // {BLPipeGen}

#endif
