// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchpatternpart_p.h"
#include "../../pipeline/jit/fetchutils_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipedebug_p.h"
#include "../../support/intops_p.h"

namespace BLPipeline {
namespace JIT {

#define REL_PATTERN(FIELD) BL_OFFSET_OF(FetchData::Pattern, FIELD)

// BLPipeline::JIT::FetchPatternPart - Construction & Destruction
// ==============================================================

FetchPatternPart::FetchPatternPart(PipeCompiler* pc, FetchType fetchType, uint32_t format) noexcept
  : FetchPart(pc, fetchType, format) {}

// BLPipeline::JIT::FetchSimplePatternPart - Construction & Destruction
// ====================================================================

FetchSimplePatternPart::FetchSimplePatternPart(PipeCompiler* pc, FetchType fetchType, uint32_t format) noexcept
  : FetchPatternPart(pc, fetchType, format) {

  _idxShift = 0;
  _maxPixels = 4;

  static const ExtendMode aExtendTable[] = { ExtendMode::kPad, ExtendMode::kRepeat, ExtendMode::kRoR };
  static const ExtendMode uExtendTable[] = { ExtendMode::kPad, ExtendMode::kRoR };

  // Setup persistent and temporary registers, extend mode, and the maximum
  // number of pixels that can be fetched at once.
  switch (fetchType) {
    case FetchType::kPatternAlignedBlit:
      _maxPixels = 8;
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
    if (BLIntOps::isPowerOf2(_bpp))
      _idxShift = uint8_t(BLIntOps::ctz(_bpp));
  }

  JitUtils::resetVarStruct(&f, sizeof(f));
}

// BLPipeline::JIT::FetchSimplePatternPart - Init & Fini
// =====================================================

void FetchSimplePatternPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  if (isAlignedBlit()) {
    // This is a special-case designed only for rectangular blits that never
    // go out of image bounds (this implies that no extend mode is applied).
    BL_ASSERT(isRectFill());

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    f->stride       = cc->newIntPtr("f.stride");      // Mem.
    f->srcp1        = cc->newIntPtr("f.srcp1");       // Reg.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    cc->mov(f->srcp1.r32(), y.r32());
    cc->sub(f->srcp1.r32(), x86::ptr(pc->_fetchData, REL_PATTERN(simple.ty)));

    cc->imul(f->srcp1, x86::ptr(pc->_fetchData, REL_PATTERN(src.stride)));
    cc->mov(f->stride.r32(), x86::ptr(pc->_fetchData, REL_PATTERN(src.size.w)));

    cc->add(f->srcp1, x86::ptr(pc->_fetchData, REL_PATTERN(src.pixelData)));
    pc->uPrefetch(x86::ptr(f->srcp1));

    pc->uMul(f->stride, f->stride, -int(bpp()));
    cc->add(f->stride, x86::ptr(pc->_fetchData, REL_PATTERN(src.stride)));
  }
  else {
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    f->w            = cc->newInt32("f.w");            // Mem.
    f->h            = cc->newInt32("f.h");            // Mem.
    f->srctop       = cc->newIntPtr("f.srctop");      // Mem.
    f->stride       = cc->newIntPtr("f.stride");      // Mem.
    f->strideOrig   = cc->newIntPtr("f.strideOrig");  // Mem.
    f->srcp0        = cc->newIntPtr("f.srcp0");       // Reg.
    f->srcp1        = cc->newIntPtr("f.srcp1");       // Reg (Fy|FxFy).
    f->y            = cc->newInt32("f.y");            // Reg.
    f->ry           = cc->newInt32("f.ry");           // Mem.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    cc->mov(f->y, x86::ptr(pc->_fetchData, REL_PATTERN(simple.ty)));
    cc->add(f->y, y);

    cc->mov(f->srctop, x86::ptr(pc->_fetchData, REL_PATTERN(src.pixelData)));
    cc->mov(f->stride, x86::ptr(pc->_fetchData, REL_PATTERN(src.stride)));
    cc->mov(f->strideOrig, f->stride);

    cc->mov(f->h , x86::ptr(pc->_fetchData, REL_PATTERN(src.size.h)));
    cc->mov(f->ry, x86::ptr(pc->_fetchData, REL_PATTERN(simple.ry)));

    // Vertical Extend
    // ===============
    //
    // Vertical extend modes are not hardcoded in the generated pipeline to
    // decrease the number of possible pipeline combinations. This means that
    // the compiled pipeline supports all vertical extend modes. The amount of
    // code that handles vertical extend modes is minimal and runtime overhead
    // during `advanceY()` was minimized. There should be no performance penalty
    // for this decision.

    Label L_VertPadA    = cc->newLabel();
    Label L_VertPadB    = cc->newLabel();
    Label L_VertRoR     = cc->newLabel();
    Label L_VertReflect = cc->newLabel();
    Label L_VertDone    = cc->newLabel();

    // if (ry == 0) {
    //   {Vectical-Pad}
    // }
    // else {
    //   {Vectical-Repeat|Reflect}
    // }
    cc->test(f->ry, f->ry);
    cc->jnz(L_VertRoR);

    // Vertical Pad
    // ------------
    //
    // `f->y` represents a counter that contains how many scanlines we can iterate
    // simply by decreasing `f->y` and adding `f->stride` to `f->srcp1`. When `f->y`
    // becomes zero `f->stride` can no longer be added to `f->srcp1` and the counter
    // needs to be recalculated.
    //
    // There are in general 3 cases:
    //
    //   A. `f->srcp1`  - Points to the first scanline.
    //      `f->stride` - Always zero.
    //      `f->y`      - Counts how many scanlines have to be PADded.
    //
    //      When `f->y` decrements to zero we move to case B:
    //
    //   B. `f->srcp`   - Points to a valid scanline from 0 to `f->h-1`.
    //      `f->stride` - Real stride copied from the pattern data.
    //      `f->y`      - How many scanlines until we reach the bottom of the pattern.
    //
    //      When `f->y` decrements to zero we move to case C:
    //
    //   C. `f->srcp`   - Points to the last scanline.
    //      `f->stride` - Always zero.
    //      `f->y`      - Zero or negative.
    //
    //      When we move to case C `f->y` is set to zero. This means that it will never
    //      fulfill the `f->y==0` condition again, as it's always decremented and then
    //      checked. This also means `f->y` starts at zero and then just decreases and
    //      stays negative.

    cc->dec(f->h);
    pc->uBound0ToN(f->srcp1.r32(), f->y, f->h);        // f->srcp1 = bound(f->y, 0, f->h - 1) * stride;
    cc->inc(f->h);

    cc->imul(f->srcp1, f->stride);                     // f->srcp1 *= f->stride;
    cc->cmp(f->y, f->h);
    cc->short_().jbe(L_VertPadB);                      // if (f->y < 0 || f->y >= h) {
    cc->short_().jl(L_VertPadA);                       //   if (f->y >= f->h) {
    cc->xor_(f->y, f->y);                              //     `f->y` = 0;
    cc->bind(L_VertPadA);                              //   }
    cc->neg(f->y);                                     //   f->y = -f->y; (like abs(f->y) or keeps it zero).
    cc->mov(f->stride, 0);                             //   f->stride = 0;
    cc->jmp(L_VertDone);                               // }

    cc->bind(L_VertPadB);                              // else {
    cc->sub(f->y, f->h);                               //   ...
    cc->neg(f->y);                                     //   f->y = f->h - f->y;
    cc->jmp(L_VertDone);                               // }

    // Vertical Repeat or Reflect
    // --------------------------

    cc->bind(L_VertRoR);
    pc->uMod(f->y, f->ry);                             // f->y %= f->ry;
    cc->mov(f->srcp1, f->stride);                      // f->srcp1 = f->stride;

    cc->cmp(f->y, f->h);
    cc->short_().jnb(L_VertReflect);                   // if (f->y < f->h) {
    cc->imul(f->srcp1, f->y.cloneAs(f->srcp1));        //   f->srcp1 *= intptr_t(f->y);
    cc->sub(f->y, f->h);                               //   ...
    cc->neg(f->y);                                     //   f->y = f->h - f->y;
    cc->jmp(L_VertDone);                               // }

    cc->bind(L_VertReflect);                           // else {
    cc->not_(f->y);                                    //   ...
    cc->add(f->y, f->ry);                              //   f->y = f->ry - f->y - 1;
    cc->imul(f->srcp1, f->y.cloneAs(f->srcp1));        //   f->srcp *= f->y;
    cc->inc(f->y);                                     //   f->y++;
    cc->neg(f->stride);                                //   f->stride = -f->stride;
    cc->bind(L_VertDone);                              // }

    cc->add(f->srcp1, f->srctop);

    // Horizontal Extend
    // =================
    //
    // Hozizontal extend modes are hardcoded for performance reasons. Every
    // extend mode requires different strategy to make horizontal advancing
    // as fast as possible.

    // Horizontal Pad
    // --------------
    //
    // There is not much to invent to clamp horizontally. The `f->x` is a raw
    // coordinate that is clamped each time it's used as an index. To make it
    // fast we use two variables `x` and `xPadded`, which always contains `x`
    // clamped to `[x, w]` range. The advantage of this approach is that every
    // time we increment `1` to `x` we need only 2 instructions to calculate
    // new `xPadded` value as it was already padded to the previous index, and
    // it could only get greater by `1` or stay where it was in a case we already
    // reached the width `w`.
    if (extendX() == ExtendMode::kPad) {
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      f->x          = cc->newInt32("f.x");            // Reg.
      f->xPadded    = cc->newIntPtr("f.xPadded");     // Reg.
      f->xOrigin    = cc->newInt32("f.xOrigin");      // Mem.
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      cc->mov(f->w      , x86::ptr(pc->_fetchData, REL_PATTERN(src.size.w)));
      cc->mov(f->xOrigin, x86::ptr(pc->_fetchData, REL_PATTERN(simple.tx)));

      if (isRectFill())
        cc->add(f->xOrigin, x);

      cc->dec(f->w);
    }

    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------
    //
    // This extend mode is only used to blit patterns that are tiled and that
    // exceed some predefined width-limit (like 16|32|etc). It's specialized
    // for larger patterns because it contains a condition in fetchN() that
    // jumps if `f->x` is at the end (or near it) of the pattern. That's why
    // the pattern width should be large enough that this branch is not
    // mispredicted often. For smaller patterns RoR more is more suitable as
    // there is no branch required and the repeat|reflect is handled by SIMD
    // instructions.
    //
    // This implementation generally uses two tricks to make the tiling faster:
    //
    //   1. It changes row indexing from [0..width) to [-width..0). The reason
    //      for such change is that when ADD instruction is executed it updates
    //      processor FLAGS register, if SIGN flag is zero it means that repeat
    //      is needed. This saves us one condition.
    //
    //   2. It multiplies X coordinates (all of them) by pattern's BPP (bytes
    //      per pixel). The reason is to completely eliminate `index * scale`
    //      in memory addressing (and in case of weird BPP to eliminate IMUL).

    if (extendX() == ExtendMode::kRepeat) {
      // NOTE: These all must be `intptr_t` because of memory indexing and the
      // use of the sign (when f->x is used as an index it's always negative).
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      f->x          = cc->newIntPtr("f.x");           // Reg.
      f->xOrigin    = cc->newIntPtr("f.xOrigin");     // Mem.
      f->xRestart   = cc->newIntPtr("f.xRestart");    // Mem.
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      cc->mov(f->w            , x86::ptr(pc->_fetchData, REL_PATTERN(src.size.w)));
      cc->mov(f->xOrigin.r32(), x86::ptr(pc->_fetchData, REL_PATTERN(simple.tx)));

      if (isRectFill()) {
        cc->add(f->xOrigin.r32(), x);
        pc->uMod(f->xOrigin.r32(), f->w);
      }

      pc->uMul(f->w      , f->w      , int(bpp()));
      pc->uMul(f->xOrigin, f->xOrigin, int(bpp()));

      cc->sub(f->xOrigin, f->w.cloneAs(f->xOrigin));
      cc->add(f->srcp1, f->w.cloneAs(f->srcp1));
      cc->add(f->srctop, f->w.cloneAs(f->srctop));

      cc->mov(f->xRestart.r32(), f->w);
      cc->neg(f->xRestart);
    }

    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------
    //
    // This mode handles both Repeat and Reflect cases. It uses the following
    // formula to either REPEAT or REFLECT X coordinate:
    //
    //   int index = (x >> 31) ^ x;
    //
    // The beauty of this method is that if X is negative it reflects, if it's
    // positive it's kept as is. Then the implementation handles both modes the
    // following way:
    //
    //   1. REPEAT - X is always bound to interval [0...Width), so when the
    //      index is calculated it never reflects. When `f->x` reaches the
    //      pattern width it's simply corrected as `f->x -= f->rx`, where `f->rx`
    //      is equal to `pattern.size.w`.
    //
    //   2. REFLECT - X is always bound to interval [-Width...Width) so it
    //      can reflect. When `f->x` reaches the pattern width it's simply
    //      corrected as `f->x -= f->rx`, where `f->rx` is equal to
    //      `pattern.size.w * 2` so it goes negative.

    if (extendX() == ExtendMode::kRoR) {
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      f->x          = cc->newInt32("f.x");            // Reg.
      f->xOrigin    = cc->newInt32("f.xOrigin");      // Mem.
      f->xRestart   = cc->newInt32("f.xRestart");     // Mem.
      f->rx         = cc->newInt32("f.rx");           // Mem.

      if (maxPixels() >= 4) {
        f->xVec4    = cc->newXmm("f.xVec4");          // Reg (fetchN).
        f->xSet4    = cc->newXmm("f.xSet4");          // Mem (fetchN).
        f->xInc4    = cc->newXmm("f.xInc4");          // Mem (fetchN).
        f->xNrm4    = cc->newXmm("f.xNrm4");          // Mem (fetchN).
        f->xMax4    = cc->newXmm("f.xMax4");          // Mem (fetchN).
      }
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      cc->mov(f->w , x86::ptr(pc->_fetchData, REL_PATTERN(src.size.w)));
      cc->mov(f->rx, x86::ptr(pc->_fetchData, REL_PATTERN(simple.rx)));

      if (maxPixels() >= 4) {
        cc->dec(f->w);
        pc->v_broadcast_u32(f->xMax4, f->w);
        cc->inc(f->w);

        pc->vmovu8u32(f->xSet4, x86::ptr(pc->_fetchData, REL_PATTERN(simple.ix)));
        pc->v_swizzle_i32(f->xInc4, f->xSet4, x86::shuffleImm(3, 3, 3, 3));
        pc->v_sllb_i128(f->xSet4, f->xSet4, 4);
      }

      cc->mov(f->xRestart, f->w);
      cc->sub(f->xRestart, f->rx);

      if (maxPixels() >= 4) {
        pc->v_broadcast_u32(f->xNrm4, f->rx);
      }

      cc->mov(f->xOrigin, x86::ptr(pc->_fetchData, REL_PATTERN(simple.tx)));

      if (isRectFill()) {
        x86::Gp norm = cc->newInt32("@norm");

        cc->add(f->xOrigin, x);
        pc->uMod(f->xOrigin, f->rx);

        cc->xor_(norm, norm);
        cc->cmp(f->xOrigin, f->w);
        cc->cmovae(norm, f->rx);
        cc->sub(f->xOrigin, norm);
      }
    }

    // Fractional - Fx|Fy|FxFy
    // =======================

    if (isPatternUnaligned()) {
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      f->pixL       = cc->newXmm("f.pixL");           // Reg (Fx|FxFy).

      f->wb_wb      = cc->newXmm("f.wb_wb");          // Mem (RGBA mode).
      f->wd_wd      = cc->newXmm("f.wd_wd");          // Mem (RGBA mode).
      f->wc_wd      = cc->newXmm("f.wc_wd");          // Mem (RGBA mode).
      f->wa_wb      = cc->newXmm("f.wa_wb");          // Mem (RGBA mode).

      f->wd_wb      = cc->newXmm("f.wd_wb");          // Mem (Alpha mode).
      f->wa_wc      = cc->newXmm("f.wa_wc");          // Mem (Alpha mode).
      f->wb_wd      = cc->newXmm("f.wb_wd");          // Mem (Alpha mode).
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      x86::Xmm weights = cc->newXmm("weights");
      x86::Mem wPtr = x86::ptr(pc->_fetchData, REL_PATTERN(simple.wa));

      // [00 Wd 00 Wc 00 Wb 00 Wa]
      pc->v_loadu_i128(weights, wPtr);
      // [Wd Wc Wb Wa Wd Wc Wb Wa]
      pc->v_packs_i32_i16(weights, weights, weights);

      if (isAlphaFetch()) {
        if (isPatternFy()) {
          pc->v_swizzle_lo_i16(f->wd_wb, weights, x86::shuffleImm(3, 1, 3, 1));
          if (maxPixels() >= 4)
            pc->v_swizzle_i32(f->wd_wb, f->wd_wb, x86::shuffleImm(1, 0, 1, 0));
        }
        else if (isPatternFx()) {
          pc->v_swizzle_i32(f->wc_wd, weights, x86::shuffleImm(3, 3, 3, 3));
        }
        else {
          pc->v_swizzle_lo_i16(f->wa_wc, weights, x86::shuffleImm(2, 0, 2, 0));
          pc->v_swizzle_lo_i16(f->wb_wd, weights, x86::shuffleImm(3, 1, 3, 1));
          if (maxPixels() >= 4) {
            pc->v_swizzle_i32(f->wa_wc, f->wa_wc, x86::shuffleImm(1, 0, 1, 0));
            pc->v_swizzle_i32(f->wb_wd, f->wb_wd, x86::shuffleImm(1, 0, 1, 0));
          }
        }
      }
      else {
        // [Wd Wd Wc Wc Wb Wb Wa Wa]
        pc->v_interleave_lo_i16(weights, weights, weights);

        if (isPatternFy()) {
          pc->v_swizzle_i32(f->wb_wb, weights, x86::shuffleImm(1, 1, 1, 1));
          pc->v_swizzle_i32(f->wd_wd, weights, x86::shuffleImm(3, 3, 3, 3));
        }
        else if (isPatternFx()) {
          pc->v_swizzle_i32(f->wc_wd, weights, x86::shuffleImm(2, 2, 3, 3));
        }
        else {
          pc->v_swizzle_i32(f->wa_wb, weights, x86::shuffleImm(0, 0, 1, 1));
          pc->v_swizzle_i32(f->wc_wd, weights, x86::shuffleImm(2, 2, 3, 3));
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

// BLPipeline::JIT::FetchSimplePatternPart - Advance
// =================================================

void FetchSimplePatternPart::advanceY() noexcept {
  if (isAlignedBlit()) {
    // Blit AA
    // -------

    // That's the beauty of AABlit - no checks needed, no extend modes used.
    cc->add(f->srcp1, f->stride);
  }
  else {
    // Vertical Pad, Repeat, and Reflect
    // ---------------------------------

    Label L_VertDone = cc->newLabel();
    Label L_VertZero = cc->newLabel();

    // If this pattern fetch uses two source pointers (one for current scanline
    // and one for previous one) copy current to the previous so it can be used
    // (only fetchers that use Fy).
    if (hasFracY())
      cc->mov(f->srcp0, f->srcp1);

    cc->dec(f->y);                                         // if (--f->y == 0) {
    cc->jz(L_VertZero);                                    //   <L_VertZero>
                                                           // } else {
    cc->add(f->srcp1, f->stride);                          //   f->srcp1 += f->stride;
    cc->bind(L_VertDone);                                  // }

    // Vertical Repeat
    // ---------------

    PipeInjectAtTheEnd injected(pc);

    Label L_VertPadC = cc->newLabel();
    Label L_VertRepeat  = cc->newLabel();
    Label L_VertReflect = cc->newLabel();

    cc->bind(L_VertZero);

    cc->mov(f->y, f->h);                                   // A single comparison can
    cc->cmp(f->y, f->ry);                                  // handle all 3 extend modes.

    cc->je(L_VertRepeat);
    cc->jb(L_VertReflect);

    // Vertical Pad - Case B|C:
    cc->cmp(f->stride, 0);
    cc->jne(L_VertPadC);                                   // if (f->stride == 0) {
    cc->mov(f->stride, f->strideOrig);                     //   f->stride = f->strideOrig;
    cc->jmp(L_VertDone);                                   // }

    // Vertical Pad - Case C:
    cc->bind(L_VertPadC);                                  // else {
    cc->xor_(f->y, f->y);                                  //   f->y = 0;
    cc->mov(f->stride, f->y.cloneAs(f->stride));           //   f->stride = 0;
    cc->jmp(L_VertDone);                                   // }

    // Vertical Repeat:
    cc->bind(L_VertRepeat);                                // if (f->h == f->ry) {
    cc->mov(f->srcp1, f->srctop);                          //   f->srcp1 = f->srctop;
    cc->jmp(L_VertDone);                                   // }

    // Vertical Reflect:
    cc->bind(L_VertReflect);                               // if (f->h < f->ry) {
    cc->neg(f->stride);                                    //   f->stride = -f->stride;
    cc->jmp(L_VertDone);                                   // }
  }
}

void FetchSimplePatternPart::startAtX(x86::Gp& x) noexcept {
  if (isAlignedBlit()) {
    // Blit AA
    // -------

    // TODO: [PIPEGEN] Relax this constraint.
    // Rectangular blits only.
    BL_ASSERT(isRectFill());
  }
  else {
    cc->mov(f->x, f->xOrigin);                             // f->x = f->xOrigin;

    // Horizontal Pad
    // --------------

    if (extendX() == ExtendMode::kPad) {
      if (!isRectFill())
        cc->add(f->x, x);
      pc->uBound0ToN(f->xPadded.r32(), f->x, f->w);
    }

    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    if (extendX() == ExtendMode::kRepeat) {
      if (!isRectFill()) {                                 // if (!RectFill) {
        pc->uAddMulImm(f->x, x, int(bpp()));               //   f->x += x * pattern.bpp;
        repeatOrReflectX();                                //   f->x = repeatLarge(f->x);
      }                                                    // }
    }

    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    if (extendX() == ExtendMode::kRoR) {
      if (!isRectFill()) {                                 // if (!RectFill) {
        cc->add(f->x, x);                                  //   f->x += x;
        repeatOrReflectX();                                //   f->x = repeatOrReflect(f->x);
      }                                                    // }
    }
  }

  prefetchAccX();

  if (pixelGranularity() > 1)
    enterN();
}

void FetchSimplePatternPart::advanceX(x86::Gp& x, x86::Gp& diff) noexcept {
  blUnused(x);
  x86::Gp fx32 = f->x.r32();

  if (pixelGranularity() > 1)
    leaveN();

  if (isAlignedBlit()) {
    // Blit AA
    // -------

    pc->uAddMulImm(f->srcp1, diff.cloneAs(f->srcp1), int(bpp()));
  }
  else if (extendX() == ExtendMode::kPad) {
    // Horizontal Pad
    // --------------

    if (hasFracX())                                        // if (hasFracX())
      cc->lea(fx32, x86::ptr(f->x.r32(), diff, 0, -1));    //   f->x += diff - 1;
    else                                                   // else
      cc->add(fx32, diff);                                 //   f->x += diff;

    pc->uBound0ToN(f->xPadded.r32(), f->x, f->w);
  }
  else if (extendX() == ExtendMode::kRepeat) {
    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    pc->uAddMulImm(f->x, diff, int(bpp()));                // f->x += diff * pattern.bpp;
    repeatOrReflectX();                                    // f->x = repeatLarge(f->x);
  }
  else if (extendX() == ExtendMode::kRoR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    if (hasFracX())                                        // if (hasFracX())
      cc->lea(fx32, x86::ptr(fx32, diff, 0, -1));          //   f->x += diff - 1;
    else                                                   // else
      cc->add(fx32, diff);                                 //   f->x += diff;

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

    cc->add(f->srcp1, int(bpp()));
  }
  else if (extendX() == ExtendMode::kPad) {
    // Horizontal Pad
    // --------------

    cc->inc(f->x);
    cc->cmp(f->x, f->w);
    cc->cmovbe(f->xPadded.r32(), f->x);
  }
  else if (extendX() == ExtendMode::kRepeat) {
    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    cc->add(f->x, int(bpp()));
    cc->cmovz(f->x, f->xRestart);
  }
  else if (extendX() == ExtendMode::kRoR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    cc->inc(f->x);
    cc->cmp(f->x, f->w);
    cc->cmovz(f->x, f->xRestart);
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

    Label L_HorzSkip = cc->newLabel();

    cc->cmp(f->x, 0);
    cc->short_().jl(L_HorzSkip);                           // if (f->x >= 0)
    cc->add(f->x, f->xRestart);                            //   f->x -= f->w;

    // `f->x` too large to be corrected by `f->w`, so do it the slow way:

    cc->short_().js(L_HorzSkip);                           // if (f->x >= 0) {
    pc->uMod(f->x.r32(), f->w);                            //   f->x %= f->w;
    cc->add(f->x, f->xRestart);                            //   f->x -= f->w;
    cc->bind(L_HorzSkip);                                  // }
  }
  else if (extendX() == ExtendMode::kRoR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    Label L_HorzSkip = cc->newLabel();
    x86::Gp norm = cc->newInt32("@norm");

    cc->cmp(f->x, f->rx);
    cc->short_().jl(L_HorzSkip);                           // if (f->x >= f->rx) {
    pc->uMod(f->x, f->rx);                                 //   f->x %= f->rx;
    cc->xor_(norm, norm);                                  //   norm = 0;
    cc->cmp(f->x, f->w);                                   //   if (f->x >= f->w)
    cc->cmovae(norm, f->rx);                               //     norm = f->rx;
    cc->sub(f->x, norm);                                   //   f->x -= norm;
    cc->bind(L_HorzSkip);                                  // }
  }
}

void FetchSimplePatternPart::prefetchAccX() noexcept {
  if (!hasFracX())
    return;

  x86::Gp idx;

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
    idx = cc->newIntPtr("@idx");
    pc->uReflect(idx.r32(), f->x);
  }

  if (isAlphaFetch()) {
    if (isPatternFx()) {
      pc->v_load_i8(f->pixL, x86::ptr(f->srcp1, idx, _idxShift, _alphaOffset));
    }
    else {
      pc->v_load_i8(f->pixL, x86::ptr(f->srcp0, idx, _idxShift, _alphaOffset));
      pc->xInsertWordOrByte(f->pixL, x86::ptr(f->srcp1, idx, _idxShift, _alphaOffset), 1);
    }
  }
  else {
    if (isPatternFx()) {
      pc->v_load_i32(f->pixL, x86::ptr(f->srcp1, idx, _idxShift));
      pc->vmovu8u16(f->pixL, f->pixL);
      pc->v_mul_i16(f->pixL, f->pixL, f->wc_wd);
    }
    else {
      x86::Xmm pixL = f->pixL;
      x86::Xmm pixT = cc->newXmm("@pixT");

      pc->v_load_i32(pixL, x86::ptr_32(f->srcp0, idx, _idxShift));
      pc->v_load_i32(pixT, x86::ptr_32(f->srcp1, idx, _idxShift));

      pc->vmovu8u16(pixL, pixL);
      pc->vmovu8u16(pixT, pixT);

      pc->v_mul_i16(pixL, pixL, f->wa_wb);
      pc->v_mul_i16(pixT, pixT, f->wc_wd);

      pc->v_add_i16(pixL, pixL, pixT);
    }
}

  advanceXByOne();
}

// BLPipeline::JIT::FetchSimplePatternPart - Fetch
// ===============================================

void FetchSimplePatternPart::fetch1(Pixel& p, PixelFlags flags) noexcept {
  if (isAlignedBlit()) {
    // Blit AA
    // -------

    pc->xFetchPixel_1x(p, flags, format(), x86::ptr(f->srcp1), 4);
    advanceXByOne();
  }
  else {
    p.setCount(1);
    x86::Gp idx;

    // Pattern AA or Fx/Fy
    // -------------------

    if (extendX() == ExtendMode::kPad) {
      idx = f->xPadded;
    }

    if (extendX() == ExtendMode::kRepeat) {
      idx = f->x;
    }

    if (extendX() == ExtendMode::kRoR) {
      idx = cc->newIntPtr("@idx");
      pc->uReflect(idx.r32(), f->x);
    }

    if (isPatternAligned()) {
      pc->xFetchPixel_1x(p, flags, format(), x86::ptr(f->srcp1, idx, _idxShift), 4);
      advanceXByOne();
    }

    if (isPatternFy()) {
      if (isAlphaFetch()) {
        x86::Xmm pixA = cc->newXmm("@pixA");

        pc->xFetchUnpackedA8_2x(pixA, format(), x86::ptr(f->srcp1, idx, _idxShift), x86::ptr(f->srcp0, idx, _idxShift));
        pc->v_madd_i16_i32(pixA, pixA, f->wd_wb);
        pc->v_srl_i16(pixA, pixA, 8);

        advanceXByOne();

        pc->xAssignUnpackedAlphaValues(p, flags, pixA);
        pc->xSatisfyPixel(p, flags);
      }
      else if (p.isRGBA()) {
        x86::Xmm pix0 = cc->newXmm("@pix0");
        x86::Xmm pix1 = cc->newXmm("@pix1");

        pc->v_load_i32(pix0, x86::ptr(f->srcp0, idx, _idxShift));
        pc->v_load_i32(pix1, x86::ptr(f->srcp1, idx, _idxShift));

        pc->vmovu8u16(pix0, pix0);
        pc->vmovu8u16(pix1, pix1);

        pc->v_mul_i16(pix0, pix0, f->wb_wb);
        pc->v_mul_i16(pix1, pix1, f->wd_wd);

        advanceXByOne();

        pc->v_add_i16(pix0, pix0, pix1);
        pc->v_srl_i16(pix0, pix0, 8);

        p.uc.init(pix0);
        pc->xSatisfyPixel(p, flags);
      }
    }

    if (isPatternFx()) {
      if (isAlphaFetch()) {
        x86::Xmm pixL = f->pixL;
        x86::Xmm pixA = cc->newXmm("@pixA");

        pc->xInsertWordOrByte(pixL, x86::ptr(f->srcp1, idx, _idxShift, _alphaOffset), 1);
        pc->v_madd_i16_i32(pixA, pixL, f->wc_wd);
        pc->v_srl_i32(pixL, pixL, 16);
        pc->v_srl_i16(pixA, pixA, 8);

        advanceXByOne();

        pc->xAssignUnpackedAlphaValues(p, flags, pixA);
        pc->xSatisfyPixel(p, flags);
      }
      else if (p.isRGBA()) {
        x86::Xmm pixL = f->pixL;
        x86::Xmm pix0 = cc->newXmm("@pix0");

        if (pc->hasSSE4_1()) {
          pc->v_swap_i64(pix0, pixL);
          pc->v_load_i32_u8u32_(pixL, x86::ptr(f->srcp1, idx, _idxShift));
          pc->v_packs_i32_i16(pixL, pixL, pixL);
        }
        else {
          pc->v_swap_i64(pix0, pixL);
          pc->v_load_i32(pixL, x86::ptr(f->srcp1, idx, _idxShift));
          pc->v_swizzle_i32(pixL, pixL, x86::shuffleImm(0, 0, 0, 0));
          pc->vmovu8u16(pixL, pixL);
        }

        pc->v_mul_i16(pixL, pixL, f->wc_wd);
        advanceXByOne();

        pc->v_add_i16(pix0, pix0, pixL);
        pc->v_srl_i16(pix0, pix0, 8);

        p.uc.init(pix0);
        pc->xSatisfyPixel(p, flags);
      }
    }

    if (isPatternFxFy()) {
      if (isAlphaFetch()) {
        x86::Xmm pixL = f->pixL;
        x86::Xmm pixA = cc->newXmm("@pixA");
        x86::Xmm pixB = cc->newXmm("@pixB");

        pc->vloadu8_u16_2x(pixB, x86::ptr(f->srcp0, idx, _idxShift, _alphaOffset), x86::ptr(f->srcp1, idx, _idxShift, _alphaOffset));
        pc->v_madd_i16_i32(pixA, pixL, f->wa_wc);
        pc->v_mov(pixL, pixB);
        pc->v_madd_i16_i32(pixB, pixB, f->wb_wd);
        pc->v_add_i32(pixA, pixA, pixB);
        pc->v_srl_i16(pixA, pixA, 8);

        advanceXByOne();

        pc->xAssignUnpackedAlphaValues(p, flags, pixA);
        pc->xSatisfyPixel(p, flags);
      }
      else if (p.isRGBA()) {
        x86::Xmm pixL = f->pixL;
        x86::Xmm pixT = cc->newXmm("@pixT");
        x86::Xmm pix0 = cc->newXmm("@pix0");

        if (pc->hasSSE4_1()) {
          pc->v_load_i32_u8u32_(pixT, x86::ptr(f->srcp1, idx, _idxShift));
          pc->v_swap_i64(pix0, pixL);
          pc->v_load_i32_u8u32_(pixL, x86::ptr(f->srcp0, idx, _idxShift));

          pc->v_packs_i32_i16(pixT, pixT, pixT);
          pc->v_packs_i32_i16(pixL, pixL, pixL);
        }
        else {
          pc->v_load_i32(pixT, x86::ptr(f->srcp1, idx, _idxShift));
          pc->v_swap_i64(pix0, pixL);
          pc->v_load_i32(pixL, x86::ptr(f->srcp0, idx, _idxShift));

          pc->v_swizzle_i32(pixT, pixT, x86::shuffleImm(0, 0, 0, 0));
          pc->v_swizzle_i32(pixL, pixL, x86::shuffleImm(0, 0, 0, 0));

          pc->vmovu8u16(pixT, pixT);
          pc->vmovu8u16(pixL, pixL);
        }

        pc->v_mul_i16(pixT, pixT, f->wc_wd);
        pc->v_mul_i16(pixL, pixL, f->wa_wb);

        advanceXByOne();

        pc->v_add_i16(pixL, pixL, pixT);
        pc->v_add_i16(pix0, pix0, pixL);
        pc->v_srl_i16(pix0, pix0, 8);

        p.uc.init(pix0);
        pc->xSatisfyPixel(p, flags);
      }
    }
  }
}

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

    x86::Xmm xFix4 = cc->newXmm("@xFix4");

    pc->v_broadcast_u32(f->xVec4, f->x.r32());
    pc->v_add_i32(f->xVec4, f->xVec4, f->xSet4);

    pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
    pc->v_and(xFix4, xFix4, f->xNrm4);
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

    pc->s_mov_i32(f->x.r32(), f->xVec4);
  }
}

void FetchSimplePatternPart::prefetchN() noexcept {}
void FetchSimplePatternPart::postfetchN() noexcept {}

void FetchSimplePatternPart::fetch4(Pixel& p, PixelFlags flags) noexcept {
  p.setCount(4);

  if (isAlignedBlit()) {
    // Blit AA
    // -------

    pc->xFetchPixel_4x(p, flags, format(), x86::ptr(f->srcp1), 4);
    cc->add(f->srcp1, int(4 * bpp()));
  }
  else {
    PixelType intermediateType = isAlphaFetch() ? PixelType::kAlpha : PixelType::kRGBA;
    PixelFlags intermediateFlags = isAlphaFetch() ? PixelFlags::kUA : PixelFlags::kUC;

    // Horizontal Pad
    // --------------

    if (extendX() == ExtendMode::kPad) {
      // Horizontal Pad - Aligned
      // ------------------------

      if (isPatternAligned()) {
        FetchContext fCtx(pc, &p, 4, format(), flags);

        x86::Gp idx = f->xPadded;
        x86::Mem mem = x86::ptr(f->srcp1, idx, _idxShift);

        cc->inc(f->x);
        fCtx.fetchPixel(mem);
        cc->cmp(f->x, f->w);
        cc->cmovbe(idx.r32(), f->x);

        cc->inc(f->x);
        fCtx.fetchPixel(mem);
        cc->cmp(f->x, f->w);
        cc->cmovbe(idx.r32(), f->x);

        cc->inc(f->x);
        fCtx.fetchPixel(mem);
        cc->cmp(f->x, f->w);
        cc->cmovbe(idx.r32(), f->x);

        cc->inc(f->x);
        fCtx.fetchPixel(mem);
        cc->cmp(f->x, f->w);
        cc->cmovbe(idx.r32(), f->x);

        fCtx.end();
        pc->xSatisfyPixel(p, flags);
      }

      // Horizontal Pad - FracY
      // ----------------------

      if (isPatternFy()) {
        x86::Gp idx = f->xPadded;

        if (isAlphaFetch()) {
          Pixel fPix(intermediateType);
          FetchContext fCtx(pc, &fPix, 8, format(), intermediateFlags);

          x86::Mem m0 = x86::ptr(f->srcp0, idx, _idxShift);
          x86::Mem m1 = x86::ptr(f->srcp1, idx, _idxShift);

          cc->inc(f->x);
          fCtx.fetchPixel(m0);
          fCtx.fetchPixel(m1);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          fCtx.fetchPixel(m0);
          fCtx.fetchPixel(m1);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          fCtx.fetchPixel(m0);
          fCtx.fetchPixel(m1);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          fCtx.fetchPixel(m0);
          fCtx.fetchPixel(m1);
          fCtx.end();

          x86::Xmm& pix0 = fPix.ua[0].as<x86::Xmm>();

          cc->inc(f->x);
          pc->v_madd_i16_i32(pix0, pix0, f->wd_wb);

          cc->cmp(f->x, f->w);
          pc->v_srl_i16(pix0, pix0, 8);

          cc->cmovbe(idx.r32(), f->x);
          pc->v_packs_i32_i16(pix0, pix0, pix0);

          pc->xAssignUnpackedAlphaValues(p, flags, pix0);
          pc->xSatisfyPixel(p, flags);
        }
        else if (p.isRGBA()) {
          Pixel pix0(intermediateType);
          Pixel pix1(intermediateType);

          FetchContext fCtx0(pc, &pix0, 4, format(), intermediateFlags);
          FetchContext fCtx1(pc, &pix1, 4, format(), intermediateFlags);

          x86::Mem m0 = x86::ptr(f->srcp0, idx, _idxShift);
          x86::Mem m1 = x86::ptr(f->srcp1, idx, _idxShift);

          cc->inc(f->x);
          fCtx0.fetchPixel(m0);
          fCtx1.fetchPixel(m1);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          fCtx0.fetchPixel(m0);
          fCtx1.fetchPixel(m1);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          fCtx0.fetchPixel(m0);
          fCtx1.fetchPixel(m1);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          fCtx0.fetchPixel(m0);
          fCtx1.fetchPixel(m1);
          fCtx0.end();
          fCtx1.end();

          cc->cmp(f->x, f->w);
          pc->v_mul_i16(pix0.uc, pix0.uc, f->wb_wb);
          pc->v_mul_i16(pix1.uc, pix1.uc, f->wd_wd);

          cc->cmovbe(idx.r32(), f->x);
          pc->v_add_i16(pix0.uc, pix0.uc, pix1.uc);
          pc->v_srl_i16(pix0.uc, pix0.uc, 8);

          p.uc.init(pix0.uc[0], pix0.uc[1]);
          pc->xSatisfyPixel(p, flags);
        }
      }

      // Horizontal Pad - FracX
      // ----------------------

      if (isPatternFx()) {
        x86::Gp idx = f->xPadded;
        x86::Mem m = x86::ptr(f->srcp1, idx, _idxShift);

        if (isAlphaFetch()) {
          Pixel fPix(intermediateType);
          FetchContext fCtx(pc, &fPix, 4, format(), intermediateFlags);

          x86::Vec& pixA = fPix.ua[0];
          x86::Vec& pixL = f->pixL;

          cc->inc(f->x);
          fCtx.fetchPixel(m);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          fCtx.fetchPixel(m);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          fCtx.fetchPixel(m);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          fCtx.fetchPixel(m);
          fCtx.end();

          pc->v_interleave_lo_i16(pixA, pixA, pixA);
          cc->cmp(f->x, f->w);

          pc->v_sllb_i128(pixA, pixA, 2);
          cc->cmovbe(idx.r32(), f->x);

          pc->v_or(pixL, pixL, pixA);
          pc->v_madd_i16_i32(pixA, pixL, f->wc_wd);

          pc->v_srlb_i128(pixL, pixL, 14);
          pc->v_srl_i32(pixA, pixA, 8);
          pc->v_packs_i32_i16(pixA, pixA, pixA);

          pc->xAssignUnpackedAlphaValues(p, flags, pixA.as<x86::Xmm>());
          pc->xSatisfyPixel(p, flags);
        }
        else if (p.isRGBA()) {
          x86::Xmm pixL = f->pixL;
          x86::Xmm pixT = cc->newXmm("@pixT");

          x86::Xmm pix0 = cc->newXmm("@pix0");
          x86::Xmm pix1 = cc->newXmm("@pix1");
          x86::Xmm pix2 = cc->newXmm("@pix2");

          if (pc->hasSSE4_1()) {
            cc->inc(f->x);
            pc->v_load_i32_u8u32_(pix0, m);
            cc->cmp(f->x, f->w);
            cc->cmovbe(idx.r32(), f->x);

            cc->inc(f->x);
            pc->v_load_i32_u8u32_(pix1, m);
            cc->cmp(f->x, f->w);
            cc->cmovbe(idx.r32(), f->x);

            pc->v_packs_i32_i16(pix0, pix0, pix0);
            pc->v_packs_i32_i16(pix1, pix1, pix1);

            pc->v_mul_i16(pix0, pix0, f->wc_wd);
            pc->v_mul_i16(pix1, pix1, f->wc_wd);

            cc->inc(f->x);
            pc->v_load_i32_u8u32_(pix2, m);
            cc->cmp(f->x, f->w);
            cc->cmovbe(idx.r32(), f->x);

            pc->v_combine_hl_i64(pixT, pixL, pix1);
            pc->v_load_i32_u8u32_(pixL, m);

            pc->v_packs_i32_i16(pix2, pix2, pix2);
            pc->v_packs_i32_i16(pixL, pixL, pixL);
          }
          else {
            cc->inc(f->x);
            cc->cmp(f->x, f->w);
            pc->v_load_i32(pix0, m);
            cc->cmovbe(idx.r32(), f->x);

            pc->v_swizzle_i32(pix0, pix0, x86::shuffleImm(0, 0, 0, 0));
            pc->v_load_i32(pix1, m);
            cc->inc(f->x);
            pc->v_swizzle_i32(pix1, pix1, x86::shuffleImm(0, 0, 0, 0));
            pc->vmovu8u16(pix0, pix0);
            cc->cmp(f->x, f->w);
            pc->vmovu8u16(pix1, pix1);
            cc->cmovbe(idx.r32(), f->x);

            pc->v_mul_i16(pix0, pix0, f->wc_wd);
            pc->v_mul_i16(pix1, pix1, f->wc_wd);
            cc->inc(f->x);
            cc->cmp(f->x, f->w);
            pc->v_load_i32(pix2, m);
            cc->cmovbe(idx.r32(), f->x);

            pc->v_swizzle_i32(pix2, pix2, x86::shuffleImm(0, 0, 0, 0));
            pc->v_combine_hl_i64(pixT, pixL, pix1);
            pc->v_load_i32(pixL, m);

            pc->vmovu8u16(pix2, pix2);
            pc->v_swizzle_i32(pixL, pixL, x86::shuffleImm(0, 0, 0, 0));
            pc->vmovu8u16(pixL, pixL);
          }

          pc->v_add_i16(pix0, pix0, pixT);

          pc->v_mul_i16(pixL, pixL, f->wc_wd);
          pc->v_mul_i16(pix2, pix2, f->wc_wd);
          pc->v_srl_i16(pix0, pix0, 8);

          pc->v_combine_hl_i64(pix1, pix1, pixL);
          cc->inc(f->x);
          pc->v_add_i16(pix2, pix2, pix1);
          cc->cmp(f->x, f->w);
          pc->v_srl_i16(pix2, pix2, 8);
          cc->cmovbe(idx.r32(), f->x);

          p.uc.init(pix0, pix2);
          pc->xSatisfyPixel(p, flags);
        }
      }

      // Horizontal Pad - FracXY
      // -----------------------

      if (isPatternFxFy()) {
        x86::Gp idx = f->xPadded;
        x86::Mem mA = x86::ptr(f->srcp0, idx, _idxShift);
        x86::Mem mB = x86::ptr(f->srcp1, idx, _idxShift);

        if (isAlphaFetch()) {
          Pixel fPix(intermediateType);
          FetchContext fCtx(pc, &fPix, 8, format(), intermediateFlags);

          cc->inc(f->x);
          fCtx.fetchPixel(mA);
          fCtx.fetchPixel(mB);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          fCtx.fetchPixel(mA);
          fCtx.fetchPixel(mB);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          fCtx.fetchPixel(mA);
          fCtx.fetchPixel(mB);
          cc->cmp(f->x, f->w);
          cc->cmovbe(idx.r32(), f->x);

          fCtx.fetchPixel(mA);
          fCtx.fetchPixel(mB);
          fCtx.end();

          x86::Vec& pixA = fPix.ua[0];
          x86::Vec  pixB = cc->newXmm("pixB");
          x86::Vec& pixL = f->pixL;

          pc->v_sllb_i128(pixB, pixA, 4);
          pc->v_or(pixB, pixB, pixL);
          pc->v_srlb_i128(pixL, pixA, 12);

          pc->v_madd_i16_i32(pixA, pixA, f->wb_wd);
          pc->v_madd_i16_i32(pixB, pixB, f->wa_wc);

          cc->inc(f->x);
          pc->v_add_i32(pixA, pixA, pixB);

          cc->cmp(f->x, f->w);
          pc->v_srl_i32(pixA, pixA, 8);

          cc->cmovbe(idx.r32(), f->x);
          pc->v_packs_i32_i16(pixA, pixA, pixA);

          pc->xAssignUnpackedAlphaValues(p, flags, pixA.as<x86::Xmm>());
          pc->xSatisfyPixel(p, flags);
        }
        else if (p.isRGBA()) {
          x86::Xmm pixL = f->pixL;
          x86::Xmm pixT = cc->newXmm("@pixT");

          x86::Xmm pix0  = cc->newXmm("@pix0");
          x86::Xmm pix0t = cc->newXmm("@pix0t");
          x86::Xmm pix1  = cc->newXmm("@pix1");
          x86::Xmm pix1t = cc->newXmm("@pix1t");
          x86::Xmm pix2  = cc->newXmm("@pix2");
          x86::Xmm pix2t = cc->newXmm("@pix2t");

          cc->inc(f->x);
          cc->cmp(f->x, f->w);

          if (pc->hasSSE4_1()) {
            pc->v_load_i32_u8u32_(pix0, mA);
            pc->v_load_i32_u8u32_(pix0t, mB);
            cc->cmovbe(idx.r32(), f->x);

            pc->v_load_i32_u8u32_(pix1, mA);
            pc->v_load_i32_u8u32_(pix1t, mB);
            cc->inc(f->x);
            pc->v_packs_i32_i16(pix0, pix0, pix0);
            pc->v_packs_i32_i16(pix0t, pix0t, pix0t);
            cc->cmp(f->x, f->w);
            pc->v_packs_i32_i16(pix1, pix1, pix1);
            pc->v_packs_i32_i16(pix1t, pix1t, pix1t);
            cc->cmovbe(idx.r32(), f->x);
            cc->inc(f->x);

            pc->v_mul_i16(pix1 , pix1 , f->wa_wb);
            pc->v_mul_i16(pix1t, pix1t, f->wc_wd);
            pc->v_mul_i16(pix0 , pix0 , f->wa_wb);
            pc->v_mul_i16(pix0t, pix0t, f->wc_wd);
            cc->cmp(f->x, f->w);

            pc->v_add_i16(pix1, pix1, pix1t);
            pc->v_load_i32_u8u32_(pix2, mA);
            pc->v_add_i16(pix0, pix0, pix0t);
            pc->v_load_i32_u8u32_(pix2t, mB);
            cc->cmovbe(idx.r32(), f->x);

            pc->v_combine_hl_i64(pixT, pixL, pix1);
            pc->v_load_i32_u8u32_(pixL, mA);
            pc->v_add_i16(pix0, pix0, pixT);
            pc->v_load_i32_u8u32_(pixT, mB);

            pc->v_packs_i32_i16(pixL, pixL, pixL);
            pc->v_packs_i32_i16(pix2 , pix2, pix2);
            pc->v_packs_i32_i16(pix2t, pix2t, pix2t);
            pc->v_mul_i16(pixL, pixL, f->wa_wb);
            pc->v_packs_i32_i16(pixT, pixT, pixT);
          }
          else {
            pc->v_load_i32(pix0, mA);
            pc->v_load_i32(pix0t, mB);
            cc->cmovbe(idx.r32(), f->x);

            pc->v_swizzle_i32(pix0 , pix0 , x86::shuffleImm(0, 0, 0, 0));
            pc->v_swizzle_i32(pix0t, pix0t, x86::shuffleImm(0, 0, 0, 0));

            pc->v_load_i32(pix1, mA);
            pc->v_load_i32(pix1t, mB);
            cc->inc(f->x);
            pc->v_swizzle_i32(pix1 , pix1 , x86::shuffleImm(0, 0, 0, 0));
            pc->v_swizzle_i32(pix1t, pix1t, x86::shuffleImm(0, 0, 0, 0));
            pc->vmovu8u16(pix0, pix0);
            pc->vmovu8u16(pix0t, pix0t);
            pc->vmovu8u16(pix1, pix1);
            pc->vmovu8u16(pix1t, pix1t);
            cc->cmp(f->x, f->w);

            pc->v_mul_i16(pix1 , pix1 , f->wa_wb);
            pc->v_mul_i16(pix1t, pix1t, f->wc_wd);
            pc->v_mul_i16(pix0 , pix0 , f->wa_wb);
            pc->v_mul_i16(pix0t, pix0t, f->wc_wd);
            cc->cmovbe(idx.r32(), f->x);
            cc->inc(f->x);

            pc->v_add_i16(pix1, pix1, pix1t);
            pc->v_load_i32(pix2, mA);
            pc->v_add_i16(pix0, pix0, pix0t);
            pc->v_load_i32(pix2t, mB);
            cc->cmp(f->x, f->w);

            pc->v_swizzle_i32(pix2 , pix2 , x86::shuffleImm(0, 0, 0, 0));
            pc->v_swizzle_i32(pix2t, pix2t, x86::shuffleImm(0, 0, 0, 0));
            pc->v_combine_hl_i64(pixT, pixL, pix1);
            pc->v_load_i32(pixL, mA);
            pc->v_add_i16(pix0, pix0, pixT);
            pc->v_load_i32(pixT, mB);
            cc->cmovbe(idx.r32(), f->x);

            pc->vmovu8u16(pix2 , pix2);
            pc->v_swizzle_i32(pixL, pixL, x86::shuffleImm(0, 0, 0, 0));
            pc->vmovu8u16(pix2t, pix2t);
            pc->vmovu8u16(pixL, pixL);
            pc->v_swizzle_i32(pixT, pixT, x86::shuffleImm(0, 0, 0, 0));
            pc->v_mul_i16(pixL, pixL, f->wa_wb);
            pc->vmovu8u16(pixT, pixT);
          }

          pc->v_mul_i16(pix2, pix2, f->wa_wb);
          pc->v_mul_i16(pixT, pixT, f->wc_wd);
          pc->v_mul_i16(pix2t, pix2t, f->wc_wd);
          pc->v_srl_i16(pix0, pix0, 8);

          pc->v_add_i16(pixL, pixL, pixT);
          pc->v_add_i16(pix2, pix2, pix2t);
          cc->inc(f->x);
          pc->v_combine_hl_i64(pix1, pix1, pixL);
          cc->cmp(f->x, f->w);
          pc->v_add_i16(pix2, pix2, pix1);
          cc->cmovbe(idx.r32(), f->x);
          pc->v_srl_i16(pix2, pix2, 8);

          p.uc.init(pix0, pix2);
          pc->xSatisfyPixel(p, flags);
        }
      }
    }

    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    if (extendX() == ExtendMode::kRoR) {
      x86::Xmm xIdx4 = cc->newXmm("@xIdx4");
      x86::Xmm xFix4 = cc->newXmm("@xFix4");

      // Horizontal RoR - Aligned
      // ------------------------

      if (isPatternAligned()) {
        FetchContext fCtx(pc, &p, 4, format(), flags);

        pc->v_sra_i32(xIdx4, f->xVec4, 31);
        pc->v_xor(xIdx4, xIdx4, f->xVec4);
        pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);
        FetchUtils::fetch_4x(&fCtx, x86::ptr(f->srcp1), xIdx4, _idxShift);

        pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
        pc->v_and(xFix4, xFix4, f->xNrm4);
        pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);

        fCtx.end();
        pc->xSatisfyPixel(p, flags);
      }

      // Horizontal RoR - FracY
      // ----------------------

      if (isPatternFy()) {
        pc->v_sra_i32(xIdx4, f->xVec4, 31);
        pc->v_xor(xIdx4, xIdx4, f->xVec4);
        pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);

        if (isAlphaFetch()) {
          Pixel fPix(intermediateType);
          FetchContext fCtx(pc, &fPix, 8, format(), intermediateFlags);

          FetchUtils::fetch_4x_twice(&fCtx, x86::ptr(f->srcp0), &fCtx, x86::ptr(f->srcp1), xIdx4, _idxShift);
          fCtx.end();

          x86::Xmm& pix0 = fPix.ua[0].as<x86::Xmm>();

          pc->v_madd_i16_i32(pix0, pix0, f->wd_wb);
          pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);

          pc->v_srl_i16(pix0, pix0, 8);
          pc->v_and(xFix4, xFix4, f->xNrm4);

          pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
          pc->v_packs_i32_i16(pix0, pix0, pix0);

          pc->xAssignUnpackedAlphaValues(p, flags, pix0);
          pc->xSatisfyPixel(p, flags);
        }
        else if (p.isRGBA()) {
          Pixel pix0(p.type());
          Pixel pix1(p.type());

          FetchContext fCtx0(pc, &pix0, 4, format(), PixelFlags::kUC);
          FetchContext fCtx1(pc, &pix1, 4, format(), PixelFlags::kUC);
          FetchUtils::fetch_4x_twice(&fCtx0, x86::ptr(f->srcp0), &fCtx1, x86::ptr(f->srcp1), xIdx4, _idxShift);

          fCtx0.end();
          fCtx1.end();

          pc->v_mul_i16(pix0.uc, pix0.uc, f->wb_wb);
          pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
          pc->v_mul_i16(pix1.uc, pix1.uc, f->wd_wd);

          pc->v_and(xFix4, xFix4, f->xNrm4);
          pc->v_add_i16(pix0.uc, pix0.uc, pix1.uc);

          pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
          pc->v_srl_i16(pix0.uc, pix0.uc, 8);

          p.uc.init(pix0.uc[0], pix0.uc[1]);
          pc->xSatisfyPixel(p, flags);
        }
      }

