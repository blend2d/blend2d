// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../pipegen/blcompoppart_p.h"
#include "../pipegen/blfetchgradientpart_p.h"
#include "../pipegen/blfetchutils_p.h"
#include "../pipegen/blpipecompiler_p.h"

namespace BLPipeGen {

#define REL_GRADIENT(FIELD) BL_OFFSET_OF(BLPipeFetchData::Gradient, FIELD)

// ============================================================================
// [BLPipeGen::FetchGradientPart - Construction / Destruction]
// ============================================================================

FetchGradientPart::FetchGradientPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : FetchPart(pc, fetchType, fetchPayload, format),
    _extend(0) {}

// ============================================================================
// [BLPipeGen::FetchLinearGradientPart - Construction / Destruction]
// ============================================================================

FetchLinearGradientPart::FetchLinearGradientPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : FetchGradientPart(pc, fetchType, fetchPayload, format),
    _isRoR(fetchType == BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_ROR) {

  _maxOptLevelSupported = kOptLevel_X86_AVX;
  _maxPixels = 8;

  _persistentRegs[x86::Reg::kGroupGp] = 1;
  _persistentRegs[x86::Reg::kGroupVec] = 2;
  _extend = uint8_t(fetchType - BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_PAD);

  JitUtils::resetVarStruct(&f, sizeof(f));
}

// ============================================================================
// [BLPipeGen::FetchLinearGradientPart - Init / Fini]
// ============================================================================

void FetchLinearGradientPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  f->table          = cc->newIntPtr("f.table");       // Reg.
  f->pt             = cc->newXmm("f.pt");             // Reg.
  f->dt             = cc->newXmm("f.dt");             // Reg/Mem.
  f->dt2            = cc->newXmm("f.dt2");            // Reg/Mem.
  f->py             = cc->newXmm("f.py");             // Reg/Mem.
  f->dy             = cc->newXmm("f.dy");             // Reg/Mem.
  f->rep            = cc->newXmm("f.rep");            // Reg/Mem [RoR only].
  f->msk            = cc->newXmm("f.msk");            // Reg/Mem.
  f->vIdx           = cc->newXmm("f.vIdx");           // Reg/Tmp.
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  cc->mov(f->table, x86::ptr(pc->_fetchData, REL_GRADIENT(lut.data)));

  pc->vmovsi32(f->py, y);
  pc->vloadi64(f->dy, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.dy.u64)));

  pc->vdupli64(f->py, f->py);
  pc->vdupli64(f->dy, f->dy);

  pc->vMulU64xU32Lo(f->py, f->dy, f->py);
  cc->spill(f->dy);

  pc->vloadi128u(f->pt, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.pt)));
  pc->vaddi64(f->py, f->py, f->pt);

  pc->vloadi64(f->dt, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.dt.u64)));
  pc->vdupli64(f->dt, f->dt);
  cc->spill(f->dt);

  pc->vloadi64(f->dt2, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.dt2.u64)));
  pc->vdupli64(f->dt2, f->dt2);
  cc->spill(f->dt2);

  if (isRoR()) {
    pc->vloadi64(f->rep, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.rep.u64)));
    pc->vswizi32(f->rep, f->rep, x86::Predicate::shuf(1, 0, 1, 0));
    cc->spill(f->rep);
  }

  pc->vloadi32(f->msk, x86::ptr(pc->_fetchData, REL_GRADIENT(linear.msk.u)));
  pc->vswizi32(f->msk, f->msk, x86::Predicate::shuf(0, 0, 0, 0));

  // If we cannot use `packusdw`, which was introduced by SSE4.1 we subtract
  // 32768 from the pointer and use `packssdw` instead. However, if we do this,
  // we have to adjust everything else accordingly.
  if (isPad() && !pc->hasSSE4_1()) {
    pc->vsubi32(f->py, f->py, pc->constAsMem(blCommonTable.i128_0000080000000800));
    pc->vsubi16(f->msk, f->msk, pc->constAsMem(blCommonTable.i128_8000800080008000));
  }

  cc->spill(f->msk);

  if (isRectFill()) {
    pc->vmovsi32(f->pt, x);
    pc->vdupli64(f->pt, f->pt);
    pc->vMulU64xU32Lo(f->pt, f->dt, f->pt);
    pc->vaddi64(f->py, f->py, f->pt);
  }

  if (pixelGranularity() > 1)
    enterN();
}

void FetchLinearGradientPart::_finiPart() noexcept {}

// ============================================================================
// [BLPipeGen::FetchLinearGradientPart - Advance]
// ============================================================================

void FetchLinearGradientPart::advanceY() noexcept {
  pc->vaddi64(f->py, f->py, f->dy);
}

void FetchLinearGradientPart::startAtX(x86::Gp& x) noexcept {
  pc->vmov(f->pt, f->py);

  if (!isRectFill())
    advanceX(x, x);
}

void FetchLinearGradientPart::advanceX(x86::Gp& x, x86::Gp& diff) noexcept {
  BL_UNUSED(x);

  x86::Xmm delta = cc->newXmm("f.delta");
  pc->vmovsi32(delta, diff);
  pc->vdupli64(delta, delta);
  pc->vMulU64xU32Lo(delta, f->dt, delta);
  pc->vaddi64(f->pt, f->pt, delta);
}

// ============================================================================
// [BLPipeGen::FetchLinearGradientPart - Fetch]
// ============================================================================

void FetchLinearGradientPart::prefetch1() noexcept {
  if (isPad()) {
    // Nothing...
  }
  else {
    pc->vand(f->pt, f->pt, f->rep);
  }
}

