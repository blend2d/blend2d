// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchgradientpart_p.h"
#include "../../pipeline/jit/fetchutils_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

#include "../../pipeline/jit/pipedebug_p.h"

namespace BLPipeline {
namespace JIT {

#define REL_GRADIENT(FIELD) BL_OFFSET_OF(FetchData::Gradient, FIELD)

// BLPipeline::JIT::GradientDitheringContext
// =========================================

void GradientDitheringContext::initY(const x86::Gp& x, const x86::Gp& y) noexcept {
  x86::Compiler* cc = pc->cc;

  _dmPosition = cc->newUInt32("dm.position");
  _dmOriginX = cc->newUInt32("dm.originX");
  _dmValues = pc->newVec(pc->simdWidth(), "dm.values");

  _isRectFill = x.isValid();

  cc->mov(_dmPosition, x86::ptr(pc->_ctxData, BL_OFFSET_OF(BLPipeline::ContextData, pixelOrigin.y)));
  cc->mov(_dmOriginX, x86::ptr(pc->_ctxData, BL_OFFSET_OF(BLPipeline::ContextData, pixelOrigin.x)));

  cc->add(_dmPosition, y.r32());
  if (isRectFill())
    cc->add(_dmOriginX, x.r32());

  cc->and_(_dmPosition, 15);
  if (isRectFill())
    cc->and_(_dmOriginX, 15);

  cc->shl(_dmPosition, 5);
  if (isRectFill())
    cc->add(_dmPosition, _dmOriginX);
}

void GradientDitheringContext::advanceY() noexcept {
  x86::Compiler* cc = pc->cc;

  cc->add(_dmPosition, 16 * 2);
  cc->and_(_dmPosition, 16 * 16 * 2 - 1);
}

void GradientDitheringContext::startAtX(const x86::Gp& x) noexcept {
  x86::Compiler* cc = pc->cc;
  x86::Gp dmPosition = _dmPosition;

  if (!isRectFill()) {
    // If not rectangular, we have to calculate the final position according to `x`.
    dmPosition = cc->newUInt32("dm.finalPosition");

    cc->mov(dmPosition, _dmOriginX);
    cc->add(dmPosition, x.r32());
    cc->and_(dmPosition, 15);
    cc->add(dmPosition, _dmPosition);
  }

  x86::Mem m;
  if (cc->is32Bit()) {
    m = x86::ptr(uint64_t(uintptr_t(blCommonTable.bayerMatrix16x16)), dmPosition);
  }
  else {
    pc->_initCommonTablePtr();
    m = x86::ptr(pc->_commonTablePtr, dmPosition.r64(), 0, -pc->_commonTableOff);
  }

  if (_dmValues.isXmm())
    pc->v_loadu_i128_ro(_dmValues, m);
  else
    pc->v_broadcast_u32x4(_dmValues, m);
}

void GradientDitheringContext::advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept {
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
  x86::Compiler* cc = pc->cc;
  SimdWidth simdWidth = SimdWidthUtils::simdWidthOf(p.uc[0]);

  Operand shufflePredicate = pc->simdConst(&blCommonTable.pshufb_dither_rgba64_lo, Bcst::kNA_Unique, simdWidth);
  x86::Vec ditherPredicate = cc->newSimilarReg(p.uc[0], "ditherPredicate");
  x86::Vec ditherThreshold = cc->newSimilarReg(p.uc[0], "ditherThreshold");

  switch (p.count().value()) {
    case 1: {
      if (pc->hasSSSE3()) {
        pc->v_shuffle_i8(ditherPredicate, _dmValues.cloneAs(ditherPredicate), shufflePredicate);
      }
      else {
        pc->v_interleave_lo_u8(ditherPredicate, _dmValues, pc->simdConst(&blCommonTable.i_0000000000000000, Bcst::kNA, ditherPredicate));
        pc->v_swizzle_lo_u16(ditherPredicate, ditherPredicate, x86::shuffleImm(0, 0, 0, 0));
      }

      pc->v_swizzle_lo_u16(ditherThreshold, p.uc[0], x86::shuffleImm(3, 3, 3, 3));
      pc->v_adds_u16(p.uc[0], p.uc[0], ditherPredicate);
      pc->v_min_u16(p.uc[0], p.uc[0], ditherThreshold);
      pc->v_srl_i16(p.uc[0], p.uc[0], 8);
      advanceXAfterFetch(1);
      break;
    }

    case 4:
    case 8:
    case 16: {
      if (!p.uc[0].isXmm()) {
        for (uint32_t i = 0; i < p.uc.size(); i++) {
          // At least AVX2: VPSHUFB is available...
          pc->v_shuffle_i8(ditherPredicate, _dmValues.cloneAs(ditherPredicate), shufflePredicate);
          pc->v_expand_alpha_16(ditherThreshold, p.uc[i]);
          pc->v_adds_u16(p.uc[i], p.uc[i], ditherPredicate);
          pc->v_min_u16(p.uc[i], p.uc[i], ditherThreshold);

          if (p.uc[0].isYmm())
            pc->v_shuffle_u32(_dmValues, _dmValues, _dmValues, x86::shuffleImm(0, 3, 2, 1));
          else
            pc->v_shuffle_u32(_dmValues, _dmValues, _dmValues, x86::shuffleImm(1, 0, 3, 2));
        }
        pc->v_srl_i16(p.uc, p.uc, 8);
      }
      else {
        for (uint32_t i = 0; i < p.uc.size(); i++) {
          x86::Vec dm = (i == 0) ? _dmValues.cloneAs(ditherPredicate) : ditherPredicate;

          if (pc->hasSSSE3()) {
            pc->v_shuffle_i8(ditherPredicate, dm, shufflePredicate);
          }
          else {
            pc->v_interleave_lo_u8(ditherPredicate, dm, pc->simdConst(&blCommonTable.i_0000000000000000, Bcst::kNA, ditherPredicate));
            pc->v_interleave_lo_u16(ditherPredicate, ditherPredicate, ditherPredicate);
            pc->v_swizzle_u32(ditherPredicate, ditherPredicate, x86::shuffleImm(1, 1, 0, 0));
          }

          pc->v_expand_alpha_16(ditherThreshold, p.uc[i]);
          pc->v_adds_u16(p.uc[i], p.uc[i], ditherPredicate);

          if (i + 1u < p.uc.size())
            pc->v_swizzle_lo_u16(ditherPredicate, _dmValues.cloneAs(ditherPredicate), x86::shuffleImm(0, 3, 2, 1));

          pc->v_min_u16(p.uc[i], p.uc[i], ditherThreshold);
        }

        if (p.count().value() == 4)
          pc->v_shuffle_u32(_dmValues, _dmValues, _dmValues, x86::shuffleImm(0, 3, 2, 1));
        else
          pc->v_shuffle_u32(_dmValues, _dmValues, _dmValues, x86::shuffleImm(1, 0, 3, 2));

        pc->v_srl_i16(p.uc, p.uc, 8);
      }
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// BLPipeline::JIT::FetchGradientPart - Construction & Destruction
// ===============================================================

FetchGradientPart::FetchGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept
  : FetchPart(pc, fetchType, format),
    _ditheringContext(pc) {

  _partFlags |= PipePartFlags::kAdvanceXNeedsDiff;
}

void FetchGradientPart::fetchSinglePixel(Pixel& dst, PixelFlags flags, const x86::Gp& idx) noexcept {
  x86::Mem src = x86::ptr(_tablePtr, idx, tablePtrShift());
  if (ditheringEnabled()) {
    pc->newVecArray(dst.uc, 1, SimdWidth::k128, dst.name(), "uc");
    pc->v_load_i64(dst.uc[0], src);
    _ditheringContext.ditherUnpackedPixels(dst);
  }
  else {
    pc->x_fetch_pixel(dst, PixelCount(1), flags, BLInternalFormat::kPRGB32, src, Alignment(4));
  }
}

void FetchGradientPart::fetchMultiplePixels(Pixel& dst, PixelCount n, PixelFlags flags, const x86::Vec& idx, IndexLayout indexLayout, InterleaveCallback cb, void* cbData) noexcept {
  x86::Mem src = x86::ptr(_tablePtr);
  uint32_t idxShift = tablePtrShift();

  if (ditheringEnabled()) {
    dst.setType(PixelType::kRGBA64);
    FetchUtils::x_gather_pixels(pc, dst, n, BLInternalFormat::kPRGB64, PixelFlags::kUC, src, idx, idxShift, indexLayout, cb, cbData);
    _ditheringContext.ditherUnpackedPixels(dst);

    dst.setType(PixelType::kRGBA32);
    pc->x_satisfy_pixel(dst, flags);
  }
  else {
    FetchUtils::x_gather_pixels(pc, dst, n, format(), flags, src, idx, idxShift, indexLayout, cb, cbData);
  }
}

// BLPipeline::JIT::FetchLinearGradientPart - Construction & Destruction
// =====================================================================

FetchLinearGradientPart::FetchLinearGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept
  : FetchGradientPart(pc, fetchType, format) {

  _maxSimdWidthSupported = SimdWidth::k256;

  bool dither = false;
  switch (fetchType) {
    case FetchType::kGradientLinearNNPad: _extendMode = ExtendMode::kPad; break;
    case FetchType::kGradientLinearNNRoR: _extendMode = ExtendMode::kRoR; break;
    case FetchType::kGradientLinearDitherPad: _extendMode = ExtendMode::kPad; dither = true; break;
    case FetchType::kGradientLinearDitherRoR: _extendMode = ExtendMode::kRoR; dither = true; break;
    default:
      BL_NOT_REACHED();
  }

  setDitheringEnabled(dither);
  JitUtils::resetVarStruct(&f, sizeof(f));
}

// BLPipeline::JIT::FetchLinearGradientPart - Prepare
// ==================================================

void FetchLinearGradientPart::preparePart() noexcept {
  _maxPixels = uint8_t(pc->hasSSSE3() ? 8 : 4);
}

// BLPipeline::JIT::FetchLinearGradientPart - Init & Fini
// ======================================================

void FetchLinearGradientPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  // Local Registers
  // ---------------

  _tablePtr = cc->newIntPtr("f.table");               // Reg.
  f->pt = pc->newVec("f.pt");                         // Reg.
  f->dt = pc->newVec("f.dt");                         // Reg/Mem.
  f->dtN = pc->newVec("f.dtN");                       // Reg/Mem.
  f->py = pc->newVec("f.py");                         // Reg/Mem.
  f->dy = pc->newVec("f.dy");                         // Reg/Mem.
  f->maxi = pc->newVec("f.maxi");                     // Reg/Mem.
  f->rori = pc->newVec("f.rori");                     // Reg/Mem [RoR only].
  f->vIdx = pc->newVec("f.vIdx");                     // Reg/Tmp.

  // In 64-bit mode it's easier to use imul for 64-bit multiplication instead of SIMD, because
  // we need to multiply a scalar anyway that we then broadcast and add to our 'f.pt' vector.
  if (cc->is64Bit()) {
    f->dtGp = cc->newUInt64("f.dtGp");                // Reg/Mem.
  }

  // Part Initialization
  // -------------------

  cc->mov(_tablePtr, x86::ptr(pc->_fetchData, REL_GRADIENT(lut.data)));

  if (ditheringEnabled())
    _ditheringContext.initY(x, y);

  pc->s_mov_i32(f->py, y);
  pc->v_broadcast_u64(f->dy, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.dy.u64)));
  pc->v_broadcast_u64(f->py, f->py);
  pc->v_mul_u64_u32_lo(f->py, f->dy, f->py);
  pc->v_broadcast_u64(f->dt, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.dt.u64)));