      // Horizontal RoR - FracX
      // ----------------------

      if (isPatternFx()) {
        pc->v_sra_i32(xIdx4, f->xVec4, 31);
        pc->v_xor(xIdx4, xIdx4, f->xVec4);

        if (isAlphaFetch()) {
          Pixel fPix(intermediateType);
          FetchContext fCtx(pc, &fPix, 4, format(), intermediateFlags);

          FetchUtils::fetch_4x(&fCtx, x86::ptr(f->srcp1), xIdx4, _idxShift);
          fCtx.end();

          x86::Vec& pixA = fPix.ua[0];
          x86::Vec& pixL = f->pixL;

          pc->v_interleave_lo_i16(pixA, pixA, pixA);
          pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);

          pc->v_sllb_i128(pixA, pixA, 2);
          pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
          pc->v_or(pixL, pixL, pixA);

          pc->v_and(xFix4, xFix4, f->xNrm4);
          pc->v_madd_i16_i32(pixA, pixL, f->wc_wd);

          pc->v_srlb_i128(pixL, pixL, 14);
          pc->v_srl_i32(pixA, pixA, 8);

          pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
          pc->v_packs_i32_i16(pixA, pixA, pixA);

          pc->xAssignUnpackedAlphaValues(p, flags, pixA.as<x86::Xmm>());
          pc->xSatisfyPixel(p, flags);
        }
        else if (p.isRGBA()) {
          IndexExtractor iExt(pc);
          iExt.begin(IndexExtractor::kTypeUInt32, xIdx4);

          x86::Gp idx0 = cc->newIntPtr("@idx0");
          x86::Gp idx1 = cc->newIntPtr("@idx1");

          x86::Xmm pixL = f->pixL;
          x86::Xmm pixT = cc->newXmm("@pixT");

          x86::Xmm pix0 = cc->newXmm("@pix0");
          x86::Xmm pix1 = cc->newXmm("@pix1");
          x86::Xmm pix2 = cc->newXmm("@pix2");

          pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);
          iExt.extract(idx0, 0);

          pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
          iExt.extract(idx1, 1);
          pc->v_and(xFix4, xFix4, f->xNrm4);

          if (pc->hasSSE4_1()) {
            pc->v_load_i32_u8u32_(pix0, x86::ptr(f->srcp1, idx0, _idxShift));
            iExt.extract(idx0, 2);

            pc->v_load_i32_u8u32_(pix1, x86::ptr(f->srcp1, idx1, _idxShift));
            iExt.extract(idx1, 3);

            pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
            pc->v_packs_i32_i16(pix0, pix0, pix0);
            pc->v_packs_i32_i16(pix1, pix1, pix1);

            pc->v_mul_i16(pix1, pix1, f->wc_wd);
            pc->v_mul_i16(pix0, pix0, f->wc_wd);
            pc->v_load_i32_u8u32_(pix2, x86::ptr(f->srcp1, idx0, _idxShift));
            pc->v_combine_hl_i64(pixT, pixL, pix1);

            pc->v_load_i32_u8u32_(pixL, x86::ptr(f->srcp1, idx1, _idxShift));
            pc->v_packs_i32_i16(pix2, pix2, pix2);
            pc->v_packs_i32_i16(pixL, pixL, pixL);
          }
          else {
            pc->v_load_i32(pix0, x86::ptr(f->srcp1, idx0, _idxShift));
            iExt.extract(idx0, 2);

            pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
            pc->v_swizzle_i32(pix0, pix0, x86::shuffleImm(0, 0, 0, 0));
            pc->v_load_i32(pix1, x86::ptr(f->srcp1, idx1, _idxShift));
            iExt.extract(idx1, 3);

            pc->v_swizzle_i32(pix1, pix1, x86::shuffleImm(0, 0, 0, 0));
            pc->vmovu8u16(pix0, pix0);
            pc->vmovu8u16(pix1, pix1);

            pc->v_mul_i16(pix1, pix1, f->wc_wd);
            pc->v_mul_i16(pix0, pix0, f->wc_wd);
            pc->v_load_i32(pix2, x86::ptr(f->srcp1, idx0, _idxShift));

            pc->v_swizzle_i32(pix2, pix2, x86::shuffleImm(0, 0, 0, 0));
            pc->v_combine_hl_i64(pixT, pixL, pix1);
            pc->v_load_i32(pixL, x86::ptr(f->srcp1, idx1, _idxShift));

            pc->vmovu8u16(pix2, pix2);
            pc->v_swizzle_i32(pixL, pixL, x86::shuffleImm(0, 0, 0, 0));
            pc->vmovu8u16(pixL, pixL);
          }

