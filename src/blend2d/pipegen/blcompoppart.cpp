// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../pipegen/blcompoppart_p.h"
#include "../pipegen/blfetchpart_p.h"
#include "../pipegen/blfetchpatternpart_p.h"
#include "../pipegen/blfetchpixelptrpart_p.h"
#include "../pipegen/blfetchsolidpart_p.h"
#include "../pipegen/blpipecompiler_p.h"

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::CompOpPart - Construction / Destruction]
// ============================================================================

CompOpPart::CompOpPart(PipeCompiler* pc, uint32_t compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept
  : PipePart(pc, kTypeComposite),
    _compOp(compOp),
    _cMaskLoopType(kCMaskLoopTypeNone),
    _maxPixels(1),
    _pixelGranularity(0),
    _minAlignment(1),
    _hasDa(dstPart->hasAlpha()),
    _hasSa(srcPart->hasAlpha()),
    _cMaskLoopHook(nullptr) {

  // Initialize the children of this part.
  _children[kIndexDstPart] = dstPart;
  _children[kIndexSrcPart] = srcPart;
  _childrenCount = 2;

  _maxOptLevelSupported = kOptLevel_X86_AVX;

  bool isSolid = srcPart->isSolid();
  uint32_t maxPixels = 0;

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

  if (maxPixels > 4) {
    // Decrease the maximum pixel-step to 4 it the style is not solid and the
    // application is not 64-bit. There's not enough registers to process
    // 8 pixels in parallel in 32-bit mode.
    if (BL_TARGET_ARCH_BITS < 64 && !isSolid) {
      maxPixels = 4;
    }
    // Decrease the maximum pixels to 4 if the source is complex to fetch.
    // In such case fetching and processing more pixels is causing to emit
    // bloated pipelines that are not faster compared to pipelines working
    // with just 4 pixels at a time.
    else if (dstPart->isComplexFetch() || srcPart->isComplexFetch()) {
      maxPixels = 4;
    }
  }

  // Descrease to N pixels at a time if the fetch part doesn't support more.
  // This is suboptimal, but can happen if the fetch part is not optimized.
  maxPixels = blMin(maxPixels, srcPart->maxPixels());

  if (maxPixels >= 4)
    _minAlignment = 16;

  _maxPixels = uint8_t(maxPixels);
  _mask->reset();
}

// ============================================================================
// [BLPipeGen::CompOpPart - Init / Fini]
// ============================================================================

void CompOpPart::init(x86::Gp& x, x86::Gp& y, uint32_t pixelGranularity) noexcept {
  _pixelGranularity = uint8_t(pixelGranularity);

  dstPart()->init(x, y, pixelGranularity);
  srcPart()->init(x, y, pixelGranularity);
}

void CompOpPart::fini() noexcept {
  dstPart()->fini();
  srcPart()->fini();

  _pixelGranularity = 0;
}

// ============================================================================
// [BLPipeGen::CompOpPart - Decision Making]
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

bool CompOpPart::shouldMemcpyOrMemsetOpaqueFill() const noexcept {
  if (compOp() != BL_COMP_OP_SRC_COPY)
    return false;

  return srcPart()->isSolid() ||
         srcPart()->isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT);
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

void CompOpPart::dstFetch32(PixelARGB& p, uint32_t flags, uint32_t n) noexcept {
  switch (n) {
    case 1: dstPart()->fetch1(p, flags); break;
    case 4: dstPart()->fetch4(p, flags); break;
    case 8: dstPart()->fetch8(p, flags); break;
  }
}

void CompOpPart::srcFetch32(PixelARGB& p, uint32_t flags, uint32_t n) noexcept {
  if (isUsingSolidPre()) {
    PixelARGB& s = _solidPre;

    // INJECT:
    {
      ScopedInjector injector(cc, &_cMaskLoopHook);
      pc->xSatisfySolid(s, flags);
    }

    if (flags & PixelARGB::kImmutable) {
      if (flags & PixelARGB::kPC) p.pc.init(s.pc[0]);
      if (flags & PixelARGB::kUC) p.uc.init(s.uc[0]);
      if (flags & PixelARGB::kUA) p.ua.init(s.ua[0]);
      if (flags & PixelARGB::kUIA) p.uia.init(s.uia[0]);
    }
    else {
      switch (n) {
        case 1:
          if (flags & PixelARGB::kPC) { p.pc.init(cc->newXmm("pre.pc")); pc->vmov(p.pc[0], s.pc[0]); }
          if (flags & PixelARGB::kUC) { p.uc.init(cc->newXmm("pre.uc")); pc->vmov(p.uc[0], s.uc[0]); }
          if (flags & PixelARGB::kUA) { p.ua.init(cc->newXmm("pre.ua")); pc->vmov(p.ua[0], s.ua[0]); }
          if (flags & PixelARGB::kUIA) { p.uia.init(cc->newXmm("pre.uia")); pc->vmov(p.uia[0], s.uia[0]); }
          break;

        case 4:
          if (flags & PixelARGB::kPC) {
            pc->newXmmArray(p.pc, 1, "pre.pc");
            pc->vmov(p.pc[0], s.pc[0]);
          }

          if (flags & PixelARGB::kUC) {
            pc->newXmmArray(p.uc, 2, "pre.uc");
            pc->vmov(p.uc[0], s.uc[0]);
            pc->vmov(p.uc[1], s.uc[0]);
          }

          if (flags & PixelARGB::kUA) {
            pc->newXmmArray(p.ua, 2, "pre.ua");
            pc->vmov(p.ua[0], s.ua[0]);
            pc->vmov(p.ua[1], s.ua[0]);
          }

          if (flags & PixelARGB::kUIA) {
            pc->newXmmArray(p.uia, 2, "pre.uia");
            pc->vmov(p.uia[0], s.uia[0]);
            pc->vmov(p.uia[1], s.uia[0]);
          }
          break;

        case 8:
          if (flags & PixelARGB::kPC) {
            pc->newXmmArray(p.pc, 2, "pre.pc");
            pc->vmov(p.pc[0], s.pc[0]);
            pc->vmov(p.pc[1], s.pc[0]);
          }

          if (flags & PixelARGB::kUC) {
            pc->newXmmArray(p.uc, 4, "pre.uc");
            pc->vmov(p.uc[0], s.uc[0]);
            pc->vmov(p.uc[1], s.uc[0]);
            pc->vmov(p.uc[2], s.uc[0]);
            pc->vmov(p.uc[3], s.uc[0]);
          }

          if (flags & PixelARGB::kUA) {
            pc->newXmmArray(p.ua, 4, "pre.ua");
            pc->vmov(p.ua[0], s.ua[0]);
            pc->vmov(p.ua[1], s.ua[0]);
            pc->vmov(p.ua[2], s.ua[0]);
            pc->vmov(p.ua[3], s.ua[0]);
          }

          if (flags & PixelARGB::kUIA) {
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
  else {
    if (isInPartialMode()) {
      // Partial mode is designed to fetch pixels on the right side of the
      // border one by one, so it's an error if the pipeline requests more
      // than 1 pixel at a time.
      BL_ASSERT(n == 1);

      if (!(flags & PixelARGB::kImmutable)) {
        if (flags & PixelARGB::kUC) {
          pc->newXmmArray(p.uc, 1, "uc");
          pc->vmovu8u16(p.uc[0], _pixPartial.pc[0]);
        }
        else {
          pc->newXmmArray(p.pc, 1, "pc");
          pc->vmov(p.pc[0], _pixPartial.pc[0]);
        }
      }
      else {
        p.pc.init(_pixPartial.pc[0]);
      }

      pc->xSatisfyARGB32_1x(p, flags);
    }
    else {
      switch (n) {
        case 1: srcPart()->fetch1(p, flags); break;
        case 4: srcPart()->fetch4(p, flags); break;
        case 8: srcPart()->fetch8(p, flags); break;
      }
    }
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
  BL_ASSERT(pixelGranularity() == 4);

  BL_ASSERT(!isInPartialMode());
  srcFetch32(_pixPartial, PixelARGB::kPC | partialFlags, pixelGranularity());
}

void CompOpPart::exitPartialMode() noexcept {
  // Doesn't apply to solid fills.
  if (isUsingSolidPre())
    return;

  BL_ASSERT(isInPartialMode());
  _pixPartial.reset();
}

void CompOpPart::nextPartialPixel() noexcept {
  if (isInPartialMode()) {
    const x86::Vec& pix = _pixPartial.pc[0];
    pc->vsrli128b(pix, pix, 4);
  }
}

// ============================================================================
// [BLPipeGen::CompOpPart - CMask (Any)]
// ============================================================================

void CompOpPart::cMaskInit(x86::Gp& mask) noexcept {
  if (!pc->hasAVX2()) {
    x86::Vec mv;
    if (mask.isValid()) {
      mv = cc->newXmm("c.mv");
      pc->vmovsi32(mv, mask);
      pc->vswizli16(mv, mv, x86::Predicate::shuf(0, 0, 0, 0));
      // TODO: [PIPEGEN] This is most probably redundant!
      pc->vswizi32(mv, mv, x86::Predicate::shuf(1, 0, 1, 0));
    }
    cMaskInitXmm(mv);
  }
  else {
    // TODO: [PIPEGEN] AVX2 backend.
  }
}

void CompOpPart::cMaskInit(const x86::Mem& pMsk) noexcept {
  if (!pc->hasAVX2()) {
    x86::Vec m = cc->newXmm("msk");
    pc->vloadi32(m, pMsk);
    pc->vswizli16(m, m, x86::Predicate::shuf(0, 0, 0, 0));
    pc->vswizi32(m, m, x86::Predicate::shuf(1, 0, 1, 0));
    cMaskInitXmm(m);
  }
  else {
    // TODO: [PIPEGEN] AVX2 backend.
  }
}

void CompOpPart::cMaskFini() noexcept {
  if (!pc->hasAVX2()) {
    cMaskFiniXmm();
  }
  else {
    // TODO: [PIPEGEN] AVX2 backend.
  }
}

void CompOpPart::cMaskGenericLoop(x86::Gp& i) noexcept {
  if (isLoopOpaque() && shouldMemcpyOrMemsetOpaqueFill()) {
    cMaskMemcpyOrMemsetLoop(i);
    return;
  }

  if (!pc->hasAVX2()) {
    cMaskGenericLoopXmm(i);
  }
  else {
    // TODO: [PIPEGEN] AVX2 backend.
  }
}

void CompOpPart::cMaskGranularLoop(x86::Gp& i) noexcept {
  if (isLoopOpaque() && shouldMemcpyOrMemsetOpaqueFill()) {
    cMaskMemcpyOrMemsetLoop(i);
    return;
  }

  if (!pc->hasAVX2()) {
    cMaskGranularLoopXmm(i);
  }
  else {
    // TODO: [PIPEGEN] AVX2 backend.
  }
}

void CompOpPart::cMaskMemcpyOrMemsetLoop(x86::Gp& i) noexcept {
  BL_ASSERT(shouldMemcpyOrMemsetOpaqueFill());
  x86::Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();

  if (srcPart()->isSolid()) {
    // Optimized solid opaque fill - memset32.
    BL_ASSERT(_solidOpt.px.isValid());
    pc->xLoopMemset32(dPtr, _solidOpt.px, i, 32, pixelGranularity());
  }
  else if (srcPart()->isFetchType(BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT)) {
    // Optimized solid opaque blit - memcpy32.
    pc->xLoopMemcpy32(dPtr, srcPart()->as<FetchSimplePatternPart>()->f->srcp1, i, 16, pixelGranularity());
  }
  else {
    BL_NOT_REACHED();
  }
}

void CompOpPart::_cMaskLoopInit(uint32_t loopType) noexcept {
  // Make sure `_cMaskLoopInit()` and `_cMaskLoopFini()` are used as a pair.
  BL_ASSERT(_cMaskLoopType == kCMaskLoopTypeNone);
  BL_ASSERT(_cMaskLoopHook == nullptr);

  _cMaskLoopType = loopType;
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
// [BLPipeGen::CompOpPart - CMask (XMM)]
// ============================================================================

void CompOpPart::cMaskInitXmm(x86::Vec& m) noexcept {
  bool hasMask = m.isValid();
  bool useDa = hasDa();

  // TODO: [PIPEGEN] Fix this problem in fill: Maybe `m` is not extended, so make sure it is.
  if (hasMask) {
    pc->vswizi32(m, m, x86::Predicate::shuf(1, 0, 1, 0));
  }

  if (srcPart()->isSolid()) {
    PixelARGB& s = srcPart()->as<FetchSolidPart>()->_pixel;
    SolidPixelARGB& o = _solidOpt;

    // ------------------------------------------------------------------------
    // [CInit - Solid - SrcCopy]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kPC);
        o.px = s.pc[0];
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Im  = (1 - m) << 8 (shifted so we can use vmulhu16)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

        o.px = cc->newXmm("p.px");
        o.im = m;

        pc->vmulu16(o.px, s.uc[0], o.im);
        pc->vsrli16(o.px, o.px, 8);
        pc->vpacki16u8(o.px, o.px, o.px);

        pc->vinv256u16(o.im, o.im);
        pc->vslli16(o.im, o.im, 8);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - SrcOver]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_OVER) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = 1 - Sa
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kPC | PixelARGB::kUIA);

        o.px = s.pc[0];
        o.uy = s.uia[0];

        cc->alloc(o.px);
        cc->alloc(o.uy);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = 1 - (Sa * m)
        // Ya  = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

        o.px = cc->newXmm("p.px");
        o.uy = m;

        pc->vmulu16(o.px, s.uc[0], m);
        pc->vsrli16(o.px, o.px, 8);

        pc->vswizli16(m, o.px, x86::Predicate::shuf(3, 3, 3, 3));
        pc->vpacki16u8(o.px, o.px, o.px);

        pc->vswizi32(m, m, x86::Predicate::shuf(0, 0, 0, 0));
        pc->vinv255u16(m, m);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - SrcIn / SrcOut]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_IN || compOp() == BL_COMP_OP_SRC_OUT) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

        o.ux = s.uc[0];
        cc->alloc(o.ux);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Im  = 1   - m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

        o.ux = cc->newXmm("o.uc0");
        o.im = m;

        pc->vmulu16(o.ux, s.uc[0], m);
        pc->vsrli16(o.ux, o.ux, 8);
        pc->vinv256u16(m, m);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - SrcAtop / Xor / Darken / Lighten]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SRC_ATOP || compOp() == BL_COMP_OP_XOR || compOp() == BL_COMP_OP_DARKEN || compOp() == BL_COMP_OP_LIGHTEN) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = 1 - Sa
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC | PixelARGB::kUIA);

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
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

        o.ux = cc->newXmm("o.ux");
        o.uy = m;

        pc->vmulu16(o.ux, s.uc[0], o.uy);
        pc->vsrli16(o.ux, o.ux, 8);

        pc->vswizli16(o.uy, o.ux, x86::Predicate::shuf(3, 3, 3, 3));
        pc->vswizi32(o.uy, o.uy, x86::Predicate::shuf(0, 0, 0, 0));
        pc->vinv255u16(o.uy, o.uy);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - Dst]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_COPY) {
      BL_NOT_REACHED();
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - DstOver]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_OVER) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

        o.ux = s.uc[0];
        cc->alloc(o.ux);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

        o.ux = cc->newXmm("o.uc0");
        pc->vmulu16(o.ux, s.uc[0], m);
        pc->vsrli16(o.ux, o.ux, 8);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - DstIn]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_IN) {
      if (!hasMask) {
        // Xca = Sa
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUA);

        o.ux = s.ua[0];
        cc->alloc(o.ux);
      }
      else {
        // Xca = 1 - m.(1 - Sa)
        // Xa  = 1 - m.(1 - Sa)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUA);

        o.ux = cc->newXmm("o.ux");
        pc->vmov(o.ux, s.ua[0]);

        pc->vinv255u16(o.ux, o.ux);
        pc->vmulu16(o.ux, o.ux, m);
        pc->vsrli16(o.ux, o.ux, 8);
        pc->vinv255u16(o.ux, o.ux);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - DstOut]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_OUT) {
      if (!hasMask) {
        // Xca = 1 - Sa
        // Xa  = 1 - Sa
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUIA);

          o.ux = s.uia[0];
          cc->alloc(o.ux);
        }
        // Xca = 1 - Sa
        // Xa  = 1
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUA);

          o.ux = cc->newXmm("ux");
          pc->vmov(o.ux, s.ua[0]);
          pc->vNegRgb8W(o.ux, o.ux);
        }
      }
      else {
        // Xca = 1 - (Sa * m)
        // Xa  = 1 - (Sa * m)
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUA);

          o.ux = m;
          pc->vmulu16(o.ux, o.ux, s.ua[0]);
          pc->vsrli16(o.ux, o.ux, 8);
          pc->vinv255u16(o.ux, o.ux);
        }
        // Xca = 1 - (Sa * m)
        // Xa  = 1
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUA);

          o.ux = m;
          pc->vmulu16(o.ux, o.ux, s.ua[0]);
          pc->vsrli16(o.ux, o.ux, 8);
          pc->vinv255u16(o.ux, o.ux);
          pc->vFillAlpha255W(o.ux, o.ux);
        }
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - DstAtop]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_DST_ATOP) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = Sa
        // Ya  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC | PixelARGB::kUA);

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
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC | PixelARGB::kUA);

        o.ux = cc->newXmm("o.ux");
        o.uy = cc->newXmm("o.uy");

        pc->vmov(o.uy, s.ua[0]);
        pc->vinv255u16(o.uy, o.uy);

        pc->vmulu16(o.ux, s.uc[0], m);
        pc->vmulu16(o.uy, o.uy, m);

        pc->vsrli16(o.ux, o.ux, 8);
        pc->vsrai16(o.uy, o.uy, 8);
        pc->vinv255u16(o.uy, o.uy);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - Clear]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_CLEAR) {
      if (!hasMask) {
        // Xca = 0
        // Xa  = 0 [1 if !hasDa]
        o.px = cc->newXmm("zero");

        if (useDa)
          pc->vzeropi(o.px);
        else
          pc->vmov(o.px, pc->constAsMem(blCommonTable.i128_FF000000FF000000));
      }
      else {
        // Xca = 0
        // Xa  = 0 [1 if !hasDa]
        // Im  = 1 - m
        o.px = cc->newXmm("zero");
        o.im = m;

        if (useDa)
          pc->vzeropi(o.px);
        else
          pc->vmov(o.px, pc->constAsMem(blCommonTable.i128_FF000000FF000000));

        pc->vZeroAlphaW(o.im, o.im);
        pc->vinv256u16(o.im, o.im);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - Plus]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_PLUS) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kPC);

        o.px = s.pc[0];
        cc->alloc(o.px);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);
        o.ux = cc->newXmm("ux");

        pc->vmulu16(o.ux, s.uc[0], m);
        pc->vsrli16(o.ux, o.ux, 8);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - Minus]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_MINUS) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = 0
        // Yca = Sca
        // Ya  = Sa
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

          o.ux = cc->newXmm("ux");
          o.uy = s.uc[0];

          cc->alloc(o.uy);
          pc->vmov(o.ux, o.uy);
          pc->vZeroAlphaW(o.ux, o.ux);
        }
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kPC);
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
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

          o.ux = cc->newXmm("ux");
          o.uy = cc->newXmm("uy");
          o.m = m;
          o.im = cc->newXmm("im");

          pc->vZeroAlphaW(o.ux, s.uc[0]);
          pc->vmov(o.uy, s.uc[0]);

          pc->vinv256u16(o.im, o.m);
          pc->vZeroAlphaW(o.m, o.m);
          pc->vZeroAlphaW(o.im, o.im);
          pc->vFillAlpha256W(o.m, o.m);
        }
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

          o.ux = cc->newXmm("ux");
          o.m = m;
          o.im = cc->newXmm("im");

          pc->vZeroAlphaW(o.ux, s.uc[0]);
          pc->vinv256u16(o.im, o.m);
        }
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - Multiply]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_MULTIPLY) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = Sca + (1 - Sa)
        // Ya  = Sa  + (1 - Sa)
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC | PixelARGB::kUIA);

          o.ux = s.uc[0];
          o.uy = cc->newXmm("uy");

          cc->alloc(o.ux);
          pc->vmov(o.uy, s.uia[0]);
          pc->vaddi16(o.uy, o.uy, o.ux);
        }
        // Yca = Sca + (1 - Sa)
        // Ya  = Sa  + (1 - Sa)
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC | PixelARGB::kUIA);

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
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

        o.ux = cc->newXmm("ux");
        o.uy = cc->newXmm("uy");

        pc->vmulu16(o.ux, s.uc[0], m);
        pc->vsrli16(o.ux, o.ux, 8);

        pc->vswizli16(o.uy, o.ux, x86::Predicate::shuf(3, 3, 3, 3));
        pc->vinv255u16(o.uy, o.uy);
        pc->vswizi32(o.uy, o.uy, x86::Predicate::shuf(0, 0, 0, 0));
        pc->vaddi16(o.uy, o.uy, o.ux);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - Screen]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_SCREEN) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = 1 - Sca
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kPC);

        o.px = s.pc[0];
        o.uy = cc->newXmm("uy");

        cc->alloc(o.px);
        pc->vmovu8u16(o.uy, o.px);
        pc->vinv255u16(o.uy, o.uy);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = 1 - (Sca * m)
        // Ya  = 1 - (Sa  * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

        o.px = cc->newXmm("p.px");
        o.uy = m;

        pc->vmulu16(o.px, s.uc[0], m);
        pc->vsrli16(o.px, o.px, 8);

        pc->vinv255u16(m, o.px);
        pc->vpacki16u8(o.px, o.px, o.px);
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - LinearBurn / Difference / Exclusion]
    // ------------------------------------------------------------------------

    else if (compOp() == BL_COMP_OP_LINEAR_BURN || compOp() == BL_COMP_OP_DIFFERENCE || compOp() == BL_COMP_OP_EXCLUSION) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = Sa
        // Ya  = Sa
        srcPart()->as<FetchSolidPart>()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC | PixelARGB::kUA);

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
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

        o.ux = cc->newXmm("ux");
        o.uy = cc->newXmm("uy");

        pc->vmulu16(o.ux, s.uc[0], m);
        pc->vsrli16(o.ux, o.ux, 8);

        pc->vswizli16(o.uy, o.ux, x86::Predicate::shuf(3, 3, 3, 3));
        pc->vswizi32(o.uy, o.uy, x86::Predicate::shuf(0, 0, 0, 0));
      }
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - TypeA (Non-Opaque)]
    // ------------------------------------------------------------------------

    else if ((compOpFlags() & BL_COMP_OP_FLAG_TYPE_A)  && hasMask) {
      // Multiply the source pixel with the mask if `TypeA`.
      srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelARGB::kUC);

      PixelARGB& pre = _solidPre;
      pre.uc.init(cc->newXmm("pre.uc"));

      pc->vmulu16(pre.uc[0], s.uc[0], m);
      pc->vsrli16(pre.uc[0], pre.uc[0], 8);
    }

    // ------------------------------------------------------------------------
    // [CInit - Solid - No Optimizations]
    // ------------------------------------------------------------------------

    else {
      // No optimization. The compositor will simply use the mask provided.
      _mask->vec.m = m;
    }
  }
  else {
    _mask->vec.m = m;

    // ------------------------------------------------------------------------
    // [CInit - NonSolid - Src]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      if (hasMask) {
        x86::Xmm im = cc->newXmm("im");
        pc->vinv256u16(im, m);
        _mask->vec.im = im;
      }
    }
  }

  _cMaskLoopInit(hasMask ? kCMaskLoopTypeMask : kCMaskLoopTypeOpaque);
}