void FetchLinearGradientPart::fetch1(PixelARGB& p, uint32_t flags) noexcept {
  x86::Gp tIdx = cc->newInt32("tIdx");

  x86::Xmm vIdx = f->vIdx;
  x86::Xmm vTmp = cc->newXmm("vTmp");

  if (isPad()) {
    if (pc->hasSSE4_1()) {
      pc->vpacki32u16_(vTmp, f->pt, f->pt);
      pc->vminu16(vTmp, vTmp, f->msk);
      pc->vaddi64(f->pt, f->pt, f->dt);

      pc->vextractu16(tIdx, vTmp, 1);
      pc->xFetchARGB32_1x(p, flags, x86::ptr(f->table, tIdx, 2), 4);
      pc->xSatisfyARGB32_1x(p, flags);
    }
    else {
      pc->vpacki32i16(vTmp, f->pt, f->pt);
      pc->vmini16(vTmp, vTmp, f->msk);
      pc->vaddi16(vTmp, vTmp, pc->constAsMem(blCommonTable.i128_8000800080008000));
      pc->vaddi64(f->pt, f->pt, f->dt);

      pc->vextractu16(tIdx, vTmp, 1);
      pc->xFetchARGB32_1x(p, flags, x86::ptr(f->table, tIdx, 2), 4);
      pc->xSatisfyARGB32_1x(p, flags);
    }
  }
  else {
    pc->vxor(vTmp, f->pt, f->msk);
    pc->vmini16(vTmp, vTmp, f->pt);
    pc->vaddi64(f->pt, f->pt, f->dt);

    pc->vextractu16(tIdx, vTmp, 2);
    pc->xFetchARGB32_1x(p, flags, x86::ptr(f->table, tIdx, 2), 4);

    pc->vand(f->pt, f->pt, f->rep);
    pc->xSatisfyARGB32_1x(p, flags);
  }
}

void FetchLinearGradientPart::enterN() noexcept {}
void FetchLinearGradientPart::leaveN() noexcept {}

void FetchLinearGradientPart::prefetchN() noexcept {
  x86::Xmm vIdx = f->vIdx;

  if (isPad()) {
    pc->vmov(vIdx, f->pt);
    pc->vaddi64(f->pt, f->pt, f->dt2);
    pc->vshufi32(vIdx, vIdx, f->pt, x86::Predicate::shuf(3, 1, 3, 1));
    pc->vaddi64(f->pt, f->pt, f->dt2);
  }
  else {
    pc->vand(vIdx, f->pt, f->rep);
    pc->vaddi64(f->pt, f->pt, f->dt2);
    pc->vand(f->pt, f->pt, f->rep);
    pc->vshufi32(vIdx, vIdx, f->pt, x86::Predicate::shuf(3, 1, 3, 1));
    pc->vaddi64(f->pt, f->pt, f->dt2);
  }
}

void FetchLinearGradientPart::postfetchN() noexcept {
  pc->vsubi64(f->pt, f->pt, f->dt2);
  pc->vsubi64(f->pt, f->pt, f->dt2);
}

void FetchLinearGradientPart::fetch4(PixelARGB& p, uint32_t flags) noexcept {
  IndexExtractorU16 iExt(pc, IndexExtractorU16::kStrategyStack);
  FetchContext4X fCtx(pc, &p, flags);

  x86::Gp tIdx0 = cc->newIntPtr("tIdx0");
  x86::Gp tIdx1 = cc->newIntPtr("tIdx1");

  x86::Xmm vIdx = f->vIdx;
  x86::Xmm vTmp = cc->newXmm("vTmp");

  if (isPad()) {
    if (pc->hasSSE4_1()) {
      pc->vpacki32u16_(vIdx, vIdx, vIdx);
      pc->vminu16(vIdx, vIdx, f->msk);
      iExt.begin(vIdx);

      pc->vmov(vIdx, f->pt);
      pc->vaddi64(f->pt, f->pt, f->dt2);

      iExt.extract(tIdx0, 0);
      iExt.extract(tIdx1, 1);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
      iExt.extract(tIdx0, 2);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
      iExt.extract(tIdx1, 3);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
      pc->vshufi32(vIdx, vIdx, f->pt, x86::Predicate::shuf(3, 1, 3, 1));

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
      pc->vaddi64(f->pt, f->pt, f->dt2);

      fCtx.end();
      pc->xSatisfyARGB32_Nx(p, flags);
    }
    else {
      pc->vpacki32i16(vIdx, vIdx, vIdx);
      pc->vmini16(vIdx, vIdx, f->msk);
      pc->vaddi16(vIdx, vIdx, pc->constAsMem(blCommonTable.i128_8000800080008000));
      iExt.begin(vIdx);

      pc->vmov(vIdx, f->pt);
      pc->vaddi64(f->pt, f->pt, f->dt2);

      iExt.extract(tIdx0, 0);
      iExt.extract(tIdx1, 1);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
      iExt.extract(tIdx0, 2);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
      iExt.extract(tIdx1, 3);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
      pc->vshufi32(vIdx, vIdx, f->pt, x86::Predicate::shuf(3, 1, 3, 1));

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
      pc->vaddi64(f->pt, f->pt, f->dt2);

      fCtx.end();
      pc->xSatisfyARGB32_Nx(p, flags);
    }
  }
  else {
    pc->vxor(vTmp, vIdx, f->msk);
    pc->vmini16(vTmp, vTmp, vIdx);
    pc->vand(vIdx, f->pt, f->rep);

    iExt.begin(vTmp);
    pc->vaddi64(f->pt, f->pt, f->dt2);

    iExt.extract(tIdx0, 0);
    iExt.extract(tIdx1, 2);
    pc->vand(f->pt, f->pt, f->rep);

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
    iExt.extract(tIdx0, 4);

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
    iExt.extract(tIdx1, 6);

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
    pc->vshufi32(vIdx, vIdx, f->pt, x86::Predicate::shuf(3, 1, 3, 1));

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
    pc->vaddi64(f->pt, f->pt, f->dt2);

    fCtx.end();
    pc->xSatisfyARGB32_Nx(p, flags);
  }
}

