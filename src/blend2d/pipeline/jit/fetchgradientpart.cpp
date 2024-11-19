// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchgradientpart_p.h"
#include "../../pipeline/jit/fetchutilspixelaccess_p.h"
#include "../../pipeline/jit/fetchutilspixelgather_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

#define REL_GRADIENT(FIELD) BL_OFFSET_OF(FetchData::Gradient, FIELD)

// bl::Pipeline::JIT::GradientDitheringContext
// ===========================================

static void rotateDitherBytesRight(PipeCompiler* pc, const Vec& vec, const Gp& count) noexcept {
  Gp countAsIndex = pc->gpz(count);

#if defined(BL_JIT_ARCH_X86)
  if (!pc->hasSSSE3()) {
    Mem lo = pc->tmpStack(PipeCompiler::StackId::kCustom, 32);
    Mem hi = lo.cloneAdjusted(16);

    pc->v_storea128(lo, vec);
    pc->v_storea128(hi, vec);

    Mem rotated = lo;
    rotated.setIndex(countAsIndex);
    pc->v_loadu128(vec, rotated);

    return;
  }
#endif

  Mem mPred = pc->simdMemConst(pc->ct.swizu8_rotate_right, Bcst::kNA, vec);

#if defined(BL_JIT_ARCH_X86)
  mPred.setIndex(countAsIndex);
  if (!pc->hasAVX()) {
    Vec vPred = pc->newSimilarReg(vec, "@vPred");
    pc->v_loadu128(vPred, mPred);
    pc->v_swizzlev_u8(vec, vec, vPred);
    return;
  }
#else
  Gp base = pc->newGpPtr("@swizu8_rotate_base");
  pc->cc->loadAddressOf(base, mPred);
  mPred = mem_ptr(base, countAsIndex);
#endif

  pc->v_swizzlev_u8(vec, vec, mPred);
}

void GradientDitheringContext::initY(const PipeFunction& fn, const Gp& x, const Gp& y) noexcept {
  _dmPosition = pc->newGp32("dm.position");
  _dmOriginX = pc->newGp32("dm.originX");
  _dmValues = pc->newVec(pc->vecWidth(), "dm.values");
  _isRectFill = x.isValid();

  pc->load_u32(_dmPosition, mem_ptr(fn.ctxData(), BL_OFFSET_OF(ContextData, pixelOrigin.y)));
  pc->load_u32(_dmOriginX, mem_ptr(fn.ctxData(), BL_OFFSET_OF(ContextData, pixelOrigin.x)));

  pc->add(_dmPosition, _dmPosition, y.r32());
  if (isRectFill())
    pc->add(_dmOriginX, _dmOriginX, x.r32());

  pc->and_(_dmPosition, _dmPosition, 15);
  if (isRectFill())
    pc->and_(_dmOriginX, _dmOriginX, 15);

  pc->shl(_dmPosition, _dmPosition, 5);
  if (isRectFill())
    pc->add(_dmPosition, _dmPosition, _dmOriginX);
}

void GradientDitheringContext::advanceY() noexcept {
  pc->add(_dmPosition, _dmPosition, 16 * 2);
  pc->and_(_dmPosition, _dmPosition, 16 * 16 * 2 - 1);
}

void GradientDitheringContext::startAtX(const Gp& x) noexcept {
  Gp dmPosition = _dmPosition;

  if (!isRectFill()) {
    // If not rectangular, we have to calculate the final position according to `x`.
    dmPosition = pc->newGp32("dm.finalPosition");

    pc->mov(dmPosition, _dmOriginX);
    pc->add(dmPosition, dmPosition, x.r32());
    pc->and_(dmPosition, dmPosition, 15);
    pc->add(dmPosition, dmPosition, _dmPosition);
  }

  Mem m;
#if defined(BL_JIT_ARCH_X86)
  if (pc->is32Bit()) {
    m = x86::ptr(uint64_t(uintptr_t(commonTable.bayerMatrix16x16)), dmPosition);
  }
  else {
    pc->_initCommonTablePtr();
    m = mem_ptr(pc->_commonTablePtr, dmPosition.r64(), 0, -pc->_commonTableOff);
  }
#else
  pc->_initCommonTablePtr();
  Gp ditherRow = pc->newGpPtr("@ditherRow");
  pc->add(ditherRow, pc->_commonTablePtr, -pc->_commonTableOff);
  m = mem_ptr(ditherRow, dmPosition.r64());
#endif

  if (_dmValues.isVec128())
    pc->v_loadu128(_dmValues, m);
  else
    pc->v_broadcast_v128_u32(_dmValues, m);
}

void GradientDitheringContext::advanceX(const Gp& x, const Gp& diff, bool diffWithinBounds) noexcept {
  blUnused(x);

  if (diffWithinBounds) {
    rotateDitherBytesRight(pc, _dmValues, diff);
  }
  else {
    Gp diff0To15 = pc->newSimilarReg(diff, "@diff0To15");
    pc->and_(diff0To15, diff, 0xF);
    rotateDitherBytesRight(pc, _dmValues, diff0To15);
  }
}

void GradientDitheringContext::advanceXAfterFetch(uint32_t n) noexcept {
  // The compiler would optimize this to a cheap shuffle whenever possible.
  pc->v_alignr_u128(_dmValues, _dmValues, _dmValues, n & 15);
}

