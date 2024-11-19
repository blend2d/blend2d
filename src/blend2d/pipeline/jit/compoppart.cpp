// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../runtime_p.h"
#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/compoputils_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchpatternpart_p.h"
#include "../../pipeline/jit/fetchpixelptrpart_p.h"
#include "../../pipeline/jit/fetchsolidpart_p.h"
#include "../../pipeline/jit/fetchutilscoverage_p.h"
#include "../../pipeline/jit/fetchutilsinlineloops_p.h"
#include "../../pipeline/jit/fetchutilspixelaccess_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

// bl::Pipeline::JIT::CompOpPart - Construction & Destruction
// ==========================================================

CompOpPart::CompOpPart(PipeCompiler* pc, CompOpExt compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept
  : PipePart(pc, PipePartType::kComposite),
    _compOp(compOp),
    _pixelType(dstPart->hasRGB() ? PixelType::kRGBA32 : PixelType::kA8),
    _coverageFormat(PixelCoverageFormat::kUnpacked),
    _isInPartialMode(false),
    _hasDa(dstPart->hasAlpha()),
    _hasSa(srcPart->hasAlpha()),
    _solidPre("solid", _pixelType),
    _partialPixel("partial", _pixelType) {

  _mask->reset();

  // Initialize the children of this part.
  _children[kIndexDstPart] = dstPart;
  _children[kIndexSrcPart] = srcPart;
  _childCount = 2;

#if defined(BL_JIT_ARCH_X86)
  VecWidth maxVecWidth = VecWidth::k128;
  switch (pixelType()) {
    case PixelType::kA8: {
      maxVecWidth = VecWidth::k512;
      break;
    }

    case PixelType::kRGBA32: {
      switch (compOp) {
        case CompOpExt::kSrcOver    :
        case CompOpExt::kSrcCopy    :
        case CompOpExt::kSrcIn      :
        case CompOpExt::kSrcOut     :
        case CompOpExt::kSrcAtop    :
        case CompOpExt::kDstOver    :
        case CompOpExt::kDstIn      :
        case CompOpExt::kDstOut     :
        case CompOpExt::kDstAtop    :
        case CompOpExt::kXor        :
        case CompOpExt::kClear      :
        case CompOpExt::kPlus       :
        case CompOpExt::kMinus      :
        case CompOpExt::kModulate   :
        case CompOpExt::kMultiply   :
        case CompOpExt::kScreen     :
        case CompOpExt::kOverlay    :
        case CompOpExt::kDarken     :
        case CompOpExt::kLighten    :
        case CompOpExt::kLinearBurn :
        case CompOpExt::kPinLight   :
        case CompOpExt::kHardLight  :
        case CompOpExt::kDifference :
        case CompOpExt::kExclusion  :
          maxVecWidth = VecWidth::k512;
          break;

        case CompOpExt::kColorDodge :
        case CompOpExt::kColorBurn  :
        case CompOpExt::kLinearLight:
        case CompOpExt::kSoftLight  :
          break;

        default:
          break;
      }
      break;
    }

    default:
      BL_NOT_REACHED();
  }
  _maxVecWidthSupported = maxVecWidth;
#elif defined(BL_JIT_ARCH_A64)
  // TODO: [JIT] OPTIMIZATION: Every composition mode should use packed in the future (AArch64).
  if (isSrcCopy() || isSrcOver() || isScreen()) {
    _coverageFormat = PixelCoverageFormat::kPacked;
  }
#endif
}

// bl::Pipeline::JIT::CompOpPart - Prepare
// =======================================

void CompOpPart::preparePart() noexcept {
  bool isSolid = srcPart()->isSolid();
  uint32_t maxPixels = 0;
  uint32_t pixelLimit = 64;

  _partFlags |= (dstPart()->partFlags() | srcPart()->partFlags()) & PipePartFlags::kFetchFlags;

  if (srcPart()->hasMaskedAccess() && dstPart()->hasMaskedAccess())
    _partFlags |= PipePartFlags::kMaskedAccess;

  // Limit the maximum pixel-step to 4 it the style is not solid and the target is not 64-bit.
  // There's not enough registers to process 8 pixels in parallel in 32-bit mode.
  if (blRuntimeIs32Bit() && !isSolid && _pixelType != PixelType::kA8)
    pixelLimit = 4;

  // Decrease the maximum pixels to 4 if the source is expensive to fetch. In such case fetching and processing more
  // pixels would result in emitting bloated pipelines that are not faster compared to pipelines working with just
  // 4 pixels at a time.
  if (dstPart()->isExpensive() || srcPart()->isExpensive())
    pixelLimit = 4;

  switch (pixelType()) {
    case PixelType::kA8: {
      maxPixels = 8;
      break;
    }

    case PixelType::kRGBA32: {
      switch (compOp()) {
        case CompOpExt::kSrcOver    : maxPixels = 8; break;
        case CompOpExt::kSrcCopy    : maxPixels = 8; break;
        case CompOpExt::kSrcIn      : maxPixels = 8; break;
        case CompOpExt::kSrcOut     : maxPixels = 8; break;
        case CompOpExt::kSrcAtop    : maxPixels = 8; break;
        case CompOpExt::kDstOver    : maxPixels = 8; break;
        case CompOpExt::kDstIn      : maxPixels = 8; break;
        case CompOpExt::kDstOut     : maxPixels = 8; break;
        case CompOpExt::kDstAtop    : maxPixels = 8; break;
        case CompOpExt::kXor        : maxPixels = 8; break;
        case CompOpExt::kClear      : maxPixels = 8; break;
        case CompOpExt::kPlus       : maxPixels = 8; break;
        case CompOpExt::kMinus      : maxPixels = 4; break;
        case CompOpExt::kModulate   : maxPixels = 8; break;
        case CompOpExt::kMultiply   : maxPixels = 8; break;
        case CompOpExt::kScreen     : maxPixels = 8; break;
        case CompOpExt::kOverlay    : maxPixels = 4; break;
        case CompOpExt::kDarken     : maxPixels = 8; break;
        case CompOpExt::kLighten    : maxPixels = 8; break;
        case CompOpExt::kColorDodge : maxPixels = 1; break;
        case CompOpExt::kColorBurn  : maxPixels = 1; break;
        case CompOpExt::kLinearBurn : maxPixels = 8; break;
        case CompOpExt::kLinearLight: maxPixels = 1; break;
        case CompOpExt::kPinLight   : maxPixels = 4; break;
        case CompOpExt::kHardLight  : maxPixels = 4; break;
        case CompOpExt::kSoftLight  : maxPixels = 1; break;
        case CompOpExt::kDifference : maxPixels = 4; break;
        case CompOpExt::kExclusion  : maxPixels = 4; break;

        default:
          BL_NOT_REACHED();
      }
      break;
    }

    default:
      BL_NOT_REACHED();
  }

  if (maxPixels > 1) {
    maxPixels *= pc->vecMultiplier();
    pixelLimit *= pc->vecMultiplier();
  }

  // Decrease to N pixels at a time if the fetch part doesn't support more.
  // This is suboptimal, but can happen if the fetch part is not optimized.
  maxPixels = blMin(maxPixels, pixelLimit, srcPart()->maxPixels());

  if (isRGBA32Pixel()) {
    if (maxPixels >= 4)
      _minAlignment = 16;
  }

  setMaxPixels(maxPixels);
}

// bl::Pipeline::JIT::CompOpPart - Init & Fini
// ===========================================

void CompOpPart::init(const PipeFunction& fn, Gp& x, Gp& y, uint32_t pixelGranularity) noexcept {
  _pixelGranularity = uint8_t(pixelGranularity);

  dstPart()->init(fn, x, y, pixelType(), pixelGranularity);
  srcPart()->init(fn, x, y, pixelType(), pixelGranularity);
}

void CompOpPart::fini() noexcept {
  dstPart()->fini();
  srcPart()->fini();

  _pixelGranularity = 0;
}

// bl::Pipeline::JIT::CompOpPart - Optimization Opportunities
// ==========================================================

bool CompOpPart::shouldOptimizeOpaqueFill() const noexcept {
  // Should be always optimized if the source is not solid.
  if (!srcPart()->isSolid())
    return true;

  // Do not optimize if the CompOp is TypeA. This operator doesn't need any
  // special handling as the source pixel is multiplied with mask before it's
  // passed to the compositor.
  if (blTestFlag(compOpFlags(), CompOpFlags::kTypeA))
    return false;

  // Modulate operator just needs to multiply source with mask and add (1 - m)
  // to it.
  if (isModulate())
    return false;

  // We assume that in all other cases there is a benefit of using optimized
  // `cMask` loop for a fully opaque mask.
  return true;
}

bool CompOpPart::shouldJustCopyOpaqueFill() const noexcept {
  if (!isSrcCopy())
    return false;

  if (srcPart()->isSolid())
    return true;

  if (srcPart()->isFetchType(FetchType::kPatternAlignedBlit) && srcPart()->format() == dstPart()->format())
    return true;

  return false;
}

// bl::Pipeline::JIT::CompOpPart - Advance
// =======================================

void CompOpPart::startAtX(const Gp& x) noexcept {
  dstPart()->startAtX(x);
  srcPart()->startAtX(x);
}

void CompOpPart::advanceX(const Gp& x, const Gp& diff) noexcept {
  dstPart()->advanceX(x, diff);
  srcPart()->advanceX(x, diff);
}

void CompOpPart::advanceY() noexcept {
  dstPart()->advanceY();
  srcPart()->advanceY();
}

// bl::Pipeline::JIT::CompOpPart - Prefetch & Postfetch
// ====================================================

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

// bl::Pipeline::JIT::CompOpPart - Fetch
// =====================================

void CompOpPart::dstFetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  dstPart()->fetch(p, n, flags, predicate);
}

void CompOpPart::srcFetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  // Pixels must match as we have already pre-configured the CompOpPart.
  BL_ASSERT(p.type() == pixelType());

  if (p.count() == 0)
    p.setCount(n);

  // Composition with a preprocessed solid color.
  if (isUsingSolidPre()) {
    Pixel& s = _solidPre;

    // INJECT:
    {
      ScopedInjector injector(cc, &_cMaskLoopHook);
      FetchUtils::satisfySolidPixels(pc, s, flags);
    }

    if (p.isRGBA32()) {
      VecWidth pcVecWidth = pc->vecWidthOf(DataWidth::k32, n);
      VecWidth ucVecWidth = pc->vecWidthOf(DataWidth::k64, n);

      uint32_t pcCount = pc->vecCountOf(DataWidth::k32, n);
      uint32_t ucCount = pc->vecCountOf(DataWidth::k64, n);

      if (blTestFlag(flags, PixelFlags::kImmutable)) {
        if (blTestFlag(flags, PixelFlags::kPC)) {
          p.pc.init(VecWidthUtils::cloneVecAs(s.pc[0], pcVecWidth));
        }

        if (blTestFlag(flags, PixelFlags::kUC)) {
          p.uc.init(VecWidthUtils::cloneVecAs(s.uc[0], ucVecWidth));
        }

        if (blTestFlag(flags, PixelFlags::kUA)) {
          p.ua.init(VecWidthUtils::cloneVecAs(s.ua[0], ucVecWidth));
        }

        if (blTestFlag(flags, PixelFlags::kUI)) {
          p.ui.init(VecWidthUtils::cloneVecAs(s.ui[0], ucVecWidth));
        }
      }
      else {
        if (blTestFlag(flags, PixelFlags::kPC)) {
          pc->newVecArray(p.pc, pcCount, pcVecWidth, p.name(), "pc");
          pc->v_mov(p.pc, VecWidthUtils::cloneVecAs(s.pc[0], pcVecWidth));
        }

        if (blTestFlag(flags, PixelFlags::kUC)) {
          pc->newVecArray(p.uc, ucCount, ucVecWidth, p.name(), "uc");
          pc->v_mov(p.uc, VecWidthUtils::cloneVecAs(s.uc[0], ucVecWidth));
        }

        if (blTestFlag(flags, PixelFlags::kUA)) {
          pc->newVecArray(p.ua, ucCount, ucVecWidth, p.name(), "ua");
          pc->v_mov(p.ua, VecWidthUtils::cloneVecAs(s.ua[0], ucVecWidth));
        }

        if (blTestFlag(flags, PixelFlags::kUI)) {
          pc->newVecArray(p.ui, ucCount, ucVecWidth, p.name(), "ui");
          pc->v_mov(p.ui, VecWidthUtils::cloneVecAs(s.ui[0], ucVecWidth));
        }
      }
    }
    else if (p.isA8()) {
      // TODO: [JIT] UNIMPLEMENTED: A8 pipepine.
      BL_ASSERT(false);
    }

    return;
  }

  // Partial mode is designed to fetch pixels on the right side of the border one by one, so it's an error
  // if the pipeline requests more than 1 pixel at a time.
  if (isInPartialMode()) {
    BL_ASSERT(n == 1);

    if (p.isRGBA32()) {
      if (!blTestFlag(flags, PixelFlags::kImmutable)) {
        if (blTestFlag(flags, PixelFlags::kUC)) {
          pc->newV128Array(p.uc, 1, "uc");
          pc->v_cvt_u8_lo_to_u16(p.uc[0], _partialPixel.pc[0]);
        }
        else {
          pc->newV128Array(p.pc, 1, "pc");
          pc->v_mov(p.pc[0], _partialPixel.pc[0]);
        }
      }
      else {
        p.pc.init(_partialPixel.pc[0]);
      }
    }
    else if (p.isA8()) {
      p.sa = pc->newGp32("sa");
      pc->s_extract_u16(p.sa, _partialPixel.ua[0], 0);
    }

    FetchUtils::satisfyPixels(pc, p, flags);
    return;
  }

  srcPart()->fetch(p, n, flags, predicate);
}

// bl::Pipeline::JIT::CompOpPart - PartialFetch
// ============================================

void CompOpPart::enterPartialMode(PixelFlags partialFlags) noexcept {
  // Doesn't apply to solid fills.
  if (isUsingSolidPre())
    return;

  // TODO: [JIT] We only support partial fetch of 4 pixels at the moment.
  BL_ASSERT(!isInPartialMode());
  BL_ASSERT(pixelGranularity() == 4);

  switch (pixelType()) {
    case PixelType::kA8: {
      srcFetch(_partialPixel, pixelGranularity(), PixelFlags::kUA | partialFlags, pc->emptyPredicate());
      break;
    }

    case PixelType::kRGBA32: {
      srcFetch(_partialPixel, pixelGranularity(), PixelFlags::kPC | partialFlags, pc->emptyPredicate());
      break;
    }

    default:
      BL_NOT_REACHED();
  }

  _isInPartialMode = true;
}

void CompOpPart::exitPartialMode() noexcept {
  // Doesn't apply to solid fills.
  if (isUsingSolidPre())
    return;

  BL_ASSERT(isInPartialMode());

  _isInPartialMode = false;
  _partialPixel.resetAllExceptTypeAndName();
}

