// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "compop_p.h"

static constexpr uint32_t BL_FORMAT_RESERVED_COUNT = uint32_t(BLInternalFormat::kMaxReserved) + 1u;

struct BLCompOpInfoGen {
  #define F(VALUE) BLCompOpFlags::VALUE

  static constexpr BLCompOpInfo value(size_t op) noexcept {
    return BLCompOpInfo { uint16_t(
      op == BL_COMP_OP_SRC_OVER           ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_SRC_COPY           ? F(kTypeB) | F(kNone) | F(kNone) | F(kSc)   | F(kSa)   | F(kNone)       | F(kNone)       :
      op == BL_COMP_OP_SRC_IN             ? F(kTypeB) | F(kNone) | F(kDa)   | F(kSc)   | F(kSa)   | F(kNopIfDaEq0) | F(kNone)       :
      op == BL_COMP_OP_SRC_OUT            ? F(kTypeB) | F(kNone) | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNone)       :
      op == BL_COMP_OP_SRC_ATOP           ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNopIfDaEq0) | F(kNopIfSaEq0) :
      op == BL_COMP_OP_DST_OVER           ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNopIfDaEq1) | F(kNopIfSaEq0) :
      op == BL_COMP_OP_DST_COPY           ? F(kTypeC) | F(kDc)   | F(kDa)   | F(kNone) | F(kNone) | F(kNop)        | F(kNop)        :
      op == BL_COMP_OP_DST_IN             ? F(kTypeB) | F(kDc)   | F(kDa)   | F(kNone) | F(kSa)   | F(kNone)       | F(kNopIfSaEq1) :
      op == BL_COMP_OP_DST_OUT            ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kNone) | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_DST_ATOP           ? F(kTypeB) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNone)       :
      op == BL_COMP_OP_XOR                ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_CLEAR              ? F(kTypeC) | F(kNone) | F(kNone) | F(kNone) | F(kNone) | F(kNopIfDaEq0) | F(kNone)       :

      op == BL_COMP_OP_PLUS               ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_MINUS              ? F(kTypeC) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_MODULATE           ? F(kTypeB) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNopIfDaEq0) | F(kNone)       :
      op == BL_COMP_OP_MULTIPLY           ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNopIfDaEq0) | F(kNopIfSaEq0) :
      op == BL_COMP_OP_SCREEN             ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_OVERLAY            ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_DARKEN             ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_LIGHTEN            ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_COLOR_DODGE        ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_COLOR_BURN         ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_LINEAR_BURN        ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_LINEAR_LIGHT       ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_PIN_LIGHT          ? F(kTypeC) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_HARD_LIGHT         ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_SOFT_LIGHT         ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_DIFFERENCE         ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :
      op == BL_COMP_OP_EXCLUSION          ? F(kTypeA) | F(kDc)   | F(kDa)   | F(kSc)   | F(kSa)   | F(kNone)       | F(kNopIfSaEq0) :

      op == BL_COMP_OP_INTERNAL_ALPHA_INV ? F(kTypeC) | F(kNone) | F(kDa)   | F(kNone) | F(kNone) | F(kNone)       | F(kNone)       : F(kNone)
    ) };
  }

  #undef F
};

const BLLookupTable<BLCompOpInfo, BL_COMP_OP_INTERNAL_COUNT> blCompOpInfo =
  blMakeLookupTable<BLCompOpInfo, BL_COMP_OP_INTERNAL_COUNT, BLCompOpInfoGen>();

// Legend:
//
//   - Sca  - Source color, premultiplied: `Sc * Sa`.
//   - Sc   - Source color.
//   - Sa   - Source alpha.
//
//   - Dca  - Destination color, premultiplied: `Dc * Da`.
//   - Dc   - Destination color.
//   - Da   - Destination alpha.
//
//   - Dca' - Resulting color, premultiplied.
//   - Da'  - Resulting alpha.
//
//   - m    - Mask (if used).
//
// Blending function F(Sc, Dc) is used in the following way if destination
// or source contains alpha channel (otherwise it's assumed to be `1.0`):
//
//  - Dca' = Func(Sc, Dc) * Sa.Da + Sca.(1 - Da) + Dca.(1 - Sa)
//  - Da'  = Da + Sa.(1 - Da)
struct BLCompOpSimplifyInfoGen {
  // Shorthands of pixel formats.
  using Fmt = BLInternalFormat;

  // Shorthands of composition operators.
  enum CompOp : uint32_t {
    SrcOver     = BL_COMP_OP_SRC_OVER,
    SrcCopy     = BL_COMP_OP_SRC_COPY,
    SrcIn       = BL_COMP_OP_SRC_IN,
    SrcOut      = BL_COMP_OP_SRC_OUT,
    SrcAtop     = BL_COMP_OP_SRC_ATOP,
    DstOver     = BL_COMP_OP_DST_OVER,
    DstCopy     = BL_COMP_OP_DST_COPY,
    DstIn       = BL_COMP_OP_DST_IN,
    DstOut      = BL_COMP_OP_DST_OUT,
    DstAtop     = BL_COMP_OP_DST_ATOP,
    Xor         = BL_COMP_OP_XOR,
    Clear       = BL_COMP_OP_CLEAR,
    Plus        = BL_COMP_OP_PLUS,
    Minus       = BL_COMP_OP_MINUS,
    Modulate    = BL_COMP_OP_MODULATE,
    Multiply    = BL_COMP_OP_MULTIPLY,
    Screen      = BL_COMP_OP_SCREEN,
    Overlay     = BL_COMP_OP_OVERLAY,
    Darken      = BL_COMP_OP_DARKEN,
    Lighten     = BL_COMP_OP_LIGHTEN,
    ColorDodge  = BL_COMP_OP_COLOR_DODGE,
    ColorBurn   = BL_COMP_OP_COLOR_BURN,
    LinearBurn  = BL_COMP_OP_LINEAR_BURN,
    LinearLight = BL_COMP_OP_LINEAR_LIGHT,
    PinLight    = BL_COMP_OP_PIN_LIGHT,
    HardLight   = BL_COMP_OP_HARD_LIGHT,
    SoftLight   = BL_COMP_OP_SOFT_LIGHT,
    Difference  = BL_COMP_OP_DIFFERENCE,
    Exclusion   = BL_COMP_OP_EXCLUSION,

    AlphaInv    = BL_COMP_OP_INTERNAL_ALPHA_INV
  };

  static constexpr BLCompOpSimplifyInfo makeOp(uint32_t compOp, Fmt d, Fmt s) noexcept {
    return BLCompOpSimplifyInfo::make(compOp, d, s, BLCompOpSolidId::kNone);
  }

  static constexpr BLCompOpSimplifyInfo transparent(uint32_t compOp, Fmt d, Fmt s) noexcept {
    return BLCompOpSimplifyInfo::make(compOp, d, s, BLCompOpSolidId::kTransparent);
  }

  static constexpr BLCompOpSimplifyInfo opaqueBlack(uint32_t compOp, Fmt d, Fmt s) noexcept {
    return BLCompOpSimplifyInfo::make(compOp, d, s, BLCompOpSolidId::kOpaqueBlack);
  }