          pc->v_add_i16(pix0, pix0, pixT);
          pc->v_mul_i16(pixL, pixL, f->wc_wd);
          pc->v_mul_i16(pix2, pix2, f->wc_wd);
          pc->v_srl_i16(pix0, pix0, 8);

          pc->v_combine_hl_i64(pix1, pix1, pixL);
          pc->v_add_i16(pix2, pix2, pix1);
          pc->v_srl_i16(pix2, pix2, 8);

          p.uc.init(pix0, pix2);
          pc->xSatisfyPixel(p, flags);
        }
      }

      // Horizontal RoR - FracXY
      // -----------------------

      if (isPatternFxFy()) {
        pc->v_sra_i32(xIdx4, f->xVec4, 31);
        pc->v_xor(xIdx4, xIdx4, f->xVec4);

        if (isAlphaFetch()) {
          Pixel fPix(intermediateType);
          FetchContext fCtx(pc, &fPix, 8, format(), intermediateFlags);

          FetchUtils::fetch_4x_twice(&fCtx, x86::ptr(f->srcp0), &fCtx, x86::ptr(f->srcp1), xIdx4, _idxShift);
          fCtx.end();

          x86::Vec& pixA = fPix.ua[0];
          x86::Vec  pixB = cc->newXmm("pixB");
          x86::Vec& pixL = f->pixL;

          pc->v_sllb_i128(pixB, pixA, 4);
          pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);

          pc->v_or(pixB, pixB, pixL);
          pc->v_srlb_i128(pixL, pixA, 12);

          pc->v_madd_i16_i32(pixA, pixA, f->wb_wd);
          pc->v_madd_i16_i32(pixB, pixB, f->wa_wc);
          pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);

          pc->v_add_i32(pixA, pixA, pixB);
          pc->v_and(xFix4, xFix4, f->xNrm4);

          pc->v_srl_i32(pixA, pixA, 8);
          pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);
          pc->v_packs_i32_i16(pixA, pixA, pixA);

          pc->xAssignUnpackedAlphaValues(p, flags, pixA.as<x86::Xmm>());
          pc->xSatisfyPixel(p, flags);
        }
        else if (p.isRGBA()) {
          IndexExtractor iExt(pc);

          x86::Gp idx0 = cc->newIntPtr("@idx0");
          x86::Gp idx1 = cc->newIntPtr("@idx1");

          x86::Xmm pixL = f->pixL;
          x86::Xmm pixT = cc->newXmm("@pixT");

          x86::Xmm pix0  = cc->newXmm("@pix0");
          x86::Xmm pix0t = cc->newXmm("@pix0t");
          x86::Xmm pix1  = cc->newXmm("@pix1");
          x86::Xmm pix1t = cc->newXmm("@pix1t");
          x86::Xmm pix2  = cc->newXmm("@pix2");
          x86::Xmm pix2t = cc->newXmm("@pix2t");

          iExt.begin(IndexExtractor::kTypeUInt32, xIdx4);

          pc->v_add_i32(f->xVec4, f->xVec4, f->xInc4);
          iExt.extract(idx0, 0);

          pc->v_cmp_gt_i32(xFix4, f->xVec4, f->xMax4);
          iExt.extract(idx1, 1);
          pc->v_and(xFix4, xFix4, f->xNrm4);

          if (pc->hasSSE4_1()) {
            pc->v_load_i32_u8u32_(pix0 , x86::ptr(f->srcp0, idx0, _idxShift));
            pc->v_load_i32_u8u32_(pix0t, x86::ptr(f->srcp1, idx0, _idxShift));
            iExt.extract(idx0, 2);
            pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);

            pc->v_load_i32_u8u32_(pix1 , x86::ptr(f->srcp0, idx1, _idxShift));
            pc->v_load_i32_u8u32_(pix1t, x86::ptr(f->srcp1, idx1, _idxShift));
            iExt.extract(idx1, 3);

            pc->v_packs_i32_i16(pix0, pix0, pix0);
            pc->v_packs_i32_i16(pix0t, pix0t, pix0t);
            pc->v_packs_i32_i16(pix1, pix1, pix1);
            pc->v_packs_i32_i16(pix1t, pix1t, pix1t);

            pc->v_mul_i16(pix1 , pix1 , f->wa_wb);
            pc->v_mul_i16(pix1t, pix1t, f->wc_wd);
            pc->v_mul_i16(pix0 , pix0 , f->wa_wb);
            pc->v_mul_i16(pix0t, pix0t, f->wc_wd);

            pc->v_add_i16(pix1, pix1, pix1t);
            pc->v_load_i32_u8u32_(pix2, x86::ptr(f->srcp0, idx0, _idxShift));
            pc->v_add_i16(pix0, pix0, pix0t);
            pc->v_load_i32_u8u32_(pix2t, x86::ptr(f->srcp1, idx0, _idxShift));

            pc->v_combine_hl_i64(pixT, pixL, pix1);
            pc->v_load_i32_u8u32_(pixL, x86::ptr(f->srcp0, idx1, _idxShift));
            pc->v_add_i16(pix0, pix0, pixT);
            pc->v_load_i32_u8u32_(pixT, x86::ptr(f->srcp1, idx1, _idxShift));

            pc->v_packs_i32_i16(pixL, pixL, pixL);
            pc->v_packs_i32_i16(pix2, pix2, pix2);
            pc->v_packs_i32_i16(pix2t, pix2t, pix2t);
            pc->v_mul_i16(pixL, pixL, f->wa_wb);
            pc->v_packs_i32_i16(pixT, pixT, pixT);
          }
          else {
            pc->v_load_i32(pix0, x86::ptr(f->srcp0, idx0, _idxShift));
            pc->v_load_i32(pix0t, x86::ptr(f->srcp1, idx0, _idxShift));
            iExt.extract(idx0, 2);
            pc->v_sub_i32(f->xVec4, f->xVec4, xFix4);

            pc->v_swizzle_i32(pix0, pix0, x86::shuffleImm(0, 0, 0, 0));
            pc->v_swizzle_i32(pix0t, pix0t, x86::shuffleImm(0, 0, 0, 0));

            pc->v_load_i32(pix1, x86::ptr(f->srcp0, idx1, _idxShift));
            pc->v_load_i32(pix1t, x86::ptr(f->srcp1, idx1, _idxShift));
            iExt.extract(idx1, 3);

            pc->v_swizzle_i32(pix1, pix1, x86::shuffleImm(0, 0, 0, 0));
            pc->v_swizzle_i32(pix1t, pix1t, x86::shuffleImm(0, 0, 0, 0));
            pc->vmovu8u16(pix0, pix0);
            pc->vmovu8u16(pix0t, pix0t);
            pc->vmovu8u16(pix1, pix1);
            pc->vmovu8u16(pix1t, pix1t);

            pc->v_mul_i16(pix1, pix1, f->wa_wb);
            pc->v_mul_i16(pix1t, pix1t, f->wc_wd);
            pc->v_mul_i16(pix0, pix0, f->wa_wb);
            pc->v_mul_i16(pix0t, pix0t, f->wc_wd);

            pc->v_add_i16(pix1, pix1, pix1t);
            pc->v_load_i32(pix2, x86::ptr(f->srcp0, idx0, _idxShift));
            pc->v_add_i16(pix0, pix0, pix0t);
            pc->v_load_i32(pix2t, x86::ptr(f->srcp1, idx0, _idxShift));

            pc->v_swizzle_i32(pix2, pix2, x86::shuffleImm(0, 0, 0, 0));
            pc->v_swizzle_i32(pix2t, pix2t, x86::shuffleImm(0, 0, 0, 0));
            pc->v_combine_hl_i64(pixT, pixL, pix1);
            pc->v_load_i32(pixL, x86::ptr(f->srcp0, idx1, _idxShift));
            pc->v_add_i16(pix0, pix0, pixT);
            pc->v_load_i32(pixT, x86::ptr(f->srcp1, idx1, _idxShift));

            pc->vmovu8u16(pix2, pix2);
            pc->v_swizzle_i32(pixL, pixL, x86::shuffleImm(0, 0, 0, 0));
            pc->vmovu8u16(pix2t, pix2t);
            pc->vmovu8u16(pixL, pixL);
            pc->v_swizzle_i32(pixT, pixT, x86::shuffleImm(0, 0, 0, 0));
            pc->v_mul_i16(pixL, pixL, f->wa_wb);
            pc->vmovu8u16(pixT, pixT);
          }

          pc->v_mul_i16(pix2, pix2, f->wa_wb);
          pc->v_mul_i16(pixT, pixT, f->wc_wd);
          pc->v_mul_i16(pix2t, pix2t, f->wc_wd);
          pc->v_srl_i16(pix0, pix0, 8);

          pc->v_add_i16(pixL, pixL, pixT);
          pc->v_add_i16(pix2, pix2, pix2t);
          pc->v_combine_hl_i64(pix1, pix1, pixL);
          pc->v_add_i16(pix2, pix2, pix1);
          pc->v_srl_i16(pix2, pix2, 8);

          p.uc.init(pix0, pix2);
          pc->xSatisfyPixel(p, flags);
        }
      }
    }

    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    if (extendX() == ExtendMode::kRepeat) {
      // Only generated for AA patterns.
      BL_ASSERT(isPatternAligned());

      FetchContext fCtx(pc, &p, 4, format(), flags);

      int offset = int(4 * bpp());
      x86::Mem mem = x86::ptr(f->srcp1, f->x, 0, -offset);

      Label L_Repeat = cc->newLabel();
      Label L_Done = cc->newLabel();

      cc->add(f->x, offset);
      cc->jc(L_Repeat);

      if (p.isRGBA()) {
        if (blTestFlag(flags, PixelFlags::kPC)) {
          const x86::Vec& reg = p.pc[0];

          switch (format()) {
            case BL_FORMAT_PRGB32:
            case BL_FORMAT_XRGB32: {
              pc->v_loadu_i128_ro(reg, mem);
              break;
            }
            case BL_FORMAT_A8: {
              pc->v_load_i32(reg, mem);
              pc->v_interleave_lo_i8(reg, reg, reg);
              pc->v_interleave_lo_i16(reg, reg, reg);
              break;
            }
          }
        }
        else {
          const x86::Vec& uc0 = p.uc[0];
          const x86::Vec& uc1 = p.uc[1];

          switch (format()) {
            case BL_FORMAT_PRGB32:
            case BL_FORMAT_XRGB32: {
              pc->vmovu8u16(uc0, mem);
              pc->vmovu8u16(uc1, mem.cloneAdjusted(8));
              break;
            }
            case BL_FORMAT_A8: {
              pc->v_load_i32(uc0, mem);
              pc->v_interleave_lo_i8(uc0, uc0, uc0);
              pc->vmovu8u16(uc0, uc0);
              pc->v_swizzle_i32(uc1, uc0, x86::shuffleImm(3, 3, 2, 2));
              pc->v_swizzle_i32(uc0, uc0, x86::shuffleImm(1, 1, 0, 0));
              break;
            }
          }
        }
      }
      else {
        if (blTestFlag(flags, PixelFlags::kPA)) {
          const x86::Vec& reg = p.pa[0];
          switch (format()) {
            case BL_FORMAT_PRGB32:
            case BL_FORMAT_XRGB32: {
              pc->v_loadu_i128_ro(reg, mem);
              if (pc->hasSSSE3()) {
                pc->v_shuffle_i8(reg, reg, pc->constAsXmm(&blCommonTable.i128_pshufb_argb32_to_a8_packed));
              }
              else {
                pc->v_srl_i32(reg, reg, 24);
                pc->v_packs_i32_i16(reg, reg, reg);
                pc->v_packs_i16_u8(reg, reg, reg);
              }
              break;
            }
            case BL_FORMAT_A8: {
              pc->v_load_i32(reg, mem);
              break;
            }
          }
        }
        else {
          const x86::Vec& reg = p.ua[0];
          switch (format()) {
            case BL_FORMAT_PRGB32:
            case BL_FORMAT_XRGB32: {
              pc->v_loadu_i128_ro(reg, mem);
              pc->v_srl_i32(reg, reg, 24);
              pc->v_packs_i32_i16(reg, reg, reg);
              break;
            }
            case BL_FORMAT_A8: {
              pc->v_load_i32(reg, mem);
              pc->vmovu8u16(reg, reg);
              break;
            }
          }
        }
      }

      cc->bind(L_Done);

      {
        PipeInjectAtTheEnd injected(pc);
        cc->bind(L_Repeat);

        fCtx.fetchPixel(mem);
        mem.addOffsetLo32(offset);

        cc->sub(f->x, offset - int(bpp()));
        cc->cmovz(f->x, f->xRestart);
        fCtx.fetchPixel(mem);

        cc->add(f->x, int(bpp()));
        cc->cmovz(f->x, f->xRestart);
        fCtx.fetchPixel(mem);

        cc->add(f->x, int(bpp()));
        cc->cmovz(f->x, f->xRestart);
        fCtx.fetchPixel(mem);

        cc->add(f->x, int(bpp()));
        cc->cmovz(f->x, f->xRestart);
        fCtx.end();

        cc->jmp(L_Done);
      }

      pc->xSatisfyPixel(p, flags);
    }
  }
}

