// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchpatternpart_p.h"
#include "../../pipeline/jit/fetchutils_p.h"
#include "../../pipeline/jit/fetchutilspixelaccess_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../support/intops_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

#define REL_PATTERN(FIELD) BL_OFFSET_OF(FetchData::Pattern, FIELD)

// bl::Pipeline::JIT::FetchPatternPart - Construction & Destruction
// ================================================================

FetchPatternPart::FetchPatternPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchPart(pc, fetchType, format) {}

// bl::Pipeline::JIT::FetchSimplePatternPart - Construction & Destruction
// ======================================================================

FetchSimplePatternPart::FetchSimplePatternPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchPatternPart(pc, fetchType, format) {

  static const ExtendMode aExtendTable[] = { ExtendMode::kPad, ExtendMode::kRepeat, ExtendMode::kRoR };
  static const ExtendMode uExtendTable[] = { ExtendMode::kPad, ExtendMode::kRoR };

  _partFlags |= PipePartFlags::kAdvanceXNeedsDiff;
  _idxShift = 0;
  _maxPixels = 4;

  // Setup registers, extend mode, and the maximum number of pixels that can be fetched at once.
  switch (fetchType) {
    case FetchType::kPatternAlignedBlit:
      _partFlags |= PipePartFlags::kAdvanceXIsSimple;
      _maxSimdWidthSupported = SimdWidth::k512;
      _maxPixels = kUnlimitedMaxPixels;

      if (pc->hasMaskedAccessOf(bpp()))
        _partFlags |= PipePartFlags::kMaskedAccess;
      break;

    case FetchType::kPatternAlignedPad:
      _maxPixels = 8;
      BL_FALLTHROUGH

    case FetchType::kPatternAlignedRepeat:
    case FetchType::kPatternAlignedRoR:
      _extendX = aExtendTable[uint32_t(fetchType) - uint32_t(FetchType::kPatternAlignedPad)];
      break;

    case FetchType::kPatternFxPad:
    case FetchType::kPatternFxRoR:
      _extendX = uExtendTable[uint32_t(fetchType) - uint32_t(FetchType::kPatternFxPad)];
      break;

    case FetchType::kPatternFyPad:
    case FetchType::kPatternFyRoR:
      _extendX = uExtendTable[uint32_t(fetchType) - uint32_t(FetchType::kPatternFyPad)];
      break;

    case FetchType::kPatternFxFyPad:
    case FetchType::kPatternFxFyRoR:
      _extendX = uExtendTable[uint32_t(fetchType) - uint32_t(FetchType::kPatternFxFyPad)];
      _isComplexFetch = true;
      break;

    default:
      BL_NOT_REACHED();
  }

  if (extendX() == ExtendMode::kPad || extendX() == ExtendMode::kRoR) {
    if (IntOps::isPowerOf2(_bpp))
      _idxShift = uint8_t(IntOps::ctz(_bpp));
  }

  JitUtils::resetVarStruct(&f, sizeof(f));
}

// bl::Pipeline::JIT::FetchSimplePatternPart - Init & Fini
// =======================================================

void FetchSimplePatternPart::_initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept {
  if (isAlignedBlit()) {
    // This is a special-case designed only for rectangular blits that never
    // go out of image bounds (this implies that no extend mode is applied).
    BL_ASSERT(isRectFill());

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    f->stride       = pc->newGpPtr("f.stride");       // Mem.
    f->srcp1        = pc->newGpPtr("f.srcp1");        // Reg.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    pc->load(f->stride, mem_ptr(fn.fetchData(), REL_PATTERN(src.stride)));
    pc->sub(f->srcp1.r32(), y.r32(), mem_ptr(fn.fetchData(), REL_PATTERN(simple.ty)));
    pc->mul(f->srcp1, f->srcp1, f->stride);

    pc->add(f->srcp1, f->srcp1, mem_ptr(fn.fetchData(), REL_PATTERN(src.pixelData)));
    pc->i_prefetch(mem_ptr(f->srcp1));

    Gp cut = pc->newGpPtr("@stride_cut");
    pc->mul(cut.r32(), mem_ptr(fn.fetchData(), REL_PATTERN(src.size.w)), int(bpp()));
    pc->sub(f->stride, f->stride, cut);
  }
  else {
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    f->srcp0        = pc->newGpPtr("f.srcp0");        // Reg.
    f->srcp1        = pc->newGpPtr("f.srcp1");        // Reg (Fy|FxFy).
    f->w            = pc->newGp32("f.w");             // Mem.
    f->h            = pc->newGp32("f.h");             // Mem.
    f->y            = pc->newGp32("f.y");             // Reg.

    f->stride       = pc->newGpPtr("f.stride");       // Init only.
    f->ry           = pc->newGp32("f.ry");            // Init only.
    f->vExtendData  = cc->newStack(sizeof(FetchData::Pattern::VertExtendData), 16, "f.vExtendData");
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // Apply alpha offset to source pointers.
    if (_alphaFetch)
      _alphaOffsetApplied = _alphaOffset;

    pc->add(f->y, y, mem_ptr(fn.fetchData(), REL_PATTERN(simple.ty)));
    if (isPatternFx())
      pc->inc(f->y);
    pc->load_u32(f->h, mem_ptr(fn.fetchData(), REL_PATTERN(src.size.h)));
    pc->load_u32(f->ry, mem_ptr(fn.fetchData(), REL_PATTERN(simple.ry)));
    pc->load(f->stride, mem_ptr(fn.fetchData(), REL_PATTERN(src.stride)));

    // TODO:
    // cc->mov(f->yRewindOffset, mem_ptr(fn.fetchData(), REL_PATTERN(simple.yRewindOffset)));
    // cc->mov(f->pixelPtrRewindOffset, mem_ptr(fn.fetchData(), REL_PATTERN(simple.pixelPtrRewindOffset)));

    // Vertical Extend
    // ---------------
    //
    // Vertical extend modes are not hardcoded in the generated pipeline to decrease the number of possible pipeline
    // combinations. This means that the compiled pipeline supports all vertical extend modes. The amount of code
    // that handles vertical extend modes is minimal and runtime overhead during `advanceY()` was minimized. There
    // should be no performance penalty for this decision.

    {
      // Vertical Extend - Prepare
      // -------------------------

      Label L_VertRoR = pc->newLabel();
      Label L_VertSwap = pc->newLabel();
      Label L_VertDone = pc->newLabel();

      Gp yMod = pc->newGpPtr("f.yMod").r32();
      Gp hMinus1 = pc->newGpPtr("f.hMinus1").r32();
      Gp yModReg = yMod.cloneAs(f->stride);

      VecArray vStrideStopVec;
      Vec vRewindDataVec = pc->newV128("f.vRewindData");

      if (pc->is32Bit()) {
        pc->newVecArray(vStrideStopVec, 1, SimdWidth::k128, "f.vStrideStopVec");

        constexpr int kRewindDataOffset = 16;
        pc->v_loadu64(vRewindDataVec, mem_ptr(fn.fetchData(), REL_PATTERN(simple.vExtendData) + kRewindDataOffset));
        pc->v_storeu64(f->vExtendData.cloneAdjusted(kRewindDataOffset), vRewindDataVec);
      }
      else {
        constexpr int kRewindDataOffset = 32;

#if defined(BL_JIT_ARCH_X86)
        if (pc->hasAVX2())
        {
          pc->newVecArray(vStrideStopVec, 1, SimdWidth::k256, "f.vStrideStopVec");
        }
        else
#endif // BL_JIT_ARCH_X86
        {
          pc->newVecArray(vStrideStopVec, 2, SimdWidth::k128, "f.vStrideStopVec");
        }

        pc->v_loadu128(vRewindDataVec, mem_ptr(fn.fetchData(), REL_PATTERN(simple.vExtendData) + kRewindDataOffset));
        pc->v_storea128(f->vExtendData.cloneAdjusted(kRewindDataOffset), vRewindDataVec);
      }

      pc->v_loadavec(vStrideStopVec, mem_ptr(fn.fetchData(), REL_PATTERN(simple.vExtendData)), Alignment(8));

      // Don't do anything if we are within bounds as this is the case vExtendData was prepared for.
      pc->mov(yMod, f->y);
      pc->j(L_VertDone, ucmp_lt(f->y, f->h));

      // Decide between PAD and RoR.
      pc->j(L_VertRoR, test_nz(f->ry));

      // Handle PAD - we know that we are outside of bounds, so yMod would become either 0 or h-1.
      pc->sar(yMod, yMod, 31);
      pc->sub(hMinus1, f->h, 1);

      pc->bic(yMod, hMinus1, yMod);
      pc->j(L_VertSwap);

      // Handle RoR - we have to repeat to `ry`, which is double the height in reflect case.
      pc->bind(L_VertRoR);
      pc->umod(f->y, f->y, f->ry);
      pc->mov(yMod, f->y);

      // If we are within bounds already it means this is either repeat or reflection, which is in repeat part.
      pc->j(L_VertDone, ucmp_lt(f->y, f->h));

      // We are reflecting at the moment, `yMod` has to be updated.
      pc->sub(yMod, yMod, f->ry);
      pc->sub(f->y, f->y, f->h);
      pc->not_(yMod, yMod);

      // Vertical Extend - Done
      // ----------------------

      pc->bind(L_VertSwap);
      swapStrideStopData(vStrideStopVec);

      pc->bind(L_VertDone);
      pc->mul(yModReg, yModReg, f->stride);
      pc->v_storeavec(f->vExtendData, vStrideStopVec, Alignment(16));
      pc->add(f->srcp1, yMod.cloneAs(f->srcp1), mem_ptr(fn.fetchData(), REL_PATTERN(src.pixelData)));

      if (_alphaOffsetApplied)
        pc->add(f->srcp1, f->srcp1, _alphaOffsetApplied);
    }

    // Horizontal Extend
    // -----------------
    //
    // Horizontal extend modes are hardcoded for performance reasons. Every extend mode
    // requires different strategy to make horizontal advancing as fast as possible.

    if (extendX() == ExtendMode::kPad) {
      // Horizontal Pad
      // --------------
      //
      // There is not much to invent to clamp horizontally. The `f->x` is a raw coordinate that is clamped each
      // time it's used as an index. To make it fast we use two variables `x` and `xPadded`, which always contains
      // `x` clamped to `[x, w]` range. The advantage of this approach is that every time we increment `1` to `x` we
      // need only 2 instructions to calculate new `xPadded` value as it was already padded to the previous index,
      // and it could only get greater by `1` or stay where it was in a case we already reached the width `w`.

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      f->x          = pc->newGp32("f.x");             // Reg.
      f->xPadded    = pc->newGpPtr("f.xPadded");      // Reg.
      f->xOrigin    = pc->newGp32("f.xOrigin");       // Mem.
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      pc->load_u32(f->w, mem_ptr(fn.fetchData(), REL_PATTERN(src.size.w)));
      pc->load_u32(f->xOrigin, mem_ptr(fn.fetchData(), REL_PATTERN(simple.tx)));

      if (isPatternFy())
        pc->inc(f->xOrigin);

      if (isRectFill())
        pc->add(f->xOrigin, f->xOrigin, x);

      pc->dec(f->w);
    }

    if (extendX() == ExtendMode::kRepeat) {
      // Horizontal Repeat - AA-Only, Large Fills
      // ----------------------------------------
      //
      // This extend mode is only used to blit patterns that are tiled and that exceed some predefined width-limit
      // (like 16|32|etc). It's specialized for larger patterns because it contains a condition in fetchN() that
      // jumps if `f->x` is at the end or near of the patterns end. That's why the pattern width should be large
      // enough that this branch is not mispredicted often. For smaller patterns RoR more is more suitable as there
      // is no branch required and the repeat|reflect is handled by SIMD instructions.
      //
      // This implementation generally uses two tricks to make the tiling faster:
      //
      //   1. It changes row indexing from [0..width) to [-width..0). The reason for such change is that when ADD
      //      instruction is executed it updates processor FLAGS register, if SIGN flag is zero it means that repeat
      //      is needed. This saves us one condition.
      //
      //   2. It multiplies X coordinates (all of them) by pattern's BPP (bytes per pixel). The reason is to completely
      //      eliminate `index * scale` in memory addressing (and in case of weird BPP to eliminate IMUL).

      // NOTE: These all must be `intptr_t` because of memory indexing and the
      // use of the sign (when f->x is used as an index it's always negative).

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      f->x          = pc->newGpPtr("f.x");           // Reg.
      f->xOrigin    = pc->newGpPtr("f.xOrigin");     // Mem.
      f->xRestart   = pc->newGpPtr("f.xRestart");    // Mem.
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      pc->load_u32(f->w, mem_ptr(fn.fetchData(), REL_PATTERN(src.size.w)));
      pc->load_u32(f->xOrigin.r32(), mem_ptr(fn.fetchData(), REL_PATTERN(simple.tx)));

      if (isPatternFy())
        pc->inc(f->xOrigin.r32());

      if (isRectFill()) {
        pc->add(f->xOrigin.r32(), f->xOrigin.r32(), x);
        pc->umod(f->xOrigin.r32(), f->xOrigin.r32(), f->w);
      }

      pc->mul(f->w      , f->w      , int(bpp()));
      pc->mul(f->xOrigin, f->xOrigin, int(bpp()));

      pc->sub(f->xOrigin, f->xOrigin, f->w.cloneAs(f->xOrigin));
      pc->add(f->srcp1, f->srcp1, f->w.cloneAs(f->srcp1));
      pc->neg(f->xRestart, f->w.cloneAs(f->xRestart));
    }

    if (extendX() == ExtendMode::kRoR) {
      // Horizontal RoR [Repeat or Reflect]
      // ----------------------------------
      //
      // This mode handles both Repeat and Reflect cases. It uses the following formula to either REPEAT or REFLECT
      // X coordinate:
      //
      //   int index = (x >> 31) ^ x;
      //
      // The beauty of this method is that if X is negative it reflects, if it's positive it's kept as is. Then the
      // implementation handles both modes the following way:
      //
      //   1. REPEAT - X is always bound to interval [0...Width), so when the index is calculated it never reflects.
      //      When `f->x` reaches the pattern width it's simply corrected as `f->x -= f->rx`, where `f->rx` is equal
      //      to `pattern.size.w`.
      //
      //   2. REFLECT - X is always bound to interval [-Width...Width) so it can reflect. When `f->x` reaches the
      //      pattern width it's simply corrected as `f->x -= f->rx`, where `f->rx` is equal to `pattern.size.w * 2`
      //      so it goes negative.

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      f->x          = pc->newGp32("f.x");              // Reg.
      f->xOrigin    = pc->newGp32("f.xOrigin");        // Mem.
      f->xRestart   = pc->newGp32("f.xRestart");       // Mem.
      f->rx         = pc->newGp32("f.rx");             // Mem.

      if (maxPixels() >= 4) {
        f->xVec4    = pc->newV128("f.xVec4");          // Reg (fetchN).
        f->xSet4    = pc->newV128("f.xSet4");          // Mem (fetchN).
        f->xInc4    = pc->newV128("f.xInc4");          // Mem (fetchN).
        f->xNrm4    = pc->newV128("f.xNrm4");          // Mem (fetchN).
        f->xMax4    = pc->newV128("f.xMax4");          // Mem (fetchN).
      }
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      pc->load_u32(f->w , mem_ptr(fn.fetchData(), REL_PATTERN(src.size.w)));
      pc->load_u32(f->rx, mem_ptr(fn.fetchData(), REL_PATTERN(simple.rx)));

      if (maxPixels() >= 4) {
        pc->dec(f->w);
        pc->v_broadcast_u32(f->xMax4, f->w);
        pc->inc(f->w);

        pc->v_cvt_u8_to_u32(f->xSet4, mem_ptr(fn.fetchData(), REL_PATTERN(simple.ix)));
        pc->v_swizzle_u32x4(f->xInc4, f->xSet4, swizzle(3, 3, 3, 3));
        pc->v_sllb_u128(f->xSet4, f->xSet4, 4);
      }

      pc->sub(f->xRestart, f->w, f->rx);

      if (maxPixels() >= 4) {
        pc->v_broadcast_u32(f->xNrm4, f->rx);
      }

      pc->load_u32(f->xOrigin, mem_ptr(fn.fetchData(), REL_PATTERN(simple.tx)));

      if (isPatternFy())
        pc->inc(f->xOrigin);

      if (isRectFill()) {
        Gp norm = pc->newGp32("@norm");

        pc->add(f->xOrigin, f->xOrigin, x);
        pc->umod(f->xOrigin, f->xOrigin, f->rx);

        pc->select(norm, Imm(0), f->rx, ucmp_lt(f->xOrigin, f->w));
        pc->sub(f->xOrigin, f->xOrigin, norm);
      }
    }

    // Fractional - Fx|Fy|FxFy
    // -----------------------

    if (isPatternUnaligned()) {
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      f->pixL       = pc->newV128("f.pixL");           // Reg (Fx|FxFy).

      f->wb_wb      = pc->newV128("f.wb_wb");          // Mem (RGBA mode).
      f->wd_wd      = pc->newV128("f.wd_wd");          // Mem (RGBA mode).
      f->wc_wd      = pc->newV128("f.wc_wd");          // Mem (RGBA mode).
      f->wa_wb      = pc->newV128("f.wa_wb");          // Mem (RGBA mode).

      f->wd_wb      = pc->newV128("f.wd_wb");          // Mem (Alpha mode).
      f->wa_wc      = pc->newV128("f.wa_wc");          // Mem (Alpha mode).
      f->wb_wd      = pc->newV128("f.wb_wd");          // Mem (Alpha mode).
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      Vec weights = pc->newV128("weights");
      Mem wPtr = mem_ptr(fn.fetchData(), REL_PATTERN(simple.wa));

      // [00 Wd 00 Wc 00 Wb 00 Wa]
      pc->v_loadu128(weights, wPtr);
      // [Wd Wc Wb Wa Wd Wc Wb Wa]
      pc->v_packs_i32_i16(weights, weights, weights);

      if (isAlphaFetch()) {
        if (isPatternFy()) {
          pc->v_swizzle_lo_u16x4(f->wd_wb, weights, swizzle(3, 1, 3, 1));
          if (maxPixels() >= 4)
            pc->v_swizzle_u32x4(f->wd_wb, f->wd_wb, swizzle(1, 0, 1, 0));
        }
        else if (isPatternFx()) {
          pc->v_swizzle_u32x4(f->wc_wd, weights, swizzle(3, 3, 3, 3));
        }
        else {
          pc->v_swizzle_lo_u16x4(f->wa_wc, weights, swizzle(2, 0, 2, 0));
          pc->v_swizzle_lo_u16x4(f->wb_wd, weights, swizzle(3, 1, 3, 1));
          if (maxPixels() >= 4) {
            pc->v_swizzle_u32x4(f->wa_wc, f->wa_wc, swizzle(1, 0, 1, 0));
            pc->v_swizzle_u32x4(f->wb_wd, f->wb_wd, swizzle(1, 0, 1, 0));
          }
        }
      }
      else {
        // [Wd Wd Wc Wc Wb Wb Wa Wa]
        pc->v_interleave_lo_u16(weights, weights, weights);

        if (isPatternFy()) {
          pc->v_swizzle_u32x4(f->wb_wb, weights, swizzle(1, 1, 1, 1));
          pc->v_swizzle_u32x4(f->wd_wd, weights, swizzle(3, 3, 3, 3));
        }
        else if (isPatternFx()) {
          pc->v_swizzle_u32x4(f->wc_wd, weights, swizzle(2, 2, 3, 3));
        }
        else {
          pc->v_swizzle_u32x4(f->wa_wb, weights, swizzle(0, 0, 1, 1));
          pc->v_swizzle_u32x4(f->wc_wd, weights, swizzle(2, 2, 3, 3));
        }
      }
    }

    // If the pattern has a fractional Y then advance in vertical direction.
    // This ensures that both `srcp0` and `srcp1` are initialized, otherwise
    // `srcp0` would contain undefined content.
    if (hasFracY())
      advanceY();
  }
}