void CompOpPart::cMaskFiniXmm() noexcept {
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

void CompOpPart::cMaskGenericLoopXmm(x86::Gp& i) noexcept {
  PixelARGB dPix;
  x86::Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();

  // 1 pixel at a time.
  if (maxPixels() == 1) {
    Label L_Loop = cc->newLabel();

    prefetch1();

    cc->bind(L_Loop);
    cMaskProc32Xmm1(dPix, PixelARGB::kPC | PixelARGB::kImmutable);
    pc->xStore32_ARGB(dPtr, dPix.pc[0]);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp()), i, 1);
    dPix.reset();
    cc->jnz(L_Loop);

    return;
  }

  BL_ASSERT(minAlignment() > 1);
  int alignmentMask = minAlignment() - 1;

  // 4+ pixels at a time.
  if (maxPixels() == 4) {
    Label L_Loop1     = cc->newLabel();
    Label L_Loop4     = cc->newLabel();
    Label L_Aligned   = cc->newLabel();
    Label L_Exit      = cc->newLabel();

    cc->test(dPtr.r8(), alignmentMask);
    cc->jz(L_Aligned);

    prefetch1();

    cc->bind(L_Loop1);
    cMaskProc32Xmm1(dPix, PixelARGB::kPC | PixelARGB::kImmutable);
    pc->xStore32_ARGB(dPtr, dPix.pc[0]);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp()), i, 1);
    dPix.reset();
    cc->jz(L_Exit);

    cc->test(dPtr.r8(), alignmentMask);
    cc->jnz(L_Loop1);

    cc->bind(L_Aligned);
    cc->cmp(i, 4);
    cc->jb(L_Loop1);

    cc->sub(i, 4);
    dstPart()->as<FetchPixelPtrPart>()->setPtrAlignment(16);

    enterN();
    prefetchN();

    cc->bind(L_Loop4);
    cMaskProc32Xmm4(dPix, PixelARGB::kPC | PixelARGB::kImmutable);
    pc->vstorei128a(x86::ptr(dPtr), dPix.pc[0]);
    dPix.reset();

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

  // 8+ pixels at a time.
  if (maxPixels() == 8) {
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
    cMaskProc32Xmm1(dPix, PixelARGB::kPC | PixelARGB::kImmutable);
    pc->xStore32_ARGB(dPtr, dPix.pc[0]);
    pc->uAdvanceAndDecrement(dPtr, int(dstPart()->bpp()), i, 1);
    dPix.reset();
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
    cMaskProc32Xmm8(dPix, PixelARGB::kPC | PixelARGB::kImmutable);
    pc->vstorei128a(x86::ptr(dPtr,  0), dPix.pc[0]);
    pc->vstorei128a(x86::ptr(dPtr, 16), dPix.pc[dPix.pc.size() > 1 ? 1 : 0]);
    dPix.reset();

    cc->add(dPtr, int(dstPart()->bpp() * 8));
    cc->sub(i, 8);
    cc->jnc(L_Loop8);

    cc->bind(L_Skip8);
    cc->add(i, 4);
    cc->jnc(L_Skip4);

    cMaskProc32Xmm4(dPix, PixelARGB::kPC | PixelARGB::kImmutable);
    pc->vstorei128a(x86::ptr(dPtr), dPix.pc[0]);
    dPix.reset();

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

void CompOpPart::cMaskGranularLoopXmm(x86::Gp& i) noexcept {
  BL_ASSERT(pixelGranularity() == 4);

  PixelARGB dPix;
  x86::Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();

  if (pixelGranularity() == 4) {
    // 1 pixel at a time.
    if (maxPixels() == 1) {
      Label L_Loop = cc->newLabel();
      Label L_Step = cc->newLabel();

      cc->bind(L_Loop);
      enterPartialMode();

      cc->bind(L_Step);
      cMaskProc32Xmm1(dPix, PixelARGB::kPC | PixelARGB::kImmutable);

      pc->xStore32_ARGB(dPtr, dPix.pc[0]);
      dPix.reset();

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
      cMaskProc32Xmm4(dPix, PixelARGB::kPC | PixelARGB::kImmutable);

      pc->vstorei128u(x86::ptr(dPtr), dPix.pc[0]);
      dPix.reset();

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
      cMaskProc32Xmm8(dPix, PixelARGB::kPC | PixelARGB::kImmutable);
      pc->vstorei128u(x86::ptr(dPtr,  0), dPix.pc[0]);
      pc->vstorei128u(x86::ptr(dPtr, 16), dPix.pc[dPix.pc.size() > 1 ? 1 : 0]);
      dPix.reset();
      cc->add(dPtr, int(dstPart()->bpp() * 8));
      cc->sub(i, 8);
      cc->jnc(L_Loop);

      cc->bind(L_Skip);
      cc->add(i, 8);
      cc->jz(L_End);

      // 4 remaining pixels.
      cMaskProc32Xmm4(dPix, PixelARGB::kPC | PixelARGB::kImmutable);
      pc->vstorei128u(x86::ptr(dPtr), dPix.pc[0]);
      cc->add(dPtr, int(dstPart()->bpp() * 4));

      cc->bind(L_End);
      return;
    }
  }

  BL_NOT_REACHED();
}

void CompOpPart::cMaskProc32Xmm1(PixelARGB& out, uint32_t flags) noexcept {
  cMaskProc32XmmV(out, flags, 1);
}

void CompOpPart::cMaskProc32Xmm4(PixelARGB& out, uint32_t flags) noexcept {
  cMaskProc32XmmV(out, flags, 4);
}

void CompOpPart::cMaskProc32Xmm8(PixelARGB& out, uint32_t flags) noexcept {
  cMaskProc32XmmV(out, flags, 8);
}

void CompOpPart::cMaskProc32XmmV(PixelARGB& out, uint32_t flags, uint32_t n) noexcept {
  bool hasMask = isLoopCMask();

  uint32_t kFullN = (n + 1) / 2;
  uint32_t kUseHi = n > 1;

  if (srcPart()->isSolid()) {
    PixelARGB d;
    SolidPixelARGB& o = _solidOpt;
    VecArray xv, yv, zv;

    pc->newXmmArray(xv, kFullN, "x");
    pc->newXmmArray(yv, kFullN, "y");
    pc->newXmmArray(zv, kFullN, "z");

    bool useDa = hasDa();
    bool useSa = hasSa() || hasMask;

    // ------------------------------------------------------------------------
    // [CProc - Solid - Src]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_COPY) {
      // Dca' = Xca
      // Da'  = Xa
      if (!hasMask) {
        out.pc.init(o.px);
        out.immutable = true;
      }
      // Dca' = Xca + Dca.(1 - m)
      // Da'  = Xa  + Da .(1 - m)
      else {
        dstFetch32(d, PixelARGB::kUC, n);
        VecArray& dv = d.uc;
        pc->vmulhu16(dv, dv, o.im);

        VecArray dh = dv.even();
        pc->vpacki16u8(dh, dh, dv.odd());
        pc->vaddi32(dh, dh, o.px);
        out.pc.init(dh);
      }

      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - SrcOver / Screen]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_OVER || compOp() == BL_COMP_OP_SCREEN) {
      // Dca' = Xca + Dca.Yca
      // Da'  = Xa  + Da .Ya
      dstFetch32(d, PixelARGB::kUC, n);
      VecArray& dv = d.uc;

      pc->vmulu16(dv, dv, o.uy);
      pc->vdiv255u16(dv);

      VecArray dh = dv.even();
      pc->vpacki16u8(dh, dh, dv.odd());
      pc->vaddi32(dh, dh, o.px);

      out.pc.init(dh);
      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - SrcIn]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_IN) {
      // Dca' = Xca.Da
      // Da'  = Xa .Da
      if (!hasMask) {
        dstFetch32(d, PixelARGB::kUA, n);
        VecArray& dv = d.ua;

        pc->vmulu16(dv, dv, o.ux);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Xca.Da + Dca.(1 - m)
      // Da'  = Xa .Da + Da .(1 - m)
      else {
        dstFetch32(d, PixelARGB::kUC | PixelARGB::kUA, n);
        VecArray& dv = d.uc;
        VecArray& xv = d.ua;

        pc->vmulu16(dv, dv, o.im);
        pc->vmulu16(xv, xv, o.ux);
        pc->vsrli16(dv, dv, 8);
        pc->vdiv255u16(xv);
        pc->vaddi16(dv, dv, xv);
        out.uc.init(dv);
      }

      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - SrcOut]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_OUT) {
      // Dca' = Xca.(1 - Da)
      // Da'  = Xa .(1 - Da)
      if (!hasMask) {
        dstFetch32(d, PixelARGB::kUIA, n);
        VecArray& dv = d.uia;

        pc->vmulu16(dv, dv, o.ux);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Xca.(1 - Da) + Dca.(1 - m)
      // Da'  = Xa .(1 - Da) + Da .(1 - m)
      else {
        dstFetch32(d, PixelARGB::kUC, n);
        VecArray& dv = d.uc;

        pc->vExpandAlpha16(xv, dv, kUseHi);
        pc->vinv255u16(dv, dv);
        pc->vmulu16(xv, xv, o.ux);
        pc->vmulu16(dv, dv, o.im);
        pc->vdiv255u16(xv);
        pc->vsrli16(dv, dv, 8);
        pc->vaddi16(dv, dv, xv);
        out.uc.init(dv);
      }

      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - SrcAtop]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_SRC_ATOP) {
      // Dca' = Xca.Da + Dca.Yca
      // Da'  = Xa .Da + Da .Ya
      dstFetch32(d, PixelARGB::kUC, n);
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(dv, dv, o.uy);
      pc->vmulu16(xv, xv, o.ux);

      pc->vaddi16(dv, dv, xv);
      pc->vdiv255u16(dv);

      out.uc.init(dv);
      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - Dst]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DST_COPY) {
      // Dca' = Dca
      // Da'  = Da
      BL_NOT_REACHED();
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - DstOver]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DST_OVER) {
      // Dca' = Xca.(1 - Da) + Dca
      // Da'  = Xa .(1 - Da) + Da
      dstFetch32(d, PixelARGB::kPC | PixelARGB::kUIA, n);
      VecArray& dv = d.uia;

      pc->vmulu16(dv, dv, o.ux);
      pc->vdiv255u16(dv);

      VecArray dh = dv.even();
      pc->vpacki16u8(dh, dh, dv.odd());
      pc->vaddi32(dh, dh, d.pc);

      out.pc.init(dh);
      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - DstIn / DstOut]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DST_IN || compOp() == BL_COMP_OP_DST_OUT) {
      // Dca' = Xca.Dca
      // Da'  = Xa .Da
      dstFetch32(d, PixelARGB::kUC, n);
      VecArray& dv = d.uc;

      pc->vmulu16(dv, dv, o.ux);
      pc->vdiv255u16(dv);

      out.uc.init(dv);
      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - DstAtop / Xor / Multiply]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DST_ATOP || compOp() == BL_COMP_OP_XOR || compOp() == BL_COMP_OP_MULTIPLY) {
      // Dca' = Xca.(1 - Da) + Dca.Yca
      // Da'  = Xa .(1 - Da) + Da .Ya
      if (useDa) {
        dstFetch32(d, PixelARGB::kUC, n);
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
        dstFetch32(d, PixelARGB::kUC, n);
        VecArray& dv = d.uc;

        pc->vmulu16(dv, dv, o.uy);
        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }

      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - Clear]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_CLEAR) {
      // Dca' = 0
      // Da'  = 0 [1 if !hasDa]
      if (!hasMask) {
        out.pc.init(o.px);
        out.immutable = true;
      }
      // Dca' = Dca.(1 - m)
      // Da'  = Da .(1 - m) [<unchanged> if !hasDa]
      else {
        dstFetch32(d, PixelARGB::kUC, n);
        VecArray& dv = d.uc;

        pc->vmulu16(dv, dv, o.im);
        pc->vsrli16(dv, dv, 8);
        out.uc.init(dv);
      }

      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - Minus]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_MINUS) {
      if (!hasMask) {
        // Dca' = Clamp(Dca - Xca) + Yca.(1 - Da)
        // Da'  = Da + Ya.(1 - Da)
        if (useDa) {
          dstFetch32(d, PixelARGB::kUC, n);
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
          dstFetch32(d, PixelARGB::kPC, n);
          VecArray& dh = d.pc;

          pc->vsubsu8(dh, dh, o.px);
          out.pc.init(dh);
        }
      }
      else {
        // Dca' = (Clamp(Dca - Xca) + Yca.(1 - Da)).m + Dca.(1 - m)
        // Da'  = Da + Ya.(1 - Da)
        if (useDa) {
          dstFetch32(d, PixelARGB::kUC, n);
          VecArray& dv = d.uc;

          pc->vExpandAlpha16(xv, dv, kUseHi);
          pc->vinv255u16(xv, xv);
          pc->vmulu16(yv, dv, o.im);
          pc->vsubsu16(dv, dv, o.ux);
          pc->vmulu16(xv, xv, o.uy);
          pc->vdiv255u16(xv);
          pc->vaddi16(dv, dv, xv);
          pc->vmulu16(dv, dv, o.m);

          pc->vaddi16(dv, dv, yv);
          pc->vsrli16(dv, dv, 8);
          out.uc.init(dv);
        }
        // Dca' = Clamp(Dca - Xca).m + Dca.(1 - m)
        // Da'  = <unchanged>
        else {
          dstFetch32(d, PixelARGB::kUC, n);
          VecArray& dv = d.uc;

          pc->vmulu16(yv, dv, o.im);
          pc->vsubsu16(dv, dv, o.ux);
          pc->vmulu16(dv, dv, o.m);

          pc->vaddi16(dv, dv, yv);
          pc->vsrli16(dv, dv, 8);
          out.uc.init(dv);
        }
      }

      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - Darken / Lighten]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DARKEN || compOp() == BL_COMP_OP_LIGHTEN) {
      // Dca' = minmax(Dca + Xca.(1 - Da), Xca + Dca.Yca)
      // Da'  = Xa + Da.Ya
      dstFetch32(d, PixelARGB::kUC, n);
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
      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - LinearBurn]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_LINEAR_BURN) {
      // Dca' = Dca + Xca - Yca.Da
      // Da'  = Da  + Xa  - Ya .Da
      dstFetch32(d, PixelARGB::kUC, n);
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(xv, xv, o.uy);
      pc->vaddi16(dv, dv, o.ux);
      pc->vdiv255u16(xv);
      pc->vsubsu16(dv, dv, xv);

      out.uc.init(dv);
      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - Difference]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_DIFFERENCE) {
      // Dca' = Dca + Sca - 2.min(Sca.Da, Dca.Sa)
      // Da'  = Da  + Sa  -   min(Sa .Da, Da .Sa)
      dstFetch32(d, PixelARGB::kUC, n);
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
      pc->xSatisfyARGB32(out, flags, n);
      return;
    }

    // ------------------------------------------------------------------------
    // [CProc - Solid - Exclusion]
    // ------------------------------------------------------------------------

    if (compOp() == BL_COMP_OP_EXCLUSION) {
      // Dca' = Dca + Xca - 2.Xca.Dca
      // Da'  = Da + Xa - Xa.Da
      dstFetch32(d, PixelARGB::kUC, n);
      VecArray& dv = d.uc;

      pc->vmulu16(xv, dv, o.ux);
      pc->vaddi16(dv, dv, o.ux);
      pc->vdiv255u16(xv);
      pc->vsubi16(dv, dv, xv);
      pc->vZeroAlphaW(xv, xv);
      pc->vsubi16(dv, dv, xv);

      out.uc.init(dv);
      pc->xSatisfyARGB32(out, flags, n);
      return;
    }
  }

  VecArray mv;
  if (_mask->vec.m.isValid())
    mv.init(_mask->vec.m);

  vMaskProc32XmmV(out, flags, mv, n, true);
}