void FetchSimplePatternPart::fetch8(Pixel& p, PixelFlags flags) noexcept {
  if (isAlignedBlit()) {
    // Blit AA
    // -------

    pc->xFetchPixel_8x(p, flags, format(), x86::ptr(f->srcp1), 4);
    cc->add(f->srcp1, int(8 * bpp()));
  }
  else {
    FetchPart::fetch8(p, flags);
  }
}

// BLPipeline::JIT::FetchAffinePatternPart - Construction & Destruction
// ====================================================================

FetchAffinePatternPart::FetchAffinePatternPart(PipeCompiler* pc, FetchType fetchType, uint32_t format) noexcept
  : FetchPatternPart(pc, fetchType, format) {

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

  if (BLIntOps::isPowerOf2(_bpp))
    _idxShift = uint8_t(BLIntOps::ctz(_bpp));
}

// BLPipeline::JIT::FetchAffinePatternPart - Init & Fini
// =====================================================

void FetchAffinePatternPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  f->srctop         = cc->newIntPtr("f.srctop");      // Mem.
  f->stride         = cc->newIntPtr("f.stride");      // Mem.

  f->xx_xy          = cc->newXmm("f.xx_xy");          // Reg.
  f->yx_yy          = cc->newXmm("f.yx_yy");          // Reg/Mem.
  f->tx_ty          = cc->newXmm("f.tx_ty");          // Reg/Mem.
  f->px_py          = cc->newXmm("f.px_py");          // Reg.
  f->ox_oy          = cc->newXmm("f.ox_oy");          // Reg/Mem.
  f->rx_ry          = cc->newXmm("f.rx_ry");          // Reg/Mem.
  f->qx_qy          = cc->newXmm("f.qx_qy");          // Reg     [fetch4].
  f->xx2_xy2        = cc->newXmm("f.xx2_xy2");        // Reg/Mem [fetch4].
  f->minx_miny      = cc->newXmm("f.minx_miny");      // Reg/Mem.
  f->maxx_maxy      = cc->newXmm("f.maxx_maxy");      // Reg/Mem.
  f->corx_cory      = cc->newXmm("f.corx_cory");      // Reg/Mem.
  f->tw_th          = cc->newXmm("f.tw_th");          // Reg/Mem.

  f->vIdx           = cc->newXmm("f.vIdx");           // Reg/Tmp.
  f->vAddrMul       = cc->newXmm("f.vAddrMul");       // Reg/Tmp.
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  cc->mov(f->srctop, x86::ptr(pc->_fetchData, REL_PATTERN(src.pixelData)));
  cc->mov(f->stride, x86::ptr(pc->_fetchData, REL_PATTERN(src.stride)));

  pc->v_loadu_i128(f->xx_xy, x86::ptr(pc->_fetchData, REL_PATTERN(affine.xx)));
  pc->v_loadu_i128(f->yx_yy, x86::ptr(pc->_fetchData, REL_PATTERN(affine.yx)));

  pc->s_mov_i32(f->tx_ty, y);
  pc->v_swizzle_i32(f->tx_ty, f->tx_ty, x86::shuffleImm(1, 0, 1, 0));
  pc->vMulU64xU32Lo(f->tx_ty, f->yx_yy, f->tx_ty);
  pc->v_add_i64(f->tx_ty, f->tx_ty, x86::ptr(pc->_fetchData, REL_PATTERN(affine.tx)));

  // RoR: `tw_th` and `rx_ry` are only used by repeated or reflected patterns.
  pc->v_loadu_i128(f->rx_ry, x86::ptr(pc->_fetchData, REL_PATTERN(affine.rx)));
  pc->v_loadu_i128(f->tw_th, x86::ptr(pc->_fetchData, REL_PATTERN(affine.tw)));

  pc->v_loadu_i128(f->ox_oy, x86::ptr(pc->_fetchData, REL_PATTERN(affine.ox)));
  pc->v_loadu_i128(f->rx_ry, x86::ptr(pc->_fetchData, REL_PATTERN(affine.rx)));
  pc->v_loadu_i128(f->xx2_xy2, x86::ptr(pc->_fetchData, REL_PATTERN(affine.xx2)));

  // Pad: [MaxY | MaxX | MinY | MinX]
  pc->v_loadu_i128(f->minx_miny, x86::ptr(pc->_fetchData, REL_PATTERN(affine.minX)));
  pc->v_load_i64(f->corx_cory, x86::ptr(pc->_fetchData, REL_PATTERN(affine.corX)));

  if (isOptimized()) {
    pc->v_packs_i32_i16(f->minx_miny, f->minx_miny, f->minx_miny);                   // [MaxY|MaxX|MinY|MinX|MaxY|MaxX|MinY|MinX]
    pc->v_swizzle_i32(f->maxx_maxy, f->minx_miny, x86::shuffleImm(1, 1, 1, 1)); // [MaxY|MaxX|MaxY|MaxX|MaxY|MaxX|MaxY|MaxX]
    pc->v_swizzle_i32(f->minx_miny, f->minx_miny, x86::shuffleImm(0, 0, 0, 0)); // [MinY|MinX|MinY|MinX|MinY|MinX|MinY|MinX]
  }
  else {
    pc->v_swizzle_i32(f->maxx_maxy, f->minx_miny, x86::shuffleImm(3, 3, 2, 2)); // [MaxY|MaxY|MaxX|MaxX]
    pc->v_swizzle_i32(f->minx_miny, f->minx_miny, x86::shuffleImm(1, 1, 0, 0)); // [MinY|MinY|MinX|MinX]
    pc->v_swizzle_i32(f->corx_cory, f->corx_cory, x86::shuffleImm(1, 1, 0, 0)); // [CorY|CorY|CorX|CorX]
  }

  // vAddrMul.
  if (isOptimized()) {
    pc->v_load_i32(f->vAddrMul, x86::ptr(pc->_fetchData, REL_PATTERN(affine.addrMul)));
    pc->v_swizzle_i32(f->vAddrMul, f->vAddrMul, x86::shuffleImm(0, 0, 0, 0));
  }

  if (isRectFill()) {
    advancePxPy(f->tx_ty, x);
    normalizePxPy(f->tx_ty);
  }
}