void FetchLinearGradientPart::fetch8(PixelARGB& p, uint32_t flags) noexcept {
  IndexExtractorU16 iExt(pc, IndexExtractorU16::kStrategyStack);
  FetchContext8X fCtx(pc, &p, flags);

  x86::Gp tIdx0 = cc->newIntPtr("tIdx0");
  x86::Gp tIdx1 = cc->newIntPtr("tIdx1");

  x86::Xmm vIdx = f->vIdx;
  x86::Xmm vTmp = cc->newXmm("vTmp0");

  if (isPad()) {
    if (pc->hasSSE4_1() && (flags & PixelARGB::kPC) != 0) {
      pc->vmov(vTmp, f->pt);
      pc->vaddi64(f->pt, f->pt, f->dt2);
      pc->vshufi32(vTmp, vTmp, f->pt, x86::Predicate::shuf(3, 1, 3, 1));
      pc->vaddi64(f->pt, f->pt, f->dt2);

      pc->vpacki32u16_(vTmp, vTmp, vIdx);
      pc->vmov(vIdx, f->pt);
      pc->vminu16(vTmp, vTmp, f->msk);

      iExt.begin(vTmp);
      pc->vaddi64(f->pt, f->pt, f->dt2);

      iExt.extract(tIdx0, 4);
      iExt.extract(tIdx1, 0);

      pc->vloadi32(p.pc[0], x86::ptr(f->table, tIdx0, 2));
      iExt.extract(tIdx0, 5);

      pc->vloadi32(p.pc[1], x86::ptr(f->table, tIdx1, 2));
      iExt.extract(tIdx1, 1);

      pc->vinsertu32_(p.pc[0], p.pc[0], x86::ptr(f->table, tIdx0, 2), 1);
      iExt.extract(tIdx0, 6);
      pc->vinsertu32_(p.pc[1], p.pc[1], x86::ptr(f->table, tIdx1, 2), 1);
      iExt.extract(tIdx1, 2);

      pc->vinsertu32_(p.pc[0], p.pc[0], x86::ptr(f->table, tIdx0, 2), 2);
      iExt.extract(tIdx0, 7);
      pc->vinsertu32_(p.pc[1], p.pc[1], x86::ptr(f->table, tIdx1, 2), 2);
      iExt.extract(tIdx1, 3);

      pc->vinsertu32_(p.pc[0], p.pc[0], x86::ptr(f->table, tIdx0, 2), 3);
      pc->vinsertu32_(p.pc[1], p.pc[1], x86::ptr(f->table, tIdx1, 2), 3);

      pc->vshufi32(vIdx, vIdx, f->pt, x86::Predicate::shuf(3, 1, 3, 1));
      pc->vaddi64(f->pt, f->pt, f->dt2);

      pc->xSatisfyARGB32_Nx(p, flags);
    }
    else {
      pc->vmov(vTmp, f->pt);
      pc->vaddi64(f->pt, f->pt, f->dt2);
      pc->vshufi32(vTmp, vTmp, f->pt, x86::Predicate::shuf(3, 1, 3, 1));
      pc->vaddi64(f->pt, f->pt, f->dt2);

      if (pc->hasSSE4_1()) {
        pc->vpacki32u16_(vTmp, vTmp, vIdx);
        pc->vmov(vIdx, f->pt);
        pc->vminu16(vTmp, vTmp, f->msk);
      }
      else {
        pc->vpacki32i16(vTmp, vTmp, vIdx);
        pc->vmini16(vTmp, vTmp, f->msk);
        pc->vaddi16(vTmp, vTmp, pc->constAsMem(blCommonTable.i128_8000800080008000));
        pc->vmov(vIdx, f->pt);
      }

      iExt.begin(vTmp);
      pc->vaddi64(f->pt, f->pt, f->dt2);
      iExt.extract(tIdx0, 4);
      iExt.extract(tIdx1, 5);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
      iExt.extract(tIdx0, 6);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
      iExt.extract(tIdx1, 7);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
      iExt.extract(tIdx0, 0);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
      iExt.extract(tIdx1, 1);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
      iExt.extract(tIdx0, 2);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
      iExt.extract(tIdx1, 3);

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
      pc->vshufi32(vIdx, vIdx, f->pt, x86::Predicate::shuf(3, 1, 3, 1));

      fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
      pc->vaddi64(f->pt, f->pt, f->dt2);

      fCtx.end();
      pc->xSatisfyARGB32_Nx(p, flags);
    }
  }
  else {
    pc->vand(vTmp, f->pt, f->rep);
    pc->vaddi64(f->pt, f->pt, f->dt2);
    pc->vand(f->pt, f->pt, f->rep);
    pc->vshufi32(vTmp, vTmp, f->pt, x86::Predicate::shuf(3, 1, 3, 1));

    pc->vpacki32i16(vTmp, vTmp, vIdx);
    pc->vaddi64(f->pt, f->pt, f->dt2);

    pc->vxor(vIdx, vTmp, f->msk);
    pc->vmini16(vTmp, vTmp, vIdx);

    pc->vand(vIdx, f->pt, f->rep);
    pc->vaddi64(f->pt, f->pt, f->dt2);
    iExt.begin(vTmp);

    iExt.extract(tIdx0, 4);
    iExt.extract(tIdx1, 5);
    pc->vand(f->pt, f->pt, f->rep);

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
    iExt.extract(tIdx0, 6);

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
    iExt.extract(tIdx1, 7);

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
    iExt.extract(tIdx0, 0);

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
    iExt.extract(tIdx1, 1);

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
    iExt.extract(tIdx0, 2);

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
    iExt.extract(tIdx1, 3);

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx0, 2));
    pc->vshufi32(vIdx, vIdx, f->pt, x86::Predicate::shuf(3, 1, 3, 1));

    fCtx.fetchARGB32(x86::ptr(f->table, tIdx1, 2));
    pc->vaddi64(f->pt, f->pt, f->dt2);

    fCtx.end();
    pc->xSatisfyARGB32_Nx(p, flags);
  }
}

// ============================================================================
// [BLPipeGen::FetchRadialGradientPart - Construction / Destruction]
// ============================================================================

FetchRadialGradientPart::FetchRadialGradientPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : FetchGradientPart(pc, fetchType, fetchPayload, format) {

  _maxOptLevelSupported = kOptLevel_X86_AVX;
  _maxPixels = 4;
  _isComplexFetch = true;
  _persistentRegs[x86::Reg::kGroupVec] = 3;
  _temporaryRegs[x86::Reg::kGroupVec] = 1;
  _extend = uint8_t(fetchType - BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_PAD);

  JitUtils::resetVarStruct(&f, sizeof(f));
}

// ============================================================================
// [BLPipeGen::FetchRadialGradientPart - Init / Fini]
// ============================================================================

void FetchRadialGradientPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  f->table          = cc->newIntPtr("f.table");       // Reg.
  f->xx_xy          = cc->newXmmPd("f.xx_xy");        // Mem.
  f->yx_yy          = cc->newXmmPd("f.yx_yy");        // Mem.
  f->ax_ay          = cc->newXmmPd("f.ax_ay");        // Mem.
  f->fx_fy          = cc->newXmmPd("f.fx_fy");        // Mem.
  f->da_ba          = cc->newXmmPd("f.da_ba");        // Mem.

  f->d_b            = cc->newXmmPd("f.d_b");          // Reg.
  f->dd_bd          = cc->newXmmPd("f.dd_bd");        // Reg.
  f->ddx_ddy        = cc->newXmmPd("f.ddx_ddy");      // Mem.

  f->px_py          = cc->newXmmPd("f.px_py");        // Reg.
  f->scale          = cc->newXmmPs("f.scale");        // Mem.
  f->ddd            = cc->newXmmPd("f.ddd");          // Mem.
  f->value          = cc->newXmmPs("f.value");        // Reg/Tmp.

  f->maxi           = cc->newUInt32("f.maxi");        // Mem.
  f->vmaxi          = cc->newXmm("f.vmaxi");          // Mem.
  f->vmaxf          = cc->newXmmPd("f.vmaxf");        // Mem.

  f->d_b_prev       = cc->newXmmPd("f.d_b_prev");     // Mem.
  f->dd_bd_prev     = cc->newXmmPd("f.dd_bd_prev");   // Mem.

  x86::Xmm off      = cc->newXmmPd("f.off");          // Local.
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  cc->mov(f->table, x86::ptr(pc->_fetchData, REL_GRADIENT(lut.data)));

  pc->vloadpd_128u(f->ax_ay, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.ax)));
  pc->vloadpd_128u(f->fx_fy, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.fx)));

  cc->spill(f->ax_ay);
  cc->spill(f->fx_fy);

  pc->vloadpd_128u(f->da_ba  , x86::ptr(pc->_fetchData, REL_GRADIENT(radial.dd)));
  pc->vloadpd_128u(f->ddx_ddy, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.ddx)));

  cc->spill(f->da_ba);
  cc->spill(f->ddx_ddy);

  pc->vzerops(f->scale);
  pc->vcvtsdss(f->scale, f->scale, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.scale)));

  pc->vloadsd(f->ddd, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.ddd)));
  pc->vduplpd(f->ddd, f->ddd);
  pc->vexpandlps(f->scale, f->scale);

  cc->spill(f->ddd);
  cc->spill(f->scale);

  pc->vloadpd_128u(f->xx_xy, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.xx)));
  pc->vloadpd_128u(f->yx_yy, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.yx)));

  pc->vzeropd(f->px_py);
  pc->vcvtsisd(f->px_py, f->px_py, y);
  pc->vloadpd_128u(off, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.ox)));

  pc->vduplpd(f->px_py, f->px_py);
  pc->vmulpd(f->px_py, f->px_py, f->yx_yy);
  pc->vaddpd(f->px_py, f->px_py, off);

  pc->vloadi32(f->vmaxi, x86::ptr(pc->_fetchData, REL_GRADIENT(radial.maxi)));
  pc->vexpandli32(f->vmaxi, f->vmaxi);
  pc->vmovsi32(f->maxi, f->vmaxi);

  if (extend() == BL_EXTEND_MODE_PAD) {
    pc->vcvti32ps(f->vmaxf, f->vmaxi);
    cc->spill(f->vmaxf);
  }
  cc->spill(f->vmaxi);

  if (isRectFill()) {
    pc->vzeropd(off);
    pc->vcvtsisd(off, off, x);
    pc->vduplpd(off, off);
    pc->vmulpd(off, off, f->xx_xy);
    pc->vaddpd(f->px_py, f->px_py, off);
  }

  cc->spill(f->xx_xy);
  cc->spill(f->yx_yy);
}

void FetchRadialGradientPart::_finiPart() noexcept {}

// ============================================================================
// [BLPipeGen::FetchRadialGradientPart - Advance]
// ============================================================================

void FetchRadialGradientPart::advanceY() noexcept {
  pc->vaddpd(f->px_py, f->px_py, f->yx_yy);
}

void FetchRadialGradientPart::startAtX(x86::Gp& x) noexcept {
  if (isRectFill()) {
    precalc(f->px_py);
  }
  else {
    x86::Xmm px_py = cc->newXmmPd("@px_py");

    pc->vzeropd(px_py);
    pc->vcvtsisd(px_py, px_py, x);
    pc->vduplpd(px_py, px_py);
    pc->vmulpd(px_py, px_py, f->xx_xy);
    pc->vaddpd(px_py, px_py, f->px_py);

    precalc(px_py);
  }
}

void FetchRadialGradientPart::advanceX(x86::Gp& x, x86::Gp& diff) noexcept {
  BL_UNUSED(diff);

  if (isRectFill()) {
    precalc(f->px_py);
  }
  else {
    x86::Xmm px_py = cc->newXmmPd("@px_py");

    // TODO: [PIPEGEN] Duplicated code :(
    pc->vzeropd(px_py);
    pc->vcvtsisd(px_py, px_py, x);
    pc->vduplpd(px_py, px_py);
    pc->vmulpd(px_py, px_py, f->xx_xy);
    pc->vaddpd(px_py, px_py, f->px_py);

    precalc(px_py);
  }
}

// ============================================================================
// [BLPipeGen::FetchRadialGradientPart - Fetch]
// ============================================================================

void FetchRadialGradientPart::prefetch1() noexcept {
  pc->vcvtpdps(f->value, f->d_b);
  pc->vandps(f->value, f->value, pc->constAsMem(blCommonTable.f128_abs_lo));
  pc->vsqrtss(f->value, f->value, f->value);
}