// ============================================================================
// [BLPipeGen::CompOpPart - VMask (Any)]
// ============================================================================

void CompOpPart::vMaskProc(PixelARGB& out, uint32_t flags, x86::Gp& m) noexcept {
  if (!pc->hasAVX2()) {
    x86::Vec mv = cc->newXmm("c.mv");
    pc->vmovsi32(mv, m);
    pc->vswizli16(mv, mv, x86::Predicate::shuf(0, 0, 0, 0));

    VecArray mv_(mv);
    vMaskProc32Xmm1(out, flags, mv_, false);
  }
  else {
    // TODO: [PIPEGEN] AVX2 backend.
  }
}

// ============================================================================
// [BLPipeGen::CompOpPart - VMask (XMM)]
// ============================================================================

void CompOpPart::vMaskProc32Xmm1(PixelARGB& out, uint32_t flags, VecArray& mv, bool mImmutable) noexcept {
  vMaskProc32XmmV(out, flags, mv, 1, mImmutable);
}

void CompOpPart::vMaskProc32Xmm4(PixelARGB& out, uint32_t flags, VecArray& mv, bool mImmutable) noexcept {
  vMaskProc32XmmV(out, flags, mv, 4, mImmutable);
}

void CompOpPart::vMaskProc32XmmV(PixelARGB& out, uint32_t flags, VecArray& mv, uint32_t n, bool mImmutable) noexcept {
  bool hasMask = !mv.empty();
  bool isConst = hasMask && mv.isScalar();

  bool useDa = hasDa();
  bool useSa = hasSa() || hasMask || isLoopCMask();

  uint32_t kFullN = (n + 1) / 2;
  uint32_t kSplit = (kFullN == 1) ? 1 : 2;
  uint32_t kUseHi = n == 1 ? 0 : 1;

  VecArray xv, yv, zv;
  pc->newXmmArray(xv, kFullN, "x");
  pc->newXmmArray(yv, kFullN, "y");
  pc->newXmmArray(zv, kFullN, "z");

  PixelARGB d, s;

  // --------------------------------------------------------------------------
  // [VProc32 - Src]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_COPY) {
    if (!hasMask) {
      // Dca' = Sca
      // Da'  = Sa
      srcFetch32(out, flags, n);
    }
    else {
      // Dca' = Sca.m + Dca.(1 - m)
      // Da'  = Sa .m + Da .(1 - m)
      srcFetch32(s, PixelARGB::kUC, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;
      VecArray mi;

      pc->vmulu16(sv, sv, mv);
      vMaskProc32InvertMask(mi, mv);

      pc->vmulu16(dv, dv, mi);
      pc->vaddi16(dv, dv, sv);
      vMaskProc32InvertDone(mi, mImmutable);

      pc->vsrli16(dv, dv, 8);
      out.uc.init(dv);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - SrcOver]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_OVER) {
    if (!hasMask) {
      // Dca' = Sca + Dca.(1 - Sa)
      // Da'  = Sa  + Da .(1 - Sa)
      srcFetch32(s, PixelARGB::kPC | PixelARGB::kUIA | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kUC, n);

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
      srcFetch32(s, PixelARGB::kUC, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);

      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->vinv255u16(xv, xv);
      pc->vmulu16(dv, dv, xv);
      pc->vdiv255u16(dv);

      pc->vaddi16(dv, dv, sv);
      out.uc.init(dv);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - SrcIn]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_IN) {
    // Dca' = Sca.Da
    // Da'  = Sa .Da
    if (!hasMask) {
      srcFetch32(s, PixelARGB::kUC | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kUA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.ua;

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }
    // Dca' = Sca.m.Da + Dca.(1 - m)
    // Da'  = Sa .m.Da + Da .(1 - m)
    else {
      srcFetch32(s, PixelARGB::kUC | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(xv, xv, sv);
      pc->vdiv255u16(xv);
      pc->vmulu16(xv, xv, mv);
      vMaskProc32InvertMask(mv, mv);

      pc->vmulu16(dv, dv, mv);
      vMaskProc32InvertDone(mv, mImmutable);

      pc->vaddi16(dv, dv, xv);
      pc->vsrli16(dv, dv, 8);
      out.uc.init(dv);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - SrcOut]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_OUT) {
    // Dca' = Sca.(1 - Da)
    // Da'  = Sa .(1 - Da)
    if (!hasMask) {
      srcFetch32(s, PixelARGB::kUC | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kUIA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uia;

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }
    // Dca' = Sca.m.(1 - Da) + Dca.(1 - m)
    // Da'  = Sa .m.(1 - Da) + Da .(1 - m)
    else {
      srcFetch32(s, PixelARGB::kUC | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vinv255u16(xv, xv);

      pc->vmulu16(xv, xv, sv);
      pc->vdiv255u16(xv);
      pc->vmulu16(xv, xv, mv);
      vMaskProc32InvertMask(mv, mv);

      pc->vmulu16(dv, dv, mv);
      vMaskProc32InvertDone(mv, mImmutable);

      pc->vaddi16(dv, dv, xv);
      pc->vsrli16(dv, dv, 8);
      out.uc.init(dv);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - SrcAtop]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SRC_ATOP) {
    // Dca' = Sca.Da + Dca.(1 - Sa)
    // Da'  = Sa .Da + Da .(1 - Sa) = Da
    if (!hasMask) {
      srcFetch32(s, PixelARGB::kUC | PixelARGB::kUIA | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kUC, n);

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
    // Dca' = Sca.Da.m + Dca.(1 - Sa.m)
    // Da'  = Sa .Da.m + Da .(1 - Sa.m) = Da
    else {
      srcFetch32(s, PixelARGB::kUC, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);

      pc->vExpandAlpha16(xv, sv, kUseHi);
      pc->vinv255u16(xv, xv);
      pc->vExpandAlpha16(yv, dv, kUseHi);
      pc->vmulu16(dv, dv, xv);
      pc->vmulu16(yv, yv, sv);
      pc->vaddi16(dv, dv, yv);
      pc->vdiv255u16(dv);

      out.uc.init(dv);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Dst]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_COPY) {
    // Dca' = Dca
    // Da'  = Da
    BL_NOT_REACHED();
  }

  // --------------------------------------------------------------------------
  // [VProc32 - DstOver]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_OVER) {
    // Dca' = Dca + Sca.(1 - Da)
    // Da'  = Da  + Sa .(1 - Da)
    if (!hasMask) {
      srcFetch32(s, PixelARGB::kUC | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kPC | PixelARGB::kUIA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uia;

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);

      VecArray dh = dv.even();
      pc->vpacki16u8(dh, dh, dv.odd());
      pc->vaddi32(dh, dh, d.pc);

      out.pc.init(dh);
    }
    // Dca' = Dca + Sca.m.(1 - Da)
    // Da'  = Da  + Sa .m.(1 - Da)
    else {
      srcFetch32(s, PixelARGB::kUC, n);
      dstFetch32(d, PixelARGB::kPC | PixelARGB::kUIA, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uia;

      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);

      VecArray dh = dv.even();
      pc->vpacki16u8(dh, dh, dv.odd());
      pc->vaddi32(dh, dh, d.pc);

      out.pc.init(dh);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - DstIn]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_IN) {
    // Dca' = Dca.Sa
    // Da'  = Da .Sa
    if (!hasMask) {
      srcFetch32(s, PixelARGB::kUA | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.ua;
      VecArray& dv = d.uc;

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }
    // Dca' = Dca.(1 - m.(1 - Sa))
    // Da'  = Da .(1 - m.(1 - Sa))
    else {
      srcFetch32(s, PixelARGB::kUIA, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.uia;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);
      pc->vinv255u16(sv, sv);

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - DstOut]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_OUT) {
    // Dca' = Dca.(1 - Sa)
    // Da'  = Da .(1 - Sa)
    if (!hasMask) {
      srcFetch32(s, PixelARGB::kUIA | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.uia;
      VecArray& dv = d.uc;

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }
    // Dca' = Dca.(1 - Sa.m)
    // Da'  = Da .(1 - Sa.m)
    else {
      srcFetch32(s, PixelARGB::kUA, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.ua;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);
      pc->vinv255u16(sv, sv);

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyARGB32(out, flags, n);
    if (!hasDa()) pc->vFillAlpha(out);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - DstAtop]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DST_ATOP) {
    // Dca' = Dca.Sa + Sca.(1 - Da)
    // Da'  = Da .Sa + Sa .(1 - Da)
    if (!hasMask) {
      srcFetch32(s, PixelARGB::kUC | PixelARGB::kUA | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kUC, n);

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

    // Dca' = Dca.(1 - m.(1 - Sa)) + Sca.m.(1 - Da)
    // Da'  = Da .(1 - m.(1 - Sa)) + Sa .m.(1 - Da)
    else {
      srcFetch32(s, PixelARGB::kUC | PixelARGB::kUIA, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.uc;
      VecArray& uv = s.uia;
      VecArray& dv = d.uc;

      pc->vExpandAlpha16(xv, dv, kUseHi);
      pc->vmulu16(sv, sv, mv);
      pc->vmulu16(uv, uv, mv);

      pc->vsrli16(sv, sv, 8);
      pc->vsrli16(uv, uv, 8);
      pc->vinv255u16(xv, xv);
      pc->vinv255u16(uv, uv);
      pc->vmulu16(xv, xv, sv);
      pc->vmulu16(dv, dv, uv);

      pc->vaddi16(dv, dv, xv);
      pc->vdiv255u16(dv);
      out.uc.init(dv);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Xor]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_XOR) {
    // Dca' = Dca.(1 - Sa) + Sca.(1 - Da)
    // Da'  = Da .(1 - Sa) + Sa .(1 - Da)
    if (!hasMask) {
      srcFetch32(s, PixelARGB::kUC | PixelARGB::kUIA | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kUC, n);

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
    // Dca' = Dca.(1 - Sa.m) + Sca.m.(1 - Da)
    // Da'  = Da .(1 - Sa.m) + Sa .m.(1 - Da)
    else {
      srcFetch32(s, PixelARGB::kUC, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);

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

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Clear]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_CLEAR) {
    if (!hasMask) {
      // Dca' = 0
      // Da'  = 0
      srcFetch32(out, flags, n);
    }
    else {
      // Dca' = Dca.(1 - m)
      // Da'  = Da .(1 - m)
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& dv = d.uc;
      VecArray mi;

      vMaskProc32InvertMask(mi, mv);
      pc->vmulu16(dv, dv, mi);

      vMaskProc32InvertDone(mi, mImmutable);
      pc->vsrli16(dv, dv, 8);
      out.uc.init(dv);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Plus]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_PLUS) {
    // Dca' = Clamp(Dca + Sca)
    // Da'  = Clamp(Da  + Sa )
    if (!hasMask) {
      srcFetch32(s, PixelARGB::kPC | PixelARGB::kImmutable, n);
      dstFetch32(d, PixelARGB::kPC, n);

      VecArray& sh = s.pc;
      VecArray& dh = d.pc;

      pc->vaddsu8(dh, dh, sh);
      out.pc.init(dh);
    }
    // Dca' = Clamp(Dca + Sca.m)
    // Da'  = Clamp(Da  + Sa .m)
    else {
      srcFetch32(s, PixelARGB::kUC, n);
      dstFetch32(d, PixelARGB::kPC, n);

      VecArray& sv = s.uc;
      VecArray& dh = d.pc;

      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);

      VecArray sh = sv.even();
      pc->vpacki16u8(sh, sh, sv.odd());
      pc->vaddsu8(dh, dh, sh);

      out.pc.init(dh);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Minus]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_MINUS) {
    if (!hasMask) {
      // Dca' = Clamp(Dca - Sca) + Sca.(1 - Da)
      // Da'  = Da + Sa.(1 - Da)
      if (hasDa()) {
        srcFetch32(s, PixelARGB::kUC, n);
        dstFetch32(d, PixelARGB::kUC, n);

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
        srcFetch32(s, PixelARGB::kPC, n);
        dstFetch32(d, PixelARGB::kPC, n);

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
      if (hasDa()) {
        srcFetch32(s, PixelARGB::kUC, n);
        dstFetch32(d, PixelARGB::kUC, n);

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
        pc->vmulu16(dv, dv, mv);

        pc->vZeroAlphaW(mv, mv);
        pc->vinv256u16(mv, mv);

        pc->vmulu16(yv, yv, mv);

        if (mImmutable) {
          pc->vinv256u16(mv[0], mv[0]);
          pc->vswizi32(mv[0], mv[0], x86::Predicate::shuf(2, 2, 0, 0));
        }

        pc->vaddi16(dv, dv, yv);
        pc->vsrli16(dv, dv, 8);
        out.uc.init(dv);
      }
      // Dca' = Clamp(Dca - Sca).m + Dca.(1 - m)
      // Da'  = <unchanged>
      else {
        srcFetch32(s, PixelARGB::kUC, n);
        dstFetch32(d, PixelARGB::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vinv256u16(xv, mv);
        pc->vZeroAlphaW(sv, sv);

        pc->vmulu16(xv, xv, dv);
        pc->vsubsu16(dv, dv, sv);
        pc->vmulu16(dv, dv, mv);

        pc->vaddi16(dv, dv, xv);
        pc->vsrli16(dv, dv, 8);
        out.uc.init(dv);
      }
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Multiply]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_MULTIPLY) {
    if (!hasMask) {
      // Dca' = Dca.(Sca + 1 - Sa) + Sca.(1 - Da)
      // Da'  = Da .(Sa  + 1 - Sa) + Sa .(1 - Da)
      if (useDa && useSa) {
        srcFetch32(s, PixelARGB::kUC | PixelARGB::kImmutable, n);
        dstFetch32(d, PixelARGB::kUC, n);

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
      else if (hasDa()) {
        srcFetch32(s, PixelARGB::kUC | PixelARGB::kImmutable, n);
        dstFetch32(d, PixelARGB::kUC, n);

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
        srcFetch32(s, PixelARGB::kUC | PixelARGB::kImmutable, n);
        dstFetch32(d, PixelARGB::kUC, n);

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
        srcFetch32(s, PixelARGB::kUC | PixelARGB::kImmutable, n);
        dstFetch32(d, PixelARGB::kUC, n);

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
      if (hasDa()) {
        srcFetch32(s, PixelARGB::kUC, n);
        dstFetch32(d, PixelARGB::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vmulu16(sv, sv, mv);
        pc->vsrli16(sv, sv, 8);

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
        srcFetch32(s, PixelARGB::kUC, n);
        dstFetch32(d, PixelARGB::kUC, n);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->vmulu16(sv, sv, mv);
        pc->vsrli16(sv, sv, 8);

        pc->vExpandAlpha16(xv, sv, kUseHi);
        pc->vinv255u16(xv, xv);
        pc->vaddi16(xv, xv, sv);
        pc->vmulu16(dv, dv, xv);

        pc->vdiv255u16(dv);
        out.uc.init(dv);
      }
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Overlay]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_OVERLAY) {
    srcFetch32(s, PixelARGB::kUC, n);
    dstFetch32(d, PixelARGB::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);
    }

    if (useSa) {
      // if (2.Dca < Da)
      //   Dca' = Dca + Sca - (Dca.Sa + Sca.Da - 2.Sca.Dca)
      //   Da'  = Da  + Sa  - Sa.Da
      // else
      //   Dca' = Dca + Sca + (Dca.Sa + Sca.Da - 2.Sca.Dca) - Sa.Da
      //   Da'  = Da  + Sa  - Sa.Da

      // SPLIT.
      for (unsigned int i = 0; i < kSplit; i++) {
        VecArray sh = sv.even_odd(i);
        VecArray dh = dv.even_odd(i);
        VecArray xh = xv.even_odd(i);
        VecArray yh = yv.even_odd(i);
        VecArray zh = zv.even_odd(i);

        if (useDa)
          pc->vExpandAlpha16(xh, dh, kUseHi);
        else
          pc->vmov(xh, pc->constAsMem(blCommonTable.i128_00FF00FF00FF00FF));

        pc->vExpandAlpha16(yh, sh, kUseHi);

        pc->vmulu16(xh, xh, sh);                                 // Sca.Da
        pc->vmulu16(yh, yh, dh);                                 // Dca.Sa
        pc->vmulu16(zh, dh, sh);                                 // Dca.Sca

        pc->vaddi16(sh, sh, dh);                                 // Dca + Sca
        pc->vsubi16(xh, xh, zh);                                 // Sca.Da - Dca.Sca
        pc->vaddi16(xh, xh, yh);                                 // Dca.Sa + Sca.Da - Dca.Sca
        pc->vsubi16(xh, xh, zh);                                 // Dca.Sa + Sca.Da - 2.Dca.Sca

        pc->vExpandAlpha16(zh, dh, kUseHi);                      // Da
        pc->vslli16(dh, dh, 1);                                  // 2.Dca

        pc->vExpandAlpha16(yh, yh, kUseHi);                      // Sa.Da
        pc->vcmpgti16(zh, zh, dh);
        pc->vdiv255u16_2x(xh, yh);

        pc->vxor(xh, xh, zh);
        pc->vsubi16(xh, xh, zh);
        pc->vZeroAlphaW(zh, zh);

        pc->vandnot_a(zh, zh, yh);
        pc->vaddi16(sh, sh, xh);
        pc->vsubi16(sh, sh, zh);
      }

      out.uc.init(sv);
      pc->xSatisfyARGB32(out, flags, n);
      return;
    }
    else if (useDa) {
      // if (2.Dca - Da < 0)
      //   Dca' = Sc.(2.Dca - Da + 1)
      //   Da'  = 1
      // else
      //   Dca' = 2.Dca - Da - Sc.(1 - (2.Dca - Da))
      //   Da'  = 1
      pc->vExpandAlpha16(xv, dv, kUseHi);                        // Da
      pc->vslli16(dv, dv, 1);                                    // 2.Dca
      pc->vsubi16(dv, dv, xv);                                   // 2.Dca - Da
      pc->vzeropi(xv);                                           // 0
      pc->vsubi16(xv, xv, dv);                                   // Da - 2.Dca
      pc->vsrai16(xv, xv, 15);                                   // 2.Dca - Da >= 0 ?

      pc->vmov(yv, xv);                                          // 2.Dca - Da >= 0 ?
      pc->vand(xv, xv, dv);                                      // 2.Dca - Da >= 0 ? 2.Dca - Da : 0
      pc->vxor(xv, xv, yv);
      pc->vsubi16(xv, xv, yv);
      pc->vsubi16(dv, dv, yv);                                   // 2.Dca - Da >= 0 ?   - 2.Dca + Da :     2.Dca - Da
      pc->vaddi16(dv, dv, pc->constAsMem(blCommonTable.i128_00FF00FF00FF00FF)); // 2.Dca - Da >= 0 ? 1 - 2.Dca + Da : 1 + 2.Dca - Da

      pc->vmulu16(dv, dv, sv);
      pc->vdiv255u16(dv);
      pc->vxor(dv, dv, yv);
      pc->vaddi16(dv, dv, xv);
      out.uc.init(dv);
    }
    else {
      // if (2.Dc - 1 < 0)
      //   Dc'  = 2.Dc.Sc
      // else
      //   Dc'  = 2.Dc + 2.Sc - 1 - 2.Dc.Sc
      pc->vslli16(dv, dv, 1);                                    // 2.Dc
      pc->vmov(xv, pc->constAsMem(blCommonTable.i128_00FF00FF00FF00FF));     // 1
      pc->vsubi16(xv, xv, dv);                                   // 1 - 2.Dc
      pc->vmulu16(dv, dv, sv);                                   // Dc.Sc
      pc->vaddi16(sv, sv, sv);                                   // 2.Sc
      pc->vdiv255u16(dv);

      pc->vsubi16(sv, sv, xv);                                   // 2.Dc + 2.Sc - 1
      pc->vslli16(dv, dv, 1);                                    // 2.Dc.Sc
      pc->vsrai16(xv, xv, 15);                                   // 2.Dc - 1 >= 0 ?
      pc->vsubi16(sv, sv, dv);                                   // 2.Dc + 2.Sc - 1 - 2.Dc.Sc

      pc->vand(sv, sv, xv);
      pc->vandnot_a(xv, xv, dv);
      pc->vor(sv, sv, xv);
      out.uc.init(sv);
    }

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Screen]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_SCREEN) {
    // Dca' = Sca + Dca.(1 - Sca)
    // Da'  = Sa  + Da .(1 - Sa)
    srcFetch32(s, PixelARGB::kUC | (hasMask ? 0 : int(PixelARGB::kImmutable)), n);
    dstFetch32(d, PixelARGB::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);
    }

    pc->vinv255u16(xv, sv);
    pc->vmulu16(dv, dv, xv);
    pc->vdiv255u16(dv);
    pc->vaddi16(dv, dv, sv);

    out.uc.init(dv);
    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Darken / Lighten]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DARKEN || compOp() == BL_COMP_OP_LIGHTEN) {
    // Dca' = minmax(Dca + Sca.(1 - Da), Sca + Dca.(1 - Sa))
    // Da'  = Sa + Da.(1 - Sa)
    srcFetch32(s, PixelARGB::kUC, n);
    dstFetch32(d, PixelARGB::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);
    }

    // SPLIT.
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

      if (compOp() == BL_COMP_OP_DARKEN)
        pc->vminu8(dh, dh, sh);
      else
        pc->vmaxu8(dh, dh, sh);
    }

    out.uc.init(dv);
    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - ColorDodge (SCALAR)]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_COLOR_DODGE && n == 1) {
    // Dca' = min(Dca.Sa.Sa / max(Sa - Sca, 0.001), Sa.Da) + Sca.(1 - Da) + Dca.(1 - Sa);
    // Da'  = min(Da .Sa.Sa / max(Sa - Sa , 0.001), Sa.Da) + Sa .(1 - Da) + Da .(1 - Sa);
    srcFetch32(s, PixelARGB::kUC, n);
    dstFetch32(d, PixelARGB::kPC, n);

    x86::Vec& s0 = s.uc[0];
    x86::Vec& d0 = d.pc[0];
    x86::Vec& x0 = xv[0];
    x86::Vec& y0 = yv[0];
    x86::Vec& z0 = zv[0];

    if (hasMask) {
      pc->vmulu16(s0, s0, mv[0]);
      pc->vsrli16(s0, s0, 8);
    }

    pc->vmovu8u32(d0, d0);
    pc->vmovu16u32(s0, s0);

    pc->vcvti32ps(y0, s0);
    pc->vcvti32ps(z0, d0);
    pc->vpacki32i16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);
    pc->vxorps(y0, y0, pc->constAsMem(blCommonTable.f128_sgn));
    pc->vmulps(z0, z0, x0);
    pc->vandps(y0, y0, pc->constAsMem(blCommonTable.i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0));
    pc->vaddps(y0, y0, x0);

    pc->vmaxps(y0, y0, pc->constAsMem(blCommonTable.f128_1e_m3));
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

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - ColorBurn (SCALAR)]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_COLOR_BURN && n == 1) {
    // Dca' = Sa.Da - min(Sa.Da, (Da - Dca).Sa.Sa / max(Sca, 0.001)) + Sca.(1 - Da) + Dca.(1 - Sa)
    // Da'  = Sa.Da - min(Sa.Da, (Da - Da ).Sa.Sa / max(Sa , 0.001)) + Sa .(1 - Da) + Da .(1 - Sa)
    srcFetch32(s, PixelARGB::kUC, n);
    dstFetch32(d, PixelARGB::kPC, n);

    x86::Vec& s0 = s.uc[0];
    x86::Vec& d0 = d.pc[0];
    x86::Vec& x0 = xv[0];
    x86::Vec& y0 = yv[0];
    x86::Vec& z0 = zv[0];

    if (hasMask) {
      pc->vmulu16(s0, s0, mv[0]);
      pc->vsrli16(s0, s0, 8);
    }

    pc->vmovu8u32(d0, d0);
    pc->vmovu16u32(s0, s0);

    pc->vcvti32ps(y0, s0);
    pc->vcvti32ps(z0, d0);
    pc->vpacki32i16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);
    pc->vmaxps(y0, y0, pc->constAsMem(blCommonTable.f128_1e_m3));
    pc->vmulps(z0, z0, x0);                                      // Dca.Sa

    pc->vExpandAlphaPS(x0, z0);                                  // Sa.Da
    pc->vxorps(z0, z0, pc->constAsMem(blCommonTable.f128_sgn));

    pc->vandps(z0, z0, pc->constAsMem(blCommonTable.i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0));
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
    pc->vandps(z0, z0, pc->constAsMem(blCommonTable.i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0));
    pc->vsubps(x0, x0, z0);

    pc->vcvttpsi32(x0, x0);
    pc->xPackU32ToU16Lo(x0, x0);
    pc->vaddi16(d0, d0, x0);

    pc->vdiv255u16(d0);
    out.uc.init(d0);

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - LinearBurn]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_LINEAR_BURN) {
    // Dca' = Dca + Sca - Sa.Da
    // Da'  = Da  + Sa  - Sa.Da
    srcFetch32(s, PixelARGB::kUC | (hasMask ? 0 : int(PixelARGB::kImmutable)), n);
    dstFetch32(d, PixelARGB::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);
    }

    if (useDa && useSa) {
      // SPLIT.
      for (unsigned int i = 0; i < kSplit; i++) {
        VecArray sh = sv.even_odd(i);
        VecArray dh = dv.even_odd(i);
        VecArray xh = xv.even_odd(i);
        VecArray yh = yv.even_odd(i);

        pc->vExpandAlpha16(xh, sh, kUseHi);
        pc->vExpandAlpha16(yh, dh, kUseHi);
        pc->vmulu16(xh, xh, yh);
        pc->vdiv255u16(xh);
        pc->vaddi16(dh, dh, sh);
        pc->vsubsu16(dh, dh, xh);
      }
    }
    else if (useDa || useSa) {
      pc->vExpandAlpha16(xv, useDa ? dv : sv, kUseHi);
      pc->vaddi16(dv, dv, sv);
      pc->vsubsu16(dv, dv, xv);
    }
    else {
      pc->vaddi16(dv, dv, sv);
      pc->vsubsu16(dv, dv, pc->constAsMem(blCommonTable.i128_000000FF00FF00FF));
    }

    out.uc.init(dv);
    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - LinearLight]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_LINEAR_LIGHT && n == 1) {
    // Dca' = min(max((Dca.Sa + 2.Sca.Da - Sa.Da), 0), Sa.Da) + Sca.(1 - Da) + Dca.(1 - Sa)
    // Da'  = min(max((Da .Sa + 2.Sa .Da - Sa.Da), 0), Sa.Da) + Sa .(1 - Da) + Da .(1 - Sa)
    srcFetch32(s, PixelARGB::kUC, 1);
    dstFetch32(d, PixelARGB::kUC, 1);

    x86::Vec& d0 = d.uc[0];
    x86::Vec& s0 = s.uc[0];
    x86::Vec& x0 = xv[0];
    x86::Vec& y0 = yv[0];

    if (hasMask) {
      pc->vmulu16(s0, s0, mv[0]);
      pc->vsrli16(s0, s0, 8);
    }

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

    pc->xSatisfyARGB32_1x(out, flags);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - PinLight]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_PIN_LIGHT) {
    // if 2.Sca <= Sa
    //   Dca' = min(Dca.Sa + Sca.(1 - Da) + Dca.(1 - Sa), 2.Sca.Da + Sca.(1 - Da) + Dca.(1 - Sa))
    //   Da'  = Da + Sa.(1 - Da)
    // else
    //   Dca' = max(Dca.Sa + Sca.(1 - Da) + Dca.(1 - Sa), 2.Sca.Da + Sca.(1 - Da) + Dca.(1 - Sa) - Da.Sa)
    //   Da'  = Da + Sa.(1 - Da)
    srcFetch32(s, PixelARGB::kUC, n);
    dstFetch32(d, PixelARGB::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);
    }

    // SPLIT.
    for (unsigned int i = 0; i < kSplit; i++) {
      VecArray sh = sv.even_odd(i);
      VecArray dh = dv.even_odd(i);
      VecArray xh = xv.even_odd(i);
      VecArray yh = yv.even_odd(i);
      VecArray zh = zv.even_odd(i);

      pc->vExpandAlpha16(xh, dh, kUseHi);                        // Da
      pc->vExpandAlpha16(yh, sh, kUseHi);                        // Sa

      pc->vinv255u16(xh, xh);                                    // 1 - Da
      pc->vmov(zh, yh);                                          // Sa
      pc->vinv255u16(yh, yh);                                    // 1 - Sa
      pc->vmulu16(xh, xh, sh);                                   // Sca.(1 - Da)
      pc->vmulu16(yh, yh, dh);                                   // Dca.(1 - Sa)

      pc->vaddi16(sh, sh, sh);                                   // 2.Sca
      pc->vaddi16(yh, yh, xh);                                   // Sca.(1 - Da) + Dca.(1 - Sa)
      pc->vExpandAlpha16(xh, dh, kUseHi);                        // Da

      pc->vmulu16(dh, dh, zh);                                   // Dca.Sa
      pc->vmulu16(xh, xh, sh);                                   // 2.Sca.Da
      pc->vcmpgti16(sh, sh, zh);                                 // 2.Sca > Sa
      pc->vExpandAlpha16(zh, dh, kUseHi);                        // Da.Sa

      pc->vaddi16(dh, dh, yh);                                   // Dca.Sa + Sca.(1 - Da) + Dca.(1 - Sa)
      pc->vsubi16(zh, zh, xh);                                   // Da.Sa - 2.Sca.Da
      pc->vaddi16(xh, xh, yh);                                   // 2.Sca.Da + Sca.(1 - Da) + Dca.(1 - Sa)
      pc->vsubi16(yh, yh, zh);                                   // 2.Sca.Da + Sca.(1 - Da) + Dca.(1 - Sa) - Da.Sa
      pc->vdiv255u16_3x(dh, yh, xh);

      pc->vmaxi16(yh, yh, dh);                                   // max(Dca.Sa + Sca.(1 - Da) + Dca.(1 - Sa), 2.Sca.Da + Sca.(1 - Da) + Dca.(1 - Sa) - Da.Sa)
      pc->vmini16(xh, xh, dh);                                   // min(Dca.Sa + Sca.(1 - Da) + Dca.(1 - Sa), 2.Sca.Da + Sca.(1 - Da) + Dca.(1 - Sa))

      pc->vand(yh, yh, sh);                                      // Select the right component according to the `s0` mask.
      pc->vandnot_a(sh, sh, xh);
      pc->vor(sh, sh, yh);
    }

    out.uc.init(sv);
    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - HardLight]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_HARD_LIGHT) {
    // if (2.Sca < Sa)
    //   Dca' = Dca + Sca - (Dca.Sa + Sca.Da - 2.Sca.Dca)
    //   Da'  = Da  + Sa  - Sa.Da
    // else
    //   Dca' = Dca + Sca + (Dca.Sa + Sca.Da - 2.Sca.Dca) - Sa.Da
    //   Da'  = Da  + Sa  - Sa.Da
    srcFetch32(s, PixelARGB::kUC, n);
    dstFetch32(d, PixelARGB::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);
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
    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - SoftLight (SCALAR)]
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
    srcFetch32(s, PixelARGB::kUC, n);
    dstFetch32(d, PixelARGB::kPC, n);

    x86::Vec& s0 = s.uc[0];
    x86::Vec& d0 = d.pc[0];

    x86::Vec  a0 = cc->newXmm("a0");
    x86::Vec  b0 = cc->newXmm("b0");
    x86::Vec& x0 = xv[0];
    x86::Vec& y0 = yv[0];
    x86::Vec& z0 = zv[0];

    if (hasMask) {
      pc->vmulu16(s0, s0, mv[0]);
      pc->vsrli16(s0, s0, 8);
    }

    pc->vmovu8u32(d0, d0);
    pc->vmovu16u32(s0, s0);
    pc->vloadps_128a(x0, pc->constAsMem(blCommonTable.f128_1div255));

    pc->vcvti32ps(s0, s0);
    pc->vcvti32ps(d0, d0);

    pc->vmulps(s0, s0, x0);                                      // Sca (0..1)
    pc->vmulps(d0, d0, x0);                                      // Dca (0..1)

    pc->vExpandAlphaPS(b0, d0);                                  // Da
    pc->vmulps(x0, s0, b0);                                      // Sca.Da
    pc->vmaxps(b0, b0, pc->constAsMem(blCommonTable.f128_1e_m3)); // max(Da, 0.001)

    pc->vdivps(a0, d0, b0);                                      // Dc <- Dca/Da
    pc->vaddps(d0, d0, s0);                                      // Dca + Sca

    pc->vExpandAlphaPS(y0, s0);                                  // Sa
    pc->vloadps_128a(z0, pc->constAsMem(blCommonTable.f128_4)); // 4

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
    pc->vloadps_128a(b0, pc->constAsMem(blCommonTable.f128_1)); // 1

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
    pc->vandps(s0, s0, pc->constAsMem(blCommonTable.i128_FFFFFFFF_FFFFFFFF_FFFFFFFF_0)); // Zero alpha.

    pc->vorps(z0, z0, x0);
    pc->vmulps(s0, s0, z0);

    pc->vaddps(d0, d0, s0);
    pc->vmulps(d0, d0, pc->constAsMem(blCommonTable.f128_255));

    // TODO: [PIPEGEN] Inspect if we need to packuswb here or we can use pshufb instead.
    pc->vcvtpsi32(d0, d0);
    pc->vpacki32i16(d0, d0, d0);
    pc->vpacki16u8(d0, d0, d0);
    out.pc.init(d0);

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Difference]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_DIFFERENCE) {
    // Dca' = Dca + Sca - 2.min(Sca.Da, Dca.Sa)
    // Da'  = Da  + Sa  -   min(Sa .Da, Da .Sa)
    if (!hasMask) {
      srcFetch32(s, PixelARGB::kUC | PixelARGB::kUA, n);
      dstFetch32(d, PixelARGB::kUC, n);

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
      srcFetch32(s, PixelARGB::kUC, n);
      dstFetch32(d, PixelARGB::kUC, n);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);

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

    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Exclusion]
  // --------------------------------------------------------------------------

  if (compOp() == BL_COMP_OP_EXCLUSION) {
    // Dca' = Dca + Sca - 2.Sca.Dca
    // Da'  = Da + Sa - Sa.Da
    srcFetch32(s, PixelARGB::kUC | (hasMask ? 0 : int(PixelARGB::kImmutable)), n);
    dstFetch32(d, PixelARGB::kUC, n);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->vmulu16(sv, sv, mv);
      pc->vsrli16(sv, sv, 8);
    }

    pc->vmulu16(xv, dv, sv);
    pc->vaddi16(dv, dv, sv);
    pc->vdiv255u16(xv);
    pc->vsubi16(dv, dv, xv);

    pc->vZeroAlphaW(xv, xv);
    pc->vsubi16(dv, dv, xv);

    out.uc.init(dv);
    pc->xSatisfyARGB32(out, flags, n);
    return;
  }

  // --------------------------------------------------------------------------
  // [VProc32 - Invalid]
  // --------------------------------------------------------------------------

  BL_NOT_REACHED();
}