void FetchSimplePatternPart::_finiPart() noexcept {}

// bl::Pipeline::JIT::FetchSimplePatternPart - Utilities
// =====================================================

void FetchSimplePatternPart::swapStrideStopData(VecArray& v) noexcept {
  if (pc->is32Bit())
    pc->v_swap_u32(v, v);
  else
    pc->v_swap_u64(v, v);
}

void FetchSimplePatternPart::fetchRGBA32UnpackBroadcastStep1(const Vec& vec, const Mem& src) noexcept {
#if defined(BL_JIT_ARCH_X86)
  if (pc->hasSSE4_1() && !pc->hasAVX()) {
    pc->v_loadu32_u8_to_u32(vec, src);
    return;
  }
#endif // BL_JIT_ARCH_X86

  pc->v_broadcast_u32(vec, src);
}

void FetchSimplePatternPart::fetchRGBA32UnpackBroadcastStep2(const Vec& vec) noexcept {
#if defined(BL_JIT_ARCH_X86)
  if (pc->hasSSE4_1() && !pc->hasAVX()) {
    pc->v_packs_i32_i16(vec, vec, vec);
    return;
  }
#endif // BL_JIT_ARCH_X86

  pc->v_cvt_u8_lo_to_u16(vec, vec);
}

// bl::Pipeline::JIT::FetchSimplePatternPart - Advance
// ===================================================

void FetchSimplePatternPart::advanceY() noexcept {
  if (isAlignedBlit()) {
    // Blit AA
    // -------

    // That's the beauty of AABlit - no checks needed, no extend modes used.
    pc->add(f->srcp1, f->srcp1, f->stride);
  }
  else {
    // Vertical Extend Mode Handling
    // -----------------------------

    int kStrideArrayOffset = 0;
    int kYStopArrayOffset = int(pc->registerSize()) * 2;
    int kYRewindOffset = int(pc->registerSize()) * 4;
    int kPixelPtrRewindOffset = int(pc->registerSize()) * 5;

    Label L_Done = pc->newLabel();
    Label L_YStop = pc->newLabel();

    pc->inc(f->y);

    // If this pattern fetch uses two source pointers (one for current scanline
    // and one for previous one) copy current to the previous so it can be used
    // (only fetchers that use Fy).
    if (hasFracY())
      pc->mov(f->srcp0, f->srcp1);

    pc->j(L_YStop, cmp_eq(f->y, f->vExtendData.cloneAdjusted(kYStopArrayOffset)));
    pc->add(f->srcp1, f->srcp1, f->vExtendData.cloneAdjusted(kStrideArrayOffset));
    pc->bind(L_Done);

    PipeInjectAtTheEnd injected(pc);
    pc->bind(L_YStop);

    // Swap stride and yStop pairs.
    if (pc->is64Bit()) {
#if defined(BL_JIT_ARCH_X86)
      if (pc->hasAVX2()) {
        Vec v = pc->newV256("f.vTmp");
        pc->v_swap_u64(v, f->vExtendData);
        pc->v_storeu256(f->vExtendData, v);
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        Vec v = pc->newV128("f.vTmp");
        Mem strideArray = f->vExtendData.cloneAdjusted(kStrideArrayOffset);
        Mem yStopArray = f->vExtendData.cloneAdjusted(kYStopArrayOffset);
        pc->v_swap_u64(v, strideArray);
        pc->v_storea128(strideArray, v);
        pc->v_swap_u64(v, yStopArray);
        pc->v_storea128(yStopArray, v);
      }
    }
    else {
      Vec v0 = pc->newV128("f.vTmp");
      pc->v_swap_u32(v0, f->vExtendData);
      pc->v_storea128(f->vExtendData, v0);
    }

    // Rewind y and pixel-ptr.
    pc->sub(f->y, f->y, f->vExtendData.cloneAdjusted(kYRewindOffset));
    pc->sub(f->srcp1, f->srcp1, f->vExtendData.cloneAdjusted(kPixelPtrRewindOffset));
    pc->j(L_Done);
  }
}

