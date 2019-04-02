// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../pipegen/blcompoppart_p.h"
#include "../pipegen/blfetchpatternpart_p.h"
#include "../pipegen/blfetchutils_p.h"
#include "../pipegen/blpipecompiler_p.h"

namespace BLPipeGen {

#define REL_PATTERN(FIELD) BL_OFFSET_OF(BLPipeFetchData::Pattern, FIELD)

// ============================================================================
// [BLPipeGen::FetchPatternPart - Construction / Destruction]
// ============================================================================

FetchPatternPart::FetchPatternPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : FetchPart(pc, fetchType, fetchPayload, format) {}

// ============================================================================
// [BLPipeGen::FetchSimplePatternPart - Construction / Destruction]
// ============================================================================

FetchSimplePatternPart::FetchSimplePatternPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : FetchPatternPart(pc, fetchType, fetchPayload, format),
    _extendX(0) {

  _maxPixels = 4;
  _maxOptLevelSupported = kOptLevel_X86_AVX;

  static const uint8_t aaExtendTable[] = {
    BL_PIPE_EXTEND_MODE_PAD,
    BL_PIPE_EXTEND_MODE_REPEAT,
    BL_PIPE_EXTEND_MODE_ROR
  };

  static const uint8_t auExtendTable[] = {
    BL_PIPE_EXTEND_MODE_PAD,
    BL_PIPE_EXTEND_MODE_ROR
  };

  // Setup persistent and temporary registers, extend mode, and the maximum
  // number of pixels that can be fetched at once.
  switch (fetchType) {
    case BL_PIPE_FETCH_TYPE_PATTERN_AA_BLIT:
      _maxPixels = 8;
      _persistentRegs[x86::Reg::kGroupGp] = 1;
      break;

    case BL_PIPE_FETCH_TYPE_PATTERN_AA_PAD:
      _maxPixels = 8;
    case BL_PIPE_FETCH_TYPE_PATTERN_AA_REPEAT:
    case BL_PIPE_FETCH_TYPE_PATTERN_AA_ROR:
      _extendX = aaExtendTable[fetchType - BL_PIPE_FETCH_TYPE_PATTERN_AA_PAD];
      _persistentRegs[x86::Reg::kGroupGp] = 3;
      break;

    case BL_PIPE_FETCH_TYPE_PATTERN_FX_PAD:
    case BL_PIPE_FETCH_TYPE_PATTERN_FX_ROR:
      _extendX = auExtendTable[fetchType - BL_PIPE_FETCH_TYPE_PATTERN_FX_PAD];
      _persistentRegs[x86::Reg::kGroupGp] = 3;
      _persistentRegs[x86::Reg::kGroupVec] = 1;
      break;

    case BL_PIPE_FETCH_TYPE_PATTERN_FY_PAD:
    case BL_PIPE_FETCH_TYPE_PATTERN_FY_ROR:
      _extendX = auExtendTable[fetchType - BL_PIPE_FETCH_TYPE_PATTERN_FY_PAD];
      _persistentRegs[x86::Reg::kGroupGp] = 3;
      break;

    case BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_PAD:
    case BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_ROR:
      _extendX = auExtendTable[fetchType - BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_PAD];
      _isComplexFetch = true;
      _persistentRegs[x86::Reg::kGroupGp] = 3;
      _persistentRegs[x86::Reg::kGroupVec] = 2;
      break;

    default:
      BL_NOT_REACHED();
  }

  JitUtils::resetVarStruct(&f, sizeof(f));
}

// ============================================================================
// [BLPipeGen::FetchSimplePatternPart - Init / Fini]
// ============================================================================

void FetchSimplePatternPart::_initPart(x86::Gp& x, x86::Gp& y) noexcept {
  if (isBlitA()) {
    // This is a special-case designed only for rectangular blits, the engine
    // pre-translates the coordinates so it doesn't have to do anything but
    // fetch pixels.
    BL_ASSERT(isRectFill());

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    f->stride       = cc->newIntPtr("f.stride");      // Mem.
    f->srcp1        = cc->newIntPtr("f.srcp1");       // Reg.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    cc->mov(f->srcp1, x86::ptr(pc->_fetchData, REL_PATTERN(src.pixelData)));
    cc->mov(f->stride.r32(), x86::ptr(pc->_fetchData, REL_PATTERN(src.size.w)));
    pc->uPrefetch(x86::ptr(f->srcp1));

    pc->uMulImm(f->stride, f->stride, -int(bpp()));
    cc->add(f->stride, x86::ptr(pc->_fetchData, REL_PATTERN(src.stride)));
    cc->spill(f->stride);
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
    cc->spill(f->srctop);

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

    // if (ry == 0) {
    //   {Vectical-Pad}
    // }
    // else {
    //   {Vectical-Repeat|Reflect}
    // }
    Label L_VertPadA    = cc->newLabel();
    Label L_VertPadB    = cc->newLabel();
    Label L_VertRoR     = cc->newLabel();
    Label L_VertReflect = cc->newLabel();
    Label L_VertDone    = cc->newLabel();

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
    cc->spill(f->stride);

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
    cc->spill(f->stride);

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

    cc->spill(f->h);
    cc->spill(f->ry);
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
    if (extendX() == BL_PIPE_EXTEND_MODE_PAD) {
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      f->x          = cc->newInt32("f.x");            // Reg.
      f->xPadded    = cc->newIntPtr("f.xPadded");     // Reg.
      f->xOrigin    = cc->newInt32("f.xOrigin");      // Mem.
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      cc->mov(f->w      , x86::ptr(pc->_fetchData, REL_PATTERN(src.size.w)));
      cc->mov(f->xOrigin, x86::ptr(pc->_fetchData, REL_PATTERN(simple.tx)));

      if (isRectFill())
        cc->add(f->xOrigin, x);

      cc->spill(f->xOrigin);
      cc->dec(f->w);
      cc->spill(f->w);
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
    if (extendX() == BL_PIPE_EXTEND_MODE_REPEAT) {
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

      pc->uMulImm(f->w      , f->w      , int(bpp()));
      pc->uMulImm(f->xOrigin, f->xOrigin, int(bpp()));

      cc->sub(f->xOrigin, f->w.cloneAs(f->xOrigin));
      cc->spill(f->xOrigin);

      cc->add(f->srcp1 , f->w.cloneAs(f->srcp1));
      cc->add(f->srctop, f->w.cloneAs(f->srctop));

      cc->mov(f->xRestart.r32(), f->w);
      cc->spill(f->w);

      cc->neg(f->xRestart);
      cc->spill(f->xRestart);
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
    if (extendX() == BL_PIPE_EXTEND_MODE_ROR) {
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
        pc->vmovsi32(f->xMax4, f->w);
        pc->vswizi32(f->xMax4, f->xMax4, x86::Predicate::shuf(0, 0, 0, 0));
        cc->spill(f->xMax4);
        cc->inc(f->w);

        pc->vmovu8u32(f->xSet4, x86::ptr(pc->_fetchData, REL_PATTERN(simple.ix)));
        pc->vswizi32(f->xInc4, f->xSet4, x86::Predicate::shuf(3, 3, 3, 3));
        pc->vslli128b(f->xSet4, f->xSet4, 4);

        cc->spill(f->xInc4);
        cc->spill(f->xSet4);
      }

      cc->mov(f->xRestart, f->w);
      cc->spill(f->w);
      cc->sub(f->xRestart, f->rx);

      if (maxPixels() >= 4) {
        pc->vmovsi32(f->xNrm4, f->rx);
        pc->vswizi32(f->xNrm4, f->xNrm4, x86::Predicate::shuf(0, 0, 0, 0));
        cc->spill(f->xNrm4);
      }

      cc->spill(f->xRestart);
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

      cc->spill(f->rx);
      cc->spill(f->xOrigin);
    }

    // Fractional - Fx|Fy|FxFy
    // =======================
    if (isPatternF()) {
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      f->pixL       = cc->newXmm("f.pixL");           // Reg (Fx|FxFy).
      f->wb_wb      = cc->newXmm("f.wb_wb");          // Mem.
      f->wd_wd      = cc->newXmm("f.wd_wd");          // Mem.
      f->wc_wd      = cc->newXmm("f.wc_wd");          // Mem.
      f->wa_wb      = cc->newXmm("f.wa_wb");          // Mem.
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      x86::Xmm weights = cc->newXmm("weights");

      pc->vloadi128u(weights, x86::ptr(pc->_fetchData, REL_PATTERN(simple.wa))); // [00 Wd 00 Wc 00 Wb 00 Wa]
      pc->vpacki32i16(weights, weights, weights);                                // [Wd Wc Wb Wa Wd Wc Wb Wa]
      pc->vunpackli16(weights, weights, weights);                                // [Wd Wd Wc Wc Wb Wb Wa Wa]

      if (isPatternFx()) {
        pc->vswizi32(f->wc_wd, weights, x86::Predicate::shuf(2, 2, 3, 3));
      }
      else if (isPatternFy()) {
        pc->vswizi32(f->wb_wb, weights, x86::Predicate::shuf(1, 1, 1, 1));
        pc->vswizi32(f->wd_wd, weights, x86::Predicate::shuf(3, 3, 3, 3));
      }
      else if (isPatternFxFy()) {
        pc->vswizi32(f->wa_wb, weights, x86::Predicate::shuf(0, 0, 1, 1));
        pc->vswizi32(f->wc_wd, weights, x86::Predicate::shuf(2, 2, 3, 3));
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

// ============================================================================
// [BLPipeGen::FetchSimplePatternPart - Advance]
// ============================================================================

void FetchSimplePatternPart::advanceY() noexcept {
  if (isBlitA()) {
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
    cc->spill(f->stride);                                  // }
    cc->jmp(L_VertDone);

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
  if (isBlitA()) {
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

    if (extendX() == BL_PIPE_EXTEND_MODE_PAD) {
      if (!isRectFill())
        cc->add(f->x, x);
      pc->uBound0ToN(f->xPadded.r32(), f->x, f->w);
    }

    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    if (extendX() == BL_PIPE_EXTEND_MODE_REPEAT) {
      if (!isRectFill()) {                                 // if (!RectFill) {
        pc->uAddMulImm(f->x, x, int(bpp()));               //   f->x += x * pattern.bpp;
        repeatOrReflectX();                                //   f->x = repeatLarge(f->x);
      }                                                    // }
    }

    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    if (extendX() == BL_PIPE_EXTEND_MODE_ROR) {
      if (!isRectFill()) {                                 // if (!RectFill) {
        cc->add(f->x, x);                                  //   f->x += x;
        repeatOrReflectX();                                //   f->x = repeatOrReflect(f->x);
      }                                                    // }
    }
  }

  if (hasFracX())
    prefetchAccX();

  if (pixelGranularity() > 1)
    enterN();
}

void FetchSimplePatternPart::advanceX(x86::Gp& x, x86::Gp& diff) noexcept {
  BL_UNUSED(x);

  x86::Gp fx32 = f->x.r32();

  if (pixelGranularity() > 1)
    leaveN();

  if (isBlitA()) {
    // Blit AA
    // -------

    pc->uAddMulImm(f->srcp1, diff.cloneAs(f->srcp1), int(bpp()));
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_PAD) {
    // Horizontal Pad
    // --------------

    if (hasFracX())                                        // if (hasFracX())
      cc->lea(fx32, x86::ptr(f->x.r32(), diff, 0, -1));    //   f->x += diff - 1;
    else                                                   // else
      cc->add(fx32, diff);                                 //   f->x += diff;

    pc->uBound0ToN(f->xPadded.r32(), f->x, f->w);
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_REPEAT) {
    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    pc->uAddMulImm(f->x, diff, int(bpp()));                // f->x += diff * pattern.bpp;
    repeatOrReflectX();                                    // f->x = repeatLarge(f->x);
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_ROR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    if (hasFracX())                                        // if (hasFracX())
      cc->lea(fx32, x86::ptr(fx32, diff, 0, -1));          //   f->x += diff - 1;
    else                                                   // else
      cc->add(fx32, diff);                                 //   f->x += diff;

    repeatOrReflectX();                                    // f->x = repeatOrReflect(f->x);
  }

  if (hasFracX())
    prefetchAccX();

  if (pixelGranularity() > 1)
    enterN();
}

void FetchSimplePatternPart::advanceXByOne() noexcept {
  if (isBlitA()) {
    // Blit AA
    // -------

    cc->add(f->srcp1, int(bpp()));
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_PAD) {
    // Horizontal Pad
    // --------------

    cc->inc(f->x);
    cc->cmp(f->x, f->w);
    cc->cmovbe(f->xPadded.r32(), f->x);
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_REPEAT) {
    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    cc->add(f->x, int(bpp()));
    cc->cmovz(f->x, f->xRestart);
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_ROR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    cc->inc(f->x);
    cc->cmp(f->x, f->w);
    cc->cmovz(f->x, f->xRestart);
  }
}

void FetchSimplePatternPart::repeatOrReflectX() noexcept {
  if (isBlitA()) {
    // Blit AA
    // -------

    // Nothing...
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_REPEAT) {
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
  else if (extendX() == BL_PIPE_EXTEND_MODE_ROR) {
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
  BL_ASSERT(hasFracX());

  x86::Gp idx;
  int shift = 0;

  // Horizontal Pad
  // --------------
  if (extendX() == BL_PIPE_EXTEND_MODE_PAD) {
    idx = f->xPadded;
    shift = 2;
  }

  // Horizontal Repeat - AA-Only, Large Fills
  // ----------------------------------------
  if (extendX() == BL_PIPE_EXTEND_MODE_REPEAT) {
    idx = f->x;
  }

  // Horizontal RoR [Repeat or Reflect]
  // ----------------------------------
  if (extendX() == BL_PIPE_EXTEND_MODE_ROR) {
    idx = cc->newIntPtr("@idx");
    shift = 2;
    pc->uReflect(idx.r32(), f->x);
  }

  if (!hasFracY()) {
    pc->vloadi32(f->pixL, x86::dword_ptr(f->srcp1, idx, shift));
    pc->vmovu8u16(f->pixL, f->pixL);
    pc->vmuli16(f->pixL, f->pixL, f->wc_wd);
  }
  else {
    x86::Xmm pixL = f->pixL;
    x86::Xmm pixT = cc->newXmm("@pixT");

    pc->vloadi32(pixL, x86::dword_ptr(f->srcp0, idx, shift));
    pc->vloadi32(pixT, x86::dword_ptr(f->srcp1, idx, shift));

    pc->vmovu8u16(pixL, pixL);
    pc->vmovu8u16(pixT, pixT);

    pc->vmuli16(pixL, pixL, f->wa_wb);
    pc->vmuli16(pixT, pixT, f->wc_wd);

    pc->vaddi16(pixL, pixL, pixT);
  }

  advanceXByOne();
}

// ============================================================================
// [BLPipeGen::FetchSimplePatternPart - Fetch]
// ============================================================================

void FetchSimplePatternPart::fetch1(PixelARGB& p, uint32_t flags) noexcept {
  if (isBlitA()) {
    // Blit AA
    // -------

    pc->xFetchARGB32_1x(p, flags, x86::ptr(f->srcp1), 4);
    advanceXByOne();
  }
  else {
    // Pattern AA or Fx/Fy
    // -------------------

    x86::Gp idx;
    int shift = 0;

    if (extendX() == BL_PIPE_EXTEND_MODE_PAD) {
      idx = f->xPadded;
      shift = 2;
    }

    if (extendX() == BL_PIPE_EXTEND_MODE_REPEAT) {
      idx = f->x;
    }

    if (extendX() == BL_PIPE_EXTEND_MODE_ROR) {
      idx = cc->newIntPtr("@idx");
      pc->uReflect(idx.r32(), f->x);
      shift = 2;
    }

    if (isPatternA()) {
      pc->xFetchARGB32_1x(p, flags, x86::ptr(f->srcp1, idx, shift), 4);
      advanceXByOne();
    }
    else if (isPatternFy()) {
      x86::Xmm pix0 = cc->newXmm("@pix0");
      x86::Xmm pix1 = cc->newXmm("@pix1");

      pc->vloadi32(pix0, x86::ptr(f->srcp0, idx, shift));
      pc->vloadi32(pix1, x86::ptr(f->srcp1, idx, shift));

      pc->vmovu8u16(pix0, pix0);
      pc->vmovu8u16(pix1, pix1);

      pc->vmuli16(pix0, pix0, f->wb_wb);
      pc->vmuli16(pix1, pix1, f->wd_wd);

      advanceXByOne();

      pc->vaddi16(pix0, pix0, pix1);
      pc->vsrli16(pix0, pix0, 8);

      p.uc.init(pix0);
      pc->xSatisfyARGB32_1x(p, flags);
    }
    else if (isPatternFx()) {
      x86::Xmm pixL = f->pixL;
      x86::Xmm pix0 = cc->newXmm("@pix0");

      if (pc->hasSSE4_1()) {
        pc->vswapi64(pix0, pixL);
        pc->vloadi32_u8u32_(pixL, x86::ptr(f->srcp1, idx, shift));

        pc->vpacki32i16(pixL, pixL, pixL);
      }
      else {
        pc->vswapi64(pix0, pixL);
        pc->vloadi32(pixL, x86::ptr(f->srcp1, idx, shift));

        pc->vswizi32(pixL, pixL, x86::Predicate::shuf(0, 0, 0, 0));
        pc->vmovu8u16(pixL, pixL);
      }

      pc->vmuli16(pixL, pixL, f->wc_wd);
      advanceXByOne();

      pc->vaddi16(pix0, pix0, pixL);
      pc->vsrli16(pix0, pix0, 8);

      p.uc.init(pix0);
      pc->xSatisfyARGB32_1x(p, flags);
    }
    else {
      x86::Xmm pixL = f->pixL;
      x86::Xmm pixT = cc->newXmm("@pixT");
      x86::Xmm pix0 = cc->newXmm("@pix0");

      if (pc->hasSSE4_1()) {
        pc->vloadi32_u8u32_(pixT, x86::ptr(f->srcp1, idx, shift));
        pc->vswapi64(pix0, pixL);
        pc->vloadi32_u8u32_(pixL, x86::ptr(f->srcp0, idx, shift));

        pc->vpacki32i16(pixT, pixT, pixT);
        pc->vpacki32i16(pixL, pixL, pixL);
      }
      else {
        pc->vloadi32(pixT, x86::ptr(f->srcp1, idx, shift));
        pc->vswapi64(pix0, pixL);
        pc->vloadi32(pixL, x86::ptr(f->srcp0, idx, shift));

        pc->vswizi32(pixT, pixT, x86::Predicate::shuf(0, 0, 0, 0));
        pc->vswizi32(pixL, pixL, x86::Predicate::shuf(0, 0, 0, 0));

        pc->vmovu8u16(pixT, pixT);
        pc->vmovu8u16(pixL, pixL);
      }

      pc->vmuli16(pixT, pixT, f->wc_wd);
      pc->vmuli16(pixL, pixL, f->wa_wb);

      advanceXByOne();

      pc->vaddi16(pixL, pixL, pixT);
      pc->vaddi16(pix0, pix0, pixL);
      pc->vsrli16(pix0, pix0, 8);

      p.uc.init(pix0);
      pc->xSatisfyARGB32_1x(p, flags);
    }
  }
}

void FetchSimplePatternPart::enterN() noexcept {
  if (isBlitA()) {
    // Blit AA
    // -------

    // Nothing...
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_PAD) {
    // Horizontal Pad
    // --------------

    // Nothing...
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_ROR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    x86::Xmm xFix4 = cc->newXmm("@xFix4");

    pc->vmovsi32(f->xVec4, f->x.r32());
    pc->vswizi32(f->xVec4, f->xVec4, x86::Predicate::shuf(0, 0, 0, 0));
    pc->vaddi32(f->xVec4, f->xVec4, f->xSet4);

    pc->vcmpgti32(xFix4, f->xVec4, f->xMax4);
    pc->vand(xFix4, xFix4, f->xNrm4);
    pc->vsubi32(f->xVec4, f->xVec4, xFix4);
  }
}

void FetchSimplePatternPart::leaveN() noexcept {
  if (isBlitA()) {
    // Blit AA
    // -------

    // Nothing...
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_PAD) {
    // Horizontal Pad
    // --------------

    // Nothing...
  }
  else if (extendX() == BL_PIPE_EXTEND_MODE_ROR) {
    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    pc->vmovsi32(f->x.r32(), f->xVec4);
  }
}

void FetchSimplePatternPart::prefetchN() noexcept {}
void FetchSimplePatternPart::postfetchN() noexcept {}

void FetchSimplePatternPart::fetch4(PixelARGB& p, uint32_t flags) noexcept {
  if (isBlitA()) {
    // Blit AA
    // -------

    pc->xFetchARGB32_4x(p, flags, x86::ptr(f->srcp1), 4);
    cc->add(f->srcp1, int(4 * bpp()));
  }
  else {
    // Horizontal Pad
    // --------------

    if (extendX() == BL_PIPE_EXTEND_MODE_PAD) {
      if (isPatternA()) {
        FetchContext4X fCtx(pc, &p, flags);
        int shift = 2;

        x86::Gp idx = f->xPadded;
        x86::Mem mem = x86::ptr(f->srcp1, idx, shift);

        cc->inc(f->x);
        cc->cmp(f->x, f->w);
        fCtx.fetchARGB32(mem);
        cc->cmovbe(idx.r32(), f->x);

        cc->inc(f->x);
        cc->cmp(f->x, f->w);
        fCtx.fetchARGB32(mem);
        cc->cmovbe(idx.r32(), f->x);

        cc->inc(f->x);
        cc->cmp(f->x, f->w);
        fCtx.fetchARGB32(mem);
        cc->cmovbe(idx.r32(), f->x);

        cc->inc(f->x);
        cc->cmp(f->x, f->w);
        fCtx.fetchARGB32(mem);
        cc->cmovbe(idx.r32(), f->x);

        fCtx.end();
        pc->xSatisfyARGB32_Nx(p, flags);
      }

      if (isPatternFy()) {
        PixelARGB pix0;
        PixelARGB pix1;

        FetchContext4X fCtx0(pc, &pix0, PixelARGB::kUC);
        FetchContext4X fCtx1(pc, &pix1, PixelARGB::kUC);

        x86::Gp idx = f->xPadded;
        int shift = 2;

        x86::Mem m0 = x86::ptr(f->srcp0, idx, shift);
        x86::Mem m1 = x86::ptr(f->srcp1, idx, shift);

        cc->inc(f->x);
        cc->cmp(f->x, f->w);
        fCtx0.fetchARGB32(m0);
        fCtx1.fetchARGB32(m1);
        cc->cmovbe(idx.r32(), f->x);

        cc->inc(f->x);
        cc->cmp(f->x, f->w);
        fCtx0.fetchARGB32(m0);
        fCtx1.fetchARGB32(m1);
        cc->cmovbe(idx.r32(), f->x);

        cc->inc(f->x);
        cc->cmp(f->x, f->w);
        fCtx0.fetchARGB32(m0);
        fCtx1.fetchARGB32(m1);
        cc->cmovbe(idx.r32(), f->x);

        cc->inc(f->x);
        cc->cmp(f->x, f->w);
        fCtx0.fetchARGB32(m0);
        fCtx1.fetchARGB32(m1);
        fCtx0.end();
        fCtx1.end();

        pc->vmuli16(pix0.uc, pix0.uc, f->wb_wb);
        pc->vmuli16(pix1.uc, pix1.uc, f->wd_wd);

        cc->cmovbe(idx.r32(), f->x);
        pc->vaddi16(pix0.uc, pix0.uc, pix1.uc);
        pc->vsrli16(pix0.uc, pix0.uc, 8);

        p.uc.init(pix0.uc[0], pix0.uc[1]);
        pc->xSatisfyARGB32_Nx(p, flags);
      }

      if (isPatternFx()) {
        x86::Gp idx = f->xPadded;
        int shift = 2;

        x86::Mem m = x86::ptr(f->srcp1, idx, shift);

        x86::Xmm pixL = f->pixL;
        x86::Xmm pixT = cc->newXmm("@pixT");

        x86::Xmm pix0 = cc->newXmm("@pix0");
        x86::Xmm pix1 = cc->newXmm("@pix1");
        x86::Xmm pix2 = cc->newXmm("@pix2");

        if (pc->hasSSE4_1()) {
          cc->inc(f->x);
          cc->cmp(f->x, f->w);
          pc->vloadi32_u8u32_(pix0, m);
          cc->cmovbe(idx.r32(), f->x);

          cc->inc(f->x);
          cc->cmp(f->x, f->w);
          pc->vloadi32_u8u32_(pix1, m);
          cc->cmovbe(idx.r32(), f->x);

          pc->vpacki32i16(pix0, pix0, pix0);
          pc->vpacki32i16(pix1, pix1, pix1);

          pc->vmuli16(pix0, pix0, f->wc_wd);
          pc->vmuli16(pix1, pix1, f->wc_wd);

          cc->inc(f->x);
          cc->cmp(f->x, f->w);
          pc->vloadi32_u8u32_(pix2, m);
          cc->cmovbe(idx.r32(), f->x);

          pc->vcombhli64(pixT, pixL, pix1);
          pc->vloadi32_u8u32_(pixL, m);

          pc->vpacki32i16(pix2, pix2, pix2);
          pc->vpacki32i16(pixL, pixL, pixL);
        }
        else {
          cc->inc(f->x);
          cc->cmp(f->x, f->w);
          pc->vloadi32(pix0, m);
          cc->cmovbe(idx.r32(), f->x);

          pc->vswizi32(pix0, pix0, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vloadi32(pix1, m);
          cc->inc(f->x);
          pc->vswizi32(pix1, pix1, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vmovu8u16(pix0, pix0);
          cc->cmp(f->x, f->w);
          pc->vmovu8u16(pix1, pix1);
          cc->cmovbe(idx.r32(), f->x);

          pc->vmuli16(pix0, pix0, f->wc_wd);
          pc->vmuli16(pix1, pix1, f->wc_wd);
          cc->inc(f->x);
          cc->cmp(f->x, f->w);
          pc->vloadi32(pix2, m);
          cc->cmovbe(idx.r32(), f->x);

          pc->vswizi32(pix2, pix2, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vcombhli64(pixT, pixL, pix1);
          pc->vloadi32(pixL, m);

          pc->vmovu8u16(pix2, pix2);
          pc->vswizi32(pixL, pixL, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vmovu8u16(pixL, pixL);
        }

        pc->vaddi16(pix0, pix0, pixT);

        pc->vmuli16(pixL, pixL, f->wc_wd);
        pc->vmuli16(pix2, pix2, f->wc_wd);
        pc->vsrli16(pix0, pix0, 8);
        cc->inc(f->x);

        pc->vcombhli64(pix1, pix1, pixL);
        cc->cmp(f->x, f->w);
        pc->vaddi16(pix2, pix2, pix1);
        cc->cmovbe(idx.r32(), f->x);
        pc->vsrli16(pix2, pix2, 8);

        p.uc.init(pix0, pix2);
        pc->xSatisfyARGB32_Nx(p, flags);
      }

      if (isPatternFxFy()) {
        x86::Gp idx = f->xPadded;
        int shift = 2;

        x86::Mem mA = x86::ptr(f->srcp0, idx, shift);
        x86::Mem mB = x86::ptr(f->srcp1, idx, shift);

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
          pc->vloadi32_u8u32_(pix0, mA);
          pc->vloadi32_u8u32_(pix0t, mB);
          cc->cmovbe(idx.r32(), f->x);

          pc->vloadi32_u8u32_(pix1, mA);
          pc->vloadi32_u8u32_(pix1t, mB);
          cc->inc(f->x);
          pc->vpacki32i16(pix0, pix0, pix0);
          pc->vpacki32i16(pix0t, pix0t, pix0t);
          cc->cmp(f->x, f->w);
          pc->vpacki32i16(pix1, pix1, pix1);
          pc->vpacki32i16(pix1t, pix1t, pix1t);
          cc->cmovbe(idx.r32(), f->x);

          pc->vmuli16(pix1 , pix1 , f->wa_wb);
          pc->vmuli16(pix1t, pix1t, f->wc_wd);
          pc->vmuli16(pix0 , pix0 , f->wa_wb);
          pc->vmuli16(pix0t, pix0t, f->wc_wd);
          cc->inc(f->x);
          cc->cmp(f->x, f->w);

          pc->vaddi16(pix1, pix1, pix1t);
          pc->vloadi32_u8u32_(pix2, mA);
          pc->vaddi16(pix0, pix0, pix0t);
          pc->vloadi32_u8u32_(pix2t, mB);
          cc->cmovbe(idx.r32(), f->x);

          pc->vcombhli64(pixT, pixL, pix1);
          pc->vloadi32_u8u32_(pixL, mA);
          pc->vaddi16(pix0, pix0, pixT);
          pc->vloadi32_u8u32_(pixT, mB);

          pc->vpacki32i16(pixL, pixL, pixL);
          pc->vpacki32i16(pix2 , pix2, pix2);
          pc->vpacki32i16(pix2t, pix2t, pix2t);
          pc->vmuli16(pixL, pixL, f->wa_wb);
          pc->vpacki32i16(pixT, pixT, pixT);
        }
        else {
          pc->vloadi32(pix0, mA);
          pc->vloadi32(pix0t, mB);
          cc->cmovbe(idx.r32(), f->x);

          pc->vswizi32(pix0 , pix0 , x86::Predicate::shuf(0, 0, 0, 0));
          pc->vswizi32(pix0t, pix0t, x86::Predicate::shuf(0, 0, 0, 0));

          pc->vloadi32(pix1, mA);
          pc->vloadi32(pix1t, mB);
          cc->inc(f->x);
          pc->vswizi32(pix1 , pix1 , x86::Predicate::shuf(0, 0, 0, 0));
          pc->vswizi32(pix1t, pix1t, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vmovu8u16(pix0, pix0);
          pc->vmovu8u16(pix0t, pix0t);
          cc->cmp(f->x, f->w);
          pc->vmovu8u16(pix1, pix1);
          pc->vmovu8u16(pix1t, pix1t);
          cc->cmovbe(idx.r32(), f->x);

          pc->vmuli16(pix1 , pix1 , f->wa_wb);
          pc->vmuli16(pix1t, pix1t, f->wc_wd);
          pc->vmuli16(pix0 , pix0 , f->wa_wb);
          pc->vmuli16(pix0t, pix0t, f->wc_wd);
          cc->inc(f->x);
          cc->cmp(f->x, f->w);

          pc->vaddi16(pix1, pix1, pix1t);
          pc->vloadi32(pix2, mA);
          pc->vaddi16(pix0, pix0, pix0t);
          pc->vloadi32(pix2t, mB);
          cc->cmovbe(idx.r32(), f->x);

          pc->vswizi32(pix2 , pix2 , x86::Predicate::shuf(0, 0, 0, 0));
          pc->vswizi32(pix2t, pix2t, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vcombhli64(pixT, pixL, pix1);
          pc->vloadi32(pixL, mA);
          pc->vaddi16(pix0, pix0, pixT);
          pc->vloadi32(pixT, mB);

          pc->vmovu8u16(pix2 , pix2);
          pc->vswizi32(pixL, pixL, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vmovu8u16(pix2t, pix2t);
          pc->vmovu8u16(pixL, pixL);
          pc->vswizi32(pixT, pixT, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vmuli16(pixL, pixL, f->wa_wb);
          pc->vmovu8u16(pixT, pixT);
        }

        pc->vmuli16(pix2, pix2, f->wa_wb);
        pc->vmuli16(pixT, pixT, f->wc_wd);
        pc->vmuli16(pix2t, pix2t, f->wc_wd);
        pc->vsrli16(pix0, pix0, 8);

        pc->vaddi16(pixL, pixL, pixT);
        pc->vaddi16(pix2, pix2, pix2t);
        cc->inc(f->x);
        pc->vcombhli64(pix1, pix1, pixL);
        cc->cmp(f->x, f->w);
        pc->vaddi16(pix2, pix2, pix1);
        cc->cmovbe(idx.r32(), f->x);
        pc->vsrli16(pix2, pix2, 8);

        p.uc.init(pix0, pix2);
        pc->xSatisfyARGB32_Nx(p, flags);
      }
    }

    // Horizontal Repeat - AA-Only, Large Fills
    // ----------------------------------------

    if (extendX() == BL_PIPE_EXTEND_MODE_REPEAT) {
      // Only generated for AA patterns.
      BL_ASSERT(isPatternA());

      FetchContext4X fCtx(pc, &p, flags);

      int offset = int(4 * bpp());
      x86::Mem mem = x86::ptr(f->srcp1, f->x, 0, -offset);

      Label L_Repeat = cc->newLabel();
      Label L_Done = cc->newLabel();

      cc->add(f->x, offset);
      cc->jc(L_Repeat);

      if (flags & PixelARGB::kPC) {
        pc->vloadi128u_ro(p.pc[0], mem);
      }
      else {
        pc->vmovu8u16(p.uc[0], mem);
        pc->vmovu8u16(p.uc[1], mem.cloneAdjusted(8));
      }

      cc->bind(L_Done);

      {
        PipeInjectAtTheEnd injected(pc);
        cc->bind(L_Repeat);

        fCtx.fetchARGB32(mem);
        mem.addOffsetLo32(offset);

        cc->sub(f->x, offset - int(bpp()));
        cc->cmovz(f->x, f->xRestart);
        fCtx.fetchARGB32(mem);

        cc->add(f->x, int(bpp()));
        cc->cmovz(f->x, f->xRestart);
        fCtx.fetchARGB32(mem);

        cc->add(f->x, int(bpp()));
        cc->cmovz(f->x, f->xRestart);
        fCtx.fetchARGB32(mem);

        cc->add(f->x, int(bpp()));
        cc->cmovz(f->x, f->xRestart);
        fCtx.end();

        cc->jmp(L_Done);
      }

      pc->xSatisfyARGB32_Nx(p, flags);
    }

    // Horizontal RoR [Repeat or Reflect]
    // ----------------------------------

    if (extendX() == BL_PIPE_EXTEND_MODE_ROR) {
      x86::Xmm xIdx4 = cc->newXmm("@xIdx4");
      x86::Xmm xFix4 = cc->newXmm("@xFix4");

      if (isPatternA()) {
        FetchContext4X fCtx(pc, &p, flags);
        int shift = 2;

        pc->vsrai32(xIdx4, f->xVec4, 31);
        pc->vxor(xIdx4, xIdx4, f->xVec4);
        pc->vaddi32(f->xVec4, f->xVec4, f->xInc4);
        FetchUtils::fetchARGB32_4x(&fCtx, x86::ptr(f->srcp1), xIdx4, shift);

        pc->vcmpgti32(xFix4, f->xVec4, f->xMax4);
        pc->vand(xFix4, xFix4, f->xNrm4);
        pc->vsubi32(f->xVec4, f->xVec4, xFix4);

        fCtx.end();
        pc->xSatisfyARGB32_Nx(p, flags);
      }

      if (isPatternFy()) {
        PixelARGB pix0;
        PixelARGB pix1;

        FetchContext4X fCtx0(pc, &pix0, PixelARGB::kUC);
        FetchContext4X fCtx1(pc, &pix1, PixelARGB::kUC);

        int shift = 2;

        pc->vsrai32(xIdx4, f->xVec4, 31);
        pc->vxor(xIdx4, xIdx4, f->xVec4);
        pc->vaddi32(f->xVec4, f->xVec4, f->xInc4);
        FetchUtils::fetchARGB32_4x_twice(&fCtx0, x86::ptr(f->srcp0), &fCtx1, x86::ptr(f->srcp1), xIdx4, shift);

        fCtx0.end();
        fCtx1.end();

        pc->vmuli16(pix0.uc, pix0.uc, f->wb_wb);
        pc->vcmpgti32(xFix4, f->xVec4, f->xMax4);
        pc->vmuli16(pix1.uc, pix1.uc, f->wd_wd);

        pc->vand(xFix4, xFix4, f->xNrm4);
        pc->vaddi16(pix0.uc, pix0.uc, pix1.uc);

        pc->vsubi32(f->xVec4, f->xVec4, xFix4);
        pc->vsrli16(pix0.uc, pix0.uc, 8);

        p.uc.init(pix0.uc[0], pix0.uc[1]);
        pc->xSatisfyARGB32_Nx(p, flags);
      }

      if (isPatternFx()) {
        IndexExtractorU32 iExt(pc);

        x86::Gp idx0 = cc->newIntPtr("@idx0");
        x86::Gp idx1 = cc->newIntPtr("@idx1");
        int shift = 2;

        x86::Xmm pixL = f->pixL;
        x86::Xmm pixT = cc->newXmm("@pixT");

        x86::Xmm pix0 = cc->newXmm("@pix0");
        x86::Xmm pix1 = cc->newXmm("@pix1");
        x86::Xmm pix2 = cc->newXmm("@pix2");

        pc->vsrai32(xIdx4, f->xVec4, 31);
        pc->vxor(xIdx4, xIdx4, f->xVec4);
        iExt.begin(xIdx4);

        pc->vaddi32(f->xVec4, f->xVec4, f->xInc4);
        iExt.extract(idx0, 0);

        pc->vcmpgti32(xFix4, f->xVec4, f->xMax4);
        iExt.extract(idx1, 1);
        pc->vand(xFix4, xFix4, f->xNrm4);

        if (pc->hasSSE4_1()) {
          pc->vloadi32_u8u32_(pix0, x86::ptr(f->srcp1, idx0, shift));
          iExt.extract(idx0, 2);

          pc->vloadi32_u8u32_(pix1, x86::ptr(f->srcp1, idx1, shift));
          iExt.extract(idx1, 3);

          pc->vsubi32(f->xVec4, f->xVec4, xFix4);
          pc->vpacki32i16(pix0, pix0, pix0);
          pc->vpacki32i16(pix1, pix1, pix1);

          pc->vmuli16(pix1, pix1, f->wc_wd);
          pc->vmuli16(pix0, pix0, f->wc_wd);
          pc->vloadi32_u8u32_(pix2, x86::ptr(f->srcp1, idx0, shift));
          pc->vcombhli64(pixT, pixL, pix1);

          pc->vloadi32_u8u32_(pixL, x86::ptr(f->srcp1, idx1, shift));
          pc->vpacki32i16(pix2, pix2, pix2);
          pc->vpacki32i16(pixL, pixL, pixL);
        }
        else {
          pc->vloadi32(pix0, x86::ptr(f->srcp1, idx0, shift));
          iExt.extract(idx0, 2);

          pc->vsubi32(f->xVec4, f->xVec4, xFix4);
          pc->vswizi32(pix0, pix0, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vloadi32(pix1, x86::ptr(f->srcp1, idx1, shift));
          iExt.extract(idx1, 3);

          pc->vswizi32(pix1, pix1, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vmovu8u16(pix0, pix0);
          pc->vmovu8u16(pix1, pix1);

          pc->vmuli16(pix1, pix1, f->wc_wd);
          pc->vmuli16(pix0, pix0, f->wc_wd);
          pc->vloadi32(pix2, x86::ptr(f->srcp1, idx0, shift));

          pc->vswizi32(pix2, pix2, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vcombhli64(pixT, pixL, pix1);
          pc->vloadi32(pixL, x86::ptr(f->srcp1, idx1, shift));

          pc->vmovu8u16(pix2, pix2);
          pc->vswizi32(pixL, pixL, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vmovu8u16(pixL, pixL);
        }

        pc->vaddi16(pix0, pix0, pixT);
        pc->vmuli16(pixL, pixL, f->wc_wd);
        pc->vmuli16(pix2, pix2, f->wc_wd);
        pc->vsrli16(pix0, pix0, 8);

        pc->vcombhli64(pix1, pix1, pixL);
        pc->vaddi16(pix2, pix2, pix1);
        pc->vsrli16(pix2, pix2, 8);

        p.uc.init(pix0, pix2);
        pc->xSatisfyARGB32_Nx(p, flags);
      }

      if (isPatternFxFy()) {
        IndexExtractorU32 iExt(pc);

        x86::Gp idx0 = cc->newIntPtr("@idx0");
        x86::Gp idx1 = cc->newIntPtr("@idx1");
        int shift = 2;

        x86::Xmm pixL = f->pixL;
        x86::Xmm pixT = cc->newXmm("@pixT");

        x86::Xmm pix0  = cc->newXmm("@pix0");
        x86::Xmm pix0t = cc->newXmm("@pix0t");
        x86::Xmm pix1  = cc->newXmm("@pix1");
        x86::Xmm pix1t = cc->newXmm("@pix1t");
        x86::Xmm pix2  = cc->newXmm("@pix2");
        x86::Xmm pix2t = cc->newXmm("@pix2t");

        pc->vsrai32(xIdx4, f->xVec4, 31);
        pc->vxor(xIdx4, xIdx4, f->xVec4);
        iExt.begin(xIdx4);

        pc->vaddi32(f->xVec4, f->xVec4, f->xInc4);
        iExt.extract(idx0, 0);

        pc->vcmpgti32(xFix4, f->xVec4, f->xMax4);
        iExt.extract(idx1, 1);
        pc->vand(xFix4, xFix4, f->xNrm4);

        if (pc->hasSSE4_1()) {
          pc->vloadi32_u8u32_(pix0 , x86::ptr(f->srcp0, idx0, shift));
          pc->vloadi32_u8u32_(pix0t, x86::ptr(f->srcp1, idx0, shift));
          iExt.extract(idx0, 2);
          pc->vsubi32(f->xVec4, f->xVec4, xFix4);

          pc->vloadi32_u8u32_(pix1 , x86::ptr(f->srcp0, idx1, shift));
          pc->vloadi32_u8u32_(pix1t, x86::ptr(f->srcp1, idx1, shift));
          iExt.extract(idx1, 3);

          pc->vpacki32i16(pix0, pix0, pix0);
          pc->vpacki32i16(pix0t, pix0t, pix0t);
          pc->vpacki32i16(pix1, pix1, pix1);
          pc->vpacki32i16(pix1t, pix1t, pix1t);

          pc->vmuli16(pix1 , pix1 , f->wa_wb);
          pc->vmuli16(pix1t, pix1t, f->wc_wd);
          pc->vmuli16(pix0 , pix0 , f->wa_wb);
          pc->vmuli16(pix0t, pix0t, f->wc_wd);

          pc->vaddi16(pix1, pix1, pix1t);
          pc->vloadi32_u8u32_(pix2, x86::ptr(f->srcp0, idx0, shift));
          pc->vaddi16(pix0, pix0, pix0t);
          pc->vloadi32_u8u32_(pix2t, x86::ptr(f->srcp1, idx0, shift));

          pc->vcombhli64(pixT, pixL, pix1);
          pc->vloadi32_u8u32_(pixL, x86::ptr(f->srcp0, idx1, shift));
          pc->vaddi16(pix0, pix0, pixT);
          pc->vloadi32_u8u32_(pixT, x86::ptr(f->srcp1, idx1, shift));

          pc->vpacki32i16(pixL, pixL, pixL);
          pc->vpacki32i16(pix2, pix2, pix2);
          pc->vpacki32i16(pix2t, pix2t, pix2t);
          pc->vmuli16(pixL, pixL, f->wa_wb);
          pc->vpacki32i16(pixT, pixT, pixT);
        }
        else {
          pc->vloadi32(pix0, x86::ptr(f->srcp0, idx0, shift));
          pc->vloadi32(pix0t, x86::ptr(f->srcp1, idx0, shift));
          iExt.extract(idx0, 2);
          pc->vsubi32(f->xVec4, f->xVec4, xFix4);

          pc->vswizi32(pix0, pix0, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vswizi32(pix0t, pix0t, x86::Predicate::shuf(0, 0, 0, 0));

          pc->vloadi32(pix1, x86::ptr(f->srcp0, idx1, shift));
          pc->vloadi32(pix1t, x86::ptr(f->srcp1, idx1, shift));
          iExt.extract(idx1, 3);

          pc->vswizi32(pix1, pix1, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vswizi32(pix1t, pix1t, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vmovu8u16(pix0, pix0);
          pc->vmovu8u16(pix0t, pix0t);
          pc->vmovu8u16(pix1, pix1);
          pc->vmovu8u16(pix1t, pix1t);

          pc->vmuli16(pix1, pix1, f->wa_wb);
          pc->vmuli16(pix1t, pix1t, f->wc_wd);
          pc->vmuli16(pix0, pix0, f->wa_wb);
          pc->vmuli16(pix0t, pix0t, f->wc_wd);

          pc->vaddi16(pix1, pix1, pix1t);
          pc->vloadi32(pix2, x86::ptr(f->srcp0, idx0, shift));
          pc->vaddi16(pix0, pix0, pix0t);
          pc->vloadi32(pix2t, x86::ptr(f->srcp1, idx0, shift));

          pc->vswizi32(pix2, pix2, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vswizi32(pix2t, pix2t, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vcombhli64(pixT, pixL, pix1);
          pc->vloadi32(pixL, x86::ptr(f->srcp0, idx1, shift));
          pc->vaddi16(pix0, pix0, pixT);
          pc->vloadi32(pixT, x86::ptr(f->srcp1, idx1, shift));

          pc->vmovu8u16(pix2, pix2);
          pc->vswizi32(pixL, pixL, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vmovu8u16(pix2t, pix2t);
          pc->vmovu8u16(pixL, pixL);
          pc->vswizi32(pixT, pixT, x86::Predicate::shuf(0, 0, 0, 0));
          pc->vmuli16(pixL, pixL, f->wa_wb);
          pc->vmovu8u16(pixT, pixT);
        }

        pc->vmuli16(pix2, pix2, f->wa_wb);
        pc->vmuli16(pixT, pixT, f->wc_wd);
        pc->vmuli16(pix2t, pix2t, f->wc_wd);
        pc->vsrli16(pix0, pix0, 8);

        pc->vaddi16(pixL, pixL, pixT);
        pc->vaddi16(pix2, pix2, pix2t);
        pc->vcombhli64(pix1, pix1, pixL);
        pc->vaddi16(pix2, pix2, pix1);
        pc->vsrli16(pix2, pix2, 8);

        p.uc.init(pix0, pix2);
        pc->xSatisfyARGB32_Nx(p, flags);
      }
    }
  }
}

void FetchSimplePatternPart::fetch8(PixelARGB& p, uint32_t flags) noexcept {
  FetchPart::fetch8(p, flags);
}

// ============================================================================
// [BLPipeGen::FetchAffinePatternPart - Construction / Destruction]
// ============================================================================

FetchAffinePatternPart::FetchAffinePatternPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : FetchPatternPart(pc, fetchType, fetchPayload, format) {

  _maxPixels = 4;
  _maxOptLevelSupported = kOptLevel_X86_AVX;

  switch (fetchType) {
    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_ANY:
    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_OPT:
      _isComplexFetch = true;
      _persistentRegs[x86::Reg::kGroupVec] = 3;
      break;

    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_ANY:
    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_OPT:
      // TODO: [PIPEGEN] Implement fetch4.
      _maxPixels = 1;
      _isComplexFetch = true;
      _persistentRegs[x86::Reg::kGroupVec] = 3;
      break;

    default:
      BL_NOT_REACHED();
  }

  JitUtils::resetVarStruct(&f, sizeof(f));
}

// ============================================================================
// [BLPipeGen::FetchAffinePatternPart - Init / Fini]
// ============================================================================

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
  cc->spill(f->srctop);

  pc->vloadi128u(f->xx_xy, x86::ptr(pc->_fetchData, REL_PATTERN(affine.xx)));
  pc->vloadi128u(f->yx_yy, x86::ptr(pc->_fetchData, REL_PATTERN(affine.yx)));

  pc->vmovsi32(f->tx_ty, y);
  pc->vswizi32(f->tx_ty, f->tx_ty, x86::Predicate::shuf(1, 0, 1, 0));
  pc->vMulU64xU32Lo(f->tx_ty, f->yx_yy, f->tx_ty);
  pc->vaddi64(f->tx_ty, f->tx_ty, x86::ptr(pc->_fetchData, REL_PATTERN(affine.tx)));

  // RoR: `tw_th` and `rx_ry` are only used by repeated or reflected patterns.
  pc->vloadi128u(f->rx_ry, x86::ptr(pc->_fetchData, REL_PATTERN(affine.rx)));
  pc->vloadi128u(f->tw_th, x86::ptr(pc->_fetchData, REL_PATTERN(affine.tw)));

  pc->vloadi128u(f->ox_oy, x86::ptr(pc->_fetchData, REL_PATTERN(affine.ox)));
  pc->vloadi128u(f->rx_ry, x86::ptr(pc->_fetchData, REL_PATTERN(affine.rx)));
  pc->vloadi128u(f->xx2_xy2, x86::ptr(pc->_fetchData, REL_PATTERN(affine.xx2)));

  // Pad: [MaxY | MaxX | MinY | MinX]
  pc->vloadi128u(f->minx_miny, x86::ptr(pc->_fetchData, REL_PATTERN(affine.minX)));
  pc->vloadi64(f->corx_cory, x86::ptr(pc->_fetchData, REL_PATTERN(affine.corX)));

  if (isOptimized()) {
    pc->vpacki32i16(f->minx_miny, f->minx_miny, f->minx_miny);                  // [MaxY|MaxX|MinY|MinX|MaxY|MaxX|MinY|MinX]
    pc->vswizi32(f->maxx_maxy, f->minx_miny, x86::Predicate::shuf(1, 1, 1, 1)); // [MaxY|MaxX|MaxY|MaxX|MaxY|MaxX|MaxY|MaxX]
    pc->vswizi32(f->minx_miny, f->minx_miny, x86::Predicate::shuf(0, 0, 0, 0)); // [MinY|MinX|MinY|MinX|MinY|MinX|MinY|MinX]
  }
  else {
    pc->vswizi32(f->maxx_maxy, f->minx_miny, x86::Predicate::shuf(3, 3, 2, 2)); // [MaxY|MaxY|MaxX|MaxX]
    pc->vswizi32(f->minx_miny, f->minx_miny, x86::Predicate::shuf(1, 1, 0, 0)); // [MinY|MinY|MinX|MinX]

    pc->vswizi32(f->corx_cory, f->corx_cory, x86::Predicate::shuf(1, 1, 0, 0)); // [CorY|CorY|CorX|CorX]
  }

  // vAddrMul.
  if (isOptimized()) {
    pc->vloadi32(f->vAddrMul, x86::ptr(pc->_fetchData, REL_PATTERN(affine.addrMul)));
    pc->vswizi32(f->vAddrMul, f->vAddrMul, x86::Predicate::shuf(0, 0, 0, 0));
  }

  if (isRectFill()) {
    advancePxPy(f->tx_ty, x);
    normalizePxPy(f->tx_ty);
  }
}

void FetchAffinePatternPart::_finiPart() noexcept {}

// ============================================================================
// [BLPipeGen::FetchAffinePatternPart - Advance]
// ============================================================================

void FetchAffinePatternPart::advanceY() noexcept {
  pc->vaddi64(f->tx_ty, f->tx_ty, f->yx_yy);

  if (isRectFill())
    normalizePxPy(f->tx_ty);
}

void FetchAffinePatternPart::startAtX(x86::Gp& x) noexcept {
  if (isRectFill()) {
    pc->vmov(f->px_py, f->tx_ty);
  }
  else {
    // Similar to `advancePxPy()`, however, we don't need a temporary here...
    pc->vmovsi32(f->px_py, x.r32());
    pc->vswizi32(f->px_py, f->px_py, x86::Predicate::shuf(1, 0, 1, 0));
    pc->vMulU64xU32Lo(f->px_py, f->xx_xy, f->px_py);
    pc->vaddi64(f->px_py, f->px_py, f->tx_ty);

    normalizePxPy(f->px_py);
  }

  if (pixelGranularity() > 1)
    enterN();
}

void FetchAffinePatternPart::advanceX(x86::Gp& x, x86::Gp& diff) noexcept {
  BL_UNUSED(x);
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

  pc->vmovsi32(t, i.r32());
  pc->vswizi32(t, t, x86::Predicate::shuf(1, 0, 1, 0));
  pc->vMulU64xU32Lo(t, f->xx_xy, t);
  pc->vaddi64(px_py, px_py, t);
}

void FetchAffinePatternPart::normalizePxPy(x86::Xmm& px_py) noexcept {
  x86::Xmm v0 = cc->newXmm("v0");

  pc->vzeropi(v0);
  pc->xModI64HIxDouble(px_py, px_py, f->tw_th);
  pc->vcmpgti32(v0, v0, px_py);
  pc->vand(v0, v0, f->rx_ry);
  pc->vaddi32(px_py, px_py, v0);

  pc->vcmpgti32(v0, px_py, f->ox_oy);
  pc->vand(v0, v0, f->rx_ry);
  pc->vsubi32(px_py, px_py, v0);
}

// ============================================================================
// [BLPipeGen::FetchAffinePatternPart - Fetch]
// ============================================================================

void FetchAffinePatternPart::prefetch1() noexcept {
  x86::Xmm vIdx = f->vIdx;

  switch (fetchType()) {
    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_ANY: {
      clampVIdx32(vIdx, f->px_py, kClampStepA_NN);
      clampVIdx32(vIdx, vIdx    , kClampStepB_NN);
      break;
    }

    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_OPT: {
      pc->vswizi32(vIdx, f->px_py, x86::Predicate::shuf(3, 1, 3, 1));
      pc->vpacki32i16(vIdx, vIdx, vIdx);
      pc->vmaxi16(vIdx, vIdx, f->minx_miny);
      pc->vmini16(vIdx, vIdx, f->maxx_maxy);
      break;
    }
  }
}

void FetchAffinePatternPart::fetch1(PixelARGB& p, uint32_t flags) noexcept {
  int shift = 2;

  switch (fetchType()) {
    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_ANY: {
      x86::Gp texPtr = cc->newIntPtr("texPtr");
      x86::Gp texOff = cc->newIntPtr("texOff");

      x86::Xmm vIdx = f->vIdx;
      x86::Xmm vMsk = cc->newXmm("vMsk");

      clampVIdx32(vIdx, vIdx, kClampStepC_NN);
      pc->vaddi64(f->px_py, f->px_py, f->xx_xy);

      IndexExtractorU32 iExt(pc);
      iExt.begin(vIdx);
      iExt.extract(texPtr, 3);
      iExt.extract(texOff, 1);

      pc->vcmpgti32(vMsk, f->px_py, f->ox_oy);
      cc->imul(texPtr, f->stride);
      pc->vand(vMsk, vMsk, f->rx_ry);
      pc->vsubi32(f->px_py, f->px_py, vMsk);

      cc->add(texPtr, f->srctop);
      pc->xFetchARGB32_1x(p, flags, x86::ptr(texPtr, texOff, shift), 4);
      clampVIdx32(vIdx, f->px_py, kClampStepA_NN);

      pc->xSatisfyARGB32_1x(p, flags);
      clampVIdx32(vIdx, vIdx, kClampStepB_NN);
      break;
    }

    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_OPT: {
      x86::Gp texPtr = cc->newIntPtr("texPtr");
      x86::Xmm vIdx = f->vIdx;
      x86::Xmm vMsk = cc->newXmm("vMsk");

      pc->vsrai16(vMsk, vIdx, 15);
      pc->vxor(vIdx, vIdx, vMsk);

      pc->vaddi64(f->px_py, f->px_py, f->xx_xy);
      pc->vmaddi16(vIdx, vIdx, f->vAddrMul);

      pc->vcmpgti32(vMsk, f->px_py, f->ox_oy);
      pc->vand(vMsk, vMsk, f->rx_ry);
      pc->vsubi32(f->px_py, f->px_py, vMsk);
      pc->vmovsi32(texPtr.r32(), vIdx);

      pc->vswizi32(vIdx, f->px_py, x86::Predicate::shuf(3, 1, 3, 1));
      pc->vpacki32i16(vIdx, vIdx, vIdx);

      cc->add(texPtr, f->srctop);
      pc->vmaxi16(vIdx, vIdx, f->minx_miny);
      pc->xFetchARGB32_1x(p, flags, x86::ptr(texPtr), 4);

      pc->vmini16(vIdx, vIdx, f->maxx_maxy);
      pc->xSatisfyARGB32_1x(p, flags);
      break;
    }

    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_ANY: {
      x86::Xmm vIdx = cc->newXmm("vIdx");
      x86::Xmm vMsk = cc->newXmm("vMsk");
      x86::Xmm vWeights = cc->newXmm("vWeights");

      pc->vswizi32(vIdx, f->px_py, x86::Predicate::shuf(3, 3, 1, 1));
      pc->vsubi32(vIdx, vIdx, pc->constAsMem(blCommonTable.i128_FFFFFFFF00000000));

      pc->vswizli16(vWeights, f->px_py, x86::Predicate::shuf(1, 1, 1, 1));
      clampVIdx32(vIdx, vIdx, kClampStepA_BI);

      pc->vaddi64(f->px_py, f->px_py, f->xx_xy);
      clampVIdx32(vIdx, vIdx, kClampStepB_BI);

      pc->vcmpgti32(vMsk, f->px_py, f->ox_oy);
      pc->vswizhi16(vWeights, vWeights, x86::Predicate::shuf(1, 1, 1, 1));

      pc->vand(vMsk, vMsk, f->rx_ry);
      pc->vsrli16(vWeights, vWeights, 8);

      pc->vsubi32(f->px_py, f->px_py, vMsk);
      pc->vxor(vWeights, vWeights, pc->constAsMem(blCommonTable.i128_FFFFFFFF00000000));

      clampVIdx32(vIdx, vIdx, kClampStepC_BI);
      pc->vaddi16(vWeights, vWeights, pc->constAsMem(blCommonTable.i128_0101010100000000));

      p.uc.init(cc->newXmm("pix0"));
      FetchUtils::xFilterBilinearARGB32_1x(pc, p.uc[0], f->srctop, f->stride, vIdx, vWeights);
      pc->xSatisfyARGB32_1x(p, flags);
      break;
    }

    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_OPT: {
      // TODO: [PIPEGEN] Not implemented, not used for now...
      break;
    }
  }
}

void FetchAffinePatternPart::enterN() noexcept {
  x86::Xmm vMsk0 = cc->newXmm("vMsk0");

  pc->vaddi64(f->qx_qy, f->px_py, f->xx_xy);
  pc->vcmpgti32(vMsk0, f->qx_qy, f->ox_oy);
  pc->vand(vMsk0, vMsk0, f->rx_ry);
  pc->vsubi32(f->qx_qy, f->qx_qy, vMsk0);
}

void FetchAffinePatternPart::leaveN() noexcept {}

void FetchAffinePatternPart::prefetchN() noexcept {
  switch (fetchType()) {
    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_OPT: {
      x86::Xmm vIdx = f->vIdx;
      x86::Xmm vMsk0 = cc->newXmm("vMsk0");
      x86::Xmm vMsk1 = cc->newXmm("vMsk1");

      pc->vshufi32(vIdx, f->px_py, f->qx_qy, x86::Predicate::shuf(3, 1, 3, 1));
      pc->vaddi64(f->px_py, f->px_py, f->xx2_xy2);
      pc->vaddi64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      pc->vcmpgti32(vMsk0, f->px_py, f->ox_oy);
      pc->vcmpgti32(vMsk1, f->qx_qy, f->ox_oy);

      pc->vand(vMsk0, vMsk0, f->rx_ry);
      pc->vand(vMsk1, vMsk1, f->rx_ry);

      pc->vsubi32(f->px_py, f->px_py, vMsk0);
      pc->vsubi32(f->qx_qy, f->qx_qy, vMsk1);

      pc->vshufi32(vMsk0, f->px_py, f->qx_qy, x86::Predicate::shuf(3, 1, 3, 1));
      pc->vpacki32i16(vIdx, vIdx, vMsk0);

      pc->vmaxi16(vIdx, vIdx, f->minx_miny);
      pc->vmini16(vIdx, vIdx, f->maxx_maxy);

      pc->vsrai16(vMsk0, vIdx, 15);
      pc->vxor(vIdx, vIdx, vMsk0);
      break;
    }
  }
}

void FetchAffinePatternPart::postfetchN() noexcept {
  switch (fetchType()) {
    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_OPT: {
      break;
    }
  }
}

void FetchAffinePatternPart::fetch4(PixelARGB& p, uint32_t flags) noexcept {
  int shift = 2;

  switch (fetchType()) {
    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_ANY: {
      // Nearest neighbor.
      FetchContext4X fCtx(pc, &p, flags);
      IndexExtractorU32 iExt(pc, IndexExtractorU32::kStrategyStack);

      x86::Gp texPtr0 = cc->newIntPtr("texPtr0");
      x86::Gp texOff0 = cc->newIntPtr("texOff0");
      x86::Gp texPtr1 = cc->newIntPtr("texPtr1");
      x86::Gp texOff1 = cc->newIntPtr("texOff1");

      x86::Xmm vIdx0 = cc->newXmm("vIdx0");
      x86::Xmm vIdx1 = cc->newXmm("vIdx1");
      x86::Xmm vMsk0 = cc->newXmm("vMsk0");
      x86::Xmm vMsk1 = cc->newXmm("vMsk1");

      pc->vshufi32(vIdx0, f->px_py, f->qx_qy, x86::Predicate::shuf(3, 1, 3, 1));
      pc->vaddi64(f->px_py, f->px_py, f->xx2_xy2);

      clampVIdx32(vIdx0, vIdx0, kClampStepA_NN);
      pc->vaddi64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      clampVIdx32(vIdx0, vIdx0, kClampStepB_NN);
      pc->vcmpgti32(vMsk0, f->px_py, f->ox_oy);
      clampVIdx32(vIdx0, vIdx0, kClampStepC_NN);

      pc->vcmpgti32(vMsk1, f->qx_qy, f->ox_oy);
      pc->vand(vMsk0, vMsk0, f->rx_ry);
      pc->vand(vMsk1, vMsk1, f->rx_ry);
      pc->vsubi32(f->px_py, f->px_py, vMsk0);
      pc->vsubi32(f->qx_qy, f->qx_qy, vMsk1);

      iExt.begin(vIdx0);
      pc->vshufi32(vIdx1, f->px_py, f->qx_qy, x86::Predicate::shuf(3, 1, 3, 1));
      iExt.extract(texPtr0, 1);
      iExt.extract(texOff0, 0);

      clampVIdx32(vIdx1, vIdx1, kClampStepA_NN);
      clampVIdx32(vIdx1, vIdx1, kClampStepB_NN);

      iExt.extract(texPtr1, 3);
      iExt.extract(texOff1, 2);

      cc->imul(texPtr0, f->stride);
      cc->imul(texPtr1, f->stride);

      clampVIdx32(vIdx1, vIdx1, kClampStepC_NN);
      pc->vaddi64(f->px_py, f->px_py, f->xx2_xy2);
      pc->vaddi64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      cc->add(texPtr0, f->srctop);
      cc->add(texPtr1, f->srctop);
      iExt.begin(vIdx1);

      fCtx.fetchARGB32(x86::ptr(texPtr0, texOff0, shift));
      iExt.extract(texPtr0, 1);
      iExt.extract(texOff0, 0);

      pc->vcmpgti32(vMsk0, f->px_py, f->ox_oy);
      pc->vcmpgti32(vMsk1, f->qx_qy, f->ox_oy);

      fCtx.fetchARGB32(x86::ptr(texPtr1, texOff1, shift));
      iExt.extract(texPtr1, 3);
      iExt.extract(texOff1, 2);
      cc->imul(texPtr0, f->stride);

      pc->vand(vMsk0, vMsk0, f->rx_ry);
      pc->vand(vMsk1, vMsk1, f->rx_ry);

      cc->imul(texPtr1, f->stride);
      pc->vsubi32(f->px_py, f->px_py, vMsk0);

      cc->add(texPtr0, f->srctop);
      cc->add(texPtr1, f->srctop);
      fCtx.fetchARGB32(x86::ptr(texPtr0, texOff0, shift));

      pc->vsubi32(f->qx_qy, f->qx_qy, vMsk1);
      fCtx.fetchARGB32(x86::ptr(texPtr1, texOff1, shift));
      fCtx.end();

      pc->xSatisfyARGB32_Nx(p, flags);
      break;
    }

    case BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_OPT: {
      FetchContext4X fCtx(pc, &p, flags);
      IndexExtractorU32 iExt(pc, IndexExtractorU32::kStrategyStack);

      x86::Gp texPtr0 = cc->newIntPtr("texPtr0");
      x86::Gp texPtr1 = cc->newIntPtr("texPtr1");

      x86::Xmm vIdx = f->vIdx;
      x86::Xmm vMsk0 = cc->newXmm("vMsk0");
      x86::Xmm vMsk1 = cc->newXmm("vMsk1");

      pc->vmaddi16(vIdx, vIdx, f->vAddrMul);
      iExt.begin(vIdx);

      pc->vaddi64(f->px_py, f->px_py, f->xx2_xy2);
      pc->vaddi64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      pc->vcmpgti32(vMsk0, f->px_py, f->ox_oy);
      pc->vcmpgti32(vMsk1, f->qx_qy, f->ox_oy);

      pc->vand(vMsk0, vMsk0, f->rx_ry);
      pc->vand(vMsk1, vMsk1, f->rx_ry);
      iExt.extract(texPtr0, 0);

      pc->vsubi32(f->px_py, f->px_py, vMsk0);
      pc->vsubi32(f->qx_qy, f->qx_qy, vMsk1);
      iExt.extract(texPtr1, 1);

      pc->vshufi32(vIdx, f->px_py, f->qx_qy, x86::Predicate::shuf(3, 1, 3, 1));
      cc->add(texPtr0, f->srctop);
      cc->add(texPtr1, f->srctop);

      pc->vaddi64(f->px_py, f->px_py, f->xx2_xy2);
      pc->vaddi64(f->qx_qy, f->qx_qy, f->xx2_xy2);

      fCtx.fetchARGB32(x86::ptr(texPtr0));
      iExt.extract(texPtr0, 2);

      pc->vcmpgti32(vMsk0, f->px_py, f->ox_oy);
      pc->vcmpgti32(vMsk1, f->qx_qy, f->ox_oy);

      fCtx.fetchARGB32(x86::ptr(texPtr1));
      iExt.extract(texPtr1, 3);

      pc->vand(vMsk0, vMsk0, f->rx_ry);
      pc->vand(vMsk1, vMsk1, f->rx_ry);
      cc->add(texPtr0, f->srctop);

      pc->vsubi32(f->px_py, f->px_py, vMsk0);
      pc->vsubi32(f->qx_qy, f->qx_qy, vMsk1);
      pc->vshufi32(vMsk0, f->px_py, f->qx_qy, x86::Predicate::shuf(3, 1, 3, 1));
      cc->add(texPtr1, f->srctop);

      pc->vpacki32i16(vIdx, vIdx, vMsk0);
      fCtx.fetchARGB32(x86::ptr(texPtr0));

      pc->vmaxi16(vIdx, vIdx, f->minx_miny);
      fCtx.fetchARGB32(x86::ptr(texPtr1));

      pc->vmini16(vIdx, vIdx, f->maxx_maxy);
      fCtx.end();

      pc->vsrai16(vMsk0, vIdx, 15);
      pc->vxor(vIdx, vIdx, vMsk0);

      pc->xSatisfyARGB32_Nx(p, flags);
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
        pc->vmaxi32_(dst, src, f->minx_miny);
      }
      else {
        if (dst.id() == src.id()) {
          x86::Xmm tmp = cc->newXmm("vIdxPad");
          pc->vmov(tmp, dst);
          pc->vcmpgti32(dst, dst, f->minx_miny); // `-1` if `src` is greater than `minx_miny`.
          pc->vand(dst, dst, tmp);               // Changes `dst` to `0` if clamped.
        }
        else {
          pc->vmov(dst, src);
          pc->vcmpgti32(dst, dst, f->minx_miny); // `-1` if `src` is greater than `minx_miny`.
          pc->vand(dst, dst, src);               // Changes `dst` to `0` if clamped.
        }
      }
      break;
    }

    // Step B - Handle a possible overflow (PAD | Bilinear overflow).
    case kClampStepB_NN:
    case kClampStepB_BI: {
      // Always performed on the same register.
      BL_ASSERT(dst.id() == src.id());

      x86::Xmm t1 = cc->newXmm("vIdxMsk1");
      x86::Xmm t2 = cc->newXmm("vIdxMsk2");

      if (pc->hasSSE4_1()) {
        pc->vcmpgti32(t1, dst, f->maxx_maxy);
        pc->vblendv8_(dst, dst, f->corx_cory, t1);
      }
      else {
        pc->vmov(t1, dst);
        pc->vmov(t2, f->corx_cory);

        pc->vcmpgti32(dst, dst, f->maxx_maxy);
        pc->vand(t2, t2, dst);

        pc->vandnot_a(dst, dst, t1);
        pc->vor(dst, dst, t2);
      }

      break;
    }

    // Step C - Handle a possible reflection (RoR).
    case kClampStepC_NN:
    case kClampStepC_BI: {
      // Always performed on the same register.
      BL_ASSERT(dst.id() == src.id());

      x86::Xmm tmp = cc->newXmm("vIdxRoR");
      pc->vsrai32(tmp, dst, 31);
      pc->vxor(dst, dst, tmp);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

} // {BLPipeGen}