void GradientDitheringContext::ditherUnpackedPixels(Pixel& p, AdvanceMode advanceMode) noexcept {
  VecWidth vecWidth = VecWidthUtils::vecWidthOf(p.uc[0]);

  Operand shufflePredicate = pc->simdConst(&commonTable.swizu8_dither_rgba64_lo, Bcst::kNA_Unique, vecWidth);
  Vec ditherPredicate = pc->newSimilarReg(p.uc[0], "ditherPredicate");
  Vec ditherThreshold = pc->newSimilarReg(p.uc[0], "ditherThreshold");

  Vec dmValues = _dmValues;

  switch (p.count().value()) {
    case 1: {
#if defined(BL_JIT_ARCH_X86)
      if (!pc->hasSSSE3()) {
        pc->v_interleave_lo_u8(ditherPredicate, dmValues, pc->simdConst(&commonTable.i_0000000000000000, Bcst::kNA, ditherPredicate));
        pc->v_swizzle_lo_u16x4(ditherPredicate, ditherPredicate, swizzle(0, 0, 0, 0));
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        pc->v_swizzlev_u8(ditherPredicate, dmValues.cloneAs(ditherPredicate), shufflePredicate);
      }

      pc->v_swizzle_lo_u16x4(ditherThreshold, p.uc[0], swizzle(3, 3, 3, 3));
      pc->v_adds_u16(p.uc[0], p.uc[0], ditherPredicate);
      pc->v_min_u16(p.uc[0], p.uc[0], ditherThreshold);
      pc->v_srli_u16(p.uc[0], p.uc[0], 8);

      if (advanceMode == AdvanceMode::kAdvance) {
        advanceXAfterFetch(1);
      }
      break;
    }

    case 4:
    case 8:
    case 16: {
#if defined(BL_JIT_ARCH_X86)
      if (!p.uc[0].isVec128()) {
        for (uint32_t i = 0; i < p.uc.size(); i++) {
          // At least AVX2: VPSHUFB is available...
          pc->v_swizzlev_u8(ditherPredicate, dmValues.cloneAs(ditherPredicate), shufflePredicate);
          pc->v_expand_alpha_16(ditherThreshold, p.uc[i]);
          pc->v_adds_u16(p.uc[i], p.uc[i], ditherPredicate);
          pc->v_min_u16(p.uc[i], p.uc[i], ditherThreshold);

          Swizzle4 swiz = p.uc[0].isVec256() ? swizzle(0, 3, 2, 1) : swizzle(1, 0, 3, 2);

          if (advanceMode == AdvanceMode::kNoAdvance) {
            if (i + 1 == p.uc.size()) {
              break;
            }

            if (dmValues.id() == _dmValues.id()) {
              dmValues = pc->newSimilarReg(ditherPredicate, "dm.local");
              pc->v_swizzle_u32x4(dmValues, _dmValues.cloneAs(dmValues), swiz);
              continue;
            }
          }

          pc->v_swizzle_u32x4(dmValues, dmValues, swiz);
        }
        pc->v_srli_u16(p.uc, p.uc, 8);
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        for (uint32_t i = 0; i < p.uc.size(); i++) {
          Vec dm = (i == 0) ? dmValues.cloneAs(ditherPredicate) : ditherPredicate;

#if defined(BL_JIT_ARCH_X86)
          if (!pc->hasSSSE3()) {
            pc->v_interleave_lo_u8(ditherPredicate, dm, pc->simdConst(&commonTable.i_0000000000000000, Bcst::kNA, ditherPredicate));
            pc->v_interleave_lo_u16(ditherPredicate, ditherPredicate, ditherPredicate);
            pc->v_swizzle_u32x4(ditherPredicate, ditherPredicate, swizzle(1, 1, 0, 0));
          }
          else
#endif // BL_JIT_ARCH_X86
          {
            pc->v_swizzlev_u8(ditherPredicate, dm, shufflePredicate);
          }

          pc->v_expand_alpha_16(ditherThreshold, p.uc[i]);
          pc->v_adds_u16(p.uc[i], p.uc[i], ditherPredicate);

          if (i + 1u < p.uc.size())
            pc->v_swizzle_lo_u16x4(ditherPredicate, dmValues.cloneAs(ditherPredicate), swizzle(0, 3, 2, 1));

          pc->v_min_u16(p.uc[i], p.uc[i], ditherThreshold);
        }

        if (advanceMode == AdvanceMode::kAdvance) {
          Swizzle4 swiz = p.count().value() == 4 ? swizzle(0, 3, 2, 1) : swizzle(1, 0, 3, 2);
          pc->v_swizzle_u32x4(dmValues, dmValues, swiz);
        }

        pc->v_srli_u16(p.uc, p.uc, 8);
      }
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::JIT::FetchGradientPart - Construction & Destruction
// =================================================================

FetchGradientPart::FetchGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchPart(pc, fetchType, format),
    _ditheringContext(pc) {}

void FetchGradientPart::fetchSinglePixel(Pixel& dst, PixelFlags flags, const Gp& idx) noexcept {
  Mem src = mem_ptr(_tablePtr, idx, tablePtrShift());
  if (ditheringEnabled()) {
    pc->newVecArray(dst.uc, 1, VecWidth::k128, dst.name(), "uc");
    pc->v_loadu64(dst.uc[0], src);
    _ditheringContext.ditherUnpackedPixels(dst, AdvanceMode::kAdvance);
  }
  else {
    FetchUtils::fetchPixel(pc, dst, flags, PixelFetchInfo(FormatExt::kPRGB32), src);
  }
}

void FetchGradientPart::fetchMultiplePixels(Pixel& dst, PixelCount n, PixelFlags flags, const Vec& idx, FetchUtils::IndexLayout indexLayout, GatherMode mode, InterleaveCallback cb, void* cbData) noexcept {
  Mem src = mem_ptr(_tablePtr);
  uint32_t idxShift = tablePtrShift();

  if (ditheringEnabled()) {
    dst.setType(PixelType::kRGBA64);
    FetchUtils::gatherPixels(pc, dst, n, PixelFlags::kUC, PixelFetchInfo(FormatExt::kPRGB64), src, idx, idxShift, indexLayout, mode, cb, cbData);
    _ditheringContext.ditherUnpackedPixels(dst, mode == GatherMode::kFetchAll ? AdvanceMode::kAdvance : AdvanceMode::kNoAdvance);

    dst.setType(PixelType::kRGBA32);
    FetchUtils::satisfyPixels(pc, dst, flags);
  }
  else {
    FetchUtils::gatherPixels(pc, dst, n, flags, fetchInfo(), src, idx, idxShift, indexLayout, mode, cb, cbData);
  }
}

// bl::Pipeline::JIT::FetchLinearGradientPart - Construction & Destruction
// =======================================================================

FetchLinearGradientPart::FetchLinearGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchGradientPart(pc, fetchType, format) {

  bool dither = false;
  switch (fetchType) {
    case FetchType::kGradientLinearNNPad:
      _extendMode = ExtendMode::kPad;
      break;

    case FetchType::kGradientLinearNNRoR:
      _extendMode = ExtendMode::kRoR;
      break;

    case FetchType::kGradientLinearDitherPad:
      _extendMode = ExtendMode::kPad;
      dither = true;
      break;

    case FetchType::kGradientLinearDitherRoR:
      _extendMode = ExtendMode::kRoR;
      dither = true;
      break;

    default:
      BL_NOT_REACHED();
  }

  _maxVecWidthSupported = VecWidth::kMaxPlatformWidth;

  addPartFlags(PipePartFlags::kExpensive |
               PipePartFlags::kMaskedAccess |
               PipePartFlags::kAdvanceXNeedsDiff);
  setDitheringEnabled(dither);
  JitUtils::resetVarStruct(&f, sizeof(f));
}

// bl::Pipeline::JIT::FetchLinearGradientPart - Prepare
// ====================================================

void FetchLinearGradientPart::preparePart() noexcept {
#if defined(BL_JIT_ARCH_X86)
  _maxPixels = int8_t(pc->hasSSSE3() ? 8 : 4);
#else
  _maxPixels = 8;
#endif
}

// bl::Pipeline::JIT::FetchLinearGradientPart - Init & Fini
// ========================================================

void FetchLinearGradientPart::_initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept {
  VecWidth vw = vecWidth();

  // Local Registers
  // ---------------

  _tablePtr = pc->newGpPtr("f.table");                // Reg.
  f->pt = pc->newVec(vw, "f.pt");                     // Reg.
  f->dt = pc->newVec(vw, "f.dt");                     // Reg/Mem.
  f->dtN = pc->newVec(vw, "f.dtN");                   // Reg/Mem.
  f->py = pc->newVec(vw, "f.py");                     // Reg/Mem.
  f->dy = pc->newVec(vw, "f.dy");                     // Reg/Mem.
  f->maxi = pc->newVec(vw, "f.maxi");                 // Reg/Mem.
  f->rori = pc->newVec(vw, "f.rori");                 // Reg/Mem [RoR only].
  f->vIdx = pc->newVec(vw, "f.vIdx");                 // Reg/Tmp.

  // In 64-bit mode it's easier to use IMUL for 64-bit multiplication instead of SIMD, because
  // we need to multiply a scalar anyway that we then broadcast and add to our 'f.pt' vector.
  if (pc->is64Bit()) {
    f->dtGp = pc->newGp64("f.dtGp");                  // Reg/Mem.
  }

  // Part Initialization
  // -------------------

  pc->load(_tablePtr, mem_ptr(fn.fetchData(), REL_GRADIENT(lut.data)));

  if (ditheringEnabled())
    _ditheringContext.initY(fn, x, y);

  pc->s_mov_u32(f->py, y);
  pc->v_broadcast_u64(f->dy, mem_ptr(fn.fetchData(), REL_GRADIENT(linear.dy.u64)));
  pc->v_broadcast_u64(f->py, f->py);
  pc->v_mul_u64_lo_u32(f->py, f->dy, f->py);
  pc->v_broadcast_u64(f->dt, mem_ptr(fn.fetchData(), REL_GRADIENT(linear.dt.u64)));

  if (isPad()) {
    pc->v_broadcast_u16(f->maxi, mem_ptr(fn.fetchData(), REL_GRADIENT(linear.maxi)));
  }
  else {
    pc->v_broadcast_u32(f->maxi, mem_ptr(fn.fetchData(), REL_GRADIENT(linear.maxi)));
    pc->v_broadcast_u16(f->rori, mem_ptr(fn.fetchData(), REL_GRADIENT(linear.rori)));
  }

  pc->v_loadu128(f->pt, mem_ptr(fn.fetchData(), REL_GRADIENT(linear.pt)));
  pc->v_slli_i64(f->dtN, f->dt, 1u);

#if defined(BL_JIT_ARCH_X86)
  if (pc->use256BitSimd()) {
    cc->vperm2i128(f->dtN, f->dtN, f->dtN, perm2x128Imm(Perm2x128::kALo, Perm2x128::kZero));
    cc->vperm2i128(f->pt, f->pt, f->pt, perm2x128Imm(Perm2x128::kALo, Perm2x128::kALo));
    pc->v_add_i64(f->pt, f->pt, f->dtN);
    pc->v_slli_i64(f->dtN, f->dt, 2u);
  }
#endif // BL_JIT_ARCH_X86

  pc->v_add_i64(f->py, f->py, f->pt);

#if defined(BL_JIT_ARCH_X86)
  // If we cannot use PACKUSDW, which was introduced by SSE4.1 we subtract 32768 from the pointer
  // and use PACKSSDW instead. However, if we do this, we have to adjust everything else accordingly.
  if (isPad() && !pc->hasSSE4_1()) {
    pc->v_sub_i32(f->py, f->py, pc->simdConst(&ct.i_0000800000000000, Bcst::k32, f->py));
    pc->v_sub_i16(f->maxi, f->maxi, pc->simdConst(&ct.i_8000800080008000, Bcst::kNA, f->maxi));
  }
#endif // BL_JIT_ARCH_X86

  if (pc->is64Bit())
    pc->s_mov_u64(f->dtGp, f->dt);

  if (isRectFill()) {
    Vec adv = pc->newSimilarReg(f->dt, "f.adv");
    calcAdvanceX(adv, x);
    pc->v_add_i64(f->py, f->py, adv);
  }

  if (pixelGranularity() > 1)
    enterN();
}

void FetchLinearGradientPart::_finiPart() noexcept {}

// bl::Pipeline::JIT::FetchLinearGradientPart - Advance
// ====================================================

void FetchLinearGradientPart::advanceY() noexcept {
  pc->v_add_i64(f->py, f->py, f->dy);

  if (ditheringEnabled())
    _ditheringContext.advanceY();
}

void FetchLinearGradientPart::startAtX(const Gp& x) noexcept {
  if (!isRectFill()) {
    calcAdvanceX(f->pt, x);
    pc->v_add_i64(f->pt, f->pt, f->py);
  }
  else {
    pc->v_mov(f->pt, f->py);
  }

  if (ditheringEnabled())
    _ditheringContext.startAtX(x);
}

void FetchLinearGradientPart::advanceX(const Gp& x, const Gp& diff) noexcept {
  advanceX(x, diff, false);
}

void FetchLinearGradientPart::advanceX(const Gp& x, const Gp& diff, bool diffWithinBounds) noexcept {
  Vec adv = pc->newSimilarReg(f->pt, "f.adv");
  calcAdvanceX(adv, diff);
  pc->v_add_i64(f->pt, f->pt, adv);

  if (ditheringEnabled())
    _ditheringContext.advanceX(x, diff, diffWithinBounds);
}

void FetchLinearGradientPart::calcAdvanceX(const Vec& dst, const Gp& diff) const noexcept {
  // Use 64-bit multiply on 64-bit targets as it's much shorter than doing a vectorized 64x32 multiply.
  if (pc->is64Bit()) {
    Gp advTmp = pc->newGp64("f.advTmp");
    pc->mul(advTmp, diff.r64(), f->dtGp);
    pc->v_broadcast_u64(dst, advTmp);
  }
  else {
    pc->v_broadcast_u32(dst, diff);
    pc->v_mul_u64_lo_u32(dst, f->dt, dst);
  }
}

// bl::Pipeline::JIT::FetchLinearGradientPart - Fetch
// ==================================================

void FetchLinearGradientPart::enterN() noexcept {}
void FetchLinearGradientPart::leaveN() noexcept {}

void FetchLinearGradientPart::prefetchN() noexcept {}
void FetchLinearGradientPart::postfetchN() noexcept {}

void FetchLinearGradientPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  p.setCount(n);

  GatherMode gatherMode = predicate.gatherMode();

  switch (n.value()) {
    case 1: {
      BL_ASSERT(predicate.empty());

      Gp gIdx = pc->newGp32("f.gIdx");
      Vec vIdx = pc->newV128("f.vIdx");
      uint32_t vIdxLane = 1u + uint32_t(!isPad());

      if (isPad()) {
#if defined(BL_JIT_ARCH_X86)
        if (!pc->hasSSE4_1()) {
          pc->v_packs_i32_i16(vIdx, f->pt.v128(), f->pt.v128());
          pc->v_min_i16(vIdx, vIdx, f->maxi.v128());
          pc->v_add_i16(vIdx, vIdx, pc->simdConst(&ct.i_8000800080008000, Bcst::kNA, vIdx));
        }
        else
#endif // BL_JIT_ARCH_X86
        {
          pc->v_packs_i32_u16(vIdx, f->pt.v128(), f->pt.v128());
          pc->v_min_u16(vIdx, vIdx, f->maxi.v128());
        }
      }
      else {
        Vec vTmp = pc->newV128("f.vTmp");
        pc->v_and_i32(vIdx, f->pt.v128(), f->maxi.v128());
        pc->v_xor_i32(vTmp, vIdx, f->rori.v128());
        pc->v_min_i16(vIdx, vIdx, vTmp);
      }

      pc->v_add_i64(f->pt, f->pt, f->dt);
      pc->s_extract_u16(gIdx, vIdx, vIdxLane);
      fetchSinglePixel(p, flags, gIdx);
      FetchUtils::satisfyPixels(pc, p, flags);
      break;
    }

    case 4: {
      Vec vIdx = f->vIdx;
      Vec vTmp = pc->newSimilarReg(vIdx, "f.vTmp");
      Vec vPt = f->pt;

      if (!predicate.empty()) {
        vPt = pc->newSimilarReg(vPt, "@pt");
      }

#if defined(BL_JIT_ARCH_X86)
      if (pc->use256BitSimd()) {
        if (isPad()) {
          pc->v_packs_i32_u16(vIdx, f->pt, f->pt);
          pc->v_add_i64(vPt, f->pt, f->dtN);
          pc->v_min_u16(vIdx, vIdx, f->maxi);
        }
        else {
          pc->v_and_i32(vIdx, f->pt, f->maxi);
          pc->v_add_i64(vPt, f->pt, f->dtN);
          pc->v_and_i32(vTmp, vPt, f->maxi);
          pc->v_packs_i32_u16(vIdx, vIdx, vTmp);
          pc->v_xor_i32(vTmp, vIdx, f->rori);
          pc->v_min_u16(vIdx, vIdx, vTmp);
        }
        pc->v_swizzle_u64x4(vIdx, vIdx, swizzle(3, 1, 2, 0));

        fetchMultiplePixels(p, n, flags, vIdx.v128(), FetchUtils::IndexLayout::kUInt32Hi16, gatherMode);
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        FetchUtils::IndexLayout indexLayout = FetchUtils::IndexLayout::kUInt16;

        if (pc->hasNonDestructiveSrc()) {
          pc->v_add_i64(vTmp, f->pt, f->dtN);
          pc->v_interleave_shuffle_u32x4(vIdx, f->pt, vTmp, swizzle(3, 1, 3, 1));
          pc->v_add_i64(vPt, vTmp, f->dtN);
        }
        else {
          pc->v_mov(vIdx, f->pt);
          pc->v_add_i64(vPt, f->pt, f->dtN);
          pc->v_interleave_shuffle_u32x4(vIdx, vIdx, vPt, swizzle(3, 1, 3, 1));
          pc->v_add_i64(vPt, vPt, f->dtN);
        }

        if (isPad()) {
#if defined(BL_JIT_ARCH_X86)
          if (!pc->hasSSE4_1()) {
            pc->v_packs_i32_i16(vIdx, vIdx, vIdx);
            pc->v_min_i16(vIdx, vIdx, f->maxi);
            pc->v_add_i16(vIdx, vIdx, pc->simdConst(&ct.i_8000800080008000, Bcst::kNA, vIdx));
          }
          else
#endif // BL_JIT_ARCH_X86
          {
            pc->v_packs_i32_u16(vIdx, vIdx, vIdx);
            pc->v_min_u16(vIdx, vIdx, f->maxi);
          }
        }
        else {
          indexLayout = FetchUtils::IndexLayout::kUInt32Lo16;
          pc->v_and_i32(vIdx, vIdx, f->maxi);
          pc->v_xor_i32(vTmp, vIdx, f->rori);
          pc->v_min_i16(vIdx, vIdx, vTmp);
        }

        fetchMultiplePixels(p, n, flags, vIdx.v128(), indexLayout, gatherMode);
      }

      FetchUtils::satisfyPixels(pc, p, flags);
      break;
    }

    case 8: {
      Vec vIdx = f->vIdx;
      Vec vTmp = pc->newSimilarReg(vIdx, "f.vTmp");
      Vec vPt = f->pt;

      if (!predicate.empty()) {
        vPt = pc->newSimilarReg(vPt, "@pt");
      }

#if defined(BL_JIT_ARCH_X86)
      if (pc->vecWidth() >= VecWidth::k256) {
        if (isPad()) {
          pc->v_add_i64(vTmp, f->pt, f->dtN);
          pc->v_packs_i32_u16(vIdx, f->pt, vTmp);

          if (predicate.empty()) {
            pc->v_add_i64(vPt, vTmp, f->dtN);
          }

          pc->v_min_u16(vIdx, vIdx, f->maxi);
          pc->v_swizzle_u64x4(vIdx, vIdx, swizzle(3, 1, 2, 0));
        }
        else {
          pc->v_and_i32(vIdx, f->pt, f->maxi);
          pc->v_add_i64(vPt, f->pt, f->dtN);
          pc->v_and_i32(vTmp, vPt, f->maxi);
          pc->v_packs_i32_u16(vIdx, vIdx, vTmp);

          if (predicate.empty()) {
            pc->v_add_i64(vPt, vPt, f->dtN);
          }

          pc->v_xor_i32(vTmp, vIdx, f->rori);
          pc->v_min_u16(vIdx, vIdx, vTmp);
          pc->v_swizzle_u64x4(vIdx, vIdx, swizzle(3, 1, 2, 0));
        }

        fetchMultiplePixels(p, n, flags, vIdx, FetchUtils::IndexLayout::kUInt32Hi16, gatherMode);
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        pc->v_add_i64(vTmp, f->pt, f->dtN);
        pc->v_interleave_shuffle_u32x4(vIdx, f->pt, vTmp, swizzle(3, 1, 3, 1));
        pc->v_add_i64(vTmp, vTmp, f->dtN);
        pc->v_add_i64(vPt, vTmp, f->dtN);
        pc->v_interleave_shuffle_u32x4(vTmp, vTmp, vPt, swizzle(3, 1, 3, 1));

        if (predicate.empty()) {
          pc->v_add_i64(vPt, vPt, f->dtN);
        }

        if (isPad()) {
#if defined(BL_JIT_ARCH_X86)
          if (!pc->hasSSE4_1()) {
            pc->v_packs_i32_i16(vIdx, vIdx, vTmp);
            pc->v_min_i16(vIdx, vIdx, f->maxi);
            pc->v_add_i16(vIdx, vIdx, pc->simdConst(&ct.i_8000800080008000, Bcst::kNA, vIdx));
          }
          else
#endif // BL_JIT_ARCH_X86
          {
            pc->v_packs_i32_u16(vIdx, vIdx, vTmp);
            pc->v_min_u16(vIdx, vIdx, f->maxi);
          }
        }
        else {
          pc->v_and_i32(vIdx, vIdx, f->maxi);
          pc->v_and_i32(vTmp, vTmp, f->maxi);
          pc->v_packs_i32_i16(vIdx, vIdx, vTmp);
          pc->v_xor_i32(vTmp, vIdx, f->rori);
          pc->v_min_i16(vIdx, vIdx, vTmp);
        }

        fetchMultiplePixels(p, n, flags, vIdx, FetchUtils::IndexLayout::kUInt16, gatherMode);
      }

      FetchUtils::satisfyPixels(pc, p, flags);
      break;
    }

    default:
      BL_NOT_REACHED();
  }

  if (!predicate.empty()) {
    advanceX(pc->_gpNone, predicate.count().r32());
  }
}

// bl::Pipeline::JIT::FetchRadialGradientPart - Construction & Destruction
// =======================================================================

FetchRadialGradientPart::FetchRadialGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchGradientPart(pc, fetchType, format) {

  _maxVecWidthSupported = VecWidth::kMaxPlatformWidth;

  bool dither = false;
  switch (fetchType) {
    case FetchType::kGradientRadialNNPad:
      _extendMode = ExtendMode::kPad;
      break;

    case FetchType::kGradientRadialNNRoR:
      _extendMode = ExtendMode::kRoR;
      break;

    case FetchType::kGradientRadialDitherPad:
      _extendMode = ExtendMode::kPad;
      dither = true;
      break;

    case FetchType::kGradientRadialDitherRoR:
      _extendMode = ExtendMode::kRoR;
      dither = true;
      break;

    default:
      BL_NOT_REACHED();
  }

  addPartFlags(PipePartFlags::kAdvanceXNeedsDiff |
               PipePartFlags::kMaskedAccess |
               PipePartFlags::kExpensive);
  setDitheringEnabled(dither);
  JitUtils::resetVarStruct(&f, sizeof(f));
}

// bl::Pipeline::JIT::FetchRadialGradientPart - Prepare
// ====================================================

void FetchRadialGradientPart::preparePart() noexcept {
  VecWidth vw = vecWidth();
  _maxPixels = 4 << uint32_t(vw);
}

// bl::Pipeline::JIT::FetchRadialGradientPart - Init & Fini
// ========================================================

void FetchRadialGradientPart::_initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept {
  // Local Registers
  // ---------------

  VecWidth vw = vecWidth();

  _tablePtr = pc->newGpPtr("f.table");                // Reg.

  f->ty_tx = pc->newV128_F64("f.ty_tx");              // Mem.
  f->yy_yx = pc->newV128_F64("f.yy_yx");              // Mem.
  f->dd0_b0 = pc->newV128_F64("f.dd0_b0");            // Mem.
  f->ddy_by = pc->newV128_F64("f.ddy_by");            // Mem.

  f->vy = pc->newV128_F64("f.vy");                    // Reg/Mem.

  f->inv2a_4a = pc->newV128_F64("f.inv2a_4a");        // Reg/Mem.
  f->sqinv2a_sqfr = pc->newV128_F64("f.sqinv2a_sqfr");// Reg/Mem.

  f->d = pc->newVec(vw, "f.d");                       // Reg.
  f->b = pc->newVec(vw, "f.b");                       // Reg.
  f->dd = pc->newVec(vw, "f.dd");                     // Reg/Mem.
  f->vx = pc->newVec(vw, "f.vx");                     // Reg.
  f->value = pc->newVec(vw, "f.value");               // Reg.

  f->bd = pc->newVec(vw, "f.bd");                     // Reg/Mem.
  f->ddd = pc->newVec(vw, "f.ddd");                   // Reg/Mem.

  f->vmaxi = pc->newVec(vw, "f.vmaxi");               // Reg/Mem.

  // Part Initialization
  // -------------------

  if (ditheringEnabled())
    _ditheringContext.initY(fn, x, y);

  pc->load(_tablePtr, mem_ptr(fn.fetchData(), REL_GRADIENT(lut.data)));

  pc->v_loadu128_f64(f->ty_tx, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.tx)));
  pc->v_loadu128_f64(f->yy_yx, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.yx)));

  pc->v_loadu128_f64(f->inv2a_4a, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.amul4)));
  pc->v_loadu128_f64(f->sqinv2a_sqfr, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.sq_fr)));

  pc->v_loadu128_f64(f->dd0_b0, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.b0)));
  pc->v_loadu128_f64(f->ddy_by, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.by)));
  pc->v_broadcast_f32(f->bd, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.f32_bd)));
  pc->v_broadcast_f32(f->ddd, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.f32_ddd)));

  pc->s_cvt_int_to_f64(f->vy, y);
  pc->v_broadcast_f64(f->vy, f->vy);

  if (isPad()) {
#if defined(BL_JIT_ARCH_X86)
    if (vw > VecWidth::k128) {
      pc->v_broadcast_u32(f->vmaxi, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.maxi)));
    }
    else