  if (isPad()) {
    pc->v_broadcast_u16(f->maxi, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.maxi)));
  }
  else {
    pc->v_broadcast_u32(f->maxi, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.maxi)));
    pc->v_broadcast_u16(f->rori, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.rori)));
  }

  pc->v_loadu_i128(f->pt, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.pt)));
  pc->v_sll_i64(f->dtN, f->dt, 1u);

  if (pc->use256BitSimd()) {
    cc->vperm2i128(f->dtN, f->dtN, f->dtN, perm2x128Imm(Perm2x128::kALo, Perm2x128::kZero));
    cc->vperm2i128(f->pt, f->pt, f->pt, perm2x128Imm(Perm2x128::kALo, Perm2x128::kALo));
    pc->v_add_i64(f->pt, f->pt, f->dtN);
    pc->v_sll_i64(f->dtN, f->dt, 2u);
  }

  pc->v_add_i64(f->py, f->py, f->pt);

  // If we cannot use PACKUSDW, which was introduced by SSE4.1 we subtract 32768 from the pointer
  // and use PACKSSDW instead. However, if we do this, we have to adjust everything else accordingly.
  if (isPad() && !pc->hasSSE4_1()) {
    pc->v_sub_i32(f->py, f->py, pc->simdConst(&ct.i_0000800000008000, Bcst::k32, f->py));
    pc->v_sub_i16(f->maxi, f->maxi, pc->simdConst(&ct.i_8000800080008000, Bcst::kNA, f->maxi));
  }

  if (cc->is64Bit())
    pc->s_mov_i64(f->dtGp, f->dt);

  if (isRectFill()) {
    x86::Vec adv = cc->newSimilarReg(f->dt, "f.adv");
    calcAdvanceX(adv, x);
    pc->v_add_i64(f->py, f->py, adv);
  }

  if (pixelGranularity() > 1)
    enterN();
}