void FetchRadialGradientPart::fetch1(PixelARGB& p, uint32_t flags) noexcept {
  x86::Xmm x0 = cc->newXmmPs("@x0");
  x86::Gp idx = cc->newInt32("@idx");

  pc->vswizi32(x0, f->value, x86::Predicate::shuf(1, 1, 1, 1));
  pc->vaddpd(f->d_b, f->d_b, f->dd_bd);

  pc->vaddss(x0, x0, f->value);
  pc->vcvtpdps(f->value, f->d_b);

  pc->vmulss(x0, x0, f->scale);
  pc->vandps(f->value, f->value, pc->constAsMem(blCommonTable.f128_abs_lo));

  if (extend() == BL_EXTEND_MODE_PAD) {
    pc->vmaxss(x0, x0, pc->constAsXmm(blCommonTable.i128_0000000000000000));
    pc->vminss(x0, x0, f->vmaxf);
  }

  pc->vaddsd(f->dd_bd, f->dd_bd, f->ddd);
  pc->vcvttsssi(idx, x0);
  pc->vsqrtss(f->value, f->value, f->value);

  if (extend() == BL_EXTEND_MODE_REPEAT) {
    cc->and_(idx, f->maxi);
  }

  if (extend() == BL_EXTEND_MODE_REFLECT) {
    x86::Gp t = cc->newGpd("f.t");

    cc->mov(t, f->maxi);
    cc->and_(idx, t);
    cc->sub(t, idx);

    // Select the lesser, which would be at [0...tableSize).
    cc->cmp(idx, t);
    cc->cmovge(idx, t);
  }

  pc->xFetchARGB32_1x(p, flags, x86::ptr(f->table, idx, 2), 4);
  pc->xSatisfyARGB32_1x(p, flags);
}

void FetchRadialGradientPart::prefetchN() noexcept {
  x86::Xmm& d_b   = f->d_b;
  x86::Xmm& dd_bd = f->dd_bd;
  x86::Xmm& ddd   = f->ddd;
  x86::Xmm& value = f->value;

  x86::Xmm x0 = cc->newXmmSd("@x0");
  x86::Xmm x1 = cc->newXmmSd("@x1");
  x86::Xmm x2 = cc->newXmmSd("@x2");

  pc->vmovaps(f->d_b_prev, f->d_b);     // Save `d_b`.
  pc->vmovaps(f->dd_bd_prev, f->dd_bd); // Save `dd_bd`.

  pc->vcvtpdps(x0, d_b);
  pc->vaddpd(d_b, d_b, dd_bd);
  pc->vaddsd(dd_bd, dd_bd, ddd);

  pc->vcvtpdps(x1, d_b);
  pc->vaddpd(d_b, d_b, dd_bd);
  pc->vaddsd(dd_bd, dd_bd, ddd);
  pc->vshufps(x0, x0, x1, x86::Predicate::shuf(1, 0, 1, 0));

  pc->vcvtpdps(x1, d_b);
  pc->vaddpd(d_b, d_b, dd_bd);
  pc->vaddsd(dd_bd, dd_bd, ddd);

  pc->vcvtpdps(x2, d_b);
  pc->vaddpd(d_b, d_b, dd_bd);
  pc->vaddsd(dd_bd, dd_bd, ddd);
  pc->vshufps(x1, x1, x2, x86::Predicate::shuf(1, 0, 1, 0));

  pc->vshufps(value, x0, x1, x86::Predicate::shuf(2, 0, 2, 0));
  pc->vandps(value, value, pc->constAsMem(blCommonTable.f128_abs));
  pc->vsqrtps(value, value);

  pc->vshufps(x0, x0, x1, x86::Predicate::shuf(3, 1, 3, 1));
  cc->spill(ddd);
  pc->vaddps(value, value, x0);
}

void FetchRadialGradientPart::postfetchN() noexcept {
  pc->vmovaps(f->d_b, f->d_b_prev);     // Restore `d_b`.
  pc->vmovaps(f->dd_bd, f->dd_bd_prev); // Restore `dd_bd`.
}

void FetchRadialGradientPart::fetch4(PixelARGB& p, uint32_t flags) noexcept {
  x86::Xmm& d_b   = f->d_b;
  x86::Xmm& dd_bd = f->dd_bd;
  x86::Xmm& ddd   = f->ddd;
  x86::Xmm& value = f->value;

  x86::Xmm x0 = cc->newXmmSd("@x0");
  x86::Xmm x1 = cc->newXmmSd("@x1");
  x86::Xmm x2 = cc->newXmmSd("@x2");
  x86::Xmm x3 = cc->newXmmSd("@x3");

  x86::Gp idx0 = cc->newInt32("@idx0");
  x86::Gp idx1 = cc->newInt32("@idx1");
  FetchContext4X fCtx(pc, &p, flags);

  pc->vmulps(value, value, f->scale);
  pc->vcvtpdps(x0, d_b);

  pc->vmovaps(f->d_b_prev, d_b);     // Save `d_b_prev`.
  pc->vmovaps(f->dd_bd_prev, dd_bd); // Save `dd_bd_prev`.

  if (extend() == BL_EXTEND_MODE_PAD)
    pc->vmaxps(value, value, pc->constAsXmm(blCommonTable.i128_0000000000000000));

  pc->vaddpd(d_b, d_b, dd_bd);
  pc->vaddsd(dd_bd, dd_bd, ddd);

  if (extend() == BL_EXTEND_MODE_PAD)
    pc->vminps(value, value, f->vmaxf);

  pc->vcvtpdps(x1, d_b);
  pc->vaddpd(d_b, d_b, dd_bd);

  pc->vcvtpsi32(x3, value);
  pc->vaddsd(dd_bd, dd_bd, ddd);

  if (extend() == BL_EXTEND_MODE_REPEAT) {
    pc->vand(x3, x3, f->vmaxi);
  }

  if (extend() == BL_EXTEND_MODE_REFLECT) {
    x86::Xmm t = cc->newXmm("t");
    pc->vmovaps(t, f->vmaxi);

    pc->vand(x3, x3, t);
    pc->vsubi32(t, t, x3);
    pc->vmini16(x3, x3, t);
  }

  pc->vshufps(x0, x0, x1, x86::Predicate::shuf(1, 0, 1, 0));
  pc->vcvtpdps(x1, d_b);
  pc->vaddpd(d_b, d_b, dd_bd);

  pc->vextractu16(idx0, x3, 0);
  pc->vmovaps(value, x0);
  pc->vcvtpdps(x2, d_b);
  fCtx.fetchARGB32(x86::dword_ptr(f->table, idx0, 2));

  pc->vaddsd(dd_bd, dd_bd, ddd);
  pc->vextractu16(idx1, x3, 2);
  pc->vshufps(x1, x1, x2, x86::Predicate::shuf(1, 0, 1, 0));

  pc->vextractu16(idx0, x3, 4);
  pc->vshufps(x0, x0, x1, x86::Predicate::shuf(2, 0, 2, 0));
  fCtx.fetchARGB32(x86::dword_ptr(f->table, idx1, 2));

  pc->vandps(x0, x0, pc->constAsMem(blCommonTable.f128_abs));
  pc->vextractu16(idx1, x3, 6);
  pc->vsqrtps(x0, x0);

  pc->vaddpd(d_b, d_b, dd_bd);
  fCtx.fetchARGB32(x86::dword_ptr(f->table, idx0, 2));

  pc->vshufps(value, value, x1, x86::Predicate::shuf(3, 1, 3, 1));
  pc->vaddsd(dd_bd, dd_bd, ddd);

  fCtx.fetchARGB32(x86::dword_ptr(f->table, idx1, 2));
  cc->spill(ddd);

  fCtx.end();
  pc->xSatisfyARGB32_Nx(p, flags);

  pc->vaddps(value, value, x0);
}