  static constexpr BLCompOpSimplifyInfo opaqueWhite(uint32_t compOp, Fmt d, Fmt s) noexcept {
    return BLCompOpSimplifyInfo::make(compOp, d, s, BLCompOpSolidId::kOpaqueWhite);
  }

  static constexpr BLCompOpSimplifyInfo opaqueAlpha(uint32_t compOp, Fmt d, Fmt s) noexcept {
    return BLCompOpSimplifyInfo::make(compOp, d, s, BLCompOpSolidId::kOpaqueWhite);
  }

  // Internal Formats:
  static constexpr BLCompOpSimplifyInfo alphaInv(Fmt d, Fmt s) noexcept {
    return BLCompOpSimplifyInfo::make(AlphaInv, d, s, BLCompOpSolidId::kOpaqueWhite);
  }

  // Clear
  // -----
  //
  // [Clear PRGBxPRGB]
  //   Dca' = 0                              Dca' = Dca.(1 - m)
  //   Da'  = 0                              Da'  = Da .(1 - m)
  //
  // [Clear XRGBxPRGB]
  //   Dc'  = 0                              Dc'  = Dca.(1 - m)
  //
  // [Clear PRGBxXRGB] ~= [Clear PRGBxPRGB]
  // [Clear XRGBxXRGB] ~= [Clear XRGBxPRGB]
  static constexpr BLCompOpSimplifyInfo clear(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 ? transparent(SrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 ? opaqueBlack(SrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kA8     ? transparent(SrcCopy, Fmt::kA8    , Fmt::kPRGB32) :

           makeOp(Clear, d, s);
  }

  // SrcCopy
  // -------
  //
  // [Src PRGBxPRGB]
  //   Dca' = Sca                            Dca' = Sca.m + Dca.(1 - m)
  //   Da'  = Sa                             Da'  = Sa .m + Da .(1 - m)
  //
  // [Src PRGBxXRGB] ~= [Src PRGBxPRGB]
  //   Dca' = Sc                             Dca' = Sc.m + Dca.(1 - m)
  //   Da'  = 1                              Da'  = 1 .m + Da .(1 - m)
  //
  // [Src XRGBxPRGB]
  //   Dc'  = Sca                            Dc'  = Sca.m + Dc.(1 - m)
  //
  // [Src XRGBxXRGB]
  //   Dc'  = Sc                             Dc'  = Sc.m + Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo srcCopy(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? makeOp(SrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? makeOp(SrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? makeOp(SrcCopy, Fmt::kPRGB32, Fmt::kXRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? makeOp(SrcCopy, Fmt::kPRGB32, Fmt::kXRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? makeOp(SrcCopy, Fmt::kPRGB32, Fmt::kXRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? makeOp(SrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? clear(Fmt::kA8, Fmt::kZERO32) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? opaqueAlpha(SrcCopy, d, Fmt::kPRGB32) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? opaqueAlpha(SrcCopy, d, Fmt::kPRGB32) :

           makeOp(SrcCopy, d, s);
  }

  // DstCopy
  // -------
  //
  // [DstCopy ANYxANY]
  //   Dca' = Dca
  //   Da   = Da
  BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)
  static constexpr BLCompOpSimplifyInfo dstCopy(Fmt d, Fmt s) noexcept {
    return BLCompOpSimplifyInfo::dstCopy();
  }
  BL_DIAGNOSTIC_POP

  // SrcOver
  // -------
  //
  // [SrcOver PRGBxPRGB]
  //   Dca' = Sca + Dca.(1 - Sa)             Dca' = Sca.m + Dca.(1 - Sa.m)
  //   Da'  = Sa  + Da .(1 - Sa)             Da'  = Sa .m + Da .(1 - Sa.m)
  //
  // [SrcOver PRGBxXRGB] ~= [Src PRGBxPRGB]
  //   Dca' = Sc                             Dca' = Sc.m + Dca.(1 - m)
  //   Da'  = 1                              Da'  = 1 .m + Da .(1 - m)
  //
  // [SrcOver XRGBxPRGB] ~= [SrcOver PRGBxPRGB]
  //   Dc'  = Sca   + Dc.(1 - Sa  )          Dc'  = Sca.m + Dc.(1 - Sa.m)
  //
  // [SrcOver XRGBxXRGB] ~= [Src PRGBxPRGB]
  //   Dc'  = Sc                             Dc'  = Sc.m + Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo srcOver(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kPRGB32 && s == Fmt::kXRGB32 ? srcCopy(Fmt::kPRGB32, Fmt::kXRGB32) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? srcCopy(Fmt::kPRGB32, Fmt::kFRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? srcOver(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? srcCopy(Fmt::kPRGB32, Fmt::kXRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? srcCopy(Fmt::kPRGB32, Fmt::kFRGB32) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? dstCopy(Fmt::kA8, Fmt::kPRGB32) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? srcCopy(Fmt::kA8, Fmt::kXRGB32) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? srcCopy(Fmt::kA8, Fmt::kFRGB32) :