#endif // BL_JIT_ARCH_X86
    {
      pc->v_broadcast_u16(f->vmaxi, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.maxi)));
    }
  }
  else {
    f->vrori = pc->newVec(vw, "f.vrori");
    pc->v_broadcast_u32(f->vmaxi, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.maxi)));
    pc->v_broadcast_u16(f->vrori, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.rori)));
  }

  if (isRectFill()) {
    f->vx_start = pc->newSimilarReg(f->vx, "f.vx_start");
    initVx(f->vx_start, x);
  }
}

void FetchRadialGradientPart::_finiPart() noexcept {}

// bl::Pipeline::JIT::FetchRadialGradientPart - Advance
// ====================================================

void FetchRadialGradientPart::advanceY() noexcept {
  pc->v_add_f64(f->vy, f->vy, pc->simdConst(&pc->ct.f64_1, Bcst::k64, f->vy));

  if (ditheringEnabled())
    _ditheringContext.advanceY();
}

void FetchRadialGradientPart::startAtX(const Gp& x) noexcept {
  Vec v0 = pc->newV128_F64("@v0");
  Vec v1 = pc->newV128_F64("@v1");
  Vec v2 = pc->newV128_F64("@v2");
  Vec v3 = pc->newV128_F64("@v3");

  pc->v_madd_f64(v1, f->vy, f->yy_yx, f->ty_tx);          // v1    = [ ty  + Y * yy      | tx + Y * yx          ] => [  py  |  px  ]
  pc->v_madd_f64(v0, f->vy, f->ddy_by, f->dd0_b0);        // v0    = [ dd0 + Y * ddy     | b0 + Y * by          ] => [  dd  |   b  ]
  pc->v_mul_f64(v1, v1, v1);                              // v1    = [ (ty + Y * yy)^2   | (tx + Y * xx) ^ 2    ] => [ py^2 | px^2 ]
  pc->s_mul_f64(v2, v0, v0);                              // v2    = [ ?                 | b^2                  ]

  pc->v_dup_hi_f64(v3, f->inv2a_4a);                      // v3    = [ 1 / 2a            | 1 / 2a               ]
  pc->v_hadd_f64(v1, v1, v1);                             // v1    = [ py^2 + px^2       | py^2 + px^2          ]

  pc->s_sub_f64(v1, v1, f->sqinv2a_sqfr);                 // v1    = [ ?                 | py^2 + px^2 - fr^2   ]
  pc->s_madd_f64(v2, v1, f->inv2a_4a, v2);                // v2    = [ ?                 |b^2+4a(py^2+px^2-fr^2)] => [ ?    | d    ]
  pc->v_combine_hi_lo_f64(v2, v0, v2);                    // v2    = [ dd                | d                    ]
  pc->s_mul_f64(v0, v0, v3);                              // v0    = [ ?                 | b * (1/2a)           ]
  pc->v_dup_hi_f64(v3, f->sqinv2a_sqfr);                  // v3    = [ (1/2a)^2          | (1/2a)^2             ]
  pc->v_mul_f64(v2, v2, v3);                              // v2    = [ dd * (1/2a)^2     | d * (1/2a)^2         ]

  pc->v_cvt_f64_to_f32_lo(f->b.v128(), v0);
  pc->v_cvt_f64_to_f32_lo(f->d.v128(), v2);

  pc->v_broadcast_f32(f->b, f->b);
  pc->v_swizzle_f32x4(f->dd, f->d, swizzle(1, 1, 1, 1));
  pc->v_broadcast_f32(f->d, f->d);
  pc->v_broadcast_f32(f->dd, f->dd);

  if (isRectFill())
    pc->v_mov(f->vx, f->vx_start);
  else
    initVx(f->vx, x);

  if (ditheringEnabled())
    _ditheringContext.startAtX(x);
}

