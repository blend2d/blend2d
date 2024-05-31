// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPOPSIMPLIFYIMPL_P_H_INCLUDED
#define BLEND2D_COMPOPSIMPLIFYIMPL_P_H_INCLUDED

#include "api-internal_p.h"
#include "compopinfo_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

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
struct CompOpSimplifyInfoImpl {
  // Shorthands of pixel formats.
  using Fmt = bl::FormatExt;

  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo makeOp(CompOpExt compOp, Fmt d, Fmt s) noexcept {
    return CompOpSimplifyInfo::make(compOp, d, s, CompOpSolidId::kNone);
  }

  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo transparent(CompOpExt compOp, Fmt d, Fmt s) noexcept {
    return CompOpSimplifyInfo::make(compOp, d, s, CompOpSolidId::kTransparent);
  }

  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo opaqueBlack(CompOpExt compOp, Fmt d, Fmt s) noexcept {
    return CompOpSimplifyInfo::make(compOp, d, s, CompOpSolidId::kOpaqueBlack);
  }

  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo opaqueWhite(CompOpExt compOp, Fmt d, Fmt s) noexcept {
    return CompOpSimplifyInfo::make(compOp, d, s, CompOpSolidId::kOpaqueWhite);
  }

  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo opaqueAlpha(CompOpExt compOp, Fmt d, Fmt s) noexcept {
    return CompOpSimplifyInfo::make(compOp, d, s, CompOpSolidId::kOpaqueWhite);
  }