void FetchAffinePatternPart::_finiPart() noexcept {}

// BLPipeline::JIT::FetchAffinePatternPart - Advance
// =================================================

void FetchAffinePatternPart::advanceY() noexcept {
  pc->v_add_i64(f->tx_ty, f->tx_ty, f->yx_yy);

  if (isRectFill())
    normalizePxPy(f->tx_ty);
}

void FetchAffinePatternPart::startAtX(x86::Gp& x) noexcept {
  if (isRectFill()) {
    pc->v_mov(f->px_py, f->tx_ty);
  }
  else {
    // Similar to `advancePxPy()`, however, we don't need a temporary here...
    pc->s_mov_i32(f->px_py, x.r32());
    pc->v_swizzle_i32(f->px_py, f->px_py, x86::shuffleImm(1, 0, 1, 0));
    pc->vMulU64xU32Lo(f->px_py, f->xx_xy, f->px_py);
    pc->v_add_i64(f->px_py, f->px_py, f->tx_ty);

    normalizePxPy(f->px_py);
  }

  if (pixelGranularity() > 1)
    enterN();
}

void FetchAffinePatternPart::advanceX(x86::Gp& x, x86::Gp& diff) noexcept {
  blUnused(x);
  BL_ASSERT(!isRectFill());

  if (pixelGranularity() > 1)
    leaveN();

  advancePxPy(f->px_py, diff);
  normalizePxPy(f->px_py);

  if (pixelGranularity() > 1)
    enterN();
}