void CompOpPart::nextPartialPixel() noexcept {
  if (!isInPartialMode())
    return;

  switch (pixelType()) {
    case PixelType::kA8: {
      const Vec& pix = _partialPixel.ua[0];
      pc->shiftOrRotateRight(pix, pix, 2);
      break;
    }

    case PixelType::kRGBA32: {
      const Vec& pix = _partialPixel.pc[0];
      pc->shiftOrRotateRight(pix, pix, 4);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::JIT::CompOpPart - CMask - Init & Fini
// ===================================================

void CompOpPart::cMaskInit(const Mem& mem) noexcept {
  switch (pixelType()) {
    case PixelType::kA8: {
      Gp mGp = pc->newGp32("msk");
      pc->load_u8(mGp, mem);
      cMaskInitA8(mGp, Vec());
      break;
    }

    case PixelType::kRGBA32: {
      Vec vm = pc->newVec("vm");
      if (coverageFormat() == PixelCoverageFormat::kPacked)
        pc->v_broadcast_u8z(vm, mem);
      else
        pc->v_broadcast_u16z(vm, mem);
      cMaskInitRGBA32(vm);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::cMaskInit(const Gp& sm_, const Vec& vm_) noexcept {
  Gp sm(sm_);
  Vec vm(vm_);

  switch (pixelType()) {
    case PixelType::kA8: {
      cMaskInitA8(sm, vm);
      break;
    }

    case PixelType::kRGBA32: {
      if (!vm.isValid() && sm.isValid()) {
        vm = pc->newVec("vm");
        if (coverageFormat() == PixelCoverageFormat::kPacked)
          pc->v_broadcast_u8z(vm, sm);
        else
          pc->v_broadcast_u16z(vm, sm);
      }

      cMaskInitRGBA32(vm);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::cMaskInitOpaque() noexcept {
  switch (pixelType()) {
    case PixelType::kA8: {
      cMaskInitA8(Gp(), Vec());
      break;
    }

    case PixelType::kRGBA32: {
      cMaskInitRGBA32(Vec());
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::cMaskFini() noexcept {
  switch (pixelType()) {
    case PixelType::kA8: {
      cMaskFiniA8();
      break;
    }

    case PixelType::kRGBA32: {
      cMaskFiniRGBA32();
      break;
    }

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

// bl::Pipeline::JIT::CompOpPart - CMask - Generic Loop
// ====================================================

void CompOpPart::cMaskGenericLoop(Gp& i) noexcept {
  if (isLoopOpaque() && shouldJustCopyOpaqueFill()) {
    cMaskMemcpyOrMemsetLoop(i);
    return;
  }

  cMaskGenericLoopVec(i);
}

void CompOpPart::cMaskGenericLoopVec(Gp& i) noexcept {
  uint32_t n = maxPixels();

  Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();

  // 1 pixel at a time.
  if (n == 1) {
    Label L_Loop = pc->newLabel();

    pc->bind(L_Loop);
    cMaskProcStoreAdvance(dPtr, PixelCount(1), Alignment(1));
    pc->j(L_Loop, sub_nz(i, 1));

    return;
  }

  BL_ASSERT(minAlignment() >= 1);
  // uint32_t alignmentMask = minAlignment().value() - 1;

  // 4 pixels at a time.
  if (n == 4) {
    Label L_Loop = pc->newLabel();
    Label L_Tail = pc->newLabel();
    Label L_Done = pc->newLabel();

    enterN();
    prefetchN();

    pc->j(L_Tail, sub_c(i, n));

    pc->bind(L_Loop);
    cMaskProcStoreAdvance(dPtr, PixelCount(n));
    pc->j(L_Loop, sub_nc(i, n));

    pc->bind(L_Tail);
    pc->j(L_Done, add_z(i, n));

    PixelPredicate predicate;
    predicate.init(n, PredicateFlags::kNeverFull, i);
    cMaskProcStoreAdvance(dPtr, PixelCount(n), Alignment(1), predicate);

    pc->bind(L_Done);

    postfetchN();
    leaveN();
    return;
  }

  // 8 pixels at a time.
  if (n == 8) {
    Label L_LoopN = pc->newLabel();
    Label L_SkipN = pc->newLabel();
    Label L_Exit = pc->newLabel();

    enterN();
    prefetchN();

    pc->j(L_SkipN, sub_c(i, n));

    pc->bind(L_LoopN);
    cMaskProcStoreAdvance(dPtr, PixelCount(n), Alignment(1));
    pc->j(L_LoopN, sub_nc(i, n));

    pc->bind(L_SkipN);
    pc->j(L_Exit, add_z(i, n));

    if (pc->use512BitSimd()) {
      PixelPredicate predicate(n, PredicateFlags::kNeverFull, i);
      cMaskProcStoreAdvance(dPtr, PixelCount(n), Alignment(1), predicate);
    }
    else {
      Label L_Skip4 = pc->newLabel();
      pc->j(L_Skip4, ucmp_lt(i, 4));
      cMaskProcStoreAdvance(dPtr, PixelCount(4), Alignment(1));
      pc->j(L_Exit, sub_z(i, 4));

      pc->bind(L_Skip4);
      PixelPredicate predicate(8u, PredicateFlags::kNeverFull, i);
      cMaskProcStoreAdvance(dPtr, PixelCount(4), Alignment(1), predicate);
    }

    pc->bind(L_Exit);

    postfetchN();
    leaveN();

    return;
  }
  // 16 pixels at a time.
  if (n == 16) {
    Label L_LoopN = pc->newLabel();
    Label L_SkipN = pc->newLabel();
    Label L_Exit = pc->newLabel();

    enterN();
    prefetchN();

    pc->j(L_SkipN, sub_c(i, n));

    pc->bind(L_LoopN);
    cMaskProcStoreAdvance(dPtr, PixelCount(n), Alignment(1));
    pc->j(L_LoopN, sub_nc(i, n));

    pc->bind(L_SkipN);
    pc->j(L_Exit, add_z(i, n));

    if (pc->use512BitSimd()) {
      PixelPredicate predicate(n, PredicateFlags::kNeverFull, i);
      cMaskProcStoreAdvance(dPtr, PixelCount(n), Alignment(1), predicate);
    }
    else {
      Label L_Skip8 = pc->newLabel();
      pc->j(L_Skip8, ucmp_lt(i, 8));
      cMaskProcStoreAdvance(dPtr, PixelCount(8), Alignment(1));
      pc->j(L_Exit, sub_z(i, 8));

      pc->bind(L_Skip8);
      PixelPredicate predicate(8u, PredicateFlags::kNeverFull, i);
      cMaskProcStoreAdvance(dPtr, PixelCount(8), Alignment(1), predicate);
    }

    pc->bind(L_Exit);

    postfetchN();
    leaveN();

    return;
  }

  // 32 pixels at a time.
  if (n == 32) {
    Label L_LoopN = pc->newLabel();
    Label L_SkipN = pc->newLabel();
    Label L_Loop8 = pc->newLabel();
    Label L_Skip8 = pc->newLabel();
    Label L_Exit = pc->newLabel();

    enterN();
    prefetchN();

    pc->j(L_SkipN, sub_c(i, n));

    pc->bind(L_LoopN);
    cMaskProcStoreAdvance(dPtr, PixelCount(n), Alignment(1));
    pc->j(L_LoopN, sub_nc(i, n));

    pc->bind(L_SkipN);
    pc->j(L_Exit, add_z(i, n));
    pc->j(L_Skip8, sub_c(i, 8));

    pc->bind(L_Loop8);
    cMaskProcStoreAdvance(dPtr, PixelCount(8), Alignment(1));
    pc->j(L_Loop8, sub_nc(i, 8));

    pc->bind(L_Skip8);
    pc->j(L_Exit, add_z(i, 8));

    PixelPredicate predicate(8u, PredicateFlags::kNeverFull, i);
    cMaskProcStoreAdvance(dPtr, PixelCount(8), Alignment(1), predicate);

    pc->bind(L_Exit);

    postfetchN();
    leaveN();

    return;
  }

  BL_NOT_REACHED();
}

// bl::Pipeline::JIT::CompOpPart - CMask - Granular Loop
// =====================================================

void CompOpPart::cMaskGranularLoop(Gp& i) noexcept {
  if (isLoopOpaque() && shouldJustCopyOpaqueFill()) {
    cMaskMemcpyOrMemsetLoop(i);
    return;
  }

  cMaskGranularLoopVec(i);
}

void CompOpPart::cMaskGranularLoopVec(Gp& i) noexcept {
  BL_ASSERT(pixelGranularity() == 4);

  Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();
  if (pixelGranularity() == 4) {
    // 1 pixel at a time.
    if (maxPixels() == 1) {
      Label L_Loop = pc->newLabel();
      Label L_Step = pc->newLabel();

      pc->bind(L_Loop);
      enterPartialMode();

      pc->bind(L_Step);
      cMaskProcStoreAdvance(dPtr, PixelCount(1));
      pc->dec(i);
      nextPartialPixel();

      pc->j(L_Step, test_nz(i, 0x3));
      exitPartialMode();

      pc->j(L_Loop, test_nz(i));
      return;
    }

    // 4 pixels at a time.
    if (maxPixels() == 4) {
      Label L_Loop = pc->newLabel();

      pc->bind(L_Loop);
      cMaskProcStoreAdvance(dPtr, PixelCount(4));
      pc->j(L_Loop, sub_nz(i, 4));

      return;
    }

    // 8 pixels at a time.
    if (maxPixels() == 8) {
      Label L_Loop_Iter8 = pc->newLabel();
      Label L_Skip = pc->newLabel();
      Label L_End = pc->newLabel();

      pc->j(L_Skip, sub_c(i, 8));

      pc->bind(L_Loop_Iter8);
      cMaskProcStoreAdvance(dPtr, PixelCount(8));
      pc->j(L_Loop_Iter8, sub_nc(i, 8));

      pc->bind(L_Skip);
      pc->j(L_End, add_z(i, 8));

      // 4 remaining pixels.
      cMaskProcStoreAdvance(dPtr, PixelCount(4));

      pc->bind(L_End);
      return;
    }

    // 16 pixels at a time.
    if (maxPixels() == 16) {
      Label L_Loop_Iter16 = pc->newLabel();
      Label L_Loop_Iter4 = pc->newLabel();
      Label L_Skip = pc->newLabel();
      Label L_End = pc->newLabel();

      pc->j(L_Skip, sub_c(i, 16));

      pc->bind(L_Loop_Iter16);
      cMaskProcStoreAdvance(dPtr, PixelCount(16));
      pc->j(L_Loop_Iter16, sub_nc(i, 16));

      pc->bind(L_Skip);
      pc->j(L_End, add_z(i, 16));

      // 4 remaining pixels.
      pc->bind(L_Loop_Iter4);
      cMaskProcStoreAdvance(dPtr, PixelCount(4));
      pc->j(L_Loop_Iter4, sub_nz(i, 4));

      pc->bind(L_End);
      return;
    }

    // 32 pixels at a time.
    if (maxPixels() == 32) {
      Label L_Loop_Iter32 = pc->newLabel();
      Label L_Loop_Iter4 = pc->newLabel();
      Label L_Skip = pc->newLabel();
      Label L_End = pc->newLabel();

      pc->j(L_Skip, sub_c(i, 32));

      pc->bind(L_Loop_Iter32);
      cMaskProcStoreAdvance(dPtr, PixelCount(32));
      pc->j(L_Loop_Iter32, sub_nc(i, 32));

      pc->bind(L_Skip);
      pc->j(L_End, add_z(i, 32));

      // 4 remaining pixels.
      pc->bind(L_Loop_Iter4);
      cMaskProcStoreAdvance(dPtr, PixelCount(4));
      pc->j(L_Loop_Iter4, sub_nz(i, 4));

      pc->bind(L_End);
      return;
    }
  }

  BL_NOT_REACHED();
}

// bl::Pipeline::JIT::CompOpPart - CMask - MemCopy & MemSet Loop
// =============================================================

void CompOpPart::cMaskMemcpyOrMemsetLoop(Gp& i) noexcept {
  BL_ASSERT(shouldJustCopyOpaqueFill());
  Gp dPtr = dstPart()->as<FetchPixelPtrPart>()->ptr();

  if (srcPart()->isSolid()) {
    // Optimized solid opaque fill -> MemSet.
    BL_ASSERT(_solidOpt.px.isValid());
    FetchUtils::inlineFillSpanLoop(pc, dPtr, _solidOpt.px, i, 64, dstPart()->bpp(), pixelGranularity().value());
  }
  else if (srcPart()->isFetchType(FetchType::kPatternAlignedBlit)) {
    // Optimized solid opaque blit -> MemCopy.
    FetchUtils::inlineCopySpanLoop(pc, dPtr, srcPart()->as<FetchSimplePatternPart>()->f->srcp1, i, 64, dstPart()->bpp(), pixelGranularity().value(), dstPart()->format());
  }
  else {
    BL_NOT_REACHED();
  }
}

// bl::Pipeline::JIT::CompOpPart - CMask - Composition Helpers
// ===========================================================

void CompOpPart::cMaskProcStoreAdvance(const Gp& dPtr, PixelCount n, Alignment alignment) noexcept {
  PixelPredicate ptrMask;
  cMaskProcStoreAdvance(dPtr, n, alignment, ptrMask);
}

void CompOpPart::cMaskProcStoreAdvance(const Gp& dPtr, PixelCount n, Alignment alignment, PixelPredicate& predicate) noexcept {
  Pixel dPix("d", pixelType());

  switch (pixelType()) {
    case PixelType::kA8: {
      if (n == 1)
        cMaskProcA8Gp(dPix, PixelFlags::kSA | PixelFlags::kImmutable);
      else
        cMaskProcA8Vec(dPix, n, PixelFlags::kImmutable, predicate);
      FetchUtils::storePixelsAndAdvance(pc, dPtr, dPix, n, 1, alignment, predicate);
      break;
    }

    case PixelType::kRGBA32: {
      cMaskProcRGBA32Vec(dPix, n, PixelFlags::kImmutable, predicate);
      FetchUtils::storePixelsAndAdvance(pc, dPtr, dPix, n, 4, alignment, predicate);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::JIT::CompOpPart - VMask - Composition Helpers
// ===========================================================

enum class CompOpLoopStrategy : uint32_t {
  kLoop1,
  kLoopNTail4,
  kLoopNTailN
};

void CompOpPart::vMaskGenericLoop(Gp& i, const Gp& dPtr, const Gp& mPtr, GlobalAlpha* ga, const Label& done) noexcept {
  CompOpLoopStrategy strategy = CompOpLoopStrategy::kLoop1;

  if (maxPixels() >= 8) {
    strategy = CompOpLoopStrategy::kLoopNTail4;
  }
  else if (maxPixels() >= 4) {
    strategy = CompOpLoopStrategy::kLoopNTailN;
  }

  switch (strategy) {
    case CompOpLoopStrategy::kLoop1: {
      Label L_Loop1 = pc->newLabel();
      Label L_Done = done.isValid() ? done : pc->newLabel();

      pc->bind(L_Loop1);
      vMaskGenericStep(dPtr, PixelCount(1), mPtr, ga);
      pc->j(L_Loop1, sub_nz(i, 1));

      if (done.isValid())
        pc->j(L_Done);
      else
        pc->bind(L_Done);

      break;
    }

    case CompOpLoopStrategy::kLoopNTail4: {
      uint32_t n = blMin<uint32_t>(maxPixels(), 8);

      Label L_LoopN = pc->newLabel();
      Label L_SkipN = pc->newLabel();
      Label L_Skip4 = pc->newLabel();
      Label L_Done = pc->newLabel();

      enterN();
      prefetchN();

      pc->j(L_SkipN, sub_c(i, n));

      pc->bind(L_LoopN);
      vMaskGenericStep(dPtr, PixelCount(n), mPtr, ga);
      pc->j(L_LoopN, sub_nc(i, n));

      pc->bind(L_SkipN);
      pc->j(L_Done, add_z(i, n));

      pc->j(L_Skip4, ucmp_lt(i, 4));
      vMaskGenericStep(dPtr, PixelCount(4), mPtr, ga);
      pc->j(L_Done, sub_z(i, 4));

      pc->bind(L_Skip4);
      PixelPredicate predicate(n, PredicateFlags::kNeverFull, i);
      vMaskGenericStep(dPtr, PixelCount(4), mPtr, ga, predicate);
      pc->bind(L_Done);

      postfetchN();
      leaveN();

      if (done.isValid())
        pc->j(done);

      break;
    }

    case CompOpLoopStrategy::kLoopNTailN: {
      uint32_t n = blMin<uint32_t>(maxPixels(), 8);

      Label L_LoopN = pc->newLabel();
      Label L_SkipN = pc->newLabel();
      Label L_Done = pc->newLabel();

      enterN();
      prefetchN();

      pc->j(L_SkipN, sub_c(i, n));

      pc->bind(L_LoopN);
      vMaskGenericStep(dPtr, PixelCount(n), mPtr, ga);
      pc->j(L_LoopN, sub_nc(i, n));

      pc->bind(L_SkipN);
      pc->j(L_Done, add_z(i, n));

      PixelPredicate predicate(n, PredicateFlags::kNeverFull, i);
      vMaskGenericStep(dPtr, PixelCount(n), mPtr, ga, predicate);

      pc->bind(L_Done);

      postfetchN();
      leaveN();

      if (done.isValid())
        pc->j(done);

      break;
    }
  }
}

void CompOpPart::vMaskGenericStep(const Gp& dPtr, PixelCount n, const Gp& mPtr, GlobalAlpha* ga) noexcept {
  PixelPredicate noPredicate;
  vMaskGenericStep(dPtr, n, mPtr, ga, noPredicate);
}

void CompOpPart::vMaskGenericStep(const Gp& dPtr, PixelCount n, const Gp& mPtr, GlobalAlpha* ga, PixelPredicate& predicate) noexcept {
  switch (pixelType()) {
    case PixelType::kA8: {
      if (n == 1u) {
        BL_ASSERT(predicate.empty());

        Gp sm = pc->newGp32("sm");
        pc->load_u8(sm, mem_ptr(mPtr));
        pc->add(mPtr, mPtr, n.value());

        if (ga) {
          pc->mul(sm, sm, ga->sa().r32());
          pc->div_255_u32(sm, sm);
        }

        Pixel dPix("d", pixelType());
        vMaskProcA8Gp(dPix, PixelFlags::kSA | PixelFlags::kImmutable, sm, PixelCoverageFlags::kNone);
        FetchUtils::storePixelsAndAdvance(pc, dPtr, dPix, n, 1, Alignment(1), pc->emptyPredicate());
      }
      else {
        VecArray vm;
        FetchUtils::fetchMaskA8(pc, vm, mPtr, n, pixelType(), coverageFormat(), AdvanceMode::kAdvance, predicate, ga);
        vMaskProcStoreAdvance(dPtr, n, vm, PixelCoverageFlags::kNone, Alignment(1), predicate);
      }
      break;
    }

    case PixelType::kRGBA32: {
      VecArray vm;
      FetchUtils::fetchMaskA8(pc, vm, mPtr, n, pixelType(), coverageFormat(), AdvanceMode::kAdvance, predicate, ga);
      vMaskProcStoreAdvance(dPtr, n, vm, PixelCoverageFlags::kNone, Alignment(1), predicate);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::vMaskProcStoreAdvance(const Gp& dPtr, PixelCount n, const VecArray& vm, PixelCoverageFlags coverageFlags, Alignment alignment) noexcept {
  PixelPredicate ptrMask;
  vMaskProcStoreAdvance(dPtr, n, vm, coverageFlags, alignment, ptrMask);
}

void CompOpPart::vMaskProcStoreAdvance(const Gp& dPtr, PixelCount n, const VecArray& vm, PixelCoverageFlags coverageFlags, Alignment alignment, PixelPredicate& predicate) noexcept {
  Pixel dPix("d", pixelType());

  switch (pixelType()) {
    case PixelType::kA8: {
      BL_ASSERT(n != 1);

      vMaskProcA8Vec(dPix, n, PixelFlags::kPA | PixelFlags::kImmutable, vm, coverageFlags, predicate);
      FetchUtils::storePixelsAndAdvance(pc, dPtr, dPix, n, 1, alignment, predicate);
      break;
    }

    case PixelType::kRGBA32: {
      vMaskProcRGBA32Vec(dPix, n, PixelFlags::kImmutable, vm, coverageFlags, predicate);
      FetchUtils::storePixelsAndAdvance(pc, dPtr, dPix, n, 4, alignment, predicate);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::vMaskProc(Pixel& out, PixelFlags flags, Gp& msk, PixelCoverageFlags coverageFlags) noexcept {
  switch (pixelType()) {
    case PixelType::kA8: {
      vMaskProcA8Gp(out, flags, msk, coverageFlags);
      break;
    }

    case PixelType::kRGBA32: {
      Vec vm = pc->newV128("c.vm");

#if defined(BL_JIT_ARCH_X86)
      if (!pc->hasAVX()) {
        pc->s_mov_u32(vm, msk);
        pc->v_swizzle_lo_u16x4(vm, vm, swizzle(0, 0, 0, 0));
      }
      else
#endif
      {
        pc->v_broadcast_u16(vm, msk);
      }

      VecArray vm_(vm);
      vMaskProcRGBA32Vec(out, PixelCount(1), flags, vm_, PixelCoverageFlags::kNone, pc->emptyPredicate());
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::JIT::CompOpPart - CMask - Init & Fini - A8
// ========================================================

void CompOpPart::cMaskInitA8(const Gp& sm_, const Vec& vm_) noexcept {
  Gp sm(sm_);
  Vec vm(vm_);

  bool hasMask = sm.isValid() || vm.isValid();
  if (hasMask) {
    // SM must be 32-bit, so make it 32-bit if it's 64-bit for any reason.
    if (sm.isValid()) {
      sm = sm.r32();
    }

    if (vm.isValid() && !sm.isValid()) {
      sm = pc->newGp32("sm");
      pc->s_extract_u16(sm, vm, 0);
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

    if (isSrcCopy()) {
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
#if defined(BL_JIT_ARCH_A64)
        // Xa  = (Sa * m)
        // Vn  = (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = pc->newGp32("p.sx");
        o.sy = pc->newGp32("p.sy");

        pc->mul(o.sx, s.sa, sm);
        pc->inv_u8(o.sy, sm);

        if (maxPixels() > 1) {
          o.ux = pc->newVec("p.ux");
          o.vn = pc->newVec("p.vn");

          pc->v_broadcast_u16(o.ux, o.sx);
          pc->v_broadcast_u8(o.vn, o.sy);
        }

        convertToVec = false;
#else
        // Xa = (Sa * m) + <Rounding>
        // Ya = (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = pc->newGp32("p.sx");
        o.sy = sm;

        pc->mul(o.sx, s.sa, o.sy);
        pc->add(o.sx, o.sx, imm(0x80)); // Rounding
        pc->inv_u8(o.sy, o.sy);
#endif
      }
    }

    // CMaskInit - A8 - Solid - SrcOver
    // --------------------------------

    else if (isSrcOver()) {
      if (!hasMask) {
        // Xa = Sa * 1 + 0.5 <Rounding>
        // Ya = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = pc->newGp32("p.sx");
        o.sy = sm;

        pc->mov(o.sx, s.sa);
        pc->shl(o.sx, o.sx, 8);
        pc->sub(o.sx, o.sx, s.sa);
        pc->inv_u8(o.sy, o.sy);
      }
      else {
        // Xa = Sa * m + 0.5 <Rounding>
        // Ya = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = pc->newGp32("p.sx");
        o.sy = sm;

        pc->mul(o.sy, sm, s.sa);
        pc->div_255_u32(o.sy, o.sy);

        pc->shl(o.sx, o.sy, imm(8));
        pc->sub(o.sx, o.sx, o.sy);
#if defined(BL_JIT_ARCH_X86)
        pc->add(o.sx, o.sx, imm(0x80));
#endif  // BL_JIT_ARCH_X86
        pc->inv_u8(o.sy, o.sy);
      }

#if defined(BL_JIT_ARCH_A64)
      if (maxPixels() > 1) {
        o.ux = pc->newVec("p.ux");
        o.py = pc->newVec("p.py");

        pc->v_broadcast_u16(o.ux, o.sx);
        pc->v_broadcast_u8(o.py, o.sy);
      }

      convertToVec = false;
#endif // BL_JIT_ARCH_A64
    }

    // CMaskInit - A8 - Solid - SrcIn
    // ------------------------------

    else if (isSrcIn()) {
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

        o.sx = pc->newGp32("o.sx");
        pc->mul(o.sx, s.sa, sm);
        pc->div_255_u32(o.sx, o.sx);
        pc->inv_u8(sm, sm);
        pc->add(o.sx, o.sx, sm);
      }
    }

    // CMaskInit - A8 - Solid - SrcOut
    // -------------------------------

    else if (isSrcOut()) {
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

        o.sx = pc->newGp32("o.sx");
        o.sy = sm;

        pc->mul(o.sx, s.sa, o.sy);
        pc->div_255_u32(o.sx, o.sx);
        pc->inv_u8(o.sy, o.sy);
      }
    }

    // CMaskInit - A8 - Solid - DstOut
    // -------------------------------

    else if (isDstOut()) {
      if (!hasMask) {
        // Xa = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = pc->newGp32("o.sx");
        pc->inv_u8(o.sx, s.sa);

        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUI);
          o.ux = s.ui[0];
        }
      }
      else {
        // Xa = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = sm;
        pc->mul(o.sx, sm, s.sa);
        pc->div_255_u32(o.sx, o.sx);
        pc->inv_u8(o.sx, o.sx);
      }
    }

    // CMaskInit - A8 - Solid - Xor
    // ----------------------------

    else if (isXor()) {
      if (!hasMask) {
        // Xa = Sa
        // Ya = 1 - Xa (SIMD only)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);
        o.sx = s.sa;

        if (maxPixels() > 1) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUA | PixelFlags::kUI);

          o.ux = s.ua[0];
          o.uy = s.ui[0];
        }
      }
      else {
        // Xa = Sa * m
        // Ya = 1 - Xa (SIMD only)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kSA);

        o.sx = pc->newGp32("o.sx");
        pc->mul(o.sx, sm, s.sa);
        pc->div_255_u32(o.sx, o.sx);

        if (maxPixels() > 1) {
          o.ux = pc->newVec("o.ux");
          o.uy = pc->newVec("o.uy");
          pc->v_broadcast_u16(o.ux, o.sx);
          pc->v_inv255_u16(o.uy, o.ux);
        }
      }
    }

    // CMaskInit - A8 - Solid - Plus
    // -----------------------------

    else if (isPlus()) {
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
        pc->mul(o.sx, o.sx, s.sa);
        pc->div_255_u32(o.sx, o.sx);

        if (maxPixels() > 1) {
          o.px = pc->newVec("o.px");
          pc->mul(o.sx, o.sx, 0x01010101);
          pc->v_broadcast_u32(o.px, o.sx);
          pc->shr(o.sx, o.sx, imm(24));
        }

        convertToVec = false;
      }
    }

    // CMaskInit - A8 - Solid - Extras
    // -------------------------------

    if (convertToVec && maxPixels() > 1) {
      if (o.sx.isValid() && !o.ux.isValid()) {
        if (coverageFormat() == PixelCoverageFormat::kPacked) {
          o.px = pc->newVec("p.px");
          pc->v_broadcast_u8(o.px, o.sx);
        }
        else {
          o.ux = pc->newVec("p.ux");
          pc->v_broadcast_u16(o.ux, o.sx);
        }
      }

      if (o.sy.isValid() && !o.uy.isValid()) {
        if (coverageFormat() == PixelCoverageFormat::kPacked) {
          o.py = pc->newVec("p.py");
          pc->v_broadcast_u8(o.py, o.sy);
        }
        else {
          o.uy = pc->newVec("p.uy");
          pc->v_broadcast_u16(o.uy, o.sy);
        }
      }
    }
  }
  else {
    if (sm.isValid() && !vm.isValid() && maxPixels() > 1) {
      vm = pc->newVec("vm");
      if (coverageFormat() == PixelCoverageFormat::kPacked) {
        pc->v_broadcast_u8z(vm, sm);
      }
      else {
        pc->v_broadcast_u16z(vm, sm);
      }
      _mask->vm = vm;
    }

    /*
    // CMaskInit - A8 - NonSolid - SrcCopy
    // -----------------------------------

    if (isSrcCopy()) {
      if (hasMask) {
        Vec vn = pc->newVec("vn");
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
    // TODO: [JIT] ???
  }

  _mask->reset();
  _cMaskLoopFini();
}

// bl::Pipeline::JIT::CompOpPart - CMask - Proc - A8
// =================================================

void CompOpPart::cMaskProcA8Gp(Pixel& out, PixelFlags flags) noexcept {
  out.setCount(PixelCount(1));

  bool hasMask = isLoopCMask();

  if (srcPart()->isSolid()) {
    Pixel d("d", pixelType());
    SolidPixel& o = _solidOpt;

    Gp& da = d.sa;
    Gp sx = pc->newGp32("sx");

    // CMaskProc - A8 - SrcCopy
    // ------------------------

    if (isSrcCopy()) {
      if (!hasMask) {
        // Da' = Xa
        out.sa = o.sa;
        out.makeImmutable();
      }
      else {
        // Da' = Xa  + Da .(1 - m)
        dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

        pc->mul(da, da, o.sy),
        pc->add(da, da, o.sx);
        pc->mul_257_hu16(da, da);

        out.sa = da;
      }

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - A8 - SrcOver
    // ------------------------

    if (isSrcOver()) {
      // Da' = Xa + Da .Ya
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(da, da, o.sy);
      pc->add(da, da, o.sx);
      pc->mul_257_hu16(da, da);

      out.sa = da;

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - A8 - SrcIn & DstOut
    // -------------------------------

    if (isSrcIn() || isDstOut()) {
      // Da' = Xa.Da
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(da, da, o.sx);
      pc->div_255_u32(da, da);
      out.sa = da;

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - A8 - SrcOut
    // -----------------------

    if (isSrcOut()) {
      if (!hasMask) {
        // Da' = Xa.(1 - Da)
        dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

        pc->inv_u8(da, da);
        pc->mul(da, da, o.sx);
        pc->div_255_u32(da, da);
        out.sa = da;
      }
      else {
        // Da' = Xa.(1 - Da) + Da.Ya
        dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

        pc->inv_u8(sx, da);
        pc->mul(da, da, o.sy);
        pc->mul(sx, sx, o.sx);
        pc->add(da, da, sx);
        pc->div_255_u32(da, da);
        out.sa = da;
      }

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - A8 - Xor
    // --------------------

    if (isXor()) {
      // Da' = Xa.(1 - Da) + Da.Ya
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(sx, da, o.sy);
      pc->inv_u8(da, da);
      pc->mul(da, da, o.sx);
      pc->add(da, da, sx);
      pc->div_255_u32(da, da);
      out.sa = da;

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - A8 - Plus
    // ---------------------

    if (isPlus()) {
      // Da' = Clamp(Da + Xa)
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->adds_u8(da, da, o.sx);
      out.sa = da;

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }
  }

  vMaskProcA8Gp(out, flags, _mask->sm, PixelCoverageFlags::kImmutable);
}

void CompOpPart::cMaskProcA8Vec(Pixel& out, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  out.setCount(n);

  bool hasMask = isLoopCMask();

  if (srcPart()->isSolid()) {
    Pixel d("d", pixelType());
    SolidPixel& o = _solidOpt;

    VecWidth paVecWidth = pc->vecWidthOf(DataWidth::k8, n);
    VecWidth uaVecWidth = pc->vecWidthOf(DataWidth::k16, n);
    uint32_t kFullN = pc->vecCountOf(DataWidth::k16, n);

    VecArray xa;
    pc->newVecArray(xa, kFullN, uaVecWidth, "x");

    // CMaskProc - A8 - SrcCopy
    // ------------------------

    if (isSrcCopy()) {
      if (!hasMask) {
        // Da' = Xa
        out.pa.init(VecWidthUtils::cloneVecAs(o.px, paVecWidth));
        out.makeImmutable();
      }
      else {
#if defined(BL_JIT_ARCH_A64)
        dstFetch(d, n, PixelFlags::kPA, predicate);

        CompOpUtils::mul_u8_widen(pc, xa, d.pa, o.vn, n.value());
        pc->v_add_u16(xa, xa, o.ux);
        CompOpUtils::combineDiv255AndOutA8(pc, out, flags, xa);
#else
        // Da' = Xa + Da .(1 - m)
        dstFetch(d, n, PixelFlags::kUA, predicate);

        Vec s_ux = o.ux.cloneAs(d.ua[0]);
        Vec s_uy = o.uy.cloneAs(d.ua[0]);

        pc->v_mul_i16(d.ua, d.ua, s_uy),
        pc->v_add_i16(d.ua, d.ua, s_ux);
        pc->v_mul257_hi_u16(d.ua, d.ua);

        out.ua.init(d.ua);
#endif
      }

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - A8 - SrcOver
    // ------------------------

    if (isSrcOver()) {
#if defined(BL_JIT_ARCH_A64)
      // Da' = Xa + Da.Ya
      dstFetch(d, n, PixelFlags::kPA, predicate);

      CompOpUtils::mul_u8_widen(pc, xa, d.pa, o.py, n.value());
      pc->v_add_i16(xa, xa, o.ux);
      CompOpUtils::combineDiv255AndOutA8(pc, out, flags, xa);
#else
      // Da' = Xa + Da.Ya
      dstFetch(d, n, PixelFlags::kUA, predicate);

      Vec s_ux = o.ux.cloneAs(d.ua[0]);
      Vec s_uy = o.uy.cloneAs(d.ua[0]);

      pc->v_mul_i16(d.ua, d.ua, s_uy);
      pc->v_add_i16(d.ua, d.ua, s_ux);
      pc->v_mul257_hi_u16(d.ua, d.ua);

      out.ua.init(d.ua);
#endif

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - A8 - SrcIn & DstOut
    // -------------------------------

    if (isSrcIn() || isDstOut()) {
      // Da' = Xa.Da
      dstFetch(d, n, PixelFlags::kUA, predicate);

      Vec s_ux = o.ux.cloneAs(d.ua[0]);

      pc->v_mul_u16(d.ua, d.ua, s_ux);
      pc->v_div255_u16(d.ua);
      out.ua.init(d.ua);

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - A8 - SrcOut
    // -----------------------

    if (isSrcOut()) {
      if (!hasMask) {
        // Da' = Xa.(1 - Da)
        dstFetch(d, n, PixelFlags::kUA, predicate);

        Vec s_ux = o.ux.cloneAs(d.ua[0]);

        pc->v_inv255_u16(d.ua, d.ua);
        pc->v_mul_u16(d.ua, d.ua, s_ux);
        pc->v_div255_u16(d.ua);
        out.ua.init(d.ua);
      }
      else {
        // Da' = Xa.(1 - Da) + Da.Ya
        dstFetch(d, n, PixelFlags::kUA, predicate);

        Vec s_ux = o.ux.cloneAs(d.ua[0]);
        Vec s_uy = o.uy.cloneAs(d.ua[0]);

        pc->v_inv255_u16(xa, d.ua);
        pc->v_mul_u16(xa, xa, s_ux);
        pc->v_mul_u16(d.ua, d.ua, s_uy);
        pc->v_add_i16(d.ua, d.ua, xa);
        pc->v_div255_u16(d.ua);
        out.ua.init(d.ua);
      }

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - A8 - Xor
    // --------------------

    if (isXor()) {
      // Da' = Xa.(1 - Da) + Da.Ya
      dstFetch(d, n, PixelFlags::kUA, predicate);

      Vec s_ux = o.ux.cloneAs(d.ua[0]);
      Vec s_uy = o.uy.cloneAs(d.ua[0]);

      pc->v_mul_u16(xa, d.ua, s_uy);
      pc->v_inv255_u16(d.ua, d.ua);
      pc->v_mul_u16(d.ua, d.ua, s_ux);
      pc->v_add_i16(d.ua, d.ua, xa);
      pc->v_div255_u16(d.ua);
      out.ua.init(d.ua);

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - A8 - Plus
    // ---------------------

    if (isPlus()) {
      // Da' = Clamp(Da + Xa)
      dstFetch(d, n, PixelFlags::kPA, predicate);

      Vec s_px = o.px.cloneAs(d.pa[0]);

      pc->v_adds_u8(d.pa, d.pa, s_px);
      out.pa.init(d.pa);

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }
  }

  VecArray vm;
  if (_mask->vm.isValid())
    vm.init(_mask->vm);
  vMaskProcA8Vec(out, n, flags, vm, PixelCoverageFlags::kRepeatedImmutable, predicate);
}

// bl::Pipeline::JIT::CompOpPart - VMask Proc - A8 (Scalar)
// ========================================================

void CompOpPart::vMaskProcA8Gp(Pixel& out, PixelFlags flags, const Gp& msk, PixelCoverageFlags coverageFlags) noexcept {
  bool hasMask = msk.isValid();

  Pixel d("d", PixelType::kA8);
  Pixel s("s", PixelType::kA8);

  Gp x = pc->newGp32("@x");
  Gp y = pc->newGp32("@y");

  Gp& da = d.sa;
  Gp& sa = s.sa;

  out.setCount(PixelCount(1));

  // VMask - A8 - SrcCopy
  // --------------------

  if (isSrcCopy()) {
    if (!hasMask) {
      // Da' = Sa
      srcFetch(out, PixelCount(1), flags, pc->emptyPredicate());
    }
    else {
      // Da' = Sa.m + Da.(1 - m)
      srcFetch(s, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(sa, sa, msk);
      pc->inv_u8(msk, msk);
      pc->mul(da, da, msk);

      if (blTestFlag(coverageFlags, PixelCoverageFlags::kImmutable))
        pc->inv_u8(msk, msk);

      pc->add(da, da, sa);
      pc->div_255_u32(da, da);

      out.sa = da;
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - SrcOver
  // --------------------

  if (isSrcOver()) {
    if (!hasMask) {
      // Da' = Sa + Da.(1 - Sa)
      srcFetch(s, PixelCount(1), PixelFlags::kSA | PixelFlags::kImmutable, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->inv_u8(x, sa);
      pc->mul(da, da, x);
      pc->div_255_u32(da, da);
      pc->add(da, da, sa);
    }
    else {
      // Da' = Sa.m + Da.(1 - Sa.m)
      srcFetch(s, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(sa, sa, msk);
      pc->div_255_u32(sa, sa);
      pc->inv_u8(x, sa);
      pc->mul(da, da, x);
      pc->div_255_u32(da, da);
      pc->add(da, da, sa);
    }

    out.sa = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - SrcIn
  // ------------------

  if (isSrcIn()) {
    if (!hasMask) {
      // Da' = Sa.Da
      srcFetch(s, PixelCount(1), PixelFlags::kSA | PixelFlags::kImmutable, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(da, da, sa);
      pc->div_255_u32(da, da);
    }
    else {
      // Da' = Da.(Sa.m) + Da.(1 - m)
      //     = Da.(Sa.m + 1 - m)
      srcFetch(s, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(sa, sa, msk);
      pc->div_255_u32(sa, sa);
      pc->add(sa, sa, imm(255));
      pc->sub(sa, sa, msk);
      pc->mul(da, da, sa);
      pc->div_255_u32(da, da);
    }

    out.sa = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - SrcOut
  // -------------------

  if (isSrcOut()) {
    if (!hasMask) {
      // Da' = Sa.(1 - Da)
      srcFetch(s, PixelCount(1), PixelFlags::kSA | PixelFlags::kImmutable, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->inv_u8(da, da);
      pc->mul(da, da, sa);
      pc->div_255_u32(da, da);
    }
    else {
      // Da' = Sa.m.(1 - Da) + Da.(1 - m)
      srcFetch(s, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(sa, sa, msk);
      pc->div_255_u32(sa, sa);

      pc->inv_u8(x, da);
      pc->inv_u8(msk, msk);
      pc->mul(sa, sa, x);
      pc->mul(da, da, msk);

      if (blTestFlag(coverageFlags, PixelCoverageFlags::kImmutable))
        pc->inv_u8(msk, msk);

      pc->add(da, da, sa);
      pc->div_255_u32(da, da);
    }

    out.sa = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - DstOut
  // -------------------

  if (isDstOut()) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa)
      srcFetch(s, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->inv_u8(sa, sa);
      pc->mul(da, da, sa);
      pc->div_255_u32(da, da);
    }
    else {
      // Da' = Da.(1 - Sa.m)
      srcFetch(s, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(sa, sa, msk);
      pc->div_255_u32(sa, sa);
      pc->inv_u8(sa, sa);
      pc->mul(da, da, sa);
      pc->div_255_u32(da, da);
    }

    out.sa = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - Xor
  // ----------------

  if (isXor()) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa) + Sa.(1 - Da)
      srcFetch(s, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->inv_u8(y, sa);
      pc->inv_u8(x, da);

      pc->mul(da, da, y);
      pc->mul(sa, sa, x);
      pc->add(da, da, sa);
      pc->div_255_u32(da, da);
    }
    else {
      // Da' = Da.(1 - Sa.m) + Sa.m.(1 - Da)
      srcFetch(s, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(sa, sa, msk);
      pc->div_255_u32(sa, sa);

      pc->inv_u8(y, sa);
      pc->inv_u8(x, da);

      pc->mul(da, da, y);
      pc->mul(sa, sa, x);
      pc->add(da, da, sa);
      pc->div_255_u32(da, da);
    }

    out.sa = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - Plus
  // -----------------

  if (isPlus()) {
    // Da' = Clamp(Da + Sa)
    // Da' = Clamp(Da + Sa.m)
    if (hasMask) {
      srcFetch(s, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());

      pc->mul(sa, sa, msk);
      pc->div_255_u32(sa, sa);
    }
    else {
      srcFetch(s, PixelCount(1), PixelFlags::kSA | PixelFlags::kImmutable, pc->emptyPredicate());
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
    }

    pc->adds_u8(da, da, sa);

    out.sa = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - Invert
  // -------------------

  if (isAlphaInv()) {
    // Da' = 1 - Da
    // Da' = Da.(1 - m) + (1 - Da).m
    if (hasMask) {
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      pc->inv_u8(x, msk);
      pc->mul(x, x, da);
      pc->inv_u8(da, da);
      pc->mul(da, da, msk);
      pc->add(da, da, x);
      pc->div_255_u32(da, da);
    }
    else {
      dstFetch(d, PixelCount(1), PixelFlags::kSA, pc->emptyPredicate());
      pc->inv_u8(da, da);
    }

    out.sa = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - Invalid
  // --------------------

  BL_NOT_REACHED();
}

// bl::Pipeline::JIT::CompOpPart - VMask - Proc - A8 (Vec)
// =======================================================

void CompOpPart::vMaskProcA8Vec(Pixel& out, PixelCount n, PixelFlags flags, const VecArray& vm_, PixelCoverageFlags coverageFlags, PixelPredicate& predicate) noexcept {
  VecWidth vw = pc->vecWidthOf(DataWidth::k16, n);
  uint32_t kFullN = pc->vecCountOf(DataWidth::k16, n);

  VecArray vm = vm_.cloneAs(vw);
  bool hasMask = !vm.empty();

  Pixel d("d", PixelType::kA8);
  Pixel s("s", PixelType::kA8);

  VecArray& da = d.ua;
  VecArray& sa = s.ua;

  VecArray xv, yv;
  pc->newVecArray(xv, kFullN, vw, "x");
  pc->newVecArray(yv, kFullN, vw, "y");

  out.setCount(n);

  // VMask - A8 - SrcCopy
  // --------------------

  if (isSrcCopy()) {
    if (!hasMask) {
      // Da' = Sa
      srcFetch(out, n, flags, predicate);
    }
    else {
#if defined(BL_JIT_ARCH_A64)
      srcFetch(s, n, PixelFlags::kPA | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kPA, predicate);

      VecArray& vs = s.pa;
      VecArray& vd = d.pa;
      VecArray vn;

      CompOpUtils::mul_u8_widen(pc, xv, vs, vm, n.value());
      vMaskProcRGBA32InvertMask(vn, vm, coverageFlags);

      CompOpUtils::madd_u8_widen(pc, xv, vd, vn, n.value());
      vMaskProcRGBA32InvertDone(vn, vm, coverageFlags);

      CompOpUtils::combineDiv255AndOutA8(pc, out, flags, xv);
#else
      // Da' = Sa.m + Da.(1 - m)
      srcFetch(s, n, PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_inv255_u16(vm, vm);
      pc->v_mul_u16(da, da, vm);

      if (blTestFlag(coverageFlags, PixelCoverageFlags::kImmutable))
        pc->v_inv255_u16(vm, vm);

      pc->v_add_i16(da, da, sa);
      pc->v_div255_u16(da);

      out.ua = da;
#endif
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - SrcOver
  // --------------------

  if (isSrcOver()) {
#if defined(BL_JIT_ARCH_A64)
    if (!hasMask) {
      srcFetch(s, n, PixelFlags::kPA | PixelFlags::kPI | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kPA, predicate);

      CompOpUtils::mul_u8_widen(pc, xv, d.pa, s.pi, n.value());
      CompOpUtils::div255_pack(pc, d.pa, xv);
      pc->v_add_u8(d.pa, d.pa, s.pa);
      out.pa.init(d.pa);
    }
    else {
      VecArray zv;
      pc->newVecArray(zv, kFullN, vw, "z");

      srcFetch(s, n, PixelFlags::kPA | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kPA, predicate);

      VecArray xv_half = xv.half();
      VecArray yv_half = yv.half();

      CompOpUtils::mul_u8_widen(pc, xv, s.pa, vm, n.value());
      CompOpUtils::div255_pack(pc, xv_half, xv);

      pc->v_not_u32(yv_half, xv_half);

      CompOpUtils::mul_u8_widen(pc, zv, d.pa, yv_half, n.value());
      CompOpUtils::div255_pack(pc, d.pa, zv);

      pc->v_add_u8(d.pa, d.pa, xv_half);
      out.pa.init(d.pa);
    }
#else
    if (!hasMask) {
      // Da' = Sa + Da.(1 - Sa)
      srcFetch(s, n, PixelFlags::kUA | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_inv255_u16(xv, sa);
      pc->v_mul_u16(da, da, xv);
      pc->v_div255_u16(da);
      pc->v_add_i16(da, da, sa);
      out.ua = da;
    }
    else {
      // Da' = Sa.m + Da.(1 - Sa.m)
      srcFetch(s, n, PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_div255_u16(sa);
      pc->v_inv255_u16(xv, sa);
      pc->v_mul_u16(da, da, xv);
      pc->v_div255_u16(da);
      pc->v_add_i16(da, da, sa);
      out.ua = da;
    }
#endif

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - SrcIn
  // ------------------

  if (isSrcIn()) {
    if (!hasMask) {
      // Da' = Sa.Da
      srcFetch(s, n, PixelFlags::kUA | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_mul_u16(da, da, sa);
      pc->v_div255_u16(da);
    }
    else {
      // Da' = Da.(Sa.m) + Da.(1 - m)
      //     = Da.(Sa.m + 1 - m)
      srcFetch(s, n, PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_div255_u16(sa);
      pc->v_add_i16(sa, sa, pc->simdConst(&ct.i_00FF00FF00FF00FF, Bcst::kNA, sa));
      pc->v_sub_i16(sa, sa, vm);
      pc->v_mul_u16(da, da, sa);
      pc->v_div255_u16(da);
    }

    out.ua = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - SrcOut
  // -------------------

  if (isSrcOut()) {
    if (!hasMask) {
      // Da' = Sa.(1 - Da)
      srcFetch(s, n, PixelFlags::kUA | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_inv255_u16(da, da);
      pc->v_mul_u16(da, da, sa);
      pc->v_div255_u16(da);
    }
    else {
      // Da' = Sa.m.(1 - Da) + Da.(1 - m)
      srcFetch(s, n, PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_div255_u16(sa);

      pc->v_inv255_u16(xv, da);
      pc->v_inv255_u16(vm, vm);
      pc->v_mul_u16(sa, sa, xv);
      pc->v_mul_u16(da, da, vm);

      if (blTestFlag(coverageFlags, PixelCoverageFlags::kImmutable))
        pc->v_inv255_u16(vm, vm);

      pc->v_add_i16(da, da, sa);
      pc->v_div255_u16(da);
    }

    out.ua = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - DstOut
  // -------------------

  if (isDstOut()) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa)
      srcFetch(s, n, PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_inv255_u16(sa, sa);
      pc->v_mul_u16(da, da, sa);
      pc->v_div255_u16(da);
    }
    else {
      // Da' = Da.(1 - Sa.m)
      srcFetch(s, n, PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_mul_u16(sa, sa, vm);
      pc->v_div255_u16(sa);
      pc->v_inv255_u16(sa, sa);
      pc->v_mul_u16(da, da, sa);
      pc->v_div255_u16(da);
    }

    out.ua = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - Xor
  // ----------------

  if (isXor()) {
    if (!hasMask) {
      // Da' = Da.(1 - Sa) + Sa.(1 - Da)
      srcFetch(s, n, PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_inv255_u16(yv, sa);
      pc->v_inv255_u16(xv, da);

      pc->v_mul_u16(da, da, yv);
      pc->v_mul_u16(sa, sa, xv);
      pc->v_add_i16(da, da, sa);
      pc->v_div255_u16(da);
    }
    else {
      // Da' = Da.(1 - Sa.m) + Sa.m.(1 - Da)
      srcFetch(s, n, PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

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
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - Plus
  // -----------------

  if (isPlus()) {
    if (!hasMask) {
      // Da' = Clamp(Da + Sa)
      srcFetch(s, n, PixelFlags::kPA | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kPA, predicate);

      pc->v_adds_u8(d.pa, d.pa, s.pa);
      out.pa = d.pa;
    }
    else {
      // Da' = Clamp(Da + Sa.m)
      srcFetch(s, n, PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      pc->v_mul_u16(s.ua, s.ua, vm);
      pc->v_div255_u16(s.ua);
      pc->v_adds_u8(d.ua, d.ua, s.ua);
      out.ua = d.ua;
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - Invert
  // -------------------

  if (isAlphaInv()) {
    if (!hasMask) {
      // Da' = 1 - Da
      dstFetch(d, n, PixelFlags::kUA, predicate);
      pc->v_inv255_u16(da, da);
    }
    else {
      // Da' = Da.(1 - m) + (1 - Da).m
      dstFetch(d, n, PixelFlags::kUA, predicate);
      pc->v_inv255_u16(xv, vm);
      pc->v_mul_u16(xv, xv, da);
      pc->v_inv255_u16(da, da);
      pc->v_mul_u16(da, da, vm);
      pc->v_add_i16(da, da, xv);
      pc->v_div255_u16(da);
    }

    out.ua = da;
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMask - A8 - Invalid
  // --------------------

  BL_NOT_REACHED();
}

// bl::Pipeline::JIT::CompOpPart - CMask - Init & Fini - RGBA
// ==========================================================

void CompOpPart::cMaskInitRGBA32(const Vec& vm) noexcept {
  bool hasMask = vm.isValid();
  bool useDa = hasDa();

  if (srcPart()->isSolid()) {
    Pixel& s = srcPart()->as<FetchSolidPart>()->_pixel;
    SolidPixel& o = _solidOpt;

    // CMaskInit - RGBA32 - Solid - SrcCopy
    // ------------------------------------

    if (isSrcCopy()) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPC);

        o.px = s.pc[0];
      }
      else {
#if defined(BL_JIT_ARCH_A64)
        // Xca = (Sca * m)
        // Xa  = (Sa  * m)
        // Im  = (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPC);

        o.ux = pc->newSimilarReg(s.pc[0], "solid.ux");
        o.vn = vm;

        pc->v_mulw_lo_u8(o.ux, s.pc[0], vm);
        pc->v_not_u32(o.vn, vm);
#else
        // Xca = (Sca * m) + 0.5 <Rounding>
        // Xa  = (Sa  * m) + 0.5 <Rounding>
        // Im  = (1 - m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        o.vn = vm;

        pc->v_mul_u16(o.ux, s.uc[0], o.vn);
        pc->v_add_i16(o.ux, o.ux, pc->simdConst(&ct.i_0080008000800080, Bcst::kNA, o.ux));
        pc->v_inv255_u16(o.vn, o.vn);
#endif
      }
    }

    // CMaskInit - RGBA32 - Solid - SrcOver
    // ------------------------------------

    else if (isSrcOver()) {
#if defined(BL_JIT_ARCH_A64)
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = 1 - Sa
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPC | PixelFlags::kPI | PixelFlags::kImmutable);

        o.px = s.pc[0];
        o.py = s.pi[0];
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = 1 - (Sa * m)
        // Ya  = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPC | PixelFlags::kImmutable);

        o.px = pc->newSimilarReg(s.pc[0], "solid.px");
        o.py = pc->newSimilarReg(s.pc[0], "solid.py");

        pc->v_mulw_lo_u8(o.px, s.pc[0], vm);
        CompOpUtils::div255_pack(pc, o.px, o.px);
        pc->v_swizzle_u32x4(o.px, o.px, swizzle(0, 0, 0, 0));

        pc->v_not_u32(o.py, o.px);
        pc->v_swizzlev_u8(o.py, o.py, pc->simdVecConst(&pc->ct.swizu8_3xxx2xxx1xxx0xxx_to_3333222211110000, Bcst::kNA, o.py));
      }
#else
      if (!hasMask) {
        // Xca = Sca * 1 + 0.5 <Rounding>
        // Xa  = Sa  * 1 + 0.5 <Rounding>
        // Yca = 1 - Sa
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUI | PixelFlags::kImmutable);

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        o.uy = s.ui[0];

        pc->v_slli_i16(o.ux, s.uc[0], 8);
        pc->v_sub_i16(o.ux, o.ux, s.uc[0]);
        pc->v_add_i16(o.ux, o.ux, pc->simdConst(&ct.i_0080008000800080, Bcst::kNA, o.ux));
      }
      else {
        // Xca = Sca * m + 0.5 <Rounding>
        // Xa  = Sa  * m + 0.5 <Rounding>
        // Yca = 1 - (Sa * m)
        // Ya  = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kImmutable);

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        o.uy = pc->newSimilarReg(s.uc[0], "solid.uy");

        pc->v_mul_u16(o.uy, s.uc[0], vm);
        pc->v_div255_u16(o.uy);

        pc->v_slli_i16(o.ux, o.uy, 8);
        pc->v_sub_i16(o.ux, o.ux, o.uy);
        pc->v_add_i16(o.ux, o.ux, pc->simdConst(&ct.i_0080008000800080, Bcst::kNA, o.ux));

        pc->v_expand_alpha_16(o.uy, o.uy);
        pc->v_inv255_u16(o.uy, o.uy);
      }
#endif
    }

    // CMaskInit - RGBA32 - Solid - SrcIn | SrcOut
    // -------------------------------------------

    else if (isSrcIn() || isSrcOut()) {
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

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        o.vn = vm;

        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_div255_u16(o.ux);
        pc->v_inv255_u16(vm, vm);
      }
    }

    // CMaskInit - RGBA32 - Solid - SrcAtop & Xor & Darken & Lighten
    // -------------------------------------------------------------

    else if (isSrcAtop() || isXor() || isDarken() || isLighten()) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = 1 - Sa
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUI);

        o.ux = s.uc[0];
        o.uy = s.ui[0];
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = 1 - (Sa * m)
        // Ya  = 1 - (Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        o.uy = vm;

        pc->v_mul_u16(o.ux, s.uc[0], o.uy);
        pc->v_div255_u16(o.ux);

        pc->v_expand_alpha_16(o.uy, o.ux, false);
        pc->v_swizzle_u32x4(o.uy, o.uy, swizzle(0, 0, 0, 0));
        pc->v_inv255_u16(o.uy, o.uy);
      }
    }

    // CMaskInit - RGBA32 - Solid - Dst
    // --------------------------------

    else if (isDstCopy()) {
      BL_NOT_REACHED();
    }

    // CMaskInit - RGBA32 - Solid - DstOver
    // ------------------------------------

    else if (isDstOver()) {
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

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_div255_u16(o.ux);
      }
    }

    // CMaskInit - RGBA32 - Solid - DstIn
    // ----------------------------------

    else if (isDstIn()) {
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

        o.ux = pc->newSimilarReg(s.ua[0], "solid.ux");
        pc->v_mov(o.ux, s.ua[0]);
        pc->v_inv255_u16(o.ux, o.ux);
        pc->v_mul_u16(o.ux, o.ux, vm);
        pc->v_div255_u16(o.ux);
        pc->v_inv255_u16(o.ux, o.ux);
      }
    }

    // CMaskInit - RGBA32 - Solid - DstOut
    // -----------------------------------

    else if (isDstOut()) {
      if (!hasMask) {
        // Xca = 1 - Sa
        // Xa  = 1 - Sa
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUI);

          o.ux = s.ui[0];
        }
        // Xca = 1 - Sa
        // Xa  = 1
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUA);

          o.ux = pc->newSimilarReg(s.ua[0], "solid.ux");
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

    else if (isDstAtop()) {
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

        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        o.uy = pc->newSimilarReg(s.uc[0], "solid.uy");
        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_inv255_u16(o.uy, vm);
        pc->v_div255_u16(o.ux);
        pc->v_add_i16(o.uy, o.uy, o.ux);
        pc->v_expand_alpha_16(o.uy, o.uy);
      }
    }

    // CMaskInit - RGBA32 - Solid - Plus
    // ---------------------------------

    else if (isPlus()) {
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

        o.px = pc->newSimilarReg(s.pc[0], "solid.px");
        pc->v_mul_u16(o.px, s.uc[0], vm);
        pc->v_div255_u16(o.px);
        pc->v_packs_i16_u8(o.px, o.px, o.px);
      }
    }

    // CMaskInit - RGBA32 - Solid - Minus
    // ----------------------------------

    else if (isMinus()) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = 0
        // Yca = Sca
        // Ya  = Sa
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

          o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
          o.uy = s.uc[0];
          pc->v_mov(o.ux, o.uy);
          pc->vZeroAlphaW(o.ux, o.ux);
        }
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPC);

          o.px = pc->newSimilarReg(s.pc[0], "solid.px");
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
        // N   = 1 - m   <Alpha channel is set to 0  >
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

          o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
          o.uy = pc->newSimilarReg(s.uc[0], "solid.uy");
          o.vm = vm;
          o.vn = pc->newSimilarReg(s.uc[0], "vn");

          pc->vZeroAlphaW(o.ux, s.uc[0]);
          pc->v_mov(o.uy, s.uc[0]);

          pc->v_inv255_u16(o.vn, o.vm);
          pc->vZeroAlphaW(o.vm, o.vm);
          pc->vZeroAlphaW(o.vn, o.vn);
          pc->vFillAlpha255W(o.vm, o.vm);
        }
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

          o.ux = pc->newSimilarReg(s.uc[0], "ux");
          o.vm = vm;
          o.vn = pc->newSimilarReg(s.uc[0], "vn");
          pc->vZeroAlphaW(o.ux, s.uc[0]);
          pc->v_inv255_u16(o.vn, o.vm);
        }
      }
    }

    // CMaskInit - RGBA32 - Solid - Modulate
    // -------------------------------------

    else if (isModulate()) {
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

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_div255_u16(o.ux);
        pc->v_add_i16(o.ux, o.ux, pc->simdConst(&ct.i_00FF00FF00FF00FF, Bcst::kNA, o.ux));
        pc->v_sub_i16(o.ux, o.ux, vm);
      }
    }

    // CMaskInit - RGBA32 - Solid - Multiply
    // -------------------------------------

    else if (isMultiply()) {
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = Sca + (1 - Sa)
        // Ya  = Sa  + (1 - Sa)
        if (useDa) {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUI);

          o.ux = s.uc[0];
          o.uy = pc->newSimilarReg(s.uc[0], "solid.uy");

          pc->v_mov(o.uy, s.ui[0]);
          pc->v_add_i16(o.uy, o.uy, o.ux);
        }
        // Yca = Sca + (1 - Sa)
        // Ya  = Sa  + (1 - Sa)
        else {
          srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC | PixelFlags::kUI);

          o.uy = pc->newSimilarReg(s.uc[0], "solid.uy");
          pc->v_mov(o.uy, s.ui[0]);
          pc->v_add_i16(o.uy, o.uy, s.uc[0]);
        }
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = Sca * m + (1 - Sa * m)
        // Ya  = Sa  * m + (1 - Sa * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        o.uy = pc->newSimilarReg(s.uc[0], "solid.uy");

        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_div255_u16(o.ux);
        pc->v_swizzle_lo_u16x4(o.uy, o.ux, swizzle(3, 3, 3, 3));
        pc->v_inv255_u16(o.uy, o.uy);
        pc->v_swizzle_u32x4(o.uy, o.uy, swizzle(0, 0, 0, 0));
        pc->v_add_i16(o.uy, o.uy, o.ux);
      }
    }

    // CMaskInit - RGBA32 - Solid - Screen
    // -----------------------------------

    else if (isScreen()) {
#if defined(BL_JIT_ARCH_A64)
      if (!hasMask) {
        // Xca = Sca
        // Xa  = Sa
        // Yca = 1 - Sca
        // Ya  = 1 - Sa
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPC | PixelFlags::kImmutable);

        o.px = s.pc[0];
        o.py = pc->newSimilarReg(s.pc[0], "solid.py");

        pc->v_not_u32(o.py, o.px);
      }
      else {
        // Xca = Sca * m
        // Xa  = Sa  * m
        // Yca = 1 - (Sca * m)
        // Ya  = 1 - (Sa  * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kPC | PixelFlags::kImmutable);

        o.px = pc->newSimilarReg(s.pc[0], "solid.px");
        o.py = pc->newSimilarReg(s.pc[0], "solid.py");

        pc->v_mulw_lo_u8(o.px, s.pc[0], vm);
        CompOpUtils::div255_pack(pc, o.px, o.px);
        pc->v_swizzle_u32x4(o.px, o.px, swizzle(0, 0, 0, 0));

        pc->v_not_u32(o.py, o.px);
      }
#else
      if (!hasMask) {
        // Xca = Sca * 1 + 0.5 <Rounding>
        // Xa  = Sa  * 1 + 0.5 <Rounding>
        // Yca = 1 - Sca
        // Ya  = 1 - Sa

        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        o.uy = pc->newSimilarReg(s.uc[0], "solid.uy");

        pc->v_inv255_u16(o.uy, o.ux);
        pc->v_slli_i16(o.ux, s.uc[0], 8);
        pc->v_sub_i16(o.ux, o.ux, s.uc[0]);
        pc->v_add_i16(o.ux, o.ux, pc->simdConst(&ct.i_0080008000800080, Bcst::kNA, o.ux));
      }
      else {
        // Xca = Sca * m + 0.5 <Rounding>
        // Xa  = Sa  * m + 0.5 <Rounding>
        // Yca = 1 - (Sca * m)
        // Ya  = 1 - (Sa  * m)
        srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

        o.ux = pc->newSimilarReg(s.uc[0], "solid.ux");
        o.uy = pc->newSimilarReg(s.uc[0], "solid.uy");

        pc->v_mul_u16(o.uy, s.uc[0], vm);
        pc->v_div255_u16(o.uy);
        pc->v_slli_i16(o.ux, o.uy, 8);
        pc->v_sub_i16(o.ux, o.ux, o.uy);
        pc->v_add_i16(o.ux, o.ux, pc->simdConst(&ct.i_0080008000800080, Bcst::kNA, o.ux));
        pc->v_inv255_u16(o.uy, o.uy);
      }
#endif
    }

    // CMaskInit - RGBA32 - Solid - LinearBurn & Difference & Exclusion
    // ----------------------------------------------------------------

    else if (isLinearBurn() || isDifference() || isExclusion()) {
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

        o.ux = pc->newSimilarReg(s.uc[0], "ux");
        o.uy = pc->newSimilarReg(s.uc[0], "uy");

        pc->v_mul_u16(o.ux, s.uc[0], vm);
        pc->v_div255_u16(o.ux);
        pc->v_swizzle_lo_u16x4(o.uy, o.ux, swizzle(3, 3, 3, 3));
        pc->v_swizzle_u32x4(o.uy, o.uy, swizzle(0, 0, 0, 0));
      }
    }

    // CMaskInit - RGBA32 - Solid - TypeA (Non-Opaque)
    // -----------------------------------------------

    else if (blTestFlag(compOpFlags(), CompOpFlags::kTypeA) && hasMask) {
      // Multiply the source pixel with the mask if `TypeA`.
      srcPart()->as<FetchSolidPart>()->initSolidFlags(PixelFlags::kUC);

      Pixel& pre = _solidPre;
      pre.setCount(PixelCount(1));
      pre.uc.init(pc->newSimilarReg(s.uc[0], "pre.uc"));

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

    if (isSrcCopy()) {
      if (hasMask) {
        _mask->vn = pc->newSimilarReg(vm, "vn");
        if (coverageFormat() == PixelCoverageFormat::kPacked)
          pc->v_not_u32(_mask->vn, vm);
        else
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
    // TODO: [JIT] ???
  }

  _mask->reset();
  _cMaskLoopFini();
}

// bl::Pipeline::JIT::CompOpPart - CMask - Proc - RGBA
// ===================================================

static VecArray x_pack_pixels(PipeCompiler* pc, VecArray& src, PixelCount n, const char* name) noexcept {
  VecArray out;

#if defined(BL_JIT_ARCH_X86)
  if (!pc->hasAVX()) {
    out = src.even();
    pc->x_packs_i16_u8(out, out, src.odd());
  }
  else
#endif // BL_JIT_ARCH_X86
  {
    FetchUtils::_x_pack_pixel(pc, out, src, n.value() * 4, "", name);
  }

  return out;
}

void CompOpPart::cMaskProcRGBA32Vec(Pixel& out, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  bool hasMask = isLoopCMask();

  VecWidth vw = pc->vecWidthOf(DataWidth::k64, n);
  uint32_t kFullN = pc->vecCountOf(DataWidth::k64, n);
  uint32_t kUseHi = n > 1;

  out.setCount(n);

  if (srcPart()->isSolid()) {
    Pixel d("d", pixelType());
    SolidPixel& o = _solidOpt;

    VecArray xv, yv, zv;
    pc->newVecArray(xv, kFullN, vw, "x");
    pc->newVecArray(yv, kFullN, vw, "y");
    pc->newVecArray(zv, kFullN, vw, "z");

    bool useDa = hasDa();

    // CMaskProc - RGBA32 - SrcCopy
    // ----------------------------

    if (isSrcCopy()) {
      if (!hasMask) {
        // Dca' = Xca
        // Da'  = Xa
        out.pc = VecArray(o.px).cloneAs(vw);
        out.makeImmutable();
      }
      else {
#if defined(BL_JIT_ARCH_A64)
        dstFetch(d, n, PixelFlags::kPC, predicate);

        CompOpUtils::mul_u8_widen(pc, xv, d.pc, o.vn, n.value() * 4);
        pc->v_add_u16(xv, xv, o.ux);
        CompOpUtils::combineDiv255AndOutRGBA32(pc, out, flags, xv);
#else
        // Dca' = Xca + Dca.(1 - m)
        // Da'  = Xa  + Da .(1 - m)
        dstFetch(d, n, PixelFlags::kUC, predicate);
        VecArray& dv = d.uc;

        Vec s_ux = o.ux.cloneAs(dv[0]);
        Vec s_vn = o.vn.cloneAs(dv[0]);

        pc->v_mul_u16(dv, dv, s_vn);
        pc->v_add_i16(dv, dv, s_ux);
        pc->v_mul257_hi_u16(dv, dv);
        out.uc.init(dv);
#endif
      }

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - SrcOver & Screen
    // -------------------------------------

    if (isSrcOver() || isScreen()) {
#if defined(BL_JIT_ARCH_A64)
      dstFetch(d, n, PixelFlags::kPC, predicate);

      CompOpUtils::mul_u8_widen(pc, xv, d.pc, o.py, n.value() * 4u);
      CompOpUtils::div255_pack(pc, d.pc, xv);
      pc->v_add_u8(d.pc, d.pc, o.px);
      out.pc.init(d.pc);
#else
      // Dca' = Xca + Dca.Yca
      // Da'  = Xa  + Da .Ya
      dstFetch(d, n, PixelFlags::kUC, predicate);
      VecArray& dv = d.uc;

      Vec s_ux = o.ux.cloneAs(dv[0]);
      Vec s_uy = o.uy.cloneAs(dv[0]);

      pc->v_mul_u16(dv, dv, s_uy);
      pc->v_add_i16(dv, dv, s_ux);
      pc->v_mul257_hi_u16(dv, dv);

      out.uc.init(dv);
#endif

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - SrcIn
    // --------------------------

    if (isSrcIn()) {
      // Dca' = Xca.Da
      // Da'  = Xa .Da
      if (!hasMask) {
        dstFetch(d, n, PixelFlags::kUA, predicate);
        VecArray& dv = d.ua;

        Vec s_ux = o.ux.cloneAs(dv[0]);

        pc->v_mul_u16(dv, dv, s_ux);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Xca.Da + Dca.(1 - m)
      // Da'  = Xa .Da + Da .(1 - m)
      else {
        dstFetch(d, n, PixelFlags::kUC | PixelFlags::kUA, predicate);
        VecArray& dv = d.uc;
        VecArray& da = d.ua;

        Vec s_ux = o.ux.cloneAs(dv[0]);
        Vec s_vn = o.vn.cloneAs(dv[0]);

        pc->v_mul_u16(dv, dv, s_vn);
        pc->v_madd_u16(dv, da, s_ux, dv);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - SrcOut
    // ---------------------------

    if (isSrcOut()) {
      // Dca' = Xca.(1 - Da)
      // Da'  = Xa .(1 - Da)
      if (!hasMask) {
        dstFetch(d, n, PixelFlags::kUI, predicate);
        VecArray& dv = d.ui;

        Vec s_ux = o.ux.cloneAs(dv[0]);

        pc->v_mul_u16(dv, dv, s_ux);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Xca.(1 - Da) + Dca.(1 - m)
      // Da'  = Xa .(1 - Da) + Da .(1 - m)
      else {
        dstFetch(d, n, PixelFlags::kUC, predicate);
        VecArray& dv = d.uc;

        Vec s_ux = o.ux.cloneAs(dv[0]);
        Vec s_vn = o.vn.cloneAs(dv[0]);

        pc->v_expand_alpha_16(xv, dv, kUseHi);
        pc->v_inv255_u16(xv, xv);
        pc->v_mul_u16(xv, xv, s_ux);
        pc->v_mul_u16(dv, dv, s_vn);
        pc->v_add_i16(dv, dv, xv);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - SrcAtop
    // ----------------------------

    if (isSrcAtop()) {
      // Dca' = Xca.Da + Dca.Yca
      // Da'  = Xa .Da + Da .Ya
      dstFetch(d, n, PixelFlags::kUC, predicate);
      VecArray& dv = d.uc;

      Vec s_ux = o.ux.cloneAs(dv[0]);
      Vec s_uy = o.uy.cloneAs(dv[0]);

      pc->v_expand_alpha_16(xv, dv, kUseHi);
      pc->v_mul_u16(dv, dv, s_uy);
      pc->v_mul_u16(xv, xv, s_ux);

      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);

      out.uc.init(dv);
      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Dst
    // ------------------------

    if (isDstCopy()) {
      // Dca' = Dca
      // Da'  = Da
      BL_NOT_REACHED();
    }

    // CMaskProc - RGBA32 - DstOver
    // ----------------------------

    if (isDstOver()) {
      // Dca' = Xca.(1 - Da) + Dca
      // Da'  = Xa .(1 - Da) + Da
      dstFetch(d, n, PixelFlags::kPC | PixelFlags::kUI, predicate);
      VecArray& dv = d.ui;

      Vec s_ux = o.ux.cloneAs(dv[0]);

      pc->v_mul_u16(dv, dv, s_ux);
      pc->v_div255_u16(dv);

      VecArray dh = x_pack_pixels(pc, dv, n, "d");
      dh = dh.cloneAs(d.pc[0]);
      pc->v_add_i32(dh, dh, d.pc);

      out.pc.init(dh);
      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - DstIn & DstOut
    // -----------------------------------

    if (isDstIn() || isDstOut()) {
      // Dca' = Xca.Dca
      // Da'  = Xa .Da
      dstFetch(d, n, PixelFlags::kUC, predicate);
      VecArray& dv = d.uc;

      Vec s_ux = o.ux.cloneAs(dv[0]);

      pc->v_mul_u16(dv, dv, s_ux);
      pc->v_div255_u16(dv);

      out.uc.init(dv);
      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - DstAtop | Xor | Multiply
    // ---------------------------------------------

    if (isDstAtop() || isXor() || isMultiply()) {
      if (useDa) {
        // Dca' = Xca.(1 - Da) + Dca.Yca
        // Da'  = Xa .(1 - Da) + Da .Ya
        dstFetch(d, n, PixelFlags::kUC, predicate);
        VecArray& dv = d.uc;

        Vec s_ux = o.ux.cloneAs(dv[0]);
        Vec s_uy = o.uy.cloneAs(dv[0]);

        pc->v_expand_alpha_16(xv, dv, kUseHi);
        pc->v_mul_u16(dv, dv, s_uy);
        pc->v_inv255_u16(xv, xv);
        pc->v_mul_u16(xv, xv, s_ux);

        pc->v_add_i16(dv, dv, xv);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      else {
        // Dca' = Dca.Yca
        // Da'  = Da .Ya
        dstFetch(d, n, PixelFlags::kUC, predicate);
        VecArray& dv = d.uc;

        Vec s_uy = o.uy.cloneAs(dv[0]);

        pc->v_mul_u16(dv, dv, s_uy);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Plus
    // -------------------------

    if (isPlus()) {
      // Dca' = Clamp(Dca + Sca)
      // Da'  = Clamp(Da  + Sa )
      dstFetch(d, n, PixelFlags::kPC, predicate);
      VecArray& dv = d.pc;

      Vec s_px = o.px.cloneAs(dv[0]);

      pc->v_adds_u8(dv, dv, s_px);

      out.pc.init(dv);
      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Minus
    // --------------------------

    if (isMinus()) {
      if (!hasMask) {
        if (useDa) {
          // Dca' = Clamp(Dca - Xca) + Yca.(1 - Da)
          // Da'  = Da + Ya.(1 - Da)
          dstFetch(d, n, PixelFlags::kUC, predicate);
          VecArray& dv = d.uc;

          Vec s_ux = o.ux.cloneAs(dv[0]);
          Vec s_uy = o.uy.cloneAs(dv[0]);

          pc->v_expand_alpha_16(xv, dv, kUseHi);
          pc->v_inv255_u16(xv, xv);
          pc->v_mul_u16(xv, xv, s_uy);
          pc->v_subs_u16(dv, dv, s_ux);
          pc->v_div255_u16(xv);

          pc->v_add_i16(dv, dv, xv);
          out.uc.init(dv);
        }
        else {
          // Dca' = Clamp(Dca - Xca)
          // Da'  = <unchanged>
          dstFetch(d, n, PixelFlags::kPC, predicate);
          VecArray& dh = d.pc;

          Vec s_px = o.px.cloneAs(dh[0]);

          pc->v_subs_u8(dh, dh, s_px);
          out.pc.init(dh);
        }
      }
      else {
        if (useDa) {
          // Dca' = (Clamp(Dca - Xca) + Yca.(1 - Da)).m + Dca.(1 - m)
          // Da'  = Da + Ya.(1 - Da)
          dstFetch(d, n, PixelFlags::kUC, predicate);
          VecArray& dv = d.uc;

          Vec s_ux = o.ux.cloneAs(dv[0]);
          Vec s_uy = o.uy.cloneAs(dv[0]);
          Vec s_vn = o.vn.cloneAs(dv[0]);
          Vec s_vm = o.vm.cloneAs(dv[0]);

          pc->v_expand_alpha_16(xv, dv, kUseHi);
          pc->v_inv255_u16(xv, xv);
          pc->v_mul_u16(yv, dv, s_vn);
          pc->v_subs_u16(dv, dv, s_ux);
          pc->v_mul_u16(xv, xv, s_uy);
          pc->v_div255_u16(xv);
          pc->v_add_i16(dv, dv, xv);
          pc->v_mul_u16(dv, dv, s_vm);

          pc->v_add_i16(dv, dv, yv);
          pc->v_div255_u16(dv);
          out.uc.init(dv);
        }
        else {
          // Dca' = Clamp(Dca - Xca).m + Dca.(1 - m)
          // Da'  = <unchanged>
          dstFetch(d, n, PixelFlags::kUC, predicate);
          VecArray& dv = d.uc;

          Vec s_ux = o.ux.cloneAs(dv[0]);
          Vec s_vn = o.vn.cloneAs(dv[0]);
          Vec s_vm = o.vm.cloneAs(dv[0]);

          pc->v_mul_u16(yv, dv, s_vn);
          pc->v_subs_u16(dv, dv, s_ux);
          pc->v_mul_u16(dv, dv, s_vm);

          pc->v_add_i16(dv, dv, yv);
          pc->v_div255_u16(dv);
          out.uc.init(dv);
        }
      }

      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Modulate
    // -----------------------------

    if (isModulate()) {
      dstFetch(d, n, PixelFlags::kUC, predicate);
      VecArray& dv = d.uc;

      Vec s_ux = o.ux.cloneAs(dv[0]);

      // Dca' = Dca.Xca
      // Da'  = Da .Xa
      pc->v_mul_u16(dv, dv, s_ux);
      pc->v_div255_u16(dv);

      if (!useDa)
        pc->vFillAlpha255W(dv, dv);

      out.uc.init(dv);
      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Darken & Lighten
    // -------------------------------------

    if (isDarken() || isLighten()) {
      // Dca' = minmax(Dca + Xca.(1 - Da), Xca + Dca.Yca)
      // Da'  = Xa + Da.Ya
      dstFetch(d, n, PixelFlags::kUC, predicate);
      VecArray& dv = d.uc;

      Vec s_ux = o.ux.cloneAs(dv[0]);
      Vec s_uy = o.uy.cloneAs(dv[0]);

      pc->v_expand_alpha_16(xv, dv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_mul_u16(xv, xv, s_ux);
      pc->v_div255_u16(xv);
      pc->v_add_i16(xv, xv, dv);
      pc->v_mul_u16(dv, dv, s_uy);
      pc->v_div255_u16(dv);
      pc->v_add_i16(dv, dv, s_ux);

      if (isDarken())
        pc->v_min_u8(dv, dv, xv);
      else
        pc->v_max_u8(dv, dv, xv);

      out.uc.init(dv);
      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - LinearBurn
    // -------------------------------

    if (isLinearBurn()) {
      // Dca' = Dca + Xca - Yca.Da
      // Da'  = Da  + Xa  - Ya .Da
      dstFetch(d, n, PixelFlags::kUC, predicate);
      VecArray& dv = d.uc;

      Vec s_ux = o.ux.cloneAs(dv[0]);
      Vec s_uy = o.uy.cloneAs(dv[0]);

      pc->v_expand_alpha_16(xv, dv, kUseHi);
      pc->v_mul_u16(xv, xv, s_uy);
      pc->v_add_i16(dv, dv, s_ux);
      pc->v_div255_u16(xv);
      pc->v_subs_u16(dv, dv, xv);

      out.uc.init(dv);
      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Difference
    // -------------------------------

    if (isDifference()) {
      // Dca' = Dca + Sca - 2.min(Sca.Da, Dca.Sa)
      // Da'  = Da  + Sa  -   min(Sa .Da, Da .Sa)
      dstFetch(d, n, PixelFlags::kUC, predicate);
      VecArray& dv = d.uc;

      Vec s_ux = o.ux.cloneAs(dv[0]);
      Vec s_uy = o.uy.cloneAs(dv[0]);

      pc->v_expand_alpha_16(xv, dv, kUseHi);
      pc->v_mul_u16(yv, s_uy, dv);
      pc->v_mul_u16(xv, xv, s_ux);
      pc->v_add_i16(dv, dv, s_ux);
      pc->v_min_u16(yv, yv, xv);
      pc->v_div255_u16(yv);
      pc->v_sub_i16(dv, dv, yv);
      pc->vZeroAlphaW(yv, yv);
      pc->v_sub_i16(dv, dv, yv);

      out.uc.init(dv);
      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }

    // CMaskProc - RGBA32 - Exclusion
    // ------------------------------

    if (isExclusion()) {
      // Dca' = Dca + Xca - 2.Xca.Dca
      // Da'  = Da + Xa - Xa.Da
      dstFetch(d, n, PixelFlags::kUC, predicate);
      VecArray& dv = d.uc;

      Vec s_ux = o.ux.cloneAs(dv[0]);

      pc->v_mul_u16(xv, dv, s_ux);
      pc->v_add_i16(dv, dv, s_ux);
      pc->v_div255_u16(xv);
      pc->v_sub_i16(dv, dv, xv);
      pc->vZeroAlphaW(xv, xv);
      pc->v_sub_i16(dv, dv, xv);

      out.uc.init(dv);
      FetchUtils::satisfyPixels(pc, out, flags);
      return;
    }
  }

  VecArray vm;
  if (_mask->vm.isValid()) {
    vm.init(_mask->vm);
  }

  vMaskProcRGBA32Vec(out, n, flags, vm, PixelCoverageFlags::kImmutable, predicate);
}

// bl::Pipeline::JIT::CompOpPart - VMask - RGBA32 (Vec)
// ====================================================

void CompOpPart::vMaskProcRGBA32Vec(Pixel& out, PixelCount n, PixelFlags flags, const VecArray& vm_, PixelCoverageFlags coverageFlags, PixelPredicate& predicate) noexcept {
  VecWidth vw = pc->vecWidthOf(DataWidth::k64, n);
  uint32_t kFullN = pc->vecCountOf(DataWidth::k64, n);
  uint32_t kUseHi = n > 1;
  uint32_t kSplit = kFullN == 1 ? 1 : 2;

  VecArray vm = vm_.cloneAs(vw);
  bool hasMask = !vm.empty();

  bool useDa = hasDa();
  bool useSa = hasSa() || isLoopCMask() || hasMask;

  VecArray xv, yv, zv;
  pc->newVecArray(xv, kFullN, vw, "x");
  pc->newVecArray(yv, kFullN, vw, "y");
  pc->newVecArray(zv, kFullN, vw, "z");

  Pixel d("d", PixelType::kRGBA32);
  Pixel s("s", PixelType::kRGBA32);

  out.setCount(n);

  // VMaskProc - RGBA32 - SrcCopy
  // ----------------------------

  if (isSrcCopy()) {
    // Composition:
    //   Da - Optional.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Sca
      // Da'  = Sa
      srcFetch(out, n, flags, predicate);
    }
    else {
      // Dca' = Sca.m + Dca.(1 - m)
      // Da'  = Sa .m + Da .(1 - m)
#if defined(BL_JIT_ARCH_A64)
      srcFetch(s, n, PixelFlags::kPC | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kPC, predicate);

      VecArray& vs = s.pc;
      VecArray& vd = d.pc;
      VecArray vn;

      CompOpUtils::mul_u8_widen(pc, xv, vs, vm, n.value() * 4);
      vMaskProcRGBA32InvertMask(vn, vm, coverageFlags);

      CompOpUtils::madd_u8_widen(pc, xv, vd, vn, n.value() * 4);
      vMaskProcRGBA32InvertDone(vn, vm, coverageFlags);

      CompOpUtils::combineDiv255AndOutRGBA32(pc, out, flags, xv);
#else
      srcFetch(s, n, PixelFlags::kUC, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& vs = s.uc;
      VecArray& vd = d.uc;

      pc->v_mul_u16(vs, vs, vm);

      VecArray vn;
      vMaskProcRGBA32InvertMask(vn, vm, coverageFlags);

      pc->v_mul_u16(vd, vd, vn);
      pc->v_add_i16(vd, vd, vs);
      vMaskProcRGBA32InvertDone(vn, vm, coverageFlags);

      pc->v_div255_u16(vd);
      out.uc.init(vd);
#endif
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - SrcOver
  // ----------------------------

  if (isSrcOver()) {
    // Composition:
    //   Da - Optional.
    //   Sa - Required, otherwise SRC_COPY.

#if defined(BL_JIT_ARCH_A64)
    if (!hasMask) {
      srcFetch(s, n, PixelFlags::kPC | PixelFlags::kPI | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kPC, predicate);

      CompOpUtils::mul_u8_widen(pc, xv, d.pc, s.pi, n.value() * 4u);
      CompOpUtils::div255_pack(pc, d.pc, xv);
      pc->v_add_u8(d.pc, d.pc, s.pc);
      out.pc.init(d.pc);
    }
    else {
      srcFetch(s, n, PixelFlags::kPC | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kPC, predicate);

      VecArray xv_half = xv.half();
      VecArray yv_half = yv.half();

      CompOpUtils::mul_u8_widen(pc, xv, s.pc, vm, n.value() * 4u);
      CompOpUtils::div255_pack(pc, xv_half, xv);

      pc->v_swizzlev_u8(yv_half, xv_half, pc->simdVecConst(&pc->ct.swizu8_3xxx2xxx1xxx0xxx_to_3333222211110000, Bcst::kNA, yv_half));
      pc->v_not_u32(yv_half, yv_half);

      CompOpUtils::mul_u8_widen(pc, zv, d.pc, yv_half, n.value() * 4u);
      CompOpUtils::div255_pack(pc, d.pc, zv);
      pc->v_add_u8(d.pc, d.pc, xv_half);
      out.pc.init(d.pc);
    }
#else
    if (!hasMask) {
      // Dca' = Sca + Dca.(1 - Sa)
      // Da'  = Sa  + Da .(1 - Sa)
      srcFetch(s, n, PixelFlags::kPC | PixelFlags::kUI | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& uv = s.ui;
      VecArray& dv = d.uc;

      pc->v_mul_u16(dv, dv, uv);
      pc->v_div255_u16(dv);

      VecArray dh = x_pack_pixels(pc, dv, n, "d");
      dh = dh.cloneAs(s.pc[0]);
      pc->v_add_i32(dh, dh, s.pc);

      out.pc.init(dh);
    }
    else {
      // Dca' = Sca.m + Dca.(1 - Sa.m)
      // Da'  = Sa .m + Da .(1 - Sa.m)
      srcFetch(s, n, PixelFlags::kUC, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      pc->v_expand_alpha_16(xv, sv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_mul_u16(dv, dv, xv);
      pc->v_div255_u16(dv);

      pc->v_add_i16(dv, dv, sv);
      out.uc.init(dv);
    }
#endif

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - SrcIn
  // --------------------------

  if (isSrcIn()) {
    // Composition:
    //   Da - Required, otherwise SRC_COPY.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Sca.Da
      // Da'  = Sa .Da
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUA, predicate);

      VecArray& sv = s.uc;
      VecArray& dv = d.ua;

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Sca.m.Da + Dca.(1 - m)
      // Da'  = Sa .m.Da + Da .(1 - m)
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->v_expand_alpha_16(xv, dv, kUseHi);
      pc->v_mul_u16(xv, xv, sv);
      pc->v_div255_u16(xv);
      pc->v_mul_u16(xv, xv, vm);

      VecArray vn;
      vMaskProcRGBA32InvertMask(vn, vm, coverageFlags);

      pc->v_mul_u16(dv, dv, vn);
      vMaskProcRGBA32InvertDone(vn, vm, coverageFlags);

      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - SrcOut
  // ---------------------------

  if (isSrcOut()) {
    // Composition:
    //   Da - Required, otherwise CLEAR.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Sca.(1 - Da)
      // Da'  = Sa .(1 - Da)
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUI, predicate);

      VecArray& sv = s.uc;
      VecArray& dv = d.ui;

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Sca.(1 - Da).m + Dca.(1 - m)
      // Da'  = Sa .(1 - Da).m + Da .(1 - m)
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->v_expand_alpha_16(xv, dv, kUseHi);
      pc->v_inv255_u16(xv, xv);

      pc->v_mul_u16(xv, xv, sv);
      pc->v_div255_u16(xv);
      pc->v_mul_u16(xv, xv, vm);

      VecArray vn;
      vMaskProcRGBA32InvertMask(vn, vm, coverageFlags);

      pc->v_mul_u16(dv, dv, vn);
      vMaskProcRGBA32InvertDone(vn, vm, coverageFlags);

      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - SrcAtop
  // ----------------------------

  if (isSrcAtop()) {
    // Composition:
    //   Da - Required.
    //   Sa - Required.

    if (!hasMask) {
      // Dca' = Sca.Da + Dca.(1 - Sa)
      // Da'  = Sa .Da + Da .(1 - Sa) = Da
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kUI | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.uc;
      VecArray& uv = s.ui;
      VecArray& dv = d.uc;

      pc->v_expand_alpha_16(xv, dv, kUseHi);
      pc->v_mul_u16(dv, dv, uv);
      pc->v_mul_u16(xv, xv, sv);
      pc->v_add_i16(dv, dv, xv);
      pc->v_div255_u16(dv);

      out.uc.init(dv);
    }
    else {
      // Dca' = Sca.Da.m + Dca.(1 - Sa.m)
      // Da'  = Sa .Da.m + Da .(1 - Sa.m) = Da
      srcFetch(s, n, PixelFlags::kUC, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      pc->v_expand_alpha_16(xv, sv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_expand_alpha_16(yv, dv, kUseHi);
      pc->v_mul_u16(dv, dv, xv);
      pc->v_mul_u16(yv, yv, sv);
      pc->v_add_i16(dv, dv, yv);
      pc->v_div255_u16(dv);

      out.uc.init(dv);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Dst
  // ------------------------

  if (isDstCopy()) {
    // Dca' = Dca
    // Da'  = Da
    BL_NOT_REACHED();
  }

  // VMaskProc - RGBA32 - DstOver
  // ----------------------------

  if (isDstOver()) {
    // Composition:
    //   Da - Required, otherwise DST_COPY.
    //   Sa - Optional.

    if (!hasMask) {
      // Dca' = Dca + Sca.(1 - Da)
      // Da'  = Da  + Sa .(1 - Da)
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kPC | PixelFlags::kUI, predicate);

      VecArray& sv = s.uc;
      VecArray& dv = d.ui;

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);

      VecArray dh = x_pack_pixels(pc, dv, n, "d");
      dh = dh.cloneAs(d.pc[0]);
      pc->v_add_i32(dh, dh, d.pc);

      out.pc.init(dh);
    }
    else {
      // Dca' = Dca + Sca.m.(1 - Da)
      // Da'  = Da  + Sa .m.(1 - Da)
      srcFetch(s, n, PixelFlags::kUC, predicate);
      dstFetch(d, n, PixelFlags::kPC | PixelFlags::kUI, predicate);

      VecArray& sv = s.uc;
      VecArray& dv = d.ui;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);

      VecArray dh = x_pack_pixels(pc, dv, n, "d");
      dh = dh.cloneAs(d.pc[0]);
      pc->v_add_i32(dh, dh, d.pc);

      out.pc.init(dh);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - DstIn
  // --------------------------

  if (isDstIn()) {
    // Composition:
    //   Da - Optional.
    //   Sa - Required, otherwise DST_COPY.

    if (!hasMask) {
      // Dca' = Dca.Sa
      // Da'  = Da .Sa
      srcFetch(s, n, PixelFlags::kUA | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.ua;
      VecArray& dv = d.uc;

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Dca.(1 - m.(1 - Sa))
      // Da'  = Da .(1 - m.(1 - Sa))
      srcFetch(s, n, PixelFlags::kUI, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.ui;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
      pc->v_inv255_u16(sv, sv);

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - DstOut
  // ---------------------------

  if (isDstOut()) {
    // Composition:
    //   Da - Optional.
    //   Sa - Required, otherwise CLEAR.

    if (!hasMask) {
      // Dca' = Dca.(1 - Sa)
      // Da'  = Da .(1 - Sa)
      srcFetch(s, n, PixelFlags::kUI | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.ui;
      VecArray& dv = d.uc;

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }
    else {
      // Dca' = Dca.(1 - Sa.m)
      // Da'  = Da .(1 - Sa.m)
      srcFetch(s, n, PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.ua;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
      pc->v_inv255_u16(sv, sv);

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    if (!useDa)
      FetchUtils::fillAlphaChannel(pc, out);
    return;
  }

  // VMaskProc - RGBA32 - DstAtop
  // ----------------------------

  if (isDstAtop()) {
    // Composition:
    //   Da - Required.
    //   Sa - Required.

    if (!hasMask) {
      // Dca' = Dca.Sa + Sca.(1 - Da)
      // Da'  = Da .Sa + Sa .(1 - Da)
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kUA | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.uc;
      VecArray& uv = s.ua;
      VecArray& dv = d.uc;

      pc->v_expand_alpha_16(xv, dv, kUseHi);
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
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kUI, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.uc;
      VecArray& uv = s.ui;
      VecArray& dv = d.uc;

      pc->v_expand_alpha_16(xv, dv, kUseHi);
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

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Xor
  // ------------------------

  if (isXor()) {
    // Composition:
    //   Da - Required.
    //   Sa - Required.

    if (!hasMask) {
      // Dca' = Dca.(1 - Sa) + Sca.(1 - Da)
      // Da'  = Da .(1 - Sa) + Sa .(1 - Da)
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kUI | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.uc;
      VecArray& uv = s.ui;
      VecArray& dv = d.uc;

      pc->v_expand_alpha_16(xv, dv, kUseHi);
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
      srcFetch(s, n, PixelFlags::kUC, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.uc;
      VecArray& dv = d.uc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      pc->v_expand_alpha_16(xv, sv, kUseHi);
      pc->v_expand_alpha_16(yv, dv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_inv255_u16(yv, yv);
      pc->v_mul_u16(dv, dv, xv);
      pc->v_mul_u16(sv, sv, yv);

      pc->v_add_i16(dv, dv, sv);
      pc->v_div255_u16(dv);
      out.uc.init(dv);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Plus
  // -------------------------

  if (isPlus()) {
    if (!hasMask) {
      // Dca' = Clamp(Dca + Sca)
      // Da'  = Clamp(Da  + Sa )
      srcFetch(s, n, PixelFlags::kPC | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kPC, predicate);

      VecArray& sh = s.pc;
      VecArray& dh = d.pc;

      pc->v_adds_u8(dh, dh, sh);
      out.pc.init(dh);
    }
    else {
      // Dca' = Clamp(Dca + Sca.m)
      // Da'  = Clamp(Da  + Sa .m)
      srcFetch(s, n, PixelFlags::kUC, predicate);
      dstFetch(d, n, PixelFlags::kPC, predicate);

      VecArray& sv = s.uc;
      VecArray& dh = d.pc;

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);

      VecArray sh = x_pack_pixels(pc, sv, n, "s");
      pc->v_adds_u8(dh, dh, sh.cloneAs(dh[0]));

      out.pc.init(dh);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Minus
  // --------------------------

  if (isMinus()) {
    if (!hasMask) {
      // Dca' = Clamp(Dca - Sca) + Sca.(1 - Da)
      // Da'  = Da + Sa.(1 - Da)
      if (useDa) {
        srcFetch(s, n, PixelFlags::kUC, predicate);
        dstFetch(d, n, PixelFlags::kUC, predicate);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->v_expand_alpha_16(xv, dv, kUseHi);
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
        srcFetch(s, n, PixelFlags::kPC, predicate);
        dstFetch(d, n, PixelFlags::kPC, predicate);

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
        srcFetch(s, n, PixelFlags::kUC, predicate);
        dstFetch(d, n, PixelFlags::kUC, predicate);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->v_expand_alpha_16(xv, dv, kUseHi);
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

        if (blTestFlag(coverageFlags, PixelCoverageFlags::kImmutable)) {
          pc->v_inv255_u16(vm[0], vm[0]);
          pc->v_swizzle_u32x4(vm[0], vm[0], swizzle(2, 2, 0, 0));
        }

        pc->v_add_i16(dv, dv, yv);
        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      // Dca' = Clamp(Dca - Sca).m + Dca.(1 - m)
      // Da'  = <unchanged>
      else {
        srcFetch(s, n, PixelFlags::kUC, predicate);
        dstFetch(d, n, PixelFlags::kUC, predicate);

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

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Modulate
  // -----------------------------

  if (isModulate()) {
    VecArray& dv = d.uc;
    VecArray& sv = s.uc;

    if (!hasMask) {
      // Dca' = Dca.Sca
      // Da'  = Da .Sa
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kImmutable, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);
    }
    else {
      // Dca' = Dca.(Sca.m + 1 - m)
      // Da'  = Da .(Sa .m + 1 - m)
      srcFetch(s, n, PixelFlags::kUC, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
      pc->v_add_i16(sv, sv, pc->simdConst(&ct.i_00FF00FF00FF00FF, Bcst::kNA, sv));
      pc->v_sub_i16(sv, sv, vm);
      pc->v_mul_u16(dv, dv, sv);
      pc->v_div255_u16(dv);

      out.uc.init(dv);
    }

    if (!useDa)
      pc->vFillAlpha255W(dv, dv);

    out.uc.init(dv);
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Multiply
  // -----------------------------

  if (isMultiply()) {
    if (!hasMask) {
      if (useDa && useSa) {
        // Dca' = Dca.(Sca + 1 - Sa) + Sca.(1 - Da)
        // Da'  = Da .(Sa  + 1 - Sa) + Sa .(1 - Da)
        srcFetch(s, n, PixelFlags::kUC | PixelFlags::kImmutable, predicate);
        dstFetch(d, n, PixelFlags::kUC, predicate);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        // SPLIT.
        for (unsigned int i = 0; i < kSplit; i++) {
          VecArray sh = sv.even_odd(i);
          VecArray dh = dv.even_odd(i);
          VecArray xh = xv.even_odd(i);
          VecArray yh = yv.even_odd(i);

          pc->v_expand_alpha_16(yh, sh, kUseHi);
          pc->v_expand_alpha_16(xh, dh, kUseHi);
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
        srcFetch(s, n, PixelFlags::kUC | PixelFlags::kImmutable, predicate);
        dstFetch(d, n, PixelFlags::kUC, predicate);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->v_expand_alpha_16(xv, dv, kUseHi);
        pc->v_inv255_u16(xv, xv);
        pc->v_add_i16(dv, dv, xv);
        pc->v_mul_u16(dv, dv, sv);

        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      else if (hasSa()) {
        // Dc'  = Dc.(Sca + 1 - Sa)
        // Da'  = Da.(Sa  + 1 - Sa)
        srcFetch(s, n, PixelFlags::kUC | PixelFlags::kImmutable, predicate);
        dstFetch(d, n, PixelFlags::kUC, predicate);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->v_expand_alpha_16(xv, sv, kUseHi);
        pc->v_inv255_u16(xv, xv);
        pc->v_add_i16(xv, xv, sv);
        pc->v_mul_u16(dv, dv, xv);

        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
      else {
        // Dc' = Dc.Sc
        srcFetch(s, n, PixelFlags::kUC | PixelFlags::kImmutable, predicate);
        dstFetch(d, n, PixelFlags::kUC, predicate);

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
        srcFetch(s, n, PixelFlags::kUC, predicate);
        dstFetch(d, n, PixelFlags::kUC, predicate);

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

          pc->v_expand_alpha_16(yh, sh, kUseHi);
          pc->v_expand_alpha_16(xh, dh, kUseHi);
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
        srcFetch(s, n, PixelFlags::kUC, predicate);
        dstFetch(d, n, PixelFlags::kUC, predicate);

        VecArray& sv = s.uc;
        VecArray& dv = d.uc;

        pc->v_mul_u16(sv, sv, vm);
        pc->v_div255_u16(sv);

        pc->v_expand_alpha_16(xv, sv, kUseHi);
        pc->v_inv255_u16(xv, xv);
        pc->v_add_i16(xv, xv, sv);
        pc->v_mul_u16(dv, dv, xv);

        pc->v_div255_u16(dv);
        out.uc.init(dv);
      }
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Overlay
  // ----------------------------

  if (isOverlay()) {
    srcFetch(s, n, PixelFlags::kUC, predicate);
    dstFetch(d, n, PixelFlags::kUC, predicate);

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

        pc->v_expand_alpha_16(xh, dh, kUseHi);
        pc->v_expand_alpha_16(yh, sh, kUseHi);

        pc->v_mul_u16(xh, xh, sh);                                 // Sca.Da
        pc->v_mul_u16(yh, yh, dh);                                 // Dca.Sa
        pc->v_mul_u16(zh, dh, sh);                                 // Dca.Sca

        pc->v_add_i16(sh, sh, dh);                                 // Dca + Sca
        pc->v_sub_i16(xh, xh, zh);                                 // Sca.Da - Dca.Sca
        pc->vZeroAlphaW(zh, zh);
        pc->v_add_i16(xh, xh, yh);                                 // Dca.Sa + Sca.Da - Dca.Sca
        pc->v_expand_alpha_16(yh, dh, kUseHi);                     // Da
        pc->v_sub_i16(xh, xh, zh);                                 // [C=Dca.Sa + Sca.Da - 2.Dca.Sca] [A=Sa.Da]

        pc->v_slli_i16(dh, dh, 1);                                 // 2.Dca
        pc->v_cmp_gt_i16(yh, yh, dh);                              // 2.Dca < Da
        pc->v_div255_u16(xh);
        pc->v_or_i64(yh, yh, pc->simdConst(&ct.i_FFFF000000000000, Bcst::k64, yh));

        pc->v_expand_alpha_16(zh, xh, kUseHi);
        // if (2.Dca < Da)
        //   X = [C = -(Dca.Sa + Sca.Da - 2.Sca.Dca)] [A = -Sa.Da]
        // else
        //   X = [C =  (Dca.Sa + Sca.Da - 2.Sca.Dca)] [A = -Sa.Da]
        pc->v_xor_i32(xh, xh, yh);
        pc->v_sub_i16(xh, xh, yh);

        // if (2.Dca < Da)
        //   Y = [C = 0] [A = 0]
        // else
        //   Y = [C = Sa.Da] [A = 0]
        pc->v_bic_i32(yh, zh, yh);

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

      pc->v_expand_alpha_16(xv, dv, kUseHi);                       // Da
      pc->v_slli_i16(dv, dv, 1);                                   // 2.Dca

      pc->v_cmp_gt_i16(yv, xv, dv);                                //  (2.Dca < Da) ? -1 : 0
      pc->v_sub_i16(xv, xv, dv);                                   // -(2.Dca - Da)

      pc->v_xor_i32(xv, xv, yv);
      pc->v_sub_i16(xv, xv, yv);                                   // 2.Dca < Da ? 2.Dca - Da : -(2.Dca - Da)
      pc->v_bic_i32(yv, xv, yv);                                   // 2.Dca < Da ? 0          : -(2.Dca - Da)
      pc->v_add_i16(xv, xv, pc->simdConst(&ct.i_00FF00FF00FF00FF, Bcst::kNA, xv));

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

      pc->v_mul_u16(xv, dv, sv);                                                                // Dc.Sc
      pc->v_cmp_gt_i16(yv, dv, pc->simdConst(&ct.i_007F007F007F007F, Bcst::kNA, yv));           // !(2.Dc < 1)
      pc->v_add_i16(dv, dv, sv);                                                                // Dc + Sc
      pc->v_div255_u16(xv);

      pc->v_slli_i16(dv, dv, 1);                                                                 // 2.Dc + 2.Sc
      pc->v_slli_i16(xv, xv, 1);                                                                 // 2.Dc.Sc
      pc->v_sub_i16(dv, dv, pc->simdConst(&ct.i_00FF00FF00FF00FF, Bcst::kNA, dv));               // 2.Dc + 2.Sc - 1

      pc->v_xor_i32(xv, xv, yv);
      pc->v_and_i32(dv, dv, yv);                                                                 // 2.Dc < 1 ? 0 : 2.Dc + 2.Sc - 1
      pc->v_sub_i16(xv, xv, yv);                                                                 // 2.Dc < 1 ? 2.Dc.Sc : -2.Dc.Sc
      pc->v_add_i16(dv, dv, xv);                                                                 // 2.Dc < 1 ? 2.Dc.Sc : 2.Dc + 2.Sc - 1 - 2.Dc.Sc

      out.uc.init(dv);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Screen
  // ---------------------------

  if (isScreen()) {
#if defined(BL_JIT_ARCH_A64)
    srcFetch(s, n, PixelFlags::kPC | PixelFlags::kImmutable, predicate);
    dstFetch(d, n, PixelFlags::kPC, predicate);

    VecArray xv_half = xv.half();
    VecArray yv_half = yv.half();

    VecArray src = s.pc;

    if (hasMask) {
      CompOpUtils::mul_u8_widen(pc, xv, src, vm, n.value() * 4u);
      CompOpUtils::div255_pack(pc, xv_half, xv);
      src = xv_half;
    }

    pc->v_not_u32(yv_half, src);

    CompOpUtils::mul_u8_widen(pc, zv, d.pc, yv_half, n.value() * 4u);
    CompOpUtils::div255_pack(pc, d.pc, zv);

    pc->v_add_u8(d.pc, d.pc, src);
    out.pc.init(d.pc);
#else
    // Dca' = Sca + Dca.(1 - Sca)
    // Da'  = Sa  + Da .(1 - Sa)
    srcFetch(s, n, PixelFlags::kUC | (hasMask ? PixelFlags::kNone : PixelFlags::kImmutable), predicate);
    dstFetch(d, n, PixelFlags::kUC, predicate);

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
#endif

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Darken & Lighten
  // -------------------------------------

  if (isDarken() || isLighten()) {
    OpcodeVVV min_or_max = isDarken() ? OpcodeVVV::kMinU8 : OpcodeVVV::kMaxU8;

    srcFetch(s, n, PixelFlags::kUC, predicate);
    dstFetch(d, n, PixelFlags::kUC, predicate);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

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

        pc->v_expand_alpha_16(xh, dh, kUseHi);
        pc->v_expand_alpha_16(yh, sh, kUseHi);

        pc->v_inv255_u16(xh, xh);
        pc->v_inv255_u16(yh, yh);

        pc->v_mul_u16(xh, xh, sh);
        pc->v_mul_u16(yh, yh, dh);
        pc->v_div255_u16_2x(xh, yh);

        pc->v_add_i16(dh, dh, xh);
        pc->v_add_i16(sh, sh, yh);

        pc->emit_3v(min_or_max, dh, dh, sh);
      }

      out.uc.init(dv);
    }
    else if (useDa) {
      // Dca' = minmax(Dca + Sc.(1 - Da), Sc)
      // Da'  = 1
      pc->v_expand_alpha_16(xv, dv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_mul_u16(xv, xv, sv);
      pc->v_div255_u16(xv);
      pc->v_add_i16(dv, dv, xv);
      pc->emit_3v(min_or_max, dv, dv, sv);

      out.uc.init(dv);
    }
    else if (useSa) {
      // Dc' = minmax(Dc, Sca + Dc.(1 - Sa))
      pc->v_expand_alpha_16(xv, sv, kUseHi);
      pc->v_inv255_u16(xv, xv);
      pc->v_mul_u16(xv, xv, dv);
      pc->v_div255_u16(xv);
      pc->v_add_i16(xv, xv, sv);
      pc->emit_3v(min_or_max, dv, dv, xv);

      out.uc.init(dv);
    }
    else {
      // Dc' = minmax(Dc, Sc)
      pc->emit_3v(min_or_max, dv, dv, sv);

      out.uc.init(dv);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - ColorDodge (SCALAR)
  // ----------------------------------------

  if (isColorDodge() && n == 1) {
    // Dca' = min(Dca.Sa.Sa / max(Sa - Sca, 0.001), Sa.Da) + Sca.(1 - Da) + Dca.(1 - Sa);
    // Da'  = min(Da .Sa.Sa / max(Sa - Sa , 0.001), Sa.Da) + Sa .(1 - Da) + Da .(1 - Sa);

    srcFetch(s, n, PixelFlags::kUC, predicate);
    dstFetch(d, n, PixelFlags::kPC, predicate);

    Vec& s0 = s.uc[0];
    Vec& d0 = d.pc[0];
    Vec& x0 = xv[0];
    Vec& y0 = yv[0];
    Vec& z0 = zv[0];

    if (hasMask) {
      pc->v_mul_u16(s0, s0, vm[0]);
      pc->v_div255_u16(s0);
    }

    pc->v_cvt_u8_to_u32(d0, d0);
    pc->v_cvt_u16_lo_to_u32(s0, s0);

    pc->v_cvt_i32_to_f32(y0, s0);
    pc->v_cvt_i32_to_f32(z0, d0);
    pc->v_packs_i32_i16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);
    pc->v_xor_f32(y0, y0, pc->simdConst(&ct.f32_sgn, Bcst::k32, y0));
    pc->v_mul_f32(z0, z0, x0);
    pc->v_and_f32(y0, y0, pc->simdConst(&ct.i_FFFFFFFF_FFFFFFFF_FFFFFFFF_0, Bcst::kNA, y0));
    pc->v_add_f32(y0, y0, x0);

    pc->v_max_f32(y0, y0, pc->simdConst(&ct.f32_1e_m3, Bcst::k32, y0));
    pc->v_div_f32(z0, z0, y0);

    pc->v_swizzle_u32x4(s0, d0, swizzle(1, 1, 3, 3));
    pc->vExpandAlphaHi16(s0, s0);
    pc->vExpandAlphaLo16(s0, s0);
    pc->v_inv255_u16(s0, s0);
    pc->v_mul_u16(d0, d0, s0);
    pc->v_swizzle_u32x4(s0, d0, swizzle(1, 0, 3, 2));
    pc->v_add_i16(d0, d0, s0);

    pc->v_mul_f32(z0, z0, x0);
    pc->vExpandAlphaPS(x0, z0);
    pc->v_min_f32(z0, z0, x0);

    pc->v_cvt_trunc_f32_to_i32(z0, z0);
    pc->xPackU32ToU16Lo(z0, z0);
    pc->v_add_i16(d0, d0, z0);

    pc->v_div255_u16(d0);
    out.uc.init(d0);

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - ColorBurn (SCALAR)
  // ---------------------------------------

  if (isColorBurn() && n == 1) {
    // Dca' = Sa.Da - min(Sa.Da, (Da - Dca).Sa.Sa / max(Sca, 0.001)) + Sca.(1 - Da) + Dca.(1 - Sa)
    // Da'  = Sa.Da - min(Sa.Da, (Da - Da ).Sa.Sa / max(Sa , 0.001)) + Sa .(1 - Da) + Da .(1 - Sa)
    srcFetch(s, n, PixelFlags::kUC, predicate);
    dstFetch(d, n, PixelFlags::kPC, predicate);

    Vec& s0 = s.uc[0];
    Vec& d0 = d.pc[0];
    Vec& x0 = xv[0];
    Vec& y0 = yv[0];
    Vec& z0 = zv[0];

    if (hasMask) {
      pc->v_mul_u16(s0, s0, vm[0]);
      pc->v_div255_u16(s0);
    }

    pc->v_cvt_u8_to_u32(d0, d0);
    pc->v_cvt_u16_lo_to_u32(s0, s0);

    pc->v_cvt_i32_to_f32(y0, s0);
    pc->v_cvt_i32_to_f32(z0, d0);
    pc->v_packs_i32_i16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);
    pc->v_max_f32(y0, y0, pc->simdConst(&ct.f32_1e_m3, Bcst::k32, y0));
    pc->v_mul_f32(z0, z0, x0);                                     // Dca.Sa

    pc->vExpandAlphaPS(x0, z0);                                    // Sa.Da
    pc->v_xor_f32(z0, z0, pc->simdConst(&ct.f32_sgn, Bcst::k32, z0));

    pc->v_and_f32(z0, z0, pc->simdConst(&ct.i_FFFFFFFF_FFFFFFFF_FFFFFFFF_0, Bcst::kNA, z0));
    pc->v_add_f32(z0, z0, x0);                                     // (Da - Dxa).Sa
    pc->v_div_f32(z0, z0, y0);

    pc->v_swizzle_u32x4(s0, d0, swizzle(1, 1, 3, 3));
    pc->vExpandAlphaHi16(s0, s0);
    pc->vExpandAlphaLo16(s0, s0);
    pc->v_inv255_u16(s0, s0);
    pc->v_mul_u16(d0, d0, s0);
    pc->v_swizzle_u32x4(s0, d0, swizzle(1, 0, 3, 2));
    pc->v_add_i16(d0, d0, s0);

    pc->vExpandAlphaPS(x0, y0);                                    // Sa
    pc->v_mul_f32(z0, z0, x0);
    pc->vExpandAlphaPS(x0, z0);                                    // Sa.Da
    pc->v_min_f32(z0, z0, x0);
    pc->v_and_f32(z0, z0, pc->simdConst(&ct.i_FFFFFFFF_FFFFFFFF_FFFFFFFF_0, Bcst::kNA, z0));
    pc->v_sub_f32(x0, x0, z0);

    pc->v_cvt_trunc_f32_to_i32(x0, x0);
    pc->xPackU32ToU16Lo(x0, x0);
    pc->v_add_i16(d0, d0, x0);

    pc->v_div255_u16(d0);
    out.uc.init(d0);

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - LinearBurn
  // -------------------------------

  if (isLinearBurn()) {
    srcFetch(s, n, PixelFlags::kUC | (hasMask ? PixelFlags::kNone : PixelFlags::kImmutable), predicate);
    dstFetch(d, n, PixelFlags::kUC, predicate);

    VecArray& sv = s.uc;
    VecArray& dv = d.uc;

    if (hasMask) {
      pc->v_mul_u16(sv, sv, vm);
      pc->v_div255_u16(sv);
    }

    if (useDa && useSa) {
      // Dca' = Dca + Sca - Sa.Da
      // Da'  = Da  + Sa  - Sa.Da
      pc->v_expand_alpha_16(xv, sv, kUseHi);
      pc->v_expand_alpha_16(yv, dv, kUseHi);
      pc->v_mul_u16(xv, xv, yv);
      pc->v_div255_u16(xv);
      pc->v_add_i16(dv, dv, sv);
      pc->v_subs_u16(dv, dv, xv);
    }
    else if (useDa || useSa) {
      pc->v_expand_alpha_16(xv, useDa ? dv : sv, kUseHi);
      pc->v_add_i16(dv, dv, sv);
      pc->v_subs_u16(dv, dv, xv);
    }
    else {
      // Dca' = Dc + Sc - 1
      pc->v_add_i16(dv, dv, sv);
      pc->v_subs_u16(dv, dv, pc->simdConst(&ct.i_000000FF00FF00FF, Bcst::kNA, dv));
    }

    out.uc.init(dv);
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - LinearLight
  // --------------------------------

  if (isLinearLight() && n == 1) {
    srcFetch(s, n, PixelFlags::kUC, predicate);
    dstFetch(d, n, PixelFlags::kUC, predicate);

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

      Vec& d0 = dv[0];
      Vec& s0 = sv[0];
      Vec& x0 = xv[0];
      Vec& y0 = yv[0];

      pc->vExpandAlphaLo16(y0, d0);
      pc->vExpandAlphaLo16(x0, s0);

      pc->v_interleave_lo_u64(d0, d0, s0);
      pc->v_interleave_lo_u64(x0, x0, y0);

      pc->v_mov(s0, d0);
      pc->v_mul_u16(d0, d0, x0);
      pc->v_inv255_u16(x0, x0);
      pc->v_div255_u16(d0);

      pc->v_mul_u16(s0, s0, x0);
      pc->v_swap_u64(x0, s0);
      pc->v_swap_u64(y0, d0);
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
      pc->v_slli_i16(sv, sv, 1);
      pc->v_add_i16(dv, dv, sv);
      pc->v_subs_u16(dv, dv, pc->simdConst(&ct.i_000000FF00FF00FF, Bcst::kNA, dv));
      pc->v_min_i16(dv, dv, pc->simdConst(&ct.i_00FF00FF00FF00FF, Bcst::kNA, dv));

      out.uc.init(dv);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - PinLight
  // -----------------------------

  if (isPinLight()) {
    srcFetch(s, n, PixelFlags::kUC, predicate);
    dstFetch(d, n, PixelFlags::kUC, predicate);

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

      pc->v_expand_alpha_16(yv, sv, kUseHi);                                                       // Sa
      pc->v_expand_alpha_16(xv, dv, kUseHi);                                                       // Da

      pc->v_mul_u16(yv, yv, dv);                                                                   // Dca.Sa
      pc->v_mul_u16(xv, xv, sv);                                                                   // Sca.Da
      pc->v_add_i16(dv, dv, sv);                                                                   // Dca + Sca
      pc->v_div255_u16_2x(yv, xv);

      pc->v_sub_i16(yv, yv, dv);                                                                   // Dca.Sa - Dca - Sca
      pc->v_sub_i16(dv, dv, xv);                                                                   // Dca + Sca - Sca.Da
      pc->v_sub_i16(xv, xv, yv);                                                                   // Dca + Sca + Sca.Da - Dca.Sa

      pc->v_expand_alpha_16(yv, sv, kUseHi);                                                       // Sa
      pc->v_slli_i16(sv, sv, 1);                                                                   // 2.Sca
      pc->v_cmp_gt_i16(sv, sv, yv);                                                                // !(2.Sca <= Sa)

      pc->v_sub_i16(zv, dv, xv);
      pc->v_expand_alpha_16(zv, zv, kUseHi);                                                       // -Da.Sa
      pc->v_and_i32(zv, zv, sv);                                                                   // 2.Sca <= Sa ? 0 : -Da.Sa
      pc->v_add_i16(xv, xv, zv);                                                                   // 2.Sca <= Sa ? Dca + Sca + Sca.Da - Dca.Sa : Dca + Sca + Sca.Da - Dca.Sa - Da.Sa

      // if 2.Sca <= Sa:
      //   min(dv, xv)
      // else
      //   max(dv, xv) <- ~min(~dv, ~xv)
      pc->v_xor_i32(dv, dv, sv);
      pc->v_xor_i32(xv, xv, sv);
      pc->v_min_i16(dv, dv, xv);
      pc->v_xor_i32(dv, dv, sv);

      out.uc.init(dv);
    }
    else if (useDa) {
      // if 2.Sc <= 1
      //   Dca' = min(Dca + Sc - Sc.Da, Sc + Sc.Da)
      //   Da'  = min(Da  + 1  - 1 .Da, 1  + 1 .Da) = 1
      // else
      //   Dca' = max(Dca + Sc - Sc.Da, Sc + Sc.Da - Da)
      //   Da'  = max(Da  + 1  - 1 .Da, 1  + 1 .Da - Da) = 1

      pc->v_expand_alpha_16(xv, dv, kUseHi);                                                       // Da
      pc->v_mul_u16(xv, xv, sv);                                                                   // Sc.Da
      pc->v_add_i16(dv, dv, sv);                                                                   // Dca + Sc
      pc->v_div255_u16(xv);

      pc->v_cmp_gt_i16(yv, sv, pc->simdConst(&ct.i_007F007F007F007F, Bcst::kNA, yv));              // !(2.Sc <= 1)
      pc->v_add_i16(sv, sv, xv);                                                                   // Sc + Sc.Da
      pc->v_sub_i16(dv, dv, xv);                                                                   // Dca + Sc - Sc.Da
      pc->v_expand_alpha_16(xv, xv);                                                               // Da
      pc->v_and_i32(xv, xv, yv);                                                                   // 2.Sc <= 1 ? 0 : Da
      pc->v_sub_i16(sv, sv, xv);                                                                   // 2.Sc <= 1 ? Sc + Sc.Da : Sc + Sc.Da - Da

      // if 2.Sc <= 1:
      //   min(dv, sv)
      // else
      //   max(dv, sv) <- ~min(~dv, ~sv)
      pc->v_xor_i32(dv, dv, yv);
      pc->v_xor_i32(sv, sv, yv);
      pc->v_min_i16(dv, dv, sv);
      pc->v_xor_i32(dv, dv, yv);

      out.uc.init(dv);
    }
    else if (useSa) {
      // if 2.Sca <= Sa
      //   Dc' = min(Dc, Dc + 2.Sca - Dc.Sa)
      // else
      //   Dc' = max(Dc, Dc + 2.Sca - Dc.Sa - Sa)

      pc->v_expand_alpha_16(xv, sv, kUseHi);                                                       // Sa
      pc->v_slli_i16(sv, sv, 1);                                                                   // 2.Sca
      pc->v_cmp_gt_i16(yv, sv, xv);                                                                // !(2.Sca <= Sa)
      pc->v_and_i32(yv, yv, xv);                                                                   // 2.Sca <= Sa ? 0 : Sa
      pc->v_mul_u16(xv, xv, dv);                                                                   // Dc.Sa
      pc->v_add_i16(sv, sv, dv);                                                                   // Dc + 2.Sca
      pc->v_div255_u16(xv);
      pc->v_sub_i16(sv, sv, yv);                                                                   // 2.Sca <= Sa ? Dc + 2.Sca : Dc + 2.Sca - Sa
      pc->v_cmp_eq_i16(yv, yv, pc->simdConst(&ct.i_0000000000000000, Bcst::kNA, yv));              // 2.Sc <= 1
      pc->v_sub_i16(sv, sv, xv);                                                                   // 2.Sca <= Sa ? Dc + 2.Sca - Dc.Sa : Dc + 2.Sca - Dc.Sa - Sa

      // if 2.Sc <= 1:
      //   min(dv, sv)
      // else
      //   max(dv, sv) <- ~min(~dv, ~sv)
      pc->v_xor_i32(dv, dv, yv);
      pc->v_xor_i32(sv, sv, yv);
      pc->v_max_i16(dv, dv, sv);
      pc->v_xor_i32(dv, dv, yv);

      out.uc.init(dv);
    }
    else {
      // if 2.Sc <= 1
      //   Dc' = min(Dc, 2.Sc)
      // else
      //   Dc' = max(Dc, 2.Sc - 1)

      pc->v_slli_i16(sv, sv, 1);                                                                   // 2.Sc
      pc->v_min_i16(xv, sv, dv);                                                                   // min(Dc, 2.Sc)

      pc->v_cmp_gt_i16(yv, sv, pc->simdConst(&ct.i_00FF00FF00FF00FF, Bcst::kNA, yv));              // !(2.Sc <= 1)
      pc->v_sub_i16(sv, sv, pc->simdConst(&ct.i_00FF00FF00FF00FF, Bcst::kNA, sv));                 // 2.Sc - 1
      pc->v_max_i16(dv, dv, sv);                                                                   // max(Dc, 2.Sc - 1)

      pc->v_blendv_u8(xv, xv, dv, yv);                                                             // 2.Sc <= 1 ? min(Dc, 2.Sc) : max(Dc, 2.Sc - 1)
      out.uc.init(xv);
    }

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - HardLight
  // ------------------------------

  if (isHardLight()) {
    // if (2.Sca < Sa)
    //   Dca' = Dca + Sca - (Dca.Sa + Sca.Da - 2.Sca.Dca)
    //   Da'  = Da  + Sa  - Sa.Da
    // else
    //   Dca' = Dca + Sca + (Dca.Sa + Sca.Da - 2.Sca.Dca) - Sa.Da
    //   Da'  = Da  + Sa  - Sa.Da
    srcFetch(s, n, PixelFlags::kUC, predicate);
    dstFetch(d, n, PixelFlags::kUC, predicate);

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

      pc->v_expand_alpha_16(xh, dh, kUseHi);
      pc->v_expand_alpha_16(yh, sh, kUseHi);

      pc->v_mul_u16(xh, xh, sh); // Sca.Da
      pc->v_mul_u16(yh, yh, dh); // Dca.Sa
      pc->v_mul_u16(zh, dh, sh); // Dca.Sca

      pc->v_add_i16(dh, dh, sh);
      pc->v_sub_i16(xh, xh, zh);
      pc->v_add_i16(xh, xh, yh);
      pc->v_sub_i16(xh, xh, zh);

      pc->v_expand_alpha_16(yh, yh, kUseHi);
      pc->v_expand_alpha_16(zh, sh, kUseHi);
      pc->v_div255_u16_2x(xh, yh);

      pc->v_slli_i16(sh, sh, 1);
      pc->v_cmp_gt_i16(zh, zh, sh);

      pc->v_xor_i32(xh, xh, zh);
      pc->v_sub_i16(xh, xh, zh);
      pc->vZeroAlphaW(zh, zh);
      pc->v_bic_i32(zh, yh, zh);
      pc->v_add_i16(dh, dh, xh);
      pc->v_sub_i16(dh, dh, zh);
    }

    out.uc.init(dv);
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - SoftLight (SCALAR)
  // ---------------------------------------

  if (isSoftLight() && n == 1) {
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
    srcFetch(s, n, PixelFlags::kUC, predicate);
    dstFetch(d, n, PixelFlags::kPC, predicate);

    Vec& s0 = s.uc[0];
    Vec& d0 = d.pc[0];

    Vec  a0 = pc->newV128("a0");
    Vec  b0 = pc->newV128("b0");
    Vec& x0 = xv[0];
    Vec& y0 = yv[0];
    Vec& z0 = zv[0];

    if (hasMask) {
      pc->v_mul_u16(s0, s0, vm[0]);
      pc->v_div255_u16(s0);
    }

    pc->v_cvt_u8_to_u32(d0, d0);
    pc->v_cvt_u16_lo_to_u32(s0, s0);
    pc->v_broadcast_v128_f32(x0, pc->_getMemConst(&ct.f32_1div255));

    pc->v_cvt_i32_to_f32(s0, s0);
    pc->v_cvt_i32_to_f32(d0, d0);

    pc->v_mul_f32(s0, s0, x0);                                                                     // Sca (0..1)
    pc->v_mul_f32(d0, d0, x0);                                                                     // Dca (0..1)

    pc->vExpandAlphaPS(b0, d0);                                                                    // Da
    pc->v_mul_f32(x0, s0, b0);                                                                     // Sca.Da
    pc->v_max_f32(b0, b0, pc->simdConst(&ct.f32_1e_m3, Bcst::k32, b0));                            // max(Da, 0.001)

    pc->v_div_f32(a0, d0, b0);                                                                     // Dc <- Dca/Da
    pc->v_add_f32(d0, d0, s0);                                                                     // Dca + Sca

    pc->vExpandAlphaPS(y0, s0);                                                                    // Sa

    pc->v_sub_f32(d0, d0, x0);                                                                     // Dca + Sca.(1 - Da)
    pc->v_add_f32(s0, s0, s0);                                                                     // 2.Sca
    pc->v_mul_f32(z0, a0, pc->simdConst(&ct.f32_4, Bcst::k32, z0));                                // 4.Dc

    pc->v_sqrt_f32(x0, a0);                                                                        // sqrt(Dc)
    pc->v_sub_f32(s0, s0, y0);                                                                     // 2.Sca - Sa

    pc->v_mov(y0, z0);                                                                             // 4.Dc
    pc->v_madd_f32(z0, z0, a0, a0);                                                                // 4.Dc.Dc + Dc
    pc->v_mul_f32(s0, s0, b0);                                                                     // (2.Sca - Sa).Da

    pc->v_sub_f32(z0, z0, y0);                                                                     // 4.Dc.Dc + Dc - 4.Dc
    pc->v_broadcast_v128_f32(b0, pc->_getMemConst(&ct.f32_1));                                        // 1

    pc->v_add_f32(z0, z0, b0);                                                                     // 4.Dc.Dc + Dc - 4.Dc + 1
    pc->v_mul_f32(z0, z0, y0);                                                                     // 4.Dc(4.Dc.Dc + Dc - 4.Dc + 1)
    pc->v_cmp_le_f32(y0, y0, b0);                                                                  // 4.Dc <= 1

    pc->v_and_f32(z0, z0, y0);
    pc->v_bic_f32(y0, x0, y0);

    pc->v_zero_f(x0);
    pc->v_or_f32(z0, z0, y0);                                                                      // (4.Dc(4.Dc.Dc + Dc - 4.Dc + 1)) or sqrt(Dc)

    pc->v_cmp_lt_f32(x0, x0, s0);                                                                  // 2.Sca - Sa > 0
    pc->v_sub_f32(z0, z0, a0);                                                                     // [[4.Dc(4.Dc.Dc + Dc - 4.Dc + 1) or sqrt(Dc)]] - Dc

    pc->v_sub_f32(b0, b0, a0);                                                                     // 1 - Dc
    pc->v_and_f32(z0, z0, x0);

    pc->v_mul_f32(b0, b0, a0);                                                                     // Dc.(1 - Dc)
    pc->v_bic_f32(x0, b0, x0);
    pc->v_and_f32(s0, s0, pc->simdConst(&ct.i_FFFFFFFF_FFFFFFFF_FFFFFFFF_0, Bcst::kNA, s0));       // Zero alpha.

    pc->v_or_f32(z0, z0, x0);
    pc->v_mul_f32(s0, s0, z0);

    pc->v_add_f32(d0, d0, s0);
    pc->v_mul_f32(d0, d0, pc->simdConst(&ct.f32_255, Bcst::k32, d0));

    pc->v_cvt_round_f32_to_i32(d0, d0);
    pc->v_packs_i32_i16(d0, d0, d0);
    pc->v_packs_i16_u8(d0, d0, d0);
    out.pc.init(d0);

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Difference
  // -------------------------------

  if (isDifference()) {
    // Dca' = Dca + Sca - 2.min(Sca.Da, Dca.Sa)
    // Da'  = Da  + Sa  -   min(Sa .Da, Da .Sa)
    if (!hasMask) {
      srcFetch(s, n, PixelFlags::kUC | PixelFlags::kUA, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

      VecArray& sv = s.uc;
      VecArray& uv = s.ua;
      VecArray& dv = d.uc;

      // SPLIT.
      for (unsigned int i = 0; i < kSplit; i++) {
        VecArray sh = sv.even_odd(i);
        VecArray uh = uv.even_odd(i);
        VecArray dh = dv.even_odd(i);
        VecArray xh = xv.even_odd(i);

        pc->v_expand_alpha_16(xh, dh, kUseHi);
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
      srcFetch(s, n, PixelFlags::kUC, predicate);
      dstFetch(d, n, PixelFlags::kUC, predicate);

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

        pc->v_expand_alpha_16(yh, sh, kUseHi);
        pc->v_expand_alpha_16(xh, dh, kUseHi);
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

    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Exclusion
  // ------------------------------

  if (isExclusion()) {
    // Dca' = Dca + Sca - 2.Sca.Dca
    // Da'  = Da + Sa - Sa.Da
    srcFetch(s, n, PixelFlags::kUC | (hasMask ? PixelFlags::kNone : PixelFlags::kImmutable), predicate);
    dstFetch(d, n, PixelFlags::kUC, predicate);

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
    FetchUtils::satisfyPixels(pc, out, flags);
    return;
  }

  // VMaskProc - RGBA32 - Invalid
  // ----------------------------

  BL_NOT_REACHED();
}

static void CompOpPart_negateMask(CompOpPart* self, VecArray& vn, const VecArray& vm) noexcept {
  PipeCompiler* pc = self->pc;

  switch (self->coverageFormat()) {
    case PixelCoverageFormat::kPacked:
      pc->v_not_u32(vn, vm);
      break;

    case PixelCoverageFormat::kUnpacked:
      pc->v_inv255_u16(vn, vm);
      break;

    default:
      BL_NOT_REACHED();
  }
}

void CompOpPart::vMaskProcRGBA32InvertMask(VecArray& vn, const VecArray& vm, PixelCoverageFlags coverageFlags) noexcept {
  blUnused(coverageFlags);
  uint32_t size = vm.size();

  if (cMaskLoopType() == CMaskLoopType::kVariant) {
    if (_mask->vn.isValid()) {
      bool ok = true;

      // TODO: [JIT] A leftover from a template-based code, I don't understand
      // it anymore and it seems it's unnecessary so verify this and all places
      // that hit `ok == false`.
      for (uint32_t i = 0; i < blMin(vn.size(), size); i++)
        if (vn[i].id() != vm[i].id())
          ok = false;

      if (ok) {
        vn.init(_mask->vn.cloneAs(vm[0]));
        return;
      }
    }
  }

  if (vn.empty())
    pc->newVecArray(vn, size, vm[0], "vn");

  CompOpPart_negateMask(this, vn, vm);
}

void CompOpPart::vMaskProcRGBA32InvertDone(VecArray& vn, const VecArray& vm, PixelCoverageFlags coverageFlags) noexcept {
  if (!blTestFlag(coverageFlags, PixelCoverageFlags::kImmutable))
    return;

  // The inverted mask must be the same, masks cannot be empty as this is called after `vMaskProcRGBA32InvertMask()`.
  BL_ASSERT(!vn.empty());
  BL_ASSERT(!vm.empty());
  BL_ASSERT(vn.size() == vm.size());

  if (vn[0].id() != vm[0].id())
    return;

  CompOpPart_negateMask(this, vn, vn);
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