void FetchRadialGradientPart::advanceX(const Gp& x, const Gp& diff) noexcept {
  advanceX(x, diff, false);
}

void FetchRadialGradientPart::advanceX(const Gp& x, const Gp& diff, bool diffWithinBounds) noexcept {
  VecWidth vw = vecWidth();
  Vec vd = pc->newVec(vw, "@vd");

  // `vd` is `diff` converted to f32 and broadcasted to all lanes.
  pc->s_cvt_int_to_f32(vd, diff);
  pc->v_broadcast_f32(vd, vd);
  pc->v_add_f32(f->vx, f->vx, vd);

  if (ditheringEnabled())
    _ditheringContext.advanceX(x, diff, diffWithinBounds);
}

// bl::Pipeline::JIT::FetchRadialGradientPart - Fetch
// ==================================================

void FetchRadialGradientPart::prefetchN() noexcept {
  Vec v0 = f->value;
  Vec v1 = pc->newSimilarReg(v0, "v1");

  pc->v_mul_f32(v1, f->vx, f->vx);
  pc->v_madd_f32(v0, f->dd, f->vx, f->d);
  pc->v_madd_f32(v0, f->ddd, v1, v0);
  pc->v_abs_f32(v0, v0);
  pc->v_sqrt_f32(v0, v0);
}