void FetchRadialGradientPart::precalc(x86::Xmm& px_py) noexcept {
  x86::Xmm& d_b   = f->d_b;
  x86::Xmm& dd_bd = f->dd_bd;

  x86::Xmm x0 = cc->newXmmPd("@x0");
  x86::Xmm x1 = cc->newXmmPd("@x1");
  x86::Xmm x2 = cc->newXmmPd("@x2");

  pc->vmulpd(d_b, px_py, f->ax_ay);                   // [Ax.Px                             | Ay.Py         ]
  pc->vmulpd(x0, px_py, f->fx_fy);                    // [Fx.Px                             | Fy.Py         ]
  pc->vmulpd(x1, px_py, f->ddx_ddy);                  // [Ddx.Px                            | Ddy.Py        ]

  pc->vmulpd(d_b, d_b, px_py);                        // [Ax.Px^2                           | Ay.Py^2       ]
  pc->vhaddpd(d_b, d_b, x0);                          // [Ax.Px^2 + Ay.Py^2                 | Fx.Px + Fy.Py ]

  pc->vswappd(x2, x0);
  pc->vmulsd(x2, x2, x0);                             // [Fx.Px.Fy.Py                       | ?             ]
  pc->vaddsd(x2, x2, x2);                             // [2.Fx.Px.Fy.Py                     | ?             ]
  pc->vaddsd(d_b, d_b, x2);                           // [Ax.Px^2 + Ay.Py^2 + 2.Fx.Px.Fy.Py | Fx.Px + Fy.Py ]
  pc->vaddsd(dd_bd, f->da_ba, x1);                    // [Dd + Ddx.Px                       | Bd            ]

  pc->vswappd(x1, x1);
  pc->vaddsd(dd_bd, dd_bd, x1);                       // [Dd + Ddx.Px + Ddy.Py              | Bd            ]
}

// ============================================================================
// [BLPipeGen::FetchConicalGradientPart - Construction / Destruction]
// ============================================================================

FetchConicalGradientPart::FetchConicalGradientPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : FetchGradientPart(pc, fetchType, fetchPayload, format) {

  _maxOptLevelSupported = kOptLevel_X86_AVX;
  _maxPixels = 4;
  _isComplexFetch = true;
  _persistentRegs[x86::Reg::kGroupGp] = 1;
  _persistentRegs[x86::Reg::kGroupVec] = 4;
  _temporaryRegs[x86::Reg::kGroupVec] = 6;

  JitUtils::resetVarStruct(&f, sizeof(f));
}

// ============================================================================
// [BLPipeGen::FetchConicalGradientPart - Init / Fini]
// ============================================================================

void FetchConicalGradientPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  f->table          = cc->newIntPtr("f.table");       // Reg.
  f->xx_xy          = cc->newXmmPd("f.xx_xy");        // Mem.
  f->yx_yy          = cc->newXmmPd("f.yx_yy");        // Mem.
  f->hx_hy          = cc->newXmmPd("f.hx_hy");        // Reg. (TODO: Make spillable).
  f->px_py          = cc->newXmmPd("f.px_py");        // Reg.
  f->consts         = cc->newIntPtr("f.consts");      // Reg.

  f->maxi           = cc->newUInt32("f.maxi");        // Mem.
  f->vmaxi          = cc->newXmm("f.vmaxi");          // Mem.

  f->x0             = cc->newXmmPs("f.x0");           // Reg/Tmp.
  f->x1             = cc->newXmmPs("f.x1");           // Reg/Tmp.
  f->x2             = cc->newXmmPs("f.x2");           // Reg/Tmp.
  f->x3             = cc->newXmmPs("f.x3");           // Reg/Tmp.
  f->x4             = cc->newXmmPs("f.x4");           // Reg/Tmp.
  f->x5             = cc->newXmmPs("f.x5");           // Reg.

  x86::Xmm off      = cc->newXmmPd("f.off");          // Local.
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  cc->mov(f->table, x86::ptr(pc->_fetchData, REL_GRADIENT(lut.data)));

  pc->vzeropd(f->hx_hy);
  pc->vcvtsisd(f->hx_hy, f->hx_hy, y);

  pc->vloadpd_128u(f->xx_xy, x86::ptr(pc->_fetchData, REL_GRADIENT(conical.xx)));
  pc->vloadpd_128u(f->yx_yy, x86::ptr(pc->_fetchData, REL_GRADIENT(conical.yx)));
  pc->vloadpd_128u(off    , x86::ptr(pc->_fetchData, REL_GRADIENT(conical.ox)));

  pc->vduplpd(f->hx_hy, f->hx_hy);
  pc->vmulpd(f->hx_hy, f->hx_hy, f->yx_yy);
  pc->vaddpd(f->hx_hy, f->hx_hy, off);
  cc->spill(f->yx_yy);

  cc->mov(f->consts, x86::ptr(pc->_fetchData, REL_GRADIENT(conical.consts)));

  if (isRectFill()) {
    pc->vzeropd(off);
    pc->vcvtsisd(off, off, x);
    pc->vduplpd(off, off);
    pc->vmulpd(off, off, f->xx_xy);
    pc->vaddpd(f->hx_hy, f->hx_hy, off);
  }

  // Setup constants used by 4+ pixel fetches.
  if (maxPixels() > 1) {
    f->xx4_xy4 = cc->newXmmPd("f.xx4_xy4"); // Mem.
    f->xx_0123 = cc->newXmmPs("f.xx_0123"); // Mem.
    f->xy_0123 = cc->newXmmPs("f.xy_0123"); // Mem.

    pc->vcvtpdps(f->xy_0123, f->xx_xy);
    pc->vmulpd(f->xx4_xy4, f->xx_xy, pc->constAsMem(blCommonTable.d128_4));
    cc->spill(f->xx4_xy4);

    pc->vswizi32(f->xx_0123, f->xy_0123, x86::Predicate::shuf(0, 0, 0, 0));
    pc->vswizi32(f->xy_0123, f->xy_0123, x86::Predicate::shuf(1, 1, 1, 1));

    pc->vmulps(f->xx_0123, f->xx_0123, pc->constAsMem(blCommonTable.f128_3_2_1_0));
    pc->vmulps(f->xy_0123, f->xy_0123, pc->constAsMem(blCommonTable.f128_3_2_1_0));

    cc->spill(f->xx_0123);
    cc->spill(f->xy_0123);
  }

  cc->spill(f->xx_xy);

  pc->vloadi32(f->vmaxi, x86::ptr(pc->_fetchData, REL_GRADIENT(conical.maxi)));
  pc->vexpandli32(f->vmaxi, f->vmaxi);
  pc->vmovsi32(f->maxi, f->vmaxi);
}

void FetchConicalGradientPart::_finiPart() noexcept {}

// ============================================================================
// [BLPipeGen::FetchConicalGradientPart - Advance]
// ============================================================================

void FetchConicalGradientPart::advanceY() noexcept {
  pc->vaddpd(f->hx_hy, f->hx_hy, f->yx_yy);
}

void FetchConicalGradientPart::startAtX(x86::Gp& x) noexcept {
  if (isRectFill()) {
    pc->vmovapd(f->px_py, f->hx_hy);
  }
  else {
    pc->vzeropd(f->px_py);
    pc->vcvtsisd(f->px_py, f->px_py, x);
    pc->vduplpd(f->px_py, f->px_py);
    pc->vmulpd(f->px_py, f->px_py, f->xx_xy);
    pc->vaddpd(f->px_py, f->px_py, f->hx_hy);
  }
}

void FetchConicalGradientPart::advanceX(x86::Gp& x, x86::Gp& diff) noexcept {
  BL_UNUSED(diff);

  x86::Xmm& hx_hy = f->hx_hy;
  x86::Xmm& px_py = f->px_py;

  if (isRectFill()) {
    pc->vmovapd(px_py, hx_hy);
  }
  else {
    pc->vzeropd(px_py);
    pc->vcvtsisd(px_py, px_py, x);
    pc->vduplpd(px_py, px_py);
    pc->vmulpd(px_py, px_py, f->xx_xy);
    pc->vaddpd(px_py, px_py, hx_hy);
  }
}

// ============================================================================
// [BLPipeGen::FetchConicalGradientPart - Fetch]
// ============================================================================

void FetchConicalGradientPart::fetch1(PixelARGB& p, uint32_t flags) noexcept {
  x86::Gp& consts = f->consts;
  x86::Xmm& px_py = f->px_py;
  x86::Xmm& x0 = f->x0;
  x86::Xmm& x1 = f->x1;
  x86::Xmm& x2 = f->x2;
  x86::Xmm& x3 = f->x3;
  x86::Xmm& x4 = f->x4;

  x86::Gp index = cc->newInt32("f.index");

  pc->vcvtpdps(x0, px_py);
  pc->vmovaps(x1, pc->constAsMem(blCommonTable.f128_abs));
  pc->vmovaps(x2, pc->constAsMem(blCommonTable.f128_1e_m20));

  pc->vandps(x1, x1, x0);
  pc->vaddpd(px_py, px_py, f->xx_xy);

  pc->vswizi32(x3, x1, x86::Predicate::shuf(2, 3, 0, 1));
  pc->vmaxss(x2, x2, x1);

  pc->vmaxss(x2, x2, x3);
  pc->vminss(x3, x3, x1);

  pc->vcmpss(x1, x1, x3, x86::Predicate::kCmpEQ);
  pc->vdivss(x3, x3, x2);

  pc->vsrai32(x0, x0, 31);
  pc->vandps(x1, x1, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_4)));

  pc->vmulss(x2, x3, x3);
  pc->vandps(x0, x0, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_extra)));

  pc->vmulss(x4, x2, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q3)));
  pc->vaddss(x4, x4, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q2)));

  pc->vmulss(x4, x4, x2);
  pc->vaddss(x4, x4, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q1)));

  pc->vmulss(x2, x2, x4);
  pc->vaddss(x2, x2, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q0)));

  pc->vmulss(x2, x2, x3);
  pc->vsubss(x2, x2, x1);

  pc->vswizps(x1, x0, x86::Predicate::shuf(2, 3, 0, 1));
  pc->vandps(x2, x2, pc->constAsMem(blCommonTable.f128_abs));

  pc->vsubss(x2, x2, x0);
  pc->vandps(x2, x2, pc->constAsMem(blCommonTable.f128_abs));

  pc->vsubss(x2, x2, x1);
  pc->vandps(x2, x2, pc->constAsMem(blCommonTable.f128_abs));
  pc->vcvttsssi(index, x2);
  cc->and_(index.r32(), f->maxi.r32());

  pc->xFetchARGB32_1x(p, flags, x86::ptr(f->table, index, 2), 4);
  pc->xSatisfyARGB32_1x(p, flags);
}