void FetchSimplePatternPart::startAtX(const Gp& x) noexcept {
  if (isAlignedBlit()) {
    // Blit AA
    // -------

    // TODO: [PIPEGEN] Relax this constraint.
    // Rectangular blits only.
    BL_ASSERT(isRectFill());
  }
  else {
    pc->mov(f->x, f->xOrigin);                             // f->x = f->xOrigin;

    // Horizontal Pad
    // --------------

    if (extendX() == ExtendMode::kPad) {
      if (!isRectFill())
        pc->add(f->x, f->x, x);
      pc->sbound(f->xPadded.r32(), f->x, f->w);            // f->xPadded = signed_bound(f->x, f->w)
    }

    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    if (extendX() == ExtendMode::kRepeat) {
      if (!isRectFill()) {                                 // if (!RectFill) {
        pc->add_scaled(f->x, x, int(bpp()));               //   f->x += x * pattern.bpp;
        repeatOrReflectX();                                //   f->x = repeatLarge(f->x);
      }                                                    // }
    }

    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    if (extendX() == ExtendMode::kRoR) {
      if (!isRectFill()) {                                 // if (!RectFill) {
        pc->add(f->x, f->x, x);                            //   f->x += x;
        repeatOrReflectX();                                //   f->x = repeatOrReflect(f->x);
      }                                                    // }
    }
  }

  prefetchAccX();

  if (pixelGranularity() > 1)
    enterN();
}

void FetchSimplePatternPart::advanceX(const Gp& x, const Gp& diff) noexcept {
  blUnused(x);
  Gp fx32 = f->x.r32();

  if (pixelGranularity() > 1)
    leaveN();

  if (isAlignedBlit()) {
    // Blit AA
    // -------

    pc->add_scaled(f->srcp1, diff.cloneAs(f->srcp1), int(bpp()));
  }
  else if (extendX() == ExtendMode::kPad) {
    // Horizontal Pad
    // --------------

    if (hasFracX())                                        // if (hasFracX())
      pc->add_ext(fx32, fx32, diff, 1, -1);                //   f->x += diff - 1;
    else                                                   // else
      pc->add(fx32, fx32, diff);                           //   f->x += diff;

    pc->sbound(f->xPadded.r32(), f->x, f->w);              // f->xPadded = signed_bound(f->x, f->w)
  }
  else if (extendX() == ExtendMode::kRepeat) {
    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    pc->add_scaled(f->x, diff, int(bpp()));                // f->x += diff * pattern.bpp;
    repeatOrReflectX();                                    // f->x = repeatLarge(f->x);
  }
  else if (extendX() == ExtendMode::kRoR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    if (hasFracX())                                        // if (hasFracX())
      pc->add_ext(fx32, fx32, diff, 1, -1);                //   f->x += diff - 1;
    else                                                   // else
      pc->add(fx32, fx32, diff);                           //   f->x += diff;

    repeatOrReflectX();                                    // f->x = repeatOrReflect(f->x);
  }

  prefetchAccX();

  if (pixelGranularity() > 1)
    enterN();
}

void FetchSimplePatternPart::advanceXByOne() noexcept {
  if (isAlignedBlit()) {
    // Blit AA
    // -------

    pc->add(f->srcp1, f->srcp1, int(bpp()));
  }
  else if (extendX() == ExtendMode::kPad) {
    // Horizontal Pad
    // --------------

    pc->inc(f->x);
    pc->cmov(f->xPadded.r32(), f->x, ucmp_lt(f->x, f->w));
  }
  else if (extendX() == ExtendMode::kRepeat) {
    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    pc->cmov(f->x, f->xRestart, add_z(f->x, int(bpp())));
  }
  else if (extendX() == ExtendMode::kRoR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    pc->inc(f->x);
    pc->cmov(f->x, f->xRestart, cmp_eq(f->x, f->w));
  }
}

void FetchSimplePatternPart::repeatOrReflectX() noexcept {
  if (isAlignedBlit()) {
    // Blit AA
    // -------

    // Nothing...
  }
  else if (extendX() == ExtendMode::kRepeat) {
    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    Label L_HorzSkip = pc->newLabel();

    pc->j(L_HorzSkip, scmp_lt(f->x, 0));                   // if (f->x >= 0 &&
    pc->j(L_HorzSkip, add_c(f->x, f->xRestart));           //     f->x -= f->w >= 0) {
    // `f->x` too large to be corrected by `f->w`, so do it the slow way:
    pc->umod(f->x.r32(), f->x.r32(), f->w);                //   f->x %= f->w;
    pc->add(f->x, f->x, f->xRestart);                      //   f->x -= f->w;
    pc->bind(L_HorzSkip);                                  // }
  }
  else if (extendX() == ExtendMode::kRoR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    Label L_HorzSkip = pc->newLabel();
    Gp norm = pc->newGp32("@norm");

    pc->j(L_HorzSkip, scmp_lt(f->x, f->rx));               // if (f->x >= f->rx) {
    pc->umod(f->x, f->x, f->rx);                           //   f->x %= f->rx;
    pc->bind(L_HorzSkip);                                  // }
    pc->select(norm, Imm(0), f->rx, ucmp_lt(f->x, f->w));  // norm = (f->x < f->w) ? 0 : f->rx;
    pc->sub(f->x, f->x, norm);                             // f->x -= norm;
  }
}

void FetchSimplePatternPart::prefetchAccX() noexcept {
  if (!hasFracX())
    return;

  Gp idx;

  // Horizontal Pad
  // --------------

  if (extendX() == ExtendMode::kPad) {
    idx = f->xPadded;
  }

  // Horizontal Repeat - AA-Only, Large Fills
  // ----------------------------------------

  if (extendX() == ExtendMode::kRepeat) {
    idx = f->x;
  }

  // Horizontal RoR [Repeat or Reflect]
  // ----------------------------------

  if (extendX() == ExtendMode::kRoR) {
    idx = pc->newGpPtr("@idx");
    pc->reflect(idx.r32(), f->x);
  }

  if (isAlphaFetch()) {
    if (isPatternFx()) {
      pc->v_load8(f->pixL, mem_ptr(f->srcp1, idx, _idxShift));
    }
    else {
      pc->v_load8(f->pixL, mem_ptr(f->srcp0, idx, _idxShift));
      pc->x_insert_word_or_byte(f->pixL, mem_ptr(f->srcp1, idx, _idxShift), 1);
    }
  }
  else {
    if (isPatternFx()) {
      pc->v_loadu32(f->pixL, mem_ptr(f->srcp1, idx, _idxShift));
      pc->v_cvt_u8_lo_to_u16(f->pixL, f->pixL);
      pc->v_mul_i16(f->pixL, f->pixL, f->wc_wd);
    }
    else {
      Vec pixL = f->pixL;
      Vec pixT = pc->newV128("@pixT");

      pc->v_loadu32(pixL, mem_ptr(f->srcp0, idx, _idxShift));
      pc->v_loadu32(pixT, mem_ptr(f->srcp1, idx, _idxShift));

      pc->v_cvt_u8_lo_to_u16(pixL, pixL);
      pc->v_cvt_u8_lo_to_u16(pixT, pixT);

      pc->v_mul_i16(pixL, pixL, f->wa_wb);
      pc->v_mul_i16(pixT, pixT, f->wc_wd);

      pc->v_add_i16(pixL, pixL, pixT);
    }
}

  advanceXByOne();
}

// bl::Pipeline::JIT::FetchSimplePatternPart - Fetch
// =================================================

void FetchSimplePatternPart::enterN() noexcept {
  if (isAlignedBlit()) {
    // Blit AA
    // -------

    // Nothing...
  }
  else if (extendX() == ExtendMode::kPad) {
    // Horizontal Pad
    // --------------

    // Nothing...
  }
  else if (extendX() == ExtendMode::kRoR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    Vec xFix4 = pc->newV128("@xFix4");

    pc->v_broadcast_u32(f->xVec4, f->x.r32());
    pc->v_add_i32(f->xVec4, f->xVec4, f->xSet4);

    pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
    pc->v_and_i32(xFix4, xFix4, f->xNrm4);
    pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
  }
}

void FetchSimplePatternPart::leaveN() noexcept {
  if (isAlignedBlit()) {
    // Blit AA
    // -------

    // Nothing...
  }
  else if (extendX() == ExtendMode::kPad) {
    // Horizontal Pad
    // --------------

    // Nothing...
  }
  else if (extendX() == ExtendMode::kRoR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    pc->s_mov_u32(f->x.r32(), f->xVec4);
  }
}

void FetchSimplePatternPart::prefetchN() noexcept {}
void FetchSimplePatternPart::postfetchN() noexcept {}

void FetchSimplePatternPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  p.setCount(n);

  if (!blTestFlag(flags, PixelFlags::kPA_PI_UA_UI | PixelFlags::kPC_UC)) {
    if (p.isRGBA32())
      flags |= PixelFlags::kPC;
    else
      flags |= PixelFlags::kPA;
  }

  if (isAlignedBlit()) {
    FetchUtils::x_fetch_pixel(pc, p, n, flags, format(), mem_ptr(f->srcp1), Alignment(4), predicate);
    if (predicate.empty())
      pc->add(f->srcp1, f->srcp1, int(n.value() * bpp()));
    else
      pc->add_scaled(f->srcp1, predicate.count, bpp());
    return;
  }

  BL_ASSERT(predicate.empty());
  switch (n.value()) {
    case 1: {
      Gp idx;

      // Pattern AA or Fx/Fy
      // -------------------

      if (extendX() == ExtendMode::kPad) {
        idx = f->xPadded;
      }

      if (extendX() == ExtendMode::kRepeat) {
        idx = f->x;
      }

      if (extendX() == ExtendMode::kRoR) {
        idx = pc->newGpPtr("@idx");
        pc->reflect(idx.r32(), f->x);
      }

      if (isPatternAligned()) {
        FetchUtils::x_fetch_pixel(pc, p, PixelCount(1), flags, format(), mem_ptr(f->srcp1, idx, _idxShift), Alignment(4));
        advanceXByOne();
      }

      if (isPatternFy()) {
        if (isAlphaFetch()) {
          Vec pixA = pc->newV128("@pixA");

          FetchUtils::x_fetch_unpacked_a8_2x(pc, pixA, format(), mem_ptr(f->srcp1, idx, _idxShift), mem_ptr(f->srcp0, idx, _idxShift));
          pc->v_mhadd_i16_to_i32(pixA, pixA, f->wd_wb);
          pc->v_srli_u16(pixA, pixA, 8);

          advanceXByOne();

          FetchUtils::x_assign_unpacked_alpha_values(pc, p, flags, pixA);
          FetchUtils::x_satisfy_pixel(pc, p, flags);
        }
        else if (p.isRGBA32()) {
          Vec pix0 = pc->newV128("@pix0");
          Vec pix1 = pc->newV128("@pix1");

          pc->v_loadu32(pix0, mem_ptr(f->srcp0, idx, _idxShift));
          pc->v_loadu32(pix1, mem_ptr(f->srcp1, idx, _idxShift));

          pc->v_cvt_u8_lo_to_u16(pix0, pix0);
          pc->v_cvt_u8_lo_to_u16(pix1, pix1);

          pc->v_mul_i16(pix0, pix0, f->wb_wb);
          pc->v_mul_i16(pix1, pix1, f->wd_wd);

          advanceXByOne();

          pc->v_add_i16(pix0, pix0, pix1);
          pc->v_srli_u16(pix0, pix0, 8);

          p.uc.init(pix0);
          FetchUtils::x_satisfy_pixel(pc, p, flags);
        }
      }

      if (isPatternFx()) {
        if (isAlphaFetch()) {
          Vec pixL = f->pixL;
          Vec pixA = pc->newV128("@pixA");

          pc->x_insert_word_or_byte(pixL, mem_ptr(f->srcp1, idx, _idxShift), 1);
          pc->v_mhadd_i16_to_i32(pixA, pixL, f->wc_wd);
          pc->v_srli_u32(pixL, pixL, 16);
          pc->v_srli_u16(pixA, pixA, 8);

          advanceXByOne();

          FetchUtils::x_assign_unpacked_alpha_values(pc, p, flags, pixA);
          FetchUtils::x_satisfy_pixel(pc, p, flags);
        }
        else if (p.isRGBA32()) {
          Vec pixL = f->pixL;
          Vec pix0 = pc->newV128("@pix0");


          pc->v_swap_u64(pix0, pixL);
          fetchRGBA32UnpackBroadcastStep1(pixL, mem_ptr(f->srcp1, idx, _idxShift));
          fetchRGBA32UnpackBroadcastStep2(pixL);
          pc->v_mul_i16(pixL, pixL, f->wc_wd);
          advanceXByOne();

          pc->v_add_i16(pix0, pix0, pixL);
          pc->v_srli_u16(pix0, pix0, 8);

          p.uc.init(pix0);
          FetchUtils::x_satisfy_pixel(pc, p, flags);
        }
      }

      if (isPatternFxFy()) {
        if (isAlphaFetch()) {
          Vec pixL = f->pixL;
          Vec pixA = pc->newV128("@pixA");
          Vec pixB = pc->newV128("@pixB");

          pc->v_load_u8_u16_2x(pixB, mem_ptr(f->srcp0, idx, _idxShift), mem_ptr(f->srcp1, idx, _idxShift));
          pc->v_mhadd_i16_to_i32(pixA, pixL, f->wa_wc);
          pc->v_mov(pixL, pixB);
          pc->v_mhadd_i16_to_i32(pixB, pixB, f->wb_wd);
          pc->v_add_i32(pixA, pixA, pixB);
          pc->v_srli_u16(pixA, pixA, 8);

          advanceXByOne();

          FetchUtils::x_assign_unpacked_alpha_values(pc, p, flags, pixA);
          FetchUtils::x_satisfy_pixel(pc, p, flags);
        }
        else if (p.isRGBA32()) {
          Vec pixL = f->pixL;
          Vec pixT = pc->newV128("@pixT");
          Vec pix0 = pc->newV128("@pix0");

          fetchRGBA32UnpackBroadcastStep1(pixT, mem_ptr(f->srcp1, idx, _idxShift));
          pc->v_swap_u64(pix0, pixL);
          fetchRGBA32UnpackBroadcastStep1(pixL, mem_ptr(f->srcp0, idx, _idxShift));

          fetchRGBA32UnpackBroadcastStep2(pixT);
          fetchRGBA32UnpackBroadcastStep2(pixL);

          pc->v_mul_i16(pixT, pixT, f->wc_wd);
          pc->v_mul_i16(pixL, pixL, f->wa_wb);

          advanceXByOne();

          pc->v_add_i16(pixL, pixL, pixT);
          pc->v_add_i16(pix0, pix0, pixL);
          pc->v_srli_u16(pix0, pix0, 8);

          p.uc.init(pix0);
          FetchUtils::x_satisfy_pixel(pc, p, flags);
        }
      }
      break;
    }

    case 4: {
      PixelType intermediateType = isAlphaFetch() ? PixelType::kA8 : PixelType::kRGBA32;
      PixelFlags intermediateFlags = isAlphaFetch() ? PixelFlags::kUA : PixelFlags::kUC;

      // Horizontal Pad
      // --------------

      if (extendX() == ExtendMode::kPad) {
        // Horizontal Pad - Aligned
        // ------------------------

        if (isPatternAligned()) {
          FetchUtils::FetchContext fCtx(pc, &p, PixelCount(4), format(), flags);

          Gp idx = f->xPadded;
          Mem mem = mem_ptr(f->srcp1, idx, _idxShift);

          pc->inc(f->x);
          fCtx.fetchPixel(mem);
          pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

          pc->inc(f->x);
          fCtx.fetchPixel(mem);
          pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

          pc->inc(f->x);
          fCtx.fetchPixel(mem);
          pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

          pc->inc(f->x);
          fCtx.fetchPixel(mem);
          pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

          fCtx.end();
          FetchUtils::x_satisfy_pixel(pc, p, flags);
        }

        // Horizontal Pad - FracY
        // ----------------------

        if (isPatternFy()) {
          Gp idx = f->xPadded;

          if (isAlphaFetch()) {
            Pixel fPix("fPix", intermediateType);
            FetchUtils::FetchContext fCtx(pc, &fPix, PixelCount(8), format(), intermediateFlags);

            Mem m0 = mem_ptr(f->srcp0, idx, _idxShift);
            Mem m1 = mem_ptr(f->srcp1, idx, _idxShift);

            pc->inc(f->x);
            fCtx.fetchPixel(m0);
            fCtx.fetchPixel(m1);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->inc(f->x);
            fCtx.fetchPixel(m0);
            fCtx.fetchPixel(m1);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->inc(f->x);
            fCtx.fetchPixel(m0);
            fCtx.fetchPixel(m1);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            fCtx.fetchPixel(m0);
            fCtx.fetchPixel(m1);
            fCtx.end();

            Vec& pix0 = fPix.ua[0];

            pc->inc(f->x);
            pc->v_mhadd_i16_to_i32(pix0, pix0, f->wd_wb);
            pc->v_srli_u16(pix0, pix0, 8);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->v_packs_i32_i16(pix0, pix0, pix0);
            FetchUtils::x_assign_unpacked_alpha_values(pc, p, flags, pix0);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
          else if (p.isRGBA32()) {
            Pixel pix0("pix0", intermediateType);
            Pixel pix1("pix1", intermediateType);

            FetchUtils::FetchContext fCtx0(pc, &pix0, PixelCount(4), format(), intermediateFlags);
            FetchUtils::FetchContext fCtx1(pc, &pix1, PixelCount(4), format(), intermediateFlags);

            Mem m0 = mem_ptr(f->srcp0, idx, _idxShift);
            Mem m1 = mem_ptr(f->srcp1, idx, _idxShift);

            pc->inc(f->x);
            fCtx0.fetchPixel(m0);
            fCtx1.fetchPixel(m1);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->inc(f->x);
            fCtx0.fetchPixel(m0);
            fCtx1.fetchPixel(m1);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->inc(f->x);
            fCtx0.fetchPixel(m0);
            fCtx1.fetchPixel(m1);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->inc(f->x);
            fCtx0.fetchPixel(m0);
            fCtx1.fetchPixel(m1);
            fCtx0.end();
            fCtx1.end();

            pc->v_mul_i16(pix0.uc, pix0.uc, f->wb_wb);
            pc->v_mul_i16(pix1.uc, pix1.uc, f->wd_wd);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->v_add_i16(pix0.uc, pix0.uc, pix1.uc);
            pc->v_srli_u16(pix0.uc, pix0.uc, 8);

            p.uc.init(pix0.uc[0], pix0.uc[1]);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
        }

        // Horizontal Pad - FracX
        // ----------------------

        if (isPatternFx()) {
          Gp idx = f->xPadded;
          Mem m = mem_ptr(f->srcp1, idx, _idxShift);

          if (isAlphaFetch()) {
            Pixel fPix("fPix", intermediateType);
            FetchUtils::FetchContext fCtx(pc, &fPix, PixelCount(4), format(), intermediateFlags);

            Vec& pixA = fPix.ua[0];
            Vec& pixL = f->pixL;

            pc->inc(f->x);
            fCtx.fetchPixel(m);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->inc(f->x);
            fCtx.fetchPixel(m);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->inc(f->x);
            fCtx.fetchPixel(m);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->inc(f->x);
            fCtx.fetchPixel(m);
            fCtx.end();

            pc->v_interleave_lo_u16(pixA, pixA, pixA);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));
            pc->v_sllb_u128(pixA, pixA, 2);

            pc->v_or_i32(pixL, pixL, pixA);
            pc->v_mhadd_i16_to_i32(pixA, pixL, f->wc_wd);

            pc->v_srlb_u128(pixL, pixL, 14);
            pc->v_srli_u32(pixA, pixA, 8);
            pc->v_packs_i32_i16(pixA, pixA, pixA);

            FetchUtils::x_assign_unpacked_alpha_values(pc, p, flags, pixA);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
          else if (p.isRGBA32()) {
            Vec pixL = f->pixL;
            Vec pixT = pc->newV128("@pixT");

            Vec pix0 = pc->newV128("@pix0");
            Vec pix1 = pc->newV128("@pix1");
            Vec pix2 = pc->newV128("@pix2");

            pc->inc(f->x);
            fetchRGBA32UnpackBroadcastStep1(pix0, m);

            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));
            pc->inc(f->x);

            fetchRGBA32UnpackBroadcastStep1(pix1, m);
            fetchRGBA32UnpackBroadcastStep2(pix0);

            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));
            fetchRGBA32UnpackBroadcastStep2(pix1);

            pc->v_mul_i16(pix0, pix0, f->wc_wd);
            pc->v_mul_i16(pix1, pix1, f->wc_wd);

            pc->inc(f->x);
            fetchRGBA32UnpackBroadcastStep1(pix2, m);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->v_combine_lo_hi_u64(pixT, pix1, pixL);
            fetchRGBA32UnpackBroadcastStep1(pixL, m);

            fetchRGBA32UnpackBroadcastStep2(pix2);
            fetchRGBA32UnpackBroadcastStep2(pixL);

            pc->v_add_i16(pix0, pix0, pixT);
            pc->v_mul_i16(pixL, pixL, f->wc_wd);
            pc->v_mul_i16(pix2, pix2, f->wc_wd);
            pc->v_srli_u16(pix0, pix0, 8);

            pc->v_combine_lo_hi_u64(pix1, pixL, pix1);
            pc->inc(f->x);
            pc->v_add_i16(pix2, pix2, pix1);
            pc->v_srli_u16(pix2, pix2, 8);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            p.uc.init(pix0, pix2);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
        }

        // Horizontal Pad - FracXY
        // -----------------------

        if (isPatternFxFy()) {
          Gp idx = f->xPadded;
          Mem mA = mem_ptr(f->srcp0, idx, _idxShift);
          Mem mB = mem_ptr(f->srcp1, idx, _idxShift);

          if (isAlphaFetch()) {
            Pixel fPix("fPix", intermediateType);
            FetchUtils::FetchContext fCtx(pc, &fPix, PixelCount(8), format(), intermediateFlags);

            pc->inc(f->x);
            fCtx.fetchPixel(mA);
            fCtx.fetchPixel(mB);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->inc(f->x);
            fCtx.fetchPixel(mA);
            fCtx.fetchPixel(mB);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->inc(f->x);
            fCtx.fetchPixel(mA);
            fCtx.fetchPixel(mB);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            fCtx.fetchPixel(mA);
            fCtx.fetchPixel(mB);
            fCtx.end();

            Vec& pixA = fPix.ua[0];
            Vec  pixB = pc->newV128("pixB");
            Vec& pixL = f->pixL;

            pc->v_sllb_u128(pixB, pixA, 4);
            pc->v_or_i32(pixB, pixB, pixL);
            pc->v_srlb_u128(pixL, pixA, 12);

            pc->v_mhadd_i16_to_i32(pixA, pixA, f->wb_wd);
            pc->v_mhadd_i16_to_i32(pixB, pixB, f->wa_wc);

            pc->inc(f->x);
            pc->v_add_i32(pixA, pixA, pixB);
            pc->v_srli_u32(pixA, pixA, 8);

            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));
            pc->v_packs_i32_i16(pixA, pixA, pixA);

            FetchUtils::x_assign_unpacked_alpha_values(pc, p, flags, pixA);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
          else if (p.isRGBA32()) {
            Vec pixL = f->pixL;
            Vec pixT = pc->newV128("@pixT");

            Vec pix0  = pc->newV128("@pix0");
            Vec pix0t = pc->newV128("@pix0t");
            Vec pix1  = pc->newV128("@pix1");
            Vec pix1t = pc->newV128("@pix1t");
            Vec pix2  = pc->newV128("@pix2");
            Vec pix2t = pc->newV128("@pix2t");

            pc->inc(f->x);

            fetchRGBA32UnpackBroadcastStep1(pix0, mA);
            fetchRGBA32UnpackBroadcastStep1(pix0t, mB);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            fetchRGBA32UnpackBroadcastStep1(pix1, mA);
            fetchRGBA32UnpackBroadcastStep1(pix1t, mB);
            pc->inc(f->x);

            fetchRGBA32UnpackBroadcastStep2(pix0);
            fetchRGBA32UnpackBroadcastStep2(pix0t);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            fetchRGBA32UnpackBroadcastStep2(pix1);
            fetchRGBA32UnpackBroadcastStep2(pix1t);
            pc->inc(f->x);

            pc->v_mul_i16(pix1 , pix1 , f->wa_wb);
            pc->v_mul_i16(pix1t, pix1t, f->wc_wd);
            pc->v_mul_i16(pix0 , pix0 , f->wa_wb);
            pc->v_mul_i16(pix0t, pix0t, f->wc_wd);

            pc->v_add_i16(pix1, pix1, pix1t);
            pc->v_loadu32_u8_to_u32(pix2, mA);
            pc->v_add_i16(pix0, pix0, pix0t);
            pc->v_loadu32_u8_to_u32(pix2t, mB);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));

            pc->v_combine_lo_hi_u64(pixT, pix1, pixL);
            pc->v_loadu32_u8_to_u32(pixL, mA);
            pc->v_add_i16(pix0, pix0, pixT);
            pc->v_loadu32_u8_to_u32(pixT, mB);

            pc->v_packs_i32_i16(pixL, pixL, pixL);
            pc->v_packs_i32_i16(pix2 , pix2, pix2);
            pc->v_packs_i32_i16(pix2t, pix2t, pix2t);
            pc->v_mul_i16(pixL, pixL, f->wa_wb);
            pc->v_packs_i32_i16(pixT, pixT, pixT);

            pc->v_mul_i16(pix2, pix2, f->wa_wb);
            pc->v_mul_i16(pixT, pixT, f->wc_wd);
            pc->v_mul_i16(pix2t, pix2t, f->wc_wd);
            pc->v_srli_u16(pix0, pix0, 8);

            pc->v_add_i16(pixL, pixL, pixT);
            pc->v_add_i16(pix2, pix2, pix2t);
            pc->inc(f->x);
            pc->v_combine_lo_hi_u64(pix1, pixL, pix1);
            pc->v_add_i16(pix2, pix2, pix1);
            pc->cmov(idx.r32(), f->x, ucmp_le(f->x, f->w));
            pc->v_srli_u16(pix2, pix2, 8);

            p.uc.init(pix0, pix2);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
        }
      }

      // Horizontal RoR [Repeat or Reflect]
      // ----------------------------------

      if (extendX() == ExtendMode::kRoR) {
        Vec xIdx4 = pc->newV128("@xIdx4");
        Vec xFix4 = pc->newV128("@xFix4");

        // Horizontal RoR - Aligned
        // ------------------------

        if (isPatternAligned()) {
          FetchUtils::FetchContext fCtx(pc, &p, PixelCount(4), format(), flags);

          pc->v_srai_i32(xIdx4, f->xVec4, 31);
          pc->v_xor_i32(xIdx4, xIdx4, f->xVec4);
          pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);
          FetchUtils::fetch_4x(&fCtx, mem_ptr(f->srcp1), xIdx4, _idxShift);

          pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
          pc->v_and_i32(xFix4, xFix4, f->xNrm4);
          pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);

          fCtx.end();
          FetchUtils::x_satisfy_pixel(pc, p, flags);
        }

        // Horizontal RoR - FracY
        // ----------------------

        if (isPatternFy()) {
          pc->v_srai_i32(xIdx4, f->xVec4, 31);
          pc->v_xor_i32(xIdx4, xIdx4, f->xVec4);
          pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);

          if (isAlphaFetch()) {
            Pixel fPix("fPix", intermediateType);
            FetchUtils::FetchContext fCtx(pc, &fPix, PixelCount(8), format(), intermediateFlags);

            FetchUtils::fetch_4x_twice(&fCtx, mem_ptr(f->srcp0), &fCtx, mem_ptr(f->srcp1), xIdx4, _idxShift);
            fCtx.end();

            Vec& pix0 = fPix.ua[0];

            pc->v_mhadd_i16_to_i32(pix0, pix0, f->wd_wb);
            pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);

            pc->v_srli_u16(pix0, pix0, 8);
            pc->v_and_i32(xFix4, xFix4, f->xNrm4);

            pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
            pc->v_packs_i32_i16(pix0, pix0, pix0);

            FetchUtils::x_assign_unpacked_alpha_values(pc, p, flags, pix0);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
          else if (p.isRGBA32()) {
            Pixel pix0("pix0", p.type());
            Pixel pix1("pix1", p.type());

            FetchUtils::FetchContext fCtx0(pc, &pix0, PixelCount(4), format(), PixelFlags::kUC);
            FetchUtils::FetchContext fCtx1(pc, &pix1, PixelCount(4), format(), PixelFlags::kUC);
            FetchUtils::fetch_4x_twice(&fCtx0, mem_ptr(f->srcp0), &fCtx1, mem_ptr(f->srcp1), xIdx4, _idxShift);

            fCtx0.end();
            fCtx1.end();

            pc->v_mul_i16(pix0.uc, pix0.uc, f->wb_wb);
            pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
            pc->v_mul_i16(pix1.uc, pix1.uc, f->wd_wd);

            pc->v_and_i32(xFix4, xFix4, f->xNrm4);
            pc->v_add_i16(pix0.uc, pix0.uc, pix1.uc);

            pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
            pc->v_srli_u16(pix0.uc, pix0.uc, 8);

            p.uc.init(pix0.uc[0], pix0.uc[1]);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
        }

        // Horizontal RoR - FracX
        // ----------------------

        if (isPatternFx()) {
          pc->v_srai_i32(xIdx4, f->xVec4, 31);
          pc->v_xor_i32(xIdx4, xIdx4, f->xVec4);

          if (isAlphaFetch()) {
            Pixel fPix("fPix", intermediateType);
            FetchUtils::FetchContext fCtx(pc, &fPix, PixelCount(4), format(), intermediateFlags);

            FetchUtils::fetch_4x(&fCtx, mem_ptr(f->srcp1), xIdx4, _idxShift);
            fCtx.end();

            Vec& pixA = fPix.ua[0];
            Vec& pixL = f->pixL;

            pc->v_interleave_lo_u16(pixA, pixA, pixA);
            pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);

            pc->v_sllb_u128(pixA, pixA, 2);
            pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
            pc->v_or_i32(pixL, pixL, pixA);

            pc->v_and_i32(xFix4, xFix4, f->xNrm4);
            pc->v_mhadd_i16_to_i32(pixA, pixL, f->wc_wd);

            pc->v_srlb_u128(pixL, pixL, 14);
            pc->v_srli_u32(pixA, pixA, 8);

            pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
            pc->v_packs_i32_i16(pixA, pixA, pixA);

            FetchUtils::x_assign_unpacked_alpha_values(pc, p, flags, pixA);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
          else if (p.isRGBA32()) {
            FetchUtils::IndexExtractor iExt(pc);
            iExt.begin(FetchUtils::IndexExtractor::kTypeUInt32, xIdx4);

            Gp idx0 = pc->newGpPtr("@idx0");
            Gp idx1 = pc->newGpPtr("@idx1");

            Vec pixL = f->pixL;
            Vec pixT = pc->newV128("@pixT");

            Vec pix0 = pc->newV128("@pix0");
            Vec pix1 = pc->newV128("@pix1");
            Vec pix2 = pc->newV128("@pix2");

            pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);
            iExt.extract(idx0, 0);

            pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
            iExt.extract(idx1, 1);
            pc->v_and_i32(xFix4, xFix4, f->xNrm4);

            fetchRGBA32UnpackBroadcastStep1(pix0, mem_ptr(f->srcp1, idx0, _idxShift));
            iExt.extract(idx0, 2);

            fetchRGBA32UnpackBroadcastStep1(pix1, mem_ptr(f->srcp1, idx1, _idxShift));
            iExt.extract(idx1, 3);

            pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
            fetchRGBA32UnpackBroadcastStep2(pix1);
            fetchRGBA32UnpackBroadcastStep2(pix0);

            pc->v_mul_i16(pix1, pix1, f->wc_wd);
            pc->v_mul_i16(pix0, pix0, f->wc_wd);

            fetchRGBA32UnpackBroadcastStep1(pix2, mem_ptr(f->srcp1, idx0, _idxShift));
            pc->v_combine_lo_hi_u64(pixT, pix1, pixL);

            fetchRGBA32UnpackBroadcastStep1(pixL, mem_ptr(f->srcp1, idx1, _idxShift));
            fetchRGBA32UnpackBroadcastStep2(pix2);
            fetchRGBA32UnpackBroadcastStep2(pixL);

            pc->v_add_i16(pix0, pix0, pixT);
            pc->v_mul_i16(pixL, pixL, f->wc_wd);
            pc->v_mul_i16(pix2, pix2, f->wc_wd);
            pc->v_srli_u16(pix0, pix0, 8);

            pc->v_combine_lo_hi_u64(pix1, pixL, pix1);
            pc->v_add_i16(pix2, pix2, pix1);
            pc->v_srli_u16(pix2, pix2, 8);

            p.uc.init(pix0, pix2);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
        }

        // Horizontal RoR - FracXY
        // -----------------------

        if (isPatternFxFy()) {
          pc->v_srai_i32(xIdx4, f->xVec4, 31);
          pc->v_xor_i32(xIdx4, xIdx4, f->xVec4);

          if (isAlphaFetch()) {
            Pixel fPix("fPix", intermediateType);
            FetchUtils::FetchContext fCtx(pc, &fPix, PixelCount(8), format(), intermediateFlags);

            FetchUtils::fetch_4x_twice(&fCtx, mem_ptr(f->srcp0), &fCtx, mem_ptr(f->srcp1), xIdx4, _idxShift);
            fCtx.end();

            Vec& pixA = fPix.ua[0];
            Vec  pixB = pc->newV128("pixB");
            Vec& pixL = f->pixL;

            pc->v_sllb_u128(pixB, pixA, 4);
            pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);

            pc->v_or_i32(pixB, pixB, pixL);
            pc->v_srlb_u128(pixL, pixA, 12);

            pc->v_mhadd_i16_to_i32(pixA, pixA, f->wb_wd);
            pc->v_mhadd_i16_to_i32(pixB, pixB, f->wa_wc);
            pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);

            pc->v_add_i32(pixA, pixA, pixB);
            pc->v_and_i32(xFix4, xFix4, f->xNrm4);

            pc->v_srli_u32(pixA, pixA, 8);
            pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
            pc->v_packs_i32_i16(pixA, pixA, pixA);

            FetchUtils::x_assign_unpacked_alpha_values(pc, p, flags, pixA);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
          else if (p.isRGBA32()) {
            FetchUtils::IndexExtractor iExt(pc);

            Gp idx0 = pc->newGpPtr("@idx0");
            Gp idx1 = pc->newGpPtr("@idx1");

            Vec pixL = f->pixL;
            Vec pixT = pc->newV128("@pixT");

            Vec pix0  = pc->newV128("@pix0");
            Vec pix0t = pc->newV128("@pix0t");
            Vec pix1  = pc->newV128("@pix1");
            Vec pix1t = pc->newV128("@pix1t");
            Vec pix2  = pc->newV128("@pix2");
            Vec pix2t = pc->newV128("@pix2t");

            iExt.begin(FetchUtils::IndexExtractor::kTypeUInt32, xIdx4);

            pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);
            iExt.extract(idx0, 0);

            pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
            iExt.extract(idx1, 1);
            pc->v_and_i32(xFix4, xFix4, f->xNrm4);

            fetchRGBA32UnpackBroadcastStep1(pix0 , mem_ptr(f->srcp0, idx0, _idxShift));
            fetchRGBA32UnpackBroadcastStep1(pix0t, mem_ptr(f->srcp1, idx0, _idxShift));
            iExt.extract(idx0, 2);
            pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);

            fetchRGBA32UnpackBroadcastStep1(pix1 , mem_ptr(f->srcp0, idx1, _idxShift));
            fetchRGBA32UnpackBroadcastStep1(pix1t, mem_ptr(f->srcp1, idx1, _idxShift));
            iExt.extract(idx1, 3);

            fetchRGBA32UnpackBroadcastStep2(pix0);
            fetchRGBA32UnpackBroadcastStep2(pix0t);
            fetchRGBA32UnpackBroadcastStep2(pix1);
            fetchRGBA32UnpackBroadcastStep2(pix1t);

            pc->v_mul_i16(pix1 , pix1 , f->wa_wb);
            pc->v_mul_i16(pix1t, pix1t, f->wc_wd);
            pc->v_mul_i16(pix0 , pix0 , f->wa_wb);
            pc->v_mul_i16(pix0t, pix0t, f->wc_wd);

            pc->v_add_i16(pix1, pix1, pix1t);
            fetchRGBA32UnpackBroadcastStep1(pix2, mem_ptr(f->srcp0, idx0, _idxShift));
            pc->v_add_i16(pix0, pix0, pix0t);
            fetchRGBA32UnpackBroadcastStep1(pix2t, mem_ptr(f->srcp1, idx0, _idxShift));

            pc->v_combine_lo_hi_u64(pixT, pix1, pixL);
            fetchRGBA32UnpackBroadcastStep1(pixL, mem_ptr(f->srcp0, idx1, _idxShift));
            pc->v_add_i16(pix0, pix0, pixT);
            fetchRGBA32UnpackBroadcastStep1(pixT, mem_ptr(f->srcp1, idx1, _idxShift));

            fetchRGBA32UnpackBroadcastStep2(pixL);
            fetchRGBA32UnpackBroadcastStep2(pix2);
            fetchRGBA32UnpackBroadcastStep2(pix2t);
            pc->v_mul_i16(pixL, pixL, f->wa_wb);
            fetchRGBA32UnpackBroadcastStep2(pixT);

            pc->v_mul_i16(pix2, pix2, f->wa_wb);
            pc->v_mul_i16(pixT, pixT, f->wc_wd);
            pc->v_mul_i16(pix2t, pix2t, f->wc_wd);
            pc->v_srli_u16(pix0, pix0, 8);

            pc->v_add_i16(pixL, pixL, pixT);
            pc->v_add_i16(pix2, pix2, pix2t);
            pc->v_combine_lo_hi_u64(pix1, pixL, pix1);
            pc->v_add_i16(pix2, pix2, pix1);
            pc->v_srli_u16(pix2, pix2, 8);

            p.uc.init(pix0, pix2);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
        }
      }

      // Horizontal Repeat - AA-Only, Large Fills
      // ----------------------------------------

      if (extendX() == ExtendMode::kRepeat) {
        // Only generated for AA patterns.
        BL_ASSERT(isPatternAligned());

        FetchUtils::FetchContext fCtx(pc, &p, PixelCount(4), format(), flags);

        int offset = int(4 * bpp());

        Label L_Repeat = pc->newLabel();
        Label L_Done = pc->newLabel();

#if defined(BL_JIT_ARCH_X86)
        // This forms a pointer that takes advantage of X86 addressing [base + index + offset].
        // What we want to do in the fast case is to just read [base + x - offset], because we have
        // just incremented the offset, so we want to read the pointer `srcp1 + x` pointer before x
        // was incremented.
        Mem mem = mem_ptr(f->srcp1, f->x, 0, -offset);
#else
        // AArch64 addressing is more restricted than x86 one, so we can form either a [base + index]
        // or [base + offset] address.
        Gp srcBase = pc->newSimilarReg(f->srcp1, "srcBase");
        pc->sub(srcBase, f->srcp1, offset);
        Mem mem = mem_ptr(srcBase, f->x);
        // pc->add_ext(srcBase, f->srcp1, f->x, 1);
        // Mem mem = mem_ptr(srcBase);
#endif
        pc->j(L_Repeat, add_c(f->x, offset));

        if (p.isRGBA32()) {
          if (blTestFlag(flags, PixelFlags::kPC)) {
            const Vec& reg = p.pc[0];

            switch (format()) {
              case FormatExt::kPRGB32:
              case FormatExt::kXRGB32: {
                pc->v_loadu128(reg, mem);
                break;
              }

              case FormatExt::kA8: {
                pc->v_loadu32(reg, mem);
                pc->v_interleave_lo_u8(reg, reg, reg);
                pc->v_interleave_lo_u16(reg, reg, reg);
                break;
              }

              default:
                BL_NOT_REACHED();
            }
          }
          else {
            const Vec& uc0 = p.uc[0];
            const Vec& uc1 = p.uc[1];

            switch (format()) {
              case FormatExt::kPRGB32:
              case FormatExt::kXRGB32: {
                pc->v_cvt_u8_lo_to_u16(uc0, mem);
                pc->v_cvt_u8_lo_to_u16(uc1, mem.cloneAdjusted(8));
                break;
              }

              case FormatExt::kA8: {
                pc->v_loadu32(uc0, mem);
                pc->v_interleave_lo_u8(uc0, uc0, uc0);
                pc->v_cvt_u8_lo_to_u16(uc0, uc0);
                pc->v_swizzle_u32x4(uc1, uc0, swizzle(3, 3, 2, 2));
                pc->v_swizzle_u32x4(uc0, uc0, swizzle(1, 1, 0, 0));
                break;
              }

              default:
                BL_NOT_REACHED();
            }
          }
        }
        else {
          if (blTestFlag(flags, PixelFlags::kPA)) {
            const Vec& reg = p.pa[0];
            switch (format()) {
              case FormatExt::kPRGB32:
              case FormatExt::kXRGB32: {
                pc->v_loadu128(reg, mem);

#if defined(BL_JIT_ARCH_X86)
                if (!pc->hasSSSE3()) {
                  pc->v_srli_u32(reg, reg, 24);
                  pc->v_packs_i32_i16(reg, reg, reg);
                  pc->v_packs_i16_u8(reg, reg, reg);
                }
                else
#endif // BL_JIT_ARCH_X86
                {
                  pc->v_swizzlev_u8(reg, reg, pc->simdConst(&ct.pshufb_3xxx2xxx1xxx0xxx_to_zzzzzzzzzzzz3210, Bcst::kNA, reg));
                }
                break;
              }

              case FormatExt::kA8: {
                pc->v_loadu32(reg, mem);
                break;
              }

              default:
                BL_NOT_REACHED();
            }
          }
          else {
            const Vec& reg = p.ua[0];
            switch (format()) {
              case FormatExt::kPRGB32:
              case FormatExt::kXRGB32: {
                pc->v_loadu128(reg, mem);
                pc->v_srli_u32(reg, reg, 24);
                pc->v_packs_i32_i16(reg, reg, reg);
                break;
              }

              case FormatExt::kA8: {
                pc->v_loadu32(reg, mem);
                pc->v_cvt_u8_lo_to_u16(reg, reg);
                break;
              }

              default:
                BL_NOT_REACHED();
            }
          }
        }

        pc->bind(L_Done);

        {
          PipeInjectAtTheEnd injected(pc);
          pc->bind(L_Repeat);

          fCtx.fetchPixel(mem);
#if defined(BL_JIT_ARCH_X86)
          mem.addOffsetLo32(offset);
#else
          mem = mem_ptr(f->srcp1, f->x);
#endif

          pc->cmov(f->x, f->xRestart, sub_z(f->x, offset - int(bpp())));
          fCtx.fetchPixel(mem);

          pc->cmov(f->x, f->xRestart, add_z(f->x, bpp()));
          fCtx.fetchPixel(mem);

          pc->cmov(f->x, f->xRestart, add_z(f->x, bpp()));
          fCtx.fetchPixel(mem);

          pc->cmov(f->x, f->xRestart, add_z(f->x, bpp()));
          fCtx.end();

          pc->j(L_Done);
        }

        FetchUtils::x_satisfy_pixel(pc, p, flags);
      }
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