void FetchRadialGradientPart::postfetchN() noexcept {}

void FetchRadialGradientPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  p.setCount(n);

#if defined(BL_JIT_ARCH_X86)
  VecWidth vw = vecWidth();
#endif // BL_JIT_ARCH_X86

  GatherMode gatherMode = predicate.gatherMode();

  switch (n.value()) {
    case 1: {
      BL_ASSERT(predicate.empty());

      Gp gIdx = pc->newGpPtr("gIdx");
      Vec vIdx = pc->newV128("vIdx");
      Vec v0 = pc->newV128("v0");

      pc->v_mov(v0, f->d.v128());
      pc->s_mul_f32(vIdx, f->vx, f->vx);
      pc->s_madd_f32(v0, f->dd, f->vx, v0);
      pc->s_madd_f32(v0, f->ddd, vIdx, v0);
      pc->v_abs_f32(v0, v0);
      pc->s_sqrt_f32(v0, v0);
      pc->s_madd_f32(vIdx, f->bd, f->vx, f->b);
      pc->v_add_f32(f->vx, f->vx, pc->simdConst(&pc->ct.f32_1, Bcst::k32, f->vx));

      pc->v_add_f32(vIdx, vIdx, v0);

      pc->v_cvt_trunc_f32_to_i32(vIdx, vIdx);

      applyExtend(vIdx, vIdx, v0);

      pc->s_extract_u16(gIdx, vIdx, 0u);
      fetchSinglePixel(p, flags, gIdx);

      FetchUtils::satisfyPixels(pc, p, flags);
      break;
    }

    case 4: {
      Vec v0 = f->value;
      Vec v1 = pc->newSimilarReg(v0, "v0");
      Vec vIdx = pc->newV128("vIdx");

      pc->v_madd_f32(vIdx, f->bd.v128(), f->vx.v128(), f->b.v128());

      if (predicate.empty()) {
        pc->v_add_f32(f->vx, f->vx, pc->simdConst(&pc->ct.f32_4, Bcst::k32, f->vx));
      }

      pc->v_add_f32(vIdx, vIdx, v0.v128());
      pc->v_cvt_trunc_f32_to_i32(vIdx, vIdx);

      FetchUtils::IndexLayout indexLayout = applyExtend(vIdx, vIdx, v0.v128());

      fetchMultiplePixels(p, n, flags, vIdx, indexLayout, gatherMode, [&](uint32_t step) noexcept {
        // Don't recalculate anything if this is a predicated load as it won't be used.
        if (!predicate.empty())
          return;

        switch (step) {
          case 0:
            pc->v_madd_f32(v0, f->dd, f->vx, f->d);
            break;
          case 1:
            pc->v_mul_f32(v1, f->vx, f->vx);
            break;
          case 2:
            pc->v_madd_f32(v0, f->ddd, v1, v0);
            pc->v_abs_f32(v0, v0);
            break;
          case 3:
            pc->v_sqrt_f32(v0, v0);
            break;
          default:
            break;
        }
      });

      if (!predicate.empty()) {
        advanceX(pc->_gpNone, predicate.count(), true);
        prefetchN();
      }

      FetchUtils::satisfyPixels(pc, p, flags);
      break;
    }

    case 8: {
#if defined(BL_JIT_ARCH_X86)
      if (vw >= VecWidth::k256) {
        Vec v0 = f->value;
        Vec v1 = pc->newSimilarReg(v0, "v1");
        Vec vIdx = pc->newSimilarReg(v0, "vIdx");

        pc->v_madd_f32(vIdx, f->bd, f->vx, f->b);

        if (predicate.empty()) {
          pc->v_add_f32(f->vx, f->vx, pc->simdConst(&pc->ct.f32_8, Bcst::k32, f->vx));
        }

        pc->v_add_f32(vIdx, vIdx, v0);
        pc->v_cvt_trunc_f32_to_i32(vIdx, vIdx);

        FetchUtils::IndexLayout indexLayout = applyExtend(vIdx, vIdx, v0);

        if (predicate.empty()) {
          pc->v_mov(v0, f->d);
          pc->v_mul_f32(v1, f->vx, f->vx);
        }

        fetchMultiplePixels(p, n, flags, vIdx, indexLayout, gatherMode, [&](uint32_t step) noexcept {
          // Don't recalculate anything if this is a predicated load as it won't be used.
          if (!predicate.empty())
            return;

          switch (step) {
            case 0:
              pc->v_madd_f32(v0, f->dd, f->vx, v0);
              break;
            case 1:
              pc->v_madd_f32(v0, f->ddd, v1, v0);
              break;
            case 2:
              pc->v_abs_f32(v0, v0);
              break;
            case 3:
              pc->v_sqrt_f32(v0, v0);
              break;
            default:
              break;
          }
        });

        if (!predicate.empty()) {
          advanceX(pc->_gpNone, predicate.count(), true);
          prefetchN();
        }

        FetchUtils::satisfyPixels(pc, p, flags);
        break;
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        Vec v0 = f->value;
        Vec vTmp = pc->newV128("v0");
        Vec vIdx0 = pc->newV128("vIdx0");
        Vec vIdx1 = pc->newV128("vIdx1");

        pc->v_add_f32(vTmp, f->vx, pc->simdConst(&pc->ct.f32_4, Bcst::k32, f->vx));
        pc->v_madd_f32(vIdx1, f->dd, vTmp, f->d);
        pc->v_madd_f32(vIdx0, f->bd.v128(), f->vx.v128(), f->b.v128());

        if (predicate.empty()) {
          pc->v_add_f32(f->vx, vTmp, pc->simdConst(&pc->ct.f32_4, Bcst::k32, f->vx));
        }

        pc->v_mul_f32(vTmp, vTmp, vTmp);
        pc->v_madd_f32(vIdx1, f->ddd, vTmp, vIdx1);
        pc->v_abs_f32(vIdx1, vIdx1);
        pc->v_sqrt_f32(vIdx1, vIdx1);

        pc->v_add_f32(vIdx0, vIdx0, v0.v128());
        pc->v_cvt_trunc_f32_to_i32(vIdx0, vIdx0);
        pc->v_cvt_trunc_f32_to_i32(vIdx1, vIdx1);

        FetchUtils::IndexLayout indexLayout = applyExtend(vIdx0, vIdx1, vTmp);

        fetchMultiplePixels(p, n, flags, vIdx0, indexLayout, gatherMode, [&](uint32_t step) noexcept {
          // Don't recalculate anything if this is a predicated load as it won't be used.
          if (!predicate.empty())
            return;

          switch (step) {
            case 0:
              pc->v_madd_f32(v0, f->dd, f->vx, f->d);
              break;
            case 1:
              pc->v_mul_f32(vTmp, f->vx, f->vx);
              break;
            case 2:
              pc->v_madd_f32(v0, f->ddd, vTmp, v0);
              pc->v_abs_f32(v0, v0);
              break;
            case 3:
              pc->v_sqrt_f32(v0, v0);
              break;
            default:
              break;
          }
        });

        if (!predicate.empty()) {
          advanceX(pc->_gpNone, predicate.count(), true);
          prefetchN();
        }

        FetchUtils::satisfyPixels(pc, p, flags);
        break;
      }

      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void FetchRadialGradientPart::initVx(const Vec& vx, const Gp& x) noexcept {
  Mem increments = pc->simdMemConst(&ct.f32_increments, Bcst::kNA_Unique, vx);
  pc->s_cvt_int_to_f32(vx, x);
  pc->v_broadcast_f32(vx, vx);
  pc->v_add_f32(vx, vx, increments);
}

FetchUtils::IndexLayout FetchRadialGradientPart::applyExtend(const Vec& idx0, const Vec& idx1, const Vec& tmp) noexcept {
  if (isPad()) {
#if defined(BL_JIT_ARCH_X86)
    if (!pc->hasSSE4_1()) {
      pc->v_packs_i32_i16(idx0, idx0, idx1);
      pc->v_min_i16(idx0, idx0, f->vmaxi);
      pc->v_max_i16(idx0, idx0, pc->simdConst(&ct.i_0000000000000000, Bcst::kNA, idx0));
      return FetchUtils::IndexLayout::kUInt16;
    }

    if (vecWidth() > VecWidth::k128) {
      // Must be the same when using AVX2 vectors (256-bit and wider).
      BL_ASSERT(idx0.id() == idx1.id());

      pc->v_max_i32(idx0, idx0, pc->simdConst(&ct.i_0000000000000000, Bcst::kNA, idx0));
      pc->v_min_u32(idx0, idx0, f->vmaxi.cloneAs(idx0));
      return FetchUtils::IndexLayout::kUInt32Lo16;
    }
#endif // BL_JIT_ARCH_X86

    pc->v_packs_i32_u16(idx0, idx0, idx1);
    pc->v_min_u16(idx0, idx0, f->vmaxi.cloneAs(idx0));
    return FetchUtils::IndexLayout::kUInt16;
  }
  else if (idx0.id() == idx1.id()) {
    pc->v_and_i32(idx0, idx0, f->vmaxi.cloneAs(idx0));
    pc->v_xor_i32(tmp, idx0, f->vrori.cloneAs(idx0));
    pc->v_min_i16(idx0, idx0, tmp);
    return FetchUtils::IndexLayout::kUInt32Lo16;
  }
  else {
    pc->v_and_i32(idx0, idx0, f->vmaxi.cloneAs(idx0));
    pc->v_and_i32(idx1, idx1, f->vmaxi.cloneAs(idx1));
    pc->v_packs_i32_i16(idx0, idx0, idx1);
    pc->v_xor_i32(tmp, idx0, f->vrori.cloneAs(idx0));
    pc->v_min_i16(idx0, idx0, tmp);
    return FetchUtils::IndexLayout::kUInt16;
  }
}

// bl::Pipeline::JIT::FetchConicGradientPart - Construction & Destruction
// ======================================================================

FetchConicGradientPart::FetchConicGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchGradientPart(pc, fetchType, format) {

  _maxVecWidthSupported = VecWidth::kMaxPlatformWidth;

  addPartFlags(PipePartFlags::kMaskedAccess | PipePartFlags::kExpensive);
  setDitheringEnabled(fetchType == FetchType::kGradientConicDither);
  JitUtils::resetVarStruct(&f, sizeof(f));
}

// bl::Pipeline::JIT::FetchConicGradientPart - Prepare
// ===================================================

void FetchConicGradientPart::preparePart() noexcept {
  _maxPixels = uint8_t(4 * pc->vecMultiplier());
}

// bl::Pipeline::JIT::FetchConicGradientPart - Init & Fini
// =======================================================

void FetchConicGradientPart::_initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept {
  VecWidth vw = vecWidth(maxPixels());

  // Local Registers
  // ---------------

  _tablePtr = pc->newGpPtr("f.table");                // Reg.

  f->ty_tx = pc->newV128_F64("f.ty_tx");              // Reg/Mem.
  f->yy_yx = pc->newV128_F64("f.yy_yx");              // Reg/Mem.

  f->tx = pc->newVec(vw, "f.tx");                     // Reg/Mem.
  f->xx = pc->newVec(vw, "f.xx");                     // Reg/Mem.
  f->vx = pc->newVec(vw, "f.vx");                     // Reg.

  f->ay = pc->newVec(vw, "f.ay");                     // Reg/Mem.
  f->by = pc->newVec(vw, "f.by");                     // Reg/Mem.

  f->q_coeff = pc->newVec(vw, "f.q_coeff");           // Reg/Mem.
  f->n_coeff = pc->newVec(vw, "f.n_coeff");           // Reg/Mem.

  f->maxi = pc->newVec(vw, "f.maxi");                 // Reg/Mem.
  f->rori = pc->newVec(vw, "f.rori");                 // Reg/Mem.

  // Part Initialization
  // -------------------

  pc->load(_tablePtr, mem_ptr(fn.fetchData(), REL_GRADIENT(lut.data)));

  if (ditheringEnabled())
    _ditheringContext.initY(fn, x, y);

  pc->s_cvt_int_to_f64(f->ty_tx, y);
  pc->v_loadu128_f64(f->yy_yx, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.yx)));
  pc->v_broadcast_f64(f->ty_tx, f->ty_tx);
  pc->v_madd_f64(f->ty_tx, f->ty_tx, f->yy_yx, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.tx)));

  pc->v_broadcast_v128_f32(f->q_coeff, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.q_coeff)));
  pc->v_broadcast_v128_f32(f->n_coeff, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.n_div_1_2_4)));
  pc->v_broadcast_f32(f->xx, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.xx)));
  pc->v_broadcast_u32(f->maxi, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.maxi)));
  pc->v_broadcast_u32(f->rori, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.rori)));

  if (isRectFill()) {
    f->vx_start = pc->newSimilarReg(f->vx, "f.vx_start");
    initVx(f->vx_start, x);
  }
}