void FetchAffinePatternPart::advancePxPy(x86::Xmm& px_py, const x86::Gp& i) noexcept {
  x86::Xmm t = cc->newXmm("@t");

  pc->s_mov_i32(t, i.r32());
  pc->v_swizzle_i32(t, t, x86::shuffleImm(1, 0, 1, 0));
  pc->vMulU64xU32Lo(t, f->xx_xy, t);
  pc->v_add_i64(px_py, px_py, t);
}

void FetchAffinePatternPart::normalizePxPy(x86::Xmm& px_py) noexcept {
  x86::Xmm v0 = cc->newXmm("v0");

  pc->v_zero_i(v0);
  pc->xModI64HIxDouble(px_py, px_py, f->tw_th);
  pc->v_cmp_gt_i32(v0, v0, px_py);
  pc->v_and(v0, v0, f->rx_ry);
  pc->v_add_i32(px_py, px_py, v0);

  pc->v_cmp_gt_i32(v0, px_py, f->ox_oy);
  pc->v_and(v0, v0, f->rx_ry);
  pc->v_sub_i32(px_py, px_py, v0);
}

// BLPipeline::JIT::FetchAffinePatternPart - Fetch
// ===============================================

void FetchAffinePatternPart::prefetch1() noexcept {
  x86::Xmm vIdx = f->vIdx;

  switch (fetchType()) {
    case FetchType::kPatternAffineNNAny: {
      clampVIdx32(vIdx, f->px_py, kClampStepA_NN);
      clampVIdx32(vIdx, vIdx    , kClampStepB_NN);
      break;
    }

    case FetchType::kPatternAffineNNOpt: {
      pc->v_swizzle_i32(vIdx, f->px_py, x86::shuffleImm(3, 1, 3, 1));
      pc->v_packs_i32_i16(vIdx, vIdx, vIdx);
      pc->v_max_i16(vIdx, vIdx, f->minx_miny);
      pc->v_min_i16(vIdx, vIdx, f->maxx_maxy);
      break;
    }
  }
}

void FetchAffinePatternPart::fetch1(Pixel& p, PixelFlags flags) noexcept {
  p.setCount(1);

  switch (fetchType()) {
    case FetchType::kPatternAffineNNAny: {
      x86::Gp texPtr = cc->newIntPtr("texPtr");
      x86::Gp texOff = cc->newIntPtr("texOff");

      x86::Xmm vIdx = f->vIdx;
      x86::Xmm vMsk = cc->newXmm("vMsk");

      clampVIdx32(vIdx, vIdx, kClampStepC_NN);
      pc->v_add_i64(f->px_py, f->px_py, f->xx_xy);

      IndexExtractor iExt(pc);
      iExt.begin(IndexExtractor::kTypeUInt32, vIdx);
      iExt.extract(texPtr, 3);
      iExt.extract(texOff, 1);

      pc->v_cmp_gt_i32(vMsk, f->px_py, f->ox_oy);
      cc->imul(texPtr, f->stride);
      pc->v_and(vMsk, vMsk, f->rx_ry);
      pc->v_sub_i32(f->px_py, f->px_py, vMsk);

      cc->add(texPtr, f->srctop);
      pc->xFetchPixel_1x(p, flags, format(), x86::ptr(texPtr, texOff, _idxShift), 4);
      clampVIdx32(vIdx, f->px_py, kClampStepA_NN);

      pc->xSatisfyPixel(p, flags);
      clampVIdx32(vIdx, vIdx, kClampStepB_NN);
      break;
    }

    case FetchType::kPatternAffineNNOpt: {
      x86::Gp texPtr = cc->newIntPtr("texPtr");
      x86::Xmm vIdx = f->vIdx;
      x86::Xmm vMsk = cc->newXmm("vMsk");

      pc->v_sra_i16(vMsk, vIdx, 15);
      pc->v_xor(vIdx, vIdx, vMsk);

      pc->v_add_i64(f->px_py, f->px_py, f->xx_xy);
      pc->v_madd_i16_i32(vIdx, vIdx, f->vAddrMul);

      pc->v_cmp_gt_i32(vMsk, f->px_py, f->ox_oy);
      pc->v_and(vMsk, vMsk, f->rx_ry);
      pc->v_sub_i32(f->px_py, f->px_py, vMsk);
      pc->s_mov_i32(texPtr.r32(), vIdx);

      pc->v_swizzle_i32(vIdx, f->px_py, x86::shuffleImm(3, 1, 3, 1));
      pc->v_packs_i32_i16(vIdx, vIdx, vIdx);

      cc->add(texPtr, f->srctop);
      pc->v_max_i16(vIdx, vIdx, f->minx_miny);
      pc->xFetchPixel_1x(p, flags, format(), x86::ptr(texPtr), 4);

      pc->v_min_i16(vIdx, vIdx, f->maxx_maxy);
      pc->xSatisfyPixel(p, flags);
      break;
    }

    case FetchType::kPatternAffineBIAny: {
      if (isAlphaFetch()) {
        x86::Xmm vIdx = cc->newXmm("vIdx");
        x86::Xmm vMsk = cc->newXmm("vMsk");
        x86::Xmm vWeights = cc->newXmm("vWeights");

        pc->v_swizzle_i32(vIdx, f->px_py, x86::shuffleImm(3, 3, 1, 1));
        pc->v_sub_i32(vIdx, vIdx, pc->constAsMem(&blCommonTable.i128_FFFFFFFF00000000));

        pc->v_swizzle_lo_i16(vWeights, f->px_py, x86::shuffleImm(1, 1, 1, 1));
        clampVIdx32(vIdx, vIdx, kClampStepA_BI);

        pc->v_add_i64(f->px_py, f->px_py, f->xx_xy);
        clampVIdx32(vIdx, vIdx, kClampStepB_BI);

        pc->v_cmp_gt_i32(vMsk, f->px_py, f->ox_oy);
        pc->v_swizzle_hi_i16(vWeights, vWeights, x86::shuffleImm(1, 1, 1, 1));

        pc->v_and(vMsk, vMsk, f->rx_ry);
        pc->v_srl_i16(vWeights, vWeights, 8);

        pc->v_sub_i32(f->px_py, f->px_py, vMsk);
        pc->v_xor(vWeights, vWeights, pc->constAsMem(&blCommonTable.i128_FFFF0000FFFF0000));

        clampVIdx32(vIdx, vIdx, kClampStepC_BI);
        pc->v_add_i16(vWeights, vWeights, pc->constAsMem(&blCommonTable.i128_0101000001010000));

        x86::Vec pixA = cc->newXmm("pixA");
        FetchUtils::xFilterBilinearA8_1x(pc, pixA, f->srctop, f->stride, format(), _idxShift, vIdx, vWeights);

        pc->xAssignUnpackedAlphaValues(p, flags, pixA.as<x86::Xmm>());
        pc->xSatisfyPixel(p, flags);
      }
      else if (p.isRGBA()) {
        x86::Xmm vIdx = cc->newXmm("vIdx");
        x86::Xmm vMsk = cc->newXmm("vMsk");
        x86::Xmm vWeights = cc->newXmm("vWeights");

        pc->v_swizzle_i32(vIdx, f->px_py, x86::shuffleImm(3, 3, 1, 1));
        pc->v_sub_i32(vIdx, vIdx, pc->constAsMem(&blCommonTable.i128_FFFFFFFF00000000));

        pc->v_swizzle_lo_i16(vWeights, f->px_py, x86::shuffleImm(1, 1, 1, 1));
        clampVIdx32(vIdx, vIdx, kClampStepA_BI);

        pc->v_add_i64(f->px_py, f->px_py, f->xx_xy);
        clampVIdx32(vIdx, vIdx, kClampStepB_BI);

        pc->v_cmp_gt_i32(vMsk, f->px_py, f->ox_oy);
        pc->v_swizzle_hi_i16(vWeights, vWeights, x86::shuffleImm(1, 1, 1, 1));

        pc->v_and(vMsk, vMsk, f->rx_ry);
        pc->v_srl_i16(vWeights, vWeights, 8);

        pc->v_sub_i32(f->px_py, f->px_py, vMsk);
        pc->v_xor(vWeights, vWeights, pc->constAsMem(&blCommonTable.i128_FFFFFFFF00000000));

        clampVIdx32(vIdx, vIdx, kClampStepC_BI);
        pc->v_add_i16(vWeights, vWeights, pc->constAsMem(&blCommonTable.i128_0101010100000000));

        p.uc.init(cc->newXmm("pix0"));
        FetchUtils::xFilterBilinearARGB32_1x(pc, p.uc[0], f->srctop, f->stride, vIdx, vWeights);
        pc->xSatisfyPixel(p, flags);
      }
      break;
    }

    case FetchType::kPatternAffineBIOpt: {
      // TODO: [PIPEGEN] Not implemented, not used for now...
      break;
    }
  }
}