// bl::Pipeline::JIT::FetchAffinePatternPart - Construction & Destruction
// ======================================================================

FetchAffinePatternPart::FetchAffinePatternPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchPatternPart(pc, fetchType, format) {

  _partFlags |= PipePartFlags::kAdvanceXNeedsDiff;
  _maxPixels = 4;

  switch (fetchType) {
    case FetchType::kPatternAffineNNAny:
    case FetchType::kPatternAffineNNOpt:
      _isComplexFetch = true;
      break;

    case FetchType::kPatternAffineBIAny:
    case FetchType::kPatternAffineBIOpt:
      // TODO: [PIPEGEN] Implement fetch4.
      _maxPixels = 1;
      _isComplexFetch = true;
      break;

    default:
      BL_NOT_REACHED();
  }

  JitUtils::resetVarStruct(&f, sizeof(f));

  if (IntOps::isPowerOf2(_bpp))
    _idxShift = uint8_t(IntOps::ctz(_bpp));
}

// bl::Pipeline::JIT::FetchAffinePatternPart - Init & Fini
// =======================================================

void FetchAffinePatternPart::_initPart(const PipeFunction& fn, Gp& x, Gp& y) noexcept {
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  f->srctop         = pc->newGpPtr("f.srctop");       // Mem.
  f->stride         = pc->newGpPtr("f.stride");       // Mem.

  f->xx_xy          = pc->newV128("f.xx_xy");          // Reg.
  f->yx_yy          = pc->newV128("f.yx_yy");          // Reg/Mem.
  f->tx_ty          = pc->newV128("f.tx_ty");          // Reg/Mem.
  f->px_py          = pc->newV128("f.px_py");          // Reg.
  f->ox_oy          = pc->newV128("f.ox_oy");          // Reg/Mem.
  f->rx_ry          = pc->newV128("f.rx_ry");          // Reg/Mem.
  f->qx_qy          = pc->newV128("f.qx_qy");          // Reg     [fetch4].
  f->xx2_xy2        = pc->newV128("f.xx2_xy2");        // Reg/Mem [fetch4].
  f->minx_miny      = pc->newV128("f.minx_miny");      // Reg/Mem.
  f->maxx_maxy      = pc->newV128("f.maxx_maxy");      // Reg/Mem.
  f->corx_cory      = pc->newV128("f.corx_cory");      // Reg/Mem.
  f->tw_th          = pc->newV128("f.tw_th");          // Reg/Mem.

  f->vIdx           = pc->newV128("f.vIdx");           // Reg/Tmp.
  f->vAddrMul       = pc->newV128("f.vAddrMul");       // Reg/Tmp.
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  pc->load(f->srctop, mem_ptr(fn.fetchData(), REL_PATTERN(src.pixelData)));
  pc->load(f->stride, mem_ptr(fn.fetchData(), REL_PATTERN(src.stride)));

  pc->v_loadu128(f->xx_xy, mem_ptr(fn.fetchData(), REL_PATTERN(affine.xx)));
  pc->v_loadu128(f->yx_yy, mem_ptr(fn.fetchData(), REL_PATTERN(affine.yx)));

  pc->s_mov_u32(f->tx_ty, y);
  pc->v_swizzle_u32x4(f->tx_ty, f->tx_ty, swizzle(1, 0, 1, 0));
  pc->v_mul_u64_lo_u32(f->tx_ty, f->yx_yy, f->tx_ty);
  pc->v_add_i64(f->tx_ty, f->tx_ty, mem_ptr(fn.fetchData(), REL_PATTERN(affine.tx)));

  // RoR: `tw_th` and `rx_ry` are only used by repeated or reflected patterns.
  pc->v_loadu128(f->rx_ry, mem_ptr(fn.fetchData(), REL_PATTERN(affine.rx)));
  pc->v_loadu128(f->tw_th, mem_ptr(fn.fetchData(), REL_PATTERN(affine.tw)));

  pc->v_loadu128(f->ox_oy, mem_ptr(fn.fetchData(), REL_PATTERN(affine.ox)));
  pc->v_loadu128(f->xx2_xy2, mem_ptr(fn.fetchData(), REL_PATTERN(affine.xx2)));

  // Pad: [MaxY | MaxX | MinY | MinX]
  pc->v_loadu128(f->minx_miny, mem_ptr(fn.fetchData(), REL_PATTERN(affine.minX)));
  pc->v_loadu64(f->corx_cory, mem_ptr(fn.fetchData(), REL_PATTERN(affine.corX)));

  if (isOptimized()) {
    pc->v_packs_i32_i16(f->minx_miny, f->minx_miny, f->minx_miny);        // [MaxY|MaxX|MinY|MinX|MaxY|MaxX|MinY|MinX]
    pc->v_swizzle_u32x4(f->maxx_maxy, f->minx_miny, swizzle(1, 1, 1, 1)); // [MaxY|MaxX|MaxY|MaxX|MaxY|MaxX|MaxY|MaxX]
    pc->v_swizzle_u32x4(f->minx_miny, f->minx_miny, swizzle(0, 0, 0, 0)); // [MinY|MinX|MinY|MinX|MinY|MinX|MinY|MinX]
  }
  else if (fetchType() == FetchType::kPatternAffineNNAny) {
    // TODO: This is unfortunate that it's different compared to other cases.
    pc->v_swizzle_u32x4(f->maxx_maxy, f->minx_miny, swizzle(3, 2, 3, 2)); // [MaxY|MaxX|MaxY|MaxX]
    pc->v_swizzle_u32x4(f->minx_miny, f->minx_miny, swizzle(1, 0, 1, 0)); // [MinY|MinX|MinY|MinX]
    pc->v_swizzle_u32x4(f->corx_cory, f->corx_cory, swizzle(1, 0, 1, 0)); // [CorY|CorX|CorY|CorX]
  }
  else {
    pc->v_swizzle_u32x4(f->maxx_maxy, f->minx_miny, swizzle(3, 3, 2, 2)); // [MaxY|MaxY|MaxX|MaxX]
    pc->v_swizzle_u32x4(f->minx_miny, f->minx_miny, swizzle(1, 1, 0, 0)); // [MinY|MinY|MinX|MinX]
    pc->v_swizzle_u32x4(f->corx_cory, f->corx_cory, swizzle(1, 1, 0, 0)); // [CorY|CorY|CorX|CorX]
  }

  if (isOptimized())
    pc->v_broadcast_u32(f->vAddrMul, mem_ptr(fn.fetchData(), REL_PATTERN(affine.addrMul16)));
  else
    pc->v_broadcast_u64(f->vAddrMul, mem_ptr(fn.fetchData(), REL_PATTERN(affine.addrMul32)));

  if (isRectFill()) {
    advancePxPy(f->tx_ty, x);
    normalizePxPy(f->tx_ty);
  }
}

