// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchgradientpart_p.h"
#include "../../pipeline/jit/fetchutils_p.h"
#include "../../pipeline/jit/fetchutilspixelaccess_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

#include "../../pipeline/jit/pipedebug_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

#define REL_GRADIENT(FIELD) BL_OFFSET_OF(FetchData::Gradient, FIELD)

// bl::Pipeline::JIT::GradientDitheringContext
// ===========================================

void GradientDitheringContext::initY(const PipeFunction& fn, const Gp& x, const Gp& y) noexcept {
  _dmPosition = pc->newGp32("dm.position");
  _dmOriginX = pc->newGp32("dm.originX");
  _dmValues = pc->newVec(pc->simdWidth(), "dm.values");

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
#if defined BL_JIT_ARCH_X86
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

void GradientDitheringContext::advanceX(const Gp& x, const Gp& diff) noexcept {
  // FillRect never advance X as that would mean that there is a hole, which is impossible.
  BL_ASSERT(!isRectFill());

  blUnused(diff);
  startAtX(x);
}

void GradientDitheringContext::advanceXAfterFetch(uint32_t n) noexcept {
  // The compiler would optimize this to a cheap shuffle whenever possible.
  pc->v_alignr_u128(_dmValues, _dmValues, _dmValues, n & 15);
}

void GradientDitheringContext::ditherUnpackedPixels(Pixel& p) noexcept {
  SimdWidth simdWidth = SimdWidthUtils::simdWidthOf(p.uc[0]);

  Operand shufflePredicate = pc->simdConst(&commonTable.pshufb_dither_rgba64_lo, Bcst::kNA_Unique, simdWidth);
  Vec ditherPredicate = pc->newSimilarReg(p.uc[0], "ditherPredicate");
  Vec ditherThreshold = pc->newSimilarReg(p.uc[0], "ditherThreshold");

  switch (p.count().value()) {
    case 1: {
#if defined(BL_JIT_ARCH_X86)
      if (!pc->hasSSSE3()) {
        pc->v_interleave_lo_u8(ditherPredicate, _dmValues, pc->simdConst(&commonTable.i_0000000000000000, Bcst::kNA, ditherPredicate));
        pc->v_swizzle_lo_u16x4(ditherPredicate, ditherPredicate, swizzle(0, 0, 0, 0));
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        pc->v_swizzlev_u8(ditherPredicate, _dmValues.cloneAs(ditherPredicate), shufflePredicate);
      }

      pc->v_swizzle_lo_u16x4(ditherThreshold, p.uc[0], swizzle(3, 3, 3, 3));
      pc->v_adds_u16(p.uc[0], p.uc[0], ditherPredicate);
      pc->v_min_u16(p.uc[0], p.uc[0], ditherThreshold);
      pc->v_srli_u16(p.uc[0], p.uc[0], 8);
      advanceXAfterFetch(1);
      break;
    }

    case 4:
    case 8:
    case 16: {
#if defined(BL_JIT_ARCH_X86)
      if (!p.uc[0].isXmm()) {
        for (uint32_t i = 0; i < p.uc.size(); i++) {
          // At least AVX2: VPSHUFB is available...
          pc->v_swizzlev_u8(ditherPredicate, _dmValues.cloneAs(ditherPredicate), shufflePredicate);
          pc->v_expand_alpha_16(ditherThreshold, p.uc[i]);
          pc->v_adds_u16(p.uc[i], p.uc[i], ditherPredicate);
          pc->v_min_u16(p.uc[i], p.uc[i], ditherThreshold);

          if (p.uc[0].isYmm())
            pc->v_swizzle_u32x4(_dmValues, _dmValues, swizzle(0, 3, 2, 1));
          else
            pc->v_swizzle_u32x4(_dmValues, _dmValues, swizzle(1, 0, 3, 2));
        }
        pc->v_srli_u16(p.uc, p.uc, 8);
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        for (uint32_t i = 0; i < p.uc.size(); i++) {
          Vec dm = (i == 0) ? _dmValues.cloneAs(ditherPredicate) : ditherPredicate;

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
            pc->v_swizzle_lo_u16x4(ditherPredicate, _dmValues.cloneAs(ditherPredicate), swizzle(0, 3, 2, 1));

          pc->v_min_u16(p.uc[i], p.uc[i], ditherThreshold);
        }

        if (p.count().value() == 4)
          pc->v_swizzle_u32x4(_dmValues, _dmValues, swizzle(0, 3, 2, 1));
        else
          pc->v_swizzle_u32x4(_dmValues, _dmValues, swizzle(1, 0, 3, 2));

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
    _ditheringContext(pc) {

  _partFlags |= PipePartFlags::kAdvanceXNeedsDiff;
}

void FetchGradientPart::fetchSinglePixel(Pixel& dst, PixelFlags flags, const Gp& idx) noexcept {
  Mem src = mem_ptr(_tablePtr, idx, tablePtrShift());
  if (ditheringEnabled()) {
    pc->newVecArray(dst.uc, 1, SimdWidth::k128, dst.name(), "uc");
    pc->v_loadu64(dst.uc[0], src);
    _ditheringContext.ditherUnpackedPixels(dst);
  }
  else {
    FetchUtils::x_fetch_pixel(pc, dst, PixelCount(1), flags, FormatExt::kPRGB32, src, Alignment(4));
  }
}

void FetchGradientPart::fetchMultiplePixels(Pixel& dst, PixelCount n, PixelFlags flags, const Vec& idx, FetchUtils::IndexLayout indexLayout, InterleaveCallback cb, void* cbData) noexcept {
  Mem src = mem_ptr(_tablePtr);
  uint32_t idxShift = tablePtrShift();

  if (ditheringEnabled()) {
    dst.setType(PixelType::kRGBA64);
    FetchUtils::gatherPixels(pc, dst, n, FormatExt::kPRGB64, PixelFlags::kUC, src, idx, idxShift, indexLayout, cb, cbData);
    _ditheringContext.ditherUnpackedPixels(dst);

    dst.setType(PixelType::kRGBA32);
    FetchUtils::x_satisfy_pixel(pc, dst, flags);
  }
  else {
    FetchUtils::gatherPixels(pc, dst, n, format(), flags, src, idx, idxShift, indexLayout, cb, cbData);
  }
}

// bl::Pipeline::JIT::FetchLinearGradientPart - Construction & Destruction
// =======================================================================

FetchLinearGradientPart::FetchLinearGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchGradientPart(pc, fetchType, format) {

  bool dither = false;
  switch (fetchType) {
    case FetchType::kGradientLinearNNPad: _extendMode = ExtendMode::kPad; break;
    case FetchType::kGradientLinearNNRoR: _extendMode = ExtendMode::kRoR; break;
    case FetchType::kGradientLinearDitherPad: _extendMode = ExtendMode::kPad; dither = true; break;
    case FetchType::kGradientLinearDitherRoR: _extendMode = ExtendMode::kRoR; dither = true; break;
    default:
      BL_NOT_REACHED();
  }

#if defined(BL_JIT_ARCH_X86)
  _maxSimdWidthSupported = SimdWidth::k256;
#endif // BL_JIT_ARCH_X86

  setDitheringEnabled(dither);
  JitUtils::resetVarStruct(&f, sizeof(f));
}

// bl::Pipeline::JIT::FetchLinearGradientPart - Prepare
// ====================================================

void FetchLinearGradientPart::preparePart() noexcept {
#if defined(BL_JIT_ARCH_X86)
  _maxPixels = uint8_t(pc->hasSSSE3() ? 8 : 4);
#else
  _maxPixels = 8;
#endif
}

// bl::Pipeline::JIT::FetchLinearGradientPart - Init & Fini
// ========================================================

void FetchLinearGradientPart::_initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept {
  // Local Registers
  // ---------------

  _tablePtr = pc->newGpPtr("f.table");                // Reg.
  f->pt = pc->newVec("f.pt");                         // Reg.
  f->dt = pc->newVec("f.dt");                         // Reg/Mem.
  f->dtN = pc->newVec("f.dtN");                       // Reg/Mem.
  f->py = pc->newVec("f.py");                         // Reg/Mem.
  f->dy = pc->newVec("f.dy");                         // Reg/Mem.
  f->maxi = pc->newVec("f.maxi");                     // Reg/Mem.
  f->rori = pc->newVec("f.rori");                     // Reg/Mem [RoR only].
  f->vIdx = pc->newVec("f.vIdx");                     // Reg/Tmp.

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
    pc->v_sub_i32(f->py, f->py, pc->simdConst(&ct.i_0000800000008000, Bcst::k32, f->py));
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
  Vec adv = pc->newSimilarReg(f->pt, "f.adv");
  calcAdvanceX(adv, diff);
  pc->v_add_i64(f->pt, f->pt, adv);

  if (ditheringEnabled())
    _ditheringContext.advanceX(x, diff);
}

void FetchLinearGradientPart::calcAdvanceX(const Vec& dst, const Gp& diff) const noexcept {
  // Use imul on 64-bit targets as it's much shorter than doing a vectorized 64x32 multiply.
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

void FetchLinearGradientPart::prefetch1() noexcept {}

void FetchLinearGradientPart::enterN() noexcept {}
void FetchLinearGradientPart::leaveN() noexcept {}

void FetchLinearGradientPart::prefetchN() noexcept {
  Vec vIdx = f->vIdx;

#if defined(BL_JIT_ARCH_X86)
  if (pc->simdWidth() >= SimdWidth::k256) {
    if (isPad()) {
      pc->v_mov(vIdx, f->pt);
      pc->v_add_i64(f->pt, f->pt, f->dtN);
      pc->v_packs_i32_u16(vIdx, vIdx, f->pt);
      pc->v_min_u16(vIdx, vIdx, f->maxi);
    }
    else {
      Vec vTmp = pc->newSimilarReg(f->vIdx, "f.vTmp");
      pc->v_and_i32(vIdx, f->pt, f->maxi);
      pc->v_add_i64(f->pt, f->pt, f->dtN);
      pc->v_and_i32(vTmp, f->pt, f->maxi);
      pc->v_packs_i32_u16(vIdx, vIdx, vTmp);
      pc->v_xor_i32(vTmp, vIdx, f->rori);
      pc->v_min_u16(vIdx, vIdx, vTmp);
    }

    pc->v_swizzle_u64x4(vIdx, vIdx, swizzle(3, 1, 2, 0));
  }
  else
#endif // BL_JIT_ARCH_X86
  {
    pc->v_mov(vIdx, f->pt);
    pc->v_add_i64(f->pt, f->pt, f->dtN);
    pc->v_interleave_shuffle_u32x4(vIdx, vIdx, f->pt, swizzle(3, 1, 3, 1));
  }
}

void FetchLinearGradientPart::postfetchN() noexcept {
  pc->v_sub_i64(f->pt, f->pt, f->dtN);
}

void FetchLinearGradientPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  BL_ASSERT(predicate.empty());
  blUnused(predicate);

  p.setCount(n);

  switch (n.value()) {
    case 1: {
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
      FetchUtils::x_satisfy_pixel(pc, p, flags);
      break;
    }

    case 4: {
      Vec vIdx = f->vIdx;
      Vec vTmp = pc->newSimilarReg(vIdx, "f.vTmp");

#if defined(BL_JIT_ARCH_X86)
      if (pc->simdWidth() >= SimdWidth::k256) {
        Vec vWrk = pc->newSimilarReg(vIdx, "@vWrk");

        fetchMultiplePixels(p, n, flags, vIdx.v128(), FetchUtils::IndexLayout::kUInt32Hi16, [&](uint32_t step) noexcept {
          if (isPad()) {
            switch (step) {
              case 0   : pc->v_mov(vWrk, f->pt); break;
              case 1   : pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 2   : pc->v_packs_i32_u16(vWrk, vWrk, f->pt); break;
              case 3   : pc->v_min_u16(vWrk, vWrk, f->maxi); break;
              case 0xFF: pc->v_swizzle_u64x4(vIdx, vWrk, swizzle(3, 1, 2, 0)); break;
            }
          }
          else {
            switch (step) {
              case 0   : pc->v_and_i32(vWrk, f->pt, f->maxi);
                         pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 1   : pc->v_and_i32(vTmp, f->pt, f->maxi);
                         pc->v_packs_i32_u16(vWrk, vWrk, vTmp); break;
              case 2   : pc->v_xor_i32(vWrk, vWrk, f->rori); break;
              case 3   : pc->v_min_u16(vWrk, vWrk, vTmp); break;
              case 0xFF: pc->v_swizzle_u64x4(vIdx, vWrk, swizzle(3, 1, 2, 0)); break;
            }
          }
        });
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        FetchUtils::IndexLayout indexLayout = FetchUtils::IndexLayout::kUInt16;

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

        fetchMultiplePixels(p, n, flags, vIdx.v128(), indexLayout, [&](uint32_t step) noexcept {
          if (!pc->hasNonDestructiveDst()) {
            switch (step) {
              case 0   : pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 1   : pc->v_mov(vTmp, f->pt); break;
              case 2   : pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 0xFF: pc->v_interleave_shuffle_u32x4(vIdx, vTmp, f->pt, swizzle(3, 1, 3, 1)); break;
            }
          }
          else {
            switch (step) {
              case 0   : pc->v_add_i64(vTmp, f->pt, f->dtN); break;
              case 2   : pc->v_add_i64(f->pt, vTmp, f->dtN); break;
              case 0xFF: pc->v_interleave_shuffle_u32x4(vIdx, vTmp, f->pt, swizzle(3, 1, 3, 1)); break;
            }
          }
        });
      }

      FetchUtils::x_satisfy_pixel(pc, p, flags);
      break;
    }

    case 8: {
      Vec vIdx = f->vIdx;
      Vec vTmp = pc->newSimilarReg(vIdx, "f.vTmp");

#if defined(BL_JIT_ARCH_X86)
      if (pc->simdWidth() >= SimdWidth::k256) {
        Vec vWrk = pc->newSimilarReg(vIdx, "@vWrk");

        fetchMultiplePixels(p, n, flags, vIdx, FetchUtils::IndexLayout::kUInt32Hi16, [&](uint32_t step) noexcept {
          if (isPad()) {
            switch (step) {
              case 0   : pc->v_add_i64(vWrk, f->pt, f->dtN); break;
              case 1   : pc->v_add_i64(f->pt, vWrk, f->dtN); break;
              case 2   : pc->v_packs_i32_u16(vWrk, vWrk, f->pt); break;
              case 3   : pc->v_min_u16(vWrk, vWrk, f->maxi); break;
              case 0xFF: pc->v_swizzle_u64x4(vIdx, vWrk, swizzle(3, 1, 2, 0)); break;
            }
          }
          else {
            switch (step) {
              case 0   : pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 1   : pc->v_and_i32(vWrk, f->pt, f->maxi); break;
              case 2   : pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 3   : pc->v_and_i32(vTmp, f->pt, f->maxi); break;
              case 4   : pc->v_packs_i32_u16(vWrk, vWrk, vTmp); break;
              case 5   : pc->v_xor_i32(vTmp, vWrk, f->rori); break;
              case 6   : pc->v_min_u16(vWrk, vWrk, vTmp); break;
              case 0xFF: pc->v_swizzle_u64x4(vIdx, vWrk, swizzle(3, 1, 2, 0)); break;
            }
          }
        });
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        pc->v_add_i64(f->pt, f->pt, f->dtN);
        pc->v_mov(vTmp, f->pt);
        pc->v_add_i64(f->pt, f->pt, f->dtN);
        pc->v_interleave_shuffle_u32x4(vTmp, vTmp, f->pt, swizzle(3, 1, 3, 1));

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

        fetchMultiplePixels(p, n, flags, vIdx, FetchUtils::IndexLayout::kUInt16, [&](uint32_t step) noexcept {
          if (!pc->hasNonDestructiveDst()) {
            switch (step) {
              case 1   : pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 3   : pc->v_mov(vTmp, f->pt); break;
              case 5   : pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 0xFF: pc->v_interleave_shuffle_u32x4(vIdx, vTmp, f->pt, swizzle(3, 1, 3, 1)); break;
            }
          }
          else {
            switch (step) {
              case 1   : pc->v_add_i64(vTmp, f->pt, f->dtN); break;
              case 5   : pc->v_add_i64(f->pt, vTmp, f->dtN); break;
              case 0xFF: pc->v_interleave_shuffle_u32x4(vIdx, vTmp, f->pt, swizzle(3, 1, 3, 1)); break;
            }
          }
        });
      }

      FetchUtils::x_satisfy_pixel(pc, p, flags);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::JIT::FetchRadialGradientPart - Construction & Destruction
// =======================================================================

FetchRadialGradientPart::FetchRadialGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchGradientPart(pc, fetchType, format) {

  _isComplexFetch = true;
  _partFlags |= PipePartFlags::kMaskedAccess;

  bool dither = false;
  switch (fetchType) {
    case FetchType::kGradientRadialNNPad: _extendMode = ExtendMode::kPad; break;
    case FetchType::kGradientRadialNNRoR: _extendMode = ExtendMode::kRoR; break;
    case FetchType::kGradientRadialDitherPad: _extendMode = ExtendMode::kPad; dither = true; break;
    case FetchType::kGradientRadialDitherRoR: _extendMode = ExtendMode::kRoR; dither = true; break;
    default:
      BL_NOT_REACHED();
  }

#if defined(BL_JIT_ARCH_X86)
  _maxSimdWidthSupported = SimdWidth::k512;
#endif // BL_JIT_ARCH_X86

  JitUtils::resetVarStruct(&f, sizeof(f));
  setDitheringEnabled(dither);
}

// bl::Pipeline::JIT::FetchRadialGradientPart - Prepare
// ====================================================

void FetchRadialGradientPart::preparePart() noexcept {
  SimdWidth vw = simdWidth();
  _maxPixels = 4 << uint32_t(vw);
}

// bl::Pipeline::JIT::FetchRadialGradientPart - Init & Fini
// ========================================================

void FetchRadialGradientPart::_initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept {
  // Local Registers
  // ---------------

  SimdWidth vw = simdWidth();

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
    if (vw > SimdWidth::k128) {
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
    pc->v_broadcast_u32(f->vrori, mem_ptr(fn.fetchData(), REL_GRADIENT(radial.rori)));
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

  pc->v_madd_f64(v1, f->vy, f->yy_yx, f->ty_tx);          // v1    = [ ty  + Y * yy      | tx + Y * yx       ] => [  py  |  px  ]
  pc->v_madd_f64(v0, f->vy, f->ddy_by, f->dd0_b0);        // v0    = [ dd0 + Y * ddy     | b0 + Y * by       ] => [  dd  |   b  ]
  pc->v_mul_f64(v1, v1, v1);                              // v1    = [ (ty + Y * yy)^2   | (tx + Y * xx) ^ 2 ] => [ py^2 | px^2 ]
  pc->s_mul_f64(v2, v0, v0);                              // v2    = [ ?                 | b^2               ]

  pc->v_dup_hi_f64(v3, f->inv2a_4a);                      // v3    = [ 1 / 2a            | 1 / 2a            ]
  pc->v_hadd_f64(v1, v1, v1);                             // v1    = [ py^2 + px^2       | py^2 + px^2       ]

  pc->s_sub_f64(v1, v1, f->sqinv2a_sqfr);                 // v1    = [ ?                 | py^2 + px^2 - fr^2]
  pc->s_madd_f64(v2, v1, f->inv2a_4a, v2);                // v2    = [ ?                 | b^2+4a(py^2+px^2-fr^2) ] => [ ?    | d    ]
  pc->v_combine_hi_lo_f64(v2, v0, v2);                    // v2    = [ dd                | d                 ]
  pc->s_mul_f64(v0, v0, v3);                              // v0    = [ ?                 | b * (1/2a)        |
  pc->v_dup_hi_f64(v3, f->sqinv2a_sqfr);                  // v3    = [ (1/2a)^2          | (1/2a)^2          ]
  pc->v_mul_f64(v2, v2, v3);                              // v2    = [ dd * (1/2a)^2     | d * (1/2a)^2      ]

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
  SimdWidth vw = simdWidth();
  Vec vd = pc->newVec(vw, "@vd");

  // `vd` is `diff` converted to f32 and broadcasted to all lanes.
  pc->s_cvt_int_to_f32(vd, diff);
  pc->v_broadcast_f32(vd, vd);
  pc->v_add_f32(f->vx, f->vx, vd);

  if (ditheringEnabled())
    _ditheringContext.advanceX(x, diff);
}

// bl::Pipeline::JIT::FetchRadialGradientPart - Fetch
// ==================================================

void FetchRadialGradientPart::prefetch1() noexcept {}

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
#if defined(BL_JIT_ARCH_X86)
  SimdWidth vw = simdWidth();
#endif // BL_JIT_ARCH_X86

  p.setCount(n);

  switch (n.value()) {
    case 1: {
      BL_ASSERT(predicate.empty());

      Gp gIdx = pc->newGp32("gIdx");
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

      applyExtend(vIdx, v0);

      pc->s_extract_u16(gIdx, vIdx, 0u);
      fetchSinglePixel(p, flags, gIdx);

      FetchUtils::x_satisfy_pixel(pc, p, flags);
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

      FetchUtils::IndexLayout indexLayout = applyExtend(vIdx, v0.v128());

      if (predicate.empty()) {
        pc->v_mov(v0, f->d);
        pc->v_mul_f32(v1, f->vx, f->vx);
      }

      fetchMultiplePixels(p, n, flags, vIdx, indexLayout, [&](uint32_t step) noexcept {
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

      FetchUtils::x_satisfy_pixel(pc, p, flags);
      break;
    }

    case 8: {
#if defined(BL_JIT_ARCH_X86)
      if (vw >= SimdWidth::k256) {
        Vec v0 = f->value;
        Vec v1 = pc->newSimilarReg(v0, "v1");
        Vec vIdx = pc->newSimilarReg(v0, "vIdx");

        pc->v_madd_f32(vIdx, f->bd, f->vx, f->b);

        if (predicate.empty()) {
          pc->v_add_f32(f->vx, f->vx, pc->simdConst(&pc->ct.f32_8, Bcst::k32, f->vx));
        }

        pc->v_add_f32(vIdx, vIdx, v0);
        pc->v_cvt_trunc_f32_to_i32(vIdx, vIdx);

        FetchUtils::IndexLayout indexLayout = applyExtend(vIdx, v0);

        if (predicate.empty()) {
          pc->v_mov(v0, f->d);
          pc->v_mul_f32(v1, f->vx, f->vx);
        }

        fetchMultiplePixels(p, n, flags, vIdx, indexLayout, [&](uint32_t step) noexcept {
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

        FetchUtils::x_satisfy_pixel(pc, p, flags);
        return;
      }
#endif // BL_JIT_ARCH_X86

      _fetch2x4(p, flags);
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

FetchUtils::IndexLayout FetchRadialGradientPart::applyExtend(const Vec& idx, const Vec& tmp) noexcept {
  if (isPad()) {
#if defined(BL_JIT_ARCH_X86)
    if (!pc->hasSSE4_1()) {
      pc->v_packs_i32_i16(idx, idx, idx);
      pc->v_min_i16(idx, idx, f->vmaxi);
      pc->v_max_i16(idx, idx, pc->simdConst(&ct.i_0000000000000000, Bcst::kNA, idx));
      return FetchUtils::IndexLayout::kUInt16;
    }

    if (simdWidth() > SimdWidth::k128) {
      pc->v_max_i32(idx, idx, pc->simdConst(&ct.i_0000000000000000, Bcst::kNA, idx));
      pc->v_min_u32(idx, idx, f->vmaxi.cloneAs(idx));
      return FetchUtils::IndexLayout::kUInt32Lo16;
    }
#endif // BL_JIT_ARCH_X86

    pc->v_packs_i32_u16(idx, idx, idx);
    pc->v_min_u16(idx, idx, f->vmaxi.cloneAs(idx));
    return FetchUtils::IndexLayout::kUInt16;
  }
  else {
    pc->v_and_i32(idx, idx, f->vmaxi.cloneAs(idx));
    pc->v_xor_i32(tmp, idx, f->vrori.cloneAs(idx));
    pc->v_min_i16(idx, idx, tmp);
    return FetchUtils::IndexLayout::kUInt32Lo16;
  }
}

// bl::Pipeline::JIT::FetchConicGradientPart - Construction & Destruction
// ======================================================================

FetchConicGradientPart::FetchConicGradientPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchGradientPart(pc, fetchType, format) {

  _partFlags |= PipePartFlags::kMaskedAccess | PipePartFlags::kAdvanceXNeedsX;
  _isComplexFetch = true;

#if defined(BL_JIT_ARCH_X86)
  _maxSimdWidthSupported = SimdWidth::k512;
#endif // BL_JIT_ARCH_X86

  setDitheringEnabled(fetchType == FetchType::kGradientConicDither);
  JitUtils::resetVarStruct(&f, sizeof(f));
}

// bl::Pipeline::JIT::FetchConicGradientPart - Prepare
// ===================================================

void FetchConicGradientPart::preparePart() noexcept {
  _maxPixels = uint8_t(4 * pc->simdMultiplier());
}

// bl::Pipeline::JIT::FetchConicGradientPart - Init & Fini
// =======================================================

void FetchConicGradientPart::_initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept {
  // Local Registers
  // ---------------

  _tablePtr = pc->newGpPtr("f.table");                // Reg.
  f->consts = pc->newGpPtr("f.consts");               // Reg.
  f->px = pc->newV128_F64("f.px");                    // Reg.
  f->xx = pc->newV128_F64("f.xx");                    // Reg/Mem.
  f->hy_hx = pc->newV128_F64("f.hy_hx");              // Reg. (TODO: Make spillable).
  f->yy_yx = pc->newV128_F64("f.yy_yx");              // Mem.
  f->ay = pc->newVec("f.ay");                         // Reg/Mem.
  f->by = pc->newVec("f.by");                         // Reg/Mem.

  f->angleOffset = pc->newVec("f.angleOffset");       // Reg/Mem.
  f->maxi = pc->newVec("f.maxi");                     // Reg/Mem.

  f->t0 = pc->newVec("f.t0");                         // Reg/Tmp.
  f->t1 = pc->newVec("f.t1");                         // Reg/Tmp.
  f->t2 = pc->newVec("f.t2");                         // Reg/Tmp.

  Vec off = pc->newV128_F64("f.off");                 // Initialization only.

  // Part Initialization
  // -------------------

  pc->load(_tablePtr, mem_ptr(fn.fetchData(), REL_GRADIENT(lut.data)));

  if (ditheringEnabled())
    _ditheringContext.initY(fn, x, y);

  pc->s_cvt_int_to_f64(f->hy_hx, y);
  pc->v_dup_lo_f64(f->xx, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.xx)));
  pc->v_loadu128_f64(f->yy_yx, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.yx)));
  pc->v_loadu128_f64(off, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.ox)));

  pc->v_broadcast_u32(f->maxi, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.maxi)));
  pc->v_broadcast_u32(f->angleOffset, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.offset)));

  pc->v_dup_lo_f64(f->hy_hx, f->hy_hx);
  pc->v_mul_f64(f->hy_hx, f->hy_hx, f->yy_yx);
  pc->v_add_f64(f->hy_hx, f->hy_hx, off);

  pc->load(f->consts, mem_ptr(fn.fetchData(), REL_GRADIENT(conic.consts)));

  if (isRectFill()) {
    pc->s_cvt_int_to_f64(off, x);
    pc->s_mul_f64(off, off, f->xx);

    if (pc->isScalarOpPreservingVec128()) {
      // Scalar adds keep the original hi value.
      pc->s_add_f64(f->hy_hx, f->hy_hx, off);
    }
    else {
      // Scalar adds zero the original hi value, so we have to use a vector add instead.
      pc->v_add_f64(f->hy_hx, f->hy_hx, off);
    }
  }

  // Setup constants used by 4+ pixel fetches.
  if (maxPixels() > 1) {
    f->xx_inc = pc->newV128("f.xx_inc"); // Reg/Mem.
    f->xx_off = pc->newVec("f.xx_off");  // Reg/Mem.

    pc->v_cvt_f64_to_f32_lo(f->xx_off.v128(), f->xx);

    if (maxPixels() == 4)
      pc->v_mul_f64(f->xx_inc, f->xx, pc->simdMemConst(&ct.f64_4_8, Bcst::k32, f->xx_inc));
    else
      pc->v_mul_f64(f->xx_inc, f->xx, pc->simdMemConst(&ct.f64_8_4, Bcst::k32, f->xx_inc));

    pc->v_broadcast_u32(f->xx_off, f->xx_off);
    pc->v_mul_f32(f->xx_off, f->xx_off, pc->simdMemConst(&ct.f32_increments, Bcst::kNA_Unique, f->xx_off));
  }
}

void FetchConicGradientPart::_finiPart() noexcept {}

// bl::Pipeline::JIT::FetchConicGradientPart - Advance
// ===================================================

void FetchConicGradientPart::advanceY() noexcept {
  pc->v_add_f64(f->hy_hx, f->hy_hx, f->yy_yx);

  if (ditheringEnabled())
    _ditheringContext.advanceY();
}

void FetchConicGradientPart::startAtX(const Gp& x) noexcept {
  pc->v_cvt_f64_to_f32_lo(f->by.v128(), f->hy_hx);
  pc->v_swizzle_f32x4(f->by.v128(), f->by.v128(), swizzle(1, 1, 1, 1));

  if (!f->by.isVec128()) {
    pc->v_broadcast_v128_f32(f->by, f->by.v128());
  }

  pc->v_abs_f32(f->ay, f->by);
  pc->v_srai_i32(f->by, f->by, 31);
  pc->v_and_f32(f->by, f->by, mem_ptr(f->consts, BL_OFFSET_OF(CommonTable::Conic, n_div_1)));

  advanceX(x, pc->_gpNone);
}

void FetchConicGradientPart::advanceX(const Gp& x, const Gp& diff) noexcept {
  blUnused(diff);

  if (isRectFill()) {
    pc->v_dup_lo_f64(f->px, f->hy_hx);
  }
  else {
    pc->s_cvt_int_to_f64(f->px, x);
    pc->s_mul_f64(f->px, f->px, f->xx);
    pc->s_add_f64(f->px, f->px, f->hy_hx);
  }

  recalcX();

  if (ditheringEnabled())
    _ditheringContext.startAtX(x);
}

void FetchConicGradientPart::recalcX() noexcept {
  pc->v_cvt_f64_to_f32_lo(f->t0.v128(), f->px);

  if (maxPixels() == 1) {
    Vec t0 = f->t0.v128();
    Vec t1 = f->t1.v128();
    Vec t2 = f->t2.v128();
    Vec ay = f->ay.v128();
    Vec tmp = pc->newV128("f.tmp");

    pc->v_abs_f32(t1, t0);
    pc->s_max_f32(tmp, t1, ay);
    pc->s_min_f32(t2, t1, ay);

    pc->s_cmp_eq_f32(t1, t1, t2);
    pc->s_div_f32(t2, t2, tmp);

    pc->v_srai_i32(t0, t0, 31);
    pc->v_and_f32(t1, t1, mem_ptr(f->consts, BL_OFFSET_OF(CommonTable::Conic, n_div_4)));
  }
  else {
    Vec t0 = f->t0;
    Vec t1 = f->t1;
    Vec t2 = f->t2;
    Vec ay = f->ay;
    Vec tmp = pc->newSimilarReg(f->t0, "f.tmp");

    pc->v_broadcast_u32(t0, t0);
    pc->v_add_f32(t0, t0, f->xx_off);
    pc->v_abs_f32(t1, t0);

    pc->v_max_f32(tmp, t1, ay);
    pc->v_min_f32(t2, t1, ay);
    pc->v_cmp_eq_f32(t1, t1, t2);
    pc->v_div_f32(t2, t2, tmp);
  }
}

// bl::Pipeline::JIT::FetchConicGradientPart - Fetch
// =================================================

void FetchConicGradientPart::prefetchN() noexcept {}

void FetchConicGradientPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  p.setCount(n);

  Gp consts = f->consts;
  Vec px = f->px;
  Vec t0 = f->t0;
  Vec t1 = f->t1;
  Vec t2 = f->t2;

  // Use 128-bit SIMD if the number of pixels is 4 or less.
  if (n.value() <= 4) {
    t0 = t0.v128();
    t1 = t1.v128();
    t2 = t2.v128();
  }

  Vec t3 = pc->newSimilarReg(t0, "f.t3");
  Vec t4 = pc->newSimilarReg(t0, "f.t4");

  switch (n.value()) {
    case 1: {
      Gp idx = pc->newGpPtr("f.idx");

      pc->s_mul_f32(t3, t2, t2);
      pc->v_srai_i32(t0, t0, 31);
      pc->v_loada32_f32(t4, mem_ptr(consts, BL_OFFSET_OF(CommonTable::Conic, q3)));

      pc->s_madd_f32(t4, t4, t3, mem_ptr(consts, BL_OFFSET_OF(CommonTable::Conic, q2)));
      pc->v_and_f32(t1, t1, mem_ptr(f->consts, BL_OFFSET_OF(CommonTable::Conic, n_div_4)));
      pc->s_madd_f32(t4, t4, t3, mem_ptr(consts, BL_OFFSET_OF(CommonTable::Conic, q1)));
      pc->v_and_f32(t0, t0, mem_ptr(consts, BL_OFFSET_OF(CommonTable::Conic, n_div_2)));
      pc->s_madd_f32(t4, t4, t3, mem_ptr(consts, BL_OFFSET_OF(CommonTable::Conic, q0)));
      pc->s_msub_f32(t4, t4, t2, t1);

      pc->v_abs_f32(t4, t4);
      pc->s_sub_f32(t4, t4, t0);
      pc->v_abs_f32(t4, t4);

      pc->s_sub_f32(t4, t4, f->by);
      pc->v_abs_f32(t4, t4);
      pc->s_add_f32(t4, t4, f->angleOffset.v128());

      pc->v_cvt_trunc_f32_to_i32(t4, t4);
      pc->s_add_f64(px, px, f->xx);
      pc->v_and_i32(t4, t4, f->maxi.v128());
      pc->s_mov_u32(idx.r32(), t4);

      recalcX();
      fetchSinglePixel(p, flags, idx);
      FetchUtils::x_satisfy_pixel(pc, p, flags);
      break;
    }

    case 4:
    case 8:
    case 16: {
      pc->v_mul_f32(t3, t2, t2);
      pc->v_srai_i32(t0, t0, 31);
      pc->v_mov(t4, mem_ptr(consts, BL_OFFSET_OF(CommonTable::Conic, q3)));
      pc->v_and_f32(t1, t1, mem_ptr(f->consts, BL_OFFSET_OF(CommonTable::Conic, n_div_4)));

      pc->v_madd_f32(t4, t4, t3, mem_ptr(consts, BL_OFFSET_OF(CommonTable::Conic, q2)));
      pc->v_and_f32(t0, t0, mem_ptr(consts, BL_OFFSET_OF(CommonTable::Conic, n_div_2)));
      pc->v_madd_f32(t4, t4, t3, mem_ptr(consts, BL_OFFSET_OF(CommonTable::Conic, q1)));
      pc->v_madd_f32(t4, t4, t3, mem_ptr(consts, BL_OFFSET_OF(CommonTable::Conic, q0)));
      pc->v_msub_f32(t4, t4, t2, t1);

      pc->v_abs_f32(t4, t4);
      pc->v_sub_f32(t4, t4, t0);
      pc->v_abs_f32(t4, t4);

      pc->v_sub_f32(t4, t4, f->by.cloneAs(t4));
      pc->v_abs_f32(t4, t4);
      pc->v_add_f32(t4, t4, f->angleOffset.cloneAs(t4));

      pc->v_cvt_trunc_f32_to_i32(t4, t4);
      pc->v_and_i32(t4, t4, f->maxi.cloneAs(t4));

      fetchMultiplePixels(p, n, flags, t4, FetchUtils::IndexLayout::kUInt32Lo16, [&](uint32_t step) noexcept {
        // Don't recalculate anything if this is a predicated load, because this is either the end or X will be advanced.
        if (!predicate.empty())
          return;

        switch (step) {
          case 1: if (maxPixels() >= 8 && n.value() == 4) {
                    Vec tmp = pc->newV128("f.tmp");
                    pc->v_dup_hi_f64(tmp, f->xx_inc);
                    pc->s_add_f64(px, px, tmp);
                  }
                  else {
                    pc->s_add_f64(px, px, f->xx_inc);
                  }

                  if (n.value() == 16) {
                    pc->s_add_f64(px, px, f->xx_inc);
                  }
                  break;
          case 2: recalcX();
                  break;
        }
      });

      FetchUtils::x_satisfy_pixel(pc, p, flags);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