void FetchLinearGradientPart::_finiPart() noexcept {}

// BLPipeline::JIT::FetchLinearGradientPart - Advance
// ==================================================

void FetchLinearGradientPart::advanceY() noexcept {
  pc->v_add_i64(f->py, f->py, f->dy);

  if (ditheringEnabled())
    _ditheringContext.advanceY();
}

void FetchLinearGradientPart::startAtX(const x86::Gp& x) noexcept {
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

void FetchLinearGradientPart::advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept {
  x86::Vec adv = cc->newSimilarReg(f->pt, "f.adv");
  calcAdvanceX(adv, diff);
  pc->v_add_i64(f->pt, f->pt, adv);

  if (ditheringEnabled())
    _ditheringContext.advanceX(x, diff);
}

void FetchLinearGradientPart::calcAdvanceX(const x86::Vec& dst, const x86::Gp& diff) const noexcept {
  // Use imul on 64-bit targets as it's much shorter than doing vectorized 64x32 multiply.
  if (cc->is64Bit()) {
    x86::Gp advTmp = cc->newUInt64("f.advTmp");
    cc->mov(advTmp.r32(), diff.r32());
    pc->i_mul(advTmp, advTmp, f->dtGp);
    pc->v_broadcast_u64(dst, advTmp);
  }
  else {
    pc->v_broadcast_u32(dst, diff);
    pc->v_mul_u64_u32_lo(dst, f->dt, dst);
  }
}

// BLPipeline::JIT::FetchLinearGradientPart - Fetch
// ================================================

void FetchLinearGradientPart::prefetch1() noexcept {}

void FetchLinearGradientPart::enterN() noexcept {}
void FetchLinearGradientPart::leaveN() noexcept {}

void FetchLinearGradientPart::prefetchN() noexcept {
  x86::Vec vIdx = f->vIdx;

  if (pc->simdWidth() >= SimdWidth::k256) {
    if (isPad()) {
      pc->v_mov(vIdx, f->pt);
      pc->v_add_i64(f->pt, f->pt, f->dtN);
      pc->v_packs_i32_u16_(vIdx, vIdx, f->pt);
      pc->v_min_u16(vIdx, vIdx, f->maxi);
    }
    else {
      x86::Vec vTmp = cc->newSimilarReg(f->vIdx, "f.vTmp");
      pc->v_and_i32(vIdx, f->pt, f->maxi);
      pc->v_add_i64(f->pt, f->pt, f->dtN);
      pc->v_and_i32(vTmp, f->pt, f->maxi);
      pc->v_packs_i32_u16_(vIdx, vIdx, vTmp);
      pc->v_xor_i32(vTmp, vIdx, f->rori);
      pc->v_min_u16(vIdx, vIdx, vTmp);
    }

    pc->v_perm_i64(vIdx, vIdx, x86::shuffleImm(3, 1, 2, 0));
  }
  else {
    pc->v_mov(vIdx, f->pt);
    pc->v_add_i64(f->pt, f->pt, f->dtN);
    pc->v_shuffle_u32(vIdx, vIdx, f->pt, x86::shuffleImm(3, 1, 3, 1));
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
      x86::Gp gIdx = cc->newInt32("f.gIdx");
      x86::Xmm vIdx = cc->newXmm("f.vIdx");
      uint32_t vIdxLane = 1u + uint32_t(!isPad());

      if (isPad() && pc->hasSSE4_1()) {
        pc->v_packs_i32_u16_(vIdx, f->pt.xmm(), f->pt.xmm());
        pc->v_min_u16(vIdx, vIdx, f->maxi.xmm());
      }
      else if (isPad()) {
        pc->v_packs_i32_i16(vIdx, f->pt.xmm(), f->pt.xmm());
        pc->v_min_i16(vIdx, vIdx, f->maxi.xmm());
        pc->v_add_i16(vIdx, vIdx, pc->simdConst(&ct.i_8000800080008000, Bcst::kNA, vIdx));
      }
      else {
        x86::Xmm vTmp = cc->newXmm("f.vTmp");
        pc->v_and_i32(vIdx, f->pt.xmm(), f->maxi.xmm());
        pc->v_xor_i32(vTmp, vIdx, f->rori.xmm());
        pc->v_min_i16(vIdx, vIdx, vTmp);
      }

      pc->v_add_i64(f->pt, f->pt, f->dt);
      pc->v_extract_u16(gIdx, vIdx, vIdxLane);
      fetchSinglePixel(p, flags, gIdx);
      pc->x_satisfy_pixel(p, flags);
      break;
    }

    case 4: {
      x86::Vec vIdx = f->vIdx;
      x86::Vec vTmp = pc->cc->newSimilarReg(vIdx, "f.vTmp");

      if (pc->simdWidth() >= SimdWidth::k256) {
        fetchMultiplePixels(p, n, flags, vIdx.xmm(), IndexLayout::kUInt32Hi16, [&](uint32_t step) noexcept {
          if (isPad()) {
            switch (step) {
              case 0: pc->v_mov(vIdx, f->pt); break;
              case 1: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 2: pc->v_packs_i32_u16_(vIdx, vIdx, f->pt); break;
              case 3: pc->v_min_u16(vIdx, vIdx, f->maxi);
                      pc->v_perm_i64(vIdx, vIdx, x86::shuffleImm(3, 1, 2, 0)); break;
            }
          }
          else {
            switch (step) {
              case 0: pc->v_and_i32(vIdx, f->pt, f->maxi);
                      pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 1: pc->v_and_i32(vTmp, f->pt, f->maxi);
                      pc->v_packs_i32_u16_(vIdx, vIdx, vTmp); break;
              case 2: pc->v_xor_i32(vTmp, vIdx, f->rori);
                      pc->v_min_u16(vIdx, vIdx, vTmp); break;
              case 3: pc->v_perm_i64(vIdx, vIdx, x86::shuffleImm(3, 1, 2, 0)); break;
            }
          }
        });
      }
      else {
        IndexLayout indexLayout = IndexLayout::kUInt16;

        if (isPad() && pc->hasSSE4_1()) {
          pc->v_packs_i32_u16_(vIdx, vIdx, vIdx);
          pc->v_min_u16(vIdx, vIdx, f->maxi);
        }
        else if (isPad()) {
          pc->v_packs_i32_i16(vIdx, vIdx, vIdx);
          pc->v_min_i16(vIdx, vIdx, f->maxi);
          pc->v_add_i16(vIdx, vIdx, pc->simdConst(&ct.i_8000800080008000, Bcst::kNA, vIdx));
        }
        else {
          indexLayout = IndexLayout::kUInt32Lo16;
          pc->v_and_i32(vIdx, vIdx, f->maxi);
          pc->v_xor_i32(vTmp, vIdx, f->rori);
          pc->v_min_i16(vIdx, vIdx, vTmp);
        }

        fetchMultiplePixels(p, n, flags, vIdx.xmm(), indexLayout, [&](uint32_t step) noexcept {
          switch (step) {
            case 0: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
            case 1: pc->v_mov(vIdx, f->pt); break;
            case 2: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
            case 3: pc->v_shuffle_u32(vIdx, vIdx, f->pt, x86::shuffleImm(3, 1, 3, 1)); break;
          }
        });
      }

      pc->x_satisfy_pixel(p, flags);
      break;
    }

    case 8: {
      x86::Vec vIdx = f->vIdx;
      x86::Vec vTmp = pc->cc->newSimilarReg(vIdx, "f.vTmp");

      if (pc->simdWidth() >= SimdWidth::k256) {
        fetchMultiplePixels(p, n, flags, vIdx, IndexLayout::kUInt32Hi16, [&](uint32_t step) noexcept {
          if (isPad()) {
            switch (step) {
              case 0: pc->v_add_i64(vIdx, f->pt, f->dtN); break;
              case 1: pc->v_add_i64(f->pt, vIdx, f->dtN); break;
              case 2: pc->v_packs_i32_u16_(vIdx, vIdx, f->pt); break;
              case 3: pc->v_min_u16(vIdx, vIdx, f->maxi); break;
              case 4: pc->v_perm_i64(vIdx, vIdx, x86::shuffleImm(3, 1, 2, 0)); break;
            }
          }
          else {
            switch (step) {
              case 0: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 1: pc->v_and_i32(vIdx, f->pt, f->maxi); break;
              case 2: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 3: pc->v_and_i32(vTmp, f->pt, f->maxi); break;
              case 4: pc->v_packs_i32_u16_(vIdx, vIdx, vTmp); break;
              case 5: pc->v_xor_i32(vTmp, vIdx, f->rori); break;
              case 6: pc->v_min_u16(vIdx, vIdx, vTmp); break;
              case 7: pc->v_perm_i64(vIdx, vIdx, x86::shuffleImm(3, 1, 2, 0)); break;
            }
          }
        });
      }
      else {
        pc->v_add_i64(f->pt, f->pt, f->dtN);
        pc->v_mov(vTmp, f->pt);
        pc->v_add_i64(f->pt, f->pt, f->dtN);
        pc->v_shuffle_u32(vTmp, vTmp, f->pt, x86::shuffleImm(3, 1, 3, 1));

        if (isPad() && pc->hasSSE4_1()) {
          pc->v_packs_i32_u16_(vIdx, vIdx, vTmp);
          pc->v_min_u16(vIdx, vIdx, f->maxi);
        }
        else if (isPad()) {
          pc->v_packs_i32_i16(vIdx, vIdx, vTmp);
          pc->v_min_i16(vIdx, vIdx, f->maxi);
          pc->v_add_i16(vIdx, vIdx, pc->simdConst(&ct.i_8000800080008000, Bcst::kNA, vIdx));
        }
        else {
          pc->v_and_i32(vIdx, vIdx, f->maxi);
          pc->v_and_i32(vTmp, vTmp, f->maxi);
          pc->v_packs_i32_i16(vIdx, vIdx, vTmp);
          pc->v_xor_i32(vTmp, vIdx, f->rori);
          pc->v_min_i16(vIdx, vIdx, vTmp);
        }

        fetchMultiplePixels(p, n, flags, vIdx, IndexLayout::kUInt16, [&](uint32_t step) noexcept {
          switch (step) {
            case 1: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
            case 3: pc->v_mov(vIdx, f->pt); break;
            case 5: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
            case 7: pc->v_shuffle_u32(vIdx, vIdx, f->pt, x86::shuffleImm(3, 1, 3, 1)); break;
          }
        });
      }

      pc->x_satisfy_pixel(p, flags);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// BLPipeline::JIT::FetchRadialGradientPart - Construction & Destruction
// =====================================================================

FetchRadialGradientPart::FetchRadialGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept
  : FetchGradientPart(pc, fetchType, format) {

  _partFlags |= PipePartFlags::kAdvanceXNeedsX;
  _isComplexFetch = true;

  bool dither = false;
  switch (fetchType) {
    case FetchType::kGradientRadialNNPad: _extendMode = ExtendMode::kPad; break;
    case FetchType::kGradientRadialNNRoR: _extendMode = ExtendMode::kRoR; break;
    case FetchType::kGradientRadialDitherPad: _extendMode = ExtendMode::kPad; dither = true; break;
    case FetchType::kGradientRadialDitherRoR: _extendMode = ExtendMode::kRoR; dither = true; break;
    default:
      BL_NOT_REACHED();
  }

  setDitheringEnabled(dither);
  JitUtils::resetVarStruct(&f, sizeof(f));
}

// BLPipeline::JIT::FetchRadialGradientPart - Prepare
// ==================================================

void FetchRadialGradientPart::preparePart() noexcept {
  _maxPixels = 4;
}

// BLPipeline::JIT::FetchRadialGradientPart - Init & Fini
// ======================================================

void FetchRadialGradientPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  // Local Registers
  // ---------------

  _tablePtr = cc->newIntPtr("f.table");               // Reg.
  f->xx_xy = cc->newXmmPd("f.xx_xy");                 // Mem.
  f->yx_yy = cc->newXmmPd("f.yx_yy");                 // Mem.
  f->ax_ay = cc->newXmmPd("f.ax_ay");                 // Mem.
  f->fx_fy = cc->newXmmPd("f.fx_fy");                 // Mem.
  f->da_ba = cc->newXmmPd("f.da_ba");                 // Mem.

  f->d_b = cc->newXmmPd("f.d_b");                     // Reg.
  f->dd_bd = cc->newXmmPd("f.dd_bd");                 // Reg.
  f->ddx_ddy = cc->newXmmPd("f.ddx_ddy");             // Mem.

  f->px_py = cc->newXmmPd("f.px_py");                 // Reg.
  f->scale = cc->newXmmPs("f.scale");                 // Mem.
  f->ddd = cc->newXmmPd("f.ddd");                     // Mem.
  f->value = cc->newXmmPs("f.value");                 // Reg/Tmp.

  f->vmaxi = cc->newXmm("f.vmaxi");                   // Mem.
  f->vrori = cc->newXmm("f.vrori");                   // Mem.
  f->vmaxf = cc->newXmmPd("f.vmaxf");                 // Mem.

  f->d_b_prev = cc->newXmmPd("f.d_b_prev");           // Mem.
  f->dd_bd_prev = cc->newXmmPd("f.dd_bd_prev");       // Mem.

  x86::Xmm off = cc->newXmmPd("f.off");               // Initialization only.

  // Part Initialization
  // -------------------

  cc->mov(_tablePtr, x86::ptr(pc->_fetchData, REL_GRADIENT(lut.data)));

  if (ditheringEnabled())
    _ditheringContext.initY(x, y);

  pc->v_loadu_d128(f->ax_ay, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.ax)));
  pc->v_loadu_d128(f->fx_fy, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.fx)));

  pc->v_loadu_d128(f->da_ba  , x86::ptr(pc->_fetchData, REL_GRADIENT(radial.dd)));
  pc->v_loadu_d128(f->ddx_ddy, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.ddx)));

  pc->v_zero_f(f->scale);
  pc->s_cvt_f64_f32(f->scale, f->scale, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.scale)));

  pc->v_load_f64(f->ddd, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.ddd)));
  pc->v_dupl_f64(f->ddd, f->ddd);
  pc->v_expand_lo_ps(f->scale, f->scale);

  pc->v_loadu_d128(f->xx_xy, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.xx)));
  pc->v_loadu_d128(f->yx_yy, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.yx)));

  pc->v_zero_d(f->px_py);
  pc->s_cvt_int_f64(f->px_py, f->px_py, y);
  pc->v_loadu_d128(off, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.ox)));

  pc->v_dupl_f64(f->px_py, f->px_py);
  pc->v_mul_f64(f->px_py, f->px_py, f->yx_yy);
  pc->v_add_f64(f->px_py, f->px_py, off);

  if (isPad()) {
    pc->v_broadcast_u16(f->vmaxi, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.maxi)));
    pc->v_broadcast_u16(f->vrori, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.rori)));
  }
  else {
    pc->v_broadcast_u32(f->vmaxi, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.maxi)));
    pc->v_broadcast_u32(f->vrori, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.rori)));
  }

  if (isPad()) {
    pc->v_cvt_i32_f32(f->vmaxf, f->vmaxi);
  }

  if (isRectFill()) {
    pc->v_zero_d(off);
    pc->s_cvt_int_f64(off, off, x);
    pc->v_dupl_f64(off, off);
    pc->v_mul_f64(off, off, f->xx_xy);
    pc->v_add_f64(f->px_py, f->px_py, off);
  }
}