void FetchAffinePatternPart::_finiPart() noexcept {}

// bl::Pipeline::JIT::FetchAffinePatternPart - Advance
// ===================================================

void FetchAffinePatternPart::advanceY() noexcept {
  pc->v_add_i64(f->tx_ty, f->tx_ty, f->yx_yy);

  if (isRectFill())
    normalizePxPy(f->tx_ty);
}

void FetchAffinePatternPart::startAtX(const Gp& x) noexcept {
  if (isRectFill()) {
    pc->v_mov(f->px_py, f->tx_ty);
  }
  else {
    // Similar to `advancePxPy()`, however, we don't need a temporary here...
    pc->s_mov_u32(f->px_py, x.r32());
    pc->v_swizzle_u32x4(f->px_py, f->px_py, swizzle(1, 0, 1, 0));
    pc->v_mul_u64_lo_u32(f->px_py, f->xx_xy, f->px_py);
    pc->v_add_i64(f->px_py, f->px_py, f->tx_ty);

    normalizePxPy(f->px_py);
  }

  if (pixelGranularity() > 1)
    enterN();
}

void FetchAffinePatternPart::advanceX(const Gp& x, const Gp& diff) noexcept {
  blUnused(x);
  BL_ASSERT(!isRectFill());

  if (pixelGranularity() > 1)
    leaveN();

  advancePxPy(f->px_py, diff);
  normalizePxPy(f->px_py);

  if (pixelGranularity() > 1)
    enterN();
}