void FetchConicGradientPart::_finiPart() noexcept {}

// bl::Pipeline::JIT::FetchConicGradientPart - Advance
// ===================================================

void FetchConicGradientPart::advanceY() noexcept {
  pc->v_add_f64(f->ty_tx, f->ty_tx, f->yy_yx);

  if (ditheringEnabled())
    _ditheringContext.advanceY();
}

void FetchConicGradientPart::startAtX(const Gp& x) noexcept {
  Vec n_div_1 = pc->newSimilarReg(f->by, "@n_div_1");

  pc->v_cvt_f64_to_f32_lo(f->by.v128(), f->ty_tx);
  pc->v_swizzle_f32x4(f->tx.v128(), f->by.v128(), swizzle(0, 0, 0, 0));
  pc->v_swizzle_f32x4(f->by.v128(), f->by.v128(), swizzle(1, 1, 1, 1));

  if (!f->by.isVec128()) {
    pc->v_broadcast_v128_f32(f->tx, f->tx.v128());
    pc->v_broadcast_v128_f32(f->by, f->by.v128());
  }

  pc->v_swizzle_f32x4(n_div_1, f->n_coeff, swizzle(0, 0, 0, 0));
  pc->v_abs_f32(f->ay, f->by);
  pc->v_srai_i32(f->by, f->by, 31);
  pc->v_and_f32(f->by, f->by, n_div_1);

  if (isRectFill())
    pc->v_mov(f->vx, f->vx_start);
  else
    initVx(f->vx, x);

  if (ditheringEnabled())
    _ditheringContext.startAtX(x);
}

