// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blcompop_p.h"

// ============================================================================
// [BLCompOpInfo]
// ============================================================================

struct BLCompOpInfoGen {
  #define F(VALUE) BL_COMP_OP_FLAG_##VALUE

  static constexpr BLCompOpInfo value(size_t op) noexcept {
    return BLCompOpInfo {
      op == BL_COMP_OP_SRC_OVER           ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_SRC_COPY           ? F(TYPE_B) | 0     | 0     | F(SC) | F(SA) | 0              | 0              :
      op == BL_COMP_OP_SRC_IN             ? F(TYPE_B) | 0     | F(DA) | F(SC) | F(SA) | F(NOP_IF_DA_0) | 0              :
      op == BL_COMP_OP_SRC_OUT            ? F(TYPE_B) | 0     | F(DA) | F(SC) | F(SA) | 0              | 0              :
      op == BL_COMP_OP_SRC_ATOP           ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | F(NOP_IF_DA_0) | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_DST_OVER           ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | F(NOP_IF_DA_1) | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_DST_COPY           ? F(TYPE_C) | F(DC) | F(DA) | 0     | 0     | F(NOP)         | F(NOP)         :
      op == BL_COMP_OP_DST_IN             ? F(TYPE_B) | F(DC) | F(DA) | 0     | F(SA) | 0              | F(NOP_IF_SA_1) :
      op == BL_COMP_OP_DST_OUT            ? F(TYPE_A) | F(DC) | F(DA) | 0     | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_DST_ATOP           ? F(TYPE_B) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | 0              :
      op == BL_COMP_OP_XOR                ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_CLEAR              ? F(TYPE_C) | 0     | 0     | 0     | 0     | F(NOP_IF_DA_0) | 0              :

      op == BL_COMP_OP_PLUS               ? F(TYPE_C) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_MINUS              ? F(TYPE_C) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_MULTIPLY           ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | F(NOP_IF_DA_0) | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_SCREEN             ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_OVERLAY            ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_DARKEN             ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_LIGHTEN            ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_COLOR_DODGE        ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_COLOR_BURN         ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_LINEAR_BURN        ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_LINEAR_LIGHT       ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_PIN_LIGHT          ? F(TYPE_C) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_HARD_LIGHT         ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_SOFT_LIGHT         ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_DIFFERENCE         ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :
      op == BL_COMP_OP_EXCLUSION          ? F(TYPE_A) | F(DC) | F(DA) | F(SC) | F(SA) | 0              | F(NOP_IF_SA_0) :

      op == BL_COMP_OP_INTERNAL_ALPHA_SET ? F(TYPE_C) | 0     | 0     | 0     | 0     | F(NOP_IF_SA_1) | 0              :
      op == BL_COMP_OP_INTERNAL_ALPHA_INV ? F(TYPE_C) | 0     | 0     | 0     | 0     | 0              | 0              : 0
    };
  }

  #undef F
};

const BLLookupTable<BLCompOpInfo, BL_COMP_OP_INTERNAL_COUNT> blCompOpInfo =
  blLookupTable<BLCompOpInfo, BL_COMP_OP_INTERNAL_COUNT, BLCompOpInfoGen>();

// ============================================================================
// [BLCompOpSimplifyInfo]
// ============================================================================