void FetchConicalGradientPart::prefetchN() noexcept {
  x86::Gp& consts = f->consts;
  x86::Xmm& px_py = f->px_py;
  x86::Xmm& x0 = f->x0;
  x86::Xmm& x1 = f->x1;
  x86::Xmm& x2 = f->x2;
  x86::Xmm& x3 = f->x3;
  x86::Xmm& x4 = f->x4;
  x86::Xmm& x5 = f->x5;

  pc->vcvtpdps(x1, px_py);
  pc->vmovaps(x2, pc->constAsMem(blCommonTable.f128_abs));

  pc->vswizps(x0, x1, x86::Predicate::shuf(0, 0, 0, 0));
  pc->vswizps(x1, x1, x86::Predicate::shuf(1, 1, 1, 1));

  pc->vaddps(x0, x0, f->xx_0123);
  pc->vaddps(x1, x1, f->xy_0123);

  pc->vmovaps(x4, pc->constAsMem(blCommonTable.f128_1e_m20));
  pc->vandps(x3, x2, x1);
  pc->vandps(x2, x2, x0);

  pc->vmaxps(x4, x4, x2);
  pc->vmaxps(x4, x4, x3);
  pc->vminps(x3, x3, x2);

  pc->vcmpps(x2, x2, x3, x86::Predicate::kCmpEQ);
  pc->vdivps(x3, x3, x4);

  pc->vsrai32(x0, x0, 31);
  pc->vandps(x2, x2, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_4)));

  pc->vsrai32(x1, x1, 31);
  pc->vandps(x0, x0, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_2)));

  pc->vmulps(x5, x3, x3);
  pc->vandps(x1, x1, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_1)));

  pc->vmulps(x4, x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q3)));
  pc->vaddps(x4, x4, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q2)));

  pc->vmulps(x4, x4, x5);
  pc->vaddps(x4, x4, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q1)));

  pc->vmulps(x5, x5, x4);
  pc->vaddps(x5, x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q0)));

  pc->vmulps(x5, x5, x3);
  pc->vsubps(x5, x5, x2);

  pc->vandps(x5, x5, pc->constAsMem(blCommonTable.f128_abs));

  pc->vsubps(x5, x5, x0);
  pc->vandps(x5, x5, pc->constAsMem(blCommonTable.f128_abs));

  pc->vsubps(x5, x5, x1);
  pc->vandps(x5, x5, pc->constAsMem(blCommonTable.f128_abs));
}

void FetchConicalGradientPart::fetch4(PixelARGB& p, uint32_t flags) noexcept {
  x86::Gp& consts = f->consts;
  x86::Xmm& px_py = f->px_py;
  x86::Xmm& x0 = f->x0;
  x86::Xmm& x1 = f->x1;
  x86::Xmm& x2 = f->x2;
  x86::Xmm& x3 = f->x3;
  x86::Xmm& x4 = f->x4;
  x86::Xmm& x5 = f->x5;

  x86::Gp idx0 = cc->newInt32("@idx0");
  x86::Gp idx1 = cc->newInt32("@idx1");
  FetchContext4X fCtx(pc, &p, flags);

  pc->vaddpd(px_py, px_py, f->xx4_xy4);
  pc->vandps(x5, x5, pc->constAsMem(blCommonTable.f128_abs));

  pc->vcvtpdps(x1, px_py);
  pc->vmovaps(x2, pc->constAsMem(blCommonTable.f128_abs));

  pc->vswizps(x0, x1, x86::Predicate::shuf(0, 0, 0, 0));
  pc->vswizps(x1, x1, x86::Predicate::shuf(1, 1, 1, 1));

  pc->vaddps(x0, x0, f->xx_0123);
  pc->vaddps(x1, x1, f->xy_0123);

  pc->vmovaps(x4, pc->constAsMem(blCommonTable.f128_1e_m20));
  pc->vandps(x3, x2, x1);
  pc->vandps(x2, x2, x0);

  pc->vmaxps(x4, x4, x2);
  pc->vcvttpsi32(x5, x5);

  pc->vmaxps(x4, x4, x3);
  pc->vminps(x3, x3, x2);

  pc->vcmpps(x2, x2, x3, x86::Predicate::kCmpEQ);
  pc->vand(x5, x5, f->vmaxi);
  pc->vdivps(x3, x3, x4);

  pc->vextractu16(idx0, x5, 0);
  pc->vsrai32(x0, x0, 31);
  pc->vandps(x2, x2, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_4)));

  pc->vextractu16(idx1, x5, 2);
  pc->vsrai32(x1, x1, 31);
  pc->vandps(x0, x0, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_2)));

  fCtx.fetchARGB32(x86::dword_ptr(f->table, idx0, 2));
  pc->vextractu16(idx0, x5, 4);
  pc->vmulps(x4, x3, x3);

  fCtx.fetchARGB32(x86::dword_ptr(f->table, idx1, 2));
  pc->vextractu16(idx1, x5, 6);

  pc->vmovaps(x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q3)));
  pc->vmulps(x5, x5, x4);
  pc->vandps(x1, x1, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, n_div_1)));
  pc->vaddps(x5, x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q2)));
  pc->vmulps(x5, x5, x4);
  fCtx.fetchARGB32(x86::dword_ptr(f->table, idx0, 2));

  pc->vaddps(x5, x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q1)));
  pc->vmulps(x5, x5, x4);
  pc->vaddps(x5, x5, x86::ptr(consts, BL_OFFSET_OF(BLCommonTable::Conical, q0)));
  pc->vmulps(x5, x5, x3);
  fCtx.fetchARGB32(x86::dword_ptr(f->table, idx1, 2));

  pc->vsubps(x5, x5, x2);
  pc->vandps(x5, x5, pc->constAsMem(blCommonTable.f128_abs));
  pc->vsubps(x5, x5, x0);

  fCtx.end();
  pc->vandps(x5, x5, pc->constAsMem(blCommonTable.f128_abs));

  pc->xSatisfyARGB32_Nx(p, flags);
  pc->vsubps(x5, x5, x1);
}

} // {BLPipeGen}