void FetchRadialGradientPart::_finiPart() noexcept {}

// BLPipeline::JIT::FetchRadialGradientPart - Advance
// ==================================================

void FetchRadialGradientPart::advanceY() noexcept {
  pc->v_add_f64(f->px_py, f->px_py, f->yx_yy);

  if (ditheringEnabled())
    _ditheringContext.advanceY();
}

void FetchRadialGradientPart::startAtX(const x86::Gp& x) noexcept {
  if (isRectFill()) {
    precalc(f->px_py);
  }
  else {
    x86::Xmm px_py = cc->newXmmPd("f.px_py");

    pc->v_zero_d(px_py);
    pc->s_cvt_int_f64(px_py, px_py, x);
    pc->v_dupl_f64(px_py, px_py);
    pc->v_mul_f64(px_py, px_py, f->xx_xy);
    pc->v_add_f64(px_py, px_py, f->px_py);

    precalc(px_py);
  }

  if (ditheringEnabled())
    _ditheringContext.startAtX(x);
}

void FetchRadialGradientPart::advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept {
  if (isRectFill()) {
    precalc(f->px_py);
  }
  else {
    x86::Xmm px_py = cc->newXmmPd("f.px_py");

    // TODO: [PIPEGEN] Duplicated code :(
    pc->v_zero_d(px_py);
    pc->s_cvt_int_f64(px_py, px_py, x);
    pc->v_dupl_f64(px_py, px_py);
    pc->v_mul_f64(px_py, px_py, f->xx_xy);
    pc->v_add_f64(px_py, px_py, f->px_py);

    precalc(px_py);
  }

  if (ditheringEnabled())
    _ditheringContext.advanceX(x, diff);
}