           makeOp(SrcOver, d, s);
  }

  // DstOver
  // -------
  //
  // [DstOver PRGBxPRGB]
  //   Dca' = Dca + Sca.(1 - Da)             Dca' = Dca + Sca.m.(1 - Da)
  //   Da'  = Da  + Sa .(1 - Da)             Da'  = Da  + Sa .m.(1 - Da)
  //
  // [DstOver PRGBxXRGB] ~= [DstOver PRGBxPRGB]
  //   Dca' = Dca + Sc.(1 - Da)              Dca' = Dca + Sc.m.(1 - Da)
  //   Da'  = Da  + 1 .(1 - Da)              Da'  = Da  + 1 .m.(1 - Da)
  //
  // [DstOver XRGBxPRGB] ~= [Dst]
  //   Dc'  = Dc
  //
  // [DstOver XRGBxXRGB] ~= [Dst]
  //   Dc'  = Dc
  static constexpr BLCompOpSimplifyInfo dstOver(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? dstOver(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 ? dstCopy(d, s) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(DstOver, d, s);
  }

  // SrcIn
  // -----
  //
  // [SrcIn PRGBxPRGB]
  //   Dca' = Sca.Da                         Dca' = Sca.Da.m + Dca.(1 - m)
  //   Da'  = Sa .Da                         Da'  = Sa .Da.m + Da .(1 - m)
  //
  // [SrcIn PRGBxXRGB] ~= [SrcIn PRGBxPRGB]
  //   Dca' = Sc.Da                          Dca' = Sc.Da.m + Dca.(1 - m)
  //   Da'  = 1 .Da                          Da'  = 1 .Da.m + Da .(1 - m)
  //
  // [SrcIn XRGBxPRGB]
  //   Dc'  = Sca                            Dc'  = Sca.m + Dc.(1 - m)
  //
  // [SrcIn XRGBxXRGB] ~= [SrcCopy XRGBxXRGB]
  //   Dc'  = Sc                             Dc'  = Sc.m + Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo srcIn(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? srcIn(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? srcIn(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 ? srcCopy(d, s) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? clear(d, s) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? dstCopy(d, s) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? dstCopy(d, s) :

           makeOp(SrcIn, d, s);
  }

  // DstIn
  // -----
  //
  // [DstIn PRGBxPRGB]
  //   Dca' = Dca.Sa                         Dca' = Dca.Sa.m + Dca.(1 - m)
  //   Da'  = Da .Sa                         Da'  = Da .Sa.m + Da .(1 - m)
  //
  // [DstIn PRGBxXRGB] ~= [Dst]
  //   Dca' = Dca
  //   Da'  = Da
  //
  // [DstIn XRGBxPRGB]
  //   Dc'  = Dc.Sa                          Dc'  = Dc.Sa.m + Dc.(1 - m)
  //
  // [DstIn XRGBxXRGB] ~= [Dst]
  //   Dc'  = Dc
  static constexpr BLCompOpSimplifyInfo dstIn(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? srcCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kXRGB32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? dstCopy(d, s) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? dstIn(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstIn(Fmt::kPRGB32, Fmt::kFRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? dstCopy(d, s) :

           d == Fmt::kA8 ? srcIn(d, s) :

           makeOp(DstIn, d, s);
  }

  // SrcOut
  // ------
  //
  // [SrcOut PRGBxPRGB]
  //   Dca' = Sca.(1 - Da)                   Dca' = Sca.m.(1 - Da) + Dca.(1 - m)
  //   Da'  = Sa .(1 - Da)                   Da'  = Sa .m.(1 - Da) + Da .(1 - m)
  //
  // [SrcOut PRGBxXRGB] ~= [SrcOut PRGBxPRGB]
  //   Dca' = Sc.(1 - Da)                    Dca' = Sc.m.(1 - Da) + Dca.(1 - m)
  //   Da'  = 1 .(1 - Da)                    Da'  = 1 .m.(1 - Da) + Da .(1 - m)
  //
  // [SrcOut XRGBxPRGB] ~= [Clear XRGBxPRGB]
  //   Dc'  = 0                              Dc'  = Dc.(1 - m)
  //
  // [SrcOut XRGBxXRGB] ~= [Clear XRGBxPRGB]
  //   Dc'  = 0                              Dc'  = Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo srcOut(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? clear(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? srcOut(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 ? clear(d, s) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? clear(d, s) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? alphaInv(d, Fmt::kXRGB32) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? alphaInv(d, Fmt::kXRGB32) :

           makeOp(SrcOut, d, s);
  }

  // DstOut
  // ------
  //
  // [DstOut PRGBxPRGB]
  //   Dca' = Dca.(1 - Sa)                   Dca' = Dca.(1 - Sa.m)
  //   Da'  = Da .(1 - Sa)                   Da'  = Da .(1 - Sa.m)
  //
  // [DstOut PRGBxXRGB] ~= [Clear PRGBxPRGB]
  //   Dca' = 0
  //   Da'  = 0
  //
  // [DstOut XRGBxPRGB]
  //   Dc'  = Dc.(1 - Sa)                    Dc'  = Dc.(1 - Sa.m)
  //
  // [DstOut XRGBxXRGB] ~= [Clear XRGBxPRGB]
  //   Dc'  = 0
  static constexpr BLCompOpSimplifyInfo dstOut(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kXRGB32 ? clear(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? clear(d, s) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? clear(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? clear(d, s) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? clear(d, s) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? clear(d, s) :

           makeOp(DstOut, d, s);
  }

  // SrcAtop
  // -------
  //
  // [SrcAtop PRGBxPRGB]
  //   Dca' = Sca.Da + Dca.(1 - Sa)          Dca' = Sca.Da.m + Dca.(1 - Sa.m)
  //   Da'  = Sa .Da + Da .(1 - Sa) = Da     Da'  = Sa .Da.m + Da .(1 - Sa.m) = Da
  //
  // [SrcAtop PRGBxXRGB] ~= [SrcIn PRGBxPRGB]
  //   Dca' = Sc.Da                          Dca' = Sc.Da.m + Dca.(1 - m)
  //   Da'  = 1 .Da                          Da'  = 1 .Da.m + Da .(1 - m)
  //
  // [SrcAtop XRGBxPRGB] ~= [SrcOver PRGBxPRGB]
  //   Dc'  = Sca + Dc.(1 - Sa)              Dc'  = Sca.m + Dc.(1 - Sa.m)
  //
  // [SrcAtop XRGBxXRGB] ~= [Src PRGBxPRGB]
  //   Dc'  = Sc                             Dc'  = Sc.m + Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo srcAtop(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kXRGB32 ? srcIn(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? srcIn(d, s) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? srcOver(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? srcOver(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? srcCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? srcCopy(d, s) :

           d == Fmt::kA8 ? dstCopy(d, s) :

           makeOp(SrcAtop, d, s);
  }

  // DstAtop
  // -------
  //
  // [DstAtop PRGBxPRGB]
  //   Dca' = Dca.Sa + Sca.(1 - Da)          Dca' = Dca.(1 - m.(1 - Sa)) + Sca.m.(1 - Da)
  //   Da'  = Da .Sa + Sa .(1 - Da) = Sa     Da'  = Da .(1 - m.(1 - Sa)) + Sa .m.(1 - Da)
  //
  // [DstAtop PRGBxXRGB] ~= [DstOver PRGBxPRGB]
  //   Dca' = Dca + Sc.(1 - Da)              Dca' = Dca + Sc.m.(1 - Da)
  //   Da'  = Da  + 1 .(1 - Da) = 1          Da'  = Da  + 1 .m.(1 - Da)
  //
  // [DstAtop XRGBxPRGB] ~= [DstIn XRGBxPRGB]
  //   Dc'  = Dc.Sa                          Dc'  = Dc.(1 - m.(1 - Sa)) = Dc.(1 - m) + Dc.Sa.m
  //
  // [DstAtop XRGBxXRGB] ~= [Dst]
  //   Dc'  = Dc
  static constexpr BLCompOpSimplifyInfo dstAtop(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? clear(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kXRGB32 ? dstOver(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? dstOver(d, s) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? dstIn(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? clear(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? dstCopy(d, s) :

           d == Fmt::kA8 ? srcCopy(d, s) :

           makeOp(DstAtop, d, s);
  }

  // Xor
  // ---
  //
  // [Xor PRGBxPRGB]
  //   Dca' = Dca.(1 - Sa) + Sca.(1 - Da)    Dca' = Dca.(1 - Sa.m) + Sca.m.(1 - Da)
  //   Da'  = Da .(1 - Sa) + Sa .(1 - Da)    Da'  = Da .(1 - Sa.m) + Sa .m.(1 - Da)
  //
  // [Xor PRGBxXRGB] ~= [SrcOut PRGBxPRGB]
  //   Dca' = Sca.(1 - Da)                   Dca' = Sca.m.(1 - Da) + Dca.(1 - m)
  //   Da'  = 1  .(1 - Da)                   Da'  = 1  .m.(1 - Da) + Da .(1 - m)
  //
  // [Xor XRGBxPRGB] ~= [DstOut XRGBxPRGB]
  //   Dc'  = Dc.(1 - Sa)                    Dc'  = Dc.(1 - Sa.m)
  //
  // [Xor XRGBxXRGB] ~= [Clear XRGBxPRGB]
  //   Dc'  = 0                              Dc'  = Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo xor_(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kXRGB32 ? srcOut(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? srcOut(d, s) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? dstOut(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? clear(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? clear(d, s) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? alphaInv(d, Fmt::kXRGB32) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? alphaInv(d, Fmt::kXRGB32) :

           makeOp(Xor, d, s);
  }

  // Plus
  // ----
  //
  // [Plus PRGBxPRGB]
  //   Dca' = Clamp(Dca + Sca)               Dca' = Clamp(Dca + Sca.m)
  //   Da'  = Clamp(Da  + Sa )               Da'  = Clamp(Da  + Sa .m)
  //
  // [Plus PRGBxXRGB] ~= [Plus PRGBxPRGB]
  //   Dca' = Clamp(Dca + Sc)                Dca' = Clamp(Dca + Sc.m)
  //   Da'  = Clamp(Da  + 1 )                Da'  = Clamp(Da  + 1 .m)
  //
  // [Plus XRGBxPRGB] ~= [Plus PRGBxPRGB]
  //   Dc'  = Clamp(Dc + Sca)                Dc'  = Clamp(Dc + Sca.m)
  //
  // [Plus XRGBxXRGB] ~= [Plus PRGBxPRGB]
  //   Dc'  = Clamp(Dc + Sc)                 Dc'  = Clamp(Dc + Sc.m)
  static constexpr BLCompOpSimplifyInfo plus(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? plus(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? plus(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? plus(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? plus(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? opaqueAlpha(Plus, d, Fmt::kPRGB32) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? opaqueAlpha(Plus, d, Fmt::kPRGB32) :

           makeOp(Plus, d, s);
  }

  // Minus
  // -----
  //
  // [Minus PRGBxPRGB]
  //   Dca' = Clamp(Dca - Sca)               Dca' = Clamp(Dca - Sca).m + Dca.(1 - m)
  //   Da'  = Da + Sa.(1 - Da)               Da'  = Da + Sa.m(1 - Da)
  //
  // [Minus PRGBxXRGB] ~= [Minus PRGBxPRGB]
  //   Dca' = Clamp(Dca - Sc)                Dca' = Clamp(Dca - Sc).m + Dca.(1 - m)
  //   Da'  = Da + 1.(1 - Da) = 1            Da'  = Da + 1.m(1 - Da)
  //
  // [Minus XRGBxPRGB]
  //   Dc'  = Clamp(Dc - Sca)                Dc'  = Clamp(Dc - Sca).m + Dc.(1 - m)
  //
  // [Minus XRGBxXRGB] ~= [Minus XRGBxPRGB]
  //   Dc'  = Clamp(Dc - Sc)                 Dc'  = Clamp(Dc - Sc).m + Dc.(1 - m)
  //
  // NOTE:
  //   `Clamp(a - b)` == `Max(a - b, 0)` == `1 - Min(1 - a + b, 1)`
  static constexpr BLCompOpSimplifyInfo minus(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? minus(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? minus(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? minus(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? minus(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(Minus, d, s);
  }

  // Modulate
  // --------
  //
  // [Modulate PRGBxPRGB]
  //   Dca' = Dca.Sca
  //   Da'  = Da .Sa
  //
  //   Dca' = Dca.(Sca.m + 1 - m)
  //   Da'  = Da .(Sa .m + 1 - m)
  //
  // [Modulate PRGBxXRGB]
  //   Dca' = Dca.Sc
  //   Da'  = Da .1
  //
  //   Dca' = Dca.(Sc.m + 1 - m)
  //   Da'  = Da .(1 .m + 1 - m) = Da
  //
  // [Modulate XRGBxPRGB]
  //   Dc' = Dc.Sca
  //   Dc' = Dc.(Sca.m + 1 - m)
  //
  // [Modulate XRGBxXRGB]
  //   Dc' = Dc.Sc
  //   Dc' = Dc.(Sc.m + 1 - m)
  static constexpr BLCompOpSimplifyInfo modulate(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? transparent(SrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? modulate(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? opaqueBlack(SrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? modulate(Fmt::kXRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? modulate(Fmt::kXRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? dstIn(d, s) :

           makeOp(Modulate, d, s);
  }

  // Multiply
  // --------
  //
  // [Multiply PRGBxPRGB]
  //   Dca' = Dca.(Sca + 1 - Sa) + Sca.(1 - Da)
  //   Da'  = Da .(Sa  + 1 - Sa) + Sa .(1 - Da) = Da + Sa.(1 - Da)
  //
  //   Dca' = Dca.(Sca.m + 1 - Sa.m) + Sca.m(1 - Da)
  //   Da'  = Da .(Sa .m + 1 - Sa.m) + Sa .m(1 - Da) = Da + Sa.m(1 - Da)
  //
  // [Multiply PRGBxXRGB]
  //   Dca' = Sc.(Dca + 1 - Da)
  //   Da'  = 1 .(Da  + 1 - Da) = 1
  //
  //   Dca' = Dca.(Sc.m + 1 - 1.m) + Sc.m(1 - Da)
  //   Da'  = Da .(1 .m + 1 - 1.m) + 1 .m(1 - Da) = Da + Sa.m(1 - Da)
  //
  // [Multiply XRGBxPRGB] ~= [Modulate XRGBxPRGB]
  //   Dc'  = Dc.(Sca   + 1 - Sa  )
  //   Dc'  = Dc.(Sca.m + 1 - Sa.m)
  //
  // [Multiply XRGBxXRGB] ~= [Modulate XRGBxXRGB]
  //   Dc'  = Dc.Sc
  //   Dc'  = Dc.(Sc.m + 1 - m)
  static constexpr BLCompOpSimplifyInfo multiply(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? multiply(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? modulate(Fmt::kXRGB32, Fmt::kXRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? modulate(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? dstOver(d, s) :

           makeOp(Multiply, d, s);
  }

  // Screen
  // ------
  //
  // [Screen PRGBxPRGB]
  //   Dca' = Dca + Sca.(1 - Dca)
  //   Da'  = Da  + Sa .(1 - Da )
  //
  //   Dca' = Dca + Sca.m.(1 - Dca)
  //   Da'  = Da  + Sa .m.(1 - Da )
  //
  // [Screen PRGBxXRGB] ~= [Screen PRGBxPRGB]
  //   Dca' = Dca + Sc.(1 - Dca)
  //   Da'  = Da  + 1 .(1 - Da )
  //
  //   Dca' = Dca + Sc.m.(1 - Dca)
  //   Da'  = Da  + 1 .m.(1 - Da )
  //
  // [Screen XRGBxPRGB] ~= [Screen PRGBxPRGB]
  //   Dc'  = Dc + Sca  .(1 - Dca)
  //   Dc'  = Dc + Sca.m.(1 - Dca)
  //
  // [Screen PRGBxPRGB] ~= [Screen PRGBxPRGB]
  //   Dc'  = Dc + Sc  .(1 - Dc)
  //   Dc'  = Dc + Sc.m.(1 - Dc)

  static constexpr BLCompOpSimplifyInfo screen(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? screen(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? screen(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? screen(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? screen(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(Screen, d, s);
  }

  // Overlay
  // -------
  //
  // [Overlay PRGBxPRGB]
  //   if (2.Dca < Da)
  //     Dca' = Dca + Sca - (Dca.Sa + Sca.Da - 2.Sca.Dca)
  //     Da'  = Da  + Sa  - Sa.Da
  //   else
  //     Dca' = Dca + Sca + (Dca.Sa + Sca.Da - 2.Sca.Dca) - Sa.Da
  //     Da'  = Da  + Sa  - Sa.Da
  //
  // [Overlay PRGBxXRGB]
  //   if (2.Dca < Da)
  //     Dca' = Sc.(2.Dca - Da + 1)
  //     Da'  = 1
  //   else
  //     Dca' = 2.Dca - Da - Sc.(1 - (2.Dca - Da))
  //     Da'  = 1
  //
  // [Overlay XRGBxPRGB]
  //   if (2.Dca < Da)
  //     Dc'  = Dc - (Dc.Sa - 2.Sca.Dc)
  //   else
  //     Dc'  = Dc + 2.Sca - Sa + (Dca.Sa - 2.Sca.Dc)
  //
  // [Overlay XRGBxXRGB]
  //   if (2.Dc < 1)
  //     Dc'  = 2.Dc.Sc
  //   else
  //     Dc'  = 2.(Dc + Sc) - 2.Sc.Dc - 1
  static constexpr BLCompOpSimplifyInfo overlay(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? overlay(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? overlay(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(Overlay, d, s);
  }

  // Darken
  // ------
  //
  // [Darken PRGBxPRGB]
  //   Dca' = min(Sca.Da, Dca.Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
  //   Da'  = min(Sa .Da, Da .Sa) + Sa .(1 - Da) + Da .(1 - Sa)
  //        = Sa + Da - Sa.Da
  //
  //   Dca' = min(Sca.m.Da, Dca.Sa.m) + Sca.m.(1 - Da) + Dca.(1 - Sa.m)
  //   Da'  = min(Sa .m.Da, Da .Sa.m) + Sa .m.(1 - Da) + Da .(1 - Sa.m)
  //        = Sa.m + Da - Sa.m.Da
  //
  // [Darken PRGBxXRGB]
  //   Dca' = min(Sc.Da, Dca) + Sc.(1 - Da)
  //   Da'  = min(1 .Da, Da ) + 1 .(1 - Da)
  //        = Sa + Da - Sa.Da
  //
  //   Dca' = min(Sc.m.Da, Dca.m) + Sc.m.(1 - Da) + Dca.(1 - 1.m)
  //   Da'  = min(1 .m.Da, Da .m) + 1 .m.(1 - Da) + Da .(1 - 1.m)
  //        = 1.m + Da - 1.m.Da
  //
  // [Darken XRGBxPRGB]
  //   Dc'  = min(Sca  , Dc.Sa  ) + Dc.(1 - Sa  )
  //   Dc'  = min(Sca.m, Dc.Sa.m) + Dc.(1 - Sa.m)
  //
  // [Darken XRGBxXRGB]
  //   Dc'  = min(Sc, Dc)
  //   Dc'  = min(Sc, Dc).m + Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo darken(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? darken(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? darken(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? dstOver(d, s) :

           makeOp(Darken, d, s);
  }

  // Lighten
  // -------
  //
  // [Lighten PRGBxPRGB]
  //   Dca' = max(Sca.Da, Dca.Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
  //   Da'  = max(Sa .Da, Da .Sa) + Sa .(1 - Da) + Da .(1 - Sa)
  //        = Sa + Da - Sa.Da
  //
  //   Dca' = max(Sca.m.Da, Dca.Sa.m) + Sca.m.(1 - Da) + Dca.(1 - Sa.m)
  //   Da'  = max(Sa .m.Da, Da .Sa.m) + Sa .m.(1 - Da) + Da .(1 - Sa.m)
  //        = Sa.m + Da - Sa.m.Da
  //
  // [Lighten PRGBxXRGB]
  //   Dca' = max(Sc.Da, Dca) + Sc.(1 - Da)
  //   Da'  = max(1 .Da, Da ) + 1 .(1 - Da)
  //        = Sa + Da - Sa.Da
  //
  //   Dca' = max(Sc.m.Da, Dca.m) + Sc.m.(1 - Da) + Dca.(1 - 1.m)
  //   Da'  = max(1 .m.Da, Da .m) + 1 .m.(1 - Da) + Da .(1 - 1.m)
  //        = 1.m + Da - 1.m.Da
  //
  // [Lighten XRGBxPRGB]
  //   Dc'  = max(Sca  , Dc.Sa  ) + Dc.(1 - Sa  )
  //   Dc'  = max(Sca.m, Dc.Sa.m) + Dc.(1 - Sa.m)
  //
  // [Lighten XRGBxXRGB]
  //   Dc'  = max(Sc, Dc)
  //   Dc'  = max(Sc, Dc).m + Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo lighten(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? lighten(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? lighten(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(Lighten, d, s);
  }

  // ColorDodge
  // ----------
  //
  // [ColorDodge PRGBxPRGB]
  //   Dca' = min(Dca.Sa.Sa / max(Sa - Sca, 0.001), Da.Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
  //   Da'  = Sa + Da - Sa.Da
  //
  //   Dca' = min(Dca.Sa.m.Sa.m / max(Sa.m - Sca.m, 0.001), Da.Sa.m) + Sca.m.(1 - Da) + Dca.(1 - Sa.m)
  //   Da'  = Sa.m + Da - Sa.m.Da
  //
  // [ColorDodge PRGBxXRGB]
  //   Dca' = min(Dca / max(1 - Sc, 0.001), Da) + Sc.(1 - Da)
  //   Da'  = 1
  //
  //   Dca' = min(Dca.1.m.1.m / max(1.m - Sc.m, 0.001), Da.1.m) + Sc.m.(1 - Da) + Dca.(1 - 1.m)
  //   Da'  = 1.m + Da - 1.m.Da
  //
  // [ColorDodge XRGBxPRGB]
  //   Dc'  = min(Dc.Sa  .Sa   / max(Sa   - Sca  , 0.001), Sa)   + Dc.(1 - Sa)
  //   Dc'  = min(Dc.Sa.m.Sa.m / max(Sa.m - Sca.m, 0.001), Sa.m) + Dc.(1 - Sa.m)
  //
  // [ColorDodge XRGBxXRGB]
  //   Dc'  = min(Dc / max(1 - Sc, 0.001), 1)
  //   Dc'  = min(Dc / max(1 - Sc, 0.001), 1).m + Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo colorDodge(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? colorDodge(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? colorDodge(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(ColorDodge, d, s);
  }

  // ColorBurn
  // ---------
  //
  // [ColorBurn PRGBxPRGB]
  //   Dca' = Sa.Da - min(Sa.Da, (Da - Dca).Sa.Sa / max(Sca, 0.001)) + Sca.(1 - Da) + Dca.(1 - Sa)
  //   Da'  = Sa + Da - Sa.Da
  //
  //   Dca' = Sa.m.Da - min(Sa.m.Da, (Da - Dca).Sa.m.Sa.m / max(Sca.m, 0.001)) + Sca.m.(1 - Da) + Dca.(1 - Sa.m)
  //   Da'  = Sa.m + Da - Sa.m.Da
  //
  // [ColorBurn PRGBxXRGB]
  //   Dca' = 1.Da - min(Da, (Da - Dca) / max(Sc, 0.001)) + Sc.(1 - Da)
  //   Da'  = 1
  //
  //   Dca' = m.Da - min(1.m.Da, (Da - Dca).1.m.1.m / max(Sc.m, 0.001)) + Sc.m.(1 - Da) + Dca.(1 - 1.m)
  //   Da'  = 1.m + Da - 1.m.Da
  //
  // [ColorBurn XRGBxPRGB]
  //   Dc'  = Sa   - min(Sa  , (1 - Dc).Sa  .Sa   / max(Sca  , 0.001)) + Dc.(1 - Sa)
  //   Dc'  = Sa.m - min(Sa.m, (1 - Dc).Sa.m.Sa.m / max(Sca.m, 0.001)) + Dc.(1 - Sa.m)
  //
  // [ColorBurn XRGBxXRGB]
  //   Dc'  = (1 - min(1, (1 - Dc) / max(Sc, 0.001)))
  //   Dc'  = (1 - min(1, (1 - Dc) / max(Sc, 0.001))).m + Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo colorBurn(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? colorBurn(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? colorBurn(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? dstOver(d, s) :

           makeOp(ColorBurn, d, s);
  }

  // LinearBurn
  // ----------
  //
  // [LinearBurn PRGBxPRGB]
  //   Dca' = Clamp(Dca + Sca - Sa.Da)
  //   Da'  = Da + Sa - Sa.Da
  //
  //   Dca' = Clamp(Dca + Sca - Sa.Da).m + Dca.(1 - m)
  //   Da'  = Sa.m.(1 - Da) + Da
  //
  // [LinearBurn PRGBxXRGB]
  //   Dca' = Clamp(Dca + Sc - Da)
  //   Da'  = 1
  //
  //   Dca' = Clamp(Dca + Sc - Da).m + Dca.(1 - m)
  //   Da'  = Da + Sa - Sa.Da
  //
  // [LinearBurn XRGBxPRGB]
  //   Dc'  = Clamp(Dc + Sca - Sa)
  //   Dc'  = Clamp(Dc + Sca - Sa).m + Dc.(1 - m)
  //
  // [LinearBurn XRGBxXRGB]
  //   Dc'  = Clamp(Dc + Sc - 1)
  //   Dc'  = Clamp(Dc + Sc - 1).m + Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo linearBurn(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? linearBurn(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? linearBurn(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? dstOver(d, s) :

           makeOp(LinearBurn, d, s);
  }

  // LinearLight
  // -----------
  //
  // [LinearLight PRGBxPRGB]
  //   Dca' = min(max(Dca.Sa + 2.Sca.Da - Sa.Da, 0), Sa.Da) + Sca.(1 - Da) + Dca.(1 - Sa)
  //   Da'  = Da + Sa - Sa.Da
  //
  //   Dca' = min(max((Dca.Sa.m + 2.Sca.m.Da - Sa.m.Da), 0), Sa.m.Da) + Sca.m.(1 - Da) + Dca.(1 - Sa.m)
  //   Da'  = Da + Sa.m - Sa.m.Da
  //
  // [LinearLight PRGBxXRGB]
  //   Dca' = min(max((Dca + 2.Sc.Da - Da), 0), Da) + Sc.(1 - Da)
  //   Da'  = 1
  //
  //   Dca' = min(max((Dca.1.m + 2.Sc.m.Da - 1.m.Da), 0), 1.m.Da) + Sc.m.(1 - Da) + Dca.(1 - m)
  //   Da'  = Da + Sa.m - Sa.m.Da
  //
  // [LinearLight XRGBxPRGB]
  //   Dca' = min(max((Dc.Sa   + 2.Sca   - Sa  ), 0), Sa  ) + Dca.(1 - Sa)
  //   Dca' = min(max((Dc.Sa.m + 2.Sca.m - Sa.m), 0), Sa.m) + Dca.(1 - Sa.m)
  //
  // [LinearLight XRGBxXRGB]
  //   Dc'  = min(max((Dc + 2.Sc - 1), 0), 1)
  //   Dc'  = min(max((Dc + 2.Sc - 1), 0), 1).m + Dca.(1 - m)
  static constexpr BLCompOpSimplifyInfo linearLight(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? linearLight(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? linearLight(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(LinearLight, d, s);
  }

  // PinLight
  // --------
  //
  // [PinLight PRGBxPRGB]
  //   if 2.Sca <= Sa
  //     Dca' = min(Dca + Sca - Sca.Da, Dca + Sca + Sca.Da - Dca.Sa)
  //     Da'  = min(Da  + Sa  - Sa .Da, Da  + Sa  + Sa .Da - Da .Sa) = Da + Sa.(1 - Da)
  //   else
  //     Dca' = max(Dca + Sca - Sca.Da, Dca + Sca + Sca.Da - Dca.Sa - Da.Sa)
  //     Da'  = max(Da  + Sa  - Sa .Da, Da  + Sa  + Sa .Da - Da .Sa - Da.Sa) = Da + Sa.(1 - Da)
  //
  //   if 2.Sca.m <= Sa.m
  //     Dca' = min(Dca.Sa.m, 2.Sca.m.Da) + Sca.m.(1 - Da) + Dca.(1 - Sa.m)
  //     Da'  = Da + Sa.m.(1 - Da)
  //   else
  //     Dca' = max(Dca.Sa.m, 2.Sca.m.Da - Sa.m.Da) + Sca.m.(1 - Da) + Dca.(1 - Sa.m)
  //     Da'  = Da + Sa.m.(1 - Da)
  //
  // [PinLight PRGBxXRGB]
  //   if 2.Sc <= 1
  //     Dca' = min(Dca, 2.Sc.Da) + Sc.(1 - Da)
  //     Da'  = 1
  //   else
  //     Dca' = max(Dca, 2.Sc.Da - Da) + Sc.(1 - Da)
  //     Da'  = 1
  //
  //   if 2.Sc.m <= 1.m
  //     Dca' = min(Dca.m, 2.Sc.m.Da) + Sc.m.(1 - Da) + Dca.(1 - m)
  //     Da'  = Da + m.(1 - Da)
  //   else
  //     Dca' = max(Dca.m, 2.Sc.m.Da - m.Da) + Sc.m.(1 - Da) + Dc.(1 - m)
  //     Da'  = Da + m.(1 - Da)
  //
  // [PinLight XRGBxPRGB]
  //   if 2.Sca <= Sa
  //     Dc'  = min(Dc.Sa, 2.Sca) + Dc.(1 - Sa)
  //   else
  //     Dc'  = max(Dc.Sa, 2.Sca - Sa) + Dc.(1 - Sa)
  //
  //   if 2.Sca.m <= Sa.m
  //     Dc'  = min(Dc.Sa.m, 2.Sca.m) + Dc.(1 - Sa.m)
  //   else
  //     Dc'  = max(Dc.Sa.m, 2.Sca.m - Sa.m) + Dc.(1 - Sa.m)
  //
  // [PinLight XRGBxXRGB]
  //   if 2.Sc <= 1
  //     Dc'  = min(Dc, 2.Sc)
  //   else
  //     Dc'  = max(Dc, 2.Sc - 1)
  //
  //   if 2.Sca.m <= Sa.m
  //     Dc'  = min(Dc, 2.Sc).m + Dca.(1 - m)
  //   else
  //     Dc'  = max(Dc, 2.Sc - 1).m + Dca.(1 - m)
  static constexpr BLCompOpSimplifyInfo pinLight(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? pinLight(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? pinLight(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(PinLight, d, s);
  }

  // HardLight
  // ---------
  //
  // [HardLight PRGBxPRGB]
  //   if (2.Sca <= Sa)
  //     Dca' = 2.Sca.Dca + Sca.(1 - Da) + Dca.(1 - Sa)
  //     Da'  = Sa + Da - Sa.Da
  //   else
  //     Dca' = Sa.Da - 2.(Da - Dca).(Sa - Sca) + Sca.(1 - Da) + Dca.(1 - Sa)
  //     Da'  = Sa + Da - Sa.Da
  //
  //   if (2.Sca.m <= Sa.m)
  //     Dca' = 2.Sca.m.Dca + Sca.m(1 - Da) + Dca.(1 - Sa.m)
  //     Da'  = Sa.m + Da - Sa.m.Da
  //   else
  //     Dca' = Sa.m.Da - 2.(Da - Dca).(Sa.m - Sca.m) + Sca.m.(1 - Da) + Dca.(1 - Sa.m)
  //     Da'  = Sa.m + Da - Sa.m.Da
  //
  // [HardLight PRGBxXRGB]
  //   if (2.Sc <= 1)
  //     Dca' = 2.Sc.Dca + Sc.(1 - Da)
  //     Da'  = 1
  //   else
  //     Dca' = Da - 2.(Da - Dca).(1 - Sc) + Sc.(1 - Da)
  //     Da'  = 1
  //
  //   if (2.Sc.m <= m)
  //     Dca' = 2.Sc.m.Dca + Sc.m(1 - Da) + Dca.(1 - m)
  //     Da'  = Da + m.(1 - Da)
  //   else
  //     Dca' = 1.m.Da - 2.(Da - Dca).((1 - Sc).m) + Sc.m.(1 - Da) + Dca.(1 - m)
  //     Da'  = Da + m.(1 - Da)
  //
  // [HardLight XRGBxPRGB]
  //   if (2.Sca <= Sa)
  //     Dc'  = 2.Sca.Dc + Dc.(1 - Sa)
  //   else
  //     Dc'  = Sa - 2.(1 - Dc).(Sa - Sca) + Dc.(1 - Sa)
  //
  //   if (2.Sca.m <= Sa.m)
  //     Dc'  = 2.Sca.m.Dc + Dc.(1 - Sa.m)
  //   else
  //     Dc'  = Sa.m - 2.(1 - Dc).(Sa.m - Sca.m) + Dc.(1 - Sa.m)
  //
  // [HardLight XRGBxXRGB]
  //   if (2.Sc <= 1)
  //     Dc'  = 2.Sc.Dc
  //   else
  //     Dc'  = 1 - 2.(1 - Dc).(1 - Sc)
  //
  //   if (2.Sc.m <= 1.m)
  //     Dc'  = 2.Sc.Dc.m + Dc.(1 - m)
  //   else
  //     Dc'  = (1 - 2.(1 - Dc).(1 - Sc)).m - Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo hardLight(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? hardLight(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? hardLight(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(HardLight, d, s);
  }

  // SoftLight
  // ---------
  //
  // [SoftLight PRGBxPRGB]
  //   Dc = Dca/Da
  //   if 2.Sca - Sa <= 0
  //     Dca' = Dca + Sca.(1 - Da) + (2.Sca - Sa).Da.[[              Dc.(1 - Dc)           ]]
  //     Da'  = Da + Sa - Sa.Da
  //   else if 2.Sca - Sa > 0 and 4.Dc <= 1
  //     Dca' = Dca + Sca.(1 - Da) + (2.Sca - Sa).Da.[[ 4.Dc.(4.Dc.Dc + Dc - 4.Dc + 1) - Dc]]
  //     Da'  = Da + Sa - Sa.Da
  //   else
  //     Dca' = Dca + Sca.(1 - Da) + (2.Sca - Sa).Da.[[             sqrt(Dc) - Dc          ]]
  //     Da'  = Da + Sa - Sa.Da
  //
  // [SoftLight XRGBxXRGB]
  //   if 2.Sc <= 1
  //     Dc' = Dc + (2.Sc - 1).[[              Dc.(1 - Dc)           ]]
  //   else if 2.Sc > 1 and 4.Dc <= 1
  //     Dc' = Dc + (2.Sc - 1).[[ 4.Dc.(4.Dc.Dc + Dc - 4.Dc + 1) - Dc]]
  //   else
  //     Dc' = Dc + (2.Sc - 1).[[             sqrt(Dc) - Dc          ]]
  static constexpr BLCompOpSimplifyInfo softLight(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? softLight(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? softLight(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(SoftLight, d, s);
  }

  // Difference
  // ----------
  //
  // [Difference PRGBxPRGB]
  //   Dca' = Dca + Sca - 2.min(Sca.Da, Dca.Sa)
  //   Da'  = Sa + Da - Sa.Da
  //
  //   Dca' = Dca + Sca.m - 2.min(Sca.m.Da, Dca.Sa.m)
  //   Da'  = Sa.m + Da - Sa.m.Da
  //
  // [Difference PRGBxXRGB]
  //   Dca' = Dca + Sc - 2.min(Sc.Da, Dca)
  //   Da'  = 1
  //
  //   Dca' = Dca + Sc.m - 2.min(Sc.m.Da, Dca)
  //   Da'  = Da + 1.m - m.Da
  //
  // [Difference XRGBxPRGB]
  //   Dc'  = Dc + Sca   - 2.min(Sca  , Dc.Sa)
  //   Dc'  = Dc + Sca.m - 2.min(Sca.m, Dc.Sa.m)
  //
  // [Difference XRGBxXRGB]
  //   Dc'  = Dc + Sc   - 2.min(Sc  , Dc  )
  //   Dc'  = Dc + Sc.m - 2.min(Sc.m, Dc.m)
  static constexpr BLCompOpSimplifyInfo difference(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? difference(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? difference(Fmt::kXRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(Difference, d, s);
  }

  // Exclusion
  // ---------
  //
  // [Exclusion PRGBxPRGB]
  //   Dca' = Dca + Sca.(Da - 2.Dca)
  //   Da'  = Da  + Sa - Sa.Da
  //
  //   Dca' = Dca + Sca.m.(Da - 2.Dca)
  //   Da'  = Da  + Sa.m - Sa.m.Da
  //
  // [Exclusion PRGBxXRGB] ~= [Exclusion PRGBxPRGB]
  //   Dca' = Dca + Sc.(Da - 2.Dca)
  //   Da'  = Da  + 1 - 1.Da
  //
  //   Dca' = Dca + Sc.m.(Da - 2.Dca)
  //   Da'  = Da  + 1.m - 1.m.Da
  //
  // [Exclusion XRGBxPRGB]
  //   Dc'  = Dc + Sca  .(1 - 2.Dc)
  //   Dc'  = Dc + Sca.m.(1 - 2.Dc)
  //
  // [Exclusion XRGBxXRGB] ~= [Exclusion XRGBxPRGB]
  //   Dc'  = Dc + Sc  .(1 - 2.Dc)
  //   Dc'  = Dc + Sc.m.(1 - 2.Dc)
  static constexpr BLCompOpSimplifyInfo exclusion(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? exclusion(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? exclusion(Fmt::kXRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(Exclusion, d, s);
  }

  // HACK: MSVC has a problem with code that does multiple ternary operations (? :)
  //       so we had to split it so Blend2D can compile. So please don't be active
  //       here and don't try to join these functions as you would break MSVC builds.
  static constexpr BLCompOpSimplifyInfo valueDecomposed_1(uint32_t compOp, Fmt d, Fmt s) noexcept {
    return compOp == BL_COMP_OP_SRC_COPY     ? srcCopy(d, s)     :
           compOp == BL_COMP_OP_SRC_OVER     ? srcOver(d, s)     :
           compOp == BL_COMP_OP_SRC_IN       ? srcIn(d, s)       :
           compOp == BL_COMP_OP_SRC_OUT      ? srcOut(d, s)      :
           compOp == BL_COMP_OP_SRC_ATOP     ? srcAtop(d, s)     :
           compOp == BL_COMP_OP_DST_COPY     ? dstCopy(d, s)     :
           compOp == BL_COMP_OP_DST_OVER     ? dstOver(d, s)     :
           compOp == BL_COMP_OP_DST_IN       ? dstIn(d, s)       :
           compOp == BL_COMP_OP_DST_OUT      ? dstOut(d, s)      :
           compOp == BL_COMP_OP_DST_ATOP     ? dstAtop(d, s)     : dstCopy(d, s);
  }

  static constexpr BLCompOpSimplifyInfo valueDecomposed_2(uint32_t compOp, Fmt d, Fmt s) noexcept {
    return compOp == BL_COMP_OP_XOR          ? xor_(d, s)        :
           compOp == BL_COMP_OP_CLEAR        ? clear(d, s)       :
           compOp == BL_COMP_OP_PLUS         ? plus(d, s)        :
           compOp == BL_COMP_OP_MINUS        ? minus(d, s)       :
           compOp == BL_COMP_OP_MODULATE     ? modulate(d, s)    :
           compOp == BL_COMP_OP_MULTIPLY     ? multiply(d, s)    :
           compOp == BL_COMP_OP_SCREEN       ? screen(d, s)      :
           compOp == BL_COMP_OP_OVERLAY      ? overlay(d, s)     :
           compOp == BL_COMP_OP_DARKEN       ? darken(d, s)      :
           compOp == BL_COMP_OP_LIGHTEN      ? lighten(d, s)     : valueDecomposed_1(compOp, d, s);
  }

  static constexpr BLCompOpSimplifyInfo valueDecomposed_3(uint32_t compOp, Fmt d, Fmt s) noexcept {
    return compOp == BL_COMP_OP_COLOR_DODGE  ? colorDodge(d, s)  :
           compOp == BL_COMP_OP_COLOR_BURN   ? colorBurn(d, s)   :
           compOp == BL_COMP_OP_LINEAR_BURN  ? linearBurn(d, s)  :
           compOp == BL_COMP_OP_LINEAR_LIGHT ? linearLight(d, s) :
           compOp == BL_COMP_OP_PIN_LIGHT    ? pinLight(d, s)    :
           compOp == BL_COMP_OP_HARD_LIGHT   ? hardLight(d, s)   :
           compOp == BL_COMP_OP_SOFT_LIGHT   ? softLight(d, s)   :
           compOp == BL_COMP_OP_DIFFERENCE   ? difference(d, s)  :
           compOp == BL_COMP_OP_EXCLUSION    ? exclusion(d, s)   :

           // Internal operators, only used to simplify others.
           compOp == BL_COMP_OP_INTERNAL_ALPHA_INV ? alphaInv(d, s) : valueDecomposed_2(compOp, d, s);
  }

  // Just dispatches to the respective composition operator.
  static constexpr BLCompOpSimplifyInfo valueDecomposed(uint32_t compOp, Fmt d, Fmt s) noexcept {
    return valueDecomposed_3(compOp, d, s);
  }

  // Function called by the table generator, decompose and continue...
  static constexpr BLCompOpSimplifyInfo value(size_t index) noexcept {
    return valueDecomposed(uint32_t((index / BL_FORMAT_RESERVED_COUNT)) % uint32_t(BL_COMP_OP_INTERNAL_COUNT),
                           Fmt(index / (uint32_t(BL_COMP_OP_INTERNAL_COUNT) * BL_FORMAT_RESERVED_COUNT)),
                           Fmt(index % BL_FORMAT_RESERVED_COUNT));
  }
};

template<BLInternalFormat Dst>
struct BLSimplifyInfoRecordSetGen {
  // Function called by the table generator, decompose and continue...
  static constexpr BLCompOpSimplifyInfo value(size_t index) noexcept {
    return BLCompOpSimplifyInfoGen::valueDecomposed(uint32_t(index / BL_FORMAT_RESERVED_COUNT), Dst, BLInternalFormat(index % BL_FORMAT_RESERVED_COUNT));
  }
};

// HACK: MSVC doesn't honor constexpr functions and sometimes outputs initialization
//       code even when the expression can be calculated at compile time. To fix this
//       we go throught an additional constexpr to force the compiler to always generate
//       our lookup tables at compile time.
//
// Additionally, if there is a mistake leading to recursion the compiler would catch it
// at compile-time instead of hitting it at runtime during initialization.
static_assert(BL_FORMAT_MAX_VALUE == 3u, "Don't forget to add new formats to blCompOpSimplifyInfoTable");
static constexpr const BLCompOpSimplifyInfoTable blCompOpSimplifyInfoTable_ = {{
  blMakeLookupTable<BLCompOpSimplifyInfo, BL_COMP_OP_SIMPLIFY_RECORD_SIZE, BLSimplifyInfoRecordSetGen<BLInternalFormat(0)>>(),
  blMakeLookupTable<BLCompOpSimplifyInfo, BL_COMP_OP_SIMPLIFY_RECORD_SIZE, BLSimplifyInfoRecordSetGen<BLInternalFormat(1)>>(),
  blMakeLookupTable<BLCompOpSimplifyInfo, BL_COMP_OP_SIMPLIFY_RECORD_SIZE, BLSimplifyInfoRecordSetGen<BLInternalFormat(2)>>(),
  blMakeLookupTable<BLCompOpSimplifyInfo, BL_COMP_OP_SIMPLIFY_RECORD_SIZE, BLSimplifyInfoRecordSetGen<BLInternalFormat(3)>>()
}};
const BLCompOpSimplifyInfoTable blCompOpSimplifyInfoTable = blCompOpSimplifyInfoTable_;