void FetchAffinePatternPart::advancePxPy(Vec& px_py, const Gp& i) noexcept {
  Vec t = pc->newV128("@t");

  pc->s_mov_u32(t, i.r32());
  pc->v_swizzle_u32x4(t, t, swizzle(1, 0, 1, 0));
  pc->v_mul_u64_lo_u32(t, f->xx_xy, t);
  pc->v_add_i64(px_py, px_py, t);
}

void FetchAffinePatternPart::normalizePxPy(Vec& px_py) noexcept {
  Vec v0 = pc->newV128("v0");

  pc->v_zero_i(v0);
  pc->xModI64HIxDouble(px_py, px_py, f->tw_th);
  pc->v_cmp_gt_i32(v0, v0, px_py);
  pc->v_and_i32(v0, v0, f->rx_ry);
  pc->v_add_i32(px_py, px_py, v0);

  pc->v_cmp_gt_i32(v0, px_py, f->ox_oy);
  pc->v_and_i32(v0, v0, f->rx_ry);
  pc->v_sub_i32(px_py, px_py, v0);
}

void FetchAffinePatternPart::clampVIdx32(Vec& dst, const Vec& src, ClampStep step) noexcept {
  switch (step) {
    // Step A - Handle a possible underflow (PAD).
    //
    // We know that `minx_miny` can contain these values (per vector element):
    //
    //   a) `minx_miny == 0`         to handle PAD case.
    //   b) `minx_miny == INT32_MIN` to handle REPEAT & REFLECT cases.
    //
    // This means that we either clamp to zero if `src` is negative and `minx_miny == 0`
    // or we don't clamp at all in case that `minx_miny == INT32_MIN`. This means that
    // we don't need a pure `PMAXSD` replacement in pure SSE2 mode, just something that
    // works for the mentioned cases.
    case kClampStepA_NN:
    case kClampStepA_BI: {
#if defined(BL_JIT_ARCH_X86)
      if (!pc->hasSSE4_1()) {
        if (dst.id() == src.id()) {
          Vec tmp = pc->newV128("f.vIdxPad");
          pc->v_mov(tmp, dst);
          pc->v_cmp_gt_i32(dst, dst, f->minx_miny); // `-1` if `src` is greater than `minx_miny`.
          pc->v_and_i32(dst, dst, tmp);             // Changes `dst` to `0` if clamped.
        }
        else {
          pc->v_mov(dst, src);
          pc->v_cmp_gt_i32(dst, dst, f->minx_miny); // `-1` if `src` is greater than `minx_miny`.
          pc->v_and_i32(dst, dst, src);             // Changes `dst` to `0` if clamped.
        }
        break;
      }
#endif // BL_JIT_ARCH_X86

      pc->v_max_i32(dst, src, f->minx_miny);
      break;
    }

    // Step B - Handle a possible overflow (PAD | Bilinear overflow).
    case kClampStepB_NN:
    case kClampStepB_BI: {
      // Always performed on the same register.
      BL_ASSERT(dst.id() == src.id());

#if defined(BL_JIT_ARCH_X86)
      /* TODO: SEEMS SLOWER
      if (pc->hasAVX512()) {
        x86::KReg k = cc->newKw("f.kTmp");
        cc->vpcmpgtd(k, dst, f->maxx_maxy);
        cc->k(k).vmovdqa32(dst, f->corx_cory);
      }
      */

      if (!pc->hasSSE4_1()) {
        // Blend(a, b, cond) == a ^ ((a ^ b) &  cond)
        //                   == b ^ ((a ^ b) & ~cond)
        Vec tmp = pc->newV128("f.vTmp");
        pc->v_xor_i32(tmp, dst, f->corx_cory);
        pc->v_cmp_gt_i32(dst, dst, f->maxx_maxy);
        pc->v_bic_i32(dst, tmp, dst);
        pc->v_xor_i32(dst, dst, f->corx_cory);
        break;
      }
#endif // BL_JIT_ARCH_X86

      Vec tmp = pc->newV128("f.vTmp");
      pc->v_cmp_gt_i32(tmp, dst, f->maxx_maxy);
      pc->v_blendv_u8(dst, dst, f->corx_cory, tmp);
      break;
    }

    // Step C - Handle a possible reflection (RoR).
    case kClampStepC_NN:
    case kClampStepC_BI: {
      // Always performed on the same register.
      BL_ASSERT(dst.id() == src.id());

      Vec tmp = pc->newV128("f.vIdxRoR");
      pc->v_srai_i32(tmp, dst, 31);
      pc->v_xor_i32(dst, dst, tmp);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::JIT::FetchAffinePatternPart - Fetch
// =================================================

void FetchAffinePatternPart::prefetch1() noexcept {
  Vec vIdx = f->vIdx;

  switch (fetchType()) {
    case FetchType::kPatternAffineNNAny: {
      clampVIdx32(vIdx, f->px_py, kClampStepA_NN);
      clampVIdx32(vIdx, vIdx    , kClampStepB_NN);
      break;
    }

    case FetchType::kPatternAffineNNOpt: {
      pc->v_swizzle_u32x4(vIdx, f->px_py, swizzle(3, 1, 3, 1));
      pc->v_packs_i32_i16(vIdx, vIdx, vIdx);
      pc->v_max_i16(vIdx, vIdx, f->minx_miny);
      pc->v_min_i16(vIdx, vIdx, f->maxx_maxy);
      break;
    }

    default:
      break;
  }
}

void FetchAffinePatternPart::enterN() noexcept {
  Vec vMsk0 = pc->newV128("vMsk0");

  pc->v_add_i64(f->qx_qy, f->px_py, f->xx_xy);
  pc->v_cmp_gt_i32(vMsk0, f->qx_qy, f->ox_oy);
  pc->v_and_i32(vMsk0, vMsk0, f->rx_ry);
  pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk0);
}

void FetchAffinePatternPart::leaveN() noexcept {}

void FetchAffinePatternPart::prefetchN() noexcept {
  switch (fetchType()) {
    case FetchType::kPatternAffineNNOpt: {
      Vec vIdx = f->vIdx;
      Vec vMsk0 = pc->newV128("vMsk0");
      Vec vMsk1 = pc->newV128("vMsk1");

      pc->v_interleave_shuffle_u32x4(vIdx, f->px_py, f->qx_qy, swizzle(3, 1, 3, 1));
      pc->v_add_i64(f->px_py, f->px_py, f->xx2_xy2);
      pc->v_add_i64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      pc->v_cmp_gt_i32(vMsk0, f->px_py, f->ox_oy);
      pc->v_cmp_gt_i32(vMsk1, f->qx_qy, f->ox_oy);

      pc->v_and_i32(vMsk0, vMsk0, f->rx_ry);
      pc->v_and_i32(vMsk1, vMsk1, f->rx_ry);

      pc->v_sub_i32(f->px_py, f->px_py, vMsk0);
      pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk1);

      pc->v_interleave_shuffle_u32x4(vMsk0, f->px_py, f->qx_qy, swizzle(3, 1, 3, 1));
      pc->v_packs_i32_i16(vIdx, vIdx, vMsk0);

      pc->v_max_i16(vIdx, vIdx, f->minx_miny);
      pc->v_min_i16(vIdx, vIdx, f->maxx_maxy);

      pc->v_srai_i16(vMsk0, vIdx, 15);
      pc->v_xor_i32(vIdx, vIdx, vMsk0);
      break;
    }

    default:
      break;
  }
}

void FetchAffinePatternPart::postfetchN() noexcept {
}

void FetchAffinePatternPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  BL_ASSERT(predicate.empty());
  blUnused(predicate);

  p.setCount(n);

  switch (n.value()) {
    case 1: {
      switch (fetchType()) {
        case FetchType::kPatternAffineNNAny: {
          Gp texPtr = pc->newGpPtr("texPtr");
          Gp texOff = pc->newGpPtr("texOff");

          Vec vIdx = f->vIdx;
          Vec vMsk = pc->newV128("vMsk");

          clampVIdx32(vIdx, vIdx, kClampStepC_NN);
          pc->v_add_i64(f->px_py, f->px_py, f->xx_xy);

          FetchUtils::IndexExtractor iExt(pc);
          iExt.begin(FetchUtils::IndexExtractor::kTypeUInt32, vIdx);
          iExt.extract(texPtr, 3);
          iExt.extract(texOff, 1);

          pc->v_cmp_gt_i32(vMsk, f->px_py, f->ox_oy);
          pc->mul(texPtr, texPtr, f->stride);
          pc->v_and_i32(vMsk, vMsk, f->rx_ry);
          pc->v_sub_i32(f->px_py, f->px_py, vMsk);

          pc->add(texPtr, texPtr, f->srctop);
          FetchUtils::x_fetch_pixel(pc, p, PixelCount(1), flags, format(), mem_ptr(texPtr, texOff, _idxShift), Alignment(4));
          clampVIdx32(vIdx, f->px_py, kClampStepA_NN);

          FetchUtils::x_satisfy_pixel(pc, p, flags);
          clampVIdx32(vIdx, vIdx, kClampStepB_NN);
          break;
        }

        case FetchType::kPatternAffineNNOpt: {
          Gp texPtr = pc->newGpPtr("texPtr");
          Vec vIdx = f->vIdx;
          Vec vMsk = pc->newV128("vMsk");

          pc->v_srai_i16(vMsk, vIdx, 15);
          pc->v_xor_i32(vIdx, vIdx, vMsk);

          pc->v_add_i64(f->px_py, f->px_py, f->xx_xy);
          pc->v_mhadd_i16_to_i32(vIdx, vIdx, f->vAddrMul);

          pc->v_cmp_gt_i32(vMsk, f->px_py, f->ox_oy);
          pc->v_and_i32(vMsk, vMsk, f->rx_ry);
          pc->v_sub_i32(f->px_py, f->px_py, vMsk);
          pc->s_mov_u32(texPtr.r32(), vIdx);

          pc->v_swizzle_u32x4(vIdx, f->px_py, swizzle(3, 1, 3, 1));
          pc->v_packs_i32_i16(vIdx, vIdx, vIdx);

          pc->add(texPtr, texPtr, f->srctop);
          pc->v_max_i16(vIdx, vIdx, f->minx_miny);
          FetchUtils::x_fetch_pixel(pc, p, PixelCount(1), flags, format(), mem_ptr(texPtr), Alignment(4));

          pc->v_min_i16(vIdx, vIdx, f->maxx_maxy);
          FetchUtils::x_satisfy_pixel(pc, p, flags);
          break;
        }

        case FetchType::kPatternAffineBIAny: {
          if (isAlphaFetch()) {
            Vec vIdx = pc->newV128("vIdx");
            Vec vMsk = pc->newV128("vMsk");
            Vec vWeights = pc->newV128("vWeights");

            pc->v_swizzle_u32x4(vIdx, f->px_py, swizzle(3, 3, 1, 1));
            pc->v_sub_i32(vIdx, vIdx, pc->simdConst(&ct.i_FFFFFFFF00000000, Bcst::kNA, vIdx));

            pc->v_swizzle_lo_u16x4(vWeights, f->px_py, swizzle(1, 1, 1, 1));
            clampVIdx32(vIdx, vIdx, kClampStepA_BI);

            pc->v_add_i64(f->px_py, f->px_py, f->xx_xy);
            clampVIdx32(vIdx, vIdx, kClampStepB_BI);

            pc->v_cmp_gt_i32(vMsk, f->px_py, f->ox_oy);
            pc->v_swizzle_hi_u16x4(vWeights, vWeights, swizzle(1, 1, 1, 1));

            pc->v_and_i32(vMsk, vMsk, f->rx_ry);
            pc->v_srli_u16(vWeights, vWeights, 8);

            pc->v_sub_i32(f->px_py, f->px_py, vMsk);
            pc->v_xor_i32(vWeights, vWeights, pc->simdConst(&ct.i_FFFF0000FFFF0000, Bcst::k32, vWeights));

            clampVIdx32(vIdx, vIdx, kClampStepC_BI);
            pc->v_add_i16(vWeights, vWeights, pc->simdConst(&ct.i_0101000001010000, Bcst::kNA, vWeights));

            Vec pixA = pc->newV128("pixA");
            FetchUtils::xFilterBilinearA8_1x(pc, pixA, f->srctop, f->stride, format(), _idxShift, vIdx, vWeights);

            FetchUtils::x_assign_unpacked_alpha_values(pc, p, flags, pixA);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
          else if (p.isRGBA32()) {
            Vec vIdx = pc->newV128("vIdx");
            Vec vMsk = pc->newV128("vMsk");
            Vec vWeights = pc->newV128("vWeights");

            pc->v_swizzle_u32x4(vIdx, f->px_py, swizzle(3, 3, 1, 1));
            pc->v_sub_i32(vIdx, vIdx, pc->simdConst(&ct.i_FFFFFFFF00000000, Bcst::kNA, vIdx));

            pc->v_swizzle_lo_u16x4(vWeights, f->px_py, swizzle(1, 1, 1, 1));
            clampVIdx32(vIdx, vIdx, kClampStepA_BI);

            pc->v_add_i64(f->px_py, f->px_py, f->xx_xy);
            clampVIdx32(vIdx, vIdx, kClampStepB_BI);

            pc->v_cmp_gt_i32(vMsk, f->px_py, f->ox_oy);
            pc->v_swizzle_hi_u16x4(vWeights, vWeights, swizzle(1, 1, 1, 1));

            pc->v_and_i32(vMsk, vMsk, f->rx_ry);
            pc->v_srli_u16(vWeights, vWeights, 8);

            pc->v_sub_i32(f->px_py, f->px_py, vMsk);
            pc->v_xor_i64(vWeights, vWeights, pc->simdConst(&ct.i_FFFFFFFF00000000, Bcst::k64, vWeights));

            clampVIdx32(vIdx, vIdx, kClampStepC_BI);
            pc->v_add_i16(vWeights, vWeights, pc->simdConst(&ct.i_0101010100000000, Bcst::kNA, vWeights));

            p.uc.init(pc->newV128("pix0"));
            FetchUtils::xFilterBilinearARGB32_1x(pc, p.uc[0], f->srctop, f->stride, vIdx, vWeights);
            FetchUtils::x_satisfy_pixel(pc, p, flags);
          }
          break;
        }

        case FetchType::kPatternAffineBIOpt: {
          // TODO: [PIPEGEN] Not implemented, not used for now...
          break;
        }

        default:
          BL_NOT_REACHED();
      }

      break;
    }

    case 4: {
      switch (fetchType()) {
        case FetchType::kPatternAffineNNAny: {
          FetchUtils::FetchContext fCtx(pc, &p, PixelCount(4), format(), flags);
          FetchUtils::IndexExtractor iExt(pc);

          Gp texPtr0 = pc->newGpPtr("texPtr0");
          Gp texOff0 = pc->newGpPtr("texOff0");
          Gp texPtr1 = pc->newGpPtr("texPtr1");
          Gp texOff1 = pc->newGpPtr("texOff1");

          Vec vIdx0 = pc->newV128("vIdx0");
          Vec vIdx1 = pc->newV128("vIdx1");
          Vec vMsk0 = pc->newV128("vMsk0");
          Vec vMsk1 = pc->newV128("vMsk1");

          pc->v_interleave_shuffle_u32x4(vIdx0, f->px_py, f->qx_qy, swizzle(3, 1, 3, 1));
          pc->v_add_i64(f->px_py, f->px_py, f->xx2_xy2);

          clampVIdx32(vIdx0, vIdx0, kClampStepA_NN);
          pc->v_add_i64(f->qx_qy, f->qx_qy, f->xx2_xy2);

          clampVIdx32(vIdx0, vIdx0, kClampStepB_NN);
          pc->v_cmp_gt_i32(vMsk0, f->px_py, f->ox_oy);
          clampVIdx32(vIdx0, vIdx0, kClampStepC_NN);

          pc->v_cmp_gt_i32(vMsk1, f->qx_qy, f->ox_oy);
          pc->v_and_i32(vMsk0, vMsk0, f->rx_ry);
          pc->v_and_i32(vMsk1, vMsk1, f->rx_ry);
          pc->v_sub_i32(f->px_py, f->px_py, vMsk0);
          pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk1);

          iExt.begin(FetchUtils::IndexExtractor::kTypeUInt32, vIdx0);
          pc->v_interleave_shuffle_u32x4(vIdx1, f->px_py, f->qx_qy, swizzle(3, 1, 3, 1));
          iExt.extract(texPtr0, 1);
          iExt.extract(texOff0, 0);

          clampVIdx32(vIdx1, vIdx1, kClampStepA_NN);
          clampVIdx32(vIdx1, vIdx1, kClampStepB_NN);

          iExt.extract(texPtr1, 3);
          iExt.extract(texOff1, 2);

          pc->mul(texPtr0, texPtr0, f->stride);
          pc->mul(texPtr1, texPtr1, f->stride);

          clampVIdx32(vIdx1, vIdx1, kClampStepC_NN);
          pc->v_add_i64(f->px_py, f->px_py, f->xx2_xy2);
          pc->v_add_i64(f->qx_qy, f->qx_qy, f->xx2_xy2);

          pc->add(texPtr0, texPtr0, f->srctop);
          pc->add(texPtr1, texPtr1, f->srctop);
          iExt.begin(FetchUtils::IndexExtractor::kTypeUInt32, vIdx1);

          fCtx.fetchPixel(mem_ptr(texPtr0, texOff0, _idxShift));
          iExt.extract(texPtr0, 1);
          iExt.extract(texOff0, 0);

          pc->v_cmp_gt_i32(vMsk0, f->px_py, f->ox_oy);
          pc->v_cmp_gt_i32(vMsk1, f->qx_qy, f->ox_oy);

          fCtx.fetchPixel(mem_ptr(texPtr1, texOff1, _idxShift));
          iExt.extract(texPtr1, 3);
          iExt.extract(texOff1, 2);
          pc->mul(texPtr0, texPtr0, f->stride);

          pc->v_and_i32(vMsk0, vMsk0, f->rx_ry);
          pc->v_and_i32(vMsk1, vMsk1, f->rx_ry);

          pc->mul(texPtr1, texPtr1, f->stride);
          pc->v_sub_i32(f->px_py, f->px_py, vMsk0);

          pc->add(texPtr0, texPtr0, f->srctop);
          pc->add(texPtr1, texPtr1, f->srctop);
          fCtx.fetchPixel(mem_ptr(texPtr0, texOff0, _idxShift));

          pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk1);
          fCtx.fetchPixel(mem_ptr(texPtr1, texOff1, _idxShift));
          fCtx.end();

          FetchUtils::x_satisfy_pixel(pc, p, flags);
          break;
        }

        case FetchType::kPatternAffineNNOpt: {
          Vec vIdx = f->vIdx;
          Vec vIdxFinal = pc->newV128("vIdxFinal");
          Vec vMsk0 = pc->newV128("vMsk0");
          Vec vMsk1 = pc->newV128("vMsk1");

          pc->v_mhadd_i16_to_i32(vIdxFinal, vIdx, f->vAddrMul);
          FetchUtils::gatherPixels(pc, p, PixelCount(4), format(), flags, mem_ptr(f->srctop), vIdxFinal, 0, FetchUtils::IndexLayout::kUInt32, [&](uint32_t step) noexcept {
            switch (step) {
              case 0:
                pc->v_add_i64(f->px_py, f->px_py, f->xx2_xy2);
                pc->v_add_i64(f->qx_qy, f->qx_qy, f->xx2_xy2);
                pc->v_cmp_gt_i32(vMsk0, f->px_py, f->ox_oy);
                pc->v_cmp_gt_i32(vMsk1, f->qx_qy, f->ox_oy);
                pc->v_and_i32(vMsk0, vMsk0, f->rx_ry);
                break;
              case 1:
                pc->v_and_i32(vMsk1, vMsk1, f->rx_ry);
                pc->v_sub_i32(f->px_py, f->px_py, vMsk0);
                pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk1);
                pc->v_interleave_shuffle_u32x4(vIdx, f->px_py, f->qx_qy, swizzle(3, 1, 3, 1));
                pc->v_add_i64(f->px_py, f->px_py, f->xx2_xy2);
                pc->v_add_i64(f->qx_qy, f->qx_qy, f->xx2_xy2);
                break;
              case 2:
                pc->v_cmp_gt_i32(vMsk0, f->px_py, f->ox_oy);
                pc->v_cmp_gt_i32(vMsk1, f->qx_qy, f->ox_oy);
                pc->v_and_i32(vMsk0, vMsk0, f->rx_ry);
                pc->v_and_i32(vMsk1, vMsk1, f->rx_ry);
                break;
              case 3:
                pc->v_sub_i32(f->px_py, f->px_py, vMsk0);
                pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk1);
                pc->v_interleave_shuffle_u32x4(vMsk0, f->px_py, f->qx_qy, swizzle(3, 1, 3, 1));
                break;
              case 0xFF:
                pc->v_packs_i32_i16(vIdx, vIdx, vMsk0);
                pc->v_max_i16(vIdx, vIdx, f->minx_miny);
                break;
            }
          });

          pc->v_min_i16(vIdx, vIdx, f->maxx_maxy);
          pc->v_srai_i16(vMsk0, vIdx, 15);
          pc->v_xor_i32(vIdx, vIdx, vMsk0);
          FetchUtils::x_satisfy_pixel(pc, p, flags);
          break;
        }

        default:
          BL_NOT_REACHED();
      }

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
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
