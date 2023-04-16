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

namespace BLPipeline {
namespace JIT {

#define REL_GRADIENT(FIELD) BL_OFFSET_OF(FetchData::Gradient, FIELD)

// BLPipeline::JIT::FetchGradientPart - Construction & Destruction
// ===============================================================

FetchGradientPart::FetchGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept
  : FetchPart(pc, fetchType, format) {

  _partFlags |= PipePartFlags::kAdvanceXNeedsDiff;
}

void FetchGradientPart::fetchGradientPixel1(Pixel& dst, PixelFlags flags, const x86::Mem& src) noexcept {
  pc->x_fetch_pixel(dst, PixelCount(1), flags, BLInternalFormat::kPRGB32, src, Alignment(4));
}

// BLPipeline::JIT::FetchLinearGradientPart - Construction & Destruction
// =====================================================================

FetchLinearGradientPart::FetchLinearGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept
  : FetchGradientPart(pc, fetchType, format),
    _isRoR(fetchType == FetchType::kGradientLinearRoR) {

  _maxSimdWidthSupported = SimdWidth::k256;
  _extendMode = ExtendMode(uint32_t(fetchType) - uint32_t(FetchType::kGradientLinearPad));
  JitUtils::resetVarStruct(&f, sizeof(f));
}

// BLPipeline::JIT::FetchLinearGradientPart - Prepare
// ==================================================

void FetchLinearGradientPart::preparePart() noexcept {
  _maxPixels = 8;
}

// BLPipeline::JIT::FetchLinearGradientPart - Init & Fini
// ======================================================

void FetchLinearGradientPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  const BLCommonTable& c = blCommonTable;

  // Local Registers
  // ---------------

  f->table = cc->newIntPtr("f.table");                // Reg.
  f->pt = pc->newVec("f.pt");                         // Reg.
  f->dt = pc->newVec("f.dt");                         // Reg/Mem.
  f->dtN = pc->newVec("f.dtN");                       // Reg/Mem.
  f->py = pc->newVec("f.py");                         // Reg/Mem.
  f->dy = pc->newVec("f.dy");                         // Reg/Mem.
  f->rep = pc->newVec("f.rep");                       // Reg/Mem [RoR only].
  f->msk = pc->newVec("f.msk");                       // Reg/Mem.
  f->vIdx = pc->newVec("f.vIdx");                     // Reg/Tmp.

  // In 64-bit mode it's easier to use imul for 64-bit multiplication instead of SIMD, because
  // we need to multiply a scalar anyway that we then broadcast and add to our 'f.pt' vector.
  if (cc->is64Bit()) {
    f->dtGp = cc->newUInt64("f.dtGp");                // Reg/Mem.
  }

  // Part Initialization
  // -------------------

  cc->mov(f->table, x86::ptr(pc->_fetchData, REL_GRADIENT(lut.data)));

  pc->s_mov_i32(f->py, y);
  pc->v_broadcast_u64(f->dy, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.dy.u64)));
  pc->v_broadcast_u64(f->py, f->py);
  pc->v_mul_u64_u32_lo(f->py, f->dy, f->py);
  pc->v_broadcast_u64(f->dt, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.dt.u64)));

  if (isPad()) {
    pc->v_broadcast_u32(f->msk, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.msk.u)));
  }
  else {
    pc->v_broadcast_u32(f->rep, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.rep.u)));
    pc->v_broadcast_u32(f->msk, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.msk.u)));
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

  // If we cannot use `packusdw`, which was introduced by SSE4.1 we subtract 32768 from the pointer
  // and use `packssdw` instead. However, if we do this, we have to adjust everything else accordingly.
  if (isPad() && !pc->hasSSE4_1()) {
    pc->v_sub_i32(f->py, f->py, pc->simdConst(&c.i_0000800000008000, Bcst::k32, f->py));
    pc->v_sub_i16(f->msk, f->msk, pc->simdConst(&c.i_8000800080008000, Bcst::kNA, f->msk));
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
}

void FetchLinearGradientPart::startAtX(const x86::Gp& x) noexcept {
  if (!isRectFill()) {
    calcAdvanceX(f->pt, x);
    pc->v_add_i64(f->pt, f->pt, f->py);
  }
  else {
    pc->v_mov(f->pt, f->py);
  }
}