struct BLCompOpSimplifyInfoGen {
  // Simplify formats as we work with them here a lot.
  enum Format : uint32_t {
    NONE   = BL_FORMAT_NONE,
    A8     = BL_FORMAT_A8,
    PRGB32 = BL_FORMAT_PRGB32,
    ZERO32 = BL_FORMAT_ZERO32,
    XRGB32 = BL_FORMAT_XRGB32,
    FRGB32 = BL_FORMAT_FRGB32
  };

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
    Exclusion   = BL_COMP_OP_EXCLUSION
  };

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

  static constexpr BLCompOpSimplifyInfo makeOp(uint32_t compOp, uint32_t d, uint32_t s) noexcept {
    return BLCompOpSimplifyInfo { uint8_t(compOp), uint8_t(BL_COMP_OP_SOLID_ID_NONE), uint8_t(d), uint8_t(s) };
  }

  static constexpr BLCompOpSimplifyInfo transparent(uint32_t compOp, uint32_t d, uint32_t s) noexcept {
    return BLCompOpSimplifyInfo { uint8_t(compOp), uint8_t(BL_COMP_OP_SOLID_ID_TRANSPARENT), uint8_t(d), uint8_t(s) };
  }

  static constexpr BLCompOpSimplifyInfo opaqueBlack(uint32_t compOp, uint32_t d, uint32_t s) noexcept {
    return BLCompOpSimplifyInfo { uint8_t(compOp), uint8_t(BL_COMP_OP_SOLID_ID_OPAQUE_BLACK), uint8_t(d), uint8_t(s) };
  }

  static constexpr BLCompOpSimplifyInfo opaqueWhite(uint32_t compOp, uint32_t d, uint32_t s) noexcept {
    return BLCompOpSimplifyInfo { uint8_t(compOp), uint8_t(BL_COMP_OP_SOLID_ID_OPAQUE_WHITE), uint8_t(d), uint8_t(s) };
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
  static constexpr BLCompOpSimplifyInfo clear(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 ? transparent(SrcCopy, PRGB32, PRGB32) :
           d == XRGB32 ? opaqueBlack(SrcCopy, PRGB32, PRGB32) : makeOp(Clear, d, s);
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
  // [Src XRGBxPRGB] ~= [Src PRGBxPRGB]
  //   Dc'  = Sca                            Dc'  = Sca.m + Dc.(1 - m)
  //
  // [Src XRGBxXRGB] ~= [Src PRGBxPRGB]
  //   Dc'  = Sc                             Dc'  = Sc.m + Dc.(1 - m)
  static constexpr BLCompOpSimplifyInfo srcCopy(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? makeOp(SrcCopy, PRGB32, PRGB32) :
           d == PRGB32 && s == FRGB32 ? makeOp(SrcCopy, PRGB32, PRGB32) :
           d == XRGB32 && s == PRGB32 ? makeOp(SrcCopy, PRGB32, PRGB32) :
           d == XRGB32 && s == ZERO32 ? makeOp(SrcCopy, PRGB32, PRGB32) :
           d == XRGB32 && s == XRGB32 ? makeOp(SrcCopy, PRGB32, PRGB32) :
           d == XRGB32 && s == FRGB32 ? makeOp(SrcCopy, PRGB32, PRGB32) : makeOp(SrcCopy, d, s);
  }

  // DstCopy
  // -------
  //
  // [DstCopy ANYxANY]
  //   Dca' = Dca
  //   Da   = Da
  static constexpr BLCompOpSimplifyInfo dstCopy(uint32_t d, uint32_t s) noexcept {
    return transparent(DstCopy, d, s);
  }

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
  static constexpr BLCompOpSimplifyInfo srcOver(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(PRGB32, PRGB32) :
           d == PRGB32 && s == XRGB32 ? srcCopy(PRGB32, XRGB32) :
           d == PRGB32 && s == FRGB32 ? srcCopy(PRGB32, FRGB32) :
           d == XRGB32 && s == PRGB32 ? srcCopy(PRGB32, PRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(PRGB32, PRGB32) :
           d == XRGB32 && s == XRGB32 ? srcCopy(PRGB32, XRGB32) :
           d == XRGB32 && s == FRGB32 ? makeOp(SrcOver, PRGB32, FRGB32) : makeOp(SrcOver, d, s);
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
  static constexpr BLCompOpSimplifyInfo dstOver(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(PRGB32, PRGB32) :
           d == PRGB32 && s == FRGB32 ? dstOver(PRGB32, PRGB32) :
           d == XRGB32                ? dstCopy(d, s) : makeOp(DstOver, d, s);
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
  static constexpr BLCompOpSimplifyInfo srcIn(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? srcIn(PRGB32, PRGB32) :
           d == PRGB32 && s == FRGB32 ? srcIn(PRGB32, PRGB32) :
           d == XRGB32                ? srcCopy(d, s) : makeOp(SrcIn, d, s);
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
  static constexpr BLCompOpSimplifyInfo dstIn(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? srcCopy(d, s) :
           d == PRGB32 && s == XRGB32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? dstCopy(d, s) :
           d == XRGB32 && s == PRGB32 ? dstIn(PRGB32, PRGB32) :
           d == XRGB32 && s == ZERO32 ? dstIn(PRGB32, FRGB32) :
           d == XRGB32 && s == FRGB32 ? dstCopy(d, s) :
           d == XRGB32 && s == XRGB32 ? dstCopy(d, s) : makeOp(DstIn, d, s);
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
  static constexpr BLCompOpSimplifyInfo srcOut(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? clear(d, s) :
           d == PRGB32 && s == FRGB32 ? srcOut(PRGB32, PRGB32) :
           d == XRGB32                ? clear(d, s) : makeOp(SrcOut, d, s);
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
  static constexpr BLCompOpSimplifyInfo dstOut(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? clear(d, s) :
           d == PRGB32 && s == XRGB32 ? clear(d, s) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? clear(d, s) :
           d == XRGB32 && s == XRGB32 ? clear(d, s) : makeOp(DstOut, d, s);
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
  static constexpr BLCompOpSimplifyInfo srcAtop(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? srcIn(d, s) :
           d == PRGB32 && s == XRGB32 ? srcIn(d, s) :
           d == XRGB32 && s == PRGB32 ? srcOver(d, s) :
           d == XRGB32 && s == ZERO32 ? srcOver(d, s) :
           d == XRGB32 && s == FRGB32 ? srcCopy(d, s) :
           d == XRGB32 && s == XRGB32 ? srcCopy(d, s) : makeOp(SrcAtop, d, s);
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
  static constexpr BLCompOpSimplifyInfo dstAtop(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? clear(d, s) :
           d == PRGB32 && s == FRGB32 ? dstOver(d, s) :
           d == PRGB32 && s == XRGB32 ? dstOver(d, s) :
           d == XRGB32 && s == PRGB32 ? dstIn(d, s) :
           d == XRGB32 && s == ZERO32 ? clear(d, s) :
           d == XRGB32 && s == FRGB32 ? dstCopy(d, s) :
           d == XRGB32 && s == XRGB32 ? dstCopy(d, s) : makeOp(DstAtop, d, s);
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
  static constexpr BLCompOpSimplifyInfo xor_(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? srcOut(d, s) :
           d == PRGB32 && s == XRGB32 ? srcOut(d, s) :
           d == XRGB32 && s == PRGB32 ? dstOut(d, s) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? clear(d, s) :
           d == XRGB32 && s == XRGB32 ? clear(d, s) : makeOp(Xor, d, s);
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
  static constexpr BLCompOpSimplifyInfo plus(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? plus(PRGB32, PRGB32) :
           d == XRGB32 && s == PRGB32 ? plus(PRGB32, PRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? plus(PRGB32, PRGB32) :
           d == XRGB32 && s == XRGB32 ? plus(PRGB32, PRGB32) : makeOp(Plus, d, s);
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
  static constexpr BLCompOpSimplifyInfo minus(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? minus(PRGB32, PRGB32) :
           d == XRGB32 && s == PRGB32 ? minus(PRGB32, PRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? minus(PRGB32, PRGB32) :
           d == XRGB32 && s == XRGB32 ? minus(PRGB32, PRGB32) : makeOp(Minus, d, s);
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
  // [Multiply XRGBxPRGB]
  //   Dc'  = Dc.(Sca   + 1 - Sa  )
  //   Dc'  = Dc.(Sca.m + 1 - Sa.m)
  //
  // [Multiply XRGBxXRGB]
  //   Dc'  = Dc.(Sc   + 1 - 1  )
  //   Dc'  = Dc.(Sc.m + 1 - 1.m)
  static constexpr BLCompOpSimplifyInfo multiply(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? minus(PRGB32, XRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? minus(XRGB32, XRGB32) : makeOp(Multiply, d, s);
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
  static constexpr BLCompOpSimplifyInfo screen(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? screen(PRGB32, PRGB32) :
           d == XRGB32 && s == PRGB32 ? screen(PRGB32, PRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? screen(PRGB32, PRGB32) :
           d == XRGB32 && s == XRGB32 ? screen(PRGB32, XRGB32) : makeOp(Screen, d, s);
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
  //   if (2.Dca - Da < 0)
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
  //   if (2.Dc - 1 < 0)
  //     Dc'  = 2.Dc.Sc
  //   else
  //     Dc'  = 2.(Dc + Sc) - 2.Sc.Dc - 1
  static constexpr BLCompOpSimplifyInfo overlay(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? overlay(PRGB32, XRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? overlay(XRGB32, XRGB32) : makeOp(Screen, d, s);
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
  static constexpr BLCompOpSimplifyInfo darken(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? darken(PRGB32, XRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? darken(XRGB32, XRGB32) : makeOp(Darken, d, s);
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
  static constexpr BLCompOpSimplifyInfo lighten(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? lighten(PRGB32, XRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? lighten(XRGB32, XRGB32) : makeOp(Lighten, d, s);
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
  static constexpr BLCompOpSimplifyInfo colorDodge(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? colorDodge(PRGB32, PRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? colorDodge(XRGB32, XRGB32) : makeOp(ColorDodge, d, s);
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
  static constexpr BLCompOpSimplifyInfo colorBurn(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? colorBurn(PRGB32, XRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? colorBurn(XRGB32, XRGB32) : makeOp(ColorBurn, d, s);
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
  static constexpr BLCompOpSimplifyInfo linearBurn(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? linearBurn(PRGB32, XRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? linearBurn(XRGB32, XRGB32) : makeOp(LinearBurn, d, s);
  }

  // LinearLight
  // -----------
  //
  // [LinearLight PRGBxPRGB]
  //   Dca' = min(max((Dca.Sa + 2.Sca.Da - Sa.Da), 0), Sa.Da) + Sca.(1 - Da) + Dca.(1 - Sa)
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
  static constexpr BLCompOpSimplifyInfo linearLight(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? linearLight(PRGB32, XRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? linearLight(XRGB32, XRGB32) : makeOp(LinearLight, d, s);
  }

  // PinLight
  // --------
  //
  // [PinLight PRGBxPRGB]
  //   if 2.Sca <= Sa
  //     Dca' = min(Dca.Sa, 2.Sca.Da) + Sca.(1 - Da) + Dca.(1 - Sa)
  //     Da'  = Da + Sa.(1 - Da)
  //   else
  //     Dca' = max(Dca.Sa, 2.Sca.Da - Sa.Da) + Sca.(1 - Da) + Dca.(1 - Sa)
  //     Da'  = Da + Sa.(1 - Da)
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
  static constexpr BLCompOpSimplifyInfo pinLight(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? pinLight(PRGB32, XRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? pinLight(XRGB32, XRGB32) : makeOp(PinLight, d, s);
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
  static constexpr BLCompOpSimplifyInfo hardLight(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? hardLight(PRGB32, XRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? hardLight(XRGB32, XRGB32) : makeOp(HardLight, d, s);
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
  static constexpr BLCompOpSimplifyInfo softLight(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? softLight(PRGB32, XRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? softLight(XRGB32, XRGB32) : makeOp(SoftLight, d, s);
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
  static constexpr BLCompOpSimplifyInfo difference(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? difference(PRGB32, PRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? difference(XRGB32, PRGB32) : makeOp(Difference, d, s);
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
  static constexpr BLCompOpSimplifyInfo exclusion(uint32_t d, uint32_t s) noexcept {
    return d == PRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == PRGB32 && s == FRGB32 ? exclusion(PRGB32, PRGB32) :
           d == XRGB32 && s == ZERO32 ? dstCopy(d, s) :
           d == XRGB32 && s == FRGB32 ? exclusion(XRGB32, PRGB32) : makeOp(Exclusion, d, s);
  }

  // HACK: MSVC has a problem with code that does multiple ternary operations (? :)
  //       so we had to split it so Blend2D can compile. So please don't be active
  //       here and don't try to join these functions otherwise you break MSVC builds.
  static constexpr BLCompOpSimplifyInfo valueDecomposed_1(uint32_t compOp, uint32_t d, uint32_t s) noexcept {
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

  static constexpr BLCompOpSimplifyInfo valueDecomposed_2(uint32_t compOp, uint32_t d, uint32_t s) noexcept {
    return compOp == BL_COMP_OP_XOR          ? xor_(d, s)        :
           compOp == BL_COMP_OP_CLEAR        ? clear(d, s)       :
           compOp == BL_COMP_OP_CLEAR        ? clear(d, s)       :
           compOp == BL_COMP_OP_PLUS         ? plus(d, s)        :
           compOp == BL_COMP_OP_MINUS        ? minus(d, s)       :
           compOp == BL_COMP_OP_MULTIPLY     ? multiply(d, s)    :
           compOp == BL_COMP_OP_SCREEN       ? screen(d, s)      :
           compOp == BL_COMP_OP_OVERLAY      ? overlay(d, s)     :
           compOp == BL_COMP_OP_DARKEN       ? darken(d, s)      :
           compOp == BL_COMP_OP_LIGHTEN      ? lighten(d, s)     : valueDecomposed_1(compOp, d, s);
  }

  // Just dispatches to the respective composition operator.
  static constexpr BLCompOpSimplifyInfo valueDecomposed_3(uint32_t compOp, uint32_t d, uint32_t s) noexcept {
    return compOp == BL_COMP_OP_COLOR_DODGE  ? colorDodge(d, s)  : 
           compOp == BL_COMP_OP_COLOR_BURN   ? colorBurn(d, s)   :
           compOp == BL_COMP_OP_LINEAR_BURN  ? linearBurn(d, s)  :
           compOp == BL_COMP_OP_LINEAR_LIGHT ? linearLight(d, s) :
           compOp == BL_COMP_OP_PIN_LIGHT    ? pinLight(d, s)    :
           compOp == BL_COMP_OP_HARD_LIGHT   ? hardLight(d, s)   :
           compOp == BL_COMP_OP_SOFT_LIGHT   ? softLight(d, s)   :
           compOp == BL_COMP_OP_DIFFERENCE   ? difference(d, s)  :
           compOp == BL_COMP_OP_EXCLUSION    ? exclusion(d, s)   : valueDecomposed_2(compOp, d, s);
  }

  // Function called by the table generator, decompose and continue...
  static constexpr BLCompOpSimplifyInfo value(size_t index) noexcept {
    return valueDecomposed_3(uint32_t((index / BL_FORMAT_RESERVED_COUNT) % BL_COMP_OP_INTERNAL_COUNT),
                             uint32_t(index / (BL_COMP_OP_INTERNAL_COUNT * BL_FORMAT_RESERVED_COUNT)),
                             uint32_t(index % BL_FORMAT_RESERVED_COUNT));
  }
};

// We go throught an additional constexpr to force the compiler to always generate
// the lookup table at compile time. If there is a mistake leading to recursion the
// compiler would catch it at compile-time instead of hitting it at runtime during
// initialization.
static constexpr const auto blCompOpSimplifyInfoArray_
  = blLookupTable<BLCompOpSimplifyInfo, BL_COMP_OP_SIMPLIFY_INFO_SIZE, BLCompOpSimplifyInfoGen>();

const BLLookupTable<BLCompOpSimplifyInfo, BL_COMP_OP_SIMPLIFY_INFO_SIZE> blCompOpSimplifyInfoArray
  = blCompOpSimplifyInfoArray_;