void FetchConicGradientPart::advanceX(const Gp& x, const Gp& diff) noexcept {
  advanceX(x, diff, false);
}

void FetchConicGradientPart::advanceX(const Gp& x, const Gp& diff, bool diffWithinBounds) noexcept {
  VecWidth vw = vecWidth(maxPixels());
  Vec vd = pc->newVec(vw, "@vd");

  // `vd` is `diff` converted to f32 and broadcasted to all lanes.
  pc->s_cvt_int_to_f32(vd, diff);
  pc->v_broadcast_f32(vd, vd);
  pc->v_add_f32(f->vx, f->vx, vd);

  if (ditheringEnabled())
    _ditheringContext.advanceX(x, diff, diffWithinBounds);
}

// bl::Pipeline::JIT::FetchConicGradientPart - Fetch
// =================================================

void FetchConicGradientPart::prefetchN() noexcept {}

void FetchConicGradientPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  p.setCount(n);

  VecWidth vw = vecWidth(n.value());
  GatherMode gatherMode = predicate.gatherMode();

  Vec ay = VecWidthUtils::cloneVecAs(f->ay, vw);
  Vec by = VecWidthUtils::cloneVecAs(f->by, vw);
  Vec tx = VecWidthUtils::cloneVecAs(f->tx, vw);
  Vec xx = VecWidthUtils::cloneVecAs(f->xx, vw);
  Vec q_coeff = VecWidthUtils::cloneVecAs(f->q_coeff, vw);
  Vec n_coeff = VecWidthUtils::cloneVecAs(f->n_coeff, vw);

  Vec t0 = pc->newVec(vw, "t0");
  Vec t1 = pc->newVec(vw, "t1");
  Vec t2 = pc->newVec(vw, "t2");
  Vec t3 = pc->newVec(vw, "t3");
  Vec t4 = pc->newVec(vw, "t4");
  Vec t5 = pc->newVec(vw, "t5");

  switch (n.value()) {
    case 1: {
      Gp idx = pc->newGpPtr("f.idx");

      pc->s_madd_f32(t0, f->vx.cloneAs(t0), xx, tx);
      pc->v_abs_f32(t1, t0);

      pc->s_max_f32(t3, t1, ay);
      pc->s_min_f32(t2, t1, ay);
      pc->s_cmp_eq_f32(t1, t1, t2);
      pc->s_div_f32(t2, t2, t3);

      pc->v_swizzle_f32x4(t4, n_coeff, swizzle(kNDiv4, kNDiv4, kNDiv4, kNDiv4));
      pc->v_srai_i32(t0, t0, 31);
      pc->v_and_f32(t1, t1, t4);
      pc->s_mul_f32(t3, t2, t2);
      pc->v_swizzle_f32x4(t5, q_coeff, swizzle(kQ3, kQ3, kQ3, kQ3));
      pc->v_swizzle_f32x4(t4, q_coeff, swizzle(kQ2, kQ2, kQ2, kQ2));

      pc->s_madd_f32(t4, t5, t3, t4);
      pc->v_swizzle_f32x4(t5, q_coeff, swizzle(kQ1, kQ1, kQ1, kQ1));
      pc->s_madd_f32(t5, t4, t3, t5);
      pc->v_swizzle_f32x4(t4, n_coeff, swizzle(kNDiv2, kNDiv2, kNDiv2, kNDiv2));
      pc->v_and_f32(t0, t0, t4);
      pc->v_swizzle_f32x4(t4, q_coeff, swizzle(kQ0, kQ0, kQ0, kQ0));
      pc->s_madd_f32(t4, t5, t3, t4);
      pc->s_msub_f32(t1, t4, t2, t1);

      pc->v_abs_f32(t1, t1);
      pc->s_sub_f32(t1, t1, t0);
      pc->v_abs_f32(t1, t1);

      pc->v_swizzle_f32x4(t4, n_coeff, swizzle(kAngleOffset, kAngleOffset, kAngleOffset, kAngleOffset));
      pc->s_sub_f32(t1, t1, by);
      pc->v_abs_f32(t1, t1);
      pc->s_add_f32(t1, t1, t4);

      pc->v_cvt_round_f32_to_i32(t1, t1);
      pc->v_min_i32(t1, t1, f->maxi.cloneAs(t1));
      pc->v_and_i32(t1, t1, f->rori.cloneAs(t1));
      pc->s_extract_u16(idx, t1, 0);

      fetchSinglePixel(p, flags, idx);
      FetchUtils::satisfyPixels(pc, p, flags);

      pc->v_add_f32(f->vx, f->vx, pc->simdConst(&pc->ct.f32_1, Bcst::k32, f->vx));
      break;
    }

    case 4:
    case 8:
    case 16: {
      pc->v_madd_f32(t0, f->vx.cloneAs(t0), xx, tx);
      pc->v_abs_f32(t1, t0);

      pc->v_max_f32(t3, t1, ay);
      pc->v_min_f32(t2, t1, ay);
      pc->v_cmp_eq_f32(t1, t1, t2);
      pc->v_div_f32(t2, t2, t3);

      pc->v_swizzle_f32x4(t4, n_coeff, swizzle(kNDiv4, kNDiv4, kNDiv4, kNDiv4));
      pc->v_srai_i32(t0, t0, 31);
      pc->v_and_f32(t1, t1, t4);
      pc->v_mul_f32(t3, t2, t2);
      pc->v_swizzle_f32x4(t5, q_coeff, swizzle(kQ3, kQ3, kQ3, kQ3));
      pc->v_swizzle_f32x4(t4, q_coeff, swizzle(kQ2, kQ2, kQ2, kQ2));

      pc->v_madd_f32(t4, t5, t3, t4);
      pc->v_swizzle_f32x4(t5, q_coeff, swizzle(kQ1, kQ1, kQ1, kQ1));
      pc->v_madd_f32(t5, t4, t3, t5);
      pc->v_swizzle_f32x4(t4, n_coeff, swizzle(kNDiv2, kNDiv2, kNDiv2, kNDiv2));
      pc->v_and_f32(t0, t0, t4);
      pc->v_swizzle_f32x4(t4, q_coeff, swizzle(kQ0, kQ0, kQ0, kQ0));
      pc->v_madd_f32(t4, t5, t3, t4);
      pc->v_msub_f32(t1, t4, t2, t1);

      pc->v_abs_f32(t1, t1);
      pc->v_sub_f32(t1, t1, t0);
      pc->v_abs_f32(t1, t1);

      pc->v_swizzle_f32x4(t4, n_coeff, swizzle(kAngleOffset, kAngleOffset, kAngleOffset, kAngleOffset));
      pc->v_sub_f32(t1, t1, by);
      pc->v_abs_f32(t1, t1);
      pc->v_add_f32(t1, t1, t4);

      pc->v_cvt_round_f32_to_i32(t1, t1);
      pc->v_min_i32(t1, t1, f->maxi.cloneAs(t1));
      pc->v_and_i32(t1, t1, f->rori.cloneAs(t1));

      fetchMultiplePixels(p, n, flags, t1, FetchUtils::IndexLayout::kUInt32Lo16, gatherMode);

      if (predicate.empty()) {
        if (n == 4)
          pc->v_add_f32(f->vx, f->vx, pc->simdConst(&pc->ct.f32_4, Bcst::k32, f->vx));
        else if (n == 8)
          pc->v_add_f32(f->vx, f->vx, pc->simdConst(&pc->ct.f32_8, Bcst::k32, f->vx));
        else if (n == 16)
          pc->v_add_f32(f->vx, f->vx, pc->simdConst(&pc->ct.f32_16, Bcst::k32, f->vx));
      }
      else {
        advanceX(pc->_gpNone, predicate.count(), true);
      }

      FetchUtils::satisfyPixels(pc, p, flags);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void FetchConicGradientPart::initVx(const Vec& vx, const Gp& x) noexcept {
  Mem increments = pc->simdMemConst(&ct.f32_increments, Bcst::kNA_Unique, vx);
  pc->s_cvt_int_to_f32(vx, x);
  pc->v_broadcast_f32(vx, vx);
  pc->v_add_f32(vx, vx, increments);
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