void FetchLinearGradientPart::advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept {
  blUnused(x);

  x86::Vec adv = cc->newSimilarReg(f->pt, "f.adv");
  calcAdvanceX(adv, diff);
  pc->v_add_i64(f->pt, f->pt, adv);
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
      pc->v_min_u16(vIdx, vIdx, f->msk);
    }
    else {
      x86::Vec vTmp = cc->newSimilarReg(f->vIdx, "f.vTmp");
      pc->v_and_i32(vIdx, f->pt, f->rep);
      pc->v_add_i64(f->pt, f->pt, f->dtN);
      pc->v_and_i32(vTmp, f->pt, f->rep);
      pc->v_packs_i32_u16_(vIdx, vIdx, vTmp);
      pc->v_xor_i32(vTmp, vIdx, f->msk);
      pc->v_min_u16(vIdx, vIdx, vTmp);
    }

    pc->v_perm_i64(vIdx, vIdx, x86::shuffleImm(3, 1, 2, 0));
  }
  else {
    pc->v_mov(vIdx, f->pt);
    pc->v_add_i64(f->pt, f->pt, f->dtN);
    pc->v_shuffle_i32(vIdx, vIdx, f->pt, x86::shuffleImm(3, 1, 3, 1));
  }
}

void FetchLinearGradientPart::postfetchN() noexcept {
  pc->v_sub_i64(f->pt, f->pt, f->dtN);
}

void FetchLinearGradientPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  BL_ASSERT(predicate.empty());
  blUnused(predicate);

  const BLCommonTable& c = blCommonTable;
  p.setCount(n);

  uint32_t srcShift = 2;

  switch (n.value()) {
    case 1: {
      x86::Gp gIdx = cc->newInt32("f.gIdx");
      x86::Xmm vIdx = cc->newXmm("f.vIdx");
      uint32_t vIdxLane = 1u + uint32_t(!isPad());

      if (isPad() && pc->hasSSE4_1()) {
        pc->v_packs_i32_u16_(vIdx, f->pt.xmm(), f->pt.xmm());
        pc->v_min_u16(vIdx, vIdx, f->msk.xmm());
      }
      else if (isPad()) {
        pc->v_packs_i32_i16(vIdx, f->pt.xmm(), f->pt.xmm());
        pc->v_min_i16(vIdx, vIdx, f->msk.xmm());
        pc->v_add_i16(vIdx, vIdx, pc->simdConst(&c.i_8000800080008000, Bcst::kNA, vIdx));
      }
      else {
        x86::Xmm vTmp = cc->newXmm("f.vTmp");
        pc->v_and_i32(vIdx, f->pt.xmm(), f->rep.xmm());
        pc->v_xor_i32(vTmp, vIdx, f->msk.xmm());
        pc->v_min_i16(vIdx, vIdx, vTmp);
      }

      pc->v_add_i64(f->pt, f->pt, f->dt);
      pc->v_extract_u16(gIdx, vIdx, vIdxLane);
      fetchGradientPixel1(p, flags, x86::ptr(f->table, gIdx, 2));
      pc->x_satisfy_pixel(p, flags);
      break;
    }

    case 4: {
      if (pc->simdWidth() >= SimdWidth::k256) {
        x86::Vec vIdx = f->vIdx;
        x86::Vec vTmp = pc->newYmm("f.vTmp");

        FetchUtils::x_gather_pixels(pc, p, n, format(), flags, x86::ptr(f->table), vIdx.xmm(), srcShift, IndexLayout::kHigh16Bits, [&](uint32_t step) {
          if (isPad()) {
            switch (step) {
              case 0: pc->v_mov(vIdx, f->pt); break;
              case 1: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 2: pc->v_packs_i32_u16_(vIdx, vIdx, f->pt); break;
              case 3: pc->v_min_u16(vIdx, vIdx, f->msk);
                      pc->v_perm_i64(vIdx, vIdx, x86::shuffleImm(3, 1, 2, 0)); break;
            }
          }
          else {
            switch (step) {
              case 0: pc->v_and_i32(vIdx, f->pt, f->rep);
                      pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 1: pc->v_and_i32(vTmp, f->pt, f->rep);
                      pc->v_packs_i32_u16_(vIdx, vIdx, vTmp); break;
              case 2: pc->v_xor_i32(vTmp, vIdx, f->msk);
                      pc->v_min_u16(vIdx, vIdx, vTmp); break;
              case 3: pc->v_perm_i64(vIdx, vIdx, x86::shuffleImm(3, 1, 2, 0)); break;
            }
          }
        });
      }
      else {
        static const uint8_t srcIndexesPad[4] = { 0, 1, 2, 3 };
        static const uint8_t srcIndexesRoR[4] = { 0, 2, 4, 6 };

        x86::Vec vIdx = f->vIdx;
        const uint8_t* srcIndexes = isPad() ? srcIndexesPad : srcIndexesRoR;

        if (isPad() && pc->hasSSE4_1()) {
          pc->v_packs_i32_u16_(vIdx, vIdx, vIdx);
          pc->v_min_u16(vIdx, vIdx, f->msk);
        }
        else if (isPad()) {
          pc->v_packs_i32_i16(vIdx, vIdx, vIdx);
          pc->v_min_i16(vIdx, vIdx, f->msk);
          pc->v_add_i16(vIdx, vIdx, pc->simdConst(&c.i_8000800080008000, Bcst::kNA, vIdx));
        }
        else {
          x86::Xmm vTmp = cc->newXmm("f.vTmp");
          pc->v_and_i32(vIdx, vIdx, f->rep);
          pc->v_xor_i32(vTmp, vIdx, f->msk);
          pc->v_min_i16(vTmp, vTmp, vIdx);
        }

        pc->v_add_i64(f->pt, f->pt, f->dtN);

        IndexExtractor iExt(pc);
        iExt.begin(IndexExtractor::kTypeUInt16, vIdx);

        FetchContext fCtx(pc, &p, n, format(), flags);
        fCtx.fetchAll(x86::ptr(f->table), srcShift, iExt, srcIndexes, [&](uint32_t step) {
          switch (step) {
            case 1: pc->v_mov(vIdx, f->pt); break;
            case 2: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
            case 3: pc->v_shuffle_i32(vIdx, vIdx, f->pt, x86::shuffleImm(3, 1, 3, 1)); break;
          }
        });
        fCtx.end();
      }

      pc->x_satisfy_pixel(p, flags);
      break;
    }

    case 8: {
      if (pc->simdWidth() >= SimdWidth::k256) {
        x86::Vec vIdx = f->vIdx;
        x86::Vec vTmp = pc->newYmm("f.vTmp");

        FetchUtils::x_gather_pixels(pc, p, n, format(), flags, x86::ptr(f->table), vIdx, srcShift, IndexLayout::kHigh16Bits, [&](uint32_t step) {
          if (isPad()) {
            switch (step) {
              case 0: pc->v_add_i64(vIdx, f->pt, f->dtN); break;
              case 1: pc->v_add_i64(f->pt, vIdx, f->dtN); break;
              case 2: pc->v_packs_i32_u16_(vIdx, vIdx, f->pt); break;
              case 3: pc->v_min_u16(vIdx, vIdx, f->msk); break;
              case 4: pc->v_perm_i64(vIdx, vIdx, x86::shuffleImm(3, 1, 2, 0)); break;
            }
          }
          else {
            switch (step) {
              case 0: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 1: pc->v_and_i32(vIdx, f->pt, f->rep); break;
              case 2: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
              case 3: pc->v_and_i32(vTmp, f->pt, f->rep); break;
              case 4: pc->v_packs_i32_u16_(vIdx, vIdx, vTmp); break;
              case 5: pc->v_xor_i32(vTmp, vIdx, f->msk); break;
              case 6: pc->v_min_u16(vIdx, vIdx, vTmp); break;
              case 7: pc->v_perm_i64(vIdx, vIdx, x86::shuffleImm(3, 1, 2, 0)); break;
            }
          }
        });
      }
      else {
        x86::Vec vIdx = f->vIdx;
        x86::Vec vTmp = cc->newXmm("f.vTmp");

        static const uint8_t srcIndexes[8] = { 4, 5, 6, 7, 0, 1, 2, 3 };

        pc->v_add_i64(f->pt, f->pt, f->dtN);
        pc->v_mov(vTmp, f->pt);
        pc->v_add_i64(f->pt, f->pt, f->dtN);
        pc->v_shuffle_i32(vTmp, vTmp, f->pt, x86::shuffleImm(3, 1, 3, 1));

        if (isPad() && pc->hasSSE4_1()) {
          pc->v_packs_i32_u16_(vTmp, vTmp, vIdx);
          pc->v_min_u16(vTmp, vTmp, f->msk);
        }
        else if (isPad()) {
          pc->v_packs_i32_i16(vTmp, vTmp, vIdx);
          pc->v_min_i16(vTmp, vTmp, f->msk);
          pc->v_add_i16(vTmp, vTmp, pc->simdConst(&c.i_8000800080008000, Bcst::kNA, vTmp));
        }
        else {
          pc->v_and_i32(vIdx, vIdx, f->rep);
          pc->v_and_i32(vTmp, vTmp, f->rep);
          pc->v_packs_i32_i16(vTmp, vTmp, vIdx);
          pc->v_xor_i32(vIdx, vTmp, f->msk);
          pc->v_min_i16(vTmp, vTmp, vIdx);
        }

        IndexExtractor iExt(pc);
        iExt.begin(IndexExtractor::kTypeUInt16, vTmp);

        FetchContext fCtx(pc, &p, n, format(), flags);
        fCtx.fetchAll(x86::ptr(f->table), srcShift, iExt, srcIndexes, [&](uint32_t step) {
          switch (step) {
            case 1: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
            case 3: pc->v_mov(vIdx, f->pt); break;
            case 5: pc->v_add_i64(f->pt, f->pt, f->dtN); break;
            case 7: pc->v_shuffle_i32(vIdx, vIdx, f->pt, x86::shuffleImm(3, 1, 3, 1)); break;
          }
        });
        fCtx.end();
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
  _extendMode = ExtendMode(uint32_t(fetchType) - uint32_t(FetchType::kGradientRadialPad));

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

  f->table = cc->newIntPtr("f.table");                // Reg.
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

  f->maxi = cc->newUInt32("f.maxi");                  // Mem.
  f->vmaxi = cc->newXmm("f.vmaxi");                   // Mem.
  f->vmaxf = cc->newXmmPd("f.vmaxf");                 // Mem.

  f->d_b_prev = cc->newXmmPd("f.d_b_prev");           // Mem.
  f->dd_bd_prev = cc->newXmmPd("f.dd_bd_prev");       // Mem.

  x86::Xmm off = cc->newXmmPd("f.off");               // Initialization only.

  // Part Initialization
  // -------------------

  cc->mov(f->table, x86::ptr(pc->_fetchData, REL_GRADIENT(lut.data)));

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

  pc->v_load_i32(f->vmaxi, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.maxi)));
  pc->v_expand_lo_i32(f->vmaxi, f->vmaxi);
  pc->s_mov_i32(f->maxi, f->vmaxi);

  if (extendMode() == ExtendMode::kPad) {
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
}

void FetchRadialGradientPart::advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept {
  blUnused(diff);

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
}

// BLPipeline::JIT::FetchRadialGradientPart - Fetch
// ================================================

void FetchRadialGradientPart::prefetch1() noexcept {
  const BLCommonTable& c = blCommonTable;

  pc->v_cvt_f64_f32(f->value, f->d_b);
  pc->v_and_f32(f->value, f->value, pc->simdConst(&c.f32_abs_lo, Bcst::kNA, f->value));
  pc->s_sqrt_f32(f->value, f->value, f->value);
}

void FetchRadialGradientPart::prefetchN() noexcept {
  const BLCommonTable& c = blCommonTable;

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
  pc->v_and_f32(value, value, pc->simdConst(&c.f32_abs, Bcst::k32, value));
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

  const BLCommonTable& c = blCommonTable;
  p.setCount(n);

  switch (n.value()) {
    case 1: {
      x86::Xmm x0 = cc->newXmmPs("f.x0");
      x86::Gp gIdx = cc->newInt32("f.gIdx");

      pc->v_swizzle_i32(x0, f->value, x86::shuffleImm(1, 1, 1, 1));
      pc->v_add_f64(f->d_b, f->d_b, f->dd_bd);

      pc->s_add_f32(x0, x0, f->value);
      pc->v_cvt_f64_f32(f->value, f->d_b);

      pc->s_mul_f32(x0, x0, f->scale);
      pc->v_and_f32(f->value, f->value, pc->simdConst(&c.f32_abs_lo, Bcst::kNA, f->value));

      if (extendMode() == ExtendMode::kPad) {
        pc->s_max_f32(x0, x0, pc->simdConst(&c.i_0000000000000000, Bcst::k32, x0));
        pc->s_min_f32(x0, x0, f->vmaxf);
      }

      pc->s_add_f64(f->dd_bd, f->dd_bd, f->ddd);
      pc->s_cvtt_f32_int(gIdx, x0);
      pc->s_sqrt_f32(f->value, f->value, f->value);

      if (extendMode() == ExtendMode::kRepeat) {
        cc->and_(gIdx, f->maxi);
      }

      if (extendMode() == ExtendMode::kReflect) {
        x86::Gp t = cc->newGpd("f.t");

        cc->mov(t, f->maxi);
        cc->and_(gIdx, t);
        cc->sub(t, gIdx);

        // Select the lesser, which would be at [0...tableSize).
        cc->cmp(gIdx, t);
        cc->cmovge(gIdx, t);
      }

      fetchGradientPixel1(p, flags, x86::ptr(f->table, gIdx, 2));
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

      FetchContext fCtx(pc, &p, n, format(), flags);
      IndexExtractor iExt(pc);

      uint32_t srcShift = 2;
      const uint8_t srcIndexes[4] = { 0, 2, 4, 6 };

      pc->v_mul_f32(value, value, f->scale);
      pc->v_cvt_f64_f32(x0, d_b);

      pc->vmovaps(f->d_b_prev, d_b);     // Save `d_b_prev`.
      pc->vmovaps(f->dd_bd_prev, dd_bd); // Save `dd_bd_prev`.

      if (extendMode() == ExtendMode::kPad)
        pc->v_max_f32(value, value, pc->simdConst(&c.i_0000000000000000, Bcst::k32, value));

      pc->v_add_f64(d_b, d_b, dd_bd);
      pc->s_add_f64(dd_bd, dd_bd, ddd);

      if (extendMode() == ExtendMode::kPad)
        pc->v_min_f32(value, value, f->vmaxf);

      pc->v_cvt_f64_f32(x1, d_b);
      pc->v_add_f64(d_b, d_b, dd_bd);

      pc->v_cvt_f32_i32(x3, value);
      pc->s_add_f64(dd_bd, dd_bd, ddd);

      if (extendMode() == ExtendMode::kRepeat) {
        pc->v_and_i32(x3, x3, f->vmaxi);
      }

      if (extendMode() == ExtendMode::kReflect) {
        x86::Xmm t = cc->newXmm("t");
        pc->vmovaps(t, f->vmaxi);

        pc->v_and_i32(x3, x3, t);
        pc->v_sub_i32(t, t, x3);
        pc->v_min_i16(x3, x3, t);
      }

      pc->v_shuffle_f32(x0, x0, x1, x86::shuffleImm(1, 0, 1, 0));
      iExt.begin(IndexExtractor::kTypeUInt16, x3);

      pc->v_cvt_f64_f32(x1, d_b);
      pc->v_add_f64(d_b, d_b, dd_bd);

      fCtx.fetchAll(x86::ptr(f->table), srcShift, iExt, srcIndexes, [&](uint32_t step) {
        switch (step) {
          case 0:
            pc->vmovaps(value, x0);
            pc->v_cvt_f64_f32(x2, d_b);
            break;
          case 1:
            pc->s_add_f64(dd_bd, dd_bd, ddd);
            pc->v_shuffle_f32(x1, x1, x2, x86::shuffleImm(1, 0, 1, 0));
            break;
          case 2:
            pc->v_shuffle_f32(x0, x0, x1, x86::shuffleImm(2, 0, 2, 0));
            pc->v_and_f32(x0, x0, pc->simdConst(&c.f32_abs, Bcst::k32, x0));
            break;
          case 3:
            pc->v_sqrt_f32(x0, x0);
            pc->v_add_f64(d_b, d_b, dd_bd);
            break;
        }
      });

      pc->v_shuffle_f32(value, value, x1, x86::shuffleImm(3, 1, 3, 1));
      pc->s_add_f64(dd_bd, dd_bd, ddd);
      fCtx.end();

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

// BLPipeline::JIT::FetchConicalGradientPart - Construction & Destruction
// ======================================================================

FetchConicalGradientPart::FetchConicalGradientPart(PipeCompiler* pc, FetchType fetchType, BLInternalFormat format) noexcept
  : FetchGradientPart(pc, fetchType, format) {

  _partFlags |= PipePartFlags::kAdvanceXNeedsX;
  _isComplexFetch = true;
  JitUtils::resetVarStruct(&f, sizeof(f));
}

// BLPipeline::JIT::FetchConicalGradientPart - Prepare
// ===================================================

void FetchConicalGradientPart::preparePart() noexcept {
  _maxPixels = 4;
}

// BLPipeline::JIT::FetchConicalGradientPart - Init & Fini
// =======================================================

void FetchConicalGradientPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  const BLCommonTable& c = blCommonTable;

  // Local Registers
  // ---------------

  f->table = cc->newIntPtr("f.table");                // Reg.
  f->xx_xy = cc->newXmmPd("f.xx_xy");                 // Mem.
  f->yx_yy = cc->newXmmPd("f.yx_yy");                 // Mem.
  f->hx_hy = cc->newXmmPd("f.hx_hy");                 // Reg. (TODO: Make spillable).
  f->px_py = cc->newXmmPd("f.px_py");                 // Reg.
  f->consts = cc->newIntPtr("f.consts");              // Reg.

  f->maxi = cc->newUInt32("f.maxi");                  // Mem.
  f->vmaxi = cc->newXmm("f.vmaxi");                   // Mem.

  f->x0 = cc->newXmmPs("f.x0");                       // Reg/Tmp.
  f->x1 = cc->newXmmPs("f.x1");                       // Reg/Tmp.
  f->x2 = cc->newXmmPs("f.x2");                       // Reg/Tmp.
  f->x3 = cc->newXmmPs("f.x3");                       // Reg/Tmp.
  f->x4 = cc->newXmmPs("f.x4");                       // Reg/Tmp.
  f->x5 = cc->newXmmPs("f.x5");                       // Reg.

  x86::Xmm off = cc->newXmmPd("f.off");               // Initialization only.

  // Part Initialization
  // -------------------

  cc->mov(f->table, x86::ptr(pc->_fetchData, REL_GRADIENT(lut.data)));

  pc->v_zero_d(f->hx_hy);
  pc->s_cvt_int_f64(f->hx_hy, f->hx_hy, y);

  pc->v_loadu_d128(f->xx_xy, x86::ptr(pc->_fetchData, REL_GRADIENT(conical.xx)));
  pc->v_loadu_d128(f->yx_yy, x86::ptr(pc->_fetchData, REL_GRADIENT(conical.yx)));
  pc->v_loadu_d128(off    , x86::ptr(pc->_fetchData, REL_GRADIENT(conical.ox)));

  pc->v_dupl_f64(f->hx_hy, f->hx_hy);
  pc->v_mul_f64(f->hx_hy, f->hx_hy, f->yx_yy);
  pc->v_add_f64(f->hx_hy, f->hx_hy, off);

  cc->mov(f->consts, x86::ptr(pc->_fetchData, REL_GRADIENT(conical.consts)));

  if (isRectFill()) {
    pc->v_zero_d(off);
    pc->s_cvt_int_f64(off, off, x);
    pc->v_dupl_f64(off, off);
    pc->v_mul_f64(off, off, f->xx_xy);
    pc->v_add_f64(f->hx_hy, f->hx_hy, off);
  }

  // Setup constants used by 4+ pixel fetches.
  if (maxPixels() > 1) {
    f->xx4_xy4 = cc->newXmmPd("f.xx4_xy4"); // Mem.
    f->xx_0123 = cc->newXmmPs("f.xx_0123"); // Mem.
    f->xy_0123 = cc->newXmmPs("f.xy_0123"); // Mem.

    pc->v_cvt_f64_f32(f->xy_0123, f->xx_xy);
    pc->v_mul_f64(f->xx4_xy4, f->xx_xy, pc->simdConst(&c.f64_4, Bcst::k32, f->xx4_xy4));

    pc->v_swizzle_i32(f->xx_0123, f->xy_0123, x86::shuffleImm(0, 0, 0, 0));
    pc->v_swizzle_i32(f->xy_0123, f->xy_0123, x86::shuffleImm(1, 1, 1, 1));

    pc->v_mul_f32(f->xx_0123, f->xx_0123, pc->simdConst(&c.f32_0_1_2_3, Bcst::kNA, f->xx_0123));
    pc->v_mul_f32(f->xy_0123, f->xy_0123, pc->simdConst(&c.f32_0_1_2_3, Bcst::kNA, f->xx_0123));
  }

  pc->v_load_i32(f->vmaxi, x86::ptr(pc->_fetchData, REL_GRADIENT(conical.maxi)));
  pc->v_expand_lo_i32(f->vmaxi, f->vmaxi);
  pc->s_mov_i32(f->maxi, f->vmaxi);
}

void FetchConicalGradientPart::_finiPart() noexcept {}

// BLPipeline::JIT::FetchConicalGradientPart - Advance
// ===================================================

void FetchConicalGradientPart::advanceY() noexcept {
  pc->v_add_f64(f->hx_hy, f->hx_hy, f->yx_yy);
}

void FetchConicalGradientPart::startAtX(const x86::Gp& x) noexcept {
  if (isRectFill()) {
    pc->vmovapd(f->px_py, f->hx_hy);
  }
  else {
    pc->v_zero_d(f->px_py);
    pc->s_cvt_int_f64(f->px_py, f->px_py, x);
    pc->v_dupl_f64(f->px_py, f->px_py);
    pc->v_mul_f64(f->px_py, f->px_py, f->xx_xy);
    pc->v_add_f64(f->px_py, f->px_py, f->hx_hy);
  }
}

void FetchConicalGradientPart::advanceX(const x86::Gp& x, const x86::Gp& diff) noexcept {
  blUnused(diff);

  x86::Xmm& hx_hy = f->hx_hy;
  x86::Xmm& px_py = f->px_py;

  if (isRectFill()) {
    pc->vmovapd(px_py, hx_hy);
  }
  else {
    pc->v_zero_d(px_py);
    pc->s_cvt_int_f64(px_py, px_py, x);
    pc->v_dupl_f64(px_py, px_py);
    pc->v_mul_f64(px_py, px_py, f->xx_xy);
    pc->v_add_f64(px_py, px_py, hx_hy);
  }
}

// BLPipeline::JIT::FetchConicalGradientPart - Fetch
// =================================================

void FetchConicalGradientPart::prefetchN() noexcept {
  const BLCommonTable& c = blCommonTable;

  x86::Gp& consts = f->consts;
  x86::Xmm& px_py = f->px_py;
  x86::Xmm& x0 = f->x0;
  x86::Xmm& x1 = f->x1;
  x86::Xmm& x2 = f->x2;
  x86::Xmm& x3 = f->x3;
  x86::Xmm& x4 = f->x4;
  x86::Xmm& x5 = f->x5;

  pc->v_cvt_f64_f32(x1, px_py);
  pc->v_broadcast_f32x4(x2, pc->simdMemConst(&c.f32_abs, Bcst::kNA, SimdWidth::k128));

  pc->v_swizzle_f32(x0, x1, x86::shuffleImm(0, 0, 0, 0));
  pc->v_swizzle_f32(x1, x1, x86::shuffleImm(1, 1, 1, 1));

  pc->v_add_f32(x0, x0, f->xx_0123);
  pc->v_add_f32(x1, x1, f->xy_0123);

  pc->v_broadcast_f32x4(x4, pc->simdMemConst(&c.f32_1e_m20, Bcst::kNA, SimdWidth::k128));
  pc->v_and_f32(x3, x2, x1);
  pc->v_and_f32(x2, x2, x0);

  pc->v_max_f32(x4, x4, x2);
  pc->v_max_f32(x4, x4, x3);
  pc->v_min_f32(x3, x3, x2);

  pc->v_cmp_f32(x2, x2, x3, x86::VCmpImm::kEQ_OQ);
  pc->v_div_f32(x3, x3, x4);

  pc->v_sra_i32(x0, x0, 31);
  pc->v_and_f32(x2, x2, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_4)));

  pc->v_sra_i32(x1, x1, 31);
  pc->v_and_f32(x0, x0, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_2)));

  pc->v_mul_f32(x5, x3, x3);
  pc->v_and_f32(x1, x1, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_1)));

  pc->v_mul_f32(x4, x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q3)));
  pc->v_add_f32(x4, x4, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q2)));

  pc->v_mul_f32(x4, x4, x5);
  pc->v_add_f32(x4, x4, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q1)));

  pc->v_mul_f32(x5, x5, x4);
  pc->v_add_f32(x5, x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q0)));

  pc->v_mul_f32(x5, x5, x3);
  pc->v_sub_f32(x5, x5, x2);

  pc->v_and_f32(x5, x5, pc->simdConst(&c.f32_abs, Bcst::k32, x5));

  pc->v_sub_f32(x5, x5, x0);
  pc->v_and_f32(x5, x5, pc->simdConst(&c.f32_abs, Bcst::k32, x5));

  pc->v_sub_f32(x5, x5, x1);
  pc->v_and_f32(x5, x5, pc->simdConst(&c.f32_abs, Bcst::k32, x5));
}

void FetchConicalGradientPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  BL_ASSERT(predicate.empty());
  blUnused(predicate);

  const BLCommonTable& c = blCommonTable;
  p.setCount(n);

  switch (n.value()) {
    case 1: {
      x86::Gp& consts = f->consts;
      x86::Xmm& px_py = f->px_py;
      x86::Xmm& x0 = f->x0;
      x86::Xmm& x1 = f->x1;
      x86::Xmm& x2 = f->x2;
      x86::Xmm& x3 = f->x3;
      x86::Xmm& x4 = f->x4;

      x86::Gp gIdx = cc->newInt32("f.gIdx");

      pc->v_cvt_f64_f32(x0, px_py);
      pc->v_broadcast_f32x4(x1, pc->simdMemConst(&c.f32_abs, Bcst::kNA, SimdWidth::k128));
      pc->v_broadcast_f32x4(x2, pc->simdMemConst(&c.f32_1e_m20, Bcst::kNA, SimdWidth::k128));

      pc->v_and_f32(x1, x1, x0);
      pc->v_add_f64(px_py, px_py, f->xx_xy);

      pc->v_swizzle_i32(x3, x1, x86::shuffleImm(2, 3, 0, 1));
      pc->s_max_f32(x2, x2, x1);

      pc->s_max_f32(x2, x2, x3);
      pc->s_min_f32(x3, x3, x1);

      pc->s_cmp_f32(x1, x1, x3, x86::VCmpImm::kEQ_OQ);
      pc->s_div_f32(x3, x3, x2);

      pc->v_sra_i32(x0, x0, 31);
      pc->v_and_f32(x1, x1, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_4)));

      pc->s_mul_f32(x2, x3, x3);
      pc->v_and_f32(x0, x0, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_extra)));

      pc->s_mul_f32(x4, x2, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q3)));
      pc->s_add_f32(x4, x4, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q2)));

      pc->s_mul_f32(x4, x4, x2);
      pc->s_add_f32(x4, x4, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q1)));

      pc->s_mul_f32(x2, x2, x4);
      pc->s_add_f32(x2, x2, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q0)));

      pc->s_mul_f32(x2, x2, x3);
      pc->s_sub_f32(x2, x2, x1);

      pc->v_swizzle_f32(x1, x0, x86::shuffleImm(2, 3, 0, 1));
      pc->v_and_f32(x2, x2, pc->simdConst(&c.f32_abs, Bcst::k32, x2));

      pc->s_sub_f32(x2, x2, x0);
      pc->v_and_f32(x2, x2, pc->simdConst(&c.f32_abs, Bcst::k32, x2));

      pc->s_sub_f32(x2, x2, x1);
      pc->v_and_f32(x2, x2, pc->simdConst(&c.f32_abs, Bcst::k32, x2));
      pc->s_cvtt_f32_int(gIdx, x2);
      cc->and_(gIdx.r32(), f->maxi.r32());

      fetchGradientPixel1(p, flags, x86::ptr(f->table, gIdx, 2));
      pc->x_satisfy_pixel(p, flags);
      break;
    }

    case 4: {
      x86::Gp& consts = f->consts;
      x86::Xmm& px_py = f->px_py;
      x86::Xmm& x0 = f->x0;
      x86::Xmm& x1 = f->x1;
      x86::Xmm& x2 = f->x2;
      x86::Xmm& x3 = f->x3;
      x86::Xmm& x4 = f->x4;
      x86::Xmm& x5 = f->x5;

      x86::Gp idx0 = cc->newInt32("f.idx0");
      x86::Gp idx1 = cc->newInt32("f.idx1");

      FetchContext fCtx(pc, &p, PixelCount(4), format(), flags);
      IndexExtractor iExt(pc);

      pc->v_add_f64(px_py, px_py, f->xx4_xy4);
      pc->v_and_f32(x5, x5, pc->simdConst(&c.f32_abs, Bcst::k32, x5));

      pc->v_cvt_f64_f32(x1, px_py);
      pc->v_broadcast_f32x4(x2, pc->simdMemConst(&c.f32_abs, Bcst::kNA, SimdWidth::k128));

      pc->v_swizzle_f32(x0, x1, x86::shuffleImm(0, 0, 0, 0));
      pc->v_swizzle_f32(x1, x1, x86::shuffleImm(1, 1, 1, 1));

      pc->v_add_f32(x0, x0, f->xx_0123);
      pc->v_add_f32(x1, x1, f->xy_0123);

      pc->v_broadcast_f32x4(x4, pc->simdMemConst(&c.f32_1e_m20, Bcst::kNA, SimdWidth::k128));
      pc->v_and_f32(x3, x2, x1);
      pc->v_and_f32(x2, x2, x0);

      pc->v_max_f32(x4, x4, x2);
      pc->v_cvtt_f32_i32(x5, x5);

      pc->v_max_f32(x4, x4, x3);
      pc->v_min_f32(x3, x3, x2);

      pc->v_cmp_f32(x2, x2, x3, x86::VCmpImm::kEQ_OQ);
      pc->v_and_i32(x5, x5, f->vmaxi);
      pc->v_div_f32(x3, x3, x4);

      iExt.begin(IndexExtractor::kTypeUInt16, x5);
      pc->v_sra_i32(x0, x0, 31);
      pc->v_and_f32(x2, x2, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_4)));
      iExt.extract(idx0, 0);

      pc->v_sra_i32(x1, x1, 31);
      pc->v_and_f32(x0, x0, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_2)));
      iExt.extract(idx1, 2);

      fCtx.fetchPixel(x86::ptr(f->table, idx0, 2));
      iExt.extract(idx0, 4);
      pc->v_mul_f32(x4, x3, x3);

      fCtx.fetchPixel(x86::ptr(f->table, idx1, 2));
      iExt.extract(idx1, 6);

      pc->vmovaps(x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q3)));
      pc->v_mul_f32(x5, x5, x4);
      pc->v_and_f32(x1, x1, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_1)));
      pc->v_add_f32(x5, x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q2)));
      pc->v_mul_f32(x5, x5, x4);
      fCtx.fetchPixel(x86::ptr(f->table, idx0, 2));

      pc->v_add_f32(x5, x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q1)));
      pc->v_mul_f32(x5, x5, x4);
      pc->v_add_f32(x5, x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q0)));
      pc->v_mul_f32(x5, x5, x3);
      fCtx.fetchPixel(x86::ptr(f->table, idx1, 2));

      pc->v_sub_f32(x5, x5, x2);
      pc->v_and_f32(x5, x5, pc->simdConst(&c.f32_abs, Bcst::k32, x5));
      pc->v_sub_f32(x5, x5, x0);

      fCtx.end();
      pc->v_and_f32(x5, x5, pc->simdConst(&c.f32_abs, Bcst::k32, x5));

      pc->x_satisfy_pixel(p, flags);
      pc->v_sub_f32(x5, x5, x1);
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

} // {JIT}
} // {BLPipeline}

#endif