  // Internal Formats:
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo alphaInv(Fmt d, Fmt s) noexcept {
    return CompOpSimplifyInfo::make(CompOpExt::kAlphaInv, d, s, CompOpSolidId::kOpaqueWhite);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo clear(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 ? transparent(CompOpExt::kSrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 ? opaqueBlack(CompOpExt::kSrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kA8     ? transparent(CompOpExt::kSrcCopy, Fmt::kA8    , Fmt::kPRGB32) :

           makeOp(CompOpExt::kClear, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo srcCopy(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? makeOp(CompOpExt::kSrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? makeOp(CompOpExt::kSrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? makeOp(CompOpExt::kSrcCopy, Fmt::kPRGB32, Fmt::kXRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? makeOp(CompOpExt::kSrcCopy, Fmt::kPRGB32, Fmt::kXRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? makeOp(CompOpExt::kSrcCopy, Fmt::kPRGB32, Fmt::kXRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? makeOp(CompOpExt::kSrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? clear(Fmt::kA8, Fmt::kZERO32) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? opaqueAlpha(CompOpExt::kSrcCopy, d, Fmt::kPRGB32) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? opaqueAlpha(CompOpExt::kSrcCopy, d, Fmt::kPRGB32) :

           makeOp(CompOpExt::kSrcCopy, d, s);
  }

  // DstCopy
  // -------
  //
  // [DstCopy ANYxANY]
  //   Dca' = Dca
  //   Da   = Da
  BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo dstCopy(Fmt d, Fmt s) noexcept {
    return CompOpSimplifyInfo::dstCopy();
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo srcOver(Fmt d, Fmt s) noexcept {
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

           makeOp(CompOpExt::kSrcOver, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo dstOver(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? dstOver(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 ? dstCopy(d, s) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kDstOver, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo srcIn(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? srcIn(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? srcIn(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 ? srcCopy(d, s) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? clear(d, s) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? dstCopy(d, s) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? dstCopy(d, s) :

           makeOp(CompOpExt::kSrcIn, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo dstIn(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? srcCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kXRGB32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? dstCopy(d, s) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? dstIn(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstIn(Fmt::kPRGB32, Fmt::kFRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? dstCopy(d, s) :

           d == Fmt::kA8 ? srcIn(d, s) :

           makeOp(CompOpExt::kDstIn, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo srcOut(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? clear(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? srcOut(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 ? clear(d, s) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? clear(d, s) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? alphaInv(d, Fmt::kXRGB32) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? alphaInv(d, Fmt::kXRGB32) :

           makeOp(CompOpExt::kSrcOut, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo dstOut(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kXRGB32 ? clear(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? clear(d, s) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? clear(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? clear(d, s) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? clear(d, s) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? clear(d, s) :

           makeOp(CompOpExt::kDstOut, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo srcAtop(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kXRGB32 ? srcIn(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? srcIn(d, s) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? srcOver(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? srcOver(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? srcCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? srcCopy(d, s) :

           d == Fmt::kA8 ? dstCopy(d, s) :

           makeOp(CompOpExt::kSrcAtop, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo dstAtop(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? clear(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kXRGB32 ? dstOver(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? dstOver(d, s) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? dstIn(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? clear(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? dstCopy(d, s) :

           d == Fmt::kA8 ? srcCopy(d, s) :

           makeOp(CompOpExt::kDstAtop, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo xor_(Fmt d, Fmt s) noexcept {
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

           makeOp(CompOpExt::kXor, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo plus(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? plus(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? plus(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? plus(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? plus(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kA8 && s == Fmt::kXRGB32 ? opaqueAlpha(CompOpExt::kPlus, d, Fmt::kPRGB32) :
           d == Fmt::kA8 && s == Fmt::kFRGB32 ? opaqueAlpha(CompOpExt::kPlus, d, Fmt::kPRGB32) :

           makeOp(CompOpExt::kPlus, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo minus(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? minus(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? minus(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? minus(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? minus(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kMinus, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo modulate(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? transparent(CompOpExt::kSrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? modulate(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? opaqueBlack(CompOpExt::kSrcCopy, Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? modulate(Fmt::kXRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? modulate(Fmt::kXRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? dstIn(d, s) :

           makeOp(CompOpExt::kModulate, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo multiply(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? multiply(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? modulate(Fmt::kXRGB32, Fmt::kXRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? modulate(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? dstOver(d, s) :

           makeOp(CompOpExt::kMultiply, d, s);
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

  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo screen(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? screen(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kPRGB32 ? screen(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? screen(Fmt::kPRGB32, Fmt::kPRGB32) :
           d == Fmt::kXRGB32 && s == Fmt::kXRGB32 ? screen(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kScreen, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo overlay(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? overlay(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? overlay(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kOverlay, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo darken(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? darken(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? darken(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? dstOver(d, s) :

           makeOp(CompOpExt::kDarken, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo lighten(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? lighten(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? lighten(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kLighten, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo colorDodge(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? colorDodge(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? colorDodge(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kColorDodge, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo colorBurn(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? colorBurn(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? colorBurn(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? dstOver(d, s) :

           makeOp(CompOpExt::kColorBurn, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo linearBurn(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? linearBurn(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? linearBurn(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? dstOver(d, s) :

           makeOp(CompOpExt::kLinearBurn, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo linearLight(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? linearLight(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? linearLight(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kLinearLight, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo pinLight(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? pinLight(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? pinLight(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kPinLight, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo hardLight(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? hardLight(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? hardLight(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 || s == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kHardLight, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo softLight(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? softLight(Fmt::kPRGB32, Fmt::kXRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? softLight(Fmt::kXRGB32, Fmt::kXRGB32) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kSoftLight, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo difference(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? difference(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? difference(Fmt::kXRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kDifference, d, s);
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
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo exclusion(Fmt d, Fmt s) noexcept {
    return d == Fmt::kPRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kPRGB32 && s == Fmt::kFRGB32 ? exclusion(Fmt::kPRGB32, Fmt::kPRGB32) :

           d == Fmt::kXRGB32 && s == Fmt::kZERO32 ? dstCopy(d, s) :
           d == Fmt::kXRGB32 && s == Fmt::kFRGB32 ? exclusion(Fmt::kXRGB32, Fmt::kPRGB32) :

           d == Fmt::kA8 ? srcOver(d, s) :

           makeOp(CompOpExt::kExclusion, d, s);
  }

  // HACK: MSVC has a problem with code that does multiple ternary operations (? :)
  //       so we had to split it so Blend2D can compile. So please don't be active
  //       here and don't try to join these functions as you would break MSVC builds.
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo simplify_1(CompOpExt compOp, Fmt d, Fmt s) noexcept {
    return compOp == CompOpExt::kSrcCopy     ? srcCopy(d, s)     :
           compOp == CompOpExt::kSrcOver     ? srcOver(d, s)     :
           compOp == CompOpExt::kSrcIn       ? srcIn(d, s)       :
           compOp == CompOpExt::kSrcOut      ? srcOut(d, s)      :
           compOp == CompOpExt::kSrcAtop     ? srcAtop(d, s)     :
           compOp == CompOpExt::kDstCopy     ? dstCopy(d, s)     :
           compOp == CompOpExt::kDstOver     ? dstOver(d, s)     :
           compOp == CompOpExt::kDstIn       ? dstIn(d, s)       :
           compOp == CompOpExt::kDstOut      ? dstOut(d, s)      :
           compOp == CompOpExt::kDstAtop     ? dstAtop(d, s)     : dstCopy(d, s);
  }

  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo simplify_2(CompOpExt compOp, Fmt d, Fmt s) noexcept {
    return compOp == CompOpExt::kXor         ? xor_(d, s)        :
           compOp == CompOpExt::kClear       ? clear(d, s)       :
           compOp == CompOpExt::kPlus        ? plus(d, s)        :
           compOp == CompOpExt::kMinus       ? minus(d, s)       :
           compOp == CompOpExt::kModulate    ? modulate(d, s)    :
           compOp == CompOpExt::kMultiply    ? multiply(d, s)    :
           compOp == CompOpExt::kScreen      ? screen(d, s)      :
           compOp == CompOpExt::kOverlay     ? overlay(d, s)     :
           compOp == CompOpExt::kDarken      ? darken(d, s)      :
           compOp == CompOpExt::kLighten     ? lighten(d, s)     : simplify_1(compOp, d, s);
  }

  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo simplify_3(CompOpExt compOp, Fmt d, Fmt s) noexcept {
    return compOp == CompOpExt::kColorDodge  ? colorDodge(d, s)  :
           compOp == CompOpExt::kColorBurn   ? colorBurn(d, s)   :
           compOp == CompOpExt::kLinearBurn  ? linearBurn(d, s)  :
           compOp == CompOpExt::kLinearLight ? linearLight(d, s) :
           compOp == CompOpExt::kPinLight    ? pinLight(d, s)    :
           compOp == CompOpExt::kHardLight   ? hardLight(d, s)   :
           compOp == CompOpExt::kSoftLight   ? softLight(d, s)   :
           compOp == CompOpExt::kDifference  ? difference(d, s)  :
           compOp == CompOpExt::kExclusion   ? exclusion(d, s)   :

           // Extended operators, only used to simplify others.
           compOp == CompOpExt::kAlphaInv    ? alphaInv(d, s)    : simplify_2(compOp, d, s);
  }

  // Just dispatches to the respective composition operator.
  static BL_INLINE_NODEBUG constexpr CompOpSimplifyInfo simplify(CompOpExt compOp, Fmt d, Fmt s) noexcept {
    return simplify_3(compOp, d, s);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_COMPOPSIMPLIFYIMPL_P_H_INCLUDED