void CompOpPart::vMaskProc32InvertMask(VecArray& mi, VecArray& mv) noexcept {
  uint32_t i;
  uint32_t size = mv.size();

  if (cMaskLoopType() == kCMaskLoopTypeMask) {
    if (_mask->vec.im.isValid()) {
      bool ok = true;

      // TODO: [PIPEGEN] A leftover from a template-based code, I don't understand
      // it anymore and it seems it's unnecessary so verify this and all places
      // that hit `ok == false`.
      for (i = 0; i < blMin(mi.size(), size); i++)
        if (mi[i].id() != mv[i].id())
          ok = false;

      if (ok) {
        mi.init(_mask->vec.im);
        return;
      }
    }
  }

  if (mi.empty())
    pc->newXmmArray(mi, size, "mi");

  if (mv.isScalar()) {
    pc->vinv256u16(mi[0], mv[0]);
    for (i = 1; i < size; i++)
      pc->vmov(mi[i], mi[0]);
  }
  else {
    pc->vinv256u16(mi, mv);
  }
}

void CompOpPart::vMaskProc32InvertDone(VecArray& mi, bool mImmutable) noexcept {
  if (cMaskLoopType() == kCMaskLoopTypeMask) {
    if (mi[0].id() == _mask->vec.m.id())
      pc->vinv256u16(mi[0], mi[0]);
  }
}

} // {BLPipeGen}