// BLPipeline::JIT::FetchRadialGradientPart - Fetch
// ================================================

void FetchRadialGradientPart::prefetch1() noexcept {
  pc->v_cvt_f64_f32(f->value, f->d_b);
  pc->v_and_f32(f->value, f->value, pc->simdConst(&ct.f32_abs_lo, Bcst::kNA, f->value));
  pc->s_sqrt_f32(f->value, f->value, f->value);
}

void FetchRadialGradientPart::prefetchN() noexcept {
  x86::Vec& d_b = f->d_b;
  x86::Vec& dd_bd = f->dd_bd;
  x86::Vec& ddd = f->ddd;
  x86::Vec& value = f->value;

  x86::Vec x0 = cc->newXmmSd("f.x0");
  x86::Vec x1 = cc->newXmmSd("f.x1");
  x86::Vec x2 = cc->newXmmSd("f.x2");

  pc->v_mov(f->d_b_prev, f->d_b);     // Save `d_b`.
  pc->v_mov(f->dd_bd_prev, f->dd_bd); // Save `dd_bd`.

  pc->v_cvt_f64_f32(x0, d_b);
  pc->v_add_f64(d_b, d_b, dd_bd);
  pc->s_add_f64(dd_bd, dd_bd, ddd);

  pc->v_cvt_f64_f32(x1, d_b);
  pc->v_add_f64(d_b, d_b, dd_bd);
  pc->s_add_f64(dd_bd, dd_bd, ddd);
  pc->v_shuffle_f32(x0, x0, x1, x86::shuffleImm(1, 0, 1, 0));

  pc->v_cvt_f64_f32(x1, d_b);
  pc->v_add_f64(d_b, d_b, dd_bd);
  pc->s_add_f64(dd_bd, dd_bd, ddd);

  pc->v_cvt_f64_f32(x2, d_b);
  pc->v_add_f64(d_b, d_b, dd_bd);
  pc->s_add_f64(dd_bd, dd_bd, ddd);
  pc->v_shuffle_f32(x1, x1, x2, x86::shuffleImm(1, 0, 1, 0));

  pc->v_shuffle_f32(value, x0, x1, x86::shuffleImm(2, 0, 2, 0));
  pc->v_and_f32(value, value, pc->simdConst(&ct.f32_abs, Bcst::k32, value));
  pc->v_sqrt_f32(value, value);

  pc->v_shuffle_f32(x0, x0, x1, x86::shuffleImm(3, 1, 3, 1));
  pc->v_add_f32(value, value, x0);
}

void FetchRadialGradientPart::postfetchN() noexcept {
  pc->v_mov(f->d_b, f->d_b_prev);     // Restore `d_b`.
  pc->v_mov(f->dd_bd, f->dd_bd_prev); // Restore `dd_bd`.
}

void FetchRadialGradientPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  BL_ASSERT(predicate.empty());
  blUnused(predicate);

  p.setCount(n);

  switch (n.value()) {
    case 1: {
      x86::Xmm x0 = cc->newXmmPs("f.x0");
      x86::Gp gIdx = cc->newInt32("f.gIdx");

      pc->v_swizzle_u32(x0, f->value, x86::shuffleImm(1, 1, 1, 1));
      pc->v_add_f64(f->d_b, f->d_b, f->dd_bd);

      pc->s_add_f32(x0, x0, f->value);
      pc->v_cvt_f64_f32(f->value, f->d_b);

      pc->s_mul_f32(x0, x0, f->scale);
      pc->v_and_f32(f->value, f->value, pc->simdConst(&ct.f32_abs_lo, Bcst::kNA, f->value));

      pc->v_cvtt_f32_i32(x0, x0);

      pc->s_add_f64(f->dd_bd, f->dd_bd, f->ddd);
      pc->s_sqrt_f32(f->value, f->value, f->value);

      x86::Xmm vIdx = cc->newXmm("f.vIdx");
      if (isPad() && pc->hasSSE4_1()) {
        pc->v_packs_i32_u16_(vIdx, x0, x0);
        pc->v_min_u16(vIdx, vIdx, f->vmaxi.xmm());
      }
      else if (isPad()) {
        pc->v_packs_i32_i16(vIdx, x0, x0);
        pc->v_min_i16(vIdx, vIdx, f->vmaxi.xmm());
        pc->v_max_i16(vIdx, vIdx, pc->simdConst(&ct.i_0000000000000000, Bcst::kNA, vIdx));
      }
      else {
        x86::Xmm vTmp = cc->newXmm("f.vTmp");
        pc->v_and_i32(vIdx, x0, f->vmaxi.xmm());
        pc->v_xor_i32(vTmp, vIdx, f->vrori.xmm());
        pc->v_min_i16(vIdx, vIdx, vTmp);
      }

      pc->v_extract_u16(gIdx, vIdx, 0u);
      fetchSinglePixel(p, flags, gIdx);
      pc->x_satisfy_pixel(p, flags);
      break;
    }

    case 4: {
      x86::Vec& d_b   = f->d_b;
      x86::Vec& dd_bd = f->dd_bd;
      x86::Vec& ddd   = f->ddd;
      x86::Vec& value = f->value;

      x86::Vec x0 = cc->newXmmSd("f.x0");
      x86::Vec x1 = cc->newXmmSd("f.x1");
      x86::Vec x2 = cc->newXmmSd("f.x2");
      x86::Vec x3 = cc->newXmmSd("f.x3");

      pc->v_mul_f32(value, value, f->scale);
      pc->v_cvt_f64_f32(x0, d_b);

      pc->v_mov(f->d_b_prev, d_b);     // Save `d_b_prev`.
      pc->v_mov(f->dd_bd_prev, dd_bd); // Save `dd_bd_prev`.

      pc->v_add_f64(d_b, d_b, dd_bd);
      pc->s_add_f64(dd_bd, dd_bd, ddd);

      pc->v_cvt_f64_f32(x1, d_b);
      pc->v_add_f64(d_b, d_b, dd_bd);

      pc->v_cvt_f32_i32(x3, value);
      pc->s_add_f64(dd_bd, dd_bd, ddd);

      IndexLayout indexLayout = IndexLayout::kUInt16;
      x86::Xmm vIdx = cc->newXmm("vIdx");

      if (isPad() && pc->hasSSE4_1()) {
        pc->v_packs_i32_u16_(vIdx, x3, x3);
        pc->v_min_u16(vIdx, vIdx, f->vmaxi.xmm());
      }
      else if (isPad()) {
        pc->v_packs_i32_i16(vIdx, x3, x3);
        pc->v_min_i16(vIdx, vIdx, f->vmaxi.xmm());
        pc->v_max_i16(vIdx, vIdx, pc->simdConst(&ct.i_0000000000000000, Bcst::kNA, vIdx));
      }
      else {
        indexLayout = IndexLayout::kUInt32Lo16;
        x86::Xmm vTmp = cc->newXmm("f.vTmp");

        pc->v_and_i32(vIdx, x3, f->vmaxi.xmm());
        pc->v_xor_i32(vTmp, vIdx, f->vrori.xmm());
        pc->v_min_i16(vIdx, vIdx, vTmp);
      }

      fetchMultiplePixels(p, n, flags, vIdx, indexLayout, [&](uint32_t step) noexcept {
        switch (step) {
          case 0: pc->v_shuffle_f32(x0, x0, x1, x86::shuffleImm(1, 0, 1, 0));
                  pc->v_cvt_f64_f32(x1, d_b);
                  pc->v_add_f64(d_b, d_b, dd_bd);
                  break;
          case 1: pc->v_mov(value, x0);
                  pc->v_cvt_f64_f32(x2, d_b);
                  pc->s_add_f64(dd_bd, dd_bd, ddd);
                  break;
          case 2: pc->v_shuffle_f32(x1, x1, x2, x86::shuffleImm(1, 0, 1, 0));
                  pc->v_shuffle_f32(x0, x0, x1, x86::shuffleImm(2, 0, 2, 0));
                  pc->v_and_f32(x0, x0, pc->simdConst(&ct.f32_abs, Bcst::k32, x0));
                  break;
          case 3: pc->v_sqrt_f32(x0, x0);
                  pc->v_add_f64(d_b, d_b, dd_bd);
                  pc->v_shuffle_f32(value, value, x1, x86::shuffleImm(3, 1, 3, 1));
                  pc->s_add_f64(dd_bd, dd_bd, ddd);
                  break;
        }
      });

      pc->x_satisfy_pixel(p, flags);
      pc->v_add_f32(value, value, x0);
      break;
    }

    case 8: {
      _fetch2x4(p, flags);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void FetchRadialGradientPart::precalc(const x86::Vec& px_py) noexcept {
  x86::Vec& d_b   = f->d_b;
  x86::Vec& dd_bd = f->dd_bd;

  x86::Vec x0 = cc->newXmmPd("f.x0");
  x86::Vec x1 = cc->newXmmPd("f.x1");
  x86::Vec x2 = cc->newXmmPd("f.x2");

  pc->v_mul_f64(d_b, px_py, f->ax_ay);                   // [Ax.Px                             | Ay.Py         ]
  pc->v_mul_f64(x0, px_py, f->fx_fy);                    // [Fx.Px                             | Fy.Py         ]
  pc->v_mul_f64(x1, px_py, f->ddx_ddy);                  // [Ddx.Px                            | Ddy.Py        ]

  pc->v_mul_f64(d_b, d_b, px_py);                        // [Ax.Px^2                           | Ay.Py^2       ]
  pc->v_hadd_f64(d_b, d_b, x0);                          // [Ax.Px^2 + Ay.Py^2                 | Fx.Px + Fy.Py ]

  pc->v_swap_f64(x2, x0);
  pc->s_mul_f64(x2, x2, x0);                             // [Fx.Px.Fy.Py                       | ?             ]
  pc->s_add_f64(x2, x2, x2);                             // [2.Fx.Px.Fy.Py                     | ?             ]
  pc->s_add_f64(d_b, d_b, x2);                           // [Ax.Px^2 + Ay.Py^2 + 2.Fx.Px.Fy.Py | Fx.Px + Fy.Py ]
  pc->s_add_f64(dd_bd, f->da_ba, x1);                    // [Dd + Ddx.Px                       | Bd            ]

  pc->v_swap_f64(x1, x1);
  pc->s_add_f64(dd_bd, dd_bd, x1);                       // [Dd + Ddx.Px + Ddy.Py              | Bd            ]
}

// BLPipeline::JIT::FetchConicGradientPart - Construction & Destruction
// ====================================================================

FetchConicGradientPart::FetchConicGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept
  : FetchGradientPart(pc, fetchType, format) {

  _partFlags |= PipePartFlags::kMaskedAccess | PipePartFlags::kAdvanceXNeedsX;
  _isComplexFetch = true;
  _maxSimdWidthSupported = SimdWidth::k512;

  setDitheringEnabled(fetchType == FetchType::kGradientConicDither);
  JitUtils::resetVarStruct(&f, sizeof(f));
}

// BLPipeline::JIT::FetchConicGradientPart - Prepare
// =================================================

void FetchConicGradientPart::preparePart() noexcept {
  _maxPixels = uint8_t(4 * pc->simdMultiplier());
}

// BLPipeline::JIT::FetchConicGradientPart - Init & Fini
// =====================================================

void FetchConicGradientPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  // Local Registers
  // ---------------

  _tablePtr = cc->newIntPtr("f.table");               // Reg.
  f->consts = cc->newIntPtr("f.consts");              // Reg.
  f->px = cc->newXmmPd("f.px");                       // Reg.
  f->xx = cc->newXmmPd("f.xx");                       // Reg/Mem.
  f->hx_hy = cc->newXmmPd("f.hx_hy");                 // Reg. (TODO: Make spillable).
  f->yx_yy = cc->newXmmPd("f.yx_yy");                 // Mem.
  f->ay = pc->newVec("f.ay");                         // Reg/Mem.
  f->by = pc->newVec("f.by");                         // Reg/Mem.

  f->angleOffset = pc->newVec("f.angleOffset");       // Reg/Mem.
  f->maxi = pc->newVec("f.maxi");                     // Reg/Mem.

  f->t0 = pc->newVec("f.t0");                         // Reg/Tmp.
  f->t1 = pc->newVec("f.t1");                         // Reg/Tmp.
  f->t2 = pc->newVec("f.t2");                         // Reg/Tmp.
  f->t1Pred = cc->newKw("f.t1Pred");

  x86::Xmm off = cc->newXmmPd("f.off");               // Initialization only.

  // Part Initialization
  // -------------------

  cc->mov(_tablePtr, x86::ptr(pc->_fetchData, REL_GRADIENT(lut.data)));

  if (ditheringEnabled())
    _ditheringContext.initY(x, y);

  pc->v_zero_d(f->hx_hy);
  pc->s_cvt_int_f64(f->hx_hy, f->hx_hy, y);

  pc->v_dupl_f64(f->xx, x86::ptr(pc->_fetchData, REL_GRADIENT(conic.xx)));
  pc->v_loadu_d128(f->yx_yy, x86::ptr(pc->_fetchData, REL_GRADIENT(conic.yx)));
  pc->v_loadu_d128(off, x86::ptr(pc->_fetchData, REL_GRADIENT(conic.ox)));

  pc->v_broadcast_u32(f->maxi, x86::ptr(pc->_fetchData, REL_GRADIENT(conic.maxi)));
  pc->v_broadcast_u32(f->angleOffset, x86::ptr(pc->_fetchData, REL_GRADIENT(conic.offset)));

  pc->v_dupl_f64(f->hx_hy, f->hx_hy);
  pc->v_mul_f64(f->hx_hy, f->hx_hy, f->yx_yy);
  pc->v_add_f64(f->hx_hy, f->hx_hy, off);

  cc->mov(f->consts, x86::ptr(pc->_fetchData, REL_GRADIENT(conic.consts)));

  if (isRectFill()) {
    pc->v_zero_d(off);
    pc->s_cvt_int_f64(off, off, x);
    pc->s_mul_f64(off, off, f->xx);
    pc->s_add_f64(f->hx_hy, f->hx_hy, off);
  }

  // Setup constants used by 4+ pixel fetches.
  if (maxPixels() > 1) {
    f->xx_inc = pc->newXmm("f.xx_inc"); // Reg/Mem.
    f->xx_off = pc->newVec("f.xx_off"); // Reg/Mem.

    pc->v_cvt_f64_f32(f->xx_off.xmm(), f->xx);

    if (maxPixels() == 4)
      pc->v_mul_f64(f->xx_inc, f->xx, pc->simdMemConst(&ct.f64_4_8, Bcst::k32, f->xx_inc));
    else
      pc->v_mul_f64(f->xx_inc, f->xx, pc->simdMemConst(&ct.f64_8_4, Bcst::k32, f->xx_inc));

    pc->v_broadcast_u32(f->xx_off, f->xx_off);
    pc->v_mul_f32(f->xx_off, f->xx_off, pc->simdMemConst(&ct.f32_increments, Bcst::kNA, f->xx_off));
  }
}

void FetchConicGradientPart::_finiPart() noexcept {}

// BLPipeline::JIT::FetchConicGradientPart - Advance
// =================================================

void FetchConicGradientPart::advanceY() noexcept {
  pc->v_add_f64(f->hx_hy, f->hx_hy, f->yx_yy);

  if (ditheringEnabled())
    _ditheringContext.advanceY();
}

void FetchConicGradientPart::startAtX(const x86::Gp& x) noexcept {
  pc->v_cvt_f64_f32(f->by.xmm(), f->hx_hy);
  pc->v_swizzle_f32(f->by.xmm(), f->by.xmm(), x86::shuffleImm(1, 1, 1, 1));

  if (!f->by.isXmm()) {
    pc->v_broadcast_f32x4(f->by, f->by.xmm());
  }

  pc->v_and_f32(f->ay, f->by, pc->simdConst(&ct.f32_abs, Bcst::kNA, f->ay));
  pc->v_sra_i32(f->by, f->by, 31);
  pc->v_and_f32(f->by, f->by, x86::ptr(f->consts, BL_OFFSET_OF(BLCommonTable::Conic, n_div_1)));

  advanceX(x, pc->_gpNone);
}

void FetchConicGradientPart::advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept {
  blUnused(diff);

  if (isRectFill()) {
    pc->v_dupl_f64(f->px, f->hx_hy);
  }
  else {
    pc->v_zero_d(f->px);
    pc->s_cvt_int_f64(f->px, f->px, x);
    pc->s_mul_f64(f->px, f->px, f->xx);
    pc->s_add_f64(f->px, f->px, f->hx_hy);
  }

  recalcX();

  if (ditheringEnabled())
    _ditheringContext.startAtX(x);
}

void FetchConicGradientPart::recalcX() noexcept {
  pc->v_cvt_f64_f32(f->t0.xmm(), f->px);

  if (maxPixels() == 1) {
    x86::Vec t0 = f->t0.xmm();
    x86::Vec t1 = f->t1.xmm();
    x86::Vec t2 = f->t2.xmm();
    x86::Vec ay = f->ay.xmm();
    x86::Vec tmp = cc->newXmm("f.tmp");

    pc->v_and_f32(t1, t0, pc->simdConst(&ct.f32_abs, Bcst::k32, t1));
    pc->s_max_f32(tmp, t1, ay);
    pc->s_min_f32(t2, t1, ay);

    pc->s_cmp_f32(t1, t1, t2, x86::VCmpImm::kEQ_UQ);
    pc->s_div_f32(t2, t2, tmp);

    pc->v_sra_i32(t0, t0, 31);
    pc->v_and_f32(t1, t1, x86::ptr(f->consts, BL_OFFSET_OF(BLCommonTable::Conic, n_div_4)));
  }
  else {
    x86::Vec t0 = f->t0;
    x86::Vec t1 = f->t1;
    x86::Vec t2 = f->t2;
    x86::Vec ay = f->ay;
    x86::Vec tmp = cc->newSimilarReg(f->t0, "f.tmp");

    pc->v_broadcast_u32(t0, t0);
    pc->v_add_f32(t0, t0, f->xx_off);
    pc->v_and_f32(t1, t0, pc->simdConst(&ct.f32_abs, Bcst::k32, t1));

    pc->v_max_f32(tmp, t1, ay);
    pc->v_min_f32(t2, t1, ay);

    if (pc->hasAVX512())
      pc->v_cmp_f32(f->t1Pred, t1, t2, x86::VCmpImm::kEQ_UQ);
    else
      pc->v_cmp_f32(t1, t1, t2, x86::VCmpImm::kEQ_UQ);

    pc->v_div_f32(t2, t2, tmp);
  }
}

// BLPipeline::JIT::FetchConicGradientPart - Fetch
// ===============================================

void FetchConicGradientPart::prefetchN() noexcept {}

void FetchConicGradientPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  p.setCount(n);

  x86::Gp consts = f->consts;
  x86::Xmm px = f->px;

  x86::Vec t0 = f->t0;
  x86::Vec t1 = f->t1;
  x86::Vec t2 = f->t2;

  // Use 128-bit SIMD if the number of pixels is 4 or less.
  if (n.value() <= 4) {
    t0 = t0.xmm();
    t1 = t1.xmm();
    t2 = t2.xmm();
  }

  x86::Vec t3 = cc->newSimilarReg(t0, "f.t3");
  x86::Vec t4 = cc->newSimilarReg(t0, "f.t4");

  switch (n.value()) {
    case 1: {
      x86::Gp idx = cc->newIntPtr("f.idx");

      pc->s_mul_f32(t3, t2, t2);
      pc->v_sra_i32(t0, t0, 31);
      pc->v_load_f32(t4, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conic, q3)));

      pc->s_mul_12_add_3(t4, t3, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conic, q2)));
      pc->v_and_f32(t1, t1, x86::ptr(f->consts, BL_OFFSET_OF(BLCommonTable::Conic, n_div_4)));
      pc->s_mul_12_add_3(t4, t3, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conic, q1)));
      pc->v_and_f32(t0, t0, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conic, n_div_2)));
      pc->s_mul_12_add_3(t4, t3, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conic, q0)));
      pc->s_mul_12_sub_3(t4, t2, t1);

      pc->v_and_f32(t4, t4, pc->simdConst(&ct.f32_abs, Bcst::k32, t4));
      pc->s_sub_f32(t4, t4, t0);
      pc->v_and_f32(t4, t4, pc->simdConst(&ct.f32_abs, Bcst::k32, t4));

      pc->s_sub_f32(t4, t4, f->by);
      pc->v_and_f32(t4, t4, pc->simdConst(&ct.f32_abs, Bcst::k32, t4));
      pc->s_add_f32(t4, t4, f->angleOffset.xmm());

      pc->v_cvtt_f32_i32(t4, t4);
      pc->s_add_f64(px, px, f->xx);
      pc->v_and_i32(t4, t4, f->maxi.xmm());
      pc->s_mov_i32(idx.r32(), t4);

      recalcX();
      fetchSinglePixel(p, flags, idx);
      pc->x_satisfy_pixel(p, flags);
      break;
    }

    case 4:
    case 8:
    case 16: {
      pc->v_mul_f32(t3, t2, t2);
      pc->v_sra_i32(t0, t0, 31);
      pc->v_mov(t4, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conic, q3)));

      if (pc->hasAVX512())
        cc->k(f->t1Pred).z().vmovdqa32(t1, x86::ptr(f->consts, BL_OFFSET_OF(BLCommonTable::Conic, n_div_4)));
      else
        pc->v_and_f32(t1, t1, x86::ptr(f->consts, BL_OFFSET_OF(BLCommonTable::Conic, n_div_4)));

      pc->v_mul_12_add_3(t4, t3, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conic, q2)));
      pc->v_and_f32(t0, t0, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conic, n_div_2)));
      pc->v_mul_12_add_3(t4, t3, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conic, q1)));
      pc->v_mul_12_add_3(t4, t3, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conic, q0)));
      pc->v_mul_12_sub_3(t4, t2, t1);

      pc->v_and_f32(t4, t4, pc->simdConst(&ct.f32_abs, Bcst::k32, t4));
      pc->v_sub_f32(t4, t4, t0);
      pc->v_and_f32(t4, t4, pc->simdConst(&ct.f32_abs, Bcst::k32, t4));

      pc->v_sub_f32(t4, t4, f->by.cloneAs(t4));
      pc->v_and_f32(t4, t4, pc->simdConst(&ct.f32_abs, Bcst::k32, t4));
      pc->v_add_f32(t4, t4, f->angleOffset.cloneAs(t4));

      pc->v_cvtt_f32_i32(t4, t4);
      pc->v_and_i32(t4, t4, f->maxi.cloneAs(t4));

      fetchMultiplePixels(p, n, flags, t4, IndexLayout::kUInt32Lo16, [&](uint32_t step) noexcept {
        // Don't recalculate anything if this is predicated load, because this is either the end or X will be advanced.
        if (!predicate.empty())
          return;

        switch (step) {
          case 1: if (maxPixels() >= 8 && n.value() == 4) {
                    x86::Xmm tmp = cc->newXmm("f.tmp");
                    pc->v_duph_f64(tmp, f->xx_inc);
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

      pc->x_satisfy_pixel(p, flags);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

} // {JIT}
} // {BLPipeline}

#endif