void FetchAffinePatternPart::enterN() noexcept {
  x86::Xmm vMsk0 = cc->newXmm("vMsk0");

  pc->v_add_i64(f->qx_qy, f->px_py, f->xx_xy);
  pc->v_cmp_gt_i32(vMsk0, f->qx_qy, f->ox_oy);
  pc->v_and(vMsk0, vMsk0, f->rx_ry);
  pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk0);
}

void FetchAffinePatternPart::leaveN() noexcept {}

void FetchAffinePatternPart::prefetchN() noexcept {
  switch (fetchType()) {
    case FetchType::kPatternAffineNNOpt: {
      x86::Xmm vIdx = f->vIdx;
      x86::Xmm vMsk0 = cc->newXmm("vMsk0");
      x86::Xmm vMsk1 = cc->newXmm("vMsk1");

      pc->v_shuffle_i32(vIdx, f->px_py, f->qx_qy, x86::shuffleImm(3, 1, 3, 1));
      pc->v_add_i64(f->px_py, f->px_py, f->xx2_xy2);
      pc->v_add_i64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      pc->v_cmp_gt_i32(vMsk0, f->px_py, f->ox_oy);
      pc->v_cmp_gt_i32(vMsk1, f->qx_qy, f->ox_oy);

      pc->v_and(vMsk0, vMsk0, f->rx_ry);
      pc->v_and(vMsk1, vMsk1, f->rx_ry);

      pc->v_sub_i32(f->px_py, f->px_py, vMsk0);
      pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk1);

      pc->v_shuffle_i32(vMsk0, f->px_py, f->qx_qy, x86::shuffleImm(3, 1, 3, 1));
      pc->v_packs_i32_i16(vIdx, vIdx, vMsk0);

      pc->v_max_i16(vIdx, vIdx, f->minx_miny);
      pc->v_min_i16(vIdx, vIdx, f->maxx_maxy);

      pc->v_sra_i16(vMsk0, vIdx, 15);
      pc->v_xor(vIdx, vIdx, vMsk0);
      break;
    }
  }
}

void FetchAffinePatternPart::postfetchN() noexcept {
  switch (fetchType()) {
    case FetchType::kPatternAffineNNOpt: {
      break;
    }
  }
}

void FetchAffinePatternPart::fetch4(Pixel& p, PixelFlags flags) noexcept {
  p.setCount(4);

  switch (fetchType()) {
    case FetchType::kPatternAffineNNAny: {
      FetchContext fCtx(pc, &p, 4, format(), flags);
      IndexExtractor iExt(pc);

      x86::Gp texPtr0 = cc->newIntPtr("texPtr0");
      x86::Gp texOff0 = cc->newIntPtr("texOff0");
      x86::Gp texPtr1 = cc->newIntPtr("texPtr1");
      x86::Gp texOff1 = cc->newIntPtr("texOff1");

      x86::Xmm vIdx0 = cc->newXmm("vIdx0");
      x86::Xmm vIdx1 = cc->newXmm("vIdx1");
      x86::Xmm vMsk0 = cc->newXmm("vMsk0");
      x86::Xmm vMsk1 = cc->newXmm("vMsk1");

      pc->v_shuffle_i32(vIdx0, f->px_py, f->qx_qy, x86::shuffleImm(3, 1, 3, 1));
      pc->v_add_i64(f->px_py, f->px_py, f->xx2_xy2);

      clampVIdx32(vIdx0, vIdx0, kClampStepA_NN);
      pc->v_add_i64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      clampVIdx32(vIdx0, vIdx0, kClampStepB_NN);
      pc->v_cmp_gt_i32(vMsk0, f->px_py, f->ox_oy);
      clampVIdx32(vIdx0, vIdx0, kClampStepC_NN);

      pc->v_cmp_gt_i32(vMsk1, f->qx_qy, f->ox_oy);
      pc->v_and(vMsk0, vMsk0, f->rx_ry);
      pc->v_and(vMsk1, vMsk1, f->rx_ry);
      pc->v_sub_i32(f->px_py, f->px_py, vMsk0);
      pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk1);

      iExt.begin(IndexExtractor::kTypeUInt32, vIdx0);
      pc->v_shuffle_i32(vIdx1, f->px_py, f->qx_qy, x86::shuffleImm(3, 1, 3, 1));
      iExt.extract(texPtr0, 1);
      iExt.extract(texOff0, 0);

      clampVIdx32(vIdx1, vIdx1, kClampStepA_NN);
      clampVIdx32(vIdx1, vIdx1, kClampStepB_NN);

      iExt.extract(texPtr1, 3);
      iExt.extract(texOff1, 2);

      cc->imul(texPtr0, f->stride);
      cc->imul(texPtr1, f->stride);

      clampVIdx32(vIdx1, vIdx1, kClampStepC_NN);
      pc->v_add_i64(f->px_py, f->px_py, f->xx2_xy2);
      pc->v_add_i64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      cc->add(texPtr0, f->srctop);
      cc->add(texPtr1, f->srctop);
      iExt.begin(IndexExtractor::kTypeUInt32, vIdx1);

      fCtx.fetchPixel(x86::ptr(texPtr0, texOff0, _idxShift));
      iExt.extract(texPtr0, 1);
      iExt.extract(texOff0, 0);

      pc->v_cmp_gt_i32(vMsk0, f->px_py, f->ox_oy);
      pc->v_cmp_gt_i32(vMsk1, f->qx_qy, f->ox_oy);

      fCtx.fetchPixel(x86::ptr(texPtr1, texOff1, _idxShift));
      iExt.extract(texPtr1, 3);
      iExt.extract(texOff1, 2);
      cc->imul(texPtr0, f->stride);

      pc->v_and(vMsk0, vMsk0, f->rx_ry);
      pc->v_and(vMsk1, vMsk1, f->rx_ry);

      cc->imul(texPtr1, f->stride);
      pc->v_sub_i32(f->px_py, f->px_py, vMsk0);

      cc->add(texPtr0, f->srctop);
      cc->add(texPtr1, f->srctop);
      fCtx.fetchPixel(x86::ptr(texPtr0, texOff0, _idxShift));

      pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk1);
      fCtx.fetchPixel(x86::ptr(texPtr1, texOff1, _idxShift));
      fCtx.end();

      pc->xSatisfyPixel(p, flags);
      break;
    }

    case FetchType::kPatternAffineNNOpt: {
      FetchContext fCtx(pc, &p, 4, format(), flags);
      IndexExtractor iExt(pc);

      x86::Gp texPtr0 = cc->newIntPtr("texPtr0");
      x86::Gp texPtr1 = cc->newIntPtr("texPtr1");

      x86::Xmm vIdx = f->vIdx;
      x86::Xmm vMsk0 = cc->newXmm("vMsk0");
      x86::Xmm vMsk1 = cc->newXmm("vMsk1");

      pc->v_madd_i16_i32(vIdx, vIdx, f->vAddrMul);
      iExt.begin(IndexExtractor::kTypeUInt32, vIdx);

      pc->v_add_i64(f->px_py, f->px_py, f->xx2_xy2);
      pc->v_add_i64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      pc->v_cmp_gt_i32(vMsk0, f->px_py, f->ox_oy);
      pc->v_cmp_gt_i32(vMsk1, f->qx_qy, f->ox_oy);

      pc->v_and(vMsk0, vMsk0, f->rx_ry);
      pc->v_and(vMsk1, vMsk1, f->rx_ry);
      iExt.extract(texPtr0, 0);

      pc->v_sub_i32(f->px_py, f->px_py, vMsk0);
      pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk1);
      iExt.extract(texPtr1, 1);

      pc->v_shuffle_i32(vIdx, f->px_py, f->qx_qy, x86::shuffleImm(3, 1, 3, 1));
      cc->add(texPtr0, f->srctop);
      cc->add(texPtr1, f->srctop);

      pc->v_add_i64(f->px_py, f->px_py, f->xx2_xy2);
      pc->v_add_i64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      fCtx.fetchPixel(x86::ptr(texPtr0));
      iExt.extract(texPtr0, 2);

      pc->v_cmp_gt_i32(vMsk0, f->px_py, f->ox_oy);
      pc->v_cmp_gt_i32(vMsk1, f->qx_qy, f->ox_oy);

      fCtx.fetchPixel(x86::ptr(texPtr1));
      iExt.extract(texPtr1, 3);

      pc->v_and(vMsk0, vMsk0, f->rx_ry);
      pc->v_and(vMsk1, vMsk1, f->rx_ry);
      cc->add(texPtr0, f->srctop);

      pc->v_sub_i32(f->px_py, f->px_py, vMsk0);
      pc->v_sub_i32(f->qx_qy, f->qx_qy, vMsk1);
      pc->v_shuffle_i32(vMsk0, f->px_py, f->qx_qy, x86::shuffleImm(3, 1, 3, 1));
      cc->add(texPtr1, f->srctop);

      pc->v_packs_i32_i16(vIdx, vIdx, vMsk0);
      fCtx.fetchPixel(x86::ptr(texPtr0));

      pc->v_max_i16(vIdx, vIdx, f->minx_miny);
      fCtx.fetchPixel(x86::ptr(texPtr1));

      pc->v_min_i16(vIdx, vIdx, f->maxx_maxy);
      fCtx.end();

      pc->v_sra_i16(vMsk0, vIdx, 15);
      pc->v_xor(vIdx, vIdx, vMsk0);

      pc->xSatisfyPixel(p, flags);
      break;
    }
  }
}

void FetchAffinePatternPart::clampVIdx32(x86::Xmm& dst, const x86::Xmm& src, uint32_t step) noexcept {
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
      if (pc->hasSSE4_1()) {
        pc->v_max_i32_(dst, src, f->minx_miny);
      }
      else {
        if (dst.id() == src.id()) {
          x86::Xmm tmp = cc->newXmm("vIdxPad");
          pc->v_mov(tmp, dst);
          pc->v_cmp_gt_i32(dst, dst, f->minx_miny); // `-1` if `src` is greater than `minx_miny`.
          pc->v_and(dst, dst, tmp);                 // Changes `dst` to `0` if clamped.
        }
        else {
          pc->v_mov(dst, src);
          pc->v_cmp_gt_i32(dst, dst, f->minx_miny); // `-1` if `src` is greater than `minx_miny`.
          pc->v_and(dst, dst, src);                 // Changes `dst` to `0` if clamped.
        }
      }
      break;
    }

    // Step B - Handle a possible overflow (PAD | Bilinear overflow).
    case kClampStepB_NN:
    case kClampStepB_BI: {
      // Always performed on the same register.
      BL_ASSERT(dst.id() == src.id());

      x86::Xmm tmp = cc->newXmm("vIdxMsk1");
      if (pc->hasSSE4_1()) {
        pc->v_cmp_gt_i32(tmp, dst, f->maxx_maxy);
        pc->v_blendv_u8_(dst, dst, f->corx_cory, tmp);
      }
      else {
        // Blend(a, b, cond) == a ^ ((a ^ b) &  cond)
        //                   == b ^ ((a ^ b) & ~cond)
        pc->v_xor(tmp, dst, f->corx_cory);
        pc->v_cmp_gt_i32(dst, dst, f->maxx_maxy);
        pc->v_nand(dst, dst, tmp);
        pc->v_xor(dst, dst, f->corx_cory);
      }
      break;
    }

    // Step C - Handle a possible reflection (RoR).
    case kClampStepC_NN:
    case kClampStepC_BI: {
      // Always performed on the same register.
      BL_ASSERT(dst.id() == src.id());

      x86::Xmm tmp = cc->newXmm("vIdxRoR");
      pc->v_sra_i32(tmp, dst, 31);
      pc->v_xor(dst, dst, tmp);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

} // {JIT}
} // {BLPipeline}

#endif
